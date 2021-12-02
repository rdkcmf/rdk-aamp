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

#include <regex>


TtmlSubtecParser::TtmlSubtecParser(AampLogManager *logObj, PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(logObj, aamp, type), m_channel(nullptr)
{
	if (!PacketSender::Instance()->Init())
	{
		AAMPLOG_INFO("Init failed - subtitle parsing disabled");
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
	AAMPLOG_INFO("startPos %.3fs", startPosSeconds);
	m_channel->SendTimestampPacket(static_cast<uint64_t>(startPosSeconds * 1000.0));
	mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);

	m_parsedFirstPacket = false;
	m_sentOffset = false;
	m_firstBeginOffset = 0.0;

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

static std::int64_t parseFirstBegin(std::stringstream &ss)
{
	std::int64_t firstBegin = std::numeric_limits<std::int64_t>::max();
	std::string line;
	static const std::regex beginRegex(R"(begin="([0-9]+):([0-9][0-9]?):([0-9][0-9]?)\.?([0-9]+)?")");
	
	while(std::getline(ss, line))
	{
		try {
			bool matched = false;
			//Regex still works with no hours and/or no ms. Mins and secs are required
			std::smatch match;
			matched = std::regex_search(line, match, beginRegex);

			if (matched) {
				std::int64_t hours = 0, minutes = 0, seconds = 0, milliseconds = 0;

				if (!match.str(1).empty()) AAMPLOG_WARN("h:%s", match.str(1).c_str()); hours = std::stol(match.str(1));
				if (!match.str(2).empty()) AAMPLOG_WARN("m:%s", match.str(2).c_str()); minutes = std::stol(match.str(2));
				if (!match.str(3).empty()) AAMPLOG_WARN("s:%s", match.str(3).c_str()); seconds = std::stol(match.str(3));
				if (!match.str(4).empty()) AAMPLOG_WARN("ms:%s", match.str(4).c_str()); milliseconds = std::stol(match.str(4));

				firstBegin = milliseconds + (1000 * (seconds + (60 * (minutes + (60 * hours)))));
				break;
			}
		}
		catch (const std::regex_error& e) {
			AAMPLOG_WARN("Regex error %s from line %s", std::to_string(e.code()).c_str(), line.c_str());
		}
	}
	
	return firstBegin;
}

bool TtmlSubtecParser::processData(char* buffer, size_t bufferLen, double position, double duration)
{
	IsoBmffBuffer isobuf(mLogObj);

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
		
		//LLAMA-3328 - this hack is necessary because the offset into the TTML
		//is not available in the linear manifest
		//Take the first instance of the "begin" tag as the time offset for subtec
		if (!m_parsedFirstPacket && m_isLinear)
		{
			m_firstBeginOffset = position;
			m_parsedFirstPacket = true;
		}

		if (!m_sentOffset && m_parsedFirstPacket && m_isLinear)
		{
			AAMPLOG_TRACE("Linear content - parsing first begin as offset - pos %.3f dur %.3f m_firstBeginOffset %.3f", 
				 position, duration, m_firstBeginOffset);
			std::stringstream ss(std::string(data.begin(), data.end()));
			std::int64_t offset = parseFirstBegin(ss);
			
			if (offset != std::numeric_limits<std::int64_t>::max())
			{
				auto positionDeltaSecs = (position - m_firstBeginOffset);
				auto timeFromStartMs = mAamp->GetPositionMs() - (mAamp->seek_pos_seconds * 1000.0);
				std::int64_t totalOffset = offset - (positionDeltaSecs * 1000.0) + timeFromStartMs;

				std::stringstream output;
				output << "setting totalOffset " << totalOffset << " positionDeltaSecs " << positionDeltaSecs <<
					" timeFromStartMs " << timeFromStartMs;
				AAMPLOG_TRACE("%s",  output.str().c_str());
				m_sentOffset = true;
				m_channel->SendTimestampPacket(totalOffset);
			}
		}

		if (!m_isLinear)
		{
			//Additional Viper-only hack for LLAMA-4748
			//The original sub format was missing the "cellResolution" property
			//This means that new subs and old subs on VOD have wildly different font sizes
			//This workaround looks for cellResolution in the demuxed VOD TTML and adds the
			//originally intended default if necessary
			std::string ttml(data.begin(), data.end());

			if (ttml.find("ttp:cellResolution") == std::string::npos)
			{
				AAMPLOG_TRACE("No cellResolution found");
				std::size_t pos;
				if ((pos = ttml.find("<tt")) != std::string::npos)
				{
					pos += 4; // for "<tt "
					ttml.insert(pos, "ttp:cellResolution=\"40 30\" ");
					data.assign(ttml.begin(), ttml.end());
				}
			}
		}

		m_channel->SendDataPacket(std::move(data), 0);

		free(mdat);
		AAMPLOG_TRACE("Sent buffer with size %zu position %.3f", bufferLen, position);
	}
	else
	{
		AAMPLOG_INFO("Init Segment");
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
