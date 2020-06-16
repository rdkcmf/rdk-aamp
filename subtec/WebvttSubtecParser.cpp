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

#include "WebvttSubtecParser.hpp"
#include "PacketSender.hpp"


WebVTTSubtecParser::WebVTTSubtecParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(aamp, type)
{
}

void WebVTTSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	// m_sink->SendTimestamp(positionMs);
}

bool WebVTTSubtecParser::init(double startPos, unsigned long long basePTS)
{	
	bool ret;
	
	ret = PacketSender::Instance()->Init();
	if (ret)
	{
		// m_sink->Reset();
		int width = 0, height = 0;
		mAamp->GetPlayerVideoSize(width, height);
		// m_sink->Start(width, height);
		// m_sink->SendTimestamp(startPos);
		mAamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
	}
	return ret;
}