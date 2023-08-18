/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_DTLS_SRTP_TRANSPORT_H
#define RTC_IMPL_DTLS_SRTP_TRANSPORT_H

#include <rtc/common.hpp>
#include <rtc/dtlstransport.hpp>

#if RTC_ENABLE_MEDIA

#include <srtp2/srtp.h>


#include <atomic>

namespace rtc::impl {

class DtlsSrtpTransport final : public DtlsTransport {
public:
	static void Init();
	static void Cleanup();

	DtlsSrtpTransport(shared_ptr<IceTransport> lower, certificate_ptr certificate,
	                  optional<size_t> mtu, verifier_callback verifierCallback,
	                  message_callback srtpRecvCallback, state_callback stateChangeCallback);
	~DtlsSrtpTransport();

	bool sendMedia(message_ptr message);

private:
	void recvMedia(message_ptr message);
	bool demuxMessage(message_ptr message) override;
	void postHandshake() override;

	message_callback mSrtpRecvCallback;

	srtp_t mSrtpIn, mSrtpOut;

	std::atomic<bool> mInitDone = false;
	unsigned char mClientSessionKey[SRTP_AES_ICM_128_KEY_LEN_WSALT];
	unsigned char mServerSessionKey[SRTP_AES_ICM_128_KEY_LEN_WSALT];
	std::mutex sendMutex;
};

} // namespace rtc::impl

#endif

#endif
