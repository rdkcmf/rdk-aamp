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

#include "AampProfiler.h"

ProfileEventAAMP::ProfileEventAAMP()
{
}

void ProfileEventAAMP::TuneBegin(void)
{
}

void ProfileEventAAMP::TuneEnd(TuneEndMetrics &mTuneEndMetrics,std::string appName, std::string playerActiveMode, int playerId, bool playerPreBuffered, unsigned int durationSeconds, bool interfaceWifi,std::string failureReason)
{
}

void ProfileEventAAMP::GetClassicTuneTimeInfo(bool success, int tuneRetries, int firstTuneType, long long playerLoadTime, int streamType, bool isLive,unsigned int durationinSec, char *TuneTimeInfoStr)
{
}

void ProfileEventAAMP::ProfileBegin(ProfilerBucketType type)
{
}

void ProfileEventAAMP::ProfileError(ProfilerBucketType type, int result)
{
}

void ProfileEventAAMP::ProfileEnd(ProfilerBucketType type)
{
}

void ProfileEventAAMP::ProfilePerformed(ProfilerBucketType type)
{
}