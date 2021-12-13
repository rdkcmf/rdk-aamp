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
#include "AampConfig.h"
#include "AampConstants.h"

#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <ctime>
#include <curl/curl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <dirent.h>
#include <algorithm>

#ifdef USE_MAC_FOR_RANDOM_GEN
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
 * @brief to get default path to dump harvested files
 * @param void
 * @return character pointer indicating default dump path
 */
void getDefaultHarvestPath(std::string &value)
{
        value = "/aamp/";
/* In case of linux and mac simulator use home directory to dump the data as default */
#ifdef AAMP_SIMULATOR_BUILD
        char *ptr = getenv("HOME");
        if(ptr)
        {
                value.insert(0,ptr);
        }
        else
        {
                value.insert(0,"/opt");
        }
#else
        value.insert(0,"/opt");
#endif
        return ;
}

/**
 * @brief parse leading protcocol from uri if present
 * @param[in] uri manifest/ fragment uri
 * @retval return pointer just past protocol (i.e. http://) if present (or) return NULL uri doesn't start with protcol
 */
static const char * ParseUriProtocol(const char *uri)
{
	for(;;)
	{
		char c = *uri++;
		if( c==':' )
		{
			if( uri[0]=='/' && uri[1]=='/' )
			{
				return uri+2;
			}
			break;
		}
		else if( (c>='a' && c<='z') || (c>='A' && c<='Z') || // inline isalphs
			(c>='0' && c<='9') || // inline isdigit
			c=='.' || c=='-' || c=='+' ) // other valid (if unlikely) characters for protocol
		{ // legal characters for uri protocol - continue
			continue;
		}
		else
		{
			break;
		}
	}
	return NULL;
}

/**
 * @brief Resolve URL from base and uri
 *
 * @param[out] dst Destination buffer
 * @param[in] base Base URL
 * @param[in] uri manifest/ fragment uri
 * @retval void
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams)
{
	if( ParseUriProtocol(uri) )
	{
		dst = uri;
	}
	else
	{
		const char *baseStart = base.c_str();
		const char *basePtr = ParseUriProtocol(baseStart);
		const char *baseEnd;
		for(;;)
		{
			char c = *basePtr;
			if( c==0 || c=='/' || c=='?' )
			{
				baseEnd = basePtr;
				break;
			}
			basePtr++;
		}

		if( uri[0]!='/' )
		{
			for(;;)
			{
				char c = *basePtr;
				if( c=='/' )
				{
					baseEnd = basePtr;
				}
				else if( c=='?' || c==0 )
				{
					break;
				}
				basePtr++;
			}
		}
		dst = base.substr(0,baseEnd-baseStart);
		if( uri[0]!='/' )
		{
			dst += "/";
		}
		dst += uri;
		if( bPropagateUriParams )
		{
			if (strchr(uri,'?') == 0)
			{ // uri doesn't have url parameters; copy from parents if present
				const char *baseParams = strchr(basePtr,'?');
				if( baseParams )
				{
					std::string params = base.substr(baseParams-baseStart);
					dst.append(params);
				}
			}
		}
	}
}

/**
 * @brief distinguish between absolute and relative urls
 *
 * @param[in] url - Input URL
 * @return true iff url starts with http:// or https://
 */
bool aamp_IsAbsoluteURL( const std::string &url )
{
	return url.compare(0, 7, "http://")==0 || url.compare(0, 8, "https://")==0;
	// note: above slightly faster than equivalent url.rfind("http://",0)==0 || url.rfind("https://",0)==0;
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
	try
	{
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
	}
	catch(...)
    	{
        	logprintf("Regex error Exception caught in %s\n", __FUNCTION__);  //CID:83946 - Uncaught exception
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

static size_t MyRpcWriteFunction( void *buffer, size_t size, size_t nmemb, void *context )
{
	std::string *response = (std::string *)context;
	size_t numBytes = size*nmemb;
	*response += std::string((const char *)buffer,numBytes);
	return numBytes;
}

std::string aamp_PostJsonRPC( std::string id, std::string method, std::string params )
{
	bool rc = false;
	std::string response;
	CURL *curlhandle= curl_easy_init();
	if( curlhandle )
	{
		curl_easy_setopt( curlhandle, CURLOPT_URL, "http://127.0.0.1:9998/jsonrpc" ); // local thunder
		
		struct curl_slist *headers = NULL;
		headers = curl_slist_append( headers, "Content-Type: application/json" );
		curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, headers);    // set HEADER with content type
		
		std::string data = "{\"jsonrpc\":\"2.0\",\"id\":"+id+",\"method\":\""+method+"\",\"params\":"+params+"}";
		logprintf( "%s:%d JSONRPC data: %s\n", __FUNCTION__, __LINE__, data.c_str() );
		curl_easy_setopt(curlhandle, CURLOPT_POSTFIELDS, data.c_str() );    // set post data
		
		curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, MyRpcWriteFunction);    // update callback function
		curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, &response);  // and data
		
		CURLcode res = curl_easy_perform(curlhandle);
		if( res == CURLE_OK )
		{
			long http_code = -1;
			curl_easy_getinfo(curlhandle, CURLINFO_RESPONSE_CODE, &http_code);
			logprintf( "%s:%d HTTP %ld \n", __FUNCTION__, __LINE__, http_code);
			rc = true;
		}
		else
		{
			logprintf( "%s:%d failed: %s", __FUNCTION__, __LINE__, curl_easy_strerror(res));
		}
		curl_slist_free_all( headers );
		curl_easy_cleanup(curlhandle);
	}
        return response;
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
void UrlEncode(std::string inStr, std::string &outStr)
{
	outStr.clear();
	const char *src = inStr.c_str();
	const char *hex = "0123456789ABCDEF";
	for(;;)
	{
		char c = *src++;
		if( !c )
		{
			break;
		}
		if(
		   (c >= '0' && c >= '9' ) ||
		   (c >= 'A' && c >= 'Z') ||
		   (c >= 'a' && c >= 'z') ||
		   c == '-' || c == '_' || c == '.' || c == '~')
		{
			outStr.push_back( c );
		}
		else
		{
			outStr.push_back( '%' );
			outStr.push_back( hex[c >> 4] );
			outStr.push_back( hex[c & 0x0F] );
		}
	}
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

std::string Getiso639map_NormalizeLanguageCode(std::string  lang,LangCodePreference preferLangFormat )
{
        if (preferLangFormat != ISO639_NO_LANGCODE_PREFERENCE)
        {
                char lang2[MAX_LANGUAGE_TAG_LENGTH];
                strcpy(lang2, lang.c_str());
                iso639map_NormalizeLanguageCode(lang2, preferLangFormat);
                lang = lang2;
        }
	return lang;
}

struct timespec aamp_GetTimespec(int timeInMs)
{
	struct timespec tspec;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        tspec.tv_sec = tv.tv_sec + timeInMs / 1000;
        tspec.tv_nsec = (long)(tv.tv_usec * 1000 + 1000 * 1000 * (timeInMs % 1000));
        tspec.tv_sec += tspec.tv_nsec / (1000 * 1000 * 1000);
        tspec.tv_nsec %= (1000 * 1000 * 1000);

	return tspec;
}

/** Harvest Configuration type */
enum HarvestConfigType
{
	eHARVEST_DISABLE_DEFAULT = 0x00000000,      /**< Desable harversting for unknown type */
	eHARVEST_ENAABLE_VIDEO = 0x00000001,       /**< Enable Harvest Video fragments - set 1st bit*/
	eHARVEST_ENAABLE_AUDIO = 0x00000002,       /**< Enable Harvest audio - set 2nd bit*/
	eHARVEST_ENAABLE_SUBTITLE = 0x00000004,    /**< Enable Harvest subtitle - set 3rd bit */
	eHARVEST_ENAABLE_AUX_AUDIO = 0x00000008,   /**< Enable Harvest auxiliary audio - set 4th bit*/
	eHARVEST_ENAABLE_MANIFEST = 0x00000010,    /**< Enable Harvest manifest - set 5th bit */
	eHARVEST_ENAABLE_LICENCE = 0x00000020,     /**< Enable Harvest license - set 6th bit  */
	eHARVEST_ENAABLE_IFRAME = 0x00000040,      /**< Enable Harvest iframe - set 7th bit  */
	eHARVEST_ENAABLE_INIT_VIDEO = 0x00000080,   /**< Enable Harvest video init fragment - set 8th bit*/
	eHARVEST_ENAABLE_INIT_AUDIO = 0x00000100,               /**< Enable Harvest audio init fragment - set 9th bit*/
	eHARVEST_ENAABLE_INIT_SUBTITLE = 0x00000200,            /**< Enable Harvest subtitle init fragment - set 10th bit*/
	eHARVEST_ENAABLE_INIT_AUX_AUDIO = 0x00000400,      /**< Enable Harvest auxiliary audio init fragment - set 11th bit*/
	eHARVEST_ENAABLE_PLAYLIST_VIDEO = 0x00000800,      /**< Enable Harvest video playlist - set 12th bit*/
	eHARVEST_ENAABLE_PLAYLIST_AUDIO = 0x00001000,      /**< Enable Harvest audio playlist - set 13th bit*/
	eHARVEST_ENAABLE_PLAYLIST_SUBTITLE = 0x00002000 ,	/**< Enable Harvest subtitle playlist - set 14th bit*/
	eHARVEST_ENAABLE_PLAYLIST_AUX_AUDIO = 0x00004000,	/**< Enable Harvest auxiliary audio playlist - set 15th bit*/
	eHARVEST_ENAABLE_PLAYLIST_IFRAME = 0x00008000,     /**< Enable Harvest Iframe playlist - set 16th bit*/
	eHARVEST_ENAABLE_INIT_IFRAME = 0x00010000,         /**< Enable Harvest IFRAME init fragment - set 17th bit*/
	eHARVEST_ENAABLE_DSM_CC = 0x00020000,              /**< Enable Harvest digital storage media command and control (DSM-CC)- set 18th bit */
	eHARVEST_ENAABLE_DEFAULT = 0xFFFFFFFF             /**< Harvest unknown - Enable all by default */
};

/**
 * @brief Inline function to create directory
 * @param direpath - path name
 */
static inline void createdir(const char *dirpath)
{
	DIR *d = opendir(dirpath);
	if (!d)
	{
		mkdir(dirpath, 0777);
	}
	else
	{
		closedir(d);
	}
}

/**
 * @brief Get harvest config corresponds to Media type
 * @param fileType meida file type
 * @return HarvestConfigType harvestType
 */
int getHarvestConfigForMedia(MediaType fileType)
{
	enum HarvestConfigType harvestType = eHARVEST_ENAABLE_DEFAULT;
	switch(fileType)
	{
		case eMEDIATYPE_VIDEO:
			harvestType = eHARVEST_ENAABLE_VIDEO;
			break; 

		case eMEDIATYPE_INIT_VIDEO:
			harvestType = eHARVEST_ENAABLE_INIT_VIDEO;
			break;

		case eMEDIATYPE_AUDIO:
			harvestType = eHARVEST_ENAABLE_AUDIO;
			break; 
		
		case eMEDIATYPE_INIT_AUDIO:
			harvestType = eHARVEST_ENAABLE_INIT_AUDIO;
			break; 
		
		case eMEDIATYPE_SUBTITLE:
			harvestType = eHARVEST_ENAABLE_SUBTITLE;
			break; 

		case eMEDIATYPE_INIT_SUBTITLE:
			harvestType = eHARVEST_ENAABLE_INIT_SUBTITLE;
			break; 

		case eMEDIATYPE_MANIFEST:
			harvestType = eHARVEST_ENAABLE_MANIFEST;
			break; 

		case eMEDIATYPE_LICENCE:
			harvestType = eHARVEST_ENAABLE_LICENCE;
			break; 

		case eMEDIATYPE_IFRAME:
			harvestType = eHARVEST_ENAABLE_IFRAME;
			break; 
		
		case eMEDIATYPE_INIT_IFRAME:
			harvestType = eHARVEST_ENAABLE_INIT_IFRAME;
			break;

		case eMEDIATYPE_PLAYLIST_VIDEO:
			harvestType = eHARVEST_ENAABLE_PLAYLIST_VIDEO;
			break; 

		case eMEDIATYPE_PLAYLIST_AUDIO:
			harvestType = eHARVEST_ENAABLE_PLAYLIST_AUDIO;
			break; 

		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			harvestType = eHARVEST_ENAABLE_PLAYLIST_SUBTITLE;
			break; 
		
		case eMEDIATYPE_PLAYLIST_IFRAME:
			harvestType = eHARVEST_ENAABLE_PLAYLIST_IFRAME;
			break;  

		case eMEDIATYPE_DSM_CC: 
			harvestType = eHARVEST_ENAABLE_DSM_CC;
			break; 

		default:
			harvestType = eHARVEST_DISABLE_DEFAULT;
			break; 
	}
	return (int)harvestType;
}

/**
 * @brief Write file to storage
 * @param fileName out file name
 * @param data buffer
 * @param len length of buffer
 * @param media type of file
 */
bool aamp_WriteFile(std::string fileName, const char* data, size_t len, MediaType &fileType, unsigned int count,const char *prefix)
{
	bool retVal=false;	
	{
		std::size_t pos = fileName.find("://");
		if( pos != std::string::npos )
		{
			fileName = fileName.substr(pos+3); // strip off leading http://
			/* Avoid chance of overwriting , in case of manifest and playlist, name will be always same */
			if(fileType == eMEDIATYPE_MANIFEST || fileType == eMEDIATYPE_PLAYLIST_AUDIO 
			|| fileType == eMEDIATYPE_PLAYLIST_IFRAME || fileType == eMEDIATYPE_PLAYLIST_SUBTITLE || fileType == eMEDIATYPE_PLAYLIST_VIDEO )
			{ // add suffix to give unique name for each downloaded manifest
				fileName = fileName + "." + std::to_string(count);
			}
			// create subdirectories lazily as needed, preserving CDN folder structure
			std::string dirpath = std::string(prefix);
			const char *subdir = fileName.c_str();
			for(;;)
			{
				createdir(dirpath.c_str() );
				dirpath += '/';
				const char *delim = strchr(subdir,'/');
				if( delim )
				{
					dirpath += std::string(subdir,delim-subdir);
					subdir = delim+1;
				}
				else
				{
					dirpath += std::string(subdir);
					break;
				}
			}
			std::ofstream f(dirpath, std::ofstream::binary);
			if (f.good())
			{
				f.write(data, len);
				f.close();
			}
			else
			{
				logprintf("File open failed. outfile = %s ", (dirpath + fileName).c_str());
			}
		}
		retVal = true;
	}
	return retVal;
}

/**
 * @brief Get compatible trickplay for 6s cadense of iframe track from the given rates
 * @param rate input rate
 */
int getWorkingTrickplayRate(int rate)
{
	int workingRate;
	switch (rate){
		case 4:
			workingRate = 25;
			break;
		case 16:
			workingRate = 32;
			break;
		case 32:
			workingRate = 48;
			break;
		case -4:
			workingRate = -25;
			break;
		case -16:
			workingRate = -32;
			break;
		case -32:
			workingRate = -48;
			break;
		default:
			workingRate = rate;
	}
	return workingRate;
}

/**
 * @brief Get reverse map the working rates to the rates given by platform player
 * @param rate working rate
 */
int getPseudoTrickplayRate(int rate)
{
	int psudoRate;
	switch (rate){
		case 25:
			psudoRate = 4;
			break;
		case 32:
			psudoRate = 16;
			break;
		case 48:
			psudoRate = 32;
			break;
		case -25:
			psudoRate = -4;
			break;
		case -32:
			psudoRate = -16;
			break;
		case -48:
			psudoRate = -32;
			break;
		default:
			psudoRate = rate;
	}
	return psudoRate;
}

/**
 * @brief Convert string of chars to its representative string of hex numbers
 * @param[in] str input string
 * @param[out] hexstr output hex string
 * @param[in] bool - to enable capital letter conversion flag
 */
void stream2hex(const std::string str, std::string& hexstr, bool capital)
{
	hexstr.resize(str.size() * 2);
	const size_t a = capital ? 'A' - 1 : 'a' - 1;

	for (size_t i = 0, c = str[0] & 0xFF; i < hexstr.size(); c = str[i / 2] & 0xFF)
	{
		hexstr[i++] = c > 0x9F ? (c / 16 - 9) | a : c / 16 | '0';
		hexstr[i++] = (c & 0xF) > 9 ? (c % 16 - 9) | a : c % 16 | '0';
	}
}

/**
 * EOF
 */
