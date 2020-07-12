/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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


/**
* @file AampDRMutils.h
* @brief Context-free common utility functions.
*/

#ifndef __AAMP_UTILS_H__
#define __AAMP_UTILS_H__

#include <string>
#include <sstream>

/**
 * @brief Get current time from epoch is milliseconds
 *
 * @retval - current time in milliseconds
 */
long long aamp_GetCurrentTimeMS(void); //TODO: Use NOW_STEADY_TS_MS/NOW_SYSTEM_TS_MS instead

/**
 * @brief Extract host string from url
 *
 * @param url - Input URL
 * @retval - host of input url
 */
std::string aamp_getHostFromURL(std::string url);

/**
 * @brief Create file URL from the base and file path
 *
 * @param[out] dst - Created URL
 * @param[in] base - Base URL
 * @param[in] uri - File path
 * @retval void
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri);

/**
 * @brief Check if string start with a prefix
 *
 * @param[in] inputStr - Input string
 * @param[in] prefix - substring to be searched
 * @retval TRUE if substring is found in bigstring
 */
bool aamp_StartsWith( const char *inputStr, const char *prefix);

char *aamp_Base64_URL_Encode(const unsigned char *src, size_t len);

unsigned char *aamp_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen);

/**
 * @brief Parse date time from ISO8601 string and return value in seconds
 * @param ptr ISO8601 string
 * @retval durationMs duration in milliseconds
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr);


#endif  /* __AAMP_UTILS_H__ */
