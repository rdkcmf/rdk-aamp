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
 * @file SubtecConnector.cpp
 *
 * @brief Impl of libsubtec_connector
 *
 */

#include <closedcaptions/subtec/SubtecConnector.h>
#include <closedcaptions/subtec/CCDataController.h>

#include "ccDataReader.h"


#include <iomanip>
#include <sstream>
#include <mutex>

#include <cstdio>
#include <cstdarg>

#include "AampLogManager.h"


namespace subtecConnector
{
    mrcc_Error initHal()
    {
        const auto registerResult = vlhal_cc_Register(0, CCDataController::Instance(), closedCaptionDataCb, closedCaptionDecodeCb);
        logprintf("vlhal_cc_Register return value = %d\n", registerResult);

        if(registerResult != 0)
            return CC_VL_OS_API_RESULT_FAILED;

        const auto startResult = media_closeCaptionStart(nullptr);
        logprintf("media_closeCaptionStart return value = %d\n", registerResult);

        if(startResult != 0)
            return CC_VL_OS_API_RESULT_FAILED;
        
        return CC_VL_OS_API_RESULT_SUCCESS;
    }

    mrcc_Error initPacketSender()
    {
        const auto packetSenderStartResult = CCDataController::Instance()->InitComms();
        logprintf("CCDataController::Instance()->InitComms() return value = %d\n", (int)packetSenderStartResult);

        if(!packetSenderStartResult)
            return CC_VL_OS_API_RESULT_FAILED;

        return CC_VL_OS_API_RESULT_SUCCESS;
    }
    void resetChannel()
    {
        CCDataController::Instance()->sendResetChannelPacket();
    }

    void close()
    {
        media_closeCaptionStop();
    }


namespace ccMgrAPI
{

    typedef int mrcc_Error;

    mrcc_Error ccShow(void)
    {
        CCDataController::Instance()->sendUnmute();
        return {};
    }

    mrcc_Error ccHide(void)
    {
        CCDataController::Instance()->sendMute();
        return {};
    }

    mrcc_Error ccSetAttributes(gsw_CcAttributes * attrib, short type, gsw_CcType ccType)
    {
        CCDataController::Instance()->sendCCSetAttribute(attrib, type, ccType);
        return {};
    }

    mrcc_Error ccSetDigitalChannel(unsigned int channel)
    {
        CCDataController::Instance()->ccSetDigitalChannel(channel);
        return {};
    }

    mrcc_Error ccSetAnalogChannel(unsigned int channel)
    {
        CCDataController::Instance()->ccSetAnalogChannel(channel);
        return {};
    }

    mrcc_Error ccGetAttributes(gsw_CcAttributes * attrib, gsw_CcType ccType)
    {
        CCDataController::Instance()->ccGetAttributes(attrib, ccType);
        return {};
    }

    mrcc_Error ccGetCapability(gsw_CcAttribType attribType, gsw_CcType ccType, void **values, unsigned int *size)
    {

        if(!values || !size)
        {
            return CC_VL_OS_API_RESULT_FAILED;
        }

        *size = 0; //default case
        int i = 0;


        switch(attribType)
        {
            case GSW_CC_ATTRIB_FONT_COLOR :
            case GSW_CC_ATTRIB_BACKGROUND_COLOR:
            case GSW_CC_ATTRIB_BORDER_COLOR:
            case GSW_CC_ATTRIB_WIN_COLOR:
            case GSW_CC_ATTRIB_EDGE_COLOR:
            {
                *size = sizeof(CCSupportedColors_strings)/sizeof(char *);
                const char** pValuesNames = CCSupportedColors_strings;

                *size = sizeof(CCSupportedColors)/sizeof( unsigned long);
                const unsigned long * pValues = CCSupportedColors;
                // Copy our colors into the given capability array

                for(i = 0; i < *size; i++)
                {
                    ((gsw_CcColor*)values[i])->rgb = pValues[i];
                    strncpy(((gsw_CcColor*)values[i])->name, pValuesNames[i],
                            GSW_MAX_CC_COLOR_NAME_LENGTH);
                }
                break;
            }
            default:
                logprintf("ccGetCapability invoked with not supported attribType = 0x%x\n", (int)attribType);
                return CC_VL_OS_API_RESULT_FAILED;
        }

        return CC_VL_OS_API_RESULT_SUCCESS;
    }


} // ccMgrAPI
} // subtecConnector
