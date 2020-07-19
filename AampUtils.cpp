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
 * @file AampUtils.cpp
 * @brief Common utility functions
 */

#include "AampUtils.h"
#include "_base64.h"

#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <ctime>
#include <curl/curl.h>

/**
 * @brief Get current time stamp
 *
 * @retval - current clock time as milliseconds
 */
long long aamp_GetCurrentTimeMS(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
}

/**
 * @brief Resolve URL from base and uri
 *
 * @param[out] dst Destination buffer
 * @param[in] base Base URL
 * @param[in] uri manifest/ fragment uri
 * @retval void
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri)
{
	if (memcmp(uri, "http://", 7) != 0 && memcmp(uri, "https://", 8) != 0) // explicit endpoint - needed for DAI playlist
	{
		dst = base;

		std::size_t pos;
		if (uri[0] == '/')
		{	// absolute path; preserve only endpoint http://<endpoint>:<port>/
			//e.g uri = "/vod/video/00000001.ts"
			// base = "https://host.com/folder1/manifest.m3u8"
			// dst = "https://host.com/vod/video/00000001.ts"
			pos = dst.find("://");
			if (pos != std::string::npos)
			{
				pos = dst.find('/', pos + 3);
			}
			pos--; // skip the "/" as uri starts with "/" , this is done to avoid double "//" in URL which sometimes gives HTTP-404
		}
		else
		{	// relative path; include base directory
			// e.g base = "http://127.0.0.1:9080/manifests/video1/manifest.m3u8"
			// uri = "frag-787563519.ts"
			// dst = http://127.0.0.1:9080/manifests/video1/frag-787563519.ts
			pos = std::string::npos;
			const char *ptr = dst.c_str();
			std::size_t idx = 0;
			for(;;)
			{
				char c = ptr[idx];
				if( c=='/' )
				{ // remember final '/'
					pos = idx;
				}
				else if( c == '?' || c==0 )
				{ // bail if we find uri param delimiter or reach end of stream
					break;
				}
				idx++;
			}
		}

		assert(pos != std::string::npos);
		dst.replace(pos+1, std::string::npos, uri);

		if (strchr(uri, '?') == 0)//if uri doesn't already have url parameters, then copy from the parents(if they exist)
		{
			pos = base.find('?');
			if (pos != std::string::npos)
			{
				std::string params = base.substr(pos);
				dst.append(params);
			}
		}
	}
	else
		dst = uri; // uri = "http://host.com/video/manifest.m3u8"
}

/**
 * @brief Extract host string from url
 *
 * @param[in] url - Input URL
 * @retval - host of input url
 */
std::string aamp_getHostFromURL(std::string url)
{
	std::string host = "";
	std::size_t start_pos = std::string::npos;
	if(url.rfind("http://", 0) == 0)
	{ // starts with http://
		start_pos = 7;
	}
	else if(url.rfind("https://", 0) == 0)
	{ // starts with https://
		start_pos = 8;
	}
	if(start_pos != std::string::npos)
	{
		std::size_t pos = url.find('/', start_pos);
		if(pos != std::string::npos)
		{
			host = url.substr(start_pos, (pos - start_pos));
		}
	}
	return host;
}

/**
 * @brief Check if string start with a prefix
 *
 * @param[in] inputStr - Input string
 * @param[in] prefix - substring to be searched
 * @retval TRUE if substring is found in bigstring
 */
bool aamp_StartsWith( const char *inputStr, const char *prefix )
{
	bool rc = true;
	while( *prefix )
	{
		if( *inputStr++ != *prefix++ )
		{
			rc = false;
			break;
		}
	}
	return rc;
}

/**
 * @brief convert blob of binary data to ascii base64-URL-encoded equivalent
 * @param src pointer to first byte of binary data to be encoded
 * @param len number of bytes to encode
 * @retval pointer to malloc'd cstring containing base64 URL encoded version of string
 * @retval NULL if insufficient memory to allocate base64-URL-encoded copy
 * @note caller responsible for freeing returned cstring
 */
char *aamp_Base64_URL_Encode(const unsigned char *src, size_t len)
{
	char * b64Src = base64_Encode(src, len);
	size_t encodedLen = strlen(b64Src);
	char* urlEncodedSrc = (char*)malloc(sizeof(char) * (encodedLen + 1));
	for (int iter = 0; iter < encodedLen; iter++)
	{
		if (b64Src[iter] == '+')
		{
			urlEncodedSrc[iter] = '-';
		}
		else if (b64Src[iter] == '/')
		{
			urlEncodedSrc[iter] = '_';
		}
		else if (b64Src[iter] == '=')
		{
			urlEncodedSrc[iter] = '\0';
			break;
		}
		else
		{
			urlEncodedSrc[iter] = b64Src[iter];
		}
	}
	free(b64Src);
	urlEncodedSrc[encodedLen] = '\0';
	return urlEncodedSrc;
}


/**
 * @brief decode base64 URL encoded data to binary equivalent
 * @param src pointer to cstring containing base64-URL-encoded data
 * @param len receives byte length of returned pointer, or zero upon failure
 * @retval pointer to malloc'd memory containing decoded binary data
 * @retval NULL if insufficient memory to allocate decoded data
 * @note caller responsible for freeing returned data
 */

unsigned char *aamp_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen)
{
	//Calculate the size for corresponding Base64 Encoded string with padding
	int b64Len = (((srcLen / 4) + 1) * 4) + 1;
	char *b64Src = (char *) malloc(sizeof(char)* b64Len);
	b64Src[b64Len - 1] = '\0';
	b64Src[b64Len - 2] = '=';
	b64Src[b64Len - 3] = '=';
	for (int iter = 0; iter < strlen(src); iter++) {
		if (src[iter] == '-') {
			b64Src[iter] = '+';
		} else if (src[iter] == '_') {
			b64Src[iter] = '/';
		} else {
			b64Src[iter] = src[iter];
		}
	}
	*len = 0;
	unsigned char * decodedStr = base64_Decode(b64Src, len);
	free(b64Src);
	return decodedStr;
}


/**
 * @brief Parse date time from ISO8601 string and return value in seconds
 * @param ptr ISO8601 string
 * @retval durationMs duration in milliseconds
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr)
{
	double timeSeconds = 0;
	if(ptr)
	{
		time_t offsetFromUTC = 0;
		std::tm timeObj = { 0 };
		char *msString;
		double msvalue = 0.0;;

		//Find out offset from utc by convering epoch
		std::tm baseTimeObj = { 0 };
		strptime("1970-01-01T00:00:00.", "%Y-%m-%dT%H:%M:%S.", &baseTimeObj);
		offsetFromUTC = mktime(&baseTimeObj);

		//Convert input string to time
		msString = strptime(ptr, "%Y-%m-%dT%H:%M:%S.", &timeObj);

		if(msString)
		{
			msvalue = (double)(atoi(msString)/1000.0);
		}

		timeSeconds = (mktime(&timeObj) - offsetFromUTC) + msvalue;
	}
	return timeSeconds;
}

/**
 * @brief unescape uri-encoded uri parameter
 * @param uriParam string to un-escape
 */
void aamp_DecodeUrlParameter( std::string &uriParam )
{
	std::string rc;
	CURL *curl = curl_easy_init();
	if (curl != NULL)
	{
		int unescapedLen;
		const char* unescapedData = curl_easy_unescape(curl, uriParam.c_str(), uriParam.size(), &unescapedLen);
		if (unescapedData != NULL)
		{
			uriParam = std::string(unescapedData, unescapedLen);
			curl_free((void*)unescapedData);
		}
		curl_easy_cleanup(curl);
	}
}
