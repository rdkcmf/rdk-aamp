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

#include "WebvttSubtecDevParser.hpp"
#include "PacketSender.hpp"

std::string getTtmlHeader()
{
	std::ostringstream str; 
	str << "<head>" << std::endl;
    str << "<metadata xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\">" << std::endl;
    str << "  <ttm:title>WebVTT to TTML template</ttm:title>" << std::endl;
    str << "</metadata>" << std::endl;
    str << "<styling xmlns:tts=\"http://www.w3.org/ns/ttml#styling\">" << std::endl;
    str << "  <!-- s1 specifies default color, font, and text alignment -->" << std::endl;
    str << "  <style xml:id=\"s1\"" << std::endl;
    str << "    tts:color=\"white\"" << std::endl;
    str << "    tts:fontFamily=\"XFINITY Sans Med\"" << std::endl;
    str << "    tts:fontSize=\"100%\"" << std::endl;
    str << "    tts:textAlign=\"start\"" << std::endl;
    str << "  />" << std::endl;
    str << "</styling>" << std::endl;
    str << "<layout xmlns:tts=\"http://www.w3.org/ns/ttml#styling\">" << std::endl;
    str << "  <region xml:id=\"subtitleArea\"" << std::endl;
    str << "    style=\"s1\"" << std::endl;
    str << "    tts:origin=\"10% 70%\"" << std::endl;
    str << "    tts:extent=\"80% 30%\"" << std::endl;
    str << "    tts:backgroundColor=\"#000000FF\"" << std::endl;
    str << "    tts:displayAlign=\"after\"" << std::endl;
    str << "  />" << std::endl;
    str << "</layout> " << std::endl;
    str << "</head>" << std::endl;
	
	return str.str();
}

void stripHtmlTags(std::string& str)
{
    size_t startpos = std::string::npos, endpos = std::string::npos;

    do
    {
        startpos = str.find_first_of('<');
        if (startpos != std::string::npos)
        {
            endpos = str.find_first_of('>');
            if (endpos != std::string::npos)
            {
                str.erase(startpos, endpos - startpos + 1);
                continue;
            }
        }
        break;
    } while (true);
}

std::string convertCueToTtmlString(int id, VTTCue *cue, double startTime)
{
	std::ostringstream ss;
	std::string text;
	std::istringstream is(cue->mText);
	bool first = true;
	
	if (NULL != cue)
	{		
		ss << "<p xml:id=\"subtitle" << id << "\" begin=\"" << startTime / 1000.0 << "s\" end=\"" << (startTime + cue->mDuration) / 1000.0 << "s\" style=\"s1\">";
		while (std::getline(is, text))
		{
			if (!text.empty())
			{
				if (!first)
					ss << std::endl;
				stripHtmlTags(text);
				ss << text;
				first = false;
			}
		}
		ss << "</p>";
	}
	
	AAMPLOG_TRACE("%s: %s\n", __FUNCTION__, ss.str().c_str());
	
	return ss.str();
}

WebVTTSubtecDevParser::WebVTTSubtecDevParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type) : WebVTTParser(aamp, type), m_channel(nullptr)
{
	m_channel = make_unique<TtmlChannel>();
	PacketSender::Instance()->SendPacket(m_channel->generateResetAllPacket());
}

bool WebVTTSubtecDevParser::processData(char *buffer, size_t bufferLen, double position, double duration)
{	
	bool ret;
	
	if (mReset) mReset = false;
	ret = WebVTTParser::processData(buffer, bufferLen, position, duration);
	
	if (ret) sendCueData();
	
	return ret;
}

void WebVTTSubtecDevParser::sendCueData()
{
	std::string ttml = getVttAsTtml();
	std::vector<uint8_t> data;
	
	for(const uint8_t &ch : ttml)
		data.push_back(ch);
	
	PacketSender::Instance()->SendPacket(m_channel->generateDataPacket(data));
}

void WebVTTSubtecDevParser::reset()
{
	if (!mReset)
	{
		PacketSender::Instance()->SendPacket(m_channel->generateResetChannelPacket());
	}
	WebVTTParser::reset();
}

void WebVTTSubtecDevParser::updateTimestamp(unsigned long long positionMs)
{
	AAMPLOG_TRACE("%s: timestamp: %lldms\n", __FUNCTION__, positionMs );
	PacketSender::Instance()->SendPacket(m_channel->generateTimestampPacket(positionMs));
}

bool WebVTTSubtecDevParser::init(double startPos, unsigned long long basePTS)
{	
	if (!PacketSender::Instance()->Init())
	{
		AAMPLOG_WARN("%s: Init failed - subtitle parsing disabled\n", __FUNCTION__);
		return false;
	}
	
	int width = 1280, height = 720;
	bool ret = true;
	
	mAamp->GetPlayerVideoSize(width, height);
	PacketSender::Instance()->SendPacket(m_channel->generateSelectionPacket(width, height));
	PacketSender::Instance()->SendPacket(m_channel->generateTimestampPacket(static_cast<uint64_t>(startPos * 1000)));
	
	mVttQueueIdleTaskId = -1;

	uint64_t adjustedPts = 0;
	uint64_t startPosPts = static_cast<uint64_t>(startPos * 90000.0);

	if (basePTS > startPosPts)
	{
		adjustedPts = basePTS - startPosPts;
	}

	ret = WebVTTParser::init(startPos, adjustedPts);
	mVttQueueIdleTaskId = 0;
	
	return ret;
}

void WebVTTSubtecDevParser::mute(bool mute)
{
	if (mute)
		PacketSender::Instance()->SendPacket(m_channel->generateMutePacket());
	else
		PacketSender::Instance()->SendPacket(m_channel->generateUnmutePacket());
}

void WebVTTSubtecDevParser::pause(bool pause)
{
	if (pause)
		PacketSender::Instance()->SendPacket(m_channel->generatePausePacket());
	else
		PacketSender::Instance()->SendPacket(m_channel->generateResumePacket());
}

/**
 * @}
 */
std::string WebVTTSubtecDevParser::getVttAsTtml()
{	
	pthread_mutex_lock(&mVttQueueMutex);
	std::ostringstream ss;
	int counter = 0;
	
	ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
	ss << "<tt xmlns=\"http://www.w3.org/ns/ttml\">" << std::endl;
	ss << getTtmlHeader();
	ss << "<body region=\"subtitleArea\">";
	ss << "<div>";
	
	if (!mVttQueue.empty())
	{
		while(mVttQueue.size() > 0)
		{
			VTTCue *cue = mVttQueue.front();
			mVttQueue.pop();
			AAMPLOG_TRACE("mStart %.3f mStartPos %.3f mPtsOffset %lld", cue->mStart, mStartPos, mPtsOffset);
			int start = cue->mStart;
			if (start > 0)
			{	
				ss << convertCueToTtmlString(counter++, cue, start);
			}
			else
			{
				AAMPLOG_TRACE("%s: queue size %zu cue start %.3f", __FUNCTION__, mVttQueue.size(), cue->mStart);
			}
			delete cue;
		}
	}
	else
	{
		AAMPLOG_TRACE("%s: cue queue empty", __FUNCTION__);
		AAMPLOG_INFO("Outgoing WebVTT file with start pos %.3fs is EMPTY", __FUNCTION__, mStartPos / 1000.0);
	}

	ss << "</div>";
	ss << "</body>" << std::endl;
	ss << "</tt>" << std::endl;


	pthread_mutex_unlock(&mVttQueueMutex);

	return ss.str();
}
