/**
 * libdatachannel streamer example
 * Copyright (c) 2020 Filip Klembara (in2core)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef fileparser_hpp
#define fileparser_hpp

#include <string>
#include <vector>
#include "stream.hpp"

namespace ion {

struct FileParser: Source {
    struct iFileParser {
        std::string directory;
        std::string extension;
        uint64_t sampleDuration_us;
        uint64_t sampleTime_us = 0;
        uint32_t counter = -1;
        bool loop;
        uint64_t loopTimestampOffset = 0;
        rtc::binary sample = {};
        type_register(iFileParser);
    };

    mx_object(FileParser, Source, iFileParser);

    void  init(std::string directory, std::string extension, uint32_t samplesPerSecond, bool loop);
    FileParser(std::string directory, std::string extension, uint32_t samplesPerSecond, bool loop);
};
}

#endif /* fileparser_hpp */
