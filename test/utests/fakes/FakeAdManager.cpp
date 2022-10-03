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

#include "admanager_mpd.h"

CDAIObjectMPD::CDAIObjectMPD(AampLogManager *logObj, PrivateInstanceAAMP* aamp): CDAIObject(logObj, aamp)
{
}

CDAIObjectMPD::~CDAIObjectMPD()
{
}

void CDAIObjectMPD::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url, uint64_t startMS, uint32_t breakdur)
{
}

