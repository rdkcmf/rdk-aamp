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

std::string getTtmlHeader()
{
	return R"str(
	<head>
    <metadata xmlns:ttm="http://www.w3.org/ns/ttml#metadata">
      <ttm:title>WebVTT to TTML template</ttm:title>
    </metadata>
    <styling xmlns:tts="http://www.w3.org/ns/ttml#styling">
      <!-- s1 specifies default color, font, and text alignment -->
      <style xml:id="s1"
        tts:color="white"
        tts:fontFamily="XFINITY Sans Med"
        tts:fontSize="100%"
        tts:textAlign="start"
      />
    </styling>
    <layout xmlns:tts="http://www.w3.org/ns/ttml#styling">
      <region xml:id="subtitleArea"
        style="s1"
        tts:origin="10% 70%"
        tts:extent="80% 30%"
        tts:backgroundColor="#000000FF"
        tts:displayAlign="after"
      />
    </layout> 
    </head>)str";
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
					ss << "\n";
				stripHtmlTags(text);
				ss << text;
				first = false;
			}
		}
		ss << "</p>\n";
	}
	
	AAMPLOG_TRACE(" %s\n", ss.str().c_str());
	
	return ss.str();
}


WebVTTSubtecDevParser::WebVTTSubtecDevParser(AampLogManager *logObj, PrivateInstanceAAMP *aamp, SubtitleMimeType type) : WebVTTParser(logObj, aamp, type), m_channel(nullptr)
{
	if (!PacketSender::Instance()->Init())
	{
		AAMPLOG_WARN("Init failed - subtitle parsing disabled");
		throw std::runtime_error("PacketSender init failed");
	}
	m_channel = make_unique<TtmlChannel>();
	m_channel->SendResetAllPacket();
	int width = 1920, height = 1080;

	mAamp->GetPlayerVideoSize(width, height);
	m_channel->SendSelectionPacket(width, height);
	m_channel->SendMutePacket();
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
	std::vector<uint8_t> data(ttml.begin(), ttml.end());
	
	m_channel->SendDataPacket(std::move(data), 0);
}

void WebVTTSubtecDevParser::reset()
{
	if (!mReset)
	{
		m_channel->SendResetChannelPacket();
	}
	WebVTTParser::reset();
}

void WebVTTSubtecDevParser::updateTimestamp(unsigned long long positionMs)
{
	AAMPLOG_TRACE("timestamp: %lldms", positionMs );
	m_channel->SendTimestampPacket(positionMs);
}

bool WebVTTSubtecDevParser::init(double startPosSeconds, unsigned long long basePTS)
{
	bool ret = true;
	mVttQueueIdleTaskId = -1;

	ret = WebVTTParser::init(startPosSeconds, 0);
	mVttQueueIdleTaskId = 0;

	m_channel->SendTimestampPacket(static_cast<uint64_t>(basePTS));


	return ret;
}

void WebVTTSubtecDevParser::mute(bool mute)
{
	if (mute)
		m_channel->SendMutePacket();
	else
		m_channel->SendUnmutePacket();
}

void WebVTTSubtecDevParser::pause(bool pause)
{
	if (pause)
		m_channel->SendPausePacket();
	else
		m_channel->SendResumePacket();
}

/**
 * @}
 */
std::string WebVTTSubtecDevParser::getVttAsTtml()
{	
	pthread_mutex_lock(&mVttQueueMutex);
	std::string ss;
	int counter = 0;
	
	ss += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	ss += "<tt xmlns=\"http://www.w3.org/ns/ttml\">\n";
	ss += getTtmlHeader();
	ss += "<body region=\"subtitleArea\">\n";
	ss += "<div>\n";
	
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
				ss += convertCueToTtmlString(counter++, cue, start);
			}
			else
			{
				AAMPLOG_TRACE("queue size %zu cue start %.3f", mVttQueue.size(), cue->mStart);
			}
			SAFE_DELETE(cue);
		}
	}
	else
	{
		AAMPLOG_TRACE("cue queue empty");
		AAMPLOG_INFO("Outgoing WebVTT file with start pos %.3fs is EMPTY",  mStartPos / 1000.0);
	}

	ss += "</div>\n";
	ss += "</body>\n";
	ss += "</tt>\n";


	pthread_mutex_unlock(&mVttQueueMutex);

	return ss;
}
