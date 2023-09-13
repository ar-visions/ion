#include <GL/freeglut.h>
#include <GL/freeglut_ext.h>
#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <GL/glew.h>

#include <GL/gl.h>
#include <EGL/egl.h>

#include <vector>
#include <nvenc/nvEncodeAPI.h>
#include <stdint.h>
#include <mutex>
#include <string>
#include <iostream>
#include <sstream>
#include <string.h>






/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2023 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

//---------------------------------------------------------------------------
//! \file NvCodecUtils.h
//! \brief Miscellaneous classes and error checking functions.
//!
//! Used by Transcode/Encode samples apps for reading input files, mutithreading, performance measurement or colorspace conversion while decoding.
//---------------------------------------------------------------------------

#include <iomanip>
#include <chrono>
#include <sys/stat.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2023 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <time.h>

#ifdef _WIN32
#include <winsock.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")
#undef ERROR
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET -1
#endif

enum LogLevel {
    TRACE,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

namespace simplelogger{
class Logger {
public:
    Logger(LogLevel level, bool bPrintTimeStamp) : level(level), bPrintTimeStamp(bPrintTimeStamp) {}
    virtual ~Logger() {}
    virtual std::ostream& GetStream() = 0;
    virtual void FlushStream() {}
    bool ShouldLogFor(LogLevel l) {
        return l >= level;
    }
    char* GetLead(LogLevel l, const char *szFile, int nLine, const char *szFunc) {
        if (l < TRACE || l > FATAL) {
            sprintf(szLead, "[?????] ");
            return szLead;
        }
        const char *szLevels[] = {"TRACE", "INFO", "WARN", "ERROR", "FATAL"};
        if (bPrintTimeStamp) {
            time_t t = time(NULL);
            struct tm *ptm = localtime(&t);
            sprintf(szLead, "[%-5s][%02d:%02d:%02d] ", 
                szLevels[l], ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
        } else {
            sprintf(szLead, "[%-5s] ", szLevels[l]);
        }
        return szLead;
    }
    void EnterCriticalSection() {
        mtx.lock();
    }
    void LeaveCriticalSection() {
        mtx.unlock();
    }
private:
    LogLevel level;
    char szLead[80];
    bool bPrintTimeStamp;
    std::mutex mtx;
};

class LoggerFactory {
public:
    static Logger* CreateFileLogger(std::string strFilePath, 
            LogLevel level = INFO, bool bPrintTimeStamp = true) {
        return new FileLogger(strFilePath, level, bPrintTimeStamp);
    }
    static Logger* CreateConsoleLogger(LogLevel level = INFO, 
            bool bPrintTimeStamp = true) {
        return new ConsoleLogger(level, bPrintTimeStamp);
    }
    static Logger* CreateUdpLogger(char *szHost, unsigned uPort, LogLevel level = INFO, 
            bool bPrintTimeStamp = true) {
        return new UdpLogger(szHost, uPort, level, bPrintTimeStamp);
    }
private:
    LoggerFactory() {}

    class FileLogger : public Logger {
    public:
        FileLogger(std::string strFilePath, LogLevel level, bool bPrintTimeStamp) 
        : Logger(level, bPrintTimeStamp) {
            pFileOut = new std::ofstream();
            pFileOut->open(strFilePath.c_str());
        }
        ~FileLogger() {
            pFileOut->close();
        }
        std::ostream& GetStream() {
            return *pFileOut;
        }
    private:
        std::ofstream *pFileOut;
    };

    class ConsoleLogger : public Logger {
    public:
        ConsoleLogger(LogLevel level, bool bPrintTimeStamp) 
        : Logger(level, bPrintTimeStamp) {}
        std::ostream& GetStream() {
            return std::cout;
        }
    };

    class UdpLogger : public Logger {
    private:
        class UdpOstream : public std::ostream {
        public:
            UdpOstream(char *szHost, unsigned short uPort) : std::ostream(&sb), socket(INVALID_SOCKET){
#ifdef _WIN32
                WSADATA w;
                if (WSAStartup(0x0101, &w) != 0) {
                    fprintf(stderr, "WSAStartup() failed.\n");
                    return;
                }
#endif
                socket = ::socket(AF_INET, SOCK_DGRAM, 0);
                if (socket == INVALID_SOCKET) {
#ifdef _WIN32
                    WSACleanup();
#endif
                    fprintf(stderr, "socket() failed.\n");
                    return;
                }
#ifdef _WIN32
                unsigned int b1, b2, b3, b4;
                sscanf(szHost, "%u.%u.%u.%u", &b1, &b2, &b3, &b4);
                struct in_addr addr = {(unsigned char)b1, (unsigned char)b2, (unsigned char)b3, (unsigned char)b4};
#else
                struct in_addr addr = {inet_addr(szHost)};
#endif
                struct sockaddr_in s = {AF_INET, htons(uPort), addr};
                server = s;
            }
            ~UdpOstream() throw() {
                if (socket == INVALID_SOCKET) {
                    return;
                }
#ifdef _WIN32
                closesocket(socket);
                WSACleanup();
#else
                close(socket);
#endif
            }
            void Flush() {
                if (sendto(socket, sb.str().c_str(), (int)sb.str().length() + 1, 
                        0, (struct sockaddr *)&server, (int)sizeof(sockaddr_in)) == -1) {
                    fprintf(stderr, "sendto() failed.\n");
                }
                sb.str("");
            }

        private:
            std::stringbuf sb;
            SOCKET socket;
            struct sockaddr_in server;
        };
    public:
        UdpLogger(char *szHost, unsigned uPort, LogLevel level, bool bPrintTimeStamp) 
        : Logger(level, bPrintTimeStamp), udpOut(szHost, (unsigned short)uPort) {}
        UdpOstream& GetStream() {
            return udpOut;
        }
        virtual void FlushStream() {
            udpOut.Flush();
        }
    private:
        UdpOstream udpOut;
    };
};

class LogTransaction {
public:
    LogTransaction(Logger *pLogger, LogLevel level, const char *szFile, const int nLine, const char *szFunc) : pLogger(pLogger), level(level) {
        if (!pLogger) {
            std::cout << "[-----] ";
            return;
        }
        if (!pLogger->ShouldLogFor(level)) {
            return;
        }
        pLogger->EnterCriticalSection();
        pLogger->GetStream() << pLogger->GetLead(level, szFile, nLine, szFunc);
    }
    ~LogTransaction() {
        if (!pLogger) {
            std::cout << std::endl;
            return;
        }
        if (!pLogger->ShouldLogFor(level)) {
            return;
        }
        pLogger->GetStream() << std::endl;
        pLogger->FlushStream();
        pLogger->LeaveCriticalSection();
        if (level == FATAL) {
            exit(1);
        }
    }
    std::ostream& GetStream() {
        if (!pLogger) {
            return std::cout;
        }
        if (!pLogger->ShouldLogFor(level)) {
            return ossNull;
        }
        return pLogger->GetStream();
    }
private:
    Logger *pLogger;
    LogLevel level;
    std::ostringstream ossNull;
};

}

extern simplelogger::Logger *logger;












/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2023 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iterator>
#include <cstring>
#include <functional>

extern simplelogger::Logger *logger;

#ifndef _WIN32
inline bool operator==(const GUID &guid1, const GUID &guid2) {
    return !memcmp(&guid1, &guid2, sizeof(GUID));
}

inline bool operator!=(const GUID &guid1, const GUID &guid2) {
    return !(guid1 == guid2);
}
#endif

#define LOG(level) simplelogger::LogTransaction(logger, level, __FILE__, __LINE__, __FUNCTION__).GetStream()



/*
 * Helper class for parsing generic encoder options and preparing encoder
 * initialization parameters. This class also provides some utility methods
 * which generate verbose descriptions of the provided set of encoder
 * initialization parameters.
 */
class NvEncoderInitParam {
public:
    NvEncoderInitParam(const char *szParam = "", 
        std::function<void(NV_ENC_INITIALIZE_PARAMS *pParams)> *pfuncInit = NULL, bool _bLowLatency = false) 
        : strParam(szParam), bLowLatency(_bLowLatency)
    {
        if (pfuncInit) {
            funcInit = *pfuncInit;
        }

        std::transform(strParam.begin(), strParam.end(), strParam.begin(), tolower);
        std::istringstream ss(strParam);
        tokens = std::vector<std::string> {
            std::istream_iterator<std::string>(ss),
            std::istream_iterator<std::string>() 
        };

        for (unsigned i = 0; i < tokens.size(); i++)
        {
            if (tokens[i] == "-codec" && ++i != tokens.size())
            {
                ParseString("-codec", tokens[i], vCodec, szCodecNames, &guidCodec);
                continue;
            }
            if (tokens[i] == "-preset" && ++i != tokens.size()) {
                ParseString("-preset", tokens[i], vPreset, szPresetNames, &guidPreset);
                continue;
            }
            if (tokens[i] == "-tuninginfo" && ++i != tokens.size())
            {
                ParseString("-tuninginfo", tokens[i], vTuningInfo, szTuningInfoNames, &m_TuningInfo);
                continue;
            }
        }
    }
    virtual ~NvEncoderInitParam() {}
    virtual bool IsCodecH264() {
        return GetEncodeGUID() == NV_ENC_CODEC_H264_GUID;
    }

    virtual bool IsCodecHEVC() {
        return GetEncodeGUID() == NV_ENC_CODEC_HEVC_GUID;
    }

    virtual bool IsCodecAV1() {
        return GetEncodeGUID() == NV_ENC_CODEC_AV1_GUID;
    }

    std::string GetHelpMessage(bool bMeOnly = false, bool bUnbuffered = false, bool bHide444 = false, bool bOutputInVidMem = false)
    {
        std::ostringstream oss;

        if (bOutputInVidMem && bMeOnly)
        {
            oss << "-codec       Codec: " << "h264" << std::endl;
        }
        else
        {
            oss << "-codec       Codec: " << szCodecNames << std::endl;
        }

        oss << "-preset      Preset: " << szPresetNames << std::endl
            << "-profile     H264: " << szH264ProfileNames;

        if (bOutputInVidMem && bMeOnly)
        {
            oss << std::endl;
        }
        else
        {
            oss << "; HEVC: " << szHevcProfileNames;
            oss << "; AV1: " << szAV1ProfileNames << std::endl;
        }

        if (!bMeOnly)
        {
            if (bLowLatency == false)
                oss << "-tuninginfo  TuningInfo: " << szTuningInfoNames << std::endl;
            else
                oss << "-tuninginfo  TuningInfo: " << szLowLatencyTuningInfoNames << std::endl;
            oss << "-multipass   Multipass: " << szMultipass << std::endl;
        }

        if (!bHide444 && !bLowLatency)
        {
            oss << "-444         (Only for RGB input) YUV444 encode. Not valid for AV1 Codec" << std::endl;
        }
        if (bMeOnly) return oss.str();
        oss << "-fps         Frame rate" << std::endl;

        if (!bUnbuffered && !bLowLatency)
        {
            oss << "-bf          Number of consecutive B-frames" << std::endl;
        }

        if (!bLowLatency)
        {
            oss << "-rc          Rate control mode: " << szRcModeNames << std::endl
                << "-gop         Length of GOP (Group of Pictures)" << std::endl
                << "-bitrate     Average bit rate, can be in unit of 1, K, M" << std::endl
                << "Note:        Fps or Average bit rate values for each session can be specified in the form of v1,v1,v3 (no space) for AppTransOneToN" << std::endl 
                << "             If the number of 'bitrate' or 'fps' values specified are less than the number of sessions, then the last specified value will be considered for the remaining sessions" << std::endl
                << "-maxbitrate  Max bit rate, can be in unit of 1, K, M" << std::endl
                << "-vbvbufsize  VBV buffer size in bits, can be in unit of 1, K, M" << std::endl
                << "-vbvinit     VBV initial delay in bits, can be in unit of 1, K, M" << std::endl
                << "-aq          Enable spatial AQ and set its stength (range 1-15, 0-auto)" << std::endl
                << "-temporalaq  (No value) Enable temporal AQ" << std::endl
                << "-cq          Target constant quality level for VBR mode (range 1-51, 0-auto)" << std::endl;
        }
        if (!bUnbuffered && !bLowLatency)
        {
            oss << "-lookahead   Maximum depth of lookahead (range 0-(31 - number of B frames))" << std::endl;
        }
        oss << "-qmin        Min QP value" << std::endl
            << "-qmax        Max QP value" << std::endl
            << "-initqp      Initial QP value" << std::endl;
        if (!bLowLatency)
        {
            oss << "-constqp     QP value for constqp rate control mode" << std::endl
                << "Note: QP value can be in the form of qp_of_P_B_I or qp_P,qp_B,qp_I (no space)" << std::endl;
        }
        if (bUnbuffered && !bLowLatency)
        {
            oss << "Note: Options -bf and -lookahead are unavailable for this app" << std::endl;
        }
        return oss.str();
    }

    /**
     * @brief Generate and return a string describing the values of the main/common
     *        encoder initialization parameters
     */
    std::string MainParamToString(const NV_ENC_INITIALIZE_PARAMS *pParams) {
        std::ostringstream os;
        os 
            << "Encoding Parameters:" 
            << std::endl << "\tcodec        : " << ConvertValueToString(vCodec, szCodecNames, pParams->encodeGUID)
            << std::endl << "\tpreset       : " << ConvertValueToString(vPreset, szPresetNames, pParams->presetGUID);
        if (pParams->tuningInfo)
        {
            os << std::endl << "\ttuningInfo   : " << ConvertValueToString(vTuningInfo, szTuningInfoNames, pParams->tuningInfo);
        }
        os
            << std::endl << "\tprofile      : " << ConvertValueToString(vProfile, szProfileNames, pParams->encodeConfig->profileGUID)
            << std::endl << "\tchroma       : " << ConvertValueToString(vChroma, szChromaNames, (pParams->encodeGUID == NV_ENC_CODEC_H264_GUID) ? pParams->encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC :
                                                   (pParams->encodeGUID == NV_ENC_CODEC_HEVC_GUID) ? pParams->encodeConfig->encodeCodecConfig.hevcConfig.chromaFormatIDC :
                                                   pParams->encodeConfig->encodeCodecConfig.av1Config.chromaFormatIDC)
            << std::endl << "\tbitdepth     : " << ((pParams->encodeGUID == NV_ENC_CODEC_H264_GUID) ? 0 : (pParams->encodeGUID == NV_ENC_CODEC_HEVC_GUID) ?
                                                     pParams->encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 : pParams->encodeConfig->encodeCodecConfig.av1Config.pixelBitDepthMinus8) + 8
            << std::endl << "\trc           : " << ConvertValueToString(vRcMode, szRcModeNames, pParams->encodeConfig->rcParams.rateControlMode)
            ;
            if (pParams->encodeConfig->rcParams.rateControlMode == NV_ENC_PARAMS_RC_CONSTQP) {
                os << " (P,B,I=" << pParams->encodeConfig->rcParams.constQP.qpInterP << "," << pParams->encodeConfig->rcParams.constQP.qpInterB << "," << pParams->encodeConfig->rcParams.constQP.qpIntra << ")";
            }
        os
            << std::endl << "\tfps          : " << pParams->frameRateNum << "/" << pParams->frameRateDen
            << std::endl << "\tgop          : " << (pParams->encodeConfig->gopLength == NVENC_INFINITE_GOPLENGTH ? "INF" : std::to_string(pParams->encodeConfig->gopLength))
            << std::endl << "\tbf           : " << pParams->encodeConfig->frameIntervalP - 1
            << std::endl << "\tmultipass    : " << pParams->encodeConfig->rcParams.multiPass
            << std::endl << "\tsize         : " << pParams->encodeWidth << "x" << pParams->encodeHeight
            << std::endl << "\tbitrate      : " << pParams->encodeConfig->rcParams.averageBitRate
            << std::endl << "\tmaxbitrate   : " << pParams->encodeConfig->rcParams.maxBitRate
            << std::endl << "\tvbvbufsize   : " << pParams->encodeConfig->rcParams.vbvBufferSize
            << std::endl << "\tvbvinit      : " << pParams->encodeConfig->rcParams.vbvInitialDelay
            << std::endl << "\taq           : " << (pParams->encodeConfig->rcParams.enableAQ ? (pParams->encodeConfig->rcParams.aqStrength ? std::to_string(pParams->encodeConfig->rcParams.aqStrength) : "auto") : "disabled")
            << std::endl << "\ttemporalaq   : " << (pParams->encodeConfig->rcParams.enableTemporalAQ ? "enabled" : "disabled")
            << std::endl << "\tlookahead    : " << (pParams->encodeConfig->rcParams.enableLookahead ? std::to_string(pParams->encodeConfig->rcParams.lookaheadDepth) : "disabled")
            << std::endl << "\tcq           : " << (unsigned int)pParams->encodeConfig->rcParams.targetQuality
            << std::endl << "\tqmin         : P,B,I=" << (int)pParams->encodeConfig->rcParams.minQP.qpInterP << "," << (int)pParams->encodeConfig->rcParams.minQP.qpInterB << "," << (int)pParams->encodeConfig->rcParams.minQP.qpIntra
            << std::endl << "\tqmax         : P,B,I=" << (int)pParams->encodeConfig->rcParams.maxQP.qpInterP << "," << (int)pParams->encodeConfig->rcParams.maxQP.qpInterB << "," << (int)pParams->encodeConfig->rcParams.maxQP.qpIntra
            << std::endl << "\tinitqp       : P,B,I=" << (int)pParams->encodeConfig->rcParams.initialRCQP.qpInterP << "," << (int)pParams->encodeConfig->rcParams.initialRCQP.qpInterB << "," << (int)pParams->encodeConfig->rcParams.initialRCQP.qpIntra
            ;
        return os.str();
    }

public:
    virtual GUID GetEncodeGUID() { return guidCodec; }
    virtual GUID GetPresetGUID() { return guidPreset; }
    virtual NV_ENC_TUNING_INFO GetTuningInfo() { return m_TuningInfo; }

    /*
     * @brief Set encoder initialization parameters based on input options
     * This method parses the tokens formed from the command line options
     * provided to the application and sets the fields from NV_ENC_INITIALIZE_PARAMS
     * based on the supplied values.
     */

    virtual void setTransOneToN(bool isTransOneToN)
    {
        bTransOneToN = isTransOneToN;
    }

    virtual void SetInitParams(NV_ENC_INITIALIZE_PARAMS *pParams, NV_ENC_BUFFER_FORMAT eBufferFormat)
    {
        NV_ENC_CONFIG &config = *pParams->encodeConfig;
        int nGOPOption = 0, nBFramesOption = 0;
        for (unsigned i = 0; i < tokens.size(); i++)
        {
            if (
                tokens[i] == "-codec"      && ++i ||
                tokens[i] == "-preset"     && ++i ||
                tokens[i] == "-tuninginfo" && ++i ||
                tokens[i] == "-multipass" && ++i != tokens.size() && ParseString("-multipass", tokens[i], vMultiPass, szMultipass, &config.rcParams.multiPass) ||
                tokens[i] == "-profile"    && ++i != tokens.size() && (IsCodecH264() ? 
                    ParseString("-profile", tokens[i], vH264Profile, szH264ProfileNames, &config.profileGUID) : IsCodecHEVC() ?
                    ParseString("-profile", tokens[i], vHevcProfile, szHevcProfileNames, &config.profileGUID) :
                    ParseString("-profile", tokens[i], vAV1Profile, szAV1ProfileNames, &config.profileGUID)) ||
                tokens[i] == "-rc"         && ++i != tokens.size() && ParseString("-rc",          tokens[i], vRcMode, szRcModeNames, &config.rcParams.rateControlMode)                    ||
                tokens[i] == "-fps"        && ++i != tokens.size() && ParseInt("-fps",            tokens[i], &pParams->frameRateNum)                                                      ||
                tokens[i] == "-bf"         && ++i != tokens.size() && ParseInt("-bf",             tokens[i], &config.frameIntervalP) && ++config.frameIntervalP && ++nBFramesOption       ||
                tokens[i] == "-bitrate"    && ++i != tokens.size() && ParseBitRate("-bitrate",    tokens[i], &config.rcParams.averageBitRate)                                             ||
                tokens[i] == "-maxbitrate" && ++i != tokens.size() && ParseBitRate("-maxbitrate", tokens[i], &config.rcParams.maxBitRate)                                                 ||
                tokens[i] == "-vbvbufsize" && ++i != tokens.size() && ParseBitRate("-vbvbufsize", tokens[i], &config.rcParams.vbvBufferSize)                                              ||
                tokens[i] == "-vbvinit"    && ++i != tokens.size() && ParseBitRate("-vbvinit",    tokens[i], &config.rcParams.vbvInitialDelay)                                            ||
                tokens[i] == "-cq"         && ++i != tokens.size() && ParseInt("-cq",             tokens[i], &config.rcParams.targetQuality)                                              ||
                tokens[i] == "-initqp"     && ++i != tokens.size() && ParseQp("-initqp",          tokens[i], &config.rcParams.initialRCQP) && (config.rcParams.enableInitialRCQP = true)  ||
                tokens[i] == "-qmin"       && ++i != tokens.size() && ParseQp("-qmin",            tokens[i], &config.rcParams.minQP) && (config.rcParams.enableMinQP = true)              ||
                tokens[i] == "-qmax"       && ++i != tokens.size() && ParseQp("-qmax",            tokens[i], &config.rcParams.maxQP) && (config.rcParams.enableMaxQP = true)              ||
                tokens[i] == "-constqp"    && ++i != tokens.size() && ParseQp("-constqp",         tokens[i], &config.rcParams.constQP)                                                    ||
                tokens[i] == "-temporalaq" && (config.rcParams.enableTemporalAQ = true)
            )
            {
                continue;
            }
            if (tokens[i] == "-lookahead" && ++i != tokens.size() && ParseInt("-lookahead", tokens[i], &config.rcParams.lookaheadDepth))
            {
                config.rcParams.enableLookahead = config.rcParams.lookaheadDepth > 0;
                continue;
            }
            int aqStrength;
            if (tokens[i] == "-aq" && ++i != tokens.size() && ParseInt("-aq", tokens[i], &aqStrength)) {
                config.rcParams.enableAQ = true;
                config.rcParams.aqStrength = aqStrength;
                continue;
            }

            if (tokens[i] == "-gop" && ++i != tokens.size() && ParseInt("-gop", tokens[i], &config.gopLength))
            {
                nGOPOption = 1;
                if (IsCodecH264()) 
                {
                    config.encodeCodecConfig.h264Config.idrPeriod = config.gopLength;
                }
                else if (IsCodecHEVC())
                {
                    config.encodeCodecConfig.hevcConfig.idrPeriod = config.gopLength;
                }
                else
                {
                    config.encodeCodecConfig.av1Config.idrPeriod = config.gopLength;
                }
                continue;
            }

            if (tokens[i] == "-444")
            {
                if (IsCodecH264()) 
                {
                    config.encodeCodecConfig.h264Config.chromaFormatIDC = 3;
                } 
                else if (IsCodecHEVC())
                {
                    config.encodeCodecConfig.hevcConfig.chromaFormatIDC = 3;
                }
                else
                {
                    std::ostringstream errmessage;
                    errmessage << "Incorrect Parameter: YUV444 Input not supported with AV1 Codec" << std::endl;
                    throw std::invalid_argument(errmessage.str());
                }
                continue;
            }

            std::ostringstream errmessage;
            errmessage << "Incorrect parameter: " << tokens[i] << std::endl;
            errmessage << "Re-run the application with the -h option to get a list of the supported options.";
            errmessage << std::endl;

            throw std::invalid_argument(errmessage.str());
        }

        if (IsCodecHEVC())
        {
            if (eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT || eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)
            {
                config.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
            }
        }

        if (IsCodecAV1())
        {
            if (eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT)
            {
                config.encodeCodecConfig.av1Config.pixelBitDepthMinus8 = 2;
                config.encodeCodecConfig.av1Config.inputPixelBitDepthMinus8 = 2;
            }
        }

        if (nGOPOption && nBFramesOption && (config.gopLength < ((uint32_t)config.frameIntervalP)))
        {
            std::ostringstream errmessage;
            errmessage << "gopLength (" << config.gopLength << ") must be greater or equal to frameIntervalP (number of B frames + 1) (" << config.frameIntervalP << ")\n";
            throw std::invalid_argument(errmessage.str());
        }

        funcInit(pParams);
        LOG(INFO) << NvEncoderInitParam().MainParamToString(pParams);
        LOG(TRACE) << NvEncoderInitParam().FullParamToString(pParams);
    }

private:
    /*
     * Helper methods for parsing tokens (generated by splitting the command line)
     * and performing conversions to the appropriate target type/value.
     */
    template<typename T>
    bool ParseString(const std::string &strName, const std::string &strValue, const std::vector<T> &vValue, const std::string &strValueNames, T *pValue) {
        std::vector<std::string> vstrValueName = split(strValueNames, ' ');
        auto it = std::find(vstrValueName.begin(), vstrValueName.end(), strValue);
        if (it == vstrValueName.end()) {
            LOG(ERROR) << strName << " options: " << strValueNames;
            return false;
        }
        *pValue = vValue[it - vstrValueName.begin()];
        return true;
    }
    template<typename T>
    std::string ConvertValueToString(const std::vector<T> &vValue, const std::string &strValueNames, T value) {
        auto it = std::find(vValue.begin(), vValue.end(), value);
        if (it == vValue.end()) {
            LOG(ERROR) << "Invalid value. Can't convert to one of " << strValueNames;
            return std::string();
        }
        return split(strValueNames, ' ')[it - vValue.begin()];
    }
    bool ParseBitRate(const std::string &strName, const std::string &strValue, unsigned *pBitRate) {
        if(bTransOneToN)
        {
            std::vector<std::string> oneToNBitrate = split(strValue, ',');
            std::string currBitrate;
            if ((bitrateCnt + 1) > oneToNBitrate.size())
            {
                currBitrate = oneToNBitrate[oneToNBitrate.size() - 1];
            }
            else
            {
                currBitrate = oneToNBitrate[bitrateCnt];
                bitrateCnt++;
            }

            try {
                size_t l;
                double r = std::stod(currBitrate, &l);
                char c = currBitrate[l];
                if (c != 0 && c != 'k' && c != 'm') {
                    LOG(ERROR) << strName << " units: 1, K, M (lower case also allowed)";
                }
                *pBitRate = (unsigned)((c == 'm' ? 1000000 : (c == 'k' ? 1000 : 1)) * r);
            }
            catch (std::invalid_argument) {
                return false;
            }
            return true;
        }

        else
        {
            try {
                size_t l;
                double r = std::stod(strValue, &l);
                char c = strValue[l];
                if (c != 0 && c != 'k' && c != 'm') {
                    LOG(ERROR) << strName << " units: 1, K, M (lower case also allowed)";
                }
                *pBitRate = (unsigned)((c == 'm' ? 1000000 : (c == 'k' ? 1000 : 1)) * r);
            }
            catch (std::invalid_argument) {
                return false;
            }
            return true;
        }
    }
    template<typename T>
    bool ParseInt(const std::string &strName, const std::string &strValue, T *pInt) {
        if (bTransOneToN)
        {
            std::vector<std::string> oneToNFps = split(strValue, ',');
            std::string currFps;
            if ((fpsCnt + 1) > oneToNFps.size())
            {
                currFps = oneToNFps[oneToNFps.size() - 1];
            }
            else
            {
                currFps = oneToNFps[fpsCnt];
                fpsCnt++;
            }

            try {
                *pInt = std::stoi(currFps);
            }
            catch (std::invalid_argument) {
                LOG(ERROR) << strName << " need a value of positive number";
                return false;
            }
            return true;
        }
        else
        {
            try {
                *pInt = std::stoi(strValue);
            }
            catch (std::invalid_argument) {
                LOG(ERROR) << strName << " need a value of positive number";
                return false;
            }
            return true;
        }
    }
    bool ParseQp(const std::string &strName, const std::string &strValue, NV_ENC_QP *pQp) {
        std::vector<std::string> vQp = split(strValue, ',');
        try {
            if (vQp.size() == 1) {
                unsigned qp = (unsigned)std::stoi(vQp[0]);
                *pQp = {qp, qp, qp};
            } else if (vQp.size() == 3) {
                *pQp = {(unsigned)std::stoi(vQp[0]), (unsigned)std::stoi(vQp[1]), (unsigned)std::stoi(vQp[2])};
            } else {
                LOG(ERROR) << strName << " qp_for_P_B_I or qp_P,qp_B,qp_I (no space is allowed)";
                return false;
            }
        } catch (std::invalid_argument) {
            return false;
        }
        return true;
    }
    std::vector<std::string> split(const std::string &s, char delim) {
        std::stringstream ss(s);
        std::string token;
        std::vector<std::string> tokens;
        while (getline(ss, token, delim)) {
            tokens.push_back(token);
        }
        return tokens;
    }

private:
    std::string strParam;
    std::function<void(NV_ENC_INITIALIZE_PARAMS *pParams)> funcInit = [](NV_ENC_INITIALIZE_PARAMS *pParams){};
    std::vector<std::string> tokens;
    GUID guidCodec = NV_ENC_CODEC_H264_GUID;
    GUID guidPreset = NV_ENC_PRESET_P3_GUID;
    NV_ENC_TUNING_INFO m_TuningInfo = NV_ENC_TUNING_INFO_HIGH_QUALITY;
    bool bLowLatency = false;
    uint32_t bitrateCnt = 0;
    uint32_t fpsCnt = 0;
    bool bTransOneToN = 0;
    
    const char *szCodecNames = "h264 hevc av1";
    std::vector<GUID> vCodec = std::vector<GUID> {
        NV_ENC_CODEC_H264_GUID,
        NV_ENC_CODEC_HEVC_GUID,
        NV_ENC_CODEC_AV1_GUID
    };
    
    const char *szChromaNames = "yuv420 yuv444";
    std::vector<uint32_t> vChroma = std::vector<uint32_t>
    {
        1, 3
    };
    
    const char *szPresetNames = "p1 p2 p3 p4 p5 p6 p7";
    std::vector<GUID> vPreset = std::vector<GUID> {
        NV_ENC_PRESET_P1_GUID,
        NV_ENC_PRESET_P2_GUID,
        NV_ENC_PRESET_P3_GUID,
        NV_ENC_PRESET_P4_GUID,
        NV_ENC_PRESET_P5_GUID,
        NV_ENC_PRESET_P6_GUID,
        NV_ENC_PRESET_P7_GUID,
    };

    const char *szH264ProfileNames = "baseline main high high444";
    std::vector<GUID> vH264Profile = std::vector<GUID> {
        NV_ENC_H264_PROFILE_BASELINE_GUID,
        NV_ENC_H264_PROFILE_MAIN_GUID,
        NV_ENC_H264_PROFILE_HIGH_GUID,
        NV_ENC_H264_PROFILE_HIGH_444_GUID,
    };
    const char *szHevcProfileNames = "main main10 frext";
    std::vector<GUID> vHevcProfile = std::vector<GUID> {
        NV_ENC_HEVC_PROFILE_MAIN_GUID,
        NV_ENC_HEVC_PROFILE_MAIN10_GUID,
        NV_ENC_HEVC_PROFILE_FREXT_GUID,
    };
    const char *szAV1ProfileNames = "main";
    std::vector<GUID> vAV1Profile = std::vector<GUID>{
        NV_ENC_AV1_PROFILE_MAIN_GUID,
    };

    const char *szProfileNames = "(default) auto baseline(h264) main(h264) high(h264) high444(h264)"
        " stereo(h264) progressiv_high(h264) constrained_high(h264)"
        " main(hevc) main10(hevc) frext(hevc)"
        " main(av1) high(av1)";
    std::vector<GUID> vProfile = std::vector<GUID> {
        GUID{},
        NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID,
        NV_ENC_H264_PROFILE_BASELINE_GUID,
        NV_ENC_H264_PROFILE_MAIN_GUID,
        NV_ENC_H264_PROFILE_HIGH_GUID,
        NV_ENC_H264_PROFILE_HIGH_444_GUID,
        NV_ENC_H264_PROFILE_STEREO_GUID,
        NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID,
        NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID,
        NV_ENC_HEVC_PROFILE_MAIN_GUID,
        NV_ENC_HEVC_PROFILE_MAIN10_GUID,
        NV_ENC_HEVC_PROFILE_FREXT_GUID,
        NV_ENC_AV1_PROFILE_MAIN_GUID,
    };

    const char *szLowLatencyTuningInfoNames = "lowlatency ultralowlatency";
    const char *szTuningInfoNames = "hq lowlatency ultralowlatency lossless";
    std::vector<NV_ENC_TUNING_INFO> vTuningInfo = std::vector<NV_ENC_TUNING_INFO>{
        NV_ENC_TUNING_INFO_HIGH_QUALITY,
        NV_ENC_TUNING_INFO_LOW_LATENCY,
        NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,
        NV_ENC_TUNING_INFO_LOSSLESS
    };

    const char *szRcModeNames = "constqp vbr cbr";
    std::vector<NV_ENC_PARAMS_RC_MODE> vRcMode = std::vector<NV_ENC_PARAMS_RC_MODE> {
        NV_ENC_PARAMS_RC_CONSTQP,
        NV_ENC_PARAMS_RC_VBR,
        NV_ENC_PARAMS_RC_CBR,
    };

    const char *szMultipass = "disabled qres fullres";
    std::vector<NV_ENC_MULTI_PASS> vMultiPass = std::vector<NV_ENC_MULTI_PASS>{
        NV_ENC_MULTI_PASS_DISABLED,
        NV_ENC_TWO_PASS_QUARTER_RESOLUTION,
        NV_ENC_TWO_PASS_FULL_RESOLUTION,
    };

   const char *szQpMapModeNames = "disabled emphasis_level_map delta_qp_map qp_map";
    std::vector<NV_ENC_QP_MAP_MODE> vQpMapMode = std::vector<NV_ENC_QP_MAP_MODE> {
        NV_ENC_QP_MAP_DISABLED,
        NV_ENC_QP_MAP_EMPHASIS,
        NV_ENC_QP_MAP_DELTA,
        NV_ENC_QP_MAP,
    };


public:
    /*
     * Generates and returns a string describing the values for each field in
     * the NV_ENC_INITIALIZE_PARAMS structure (i.e. a description of the entire
     * set of initialization parameters supplied to the API).
     */
    std::string FullParamToString(const NV_ENC_INITIALIZE_PARAMS *pInitializeParams) {
        std::ostringstream os;
        os << "NV_ENC_INITIALIZE_PARAMS:" << std::endl
            << "encodeGUID: " << ConvertValueToString(vCodec, szCodecNames, pInitializeParams->encodeGUID) << std::endl
            << "presetGUID: " << ConvertValueToString(vPreset, szPresetNames, pInitializeParams->presetGUID) << std::endl;
        if (pInitializeParams->tuningInfo)
        {
            os << "tuningInfo: " << ConvertValueToString(vTuningInfo, szTuningInfoNames, pInitializeParams->tuningInfo) << std::endl;
        }
        os
            << "encodeWidth: " << pInitializeParams->encodeWidth << std::endl
            << "encodeHeight: " << pInitializeParams->encodeHeight << std::endl
            << "darWidth: " << pInitializeParams->darWidth << std::endl
            << "darHeight: " << pInitializeParams->darHeight << std::endl
            << "frameRateNum: " << pInitializeParams->frameRateNum << std::endl
            << "frameRateDen: " << pInitializeParams->frameRateDen << std::endl
            << "enableEncodeAsync: " << pInitializeParams->enableEncodeAsync << std::endl
            << "reportSliceOffsets: " << pInitializeParams->reportSliceOffsets << std::endl
            << "enableSubFrameWrite: " << pInitializeParams->enableSubFrameWrite << std::endl
            << "enableExternalMEHints: " << pInitializeParams->enableExternalMEHints << std::endl
            << "enableMEOnlyMode: " << pInitializeParams->enableMEOnlyMode << std::endl
            << "enableWeightedPrediction: " << pInitializeParams->enableWeightedPrediction << std::endl
            << "maxEncodeWidth: " << pInitializeParams->maxEncodeWidth << std::endl
            << "maxEncodeHeight: " << pInitializeParams->maxEncodeHeight << std::endl
            << "maxMEHintCountsPerBlock: " << pInitializeParams->maxMEHintCountsPerBlock << std::endl
        ;
        NV_ENC_CONFIG *pConfig = pInitializeParams->encodeConfig;
        os << "NV_ENC_CONFIG:" << std::endl
            << "profile: " << ConvertValueToString(vProfile, szProfileNames, pConfig->profileGUID) << std::endl
            << "gopLength: " << pConfig->gopLength << std::endl
            << "frameIntervalP: " << pConfig->frameIntervalP << std::endl
            << "monoChromeEncoding: " << pConfig->monoChromeEncoding << std::endl
            << "frameFieldMode: " << pConfig->frameFieldMode << std::endl
            << "mvPrecision: " << pConfig->mvPrecision << std::endl
            << "NV_ENC_RC_PARAMS:" << std::endl
            << "    rateControlMode: 0x" << std::hex << pConfig->rcParams.rateControlMode << std::dec << std::endl
            << "    constQP: " << pConfig->rcParams.constQP.qpInterP << ", " << pConfig->rcParams.constQP.qpInterB << ", " << pConfig->rcParams.constQP.qpIntra << std::endl
            << "    averageBitRate:  " << pConfig->rcParams.averageBitRate << std::endl
            << "    maxBitRate:      " << pConfig->rcParams.maxBitRate << std::endl
            << "    vbvBufferSize:   " << pConfig->rcParams.vbvBufferSize << std::endl
            << "    vbvInitialDelay: " << pConfig->rcParams.vbvInitialDelay << std::endl
            << "    enableMinQP: " << pConfig->rcParams.enableMinQP << std::endl
            << "    enableMaxQP: " << pConfig->rcParams.enableMaxQP << std::endl
            << "    enableInitialRCQP: " << pConfig->rcParams.enableInitialRCQP << std::endl
            << "    enableAQ: " << pConfig->rcParams.enableAQ << std::endl
            << "    qpMapMode: " << ConvertValueToString(vQpMapMode, szQpMapModeNames, pConfig->rcParams.qpMapMode) << std::endl
            << "    multipass: " << ConvertValueToString(vMultiPass, szMultipass, pConfig->rcParams.multiPass) << std::endl
            << "    enableLookahead: " << pConfig->rcParams.enableLookahead << std::endl
            << "    disableIadapt: " << pConfig->rcParams.disableIadapt << std::endl
            << "    disableBadapt: " << pConfig->rcParams.disableBadapt << std::endl
            << "    enableTemporalAQ: " << pConfig->rcParams.enableTemporalAQ << std::endl
            << "    zeroReorderDelay: " << pConfig->rcParams.zeroReorderDelay << std::endl
            << "    enableNonRefP: " << pConfig->rcParams.enableNonRefP << std::endl
            << "    strictGOPTarget: " << pConfig->rcParams.strictGOPTarget << std::endl
            << "    aqStrength: " << pConfig->rcParams.aqStrength << std::endl
            << "    minQP: " << pConfig->rcParams.minQP.qpInterP << ", " << pConfig->rcParams.minQP.qpInterB << ", " << pConfig->rcParams.minQP.qpIntra << std::endl
            << "    maxQP: " << pConfig->rcParams.maxQP.qpInterP << ", " << pConfig->rcParams.maxQP.qpInterB << ", " << pConfig->rcParams.maxQP.qpIntra << std::endl
            << "    initialRCQP: " << pConfig->rcParams.initialRCQP.qpInterP << ", " << pConfig->rcParams.initialRCQP.qpInterB << ", " << pConfig->rcParams.initialRCQP.qpIntra << std::endl
            << "    temporallayerIdxMask: " << pConfig->rcParams.temporallayerIdxMask << std::endl
            << "    temporalLayerQP: " << (int)pConfig->rcParams.temporalLayerQP[0] << ", " << (int)pConfig->rcParams.temporalLayerQP[1] << ", " << (int)pConfig->rcParams.temporalLayerQP[2] << ", " << (int)pConfig->rcParams.temporalLayerQP[3] << ", " << (int)pConfig->rcParams.temporalLayerQP[4] << ", " << (int)pConfig->rcParams.temporalLayerQP[5] << ", " << (int)pConfig->rcParams.temporalLayerQP[6] << ", " << (int)pConfig->rcParams.temporalLayerQP[7] << std::endl
            << "    targetQuality: " << pConfig->rcParams.targetQuality << std::endl
            << "    lookaheadDepth: " << pConfig->rcParams.lookaheadDepth << std::endl;
        if (pInitializeParams->encodeGUID == NV_ENC_CODEC_H264_GUID) {
            os  
            << "NV_ENC_CODEC_CONFIG (H264):" << std::endl
            << "    enableStereoMVC: " << pConfig->encodeCodecConfig.h264Config.enableStereoMVC << std::endl
            << "    hierarchicalPFrames: " << pConfig->encodeCodecConfig.h264Config.hierarchicalPFrames << std::endl
            << "    hierarchicalBFrames: " << pConfig->encodeCodecConfig.h264Config.hierarchicalBFrames << std::endl
            << "    outputBufferingPeriodSEI: " << pConfig->encodeCodecConfig.h264Config.outputBufferingPeriodSEI << std::endl
            << "    outputPictureTimingSEI: " << pConfig->encodeCodecConfig.h264Config.outputPictureTimingSEI << std::endl
            << "    outputAUD: " << pConfig->encodeCodecConfig.h264Config.outputAUD << std::endl
            << "    disableSPSPPS: " << pConfig->encodeCodecConfig.h264Config.disableSPSPPS << std::endl
            << "    outputFramePackingSEI: " << pConfig->encodeCodecConfig.h264Config.outputFramePackingSEI << std::endl
            << "    outputRecoveryPointSEI: " << pConfig->encodeCodecConfig.h264Config.outputRecoveryPointSEI << std::endl
            << "    enableIntraRefresh: " << pConfig->encodeCodecConfig.h264Config.enableIntraRefresh << std::endl
            << "    enableConstrainedEncoding: " << pConfig->encodeCodecConfig.h264Config.enableConstrainedEncoding << std::endl
            << "    repeatSPSPPS: " << pConfig->encodeCodecConfig.h264Config.repeatSPSPPS << std::endl
            << "    enableVFR: " << pConfig->encodeCodecConfig.h264Config.enableVFR << std::endl
            << "    enableLTR: " << pConfig->encodeCodecConfig.h264Config.enableLTR << std::endl
            << "    qpPrimeYZeroTransformBypassFlag: " << pConfig->encodeCodecConfig.h264Config.qpPrimeYZeroTransformBypassFlag << std::endl
            << "    useConstrainedIntraPred: " << pConfig->encodeCodecConfig.h264Config.useConstrainedIntraPred << std::endl
            << "    level: " << pConfig->encodeCodecConfig.h264Config.level << std::endl
            << "    idrPeriod: " << pConfig->encodeCodecConfig.h264Config.idrPeriod << std::endl
            << "    separateColourPlaneFlag: " << pConfig->encodeCodecConfig.h264Config.separateColourPlaneFlag << std::endl
            << "    disableDeblockingFilterIDC: " << pConfig->encodeCodecConfig.h264Config.disableDeblockingFilterIDC << std::endl
            << "    numTemporalLayers: " << pConfig->encodeCodecConfig.h264Config.numTemporalLayers << std::endl
            << "    spsId: " << pConfig->encodeCodecConfig.h264Config.spsId << std::endl
            << "    ppsId: " << pConfig->encodeCodecConfig.h264Config.ppsId << std::endl
            << "    adaptiveTransformMode: " << pConfig->encodeCodecConfig.h264Config.adaptiveTransformMode << std::endl
            << "    fmoMode: " << pConfig->encodeCodecConfig.h264Config.fmoMode << std::endl
            << "    bdirectMode: " << pConfig->encodeCodecConfig.h264Config.bdirectMode << std::endl
            << "    entropyCodingMode: " << pConfig->encodeCodecConfig.h264Config.entropyCodingMode << std::endl
            << "    stereoMode: " << pConfig->encodeCodecConfig.h264Config.stereoMode << std::endl
            << "    intraRefreshPeriod: " << pConfig->encodeCodecConfig.h264Config.intraRefreshPeriod << std::endl
            << "    intraRefreshCnt: " << pConfig->encodeCodecConfig.h264Config.intraRefreshCnt << std::endl
            << "    maxNumRefFrames: " << pConfig->encodeCodecConfig.h264Config.maxNumRefFrames << std::endl
            << "    sliceMode: " << pConfig->encodeCodecConfig.h264Config.sliceMode << std::endl
            << "    sliceModeData: " << pConfig->encodeCodecConfig.h264Config.sliceModeData << std::endl
            << "    NV_ENC_CONFIG_H264_VUI_PARAMETERS:" << std::endl
            << "        overscanInfoPresentFlag: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.overscanInfoPresentFlag << std::endl
            << "        overscanInfo: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.overscanInfo << std::endl
            << "        videoSignalTypePresentFlag: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.videoSignalTypePresentFlag << std::endl
            << "        videoFormat: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.videoFormat << std::endl
            << "        videoFullRangeFlag: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.videoFullRangeFlag << std::endl
            << "        colourDescriptionPresentFlag: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.colourDescriptionPresentFlag << std::endl
            << "        colourPrimaries: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.colourPrimaries << std::endl
            << "        transferCharacteristics: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.transferCharacteristics << std::endl
            << "        colourMatrix: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.colourMatrix << std::endl
            << "        chromaSampleLocationFlag: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.chromaSampleLocationFlag << std::endl
            << "        chromaSampleLocationTop: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.chromaSampleLocationTop << std::endl
            << "        chromaSampleLocationBot: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.chromaSampleLocationBot << std::endl
            << "        bitstreamRestrictionFlag: " << pConfig->encodeCodecConfig.h264Config.h264VUIParameters.bitstreamRestrictionFlag << std::endl
            << "    ltrNumFrames: " << pConfig->encodeCodecConfig.h264Config.ltrNumFrames << std::endl
            << "    ltrTrustMode: " << pConfig->encodeCodecConfig.h264Config.ltrTrustMode << std::endl
            << "    chromaFormatIDC: " << pConfig->encodeCodecConfig.h264Config.chromaFormatIDC << std::endl
            << "    maxTemporalLayers: " << pConfig->encodeCodecConfig.h264Config.maxTemporalLayers << std::endl;
        } else if (pInitializeParams->encodeGUID == NV_ENC_CODEC_HEVC_GUID) {
            os  
            << "NV_ENC_CODEC_CONFIG (HEVC):" << std::endl
            << "    level: " << pConfig->encodeCodecConfig.hevcConfig.level << std::endl
            << "    tier: " << pConfig->encodeCodecConfig.hevcConfig.tier << std::endl
            << "    minCUSize: " << pConfig->encodeCodecConfig.hevcConfig.minCUSize << std::endl
            << "    maxCUSize: " << pConfig->encodeCodecConfig.hevcConfig.maxCUSize << std::endl
            << "    useConstrainedIntraPred: " << pConfig->encodeCodecConfig.hevcConfig.useConstrainedIntraPred << std::endl
            << "    disableDeblockAcrossSliceBoundary: " << pConfig->encodeCodecConfig.hevcConfig.disableDeblockAcrossSliceBoundary << std::endl
            << "    outputBufferingPeriodSEI: " << pConfig->encodeCodecConfig.hevcConfig.outputBufferingPeriodSEI << std::endl
            << "    outputPictureTimingSEI: " << pConfig->encodeCodecConfig.hevcConfig.outputPictureTimingSEI << std::endl
            << "    outputAUD: " << pConfig->encodeCodecConfig.hevcConfig.outputAUD << std::endl
            << "    enableLTR: " << pConfig->encodeCodecConfig.hevcConfig.enableLTR << std::endl
            << "    disableSPSPPS: " << pConfig->encodeCodecConfig.hevcConfig.disableSPSPPS << std::endl
            << "    repeatSPSPPS: " << pConfig->encodeCodecConfig.hevcConfig.repeatSPSPPS << std::endl
            << "    enableIntraRefresh: " << pConfig->encodeCodecConfig.hevcConfig.enableIntraRefresh << std::endl
            << "    chromaFormatIDC: " << pConfig->encodeCodecConfig.hevcConfig.chromaFormatIDC << std::endl
            << "    pixelBitDepthMinus8: " << pConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 << std::endl
            << "    idrPeriod: " << pConfig->encodeCodecConfig.hevcConfig.idrPeriod << std::endl
            << "    intraRefreshPeriod: " << pConfig->encodeCodecConfig.hevcConfig.intraRefreshPeriod << std::endl
            << "    intraRefreshCnt: " << pConfig->encodeCodecConfig.hevcConfig.intraRefreshCnt << std::endl
            << "    maxNumRefFramesInDPB: " << pConfig->encodeCodecConfig.hevcConfig.maxNumRefFramesInDPB << std::endl
            << "    ltrNumFrames: " << pConfig->encodeCodecConfig.hevcConfig.ltrNumFrames << std::endl
            << "    vpsId: " << pConfig->encodeCodecConfig.hevcConfig.vpsId << std::endl
            << "    spsId: " << pConfig->encodeCodecConfig.hevcConfig.spsId << std::endl
            << "    ppsId: " << pConfig->encodeCodecConfig.hevcConfig.ppsId << std::endl
            << "    sliceMode: " << pConfig->encodeCodecConfig.hevcConfig.sliceMode << std::endl
            << "    sliceModeData: " << pConfig->encodeCodecConfig.hevcConfig.sliceModeData << std::endl
            << "    maxTemporalLayersMinus1: " << pConfig->encodeCodecConfig.hevcConfig.maxTemporalLayersMinus1 << std::endl
            << "    NV_ENC_CONFIG_HEVC_VUI_PARAMETERS:" << std::endl
            << "        overscanInfoPresentFlag: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.overscanInfoPresentFlag << std::endl
            << "        overscanInfo: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.overscanInfo << std::endl
            << "        videoSignalTypePresentFlag: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.videoSignalTypePresentFlag << std::endl
            << "        videoFormat: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.videoFormat << std::endl
            << "        videoFullRangeFlag: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.videoFullRangeFlag << std::endl
            << "        colourDescriptionPresentFlag: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.colourDescriptionPresentFlag << std::endl
            << "        colourPrimaries: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.colourPrimaries << std::endl
            << "        transferCharacteristics: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.transferCharacteristics << std::endl
            << "        colourMatrix: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.colourMatrix << std::endl
            << "        chromaSampleLocationFlag: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.chromaSampleLocationFlag << std::endl
            << "        chromaSampleLocationTop: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.chromaSampleLocationTop << std::endl
            << "        chromaSampleLocationBot: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.chromaSampleLocationBot << std::endl
            << "        bitstreamRestrictionFlag: " << pConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters.bitstreamRestrictionFlag << std::endl
            << "    ltrTrustMode: " << pConfig->encodeCodecConfig.hevcConfig.ltrTrustMode << std::endl;
        } else if (pInitializeParams->encodeGUID == NV_ENC_CODEC_AV1_GUID) {
            os
                << "NV_ENC_CODEC_CONFIG (AV1):" << std::endl
                << "    level: " << pConfig->encodeCodecConfig.av1Config.level << std::endl
                << "    tier: " << pConfig->encodeCodecConfig.av1Config.tier << std::endl
                << "    minPartSize: " << pConfig->encodeCodecConfig.av1Config.minPartSize << std::endl
                << "    maxPartSize: " << pConfig->encodeCodecConfig.av1Config.maxPartSize << std::endl
                << "    outputAnnexBFormat: " << pConfig->encodeCodecConfig.av1Config.outputAnnexBFormat << std::endl
                << "    enableTimingInfo: " << pConfig->encodeCodecConfig.av1Config.enableTimingInfo << std::endl
                << "    enableDecoderModelInfo: " << pConfig->encodeCodecConfig.av1Config.enableDecoderModelInfo << std::endl
                << "    enableFrameIdNumbers: " << pConfig->encodeCodecConfig.av1Config.enableFrameIdNumbers << std::endl
                << "    disableSeqHdr: " << pConfig->encodeCodecConfig.av1Config.disableSeqHdr << std::endl
                << "    repeatSeqHdr: " << pConfig->encodeCodecConfig.av1Config.repeatSeqHdr << std::endl
                << "    enableIntraRefresh: " << pConfig->encodeCodecConfig.av1Config.enableIntraRefresh << std::endl
                << "    chromaFormatIDC: " << pConfig->encodeCodecConfig.av1Config.chromaFormatIDC << std::endl
                << "    enableBitstreamPadding: " << pConfig->encodeCodecConfig.av1Config.enableBitstreamPadding << std::endl
                << "    enableCustomTileConfig: " << pConfig->encodeCodecConfig.av1Config.enableCustomTileConfig << std::endl
                << "    enableFilmGrainParams: " << pConfig->encodeCodecConfig.av1Config.enableFilmGrainParams << std::endl
                << "    inputPixelBitDepthMinus8: " << pConfig->encodeCodecConfig.av1Config.inputPixelBitDepthMinus8 << std::endl
                << "    pixelBitDepthMinus8: " << pConfig->encodeCodecConfig.av1Config.pixelBitDepthMinus8 << std::endl
                << "    idrPeriod: " << pConfig->encodeCodecConfig.av1Config.idrPeriod << std::endl
                << "    intraRefreshPeriod: " << pConfig->encodeCodecConfig.av1Config.intraRefreshPeriod << std::endl
                << "    intraRefreshCnt: " << pConfig->encodeCodecConfig.av1Config.intraRefreshCnt << std::endl
                << "    maxNumRefFramesInDPB: " << pConfig->encodeCodecConfig.av1Config.maxNumRefFramesInDPB << std::endl
                << "    numTileColumns: " << pConfig->encodeCodecConfig.av1Config.numTileColumns << std::endl
                << "    numTileRows: " << pConfig->encodeCodecConfig.av1Config.numTileRows << std::endl
                << "    maxTemporalLayersMinus1: " << pConfig->encodeCodecConfig.av1Config.maxTemporalLayersMinus1 << std::endl
                << "    colorPrimaries: " << pConfig->encodeCodecConfig.av1Config.colorPrimaries << std::endl
                << "    transferCharacteristics: " << pConfig->encodeCodecConfig.av1Config.transferCharacteristics << std::endl
                << "    matrixCoefficients: " << pConfig->encodeCodecConfig.av1Config.matrixCoefficients << std::endl
                << "    colorRange: " << pConfig->encodeCodecConfig.av1Config.colorRange << std::endl
                << "    chromaSamplePosition: " << pConfig->encodeCodecConfig.av1Config.chromaSamplePosition << std::endl
                << "    useBFramesAsRef: " << pConfig->encodeCodecConfig.av1Config.useBFramesAsRef << std::endl
                << "    numFwdRefs: " << pConfig->encodeCodecConfig.av1Config.numFwdRefs << std::endl
                << "    numBwdRefs: " << pConfig->encodeCodecConfig.av1Config.numBwdRefs << std::endl;
            if (pConfig->encodeCodecConfig.av1Config.filmGrainParams != NULL)
            {
                os
                    << "    NV_ENC_FILM_GRAIN_PARAMS_AV1:" << std::endl
                    << "        applyGrain: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->applyGrain << std::endl
                    << "        chromaScalingFromLuma: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->chromaScalingFromLuma << std::endl
                    << "        overlapFlag: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->overlapFlag << std::endl
                    << "        clipToRestrictedRange: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->clipToRestrictedRange << std::endl
                    << "        grainScalingMinus8: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->grainScalingMinus8 << std::endl
                    << "        arCoeffLag: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->arCoeffLag << std::endl
                    << "        numYPoints: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->numYPoints << std::endl
                    << "        numCbPoints: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->numCbPoints << std::endl
                    << "        numCrPoints: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->numCrPoints << std::endl
                    << "        arCoeffShiftMinus6: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->arCoeffShiftMinus6 << std::endl
                    << "        grainScaleShift: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->grainScaleShift << std::endl
                    << "        cbMult: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->cbMult << std::endl
                    << "        cbLumaMult: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->cbLumaMult << std::endl
                    << "        cbOffset: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->cbOffset << std::endl
                    << "        crMult: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->crMult << std::endl
                    << "        crLumaMult: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->crLumaMult << std::endl
                    << "        crOffset: " << pConfig->encodeCodecConfig.av1Config.filmGrainParams->crOffset << std::endl;
            }
        }

        return os.str();
    }
};














#include <ios>
#include <sstream>
#include <thread>
#include <list>
#include <vector>
#include <condition_variable>

extern simplelogger::Logger *logger;

#ifdef __cuda_cuda_h__
inline bool check(CUresult e, int iLine, const char *szFile) {
    if (e != CUDA_SUCCESS) {
        const char *szErrName = NULL;
        cuGetErrorName(e, &szErrName);
        LOG(FATAL) << "CUDA driver API error " << szErrName << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif

#ifdef __CUDA_RUNTIME_H__
inline bool check(cudaError_t e, int iLine, const char *szFile) {
    if (e != cudaSuccess) {
        LOG(FATAL) << "CUDA runtime API error " << cudaGetErrorName(e) << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif

#ifdef _NV_ENCODEAPI_H_
inline bool check(NVENCSTATUS e, int iLine, const char *szFile) {
    const char *aszErrName[] = {
        "NV_ENC_SUCCESS",
        "NV_ENC_ERR_NO_ENCODE_DEVICE",
        "NV_ENC_ERR_UNSUPPORTED_DEVICE",
        "NV_ENC_ERR_INVALID_ENCODERDEVICE",
        "NV_ENC_ERR_INVALID_DEVICE",
        "NV_ENC_ERR_DEVICE_NOT_EXIST",
        "NV_ENC_ERR_INVALID_PTR",
        "NV_ENC_ERR_INVALID_EVENT",
        "NV_ENC_ERR_INVALID_PARAM",
        "NV_ENC_ERR_INVALID_CALL",
        "NV_ENC_ERR_OUT_OF_MEMORY",
        "NV_ENC_ERR_ENCODER_NOT_INITIALIZED",
        "NV_ENC_ERR_UNSUPPORTED_PARAM",
        "NV_ENC_ERR_LOCK_BUSY",
        "NV_ENC_ERR_NOT_ENOUGH_BUFFER",
        "NV_ENC_ERR_INVALID_VERSION",
        "NV_ENC_ERR_MAP_FAILED",
        "NV_ENC_ERR_NEED_MORE_INPUT",
        "NV_ENC_ERR_ENCODER_BUSY",
        "NV_ENC_ERR_EVENT_NOT_REGISTERD",
        "NV_ENC_ERR_GENERIC",
        "NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY",
        "NV_ENC_ERR_UNIMPLEMENTED",
        "NV_ENC_ERR_RESOURCE_REGISTER_FAILED",
        "NV_ENC_ERR_RESOURCE_NOT_REGISTERED",
        "NV_ENC_ERR_RESOURCE_NOT_MAPPED",
    };
    if (e != NV_ENC_SUCCESS) {
        LOG(FATAL) << "NVENC error " << aszErrName[e] << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif

#ifdef _WINERROR_
inline bool check(HRESULT e, int iLine, const char *szFile) {
    if (e != S_OK) {
        std::stringstream stream;
        stream << std::hex << std::uppercase << e;
        LOG(FATAL) << "HRESULT error 0x" << stream.str() << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif

#if defined(__gl_h_) || defined(__GL_H__)
inline bool check(GLenum e, int iLine, const char *szFile) {
    if (e != 0) {
        LOG(ERROR) << "GLenum error " << e << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif

inline bool check(int e, int iLine, const char *szFile) {
    if (e < 0) {
        LOG(ERROR) << "General error " << e << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}

#define ck(call) check(call, __LINE__, __FILE__)
#define MAKE_FOURCC( ch0, ch1, ch2, ch3 )                               \
                ( (uint32_t)(uint8_t)(ch0) | ( (uint32_t)(uint8_t)(ch1) << 8 ) |    \
                ( (uint32_t)(uint8_t)(ch2) << 16 ) | ( (uint32_t)(uint8_t)(ch3) << 24 ) )

/**
* @brief Wrapper class around std::thread
*/
class NvThread
{
public:
    NvThread() = default;
    NvThread(const NvThread&) = delete;
    NvThread& operator=(const NvThread& other) = delete;

    NvThread(std::thread&& thread) : t(std::move(thread))
    {

    }

    NvThread(NvThread&& thread) : t(std::move(thread.t))
    {

    }

    NvThread& operator=(NvThread&& other)
    {
        t = std::move(other.t);
        return *this;
    }

    ~NvThread()
    {
        join();
    }

    void join()
    {
        if (t.joinable())
        {
            t.join();
        }
    }
private:
    std::thread t;
};

#ifndef _WIN32
#define _stricmp strcasecmp
#define _stat64 stat64
#endif

/**
* @brief Utility class to allocate buffer memory. Helps avoid I/O during the encode/decode loop in case of performance tests.
*/
class BufferedFileReader {
public:
    /**
    * @brief Constructor function to allocate appropriate memory and copy file contents into it
    */
    BufferedFileReader(const char *szFileName, bool bPartial = false) {
        struct _stat64 st;

        if (_stat64(szFileName, &st) != 0) {
            return;
        }
        
        nSize = st.st_size;
        while (nSize) {
            try {
                pBuf = new uint8_t[(size_t)nSize];
                if (nSize != st.st_size) {
                    LOG(WARNING) << "File is too large - only " << std::setprecision(4) << 100.0 * nSize / st.st_size << "% is loaded"; 
                }
                break;
            } catch(std::bad_alloc) {
                if (!bPartial) {
                    LOG(ERROR) << "Failed to allocate memory in BufferedReader";
                    return;
                }
                nSize = (uint32_t)(nSize * 0.9);
            }
        }

        std::ifstream fpIn(szFileName, std::ifstream::in | std::ifstream::binary);
        if (!fpIn)
        {
            LOG(ERROR) << "Unable to open input file: " << szFileName;
            return;
        }

        std::streamsize nRead = fpIn.read(reinterpret_cast<char*>(pBuf), nSize).gcount();
        fpIn.close();

        assert(nRead == nSize);
    }
    ~BufferedFileReader() {
        if (pBuf) {
            delete[] pBuf;
        }
    }
    bool GetBuffer(uint8_t **ppBuf, uint64_t *pnSize) {
        if (!pBuf) {
            return false;
        }

        *ppBuf = pBuf;
        *pnSize = nSize;
        return true;
    }

private:
    uint8_t *pBuf = NULL;
    uint64_t nSize = 0;
};

/**
* @brief Template class to facilitate color space conversion
*/
template<typename T>
class YuvConverter {
public:
    YuvConverter(int nWidth, int nHeight) : nWidth(nWidth), nHeight(nHeight) {
        pQuad = new T[((nWidth + 1) / 2) * ((nHeight + 1) / 2)];
    }
    ~YuvConverter() {
        delete[] pQuad;
    }
    void PlanarToUVInterleaved(T *pFrame, int nPitch = 0) {
        if (nPitch == 0) {
            nPitch = nWidth;
        }

        // sizes of source surface plane
        int nSizePlaneY = nPitch * nHeight;
        int nSizePlaneU = ((nPitch + 1) / 2) * ((nHeight + 1) / 2);
        int nSizePlaneV = nSizePlaneU;

        T *puv = pFrame + nSizePlaneY;
        if (nPitch == nWidth) {
            memcpy(pQuad, puv, nSizePlaneU * sizeof(T));
        } else {
            for (int i = 0; i < (nHeight + 1) / 2; i++) {
                memcpy(pQuad + ((nWidth + 1) / 2) * i, puv + ((nPitch + 1) / 2) * i, ((nWidth + 1) / 2) * sizeof(T));
            }
        }
        T *pv = puv + nSizePlaneU;
        for (int y = 0; y < (nHeight + 1) / 2; y++) {
            for (int x = 0; x < (nWidth + 1) / 2; x++) {
                puv[y * nPitch + x * 2] = pQuad[y * ((nWidth + 1) / 2) + x];
                puv[y * nPitch + x * 2 + 1] = pv[y * ((nPitch + 1) / 2) + x];
            }
        }
    }
    void UVInterleavedToPlanar(T *pFrame, int nPitch = 0) {
        if (nPitch == 0) {
            nPitch = nWidth;
        }

        // sizes of source surface plane
        int nSizePlaneY = nPitch * nHeight;
        int nSizePlaneU = ((nPitch + 1) / 2) * ((nHeight + 1) / 2);
        int nSizePlaneV = nSizePlaneU;

        T *puv = pFrame + nSizePlaneY,
            *pu = puv, 
            *pv = puv + nSizePlaneU;

        // split chroma from interleave to planar
        for (int y = 0; y < (nHeight + 1) / 2; y++) {
            for (int x = 0; x < (nWidth + 1) / 2; x++) {
                pu[y * ((nPitch + 1) / 2) + x] = puv[y * nPitch + x * 2];
                pQuad[y * ((nWidth + 1) / 2) + x] = puv[y * nPitch + x * 2 + 1];
            }
        }
        if (nPitch == nWidth) {
            memcpy(pv, pQuad, nSizePlaneV * sizeof(T));
        } else {
            for (int i = 0; i < (nHeight + 1) / 2; i++) {
                memcpy(pv + ((nPitch + 1) / 2) * i, pQuad + ((nWidth + 1) / 2) * i, ((nWidth + 1) / 2) * sizeof(T));
            }
        }
    }

private:
    T *pQuad;
    int nWidth, nHeight;
};

/**
* @brief Class for writing IVF format header for AV1 codec
*/
class IVFUtils {
public:
    void WriteFileHeader(std::vector<uint8_t> &vPacket, uint32_t nFourCC, uint32_t nWidth, uint32_t nHeight, uint32_t nFrameRateNum, uint32_t nFrameRateDen, uint32_t nFrameCnt)
    {
        char header[32];

        header[0] = 'D';
        header[1] = 'K';
        header[2] = 'I';
        header[3] = 'F';
        mem_put_le16(header + 4, 0);                    // version
        mem_put_le16(header + 6, 32);                   // header size
        mem_put_le32(header + 8, nFourCC);              // fourcc
        mem_put_le16(header + 12, nWidth);              // width
        mem_put_le16(header + 14, nHeight);             // height
        mem_put_le32(header + 16, nFrameRateNum);       // rate
        mem_put_le32(header + 20, nFrameRateDen);       // scale
        mem_put_le32(header + 24, nFrameCnt);           // length
        mem_put_le32(header + 28, 0);                   // unused

        vPacket.insert(vPacket.end(), &header[0], &header[32]);
    }
    
    void WriteFrameHeader(std::vector<uint8_t> &vPacket,  size_t nFrameSize, int64_t pts)
    {
        char header[12];
        mem_put_le32(header, (int)nFrameSize);
        mem_put_le32(header + 4, (int)(pts & 0xFFFFFFFF));
        mem_put_le32(header + 8, (int)(pts >> 32));
        
        vPacket.insert(vPacket.end(), &header[0], &header[12]);
    }
    
private:
    static inline void mem_put_le32(void *vmem, int val)
    {
        unsigned char *mem = (unsigned char *)vmem;
        mem[0] = (unsigned char)((val >>  0) & 0xff);
        mem[1] = (unsigned char)((val >>  8) & 0xff);
        mem[2] = (unsigned char)((val >> 16) & 0xff);
        mem[3] = (unsigned char)((val >> 24) & 0xff);
    }

    static inline void mem_put_le16(void *vmem, int val)
    {
        unsigned char *mem = (unsigned char *)vmem;
        mem[0] = (unsigned char)((val >>  0) & 0xff);
        mem[1] = (unsigned char)((val >>  8) & 0xff);
    }

};
    
/**
* @brief Utility class to measure elapsed time in seconds between the block of executed code
*/
class StopWatch {
public:
    void Start() {
        t0 = std::chrono::high_resolution_clock::now();
    }
    double Stop() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch() - t0.time_since_epoch()).count() / 1.0e9;
    }

private:
    std::chrono::high_resolution_clock::time_point t0;
};

template<typename T>
class ConcurrentQueue
{
    public:

    ConcurrentQueue() {}
    ConcurrentQueue(size_t size) : maxSize(size) {}
    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

    void setSize(size_t s) {
        maxSize = s;
    }

    void push_back(const T& value) {
        // Do not use a std::lock_guard here. We will need to explicitly
        // unlock before notify_one as the other waiting thread will
        // automatically try to acquire mutex once it wakes up
        // (which will happen on notify_one)
        std::unique_lock<std::mutex> lock(m_mutex);
        auto wasEmpty = m_List.empty();

        while (full()) {
            m_cond.wait(lock);
        }

        m_List.push_back(value);
        if (wasEmpty && !m_List.empty()) {
            lock.unlock();
            m_cond.notify_one();
        }
    }

    T pop_front() {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_List.empty()) {
            m_cond.wait(lock);
        }
        auto wasFull = full();
        T data = std::move(m_List.front());
        m_List.pop_front();

        if (wasFull && !full()) {
            lock.unlock();
            m_cond.notify_one();
        }

        return data;
    }

    T front() {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_List.empty()) {
            m_cond.wait(lock);
        }

        return m_List.front();
    }

    size_t size() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_List.size();
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_List.empty();
    }
    void clear() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_List.clear();
    }

private:
    bool full() {
        if (maxSize > 0 && m_List.size() == maxSize)
            return true;
        return false;
    }

private:
    std::list<T> m_List;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    size_t maxSize;
};

inline void CheckInputFile(const char *szInFilePath) {
    std::ifstream fpIn(szInFilePath, std::ios::in | std::ios::binary);
    if (fpIn.fail()) {
        std::ostringstream err;
        err << "Unable to open input file: " << szInFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }
}

inline void ValidateResolution(int nWidth, int nHeight) {
    
    if (nWidth <= 0 || nHeight <= 0) {
        std::ostringstream err;
        err << "Please specify positive non zero resolution as -s WxH. Current resolution is " << nWidth << "x" << nHeight << std::endl;
        throw std::invalid_argument(err.str());
    }
}

template <class COLOR32>
void Nv12ToColor32(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 0);
template <class COLOR64>
void Nv12ToColor64(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 0);

template <class COLOR32>
void P016ToColor32(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 4);
template <class COLOR64>
void P016ToColor64(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 4);

template <class COLOR32>
void YUV444ToColor32(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 0);
template <class COLOR64>
void YUV444ToColor64(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 0);

template <class COLOR32>
void YUV444P16ToColor32(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 4);
template <class COLOR64>
void YUV444P16ToColor64(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 4);

template <class COLOR32>
void Nv12ToColorPlanar(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgrp, int nBgrpPitch, int nWidth, int nHeight, int iMatrix = 0);
template <class COLOR32>
void P016ToColorPlanar(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgrp, int nBgrpPitch, int nWidth, int nHeight, int iMatrix = 4);

template <class COLOR32>
void YUV444ToColorPlanar(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgrp, int nBgrpPitch, int nWidth, int nHeight, int iMatrix = 0);
template <class COLOR32>
void YUV444P16ToColorPlanar(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgrp, int nBgrpPitch, int nWidth, int nHeight, int iMatrix = 4);

void Bgra64ToP016(uint8_t *dpBgra, int nBgraPitch, uint8_t *dpP016, int nP016Pitch, int nWidth, int nHeight, int iMatrix = 4);

void ConvertUInt8ToUInt16(uint8_t *dpUInt8, uint16_t *dpUInt16, int nSrcPitch, int nDestPitch, int nWidth, int nHeight);
void ConvertUInt16ToUInt8(uint16_t *dpUInt16, uint8_t *dpUInt8, int nSrcPitch, int nDestPitch, int nWidth, int nHeight);

void ResizeNv12(unsigned char *dpDstNv12, int nDstPitch, int nDstWidth, int nDstHeight, unsigned char *dpSrcNv12, int nSrcPitch, int nSrcWidth, int nSrcHeight, unsigned char *dpDstNv12UV = nullptr);
void ResizeP016(unsigned char *dpDstP016, int nDstPitch, int nDstWidth, int nDstHeight, unsigned char *dpSrcP016, int nSrcPitch, int nSrcWidth, int nSrcHeight, unsigned char *dpDstP016UV = nullptr);

void ScaleYUV420(unsigned char *dpDstY, unsigned char* dpDstU, unsigned char* dpDstV, int nDstPitch, int nDstChromaPitch, int nDstWidth, int nDstHeight,
    unsigned char *dpSrcY, unsigned char* dpSrcU, unsigned char* dpSrcV, int nSrcPitch, int nSrcChromaPitch, int nSrcWidth, int nSrcHeight, bool bSemiplanar);

#ifdef __cuda_cuda_h__
void ComputeCRC(uint8_t *pBuffer, uint32_t *crcValue, CUstream_st *outputCUStream);
#endif


/**
* @brief Exception class for error reporting from NvEncodeAPI calls.
*/
class NVENCException : public std::exception
{
public:
    NVENCException(const std::string& errorStr, const NVENCSTATUS errorCode)
        : m_errorString(errorStr), m_errorCode(errorCode) {}

    virtual ~NVENCException() throw() {}
    virtual const char* what() const throw() { return m_errorString.c_str(); }
    NVENCSTATUS  getErrorCode() const { return m_errorCode; }
    const std::string& getErrorString() const { return m_errorString; }
    static NVENCException makeNVENCException(const std::string& errorStr, const NVENCSTATUS errorCode,
        const std::string& functionName, const std::string& fileName, int lineNo);
private:
    std::string m_errorString;
    NVENCSTATUS m_errorCode;
};

inline NVENCException NVENCException::makeNVENCException(const std::string& errorStr, const NVENCSTATUS errorCode, const std::string& functionName,
    const std::string& fileName, int lineNo)
{
    std::ostringstream errorLog;
    errorLog << functionName << " : " << errorStr << " at " << fileName << ":" << lineNo << std::endl;
    NVENCException exception(errorLog.str(), errorCode);
    return exception;
}

#define NVENC_THROW_ERROR( errorStr, errorCode )                                                         \
    do                                                                                                   \
    {                                                                                                    \
        throw NVENCException::makeNVENCException(errorStr, errorCode, __FUNCTION__, __FILE__, __LINE__); \
    } while (0)


#define NVENC_API_CALL( nvencAPI )                                                                                 \
    do                                                                                                             \
    {                                                                                                              \
        NVENCSTATUS errorCode = nvencAPI;                                                                          \
        if( errorCode != NV_ENC_SUCCESS)                                                                           \
        {                                                                                                          \
            std::ostringstream errorLog;                                                                           \
            errorLog << #nvencAPI << " returned error " << errorCode;                                              \
            throw NVENCException::makeNVENCException(errorLog.str(), errorCode, __FUNCTION__, __FILE__, __LINE__); \
        }                                                                                                          \
    } while (0)

struct NvEncInputFrame
{
    void* inputPtr = nullptr;
    uint32_t chromaOffsets[2];
    uint32_t numChromaPlanes;
    uint32_t pitch;
    uint32_t chromaPitch;
    NV_ENC_BUFFER_FORMAT bufferFormat;
    NV_ENC_INPUT_RESOURCE_TYPE resourceType;
};

/**
* @brief Shared base class for different encoder interfaces.
*/
class NvEncoder
{
public:
    /**
    *  @brief This function is used to initialize the encoder session.
    *  Application must call this function to initialize the encoder, before
    *  starting to encode any frames.
    */
    virtual void CreateEncoder(const NV_ENC_INITIALIZE_PARAMS* pEncodeParams);

    /**
    *  @brief  This function is used to destroy the encoder session.
    *  Application must call this function to destroy the encoder session and
    *  clean up any allocated resources. The application must call EndEncode()
    *  function to get any queued encoded frames before calling DestroyEncoder().
    */
    virtual void DestroyEncoder();

    /**
    *  @brief  This function is used to reconfigure an existing encoder session.
    *  Application can use this function to dynamically change the bitrate,
    *  resolution and other QOS parameters. If the application changes the
    *  resolution, it must set NV_ENC_RECONFIGURE_PARAMS::forceIDR.
    */
    bool Reconfigure(const NV_ENC_RECONFIGURE_PARAMS *pReconfigureParams);

    /**
    *  @brief  This function is used to get the next available input buffer.
    *  Applications must call this function to obtain a pointer to the next
    *  input buffer. The application must copy the uncompressed data to the
    *  input buffer and then call EncodeFrame() function to encode it.
    */
    const NvEncInputFrame* GetNextInputFrame();


    /**
    *  @brief  This function is used to encode a frame.
    *  Applications must call EncodeFrame() function to encode the uncompressed
    *  data, which has been copied to an input buffer obtained from the
    *  GetNextInputFrame() function.
    */
    virtual void EncodeFrame(std::vector<std::vector<uint8_t>> &vPacket, NV_ENC_PIC_PARAMS *pPicParams = nullptr);

    /**
    *  @brief  This function to flush the encoder queue.
    *  The encoder might be queuing frames for B picture encoding or lookahead;
    *  the application must call EndEncode() to get all the queued encoded frames
    *  from the encoder. The application must call this function before destroying
    *  an encoder session.
    */
    virtual void EndEncode(std::vector<std::vector<uint8_t>> &vPacket);

    /**
    *  @brief  This function is used to query hardware encoder capabilities.
    *  Applications can call this function to query capabilities like maximum encode
    *  dimensions, support for lookahead or the ME-only mode etc.
    */
    int GetCapabilityValue(GUID guidCodec, NV_ENC_CAPS capsToQuery);

    /**
    *  @brief  This function is used to get the current device on which encoder is running.
    */
    void *GetDevice() const { return m_pDevice; }

    /**
    *  @brief  This function is used to get the current device type which encoder is running.
    */
    NV_ENC_DEVICE_TYPE GetDeviceType() const { return m_eDeviceType; }

    /**
    *  @brief  This function is used to get the current encode width.
    *  The encode width can be modified by Reconfigure() function.
    */
    int GetEncodeWidth() const { return m_nWidth; }

    /**
    *  @brief  This function is used to get the current encode height.
    *  The encode height can be modified by Reconfigure() function.
    */
    int GetEncodeHeight() const { return m_nHeight; }

    /**
    *   @brief  This function is used to get the current frame size based on pixel format.
    */
    int GetFrameSize() const;

    /**
    *  @brief  This function is used to initialize config parameters based on
    *          given codec and preset guids.
    *  The application can call this function to get the default configuration
    *  for a certain preset. The application can either use these parameters
    *  directly or override them with application-specific settings before
    *  using them in CreateEncoder() function.
    */
    void CreateDefaultEncoderParams(NV_ENC_INITIALIZE_PARAMS* pIntializeParams, GUID codecGuid, GUID presetGuid, NV_ENC_TUNING_INFO tuningInfo = NV_ENC_TUNING_INFO_UNDEFINED);

    /**
    *  @brief  This function is used to get the current initialization parameters,
    *          which had been used to configure the encoder session.
    *  The initialization parameters are modified if the application calls
    *  Reconfigure() function.
    */
    void GetInitializeParams(NV_ENC_INITIALIZE_PARAMS *pInitializeParams);

    /**
    *  @brief  This function is used to run motion estimation
    *  This is used to run motion estimation on a a pair of frames. The
    *  application must copy the reference frame data to the buffer obtained
    *  by calling GetNextReferenceFrame(), and copy the input frame data to
    *  the buffer obtained by calling GetNextInputFrame() before calling the
    *  RunMotionEstimation() function.
    */
    void RunMotionEstimation(std::vector<uint8_t> &mvData);

    /**
    *  @brief This function is used to get an available reference frame.
    *  Application must call this function to get a pointer to reference buffer,
    *  to be used in the subsequent RunMotionEstimation() function.
    */
    const NvEncInputFrame* GetNextReferenceFrame();

    /**
    *  @brief This function is used to get sequence and picture parameter headers.
    *  Application can call this function after encoder is initialized to get SPS and PPS
    *  nalus for the current encoder instance. The sequence header data might change when
    *  application calls Reconfigure() function.
    */
    void GetSequenceParams(std::vector<uint8_t> &seqParams);

    /**
    *  @brief  NvEncoder class virtual destructor.
    */
    virtual ~NvEncoder();

public:
    /**
    *  @brief This a static function to get chroma offsets for YUV planar formats.
    */
    static void GetChromaSubPlaneOffsets(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t pitch,
                                        const uint32_t height, std::vector<uint32_t>& chromaOffsets);
    /**
    *  @brief This a static function to get the chroma plane pitch for YUV planar formats.
    */
    static uint32_t GetChromaPitch(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t lumaPitch);

    /**
    *  @brief This a static function to get the number of chroma planes for YUV planar formats.
    */
    static uint32_t GetNumChromaPlanes(const NV_ENC_BUFFER_FORMAT bufferFormat);

    /**
    *  @brief This a static function to get the chroma plane width in bytes for YUV planar formats.
    */
    static uint32_t GetChromaWidthInBytes(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t lumaWidth);

    /**
    *  @brief This a static function to get the chroma planes height in bytes for YUV planar formats.
    */
    static uint32_t GetChromaHeight(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t lumaHeight);


    /**
    *  @brief This a static function to get the width in bytes for the frame.
    *  For YUV planar format this is the width in bytes of the luma plane.
    */
    static uint32_t GetWidthInBytes(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t width);

    /**
    *  @brief This function returns the number of allocated buffers.
    */
    uint32_t GetEncoderBufferCount() const { return m_nEncoderBuffer; }

    /*
    * @brief This function returns initializeParams(width, height, fps etc).
    */
    NV_ENC_INITIALIZE_PARAMS GetinitializeParams() const { return m_initializeParams; }
protected:

    /**
    *  @brief  NvEncoder class constructor.
    *  NvEncoder class constructor cannot be called directly by the application.
    */
    NvEncoder(NV_ENC_DEVICE_TYPE eDeviceType, void *pDevice, uint32_t nWidth, uint32_t nHeight,
        NV_ENC_BUFFER_FORMAT eBufferFormat, uint32_t nOutputDelay, bool bMotionEstimationOnly, bool bOutputInVideoMemory = false, bool bDX12Encode = false,
        bool bUseIVFContainer = true);

    /**
    *  @brief This function is used to check if hardware encoder is properly initialized.
    */
    bool IsHWEncoderInitialized() const { return m_hEncoder != NULL && m_bEncoderInitialized; }

    /**
    *  @brief This function is used to register CUDA, D3D or OpenGL input buffers with NvEncodeAPI.
    *  This is non public function and is called by derived class for allocating
    *  and registering input buffers.
    */
    void RegisterInputResources(std::vector<void*> inputframes, NV_ENC_INPUT_RESOURCE_TYPE eResourceType,
        int width, int height, int pitch, NV_ENC_BUFFER_FORMAT bufferFormat, bool bReferenceFrame = false);

    /**
    *  @brief This function is used to unregister resources which had been previously registered for encoding
    *         using RegisterInputResources() function.
    */
    void UnregisterInputResources();

    /**
    *  @brief This function is used to register CUDA, D3D or OpenGL input or output buffers with NvEncodeAPI.
    */
    NV_ENC_REGISTERED_PTR RegisterResource(void *pBuffer, NV_ENC_INPUT_RESOURCE_TYPE eResourceType,
        int width, int height, int pitch, NV_ENC_BUFFER_FORMAT bufferFormat, NV_ENC_BUFFER_USAGE bufferUsage = NV_ENC_INPUT_IMAGE,
        NV_ENC_FENCE_POINT_D3D12* pInputFencePoint = NULL);

    /**
    *  @brief This function returns maximum width used to open the encoder session.
    *  All encode input buffers are allocated using maximum dimensions.
    */
    uint32_t GetMaxEncodeWidth() const { return m_nMaxEncodeWidth; }

    /**
    *  @brief This function returns maximum height used to open the encoder session.
    *  All encode input buffers are allocated using maximum dimensions.
    */
    uint32_t GetMaxEncodeHeight() const { return m_nMaxEncodeHeight; }

    /**
    *  @brief This function returns the completion event.
    */
    void* GetCompletionEvent(uint32_t eventIdx) { return (m_vpCompletionEvent.size() == m_nEncoderBuffer) ? m_vpCompletionEvent[eventIdx] : nullptr; }

    /**
    *  @brief This function returns the current pixel format.
    */
    NV_ENC_BUFFER_FORMAT GetPixelFormat() const { return m_eBufferFormat; }

    /**
    *  @brief This function is used to submit the encode commands to the  
    *         NVENC hardware.
    */
    NVENCSTATUS DoEncode(NV_ENC_INPUT_PTR inputBuffer, NV_ENC_OUTPUT_PTR outputBuffer, NV_ENC_PIC_PARAMS *pPicParams);

    /**
    *  @brief This function is used to submit the encode commands to the 
    *         NVENC hardware for ME only mode.
    */
    NVENCSTATUS DoMotionEstimation(NV_ENC_INPUT_PTR inputBuffer, NV_ENC_INPUT_PTR inputBufferForReference, NV_ENC_OUTPUT_PTR outputBuffer);

    /**
    *  @brief This function is used to map the input buffers to NvEncodeAPI.
    */
    void MapResources(uint32_t bfrIdx);

    /**
    *  @brief This function is used to wait for completion of encode command.
    */
    void WaitForCompletionEvent(int iEvent);

    /**
    *  @brief This function is used to send EOS to HW encoder.
    */
    void SendEOS();

private:
    /**
    *  @brief This is a private function which is used to check if there is any
              buffering done by encoder.
    *  The encoder generally buffers data to encode B frames or for lookahead
    *  or pipelining.
    */
    bool IsZeroDelay() { return m_nOutputDelay == 0; }

    /**
    *  @brief This is a private function which is used to load the encode api shared library.
    */
    void LoadNvEncApi();

    /**
    *  @brief This is a private function which is used to get the output packets
    *         from the encoder HW.
    *  This is called by DoEncode() function. If there is buffering enabled,
    *  this may return without any output data.
    */
    void GetEncodedPacket(std::vector<NV_ENC_OUTPUT_PTR> &vOutputBuffer, std::vector<std::vector<uint8_t>> &vPacket, bool bOutputDelay);

    /**
    *  @brief This is a private function which is used to initialize the bitstream buffers.
    *  This is only used in the encoding mode.
    */
    void InitializeBitstreamBuffer();

    /**
    *  @brief This is a private function which is used to destroy the bitstream buffers.
    *  This is only used in the encoding mode.
    */
    void DestroyBitstreamBuffer();

    /**
    *  @brief This is a private function which is used to initialize MV output buffers.
    *  This is only used in ME-only Mode.
    */
    void InitializeMVOutputBuffer();

    /**
    *  @brief This is a private function which is used to destroy MV output buffers.
    *  This is only used in ME-only Mode.
    */
    void DestroyMVOutputBuffer();

    /**
    *  @brief This is a private function which is used to destroy HW encoder.
    */
    void DestroyHWEncoder();

    /**
    *  @brief This function is used to flush the encoder queue.
    */
    void FlushEncoder();

private:
    /**
    *  @brief This is a pure virtual function which is used to allocate input buffers.
    *  The derived classes must implement this function.
    */
    virtual void AllocateInputBuffers(int32_t numInputBuffers) = 0;

    /**
    *  @brief This is a pure virtual function which is used to destroy input buffers.
    *  The derived classes must implement this function.
    */
    virtual void ReleaseInputBuffers() = 0;

protected:
    bool m_bMotionEstimationOnly = false;
    bool m_bOutputInVideoMemory = false;
    bool m_bIsDX12Encode = false;
    void *m_hEncoder = nullptr;
    NV_ENCODE_API_FUNCTION_LIST m_nvenc;
    NV_ENC_INITIALIZE_PARAMS m_initializeParams = {};
    std::vector<NvEncInputFrame> m_vInputFrames;
    std::vector<NV_ENC_REGISTERED_PTR> m_vRegisteredResources;
    std::vector<NvEncInputFrame> m_vReferenceFrames;
    std::vector<NV_ENC_REGISTERED_PTR> m_vRegisteredResourcesForReference;
    std::vector<NV_ENC_INPUT_PTR> m_vMappedInputBuffers;
    std::vector<NV_ENC_INPUT_PTR> m_vMappedRefBuffers;
    std::vector<void *> m_vpCompletionEvent;

    int32_t m_iToSend = 0;
    int32_t m_iGot = 0;
    int32_t m_nEncoderBuffer = 0;
    int32_t m_nOutputDelay = 0;
    IVFUtils m_IVFUtils;
    bool m_bWriteIVFFileHeader = true;
    bool m_bUseIVFContainer = true;

private:
    uint32_t m_nWidth;
    uint32_t m_nHeight;
    NV_ENC_BUFFER_FORMAT m_eBufferFormat;
    void *m_pDevice;
    NV_ENC_DEVICE_TYPE m_eDeviceType;
    NV_ENC_CONFIG m_encodeConfig = {};
    bool m_bEncoderInitialized = false;
    uint32_t m_nExtraOutputDelay = 3; // To ensure encode and graphics can work in parallel, m_nExtraOutputDelay should be set to at least 1
    std::vector<NV_ENC_OUTPUT_PTR> m_vBitstreamOutputBuffer;
    std::vector<NV_ENC_OUTPUT_PTR> m_vMVDataOutputBuffer;
    uint32_t m_nMaxEncodeWidth = 0;
    uint32_t m_nMaxEncodeHeight = 0;
};


/*
#ifndef _WIN32
#include <cstring>
static inline bool operator==(const GUID &guid1, const GUID &guid2) {
    return !memcmp(&guid1, &guid2, sizeof(GUID));
}

static inline bool operator!=(const GUID &guid1, const GUID &guid2) {
    return !(guid1 == guid2);
}
#endif
*/

NvEncoder::NvEncoder(NV_ENC_DEVICE_TYPE eDeviceType, void *pDevice, uint32_t nWidth, uint32_t nHeight, NV_ENC_BUFFER_FORMAT eBufferFormat,
                            uint32_t nExtraOutputDelay, bool bMotionEstimationOnly, bool bOutputInVideoMemory, bool bDX12Encode, bool bUseIVFContainer) :
    m_pDevice(pDevice), 
    m_eDeviceType(eDeviceType),
    m_nWidth(nWidth),
    m_nHeight(nHeight),
    m_nMaxEncodeWidth(nWidth),
    m_nMaxEncodeHeight(nHeight),
    m_eBufferFormat(eBufferFormat), 
    m_bMotionEstimationOnly(bMotionEstimationOnly), 
    m_bOutputInVideoMemory(bOutputInVideoMemory),
    m_bIsDX12Encode(bDX12Encode),
    m_bUseIVFContainer(bUseIVFContainer),
    m_nExtraOutputDelay(nExtraOutputDelay), 
    m_hEncoder(nullptr)
{
    LoadNvEncApi();

    if (!m_nvenc.nvEncOpenEncodeSession) 
    {
        m_nEncoderBuffer = 0;
        NVENC_THROW_ERROR("EncodeAPI not found", NV_ENC_ERR_NO_ENCODE_DEVICE);
    }

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS encodeSessionExParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
    encodeSessionExParams.device = m_pDevice;
    encodeSessionExParams.deviceType = m_eDeviceType;
    encodeSessionExParams.apiVersion = NVENCAPI_VERSION;
    void *hEncoder = NULL;
    
    printf("%p %p %p\n", (void*)m_pDevice, (void*)m_eDeviceType, (void*)NVENCAPI_VERSION);

    NVENCSTATUS status = m_nvenc.nvEncOpenEncodeSessionEx(&encodeSessionExParams, &hEncoder);

    printf("status = %d\n", status);

    m_hEncoder = hEncoder;
}

void NvEncoder::LoadNvEncApi()
{

    uint32_t version = 0;
    uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
    NVENC_API_CALL(NvEncodeAPIGetMaxSupportedVersion(&version));
    if (currentVersion > version)
    {
        NVENC_THROW_ERROR("Current Driver Version does not support this NvEncodeAPI version, please upgrade driver", NV_ENC_ERR_INVALID_VERSION);
    }


    m_nvenc = { NV_ENCODE_API_FUNCTION_LIST_VER };
    NVENC_API_CALL(NvEncodeAPICreateInstance(&m_nvenc));
}

NvEncoder::~NvEncoder()
{
    DestroyHWEncoder();
}

void NvEncoder::CreateDefaultEncoderParams(NV_ENC_INITIALIZE_PARAMS* pIntializeParams, GUID codecGuid, GUID presetGuid, NV_ENC_TUNING_INFO tuningInfo)
{
    if (!m_hEncoder)
    {
        NVENC_THROW_ERROR("Encoder Initialization failed", NV_ENC_ERR_NO_ENCODE_DEVICE);
        return;
    }

    if (pIntializeParams == nullptr || pIntializeParams->encodeConfig == nullptr)
    {
        NVENC_THROW_ERROR("pInitializeParams and pInitializeParams->encodeConfig can't be NULL", NV_ENC_ERR_INVALID_PTR);
    }

    memset(pIntializeParams->encodeConfig, 0, sizeof(NV_ENC_CONFIG));
    auto pEncodeConfig = pIntializeParams->encodeConfig;
    memset(pIntializeParams, 0, sizeof(NV_ENC_INITIALIZE_PARAMS));
    pIntializeParams->encodeConfig = pEncodeConfig;


    pIntializeParams->encodeConfig->version = NV_ENC_CONFIG_VER;
    pIntializeParams->version = NV_ENC_INITIALIZE_PARAMS_VER;

    pIntializeParams->encodeGUID = codecGuid;
    pIntializeParams->presetGUID = presetGuid;
    pIntializeParams->encodeWidth = m_nWidth;
    pIntializeParams->encodeHeight = m_nHeight;
    pIntializeParams->darWidth = m_nWidth;
    pIntializeParams->darHeight = m_nHeight;
    pIntializeParams->frameRateNum = 30;
    pIntializeParams->frameRateDen = 1;
    pIntializeParams->enablePTD = 1;
    pIntializeParams->reportSliceOffsets = 0;
    pIntializeParams->enableSubFrameWrite = 0;
    pIntializeParams->maxEncodeWidth = m_nWidth;
    pIntializeParams->maxEncodeHeight = m_nHeight;
    pIntializeParams->enableMEOnlyMode = m_bMotionEstimationOnly;
    pIntializeParams->enableOutputInVidmem = m_bOutputInVideoMemory;
#if defined(_WIN32)
    if (!m_bOutputInVideoMemory)
    {
        pIntializeParams->enableEncodeAsync = GetCapabilityValue(codecGuid, NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT);
    }
#endif

    NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
    m_nvenc.nvEncGetEncodePresetConfig(m_hEncoder, codecGuid, presetGuid, &presetConfig);
    memcpy(pIntializeParams->encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
    pIntializeParams->encodeConfig->frameIntervalP = 1;
    pIntializeParams->encodeConfig->gopLength = NVENC_INFINITE_GOPLENGTH;

    pIntializeParams->encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;

    if (!m_bMotionEstimationOnly)
    {
        pIntializeParams->tuningInfo = tuningInfo;
        NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
        m_nvenc.nvEncGetEncodePresetConfigEx(m_hEncoder, codecGuid, presetGuid, tuningInfo, &presetConfig);
        memcpy(pIntializeParams->encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
    }
    else
    {
        m_encodeConfig.version = NV_ENC_CONFIG_VER;
        m_encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
        m_encodeConfig.rcParams.constQP = { 28, 31, 25 };
    }
    
    if (pIntializeParams->encodeGUID == NV_ENC_CODEC_H264_GUID)
    {
        if (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444 || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)
        {
            pIntializeParams->encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC = 3;
        }
        pIntializeParams->encodeConfig->encodeCodecConfig.h264Config.idrPeriod = pIntializeParams->encodeConfig->gopLength;
    }
    else if (pIntializeParams->encodeGUID == NV_ENC_CODEC_HEVC_GUID)
    {
        pIntializeParams->encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 =
            (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT ) ? 2 : 0;
        if (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444 || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)
        {
            pIntializeParams->encodeConfig->encodeCodecConfig.hevcConfig.chromaFormatIDC = 3;
        }
        pIntializeParams->encodeConfig->encodeCodecConfig.hevcConfig.idrPeriod = pIntializeParams->encodeConfig->gopLength;
    }
    else if (pIntializeParams->encodeGUID == NV_ENC_CODEC_AV1_GUID)
    {
        pIntializeParams->encodeConfig->encodeCodecConfig.av1Config.pixelBitDepthMinus8 = (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT) ? 2 : 0;
		pIntializeParams->encodeConfig->encodeCodecConfig.av1Config.inputPixelBitDepthMinus8 = (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT) ? 2 : 0;
        pIntializeParams->encodeConfig->encodeCodecConfig.av1Config.chromaFormatIDC = 1;
        pIntializeParams->encodeConfig->encodeCodecConfig.av1Config.idrPeriod = pIntializeParams->encodeConfig->gopLength;
        if (m_bOutputInVideoMemory)
        {
            pIntializeParams->encodeConfig->frameIntervalP = 1;
        }
    }

    if (m_bIsDX12Encode)
    {
        pIntializeParams->bufferFormat = m_eBufferFormat;
    }
    
    return;
}

void NvEncoder::CreateEncoder(const NV_ENC_INITIALIZE_PARAMS* pEncoderParams)
{
    if (!m_hEncoder)
    {
        NVENC_THROW_ERROR("Encoder Initialization failed", NV_ENC_ERR_NO_ENCODE_DEVICE);
    }

    if (!pEncoderParams)
    {
        NVENC_THROW_ERROR("Invalid NV_ENC_INITIALIZE_PARAMS ptr", NV_ENC_ERR_INVALID_PTR);
    }

    if (pEncoderParams->encodeWidth == 0 || pEncoderParams->encodeHeight == 0)
    {
        NVENC_THROW_ERROR("Invalid encoder width and height", NV_ENC_ERR_INVALID_PARAM);
    }

    if (pEncoderParams->encodeGUID != NV_ENC_CODEC_H264_GUID && pEncoderParams->encodeGUID != NV_ENC_CODEC_HEVC_GUID && pEncoderParams->encodeGUID != NV_ENC_CODEC_AV1_GUID)
    {
        NVENC_THROW_ERROR("Invalid codec guid", NV_ENC_ERR_INVALID_PARAM);
    }

    if (pEncoderParams->encodeGUID == NV_ENC_CODEC_H264_GUID)
    {
        if (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)
        {
            NVENC_THROW_ERROR("10-bit format isn't supported by H264 encoder", NV_ENC_ERR_INVALID_PARAM);
        }
    }

    if (pEncoderParams->encodeGUID == NV_ENC_CODEC_AV1_GUID)
    {
        if (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444 || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)
        {
            NVENC_THROW_ERROR("YUV444 format isn't supported by AV1 encoder", NV_ENC_ERR_INVALID_PARAM);
        }
    }

    // set other necessary params if not set yet
    if (pEncoderParams->encodeGUID == NV_ENC_CODEC_H264_GUID)
    {
        if ((m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444) &&
            (pEncoderParams->encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC != 3))
        {
            NVENC_THROW_ERROR("Invalid ChromaFormatIDC", NV_ENC_ERR_INVALID_PARAM);
        }
    }

    if (pEncoderParams->encodeGUID == NV_ENC_CODEC_HEVC_GUID)
    {
        bool yuv10BitFormat = (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT) ? true : false;
        if (yuv10BitFormat && pEncoderParams->encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 != 2)
        {
            NVENC_THROW_ERROR("Invalid PixelBitdepth", NV_ENC_ERR_INVALID_PARAM);
        }

        if ((m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444 || m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV444_10BIT) &&
            (pEncoderParams->encodeConfig->encodeCodecConfig.hevcConfig.chromaFormatIDC != 3))
        {
            NVENC_THROW_ERROR("Invalid ChromaFormatIDC", NV_ENC_ERR_INVALID_PARAM);
        }
    }

    if (pEncoderParams->encodeGUID == NV_ENC_CODEC_AV1_GUID)
    {
        bool yuv10BitFormat = (m_eBufferFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT) ? true : false;
        if (yuv10BitFormat && pEncoderParams->encodeConfig->encodeCodecConfig.av1Config.pixelBitDepthMinus8 != 2)
        {
            NVENC_THROW_ERROR("Invalid PixelBitdepth", NV_ENC_ERR_INVALID_PARAM);
        }

        if (pEncoderParams->encodeConfig->encodeCodecConfig.av1Config.chromaFormatIDC != 1)
        {
            NVENC_THROW_ERROR("Invalid ChromaFormatIDC", NV_ENC_ERR_INVALID_PARAM);
        }

        if (m_bOutputInVideoMemory && pEncoderParams->encodeConfig->frameIntervalP > 1)
        {
            NVENC_THROW_ERROR("Alt Ref frames not supported for AV1 in case of OutputInVideoMemory", NV_ENC_ERR_INVALID_PARAM);
        }
    }

    memcpy(&m_initializeParams, pEncoderParams, sizeof(m_initializeParams));
    m_initializeParams.version = NV_ENC_INITIALIZE_PARAMS_VER;

    if (pEncoderParams->encodeConfig)
    {
        memcpy(&m_encodeConfig, pEncoderParams->encodeConfig, sizeof(m_encodeConfig));
        m_encodeConfig.version = NV_ENC_CONFIG_VER;
    }
    else
    {
        NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
        if (!m_bMotionEstimationOnly)
        {
            m_nvenc.nvEncGetEncodePresetConfigEx(m_hEncoder, pEncoderParams->encodeGUID, pEncoderParams->presetGUID, pEncoderParams->tuningInfo, &presetConfig);
            memcpy(&m_encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
            if (m_bOutputInVideoMemory && pEncoderParams->encodeGUID == NV_ENC_CODEC_AV1_GUID)
            {
                m_encodeConfig.frameIntervalP = 1;
            }
        }
        else
        {
            m_encodeConfig.version = NV_ENC_CONFIG_VER;
            m_encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
            m_encodeConfig.rcParams.constQP = { 28, 31, 25 };
        }
    }

    if (((uint32_t)m_encodeConfig.frameIntervalP) > m_encodeConfig.gopLength)
    {
        m_encodeConfig.frameIntervalP = m_encodeConfig.gopLength;
    }

    m_initializeParams.encodeConfig = &m_encodeConfig;

    NVENC_API_CALL(m_nvenc.nvEncInitializeEncoder(m_hEncoder, &m_initializeParams));

    m_bEncoderInitialized = true;
    m_nWidth = m_initializeParams.encodeWidth;
    m_nHeight = m_initializeParams.encodeHeight;
    m_nMaxEncodeWidth = m_initializeParams.maxEncodeWidth;
    m_nMaxEncodeHeight = m_initializeParams.maxEncodeHeight;

    m_nEncoderBuffer = m_encodeConfig.frameIntervalP + m_encodeConfig.rcParams.lookaheadDepth + m_nExtraOutputDelay;
    m_nOutputDelay = m_nEncoderBuffer - 1;

    if (!m_bOutputInVideoMemory)
    {
        m_vpCompletionEvent.resize(m_nEncoderBuffer, nullptr);
    }

#if defined(_WIN32)
    for (uint32_t i = 0; i < m_vpCompletionEvent.size(); i++) 
    {
        m_vpCompletionEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!m_bIsDX12Encode)
        {
            NV_ENC_EVENT_PARAMS eventParams = { NV_ENC_EVENT_PARAMS_VER };
            eventParams.completionEvent = m_vpCompletionEvent[i];
            m_nvenc.nvEncRegisterAsyncEvent(m_hEncoder, &eventParams);
        }
    }
#endif

    m_vMappedInputBuffers.resize(m_nEncoderBuffer, nullptr);

    if (m_bMotionEstimationOnly)
    {
        m_vMappedRefBuffers.resize(m_nEncoderBuffer, nullptr);

        if (!m_bOutputInVideoMemory)
        {
            InitializeMVOutputBuffer();
        }
    }
    else
    {
        if (!m_bOutputInVideoMemory && !m_bIsDX12Encode)
        {
            m_vBitstreamOutputBuffer.resize(m_nEncoderBuffer, nullptr);
            InitializeBitstreamBuffer();
        }
    }

    AllocateInputBuffers(m_nEncoderBuffer);
}

void NvEncoder::DestroyEncoder()
{
    if (!m_hEncoder)
    {
        return;
    }

    ReleaseInputBuffers();

    DestroyHWEncoder();
}

void NvEncoder::DestroyHWEncoder()
{
    if (!m_hEncoder)
    {
        return;
    }

#if defined(_WIN32)
    for (uint32_t i = 0; i < m_vpCompletionEvent.size(); i++)
    {
        if (m_vpCompletionEvent[i])
        {
            if (!m_bIsDX12Encode)
            {
                NV_ENC_EVENT_PARAMS eventParams = { NV_ENC_EVENT_PARAMS_VER };
                eventParams.completionEvent = m_vpCompletionEvent[i];
                m_nvenc.nvEncUnregisterAsyncEvent(m_hEncoder, &eventParams);
            }
            CloseHandle(m_vpCompletionEvent[i]);
        }
    }
    m_vpCompletionEvent.clear();
#endif

    if (m_bMotionEstimationOnly)
    {
        DestroyMVOutputBuffer();
    }
    else
    {
        if (!m_bIsDX12Encode)
            DestroyBitstreamBuffer();
    }

    m_nvenc.nvEncDestroyEncoder(m_hEncoder);

    m_hEncoder = nullptr;

    m_bEncoderInitialized = false;
}

const NvEncInputFrame* NvEncoder::GetNextInputFrame()
{
    int i = m_iToSend % m_nEncoderBuffer;
    return &m_vInputFrames[i];
}

const NvEncInputFrame* NvEncoder::GetNextReferenceFrame()
{
    int i = m_iToSend % m_nEncoderBuffer;
    return &m_vReferenceFrames[i];
}

void NvEncoder::MapResources(uint32_t bfrIdx)
{
    NV_ENC_MAP_INPUT_RESOURCE mapInputResource = { NV_ENC_MAP_INPUT_RESOURCE_VER };

    mapInputResource.registeredResource = m_vRegisteredResources[bfrIdx];
    NVENC_API_CALL(m_nvenc.nvEncMapInputResource(m_hEncoder, &mapInputResource));
    m_vMappedInputBuffers[bfrIdx] = mapInputResource.mappedResource;

    if (m_bMotionEstimationOnly)
    {
        mapInputResource.registeredResource = m_vRegisteredResourcesForReference[bfrIdx];
        NVENC_API_CALL(m_nvenc.nvEncMapInputResource(m_hEncoder, &mapInputResource));
        m_vMappedRefBuffers[bfrIdx] = mapInputResource.mappedResource;
    }
}

void NvEncoder::EncodeFrame(std::vector<std::vector<uint8_t>> &vPacket, NV_ENC_PIC_PARAMS *pPicParams)
{
    vPacket.clear();
    if (!IsHWEncoderInitialized())
    {
        NVENC_THROW_ERROR("Encoder device not found", NV_ENC_ERR_NO_ENCODE_DEVICE);
    }

    int bfrIdx = m_iToSend % m_nEncoderBuffer;

    MapResources(bfrIdx);

    NVENCSTATUS nvStatus = DoEncode(m_vMappedInputBuffers[bfrIdx], m_vBitstreamOutputBuffer[bfrIdx], pPicParams);

    if (nvStatus == NV_ENC_SUCCESS || nvStatus == NV_ENC_ERR_NEED_MORE_INPUT)
    {
        m_iToSend++;
        GetEncodedPacket(m_vBitstreamOutputBuffer, vPacket, true);
    }
    else
    {
        NVENC_THROW_ERROR("nvEncEncodePicture API failed", nvStatus);
    }
}

void NvEncoder::RunMotionEstimation(std::vector<uint8_t> &mvData)
{
    if (!m_hEncoder)
    {
        NVENC_THROW_ERROR("Encoder Initialization failed", NV_ENC_ERR_NO_ENCODE_DEVICE);
        return;
    }

    const uint32_t bfrIdx = m_iToSend % m_nEncoderBuffer;

    MapResources(bfrIdx);

    NVENCSTATUS nvStatus = DoMotionEstimation(m_vMappedInputBuffers[bfrIdx], m_vMappedRefBuffers[bfrIdx], m_vMVDataOutputBuffer[bfrIdx]);

    if (nvStatus == NV_ENC_SUCCESS)
    {
        m_iToSend++;
        std::vector<std::vector<uint8_t>> vPacket;
        GetEncodedPacket(m_vMVDataOutputBuffer, vPacket, true);
        if (vPacket.size() != 1)
        {
            NVENC_THROW_ERROR("GetEncodedPacket() doesn't return one (and only one) MVData", NV_ENC_ERR_GENERIC);
        }
        mvData = vPacket[0];
    }
    else
    {
        NVENC_THROW_ERROR("nvEncEncodePicture API failed", nvStatus);
    }
}


void NvEncoder::GetSequenceParams(std::vector<uint8_t> &seqParams)
{
    uint8_t spsppsData[1024]; // Assume maximum spspps data is 1KB or less
    memset(spsppsData, 0, sizeof(spsppsData));
    NV_ENC_SEQUENCE_PARAM_PAYLOAD payload = { NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER };
    uint32_t spsppsSize = 0;

    payload.spsppsBuffer = spsppsData;
    payload.inBufferSize = sizeof(spsppsData);
    payload.outSPSPPSPayloadSize = &spsppsSize;
    NVENC_API_CALL(m_nvenc.nvEncGetSequenceParams(m_hEncoder, &payload));
    seqParams.clear();
    seqParams.insert(seqParams.end(), &spsppsData[0], &spsppsData[spsppsSize]);
}

NVENCSTATUS NvEncoder::DoEncode(NV_ENC_INPUT_PTR inputBuffer, NV_ENC_OUTPUT_PTR outputBuffer, NV_ENC_PIC_PARAMS *pPicParams)
{
    NV_ENC_PIC_PARAMS picParams = {};
    if (pPicParams)
    {
        picParams = *pPicParams;
    }
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    picParams.inputBuffer = inputBuffer;
    picParams.bufferFmt = GetPixelFormat();
    picParams.inputWidth = GetEncodeWidth();
    picParams.inputHeight = GetEncodeHeight();
    picParams.outputBitstream = outputBuffer;
    picParams.completionEvent = GetCompletionEvent(m_iToSend % m_nEncoderBuffer);
    NVENCSTATUS nvStatus = m_nvenc.nvEncEncodePicture(m_hEncoder, &picParams);

    return nvStatus; 
}

void NvEncoder::SendEOS()
{
    NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
    picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    picParams.completionEvent = GetCompletionEvent(m_iToSend % m_nEncoderBuffer);
    NVENC_API_CALL(m_nvenc.nvEncEncodePicture(m_hEncoder, &picParams));
}

void NvEncoder::EndEncode(std::vector<std::vector<uint8_t>> &vPacket)
{
    vPacket.clear();
    if (!IsHWEncoderInitialized())
    {
        NVENC_THROW_ERROR("Encoder device not initialized", NV_ENC_ERR_ENCODER_NOT_INITIALIZED);
    }

    SendEOS();

    GetEncodedPacket(m_vBitstreamOutputBuffer, vPacket, false);
}

void NvEncoder::GetEncodedPacket(std::vector<NV_ENC_OUTPUT_PTR> &vOutputBuffer, std::vector<std::vector<uint8_t>> &vPacket, bool bOutputDelay)
{
    unsigned i = 0;
    int iEnd = bOutputDelay ? m_iToSend - m_nOutputDelay : m_iToSend;
    for (; m_iGot < iEnd; m_iGot++)
    {
        WaitForCompletionEvent(m_iGot % m_nEncoderBuffer);
        NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };
        lockBitstreamData.outputBitstream = vOutputBuffer[m_iGot % m_nEncoderBuffer];
        lockBitstreamData.doNotWait = false;
        NVENC_API_CALL(m_nvenc.nvEncLockBitstream(m_hEncoder, &lockBitstreamData));
  
        uint8_t *pData = (uint8_t *)lockBitstreamData.bitstreamBufferPtr;
        if (vPacket.size() < i + 1)
        {
            vPacket.push_back(std::vector<uint8_t>());
        }
        vPacket[i].clear();
       
        if ((m_initializeParams.encodeGUID == NV_ENC_CODEC_AV1_GUID) && (m_bUseIVFContainer))
        {
            if (m_bWriteIVFFileHeader)
            {
                m_IVFUtils.WriteFileHeader(vPacket[i], MAKE_FOURCC('A', 'V', '0', '1'), m_initializeParams.encodeWidth, m_initializeParams.encodeHeight, m_initializeParams.frameRateNum, m_initializeParams.frameRateDen, 0xFFFF);
                m_bWriteIVFFileHeader = false;
            }

            m_IVFUtils.WriteFrameHeader(vPacket[i], lockBitstreamData.bitstreamSizeInBytes, lockBitstreamData.outputTimeStamp);
        }
        vPacket[i].insert(vPacket[i].end(), &pData[0], &pData[lockBitstreamData.bitstreamSizeInBytes]);
        
        i++;

        NVENC_API_CALL(m_nvenc.nvEncUnlockBitstream(m_hEncoder, lockBitstreamData.outputBitstream));

        if (m_vMappedInputBuffers[m_iGot % m_nEncoderBuffer])
        {
            NVENC_API_CALL(m_nvenc.nvEncUnmapInputResource(m_hEncoder, m_vMappedInputBuffers[m_iGot % m_nEncoderBuffer]));
            m_vMappedInputBuffers[m_iGot % m_nEncoderBuffer] = nullptr;
        }

        if (m_bMotionEstimationOnly && m_vMappedRefBuffers[m_iGot % m_nEncoderBuffer])
        {
            NVENC_API_CALL(m_nvenc.nvEncUnmapInputResource(m_hEncoder, m_vMappedRefBuffers[m_iGot % m_nEncoderBuffer]));
            m_vMappedRefBuffers[m_iGot % m_nEncoderBuffer] = nullptr;
        }
    }
}

bool NvEncoder::Reconfigure(const NV_ENC_RECONFIGURE_PARAMS *pReconfigureParams)
{
    NVENC_API_CALL(m_nvenc.nvEncReconfigureEncoder(m_hEncoder, const_cast<NV_ENC_RECONFIGURE_PARAMS*>(pReconfigureParams)));

    memcpy(&m_initializeParams, &(pReconfigureParams->reInitEncodeParams), sizeof(m_initializeParams));
    if (pReconfigureParams->reInitEncodeParams.encodeConfig)
    {
        memcpy(&m_encodeConfig, pReconfigureParams->reInitEncodeParams.encodeConfig, sizeof(m_encodeConfig));
    }

    m_nWidth = m_initializeParams.encodeWidth;
    m_nHeight = m_initializeParams.encodeHeight;
    m_nMaxEncodeWidth = m_initializeParams.maxEncodeWidth;
    m_nMaxEncodeHeight = m_initializeParams.maxEncodeHeight;

    return true;
}

NV_ENC_REGISTERED_PTR NvEncoder::RegisterResource(void *pBuffer, NV_ENC_INPUT_RESOURCE_TYPE eResourceType,
    int width, int height, int pitch, NV_ENC_BUFFER_FORMAT bufferFormat, NV_ENC_BUFFER_USAGE bufferUsage, 
    NV_ENC_FENCE_POINT_D3D12* pInputFencePoint)
{
    NV_ENC_REGISTER_RESOURCE registerResource = { NV_ENC_REGISTER_RESOURCE_VER };
    registerResource.resourceType = eResourceType;
    registerResource.resourceToRegister = pBuffer;
    registerResource.width = width;
    registerResource.height = height;
    registerResource.pitch = pitch;
    registerResource.bufferFormat = bufferFormat;
    registerResource.bufferUsage = bufferUsage;
    registerResource.pInputFencePoint = pInputFencePoint;
    NVENC_API_CALL(m_nvenc.nvEncRegisterResource(m_hEncoder, &registerResource));

    return registerResource.registeredResource;
}

void NvEncoder::RegisterInputResources(std::vector<void*> inputframes, NV_ENC_INPUT_RESOURCE_TYPE eResourceType,
                                         int width, int height, int pitch, NV_ENC_BUFFER_FORMAT bufferFormat, bool bReferenceFrame)
{
    for (uint32_t i = 0; i < inputframes.size(); ++i)
    {
        NV_ENC_REGISTERED_PTR registeredPtr = RegisterResource(inputframes[i], eResourceType, width, height, pitch, bufferFormat, NV_ENC_INPUT_IMAGE);
        
        std::vector<uint32_t> _chromaOffsets;
        NvEncoder::GetChromaSubPlaneOffsets(bufferFormat, pitch, height, _chromaOffsets);
        NvEncInputFrame inputframe = {};
        inputframe.inputPtr = (void *)inputframes[i];
        inputframe.chromaOffsets[0] = 0;
        inputframe.chromaOffsets[1] = 0;
        for (uint32_t ch = 0; ch < _chromaOffsets.size(); ch++)
        {
            inputframe.chromaOffsets[ch] = _chromaOffsets[ch];
        }
        inputframe.numChromaPlanes = NvEncoder::GetNumChromaPlanes(bufferFormat);
        inputframe.pitch = pitch;
        inputframe.chromaPitch = NvEncoder::GetChromaPitch(bufferFormat, pitch);
        inputframe.bufferFormat = bufferFormat;
        inputframe.resourceType = eResourceType;

        if (bReferenceFrame)
        {
            m_vRegisteredResourcesForReference.push_back(registeredPtr);
            m_vReferenceFrames.push_back(inputframe);
        }
        else
        {
            m_vRegisteredResources.push_back(registeredPtr);
            m_vInputFrames.push_back(inputframe);
        }
    }
}

void NvEncoder::FlushEncoder()
{
    if (!m_bMotionEstimationOnly && !m_bOutputInVideoMemory)
    {
        // Incase of error it is possible for buffers still mapped to encoder.
        // flush the encoder queue and then unmapped it if any surface is still mapped
        try
        {
            std::vector<std::vector<uint8_t>> vPacket;
            EndEncode(vPacket);
        }
        catch (...)
        {

        }
    }
}

void NvEncoder::UnregisterInputResources()
{
    FlushEncoder();
    
    if (m_bMotionEstimationOnly)
    {
        for (uint32_t i = 0; i < m_vMappedRefBuffers.size(); ++i)
        {
            if (m_vMappedRefBuffers[i])
            {
                m_nvenc.nvEncUnmapInputResource(m_hEncoder, m_vMappedRefBuffers[i]);
            }
        }
    }
    m_vMappedRefBuffers.clear();

    for (uint32_t i = 0; i < m_vMappedInputBuffers.size(); ++i)
    {
        if (m_vMappedInputBuffers[i])
        {
            m_nvenc.nvEncUnmapInputResource(m_hEncoder, m_vMappedInputBuffers[i]);
        }
    }
    m_vMappedInputBuffers.clear();

    for (uint32_t i = 0; i < m_vRegisteredResources.size(); ++i)
    {
        if (m_vRegisteredResources[i])
        {
            m_nvenc.nvEncUnregisterResource(m_hEncoder, m_vRegisteredResources[i]);
        }
    }
    m_vRegisteredResources.clear();


    for (uint32_t i = 0; i < m_vRegisteredResourcesForReference.size(); ++i)
    {
        if (m_vRegisteredResourcesForReference[i])
        {
            m_nvenc.nvEncUnregisterResource(m_hEncoder, m_vRegisteredResourcesForReference[i]);
        }
    }
    m_vRegisteredResourcesForReference.clear();

}


void NvEncoder::WaitForCompletionEvent(int iEvent)
{
#if defined(_WIN32)
    // Check if we are in async mode. If not, don't wait for event;
    NV_ENC_CONFIG sEncodeConfig = { 0 };
    NV_ENC_INITIALIZE_PARAMS sInitializeParams = { 0 };
    sInitializeParams.encodeConfig = &sEncodeConfig;
    GetInitializeParams(&sInitializeParams);

    if (0U == sInitializeParams.enableEncodeAsync)
    {
        return;
    }
#ifdef DEBUG
    WaitForSingleObject(m_vpCompletionEvent[iEvent], INFINITE);
#else
    // wait for 20s which is infinite on terms of gpu time
    if (WaitForSingleObject(m_vpCompletionEvent[iEvent], 20000) == WAIT_FAILED)
    {
        NVENC_THROW_ERROR("Failed to encode frame", NV_ENC_ERR_GENERIC);
    }
#endif
#endif
}

uint32_t NvEncoder::GetWidthInBytes(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t width)
{
    switch (bufferFormat) {
    case NV_ENC_BUFFER_FORMAT_NV12:
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
    case NV_ENC_BUFFER_FORMAT_YUV444:
        return width;
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        return width * 2;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return width * 4;
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return 0;
    }
}

uint32_t NvEncoder::GetNumChromaPlanes(const NV_ENC_BUFFER_FORMAT bufferFormat)
{
    switch (bufferFormat) 
    {
    case NV_ENC_BUFFER_FORMAT_NV12:
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        return 1;
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
    case NV_ENC_BUFFER_FORMAT_YUV444:
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        return 2;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return 0;
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return -1;
    }
}

uint32_t NvEncoder::GetChromaPitch(const NV_ENC_BUFFER_FORMAT bufferFormat,const uint32_t lumaPitch)
{
    switch (bufferFormat)
    {
    case NV_ENC_BUFFER_FORMAT_NV12:
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
    case NV_ENC_BUFFER_FORMAT_YUV444:
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        return lumaPitch;
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
        return (lumaPitch + 1)/2;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return 0;
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return -1;
    }
}

void NvEncoder::GetChromaSubPlaneOffsets(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t pitch, const uint32_t height, std::vector<uint32_t>& chromaOffsets)
{
    chromaOffsets.clear();
    switch (bufferFormat)
    {
    case NV_ENC_BUFFER_FORMAT_NV12:
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        chromaOffsets.push_back(pitch * height);
        return;
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
        chromaOffsets.push_back(pitch * height);
        chromaOffsets.push_back(chromaOffsets[0] + (NvEncoder::GetChromaPitch(bufferFormat, pitch) * GetChromaHeight(bufferFormat, height)));
        return;
    case NV_ENC_BUFFER_FORMAT_YUV444:
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        chromaOffsets.push_back(pitch * height);
        chromaOffsets.push_back(chromaOffsets[0] + (pitch * height));
        return;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return;
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return;
    }
}

uint32_t NvEncoder::GetChromaHeight(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t lumaHeight)
{
    switch (bufferFormat)
    {
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
    case NV_ENC_BUFFER_FORMAT_NV12:
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        return (lumaHeight + 1)/2;
    case NV_ENC_BUFFER_FORMAT_YUV444:
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        return lumaHeight;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return 0;
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return 0;
    }
}

uint32_t NvEncoder::GetChromaWidthInBytes(const NV_ENC_BUFFER_FORMAT bufferFormat, const uint32_t lumaWidth)
{
    switch (bufferFormat)
    {
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
        return (lumaWidth + 1) / 2;
    case NV_ENC_BUFFER_FORMAT_NV12:
        return lumaWidth;
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        return 2 * lumaWidth;
    case NV_ENC_BUFFER_FORMAT_YUV444:
        return lumaWidth;
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        return 2 * lumaWidth;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return 0;
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return 0;
    }
}


int NvEncoder::GetCapabilityValue(GUID guidCodec, NV_ENC_CAPS capsToQuery)
{
    if (!m_hEncoder)
    {
        return 0;
    }
    NV_ENC_CAPS_PARAM capsParam = { NV_ENC_CAPS_PARAM_VER };
    capsParam.capsToQuery = capsToQuery;
    int v;
    m_nvenc.nvEncGetEncodeCaps(m_hEncoder, guidCodec, &capsParam, &v);
    return v;
}

int NvEncoder::GetFrameSize() const
{
    switch (GetPixelFormat())
    {
    case NV_ENC_BUFFER_FORMAT_YV12:
    case NV_ENC_BUFFER_FORMAT_IYUV:
    case NV_ENC_BUFFER_FORMAT_NV12:
        return GetEncodeWidth() * (GetEncodeHeight() + (GetEncodeHeight() + 1) / 2);
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT:
        return 2 * GetEncodeWidth() * (GetEncodeHeight() + (GetEncodeHeight() + 1) / 2);
    case NV_ENC_BUFFER_FORMAT_YUV444:
        return GetEncodeWidth() * GetEncodeHeight() * 3;
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT:
        return 2 * GetEncodeWidth() * GetEncodeHeight() * 3;
    case NV_ENC_BUFFER_FORMAT_ARGB:
    case NV_ENC_BUFFER_FORMAT_ARGB10:
    case NV_ENC_BUFFER_FORMAT_AYUV:
    case NV_ENC_BUFFER_FORMAT_ABGR:
    case NV_ENC_BUFFER_FORMAT_ABGR10:
        return 4 * GetEncodeWidth() * GetEncodeHeight();
    default:
        NVENC_THROW_ERROR("Invalid Buffer format", NV_ENC_ERR_INVALID_PARAM);
        return 0;
    }
}

void NvEncoder::GetInitializeParams(NV_ENC_INITIALIZE_PARAMS *pInitializeParams)
{
    if (!pInitializeParams || !pInitializeParams->encodeConfig)
    {
        NVENC_THROW_ERROR("Both pInitializeParams and pInitializeParams->encodeConfig can't be NULL", NV_ENC_ERR_INVALID_PTR);
    }
    NV_ENC_CONFIG *pEncodeConfig = pInitializeParams->encodeConfig;
    *pEncodeConfig = m_encodeConfig;
    *pInitializeParams = m_initializeParams;
    pInitializeParams->encodeConfig = pEncodeConfig;
}

void NvEncoder::InitializeBitstreamBuffer()
{
    for (int i = 0; i < m_nEncoderBuffer; i++)
    {
        NV_ENC_CREATE_BITSTREAM_BUFFER createBitstreamBuffer = { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
        NVENC_API_CALL(m_nvenc.nvEncCreateBitstreamBuffer(m_hEncoder, &createBitstreamBuffer));
        m_vBitstreamOutputBuffer[i] = createBitstreamBuffer.bitstreamBuffer;
    }
}

void NvEncoder::DestroyBitstreamBuffer()
{
    for (uint32_t i = 0; i < m_vBitstreamOutputBuffer.size(); i++)
    {
        if (m_vBitstreamOutputBuffer[i])
        {
            m_nvenc.nvEncDestroyBitstreamBuffer(m_hEncoder, m_vBitstreamOutputBuffer[i]);
        }
    }

    m_vBitstreamOutputBuffer.clear();
}

void NvEncoder::InitializeMVOutputBuffer()
{
    for (int i = 0; i < m_nEncoderBuffer; i++)
    {
        NV_ENC_CREATE_MV_BUFFER createMVBuffer = { NV_ENC_CREATE_MV_BUFFER_VER };
        NVENC_API_CALL(m_nvenc.nvEncCreateMVBuffer(m_hEncoder, &createMVBuffer));
        m_vMVDataOutputBuffer.push_back(createMVBuffer.mvBuffer);
    }
}

void NvEncoder::DestroyMVOutputBuffer()
{
    for (uint32_t i = 0; i < m_vMVDataOutputBuffer.size(); i++)
    {
        if (m_vMVDataOutputBuffer[i])
        {
            m_nvenc.nvEncDestroyMVBuffer(m_hEncoder, m_vMVDataOutputBuffer[i]);
        }
    }

    m_vMVDataOutputBuffer.clear();
}

NVENCSTATUS NvEncoder::DoMotionEstimation(NV_ENC_INPUT_PTR inputBuffer, NV_ENC_INPUT_PTR inputBufferForReference, NV_ENC_OUTPUT_PTR outputBuffer)
{
    NV_ENC_MEONLY_PARAMS meParams = { NV_ENC_MEONLY_PARAMS_VER };
    meParams.inputBuffer = inputBuffer;
    meParams.referenceFrame = inputBufferForReference;
    meParams.inputWidth = GetEncodeWidth();
    meParams.inputHeight = GetEncodeHeight();
    meParams.mvBuffer = outputBuffer;
    meParams.completionEvent = GetCompletionEvent(m_iToSend % m_nEncoderBuffer);
    NVENCSTATUS nvStatus = m_nvenc.nvEncRunMotionEstimationOnly(m_hEncoder, &meParams);
    
    return nvStatus;
}


/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2023 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2023 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>

//#include "NvEncoder/NvEncoder.h"

#include <unordered_map>

class NvEncoderGL : public NvEncoder
{
public:
    /**
    *  @brief The constructor for the NvEncoderGL class
    *  An OpenGL context must be current to the calling thread/process when
    *  creating an instance of this class.
    */
    NvEncoderGL(uint32_t nWidth, uint32_t nHeight, NV_ENC_BUFFER_FORMAT eBufferFormat,
        uint32_t nExtraOutputDelay = 3, bool bMotionEstimationOnly = false);

    virtual ~NvEncoderGL();
private:
    /**
    *  @brief This function is used to allocate input buffers for encoding.
    *  This function is an override of virtual function NvEncoder::AllocateInputBuffers().
    *  This function creates OpenGL textures which are used to hold input data.
    *  To obtain handle to input buffers, the application must call NvEncoder::GetNextInputFrame().
    *  An OpenGL context must be current to the thread/process when calling
    *  this method.
    */
    virtual void AllocateInputBuffers(int32_t numInputBuffers) override;

    /**
    *  @brief This function is used to release the input buffers allocated for encoding.
    *  This function is an override of virtual function NvEncoder::ReleaseInputBuffers().
    *  An OpenGL context must be current to the thread/process when calling
    *  this method.
    */
    virtual void ReleaseInputBuffers() override;
private:
    void ReleaseGLResources();
};




NvEncoderGL::NvEncoderGL(uint32_t nWidth, uint32_t nHeight, NV_ENC_BUFFER_FORMAT eBufferFormat,
    uint32_t nExtraOutputDelay, bool bMotionEstimationOnly) :
    NvEncoder(NV_ENC_DEVICE_TYPE_OPENGL, nullptr, nWidth, nHeight, eBufferFormat,
        nExtraOutputDelay, bMotionEstimationOnly)
{
    if (!m_hEncoder)
    {
        return;
    }
}

NvEncoderGL::~NvEncoderGL()
{
    ReleaseGLResources();
}

void NvEncoderGL::ReleaseInputBuffers()
{
    ReleaseGLResources();
}

void NvEncoderGL::AllocateInputBuffers(int32_t numInputBuffers)
{
    if (!IsHWEncoderInitialized())
    {
        NVENC_THROW_ERROR("Encoder device not initialized", NV_ENC_ERR_ENCODER_NOT_INITIALIZED);
    }
    int numCount = m_bMotionEstimationOnly ? 2 : 1;

    for (int count = 0; count < numCount; count++)
    {
        std::vector<void*> inputFrames;
        for (int i = 0; i < numInputBuffers; i++)
        {
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = new NV_ENC_INPUT_RESOURCE_OPENGL_TEX;
            uint32_t tex;

            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_RECTANGLE, tex);

            uint32_t chromaHeight = GetNumChromaPlanes(GetPixelFormat()) * GetChromaHeight(GetPixelFormat(), GetMaxEncodeHeight());
            if (GetPixelFormat() == NV_ENC_BUFFER_FORMAT_YV12 || GetPixelFormat() == NV_ENC_BUFFER_FORMAT_IYUV)
                chromaHeight = GetChromaHeight(GetPixelFormat(), GetMaxEncodeHeight());

            glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8,
                GetWidthInBytes(GetPixelFormat(), GetMaxEncodeWidth()),
                GetMaxEncodeHeight() + chromaHeight,
                0, GL_RED, GL_UNSIGNED_BYTE, NULL);

            glBindTexture(GL_TEXTURE_RECTANGLE, 0);

            pResource->texture = tex;
            pResource->target = GL_TEXTURE_RECTANGLE;
            inputFrames.push_back(pResource);
        }
        RegisterInputResources(inputFrames, NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX,
            GetMaxEncodeWidth(),
            GetMaxEncodeHeight(),
            GetWidthInBytes(GetPixelFormat(), GetMaxEncodeWidth()),
            GetPixelFormat(), count == 1 ? true : false);
    }
}

void NvEncoderGL::ReleaseGLResources()
{
    if (!m_hEncoder)
    {
        return;
    }

    UnregisterInputResources();

    for (int i = 0; i < m_vInputFrames.size(); ++i)
    {
        if (m_vInputFrames[i].inputPtr)
        {
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = (NV_ENC_INPUT_RESOURCE_OPENGL_TEX *)m_vInputFrames[i].inputPtr;
            if (pResource)
            {
                glDeleteTextures(1, &pResource->texture);
                delete pResource;
            }
        }
    }
    m_vInputFrames.clear();

    for (int i = 0; i < m_vReferenceFrames.size(); ++i)
    {
        if (m_vReferenceFrames[i].inputPtr)
        {
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = (NV_ENC_INPUT_RESOURCE_OPENGL_TEX *)m_vReferenceFrames[i].inputPtr;
            if (pResource)
            {
                glDeleteTextures(1, &pResource->texture);
                delete pResource;
            }
        }
    }
    m_vReferenceFrames.clear();
}









/*
* Copyright 2017-2023 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/

/**
*  This sample application illustrates encoding of frames stored in OpenGL
*  textures. The application reads frames from the input file and uploads them
*  to the textures obtained from the encoder using NvEncoder::GetNextInputFrame().
*  The encoder subsequently maps the textures for encoder using NvEncodeAPI and
*  submits them to NVENC hardware for encoding as part of NvEncoder::EncodeFrame().
*
*  The X server must be running and the DISPLAY environment variable must be
*  set when attempting to run this application.
*/

#include <iostream>
#include <memory>
#include <stdint.h>

/**
* @brief Set up graphics resources targeting the specified context type
* The input argument to this function must be either "glx" or "egl".
*/
void GraphicsSetupWindow(const char *);

/**
* @brief Tear down graphics resources targeting the specified context type
* The input argument to this function must be the same as that used for the
* GraphicsSetupWindow() call.
*/
void GraphicsCloseWindow(const char *);


void showErrorAndExit (const char* message)
{
    bool bThrowError = false;
    std::ostringstream oss;
    if (message)
    {
        bThrowError = true;
        oss << message << std::endl;
    }

    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        exit(0);
    }
}

// egl resources
static Display *display;
static int screen;
static Window win = 0;

static EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static EGLSurface eglSurface = EGL_NO_SURFACE;
static EGLContext eglContext = EGL_NO_CONTEXT;

// glut window handle
static int window;

// Initialization function to create a simple X window and associate EGLContext and EGLSurface with it
bool SetupEGLResources(int xpos, int ypos, int width, int height, const char *windowname)
{
    bool status = true;

    EGLint configAttrs[] =
    {
        EGL_RED_SIZE,        1,
        EGL_GREEN_SIZE,      1,
        EGL_BLUE_SIZE,       1,
        EGL_DEPTH_SIZE,      16,
        EGL_SAMPLE_BUFFERS,  0,
        EGL_SAMPLES,         0,
        EGL_CONFORMANT,      EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint contextAttrs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLint windowAttrs[] = {EGL_NONE};
    EGLConfig* configList = NULL;
    EGLint configCount;

    display = XOpenDisplay(NULL);

    if (!display)
    {
        std::cout << "\nError opening X display\n";
        return false;
    }

    screen = DefaultScreen(display);

    eglDisplay = eglGetDisplay(display);

    if (eglDisplay == EGL_NO_DISPLAY)
    {
        std::cout << "\nEGL : failed to obtain display\n";
        return false;
    }

    if (!eglInitialize(eglDisplay, 0, 0))
    {
        std::cout << "\nEGL : failed to initialize\n";
        return false;
    }

    if (!eglChooseConfig(eglDisplay, configAttrs, NULL, 0, &configCount) || !configCount)
    {
        std::cout << "\nEGL : failed to return any matching configurations\n";
        return false;
    }

    configList = (EGLConfig*)malloc(configCount * sizeof(EGLConfig));

    if (!eglChooseConfig(eglDisplay, configAttrs, configList, configCount, &configCount) || !configCount)
    {
        std::cout << "\nEGL : failed to populate configuration list\n";
        status = false;
        goto end;
    }

    win = XCreateSimpleWindow(display, RootWindow(display, screen),
                              xpos, ypos, width, height, 0,
                              BlackPixel(display, screen),
                              WhitePixel(display, screen));

    eglSurface = eglCreateWindowSurface(eglDisplay, configList[0],
                                        (EGLNativeWindowType) win, windowAttrs);

    if (!eglSurface)
    {
        std::cout << "\nEGL : couldn't create window surface\n";
        status = false;
        goto end;
    }

    eglBindAPI(EGL_OPENGL_API);

    eglContext = eglCreateContext(eglDisplay, configList[0], NULL, contextAttrs);

    if (!eglContext)
    {
        std::cout << "\nEGL : couldn't create context\n";
        status = false;
        goto end;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
    {
        std::cout << "\nEGL : couldn't make context/surface current\n";
        status = false;
        goto end;
    }

end:
    free(configList);
    return status;
}

// Cleanup function to destroy the window and context
void DestroyEGLResources()
{
    if (eglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (eglContext != EGL_NO_CONTEXT)
        {
            eglDestroyContext(eglDisplay, eglContext);
        }

        if (eglSurface != EGL_NO_SURFACE)
        {
            eglDestroySurface(eglDisplay, eglSurface);
        }

        eglTerminate(eglDisplay);
    }

    if (win)
    {
        XDestroyWindow(display, win);
    }

    XCloseDisplay(display);
}

bool SetupGLXResources()
{
    int argc = 1;
    char *argv[1] = {(char*)"dummy"};

    // Use glx context/surface
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutInitWindowSize(16, 16);

    window = glutCreateWindow("AppEncGL");
    if (!window)
    {
        std::cout << "\nUnable to create GLUT window.\n" << std::endl;
        return false;
    }

    glutHideWindow();
    return true;
}

// Cleanup function to destroy glut resources
void DestroyGLXResources()
{
    glutDestroyWindow(window);
}

void GraphicsCloseWindow(const char* contextType)
{
    if (!strcmp(contextType, "egl"))
    {
        DestroyEGLResources();
    }
    else if (!strcmp(contextType, "glx"))
    {
        DestroyGLXResources();
    }
    else
    {
        std::cout << "\nInvalid context type specified.\n";
    }
}

void GraphicsSetupWindow(const char* contextType)
{
    if (!strcmp(contextType, "egl"))
    {
        // Use egl context/surface
        if (!SetupEGLResources(0, 0, 16, 16, "AppEncGL"))
        {
            showErrorAndExit("\nFailed to setup window.\n");
        }
    }
    else if (!strcmp(contextType, "glx"))
    {
        // Use glx context/surface
        if (!SetupGLXResources())
        {
            showErrorAndExit("\nFailed to setup window.\n");
        }
    }
    else
    {
        showErrorAndExit("\nInvalid context type specified.\n");
    }

    char * vendor = (char*) glGetString(GL_VENDOR);
    if (strcmp(vendor, "NVIDIA Corporation"))
    {
        GraphicsCloseWindow(contextType);
        showErrorAndExit("\nFailed to find NVIDIA libraries\n");
    }
}


simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void ShowHelpAndExit(const char *szBadOption = NULL)
{
    bool bThrowError = false;
    std::ostringstream oss;
    if (szBadOption)
    {
        bThrowError = true;
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
    }
    oss << "Options:" << std::endl
        << "-i           Input file path" << std::endl
        << "-o           Output file path" << std::endl
        << "-s           Input resolution in this form: WxH" << std::endl
        << "-if          Input format: iyuv nv12" << std::endl
        << "-c           Context type: glx egl" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage(false, false, true);
    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        exit(0);
    }
}

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, int &nWidth, int &nHeight,
    NV_ENC_BUFFER_FORMAT &eFormat, char *szOutputFileName, NvEncoderInitParam &initParam, char* contextType)
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (!_stricmp(argv[i], "-h"))
        {
            ShowHelpAndExit();
        }
        if (!_stricmp(argv[i], "-i"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-i");
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-o");
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-s"))
        {
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &nWidth, &nHeight))
            {
                ShowHelpAndExit("-s");
            }
            continue;
        }
        if (!_stricmp(argv[i], "-c"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-c");
            }
            snprintf(contextType, 4, "%s", argv[i]);
            continue;
        }

        std::vector<std::string> vszFileFormatName = { "iyuv", "nv12" };

        NV_ENC_BUFFER_FORMAT aFormat[] =
        {
            NV_ENC_BUFFER_FORMAT_IYUV,
            NV_ENC_BUFFER_FORMAT_NV12,
        };

        if (!_stricmp(argv[i], "-if"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-if");
            }
            auto it = std::find(vszFileFormatName.begin(), vszFileFormatName.end(), argv[i]);
            if (it == vszFileFormatName.end())
            {
                ShowHelpAndExit("-if");
            }
            eFormat = aFormat[it - vszFileFormatName.begin()];
            continue;
        }
        // Regard as encoder parameter
        if (argv[i][0] != '-')
        {
            ShowHelpAndExit(argv[i]);
        }
        oss << argv[i] << " ";
        while (i + 1 < argc && argv[i + 1][0] != '-')
        {
            oss << argv[++i] << " ";
        }
    }
    initParam = NvEncoderInitParam(oss.str().c_str());
}

void EncodeGL(char *szInFilePath, char *szOutFilePath, int nWidth, int nHeight,
    NV_ENC_BUFFER_FORMAT eFormat, NvEncoderInitParam *encodeCLIOptions)
{
    std::ifstream fpIn(szInFilePath, std::ifstream::in | std::ifstream::binary);
    if (!fpIn)
    {
        std::ostringstream err;
        err << "Unable to open input file: " << szInFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << szOutFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    NvEncoderGL enc(nWidth, nHeight, eFormat);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc.CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions->GetEncodeGUID(),
        encodeCLIOptions->GetPresetGUID(),encodeCLIOptions->GetTuningInfo());

    encodeCLIOptions->SetInitParams(&initializeParams, eFormat);

    enc.CreateEncoder(&initializeParams);

    int nFrameSize = enc.GetFrameSize();
    std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nFrameSize]);
    int nFrame = 0;
    while (true)
    {
        std::streamsize nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nFrameSize).gcount();

        const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();
        NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = (NV_ENC_INPUT_RESOURCE_OPENGL_TEX *)encoderInputFrame->inputPtr;

        glBindTexture(pResource->target, pResource->texture);
        glTexSubImage2D(pResource->target, 0, 0, 0,
            nWidth, nHeight * 3 / 2,
            GL_RED, GL_UNSIGNED_BYTE, pHostFrame.get());
        glBindTexture(pResource->target, 0);

        std::vector<std::vector<uint8_t>> vPacket;
        if (nRead == nFrameSize)
        {
            enc.EncodeFrame(vPacket);
        }
        else
        {
            enc.EndEncode(vPacket);
        }
        nFrame += (int)vPacket.size();
        for (std::vector<uint8_t> &packet : vPacket)
        {
            fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
        }
        if (nRead != nFrameSize) break;
    }

    enc.DestroyEncoder();

    fpOut.close();
    fpIn.close();

    std::cout << "Total frames encoded: " << nFrame << std::endl;
    std::cout << "Saved in file " << szOutFilePath << std::endl;
}

/// get texture from window, read fn when its updated...

GLuint texture_from_window() {

}

int main_sample(int argc, char **argv)
{
    char szInFilePath[256] = "",
        szOutFilePath[256] = "",
        contextType[4] = "glx";

    int nWidth = 0, nHeight = 0;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    NvEncoderInitParam encodeCLIOptions;

    try
    {
        ParseCommandLine(argc, argv, szInFilePath, nWidth, nHeight, eFormat,
            szOutFilePath, encodeCLIOptions, contextType);

        GraphicsSetupWindow(contextType);

        CheckInputFile(szInFilePath);
        ValidateResolution(nWidth, nHeight);

        if (!*szOutFilePath)
        {
            sprintf(szOutFilePath, encodeCLIOptions.IsCodecH264() ? "out.h264" : "out.hevc");
        }

        EncodeGL(szInFilePath, szOutFilePath, nWidth, nHeight, eFormat, &encodeCLIOptions);
    }
    catch (const std::exception &e)
    {
        std::cout << e.what();
        return 1;
    }

    GraphicsCloseWindow(contextType);

    return 0;
}
