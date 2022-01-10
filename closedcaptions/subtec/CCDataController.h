/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file CCDataController.h
 *
 * @brief Impl of subtec communication layer
 *
 */

#ifndef __CC_DATA_CONTROLLER_H__
#define __CC_DATA_CONTROLLER_H__

#include <ClosedCaptionsPacket.hpp>

#include "ccDataReader.h"

#include <closedcaptions/subtec/SubtecConnector.h>

/**
 * @brief Print logs to console / log file
 * @param[in] format - printf style string
 * @retuen void
 */
extern void logprintf(const char *format, ...);
extern void logprintf_new(int playerId,const char* levelstr,const char* file, int line,const char *format, ...);

namespace subtecConnector
{

/**
 * @brief Controller for CCdata
 */
class CCDataController
{
public:
    static CCDataController* Instance();

    void closedCaptionDataCb (int decoderIndex, VL_CC_DATA_TYPE eType, unsigned char* ccData,
                                    unsigned dataLength, int sequenceNumber, long long localPts);

    void closedCaptionDecodeCb(int decoderIndex, int event);

    void sendMute();
    void sendUnmute();

    void sendPause();
    void sendResume();
    void sendResetChannelPacket();
    void sendCCSetAttribute(gsw_CcAttributes * attrib, short type, gsw_CcType ccType);

    void ccSetDigitalChannel(unsigned int channel);
    void ccSetAnalogChannel(unsigned int channel);

    void ccGetAttributes(gsw_CcAttributes * attrib, gsw_CcType ccType);

private:
    CCDataController();
    CCDataController(const CCDataController&) = delete;
    CCDataController(CCDataController&&) = delete;
    ClosedCaptionsChannel channel;

    gsw_CcAttributes currentAttributes;
};

void closedCaptionDecodeCb(void *context, int decoderIndex, int event);

void closedCaptionDataCb (void *context, int decoderIndex, VL_CC_DATA_TYPE eType, unsigned char* ccData,
                                unsigned dataLength, int sequenceNumber, long long localPts);


} // subtecConnector

#endif //__CC_DATA_CONTROLLER_H__
