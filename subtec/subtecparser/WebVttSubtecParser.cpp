/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include "WebVttSubtecParser.hpp"
#include "TextStyleAttributes.h"

WebVTTSubtecParser::WebVTTSubtecParser(AampLogManager *logObj, PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(logObj, aamp, type), m_channel(nullptr)
{
	m_channel = SubtecChannel::SubtecChannelFactory(SubtecChannel::ChannelType::WEBVTT);
	if (!m_channel->InitComms())
	{
		AAMPLOG_INFO("Init failed - subtitle parsing disabled");
		throw std::runtime_error("PacketSender init failed");
	}
	AAMPLOG_INFO("Sending RESET ALL");
	m_channel->SendResetAllPacket();
	int width = 1920, height = 1080;
	m_channel->SendMutePacket();
	mAamp->GetPlayerVideoSize(width, height);
	m_channel->SendSelectionPacket(width, height);
}

void WebVTTSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	AAMPLOG_INFO("positionMs %lld",  positionMs);
	m_channel->SendTimestampPacket(positionMs);
}

void WebVTTSubtecParser::reset()
{
	m_channel->SendMutePacket();
	m_channel->SendResetChannelPacket();
}

bool WebVTTSubtecParser::init(double startPosSeconds, unsigned long long basePTS)
{
	AAMPLOG_INFO("startPos %f basePTS %lld",  startPosSeconds, basePTS);

	m_channel->SendTimestampPacket(static_cast<uint64_t>(startPosSeconds*1000.0));

	mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);

	return true;
}

bool WebVTTSubtecParser::processData(char* buffer, size_t bufferLen, double position, double duration)
{
	std::string str(const_cast<const char*>(buffer), bufferLen);
	std::vector<uint8_t> data(str.begin(), str.end());

	m_channel->SendDataPacket(std::move(data), 0);

	return true;
}

void WebVTTSubtecParser::mute(bool mute)
{
	if (mute)
		m_channel->SendMutePacket();
	else
		m_channel->SendUnmutePacket();
}

void WebVTTSubtecParser::pause(bool pause)
{
	if (pause)
		m_channel->SendPausePacket();
	else
		m_channel->SendResumePacket();
}


void WebVTTSubtecParser::setTextStyle(const std::string &options)
{
	TextStyleAttributes textStyleAttributes(mLogObj);
	uint32_t attributesMask = 0;
	attributesType attributesValues = {0};

	int ccType = 0;			/* Value not used by WebVTT */

	if (!textStyleAttributes.getAttributes(options, attributesValues, attributesMask))
	{
		if (attributesMask)
		{
			m_channel->SendCCSetAttributePacket(ccType, attributesMask, attributesValues);
		}
	}
}
