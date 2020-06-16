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

#include "TtmlSubtecParser.hpp"
#include "PacketSender.hpp"


TtmlSubtecParser::TtmlSubtecParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(aamp, type)
{
	m_channel = make_unique<TtmlChannel>();
}

bool TtmlSubtecParser::init(double startPos, unsigned long long basePTS)
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

void TtmlSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	PacketSender::Instance()->AddPacket(m_channel->generateTimestampPacket(positionMs));
	PacketSender::Instance()->SendPackets();
}

bool TtmlSubtecParser::processData(char* buffer, size_t bufferLen, double position, double duration)
{	
	IsoBmffBuffer isobuf;
	
	isobuf.setBuffer(reinterpret_cast<uint8_t *>(buffer), bufferLen);
	isobuf.parseBuffer();
	
	if (!isobuf.isInitSegment())
	{
		uint8_t *mdat;
		size_t mdatLen;
		std::vector<uint8_t> data;
		
		isobuf.printBoxes();
		isobuf.getMdatBoxSize(mdatLen);
		
		mdat = (uint8_t *)malloc(mdatLen);
		isobuf.parseMdatBox(mdat, mdatLen);
		for (int i = 0; i < mdatLen; i++)
			data.push_back(static_cast<uint8_t>(mdat[i]));
		PacketSender::Instance()->AddPacket(m_channel->generateDataPacket(data));
		PacketSender::Instance()->SendPackets();
		free(mdat);
		AAMPLOG_INFO("Sent buffer with size %d position %.3f\n", bufferLen, position);
	}
	else
	{
		AAMPLOG_INFO("%s:%d Init Segment", __FUNCTION__, __LINE__);
	}
}