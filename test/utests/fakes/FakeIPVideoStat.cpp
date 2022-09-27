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

#include "IPVideoStat.h"

size_t CSessionSummary::totalErrorCount = 0;
size_t ManifestGenericStats::totalGaps = 0;

char * CVideoStat::ToJsonString(const char* additionalData, bool forPA) const
{
    return nullptr;
}

void CVideoStat::Increment_Fragment_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity)
{
}

void CVideoStat::Increment_Init_Fragment_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity)
{
}

void CVideoStat::Increment_Manifest_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity, ManifestData * manifestData)
{
}

void CVideoStat::Record_License_EncryptionStat(VideoStatTrackType eType, bool isEncypted, bool isKeyChanged, int audioIndex)
{
}

void CVideoStat::SetFailedFragmentUrl(VideoStatTrackType eType, long bitrate, std::string url, int audioIndex)
{
}

void CVideoStat::Setlanguage(VideoStatTrackType eType, std::string strLang, int audioIndex)
{
}

void CVideoStat::Increment_Data(VideoStatDataType dataType,VideoStatTrackType eType, long bitrate , long downloadTimeMs, int response, bool connectivity, int audioIndex, ManifestData * manifestData)
{
}

void CVideoStat::SetProfileResolution(VideoStatTrackType eType, long bitrate, int width, int height, int audioIndex)
{
}

void CVideoStat::SetDisplayResolution(int width, int height)
{
}

void CVideoStat::IncrementGaps()
{
}
