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
#include "TtmlPacket.hpp"

WebVTTSubtecParser::WebVTTSubtecParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(aamp, type), m_channel(nullptr)
{
	m_channel = make_unique<WebVttChannel>();
	m_channel->SendResetAllPacket();
}

void WebVTTSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	m_channel->SendTimestampPacket(positionMs);
}

bool WebVTTSubtecParser::init(double startPos, unsigned long long basePTS)
{	
	if (!PacketSender::Instance()->Init())
	{
		AAMPLOG_INFO("%s: Init failed - subtitle parsing disabled\n", __FUNCTION__);
		return false;
	}
	
	int width = 1280, height = 720;
	
	mAamp->GetPlayerVideoSize(width, height);
	m_channel->SendSelectionPacket(width, height);
	m_channel->SendTimestampPacket(static_cast<uint64_t>(startPos));
	
	mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
	
	return true;
}

bool WebVTTSubtecParser::processData(char* buffer, size_t bufferLen, double position, double duration)
{
	std::string str(const_cast<const char*>(buffer), bufferLen);
	std::vector<uint8_t> data(str.begin(), str.end());
		
	m_channel->SendDataPacket(std::move(data));

	return true;
}
