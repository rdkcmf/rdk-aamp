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

#include "AampDrmSystems.h"
#include "main_aamp.h"
#include "iso639map.h"

#include <string>
#include <sstream>
#include <chrono>

#define NOW_SYSTEM_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()     /**< Getting current system clock in milliseconds */
#define NOW_STEADY_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()     /**< Getting current steady clock in milliseconds */

#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))

//Delete non-array object
#define SAFE_DELETE(ptr) { delete(ptr); ptr = NULL; }
//Delete Array object
#define SAFE_DELETE_ARRAY(ptr) { delete [] ptr; ptr = NULL; }

/**
 * @brief Get current time from epoch is milliseconds
 *
 * @retval - current time in milliseconds
 */
long long aamp_GetCurrentTimeMS(void); //TODO: Use NOW_STEADY_TS_MS/NOW_SYSTEM_TS_MS instead

bool aamp_IsAbsoluteURL( const std::string &url );

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
 * @param[in] propagateUriParams - flag to use base uri params
 * @retval void
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams);

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

void aamp_DecodeUrlParameter( std::string &uriParam );
/**
 * @brief Parse date time from ISO8601 string and return value in seconds
 * @param ptr ISO8601 string
 * @retval durationMs duration in milliseconds
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr);

/**
 * @brief aamp_PostJsonRPC posts JSONRPC data
 * @param[in] id string containing player id
 * @param[in] method string containing JSON method
 * @param[in] params string containing params:value pair list
 */
std::string aamp_PostJsonRPC( std::string id, std::string method, std::string params );

/**
 * @brief Get time to defer DRM acquisition
 *
 * @param  maxTimeSeconds Maximum time allowed for deferred license acquisition
 * @return Time in MS to defer DRM acquisition
 */
int aamp_GetDeferTimeMs(long maxTimeSeconds);

/**
 * @brief Get name of DRM system
 * @param drmSystem drm system
 * @retval Name of the DRM system, empty string if not supported
 */
const char * GetDrmSystemName(DRMSystems drmSystem);

/**
 * @brief Get DRM system from ID
 * @param ID of the DRM system, empty string if not supported
 * @retval drmSystem drm system
 */
DRMSystems GetDrmSystem(std::string drmSystemID);

/**
 * @brief Get ID of DRM system
 * @param drmSystem drm system
 * @retval ID of the DRM system, empty string if not supported
 */
const char * GetDrmSystemID(DRMSystems drmSystem);

/**
 * @brief Encode URL
 *
 * @param[in] inStr - Input URL
 * @param[out] outStr - Encoded URL
 * @return Encoding status
 */
void UrlEncode(std::string inStr, std::string &outStr);

/**
 * @brief Trim a string
 * @param[in][out] src Buffer containing string
 */
void trim(std::string& src);

/**
 * @brief To get the preferred iso639mapped language code
 * @param[in] lang
 * @param[LangCodePreference] preferFormat
 * @retval[out] preferred iso639 mapped language.
 */
std::string Getiso639map_NormalizeLanguageCode(std::string  lang, LangCodePreference preferFormat );

/**
 * @brief To get the timespec
 * @param[in] timeInMs 
 * @retval[out] timespec.
 */
struct timespec aamp_GetTimespec(int timeInMs);

/**
 * @brief Write file to storage
 * @param fileName out file name
 * @param data buffer
 * @param len length of buffer
 * @param media type of file
 * @param count for manifest or playlist update
 */
bool aamp_WriteFile(std::string fileName, const char* data, size_t len, MediaType &fileType, unsigned int count,const char *prefix);
/**
 * @brief Get harvest config corresponds to Media type
 * @param fileType meida file type
 * @return harvestType
 */
int getHarvestConfigForMedia(MediaType fileType);
/**
 * @brief Get compatible trickplay for 6s cadense of iframe track from the given rates
 * @param rate input rate
 */
int getWorkingTrickplayRate(int rate);

/**
 * @brief Get reverse map the working rates to the rates given by platform player
 * @param rate working rate
 */
int getPseudoTrickplayRate(int rate);

/**
 * @brief Get harvest path to dump the files
 * @return harvest path
 */

void getDefaultHarvestPath(std::string &);

/**
 * @brief Convert string of chars to its representative string of hex numbers
 * @param[in] str input string
 * @param[out] hexstr output hex string
 * @param[in] bool - to enable capital letter conversion flag
 */
void stream2hex(const std::string str, std::string& hexstr, bool capital = false);

#endif  /* __AAMP_UTILS_H__ */
