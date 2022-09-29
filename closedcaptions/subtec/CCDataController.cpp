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
 * @file CCDataController.cpp
 *
 * @brief Impl of subtec communication layer
 *
 */

#include <cstring>
#include <unordered_map>

#include <closedcaptions/subtec/CCDataController.h>

#include "priv_aamp.h"


namespace subtecConnector
{

namespace
{
    gsw_CcAttributes createDefaultAttributes()
    {
        gsw_CcAttributes attribs;
        attribs.charBgColor.rgb = GSW_CC_EMBEDDED_COLOR;
	attribs.charBgColor.name = "";
        attribs.charFgColor.rgb = GSW_CC_EMBEDDED_COLOR;
	attribs.charFgColor.name = "";
        attribs.winColor.rgb = GSW_CC_EMBEDDED_COLOR;
	attribs.winColor.name = "";
        attribs.charBgOpacity = GSW_CC_OPACITY_EMBEDDED;
        attribs.charFgOpacity = GSW_CC_OPACITY_EMBEDDED;
        attribs.winOpacity = GSW_CC_OPACITY_EMBEDDED;
        attribs.fontSize = GSW_CC_FONT_SIZE_EMBEDDED;

        std::memcpy(attribs.fontStyle , GSW_CC_FONT_STYLE_EMBEDDED, sizeof(GSW_CC_FONT_STYLE_EMBEDDED));

        attribs.fontItalic = GSW_CC_TEXT_STYLE_EMBEDDED_TEXT;
        attribs.fontUnderline = GSW_CC_TEXT_STYLE_EMBEDDED_TEXT;
        attribs.borderType = GSW_CC_BORDER_TYPE_EMBEDDED;
        attribs.borderColor.rgb = GSW_CC_EMBEDDED_COLOR;
	attribs.borderColor.name = "";
        attribs.edgeType = GSW_CC_EDGE_TYPE_EMBEDDED;
        attribs.edgeColor.rgb = GSW_CC_EMBEDDED_COLOR;
	attribs.edgeColor.name = "";
        return attribs;
    }
}


CCDataController* CCDataController::Instance()
{
    static CCDataController instance;
    return &instance;
}

void CCDataController::closedCaptionDataCb (int decoderIndex, VL_CC_DATA_TYPE eType, unsigned char* ccData,
                                unsigned dataLength, int sequenceNumber, long long localPts)
{
    channel.SendDataPacketWithPTS(localPts, ccData, dataLength);
}

void CCDataController::closedCaptionDecodeCb(int decoderIndex, int event)
{
    logprintf("closedCaptionDecodeCb decoderIndex = %d, event = %d", decoderIndex, event);
}

void CCDataController::sendMute()
{
    channel.SendMutePacket();
}

void CCDataController::sendUnmute()
{
    channel.SendUnmutePacket();
}

void CCDataController::sendPause()
{
    channel.SendPausePacket();
}

void CCDataController::sendResume()
{
    channel.SendResumePacket();
}

void CCDataController::sendResetChannelPacket()
{
    channel.SendResetChannelPacket();
}

CCDataController::CCDataController()
    : channel{}
    , currentAttributes{createDefaultAttributes()}
{
}

void CCDataController::ccSetDigitalChannel(unsigned int channelId)
{
    channel.SendActiveTypePacket(ClosedCaptionsActiveTypePacket::CEA::type_708, channelId);
}

void CCDataController::ccSetAnalogChannel(unsigned int channelId)
{
    channel.SendActiveTypePacket(ClosedCaptionsActiveTypePacket::CEA::type_608, channelId);
}

void CCDataController::ccGetAttributes(gsw_CcAttributes * attrib, gsw_CcType /*ccType*/)
{
    std::memcpy(attrib, &currentAttributes, sizeof(gsw_CcAttributes));
}

namespace
{
    uint32_t getValue(const gsw_CcColor& attrib)
    {
        return static_cast<uint32_t>(attrib.rgb);
    }
    uint32_t getValue(const gsw_CcOpacity& attrib)
    {
        return static_cast<uint32_t>(attrib);
    }
    uint32_t getValue(const gsw_CcFontSize& attrib)
    {
        return static_cast<uint32_t>(attrib);
    }
    uint32_t getValue(const gsw_CcFontStyle& attrib)
    {
        // values based on cea-708
        static const std::unordered_map<std::string, uint32_t> valuesMap{
            {GSW_CC_FONT_STYLE_DEFAULT, 0},
            {GSW_CC_FONT_STYLE_MONOSPACED_SERIF, 1},
            {GSW_CC_FONT_STYLE_PROPORTIONAL_SERIF, 2},
            {GSW_CC_FONT_STYLE_MONOSPACED_SANSSERIF, 3},
            {GSW_CC_FONT_STYLE_PROPORTIONAL_SANSSERIF, 4},
            {GSW_CC_FONT_STYLE_CASUAL, 5},
            {GSW_CC_FONT_STYLE_CURSIVE, 6},
            {GSW_CC_FONT_STYLE_SMALL_CAPITALS, 7},
        };

        const auto it = valuesMap.find(attrib);
        if(it != valuesMap.end())
            return it->second;
        else
        {
            logprintf("Cannot match %s to attribute value", attrib);
            return 0; // value for default
        }

    }
    uint32_t getValue(const gsw_CcTextStyle& attrib)
    {
        return static_cast<uint32_t>(attrib);
    }
    uint32_t getValue(const gsw_CcBorderType& attrib)
    {
        return static_cast<uint32_t>(attrib);
    }
    uint32_t getValue(const gsw_CcEdgeType& attrib)
    {
        return static_cast<uint32_t>(attrib);
    }
    uint32_t getValue(const gsw_CcType& type)
    {
        return static_cast<uint32_t>(type);
    }

    uint32_t getValue(gsw_CcAttributes * attrib, short type)
    {
        switch(type)
        {
            case GSW_CC_ATTRIB_BACKGROUND_COLOR:
                return getValue(attrib->charBgColor);
            case GSW_CC_ATTRIB_FONT_COLOR:
                return getValue(attrib->charFgColor);
            case GSW_CC_ATTRIB_WIN_COLOR:
                return getValue(attrib->winColor);
            case GSW_CC_ATTRIB_BACKGROUND_OPACITY:
                return getValue(attrib->charBgOpacity);
            case GSW_CC_ATTRIB_FONT_OPACITY:
                return getValue(attrib->charFgOpacity);
            case GSW_CC_ATTRIB_WIN_OPACITY:
                return getValue(attrib->winOpacity);
            case GSW_CC_ATTRIB_FONT_SIZE:
                return getValue(attrib->fontSize);
            case GSW_CC_ATTRIB_FONT_STYLE:
                return getValue(attrib->fontStyle);
            case GSW_CC_ATTRIB_FONT_ITALIC:
                return getValue(attrib->fontItalic);
            case GSW_CC_ATTRIB_FONT_UNDERLINE:
                return getValue(attrib->fontUnderline);
            case GSW_CC_ATTRIB_BORDER_TYPE:
                return getValue(attrib->borderType);
            case GSW_CC_ATTRIB_BORDER_COLOR:
                return getValue(attrib->borderColor);
            case GSW_CC_ATTRIB_EDGE_TYPE:
                return getValue(attrib->edgeType);
            case GSW_CC_ATTRIB_EDGE_COLOR:
                return getValue(attrib->edgeColor);
            default:
                AAMPLOG_WARN("wrong attribute type used 0x%x",type);
                return -1;

        }
    }

    void setValue(gsw_CcAttributes* currentAttributes, gsw_CcAttributes * attrib, short type)
    {
        switch(type)
        {
            case GSW_CC_ATTRIB_BACKGROUND_COLOR:
                currentAttributes->charBgColor = attrib->charBgColor;
                return;
            case GSW_CC_ATTRIB_FONT_COLOR:
                currentAttributes->charFgColor = attrib->charFgColor;
                return;
            case GSW_CC_ATTRIB_WIN_COLOR:
                currentAttributes->winColor = attrib->winColor;
                return;
            case GSW_CC_ATTRIB_BACKGROUND_OPACITY:
                currentAttributes->charBgOpacity = attrib->charBgOpacity;
                return;
            case GSW_CC_ATTRIB_FONT_OPACITY:
                currentAttributes->charFgOpacity = attrib->charFgOpacity;
                return;
            case GSW_CC_ATTRIB_WIN_OPACITY:
                currentAttributes->winOpacity = attrib->winOpacity;
                return;
            case GSW_CC_ATTRIB_FONT_SIZE:
                currentAttributes->fontSize = attrib->fontSize;
                return;
            case GSW_CC_ATTRIB_FONT_STYLE:
                std::memcpy(currentAttributes->fontStyle, attrib->fontStyle, GSW_CC_MAX_FONT_NAME_LENGTH);
                // currentAttributes->fontStyle = attrib->fontStyle;
                return;
            case GSW_CC_ATTRIB_FONT_ITALIC:
                currentAttributes->fontItalic = attrib->fontItalic;
                return;
            case GSW_CC_ATTRIB_FONT_UNDERLINE:
                currentAttributes->fontUnderline = attrib->fontUnderline;
                return;
            case GSW_CC_ATTRIB_BORDER_TYPE:
                currentAttributes->borderType = attrib->borderType;
                return;
            case GSW_CC_ATTRIB_BORDER_COLOR:
                currentAttributes->borderColor = attrib->borderColor;
                return;
            case GSW_CC_ATTRIB_EDGE_TYPE:
                currentAttributes->edgeType = attrib->edgeType;
                return;
            case GSW_CC_ATTRIB_EDGE_COLOR:
                currentAttributes->edgeColor = attrib->edgeColor;
                return;
            default:
                logprintf("%s: wrong attribute type used 0x%x",__func__,  type);
                return;

        }
    }
}

void CCDataController::sendCCSetAttribute(gsw_CcAttributes * attrib, short type, gsw_CcType ccType)
{
    using AttributesArray = std::array<uint32_t, 14> ;

    static constexpr AttributesArray masks = {
            GSW_CC_ATTRIB_FONT_COLOR,
            GSW_CC_ATTRIB_BACKGROUND_COLOR,
            GSW_CC_ATTRIB_FONT_OPACITY,
            GSW_CC_ATTRIB_BACKGROUND_OPACITY,
            GSW_CC_ATTRIB_FONT_STYLE,
            GSW_CC_ATTRIB_FONT_SIZE,
            GSW_CC_ATTRIB_FONT_ITALIC,
            GSW_CC_ATTRIB_FONT_UNDERLINE,
            GSW_CC_ATTRIB_BORDER_TYPE,
            GSW_CC_ATTRIB_BORDER_COLOR,
            GSW_CC_ATTRIB_WIN_COLOR,
            GSW_CC_ATTRIB_WIN_OPACITY,
            GSW_CC_ATTRIB_EDGE_TYPE,
            GSW_CC_ATTRIB_EDGE_COLOR,
    };

    AttributesArray attributes{};

    for(int i = 0; i<attributes.size(); ++i)
    {
        if(type & (masks[i]))
        {
            setValue(&currentAttributes, attrib, masks[i]);
            attributes[i] = getValue(attrib, masks[i]);
        }
        else
        {
            attributes[i] = 0;
        }
    }

    const auto ccTypeValue = getValue(ccType);

    channel.SendCCSetAttributePacket(ccTypeValue, uint32_t{type}, attributes);
}

void closedCaptionDecodeCb(void *context, int decoderIndex, int event)
{
    CCDataController* p = static_cast<CCDataController*>(context);
    p->closedCaptionDecodeCb(decoderIndex, event);
}

void closedCaptionDataCb (void *context, int decoderIndex, VL_CC_DATA_TYPE eType, unsigned char* ccData,
                                unsigned dataLength, int sequenceNumber, long long localPts)
{
    CCDataController* p = static_cast<CCDataController*>(context);
    p->closedCaptionDataCb(decoderIndex, eType, ccData, dataLength, sequenceNumber, localPts);
}


} // subtecConnector
