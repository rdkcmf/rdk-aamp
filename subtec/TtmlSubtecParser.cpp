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


TtmlSubtecParser::TtmlSubtecParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(aamp, type), m_channel(nullptr)
{
	if (!PacketSender::Instance()->Init())
	{
		AAMPLOG_INFO("%s: Init failed - subtitle parsing disabled", __FUNCTION__);
		throw std::runtime_error("PacketSender init failed");
	}
	m_channel = make_unique<TtmlChannel>();
	m_channel->SendResetAllPacket();

	int width = 1920, height = 1080;
	mAamp->GetPlayerVideoSize(width, height);
	m_channel->SendSelectionPacket(width, height);
	m_channel->SendMutePacket();
	mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
}

bool TtmlSubtecParser::init(double startPosSeconds, unsigned long long basePTS)
{
	AAMPLOG_INFO("%s:%d startPos %.3fs", __FUNCTION__, __LINE__, startPosSeconds);
	m_channel->SendTimestampPacket(static_cast<uint64_t>(startPosSeconds * 1000.0));
	mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
	return true;
}

void TtmlSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	m_channel->SendTimestampPacket(positionMs);
}

void TtmlSubtecParser::reset()
{
	m_channel->SendResetChannelPacket();
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

		//isobuf.printBoxes();
		isobuf.getMdatBoxSize(mdatLen);

		mdat = (uint8_t *)malloc(mdatLen);
		isobuf.parseMdatBox(mdat, mdatLen);

		std::vector<uint8_t> data(mdatLen);
		data.assign(mdat, mdat+mdatLen);

		m_channel->SendDataPacket(std::move(data));

		free(mdat);
		AAMPLOG_TRACE("Sent buffer with size %zu position %.3f", bufferLen, position);
	}
	else
	{
		AAMPLOG_INFO("%s:%d Init Segment", __FUNCTION__, __LINE__);
	}
	return true;
}

void TtmlSubtecParser::mute(bool mute)
{
	if (mute)
		m_channel->SendMutePacket();
	else
		m_channel->SendUnmutePacket();
}

void TtmlSubtecParser::pause(bool pause)
{
	if (pause)
		m_channel->SendPausePacket();
	else
		m_channel->SendResumePacket();
}
