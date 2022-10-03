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

#include "WebvttSubtecDevParser.hpp"

WebVTTSubtecDevParser::WebVTTSubtecDevParser(AampLogManager *logObj, PrivateInstanceAAMP *aamp, SubtitleMimeType type) : WebVTTParser(logObj, aamp, type), m_channel(nullptr)
{
}

bool WebVTTSubtecDevParser::processData(char *buffer, size_t bufferLen, double position, double duration)
{	
	return true;
}

void WebVTTSubtecDevParser::sendCueData()
{
}

void WebVTTSubtecDevParser::reset()
{
}

void WebVTTSubtecDevParser::updateTimestamp(unsigned long long positionMs)
{
}

bool WebVTTSubtecDevParser::init(double startPosSeconds, unsigned long long basePTS)
{
	return true;
}

void WebVTTSubtecDevParser::mute(bool mute)
{
}

void WebVTTSubtecDevParser::pause(bool pause)
{
}

/**
 * @}
 */
std::string WebVTTSubtecDevParser::getVttAsTtml()
{	
	return "";
}
