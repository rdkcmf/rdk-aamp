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

#pragma once

#include "webvttParser.h"
#include "vttCue.h"
#include "PacketSender.hpp"
#include "TtmlPacket.hpp"

class WebVTTSubtecDevParser : public WebVTTParser
{
public:
	WebVTTSubtecDevParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type);
	
	WebVTTSubtecDevParser(const WebVTTSubtecDevParser&) = delete;
	WebVTTSubtecDevParser& operator=(const WebVTTSubtecDevParser&) = delete;
	
	bool init(double startPosSeconds, unsigned long long basePTS) override;
	bool processData(char* buffer, size_t bufferLen, double position, double duration) override;
	void reset() override;
	void sendCueData() override;
	void setProgressEventOffset(double offset) override {}
	void updateTimestamp(unsigned long long positionMs) override;
	void pause(bool pause) override;
	void mute(bool mute) override;
protected:
	std::unique_ptr<TtmlChannel> m_channel;
private:
	std::string getVttAsTtml();
};