// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <string>

#include "common/hdr_util.h"
#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"
#include "testing/test_util.h"

using mkvparser::AudioTrack;
using mkvparser::Block;
using mkvparser::BlockEntry;
using mkvparser::BlockGroup;
using mkvparser::Cluster;
using mkvparser::CuePoint;
using mkvparser::Cues;
using mkvparser::MkvReader;
using mkvparser::Segment;
using mkvparser::SegmentInfo;
using mkvparser::Track;
using mkvparser::Tracks;
using mkvparser::VideoTrack;

using namespace ion;

// Base class containing boiler plate stuff.
struct webm_parser { 
 public:
  ParserTest() : is_reader_open_(false), segment_(NULL) {
    memset(dummy_data_, -1, kFrameLength);
    memset(gold_frame_, 0, kFrameLength);
  }

  ~webm_parser() {
    close_reader();
    if (segment_ != NULL) {
      delete segment_;
      segment_ = NULL;
    }
  }

  void close_reader() {
    if (is_reader_open_) {
      reader_.Close();
    }
    is_reader_open_ = false;
  }

  bool CreateAndLoadSegment(path filename, int expected_doc_type_ver) {
    if (reader_.Open(filename_.c_str())) {
      return false;
    }
    is_reader_open_ = true;
    pos_ = 0;
    mkvparser::EBMLHeader ebml_header;
    ebml_header.Parse(&reader_, pos_);
    assert(1 == ebml_header.m_version);
    assert(1 == ebml_header.m_readVersion);
    assert(strcmp("webm", ebml_header.m_docType) == 0);
    assert(expected_doc_type_ver == ebml_header.m_docTypeVersion);
    assert(2 == ebml_header.m_docTypeReadVersion);

    if (mkvparser::Segment::CreateInstance(&reader_, pos_, segment_)) {
      return false;
    }
    return !HasFailure() && segment_->Load() >= 0;
  }

  
TEST_F(ParserTest, Opus) {
  ASSERT_TRUE(CreateAndLoadSegment("bbb_480p_vp9_opus_1second.webm", 4));
  const unsigned int kTracksCount = 2;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  // --------------------------------------------------------------------------
  // Track Header validation.
  const Tracks* const tracks = segment_->GetTracks();
  EXPECT_EQ(kTracksCount, tracks->GetTracksCount());
  for (int i = 0; i < 2; ++i) {
    const Track* const track = tracks->GetTrackByIndex(i);
    ASSERT_TRUE(track != NULL);

    EXPECT_EQ(NULL, track->GetNameAsUTF8());
    EXPECT_STREQ("und", track->GetLanguage());
    EXPECT_EQ(i + 1, track->GetNumber());
    EXPECT_FALSE(track->GetLacing());

    if (track->GetType() == Track::kVideo) {
      const VideoTrack* const video_track =
          dynamic_cast<const VideoTrack*>(track);
      EXPECT_EQ(854, static_cast<int>(video_track->GetWidth()));
      EXPECT_EQ(480, static_cast<int>(video_track->GetHeight()));
      EXPECT_STREQ(kVP9CodecId, video_track->GetCodecId());
      EXPECT_DOUBLE_EQ(0., video_track->GetFrameRate());
      EXPECT_EQ(41666666,
                static_cast<int>(video_track->GetDefaultDuration()));  // 24.000
      const unsigned int kVideoUid = kVideoTrackNumber;
      EXPECT_EQ(kVideoUid, video_track->GetUid());
      const unsigned int kCodecDelay = 0;
      EXPECT_EQ(kCodecDelay, video_track->GetCodecDelay());
      const unsigned int kSeekPreRoll = 0;
      EXPECT_EQ(kSeekPreRoll, video_track->GetSeekPreRoll());

      size_t video_codec_private_size;
      EXPECT_EQ(NULL, video_track->GetCodecPrivate(video_codec_private_size));
      const unsigned int kPrivateSize = 0;
      EXPECT_EQ(kPrivateSize, video_codec_private_size);
    } else if (track->GetType() == Track::kAudio) {
      const AudioTrack* const audio_track =
          dynamic_cast<const AudioTrack*>(track);
      EXPECT_EQ(48000, audio_track->GetSamplingRate());
      EXPECT_EQ(6, audio_track->GetChannels());
      EXPECT_EQ(32, audio_track->GetBitDepth());
      EXPECT_STREQ(kOpusCodecId, audio_track->GetCodecId());
      EXPECT_EQ(kAudioTrackNumber, static_cast<int>(audio_track->GetUid()));
      const unsigned int kDefaultDuration = 0;
      EXPECT_EQ(kDefaultDuration, audio_track->GetDefaultDuration());
      EXPECT_EQ(kOpusCodecDelay, audio_track->GetCodecDelay());
      EXPECT_EQ(kOpusSeekPreroll, audio_track->GetSeekPreRoll());

      size_t audio_codec_private_size;
      EXPECT_TRUE(audio_track->GetCodecPrivate(audio_codec_private_size) !=
                  NULL);
      EXPECT_GE(audio_codec_private_size, kOpusPrivateDataSizeMinimum);
    }
  }

  // --------------------------------------------------------------------------
  // Parse the file to do block-level validation.
  const Cluster* cluster = segment_->GetFirst();
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  for (; cluster != NULL && !cluster->EOS();
       cluster = segment_->GetNext(cluster)) {
    // Get the first block
    const BlockEntry* block_entry;
    EXPECT_EQ(0, cluster->GetFirst(block_entry));
    ASSERT_TRUE(block_entry != NULL);
    EXPECT_FALSE(block_entry->EOS());

    while (block_entry != NULL && !block_entry->EOS()) {
      const Block* const block = block_entry->GetBlock();
      ASSERT_TRUE(block != NULL);
      EXPECT_FALSE(block->IsInvisible());
      EXPECT_EQ(Block::kLacingNone, block->GetLacing());

      const std::uint32_t track_number =
          static_cast<std::uint32_t>(block->GetTrackNumber());
      const Track* const track = tracks->GetTrackByNumber(track_number);
      ASSERT_TRUE(track != NULL);
      EXPECT_EQ(track->GetNumber(), block->GetTrackNumber());
      const unsigned int kContentEncodingCount = 0;
      EXPECT_EQ(kContentEncodingCount,
                track->GetContentEncodingCount());  // no encryption

      const std::int64_t track_type = track->GetType();
      EXPECT_TRUE(track_type == Track::kVideo || track_type == Track::kAudio);
      if (track_type == Track::kVideo) {
        EXPECT_EQ(BlockEntry::kBlockSimple, block_entry->GetKind());
        EXPECT_EQ(0, block->GetDiscardPadding());
      } else {
        EXPECT_TRUE(block->IsKey());
        const std::int64_t kLastAudioTimecode = 1001;
        const std::int64_t timecode = block->GetTimeCode(cluster);
        // Only the final Opus block should have discard padding.
        if (timecode == kLastAudioTimecode) {
          EXPECT_EQ(BlockEntry::kBlockGroup, block_entry->GetKind());
          EXPECT_EQ(13500000, block->GetDiscardPadding());
        } else {
          EXPECT_EQ(BlockEntry::kBlockSimple, block_entry->GetKind());
          EXPECT_EQ(0, block->GetDiscardPadding());
        }
      }

      const int frame_count = block->GetFrameCount();
      const Block::Frame& frame = block->GetFrame(0);
      EXPECT_EQ(1, frame_count);
      EXPECT_GT(frame.len, 0);

      EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
    }
  }

  ASSERT_TRUE(cluster != NULL);
  EXPECT_TRUE(cluster->EOS());
}
};
