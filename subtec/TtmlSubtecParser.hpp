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

#include "subtitleParser.h"
#include "PacketSender.hpp"
#include "isobmffbuffer.h"
#include "TtmlPacket.hpp"

class TtmlSubtecParser : public SubtitleParser
{
public:
	TtmlSubtecParser(PrivateInstanceAAMP *aamp, SubtitleMimeType type);
	
	TtmlSubtecParser(const TtmlSubtecParser&) = delete;
	TtmlSubtecParser& operator=(const TtmlSubtecParser&) = delete;

	
	bool init(double startPosSeconds, unsigned long long basePTS);
	bool processData(char* buffer, size_t bufferLen, double position, double duration);
	bool close() { return true; }
	void reset();
	void setProgressEventOffset(double offset) {}
	void updateTimestamp(unsigned long long positionMs);
	void pause(bool pause) override;
	void mute(bool mute) override;

protected:
	std::unique_ptr<TtmlChannel> m_channel;
	std::int64_t mTimeOffset = 0;
	bool mParsedFirstPacket = false;
};
