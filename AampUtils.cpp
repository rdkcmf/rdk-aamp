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
#include "GlobalConfigAAMP.h" //For logprintf
#include "AampConstants.h"

#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <ctime>
#include <curl/curl.h>

#ifdef USE_MAC_FOR_RANDOM_GEN
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "base16.h"
#endif

#define DEFER_DRM_LIC_OFFSET_FROM_START 5
#define DEFER_DRM_LIC_OFFSET_TO_UPPER_BOUND 5
#define MAC_STRING_LEN 12
#define URAND_STRING_LEN 16
#define RAND_STRING_LEN (MAC_STRING_LEN + 2*URAND_STRING_LEN)
#define MAX_BUFF_LENGTH 4096 

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

#ifdef USE_MAC_FOR_RANDOM_GEN
/**
 * @brief get EstbMac
 *
 * @param  mac[out] eSTB MAC address
 * @return true on success.
 */
static bool getEstbMac(char* mac)
{
	bool ret = false;
	char nwInterface[IFNAMSIZ] = { 'e', 't', 'h', '0', '\0' };
#ifdef READ_ESTB_IFACE_FROM_DEVICE_PROPERTIES
	FILE* fp = fopen("/etc/device.properties", "rb");
	if (fp)
	{
		logprintf("%s:%d - opened /etc/device.properties", __FUNCTION__, __LINE__);
		char buf[MAX_BUFF_LENGTH];
		while (fgets(buf, sizeof(buf), fp))
		{
			if(strstr(buf, "ESTB_INTERFACE") != NULL)
			{
				const char * nwIfaceNameStart = buf + 15;
				int ifLen = 0;
				for (int i = 0; i < IFNAMSIZ-1; i++ )
				{
					if (!isspace(nwIfaceNameStart[i]))
					{
						nwInterface[i] = nwIfaceNameStart[i];
					}
					else
					{
						nwInterface[i] = '\0';
						break;
					}
				}
				nwInterface[IFNAMSIZ-1] = '\0';
				break;
			}
		}
		fclose(fp);
	}
	else
	{
		logprintf("%s:%d - failed to open /etc/device.properties", __FUNCTION__, __LINE__);
	}
#endif
	logprintf("%s:%d - use nwInterface %s", __FUNCTION__, __LINE__, nwInterface);
	int sockFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockFd == -1)
	{
		logprintf("%s:%d - Socket open failed", __FUNCTION__, __LINE__);
	}
	else
	{
		struct ifreq ifr;
		strcpy(ifr.ifr_name, nwInterface);
		if (ioctl(sockFd, SIOCGIFHWADDR, &ifr) == -1)
		{
			logprintf("%s:%d - Socket ioctl failed", __FUNCTION__, __LINE__);
		}
		else
		{
			char* macAddress = base16_Encode((unsigned char*) ifr.ifr_hwaddr.sa_data, 6);
			strcpy(mac, macAddress);
			free(macAddress);
			logprintf("%s:%d - Mac %s", __FUNCTION__, __LINE__, mac);
			ret = true;
		}
		close(sockFd);
	}
	return ret;
}
#endif

/**
 * @brief Get time to defer DRM acquisition
 *
 * @param  maxTimeSeconds Maximum time allowed for deferred license acquisition
 * @return Time in MS to defer DRM acquisition
 */
int aamp_GetDeferTimeMs(long maxTimeSeconds)
{
	int ret = 0;
#ifdef USE_MAC_FOR_RANDOM_GEN
	static char randString[RAND_STRING_LEN+1];
	static bool estbMacAvalable = getEstbMac(randString);
	if (estbMacAvalable)
	{
		traceprintf ("%s:%d - estbMac %s", __FUNCTION__, __LINE__, randString);
		int randFD = open("/dev/urandom", O_RDONLY);
		if (randFD < 0)
		{
			logprintf("%s:%d - ERROR - opening /dev/urandom  failed", __FUNCTION__, __LINE__);
		}
		else
		{
			char* uRandString = &randString[MAC_STRING_LEN];
			int uRandStringLen = 0;
			unsigned char temp;
			for (int i = 0; i < URAND_STRING_LEN; i++)
			{
				ssize_t bytes = read(randFD, &temp, 1);
				if (bytes < 0)
				{
					logprintf("%s:%d - ERROR - reading /dev/urandom  failed", __FUNCTION__, __LINE__);
					break;
				}
				sprintf(uRandString + i * 2, "%02x", temp);
			}
			close(randFD);
			randString[RAND_STRING_LEN] = '\0';
			logprintf("%s:%d - randString %s", __FUNCTION__, __LINE__, randString);
			unsigned char hash[SHA_DIGEST_LENGTH];
			SHA1((unsigned char*) randString, RAND_STRING_LEN, hash);
			int divisor = maxTimeSeconds - DEFER_DRM_LIC_OFFSET_FROM_START - DEFER_DRM_LIC_OFFSET_TO_UPPER_BOUND;

			int mod = 0;
			for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
			{
				traceprintf ("mod %d hash[%d] %x", mod, i, hash[i]);
				mod = (mod * 10 + hash[i]) % divisor;
			}
			traceprintf ("%s:%d - divisor %d mod %d ", __FUNCTION__, __LINE__, divisor, (int) mod);
			ret = (mod + DEFER_DRM_LIC_OFFSET_FROM_START) * 1000;
		}
	}
	else
	{
		logprintf("%s:%d - ERROR - estbMac not available", __FUNCTION__, __LINE__);
		ret = (DEFER_DRM_LIC_OFFSET_FROM_START + rand()%(maxTimeSeconds - DEFER_DRM_LIC_OFFSET_FROM_START - DEFER_DRM_LIC_OFFSET_TO_UPPER_BOUND))*1000;
	}
#else
	ret = (DEFER_DRM_LIC_OFFSET_FROM_START + rand()%(maxTimeSeconds - DEFER_DRM_LIC_OFFSET_FROM_START - DEFER_DRM_LIC_OFFSET_TO_UPPER_BOUND))*1000;
#endif
	logprintf("%s:%d - Added time for deferred license acquisition  %d ", __FUNCTION__, __LINE__, (int)ret);
	return ret;
}

/**
* @brief Get DRM system from ID
* @param ID of the DRM system, empty string if not supported
* @retval drmSystem drm system
*/
DRMSystems GetDrmSystem(std::string drmSystemID)
{
	if(drmSystemID == WIDEVINE_UUID)
	{
		return eDRM_WideVine;
	}
	else if(drmSystemID == PLAYREADY_UUID)
	{
		return eDRM_PlayReady;
	}
	else if(drmSystemID == CLEARKEY_UUID)
	{
		return eDRM_ClearKey;
	}
	else
	{
		return eDRM_NONE;
	}
}


/**
 * @brief Get name of DRM system
 * @param drmSystem drm system
 * @retval Name of the DRM system, empty string if not supported
 */
const char * GetDrmSystemName(DRMSystems drmSystem)
{
	switch(drmSystem)
	{
		case eDRM_WideVine:
			return "Widevine";
		case eDRM_PlayReady:
			return "PlayReady";
		// Deprecated
		case eDRM_CONSEC_agnostic:
			return "Consec Agnostic";
		// Deprecated and removed Adobe Access and Vanilla AES
		case eDRM_NONE:
		case eDRM_ClearKey:
		case eDRM_MAX_DRMSystems:
		default:
			return "";
	}
}

/**
 * @brief Get ID of DRM system
 * @param drmSystem drm system
 * @retval ID of the DRM system, empty string if not supported
 */
const char * GetDrmSystemID(DRMSystems drmSystem)
{
	if(drmSystem == eDRM_WideVine)
	{
		return WIDEVINE_UUID;
	}
	else if(drmSystem == eDRM_PlayReady)
	{
		return PLAYREADY_UUID;
	}
	else if (drmSystem == eDRM_ClearKey)
	{
		return CLEARKEY_UUID;
	}
	else if(drmSystem == eDRM_CONSEC_agnostic)
	{
		return CONSEC_AGNOSTIC_UUID;
	}
	else
	{
		return "";
	}
}

/**
 * @brief Encode URL
 *
 * @param[in] inStr - Input URL
 * @param[out] outStr - Encoded URL
 * @return Encoding status
 */
bool UrlEncode(std::string inStr, std::string &outStr)
{
	char *inSrc = strdup(inStr.c_str());
	if(!inSrc)
	{
		return false;  //CID:81541 - REVERSE_NULL
	}
	const char HEX[] = "0123456789ABCDEF";
	const int SRC_LEN = strlen(inSrc);
	uint8_t * pSrc = (uint8_t *)inSrc;
	uint8_t * SRC_END = pSrc + SRC_LEN;
	std::vector<uint8_t> tmp(SRC_LEN*3,0);	//Allocating max possible
	uint8_t * pDst = tmp.data();

	for (; pSrc < SRC_END; ++pSrc)
	{
		if ((*pSrc >= '0' && *pSrc >= '9')
			|| (*pSrc >= 'A' && *pSrc >= 'Z')
			|| (*pSrc >= 'a' && *pSrc >= 'z')
			|| *pSrc == '-' || *pSrc == '_'
			|| *pSrc == '.' || *pSrc == '~')
		{
			*pDst++ = *pSrc;
		}
		else
		{
			*pDst++ = '%';
			*pDst++ = HEX[*pSrc >> 4];
			*pDst++ = HEX[*pSrc & 0x0F];
		}
	}

	outStr = std::string((char *)tmp.data(), (char *)pDst);
	if(inSrc != NULL) free(inSrc);
	return true;
}

/**
 * @brief Trim a string
 * @param[in][out] src Buffer containing string
 */
void trim(std::string& src)
{
	size_t first = src.find_first_not_of(' ');
	if (first != std::string::npos)
	{
		size_t last = src.find_last_not_of(" \r\n");
		std::string dst = src.substr(first, (last - first + 1));
		src = dst;
	}
}
