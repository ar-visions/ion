/**
 * Copyright (c) 2023 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#if RTC_ENABLE_MEDIA

#include "av1rtppacketizer.hpp"

#include "impl/internals.hpp"

namespace rtc {

const auto payloadHeaderSize = 1;

const auto zMask = byte(0b10000000);
const auto yMask = byte(0b01000000);
const auto nMask = byte(0b00001000);

const auto wBitshift = 4;

const auto obuFrameTypeMask = byte(0b01111000);
const auto obuFrameTypeBitshift = 3;

const auto obuHeaderSize = 1;
const auto obuHasExtensionMask = byte(0b00000100);
const auto obuHasSizeMask = byte(0b00000010);

const auto obuFrameTypeSequenceHeader = byte(1);

const auto obuTemporalUnitDelimiter = std::vector<byte>{byte(0x12), byte(0x00)};

const auto oneByteLeb128Size = 1;

const uint8_t sevenLsbBitmask = 0b01111111;
const uint8_t msbBitmask = 0b10000000;

std::vector<binary_ptr> extractTemporalUnitObus(binary_ptr message) {
	std::vector<shared_ptr<binary>> obus{};

	if (message->size() <= 2 || (message->at(0) != obuTemporalUnitDelimiter.at(0)) ||
	    (message->at(1) != obuTemporalUnitDelimiter.at(1))) {
		return obus;
	}

	size_t messageIndex = 2;
	while (messageIndex < message->size()) {
		if ((message->at(messageIndex) & obuHasSizeMask) == byte(0)) {
			return obus;
		}

		if ((message->at(messageIndex) & obuHasExtensionMask) != byte(0)) {
			messageIndex++;
		}

		// https://aomediacodec.github.io/av1-spec/#leb128
		uint32_t obuLength = 0;
		uint8_t leb128Size = 0;
		while (leb128Size < 8) {
			auto leb128Index = messageIndex + leb128Size + obuHeaderSize;
			if (message->size() < leb128Index) {
				break;
			}

			auto leb128_byte = uint8_t(message->at(leb128Index));

			obuLength |= ((leb128_byte & sevenLsbBitmask) << (leb128Size * 7));
			leb128Size++;

			if (!(leb128_byte & msbBitmask)) {
				break;
			}
		}

		obus.push_back(std::make_shared<binary>(message->begin() + messageIndex,
		                                        message->begin() + messageIndex + obuHeaderSize +
		                                            leb128Size + obuLength));

		messageIndex += obuHeaderSize + leb128Size + obuLength;
	}

	return obus;
}

/*
 *  0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+
 * |Z|Y| W |N|-|-|-|
 * +-+-+-+-+-+-+-+-+
 *
 *	Z: MUST be set to 1 if the first OBU element is an
 *	   OBU fragment that is a continuation of an OBU fragment
 *	   from the previous packet, and MUST be set to 0 otherwise.
 *
 *	Y: MUST be set to 1 if the last OBU element is an OBU fragment
 *	   that will continue in the next packet, and MUST be set to 0 otherwise.
 *
 *	W: two bit field that describes the number of OBU elements in the packet.
 *	   This field MUST be set equal to 0 or equal to the number of OBU elements
 *	   contained in the packet. If set to 0, each OBU element MUST be preceded by
 *	   a length field. If not set to 0 (i.e., W = 1, 2 or 3) the last OBU element
 *	   MUST NOT be preceded by a length field. Instead, the length of the last OBU
 *	   element contained in the packet can be calculated as follows:
 *	Length of the last OBU element =
 *	   length of the RTP payload
 *	 - length of aggregation header
 *	 - length of previous OBU elements including length fields
 *
 *	N: MUST be set to 1 if the packet is the first packet of a coded video sequence, and MUST be set
 *     to 0 otherwise.
 *
 * https://aomediacodec.github.io/av1-rtp-spec/#44-av1-aggregation-header
 *
 **/

std::vector<binary_ptr> AV1RtpPacketizer::packetizeObu(binary_ptr message,
                                                       uint16_t maximumFragmentSize) {

	std::vector<shared_ptr<binary>> payloads{};
	size_t messageIndex = 0;

	if (message->size() < 1) {
		return payloads;
	}

	// Cache sequence header and packetize with next OBU
	auto frameType = (message->at(0) & obuFrameTypeMask) >> obuFrameTypeBitshift;
	if (frameType == obuFrameTypeSequenceHeader) {
		sequenceHeader = std::make_shared<binary>(message->begin(), message->end());
		return payloads;
	}

	size_t messageRemaining = message->size();
	while (messageRemaining > 0) {
		auto obuCount = 1;
		auto metadataSize = payloadHeaderSize;

		if (sequenceHeader != nullptr) {
			obuCount++;
			metadataSize += /* 1 byte leb128 */ 1 + int(sequenceHeader->size());
		}

		auto payload = std::make_shared<binary>(
		    std::min(size_t(maximumFragmentSize), messageRemaining + metadataSize));
		auto payloadOffset = payloadHeaderSize;

		payload->at(0) = byte(obuCount) << wBitshift;

		// Packetize cached SequenceHeader
		if (obuCount == 2) {
			payload->at(0) ^= nMask;
			payload->at(1) = byte(sequenceHeader->size() & sevenLsbBitmask);
			payloadOffset += oneByteLeb128Size;

			std::memcpy(payload->data() + payloadOffset, sequenceHeader->data(),
			            sequenceHeader->size());
			payloadOffset += int(sequenceHeader->size());

			sequenceHeader = nullptr;
		}

		// Copy as much of OBU as possible into Payload
		auto payloadRemaining = payload->size() - payloadOffset;
		std::memcpy(payload->data() + payloadOffset, message->data() + messageIndex,
		            payloadRemaining);
		messageRemaining -= payloadRemaining;
		messageIndex += payloadRemaining;

		// Does this Fragment contain an OBU that started in a previous payload
		if (payloads.size() > 0) {
			payload->at(0) ^= zMask;
		}

		// This OBU will be continued in next Payload
		if (messageIndex < message->size()) {
			payload->at(0) ^= yMask;
		}

		payloads.push_back(payload);
	}

	return payloads;
}

AV1RtpPacketizer::AV1RtpPacketizer(AV1RtpPacketizer::Packetization packetization,
                                   shared_ptr<RtpPacketizationConfig> rtpConfig,
                                   uint16_t maximumFragmentSize)
    : RtpPacketizer(rtpConfig), MediaHandlerRootElement(), maximumFragmentSize(maximumFragmentSize),
      packetization(packetization) {}

ChainedOutgoingProduct
AV1RtpPacketizer::processOutgoingBinaryMessage(ChainedMessagesProduct messages,
                                               message_ptr control) {
	ChainedMessagesProduct packets = std::make_shared<std::vector<binary_ptr>>();
	for (auto message : *messages) {
		std::vector<binary_ptr> obus;

		if (packetization == AV1RtpPacketizer::Packetization::TemporalUnit) {
			obus = extractTemporalUnitObus(message);
		} else {
			obus.push_back(message);
		}

		for (auto obu : obus) {
			auto payloads = packetizeObu(obu, maximumFragmentSize);
			if (payloads.size() == 0) {
				continue;
			}

			unsigned i = 0;
			for (; i < payloads.size() - 1; i++) {
				packets->push_back(packetize(payloads[i], false));
			}
			packets->push_back(packetize(payloads[i], true));
		}
	}

	if (packets->size() == 0) {
		return ChainedOutgoingProduct();
	}

	return {packets, control};
}

} // namespace rtc

#endif /* RTC_ENABLE_MEDIA */
