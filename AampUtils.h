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
* @struct	FormatMap
* @brief	FormatMap structure for stream codec/format information
*/
struct FormatMap
{
	const char* codec;
	StreamOutputFormat format;
};

/*
* @fn GetAudioFormatStringForCodec
* @brief Function to get audio codec string from the map.
*
* @param[in] input Audio codec type
* @return Audio codec string
*/
const char * GetAudioFormatStringForCodec ( StreamOutputFormat input);

/*
* @fn GetAudioFormatForCodec
* @brief Function to get audio codec from the map.
*
* @param[in] Audio codec string
* @return Audio codec map
*/
const FormatMap * GetAudioFormatForCodec( const char *codecs );

/*
* @fn GetVideoFormatForCodec
* @brief Function to get video codec from the map.
*
* @param[in] Video codec string
* @return Video codec map
*/
const FormatMap * GetVideoFormatForCodec( const char *codecs );

/**
 * @fn aamp_GetCurrentTimeMS
 *
 */
long long aamp_GetCurrentTimeMS(void); //TODO: Use NOW_STEADY_TS_MS/NOW_SYSTEM_TS_MS instead

/**
 * @fn replace
 * @param str string to be scanned/modified
 * @param existingSubStringToReplace substring to be replaced
 * @param replacementString string to be substituted
 * @retval true iff str was modified
 */
bool replace(std::string &str, const char *existingSubStringToReplace, const char *replacementString);

/**
 * @fn aamp_IsAbsoluteURL
 *
 * @param[in] url - Input URL
 */
bool aamp_IsAbsoluteURL( const std::string &url );

/**
 * @fn aamp_getHostFromURL
 *
 * @param url - Input URL
 * @retval host of input url
 */
std::string aamp_getHostFromURL(std::string url);

/**
 * @fn aamp_ResolveURL
 *
 * @param[out] dst - Created URL
 * @param[in] base - Base URL
 * @param[in] uri - File path
 * @param[in] bPropagateUriParams - flag to use base uri params
 * @retval void
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams);

/**
 * @fn aamp_StartsWith
 *
 * @param[in] inputStr - Input string
 * @param[in] prefix - substring to be searched
 */
bool aamp_StartsWith( const char *inputStr, const char *prefix);

/**
 * @fn aamp_Base64_URL_Encode
 * @param src pointer to first byte of binary data to be encoded
 * @param len number of bytes to encode
 */
char *aamp_Base64_URL_Encode(const unsigned char *src, size_t len);

/**
 * @fn aamp_Base64_URL_Decode
 * @param src pointer to cstring containing base64-URL-encoded data
 * @param len receives byte length of returned pointer, or zero upon failure
 * @param srcLen source data length
 */
unsigned char *aamp_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen);

/**
 * @brief unescape uri-encoded uri parameter
 * @param uriParam string to un-escape
 */
void aamp_DecodeUrlParameter( std::string &uriParam );
/**
 * @fn ISO8601DateTimeToUTCSeconds
 * @param ptr ISO8601 string
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr);

/**
 * @fn aamp_PostJsonRPC
 * @param[in] id string containing player id
 * @param[in] method string containing JSON method
 * @param[in] params string containing params:value pair list
 */
std::string aamp_PostJsonRPC( std::string id, std::string method, std::string params );

/**
 * @fn aamp_GetDeferTimeMs
 *
 * @param  maxTimeSeconds Maximum time allowed for deferred license acquisition
 */
int aamp_GetDeferTimeMs(long maxTimeSeconds);

/**
 * @fn GetDrmSystemName
 * @param drmSystem drm system
 */
const char * GetDrmSystemName(DRMSystems drmSystem);

/**
 * @fn GetDrmSystem
 * @param drmSystemID - Id of the DRM system, empty string if not supported
 */
DRMSystems GetDrmSystem(std::string drmSystemID);

/**
 * @fn GetDrmSystemID
 * @param drmSystem - drm system
 */
const char * GetDrmSystemID(DRMSystems drmSystem);

/**
 * @fn UrlEncode
 *
 * @param[in] inStr - Input URL
 * @param[out] outStr - Encoded URL
 */
void UrlEncode(std::string inStr, std::string &outStr);

/**
 * @fn trim
 * @param[in][out] src Buffer containing string
 */
void trim(std::string& src);

/**
 * @fn Getiso639map_NormalizeLanguageCode 
 * @param[in] lang - Language in string format
 * @param[in] preferFormat - Preferred language foramt
 */
std::string Getiso639map_NormalizeLanguageCode(std::string  lang, LangCodePreference preferFormat );

/**
 * @fn aamp_GetTimespec
 * @param[in] timeInMs 
 */
struct timespec aamp_GetTimespec(int timeInMs);

/**
 * @fn aamp_WriteFile
 * @param fileName - out file name
 * @param data - buffer
 * @param len - length of buffer
 * @param fileType - Media type of file
 * @param count - for manifest or playlist update
 * @param prefix - prefix name
 */
bool aamp_WriteFile(std::string fileName, const char* data, size_t len, MediaType &fileType, unsigned int count,const char *prefix);
/**
 * @fn getHarvestConfigForMedia
 * @param fileType - meida file type
 */
int getHarvestConfigForMedia(MediaType fileType);
/**
 * @fn getWorkingTrickplayRate
 * @param rate input rate
 */
int getWorkingTrickplayRate(int rate);

/**
 * @fn getPseudoTrickplayRate 
 * @param rate working rate
 */
int getPseudoTrickplayRate(int rate);

/**
 * @fn getDefaultHarvestPath
 * @param[in] value - harvest path
 * @return void
 */

void getDefaultHarvestPath(std::string &value);

/**
 * @fn stream2hex
 * @param[in] str input string
 * @param[out] hexstr output hex string
 * @param[in] capital - Boolean to enable capital letter conversion flag
 */
void stream2hex(const std::string str, std::string& hexstr, bool capital = false);

/**
 * @fn mssleep
 * @param milliseconds Time to sleep
 */
void mssleep(int milliseconds);

#endif  /* __AAMP_UTILS_H__ */
