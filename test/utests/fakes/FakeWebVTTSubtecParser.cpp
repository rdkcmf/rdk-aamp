/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

WebVTTSubtecParser::WebVTTSubtecParser(AampLogManager *logObj, PrivateInstanceAAMP *aamp, SubtitleMimeType type) : SubtitleParser(logObj, aamp, type), m_channel(nullptr)
{
}

void WebVTTSubtecParser::updateTimestamp(unsigned long long positionMs)
{
}

void WebVTTSubtecParser::reset()
{
}

bool WebVTTSubtecParser::init(double startPosSeconds, unsigned long long basePTS)
{
	return true;
}

bool WebVTTSubtecParser::processData(char* buffer, size_t bufferLen, double position, double duration)
{
	return true;
}

void WebVTTSubtecParser::mute(bool mute)
{
}

void WebVTTSubtecParser::pause(bool pause)
{
}
