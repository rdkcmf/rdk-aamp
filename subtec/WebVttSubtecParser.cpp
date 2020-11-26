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
#include "PacketSender.hpp"


WebVTTSubtecParser::WebVTTSubtecParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(aamp, type), m_channel(nullptr)
{
	m_channel = make_unique<WebVttChannel>();
}

void WebVTTSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	PacketSender::Instance()->AddPacket(m_channel->generateTimestampPacket(positionMs));
	PacketSender::Instance()->SendPackets();
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
	PacketSender::Instance()->AddPacket(m_channel->generateResetAllPacket());
	PacketSender::Instance()->AddPacket(m_channel->generateSelectionPacket(width, height));
	PacketSender::Instance()->AddPacket(m_channel->generateTimestampPacket(static_cast<uint64_t>(startPos)));
	PacketSender::Instance()->SendPackets();
	
	mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
	
	return true;
}

bool WebVTTSubtecParser::processData(char* buffer, size_t bufferLen, double position, double duration)
{
	std::vector<uint8_t> data;
	
	for (size_t i = 0; i < bufferLen; i++)
		data.push_back(buffer[i]);
	
	PacketSender::Instance()->AddPacket(m_channel->generateDataPacket(data));
	PacketSender::Instance()->SendPackets();

	return true;
}
