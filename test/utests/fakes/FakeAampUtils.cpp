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

#include "AampUtils.h"

long long aamp_GetCurrentTimeMS(void)
{
    return 0;
}

float getWorkingTrickplayRate(float rate)
{
    return 0.0;
}

void aamp_DecodeUrlParameter( std::string &uriParam )
{
}

bool replace(std::string &str, const char *existingSubStringToReplace, const char *replacementString)
{
    return false;
}

bool aamp_IsLocalHost ( std::string Hostname )
{
    return false;
}

struct timespec aamp_GetTimespec(int timeInMs)
{
	struct timespec tspec;
	return tspec;
}

float getPseudoTrickplayRate(float rate)
{
    return 0.0;
}

std::string aamp_getHostFromURL(std::string url)
{
    return "";
}

int getHarvestConfigForMedia(MediaType fileType)
{
    return 0;
}

void getDefaultHarvestPath(std::string &value)
{
}

bool aamp_WriteFile(std::string fileName, const char* data, size_t len, MediaType &fileType, unsigned int count,const char *prefix)
{
    return false;
}