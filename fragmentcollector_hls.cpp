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
* \file fragmentcollector_hls.cpp
*
* Fragment Collect file includes class implementation for handling
* HLS streaming of AAMP player.
* Various functionality like manifest download , fragment collection,
* DRM initialization , synchronizing audio / video media of the stream,
* trick play handling etc are handled in this file .
*
*/
#include "iso639map.h"
#include "fragmentcollector_hls.h"
#include "_base64.h"
#include "base16.h"
#include <algorithm> // for std::min
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "priv_aamp.h"
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <openssl/sha.h>
#include <set>
#include <math.h>
#include <vector>
#include <string>
#include "HlsDrmBase.h"
#include "AampCacheHandler.h"
#ifdef USE_OPENCDM
#include "AampHlsDrmSessionManager.h"
#endif
#ifdef AAMP_VANILLA_AES_SUPPORT
#include "aamp_aes.h"
#endif
#include "webvttParser.h"
#include "SubtecFactory.hpp"
#include "tsprocessor.h"
#include "isobmffprocessor.h"
#include "AampUtils.h"

#ifdef AAMP_HLS_DRM
#include "AampDRMSessionManager.h"
#endif
#include "AampAveDrmHelper.h"
#include "AampVanillaDrmHelper.h"

#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif

//#define TRACE // compile-time optional noisy debug output

static const int DEFAULT_STREAM_WIDTH = 720;
static const int DEFAULT_STREAM_HEIGHT = 576;
static const double DEFAULT_STREAM_FRAMERATE = 25.0;

// checks if current state is going to use IFRAME ( Fragment/Playlist )
#define IS_FOR_IFRAME(rate, type) ((type == eTRACK_VIDEO) && (rate != AAMP_NORMAL_PLAY_RATE))

#ifdef AAMP_HLS_DRM
extern DrmSessionDataInfo* ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName);
extern int SpawnDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp, DrmSessionDataInfo* drmData);
extern void ReleaseDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp);
#endif 

#define UseProgramDateTimeIfAvailable() (ISCONFIGSET(eAAMPConfig_HLSAVTrackSyncUsingStartTime) || aamp->mIsVSS)
/**
* \struct	FormatMap
* \brief	FormatMap structure for stream codec/format information
*/
struct FormatMap
{
	const char* codec;
	StreamOutputFormat format;
};

/// Variable initialization for various audio formats
static const FormatMap mAudioFormatMap[] =
{
	{ "mp4a.40.2", FORMAT_AUDIO_ES_AAC },
	{ "mp4a.40.5", FORMAT_AUDIO_ES_AAC },
	{ "ac-3", FORMAT_AUDIO_ES_AC3 },
	{ "mp4a.a5", FORMAT_AUDIO_ES_AC3 },
	{ "ec-3", FORMAT_AUDIO_ES_EC3 },
	{ "ec+3", FORMAT_AUDIO_ES_ATMOS },
	{ "eac3", FORMAT_AUDIO_ES_EC3 }
};
#define AAMP_AUDIO_FORMAT_MAP_LEN ARRAY_SIZE(mAudioFormatMap)

/// Variable initialization for various video formats 
static const FormatMap * GetAudioFormatForCodec( const char *codecs )
{
	if( codecs )
	{
		for( int i=0; i<AAMP_AUDIO_FORMAT_MAP_LEN; i++ )
		{
			if( strstr( codecs, mAudioFormatMap[i].codec) )
			{
				return &mAudioFormatMap[i];
			}
		}
	}
	return NULL;
}

/***************************************************************************
* @fn GetAudioFormatStringForCodec
* @brief Function to get audio codec string from the map.
*
* @param[in] input Audio codec type
* @return Audio codec string
***************************************************************************/
static const char * GetAudioFormatStringForCodec ( StreamOutputFormat input)
{
	const char *codec = "UNKNOWN";
	if(input < FORMAT_UNKNOWN)
	{
		for( int i=0; i<AAMP_AUDIO_FORMAT_MAP_LEN; i++ )
		{
			if(mAudioFormatMap[i].format == input )
			{
				codec =  mAudioFormatMap[i].codec;
				break;
			}
		}
	}
	return codec;
}


/// Variable initialization for various video formats
static const FormatMap mVideoFormatMap[] =
{
	{ "avc1.", FORMAT_VIDEO_ES_H264 },
	{ "hvc1.", FORMAT_VIDEO_ES_HEVC },
	{ "hev1.", FORMAT_VIDEO_ES_HEVC },
	{ "mpeg2v", FORMAT_VIDEO_ES_MPEG2 }//For testing.
};
#define AAMP_VIDEO_FORMAT_MAP_LEN ARRAY_SIZE(mVideoFormatMap)

static const FormatMap * GetVideoFormatForCodec( const char *codecs )
{
	if( codecs )
	{
		for( int i=0; i<AAMP_VIDEO_FORMAT_MAP_LEN; i++ )
		{
			if( strstr( codecs, mVideoFormatMap[i].codec) )
			{
				return &mVideoFormatMap[i];
			}
		}
	}
	return NULL;
}

/// Variable initialization for media profiler buckets
static const ProfilerBucketType mediaTrackBucketTypes[AAMP_TRACK_COUNT] =
	{ PROFILE_BUCKET_FRAGMENT_VIDEO, PROFILE_BUCKET_FRAGMENT_AUDIO, PROFILE_BUCKET_FRAGMENT_SUBTITLE, PROFILE_BUCKET_FRAGMENT_AUXILIARY };
/// Variable initialization for media decrypt buckets
static const ProfilerBucketType mediaTrackDecryptBucketTypes[AAMP_DRM_CURL_COUNT] =
	{ PROFILE_BUCKET_DECRYPT_VIDEO, PROFILE_BUCKET_DECRYPT_AUDIO, PROFILE_BUCKET_DECRYPT_SUBTITLE, PROFILE_BUCKET_DECRYPT_AUXILIARY};

#ifdef AVE_DRM
extern "C"
{
//setCustomLicensePayload is new extension to AVE's DRM library, required to be populated with Virtual Stream Stitcher (VSS) content
extern void setCustomLicensePayLoad(const char* customData);
}
#endif
static size_t FindLineLength(const char* ptr);

/***************************************************************************
* @fn startswith
* @brief Function to check if string is start of main string
*
* @param pstring[in] Main string
* @param prefix[in] Sub string to check
* @return bool true/false
***************************************************************************/
static bool startswith(const char **pstring, const char *prefix)
{
	const char *string = *pstring;
	for (;;)
	{
		char c = *prefix++;
		if (c == 0)
		{
			*pstring = string;
			return true;
		}
		if (*string++ != c)
		{
			return false;
		}
	}
}

/***************************************************************************
* @fn startswith
* @brief Function to check if string is start of main string
*
* @param pstring[in] Main string
* @param prefix[in] Sub string to check
* @return bool true/false
***************************************************************************/
static bool startswith(char **pstring, const char *prefix)
{
        char *string = *pstring;
        for (;;)
        {
                char c = *prefix++;
                if (c == 0)
                {
                        *pstring = string;
                        return true;
                }
                if (*string++ != c)
                {
                        return false;
                }
        }
}



/***************************************************************************
* @fn AttributeNameMatch
* @brief Function to check if attribute name matches
*
* @param attrNamePtr[in] Attribute Name pointer
* @param targetAttrNameCString[in] string to compare
* @return bool true/false
***************************************************************************/
static bool AttributeNameMatch(const char *attrNamePtr, const char *targetAttrNameCString)
{
	while (*targetAttrNameCString)
	{
		if (*targetAttrNameCString++ != *attrNamePtr++)
		{
			return false;
		}
	}
	return *attrNamePtr == '=';
}

/***************************************************************************
* @fn SubStringMatch
* @brief Function to check if substring present in main string
*
* @param srcStart[in] Start of string pointer
* @param srcFin[in] End of string pointer (length = srcFin - srcStart)
* @param cstring[in] string to compare
* @return bool true/false
***************************************************************************/
static bool SubStringMatch(const char *srcStart, const char *srcFin, const char *cstring)
{
	while (srcStart <= srcFin)
	{
		char c = *cstring++;
		if (c == 0)
		{
			return true;
		}
		if (*srcStart != c)
		{
			break;
		}
		srcStart++;
	}
	return false;
}

/***************************************************************************
 * @fn GetAttributeValueString
 * @brief convert quoted string to NUL-terminated C-string, stripping quotes
 *
 * @param valuePtr[in] quoted string, or string starting with NONE - not nul-terminated; note that valuePtr may be modified in place
 * @param fin[in] pointer to first character past end of string
 * @return char * read-only NUL-terminated string; will normally point to memory within valuePtr
 */
static const char * GetAttributeValueString(char *valuePtr, char *fin)
{
	const char *rc = NULL;
	if (*valuePtr == '\"')
	{
		valuePtr++; // skip begin-quote
		rc = valuePtr;
		fin--;
		if( *fin!='\"' )
		{
			AAMPLOG_WARN( "quoted-string missing end-quote:%s",valuePtr );
		}
		*fin = 0x00; // replace end-quote with NUL-terminator
	}
	else
	{ // per specification, some attributes (CLOSED-CAPTIONS) may be quoted-string or NONE
		rc = "NONE";
		if( memcmp(valuePtr,"NONE",4) !=0 )
		{
			AAMPLOG_WARN("GetAttributeValueString input neither quoted string nor NONE");
		}
	}
	return rc;
}

/***************************************************************************
* @fn ParseKeyAttributeCallback
* @brief Callback function to decode Key and attribute
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] TrackState pointer for storage
* @return void
***************************************************************************/
static void ParseKeyAttributeCallback(char *attrName, char *delimEqual, char *fin, void* arg)
{
	TrackState *ts = (TrackState *)arg;
	char *valuePtr = delimEqual + 1;
	if (AttributeNameMatch(attrName, "METHOD"))
	{
		if (SubStringMatch(valuePtr, fin, "NONE"))
		{ // used by DAI
			if(ts->fragmentEncrypted)
			{
				if (!ts->mIndexingInProgress)
				{
					AAMPLOG_WARN("Track %s encrypted to clear ", ts->name);
				}
				ts->fragmentEncrypted = false;
				ts->UpdateDrmCMSha1Hash(NULL);
			}
			ts->mDrmMethod = eDRM_KEY_METHOD_NONE;
		}
		else if (SubStringMatch(valuePtr, fin, "AES-128"))
		{
			if(!ts->fragmentEncrypted)
			{
				if (!ts->mIndexingInProgress)
				{
					AAMPLOG_WARN("Track %s clear to encrypted ", ts->name);
				}
				ts->fragmentEncrypted = true;
			}
			ts->mDrmInfo.method = eMETHOD_AES_128;
			ts->mDrmMethod = eDRM_KEY_METHOD_AES_128;
			ts->mKeyTagChanged = true;
		}
		else if (SubStringMatch(valuePtr, fin, "SAMPLE-AES-CTR"))
		{

            if(!ts->fragmentEncrypted)
			{
				if (!ts->mIndexingInProgress)
				{
					AAMPLOG_WARN("Track %s clear to encrypted", ts->name);
				}
				ts->fragmentEncrypted = true;
			}
			ts->mDrmMethod = eDRM_KEY_METHOD_SAMPLE_AES_CTR;
		}
		else if (SubStringMatch(valuePtr, fin, "SAMPLE-AES"))
		{
			ts->mDrmMethod = eDRM_KEY_METHOD_SAMPLE_AES;
			AAMPLOG_ERR("SAMPLE-AES unsupported");
		}
		else
		{
			ts->mDrmMethod = eDRM_KEY_METHOD_UNKNOWN;
			AAMPLOG_ERR("unsupported METHOD");
		}
	}
	else if (AttributeNameMatch(attrName, "KEYFORMAT"))
	{
		const char *keyFormat = GetAttributeValueString(valuePtr, fin);
		ts->mDrmInfo.keyFormat = keyFormat;

		if (startswith(&keyFormat, "urn:uuid:"))
		{
			ts->mDrmInfo.systemUUID = keyFormat;
		}
	}
	else if (AttributeNameMatch(attrName, "URI"))
	{
		const char *uri = GetAttributeValueString(valuePtr, fin);
		ts->mDrmInfo.keyURI = uri;
	}
	else if (AttributeNameMatch(attrName, "IV"))
	{ // 16 bytes
		const char *srcPtr = valuePtr;
		assert(srcPtr[0] == '0');
		assert((srcPtr[1] == 'x') || (srcPtr[1] == 'X'));
		srcPtr += 2;
		ts->UpdateDrmIV(srcPtr);
		ts->mDrmInfo.bUseMediaSequenceIV = false;
	}
	else if (AttributeNameMatch(attrName, "CMSha1Hash"))
	{ // 20 bytes; Metadata Hash.
		assert(valuePtr[0] == '0');
		assert((valuePtr[1] == 'x') || (valuePtr[1] == 'X'));
		ts->UpdateDrmCMSha1Hash(valuePtr+2);
	}
}

/***************************************************************************
* @fn ParseTileInfCallback
* @brief Callback function to decode Key and attribute
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] Updated TileInfo structure instance
* @return void
***************************************************************************/
static void ParseTileInfCallback(char *attrName, char *delimEqual, char *fin, void* arg)
{
	// #EXT-X-TILES:RESOLUTION=416x234,LAYOUT=9x17,DURATION=2.002
	TileInfo *var = (TileInfo *)arg;
	const char *valuePtr = delimEqual + 1;
	if (AttributeNameMatch(attrName, "LAYOUT"))
	{
		sscanf(valuePtr, "%dx%d", &var->numCols, &var->numRows);
		traceprintf("In rows:%d cols:%d",var->numRows, var->numCols);
	}
	else if (AttributeNameMatch(attrName, "DURATION"))
	{
		var->posterDuration = atof(valuePtr);
	    	traceprintf("In duration:%f",var->duration);
	}
}

/***************************************************************************
* @fn ParseXStartAttributeCallback
* @brief Callback function to decode XStart attributes
*		 
* @param attrName[in] input string	
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] TrackState pointer for storage
* @return void
***************************************************************************/
static void ParseXStartAttributeCallback(char *attrName, char *delimEqual, char *fin, void* arg)
{
	HLSXStart *var = (HLSXStart *)arg;
	char *valuePtr = delimEqual + 1;
	if (AttributeNameMatch(attrName, "TIME-OFFSET"))
	{
		var->offset = atof(valuePtr);
	}
	else if (AttributeNameMatch(attrName, "PRECISE"))
	{
		// Precise attribute is not considered . By default NO option is selected 	
		var->precise = false;
	}
}

/***************************************************************************
* @fn ParseStreamInfCallback
* @brief Callback function to extract stream tag attributes
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] StreamAbstractionAAMP_HLS pointer for storage
* @return void
***************************************************************************/
static void ParseStreamInfCallback(char *attrName, char *delimEqual, char *fin, void* arg)
{
	StreamAbstractionAAMP_HLS *context = (StreamAbstractionAAMP_HLS *) arg;
	char *valuePtr = delimEqual + 1;
	HlsStreamInfo *streamInfo = &context->streamInfo[context->GetTotalProfileCount()];
	if (AttributeNameMatch(attrName, "URI"))
	{
		streamInfo->uri = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "BANDWIDTH"))
	{
		streamInfo->bandwidthBitsPerSecond = atol(valuePtr);
	}
	else if (AttributeNameMatch(attrName, "PROGRAM-ID"))
	{
		streamInfo->program_id = atol(valuePtr);
	}
	else if (AttributeNameMatch(attrName, "AUDIO"))
	{
		streamInfo->audio = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "CODECS"))
	{
		streamInfo->codecs = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "RESOLUTION"))
	{
		sscanf(delimEqual + 1, "%dx%d", &streamInfo->resolution.width, &streamInfo->resolution.height);
	}
	// following are rarely present
	else if (AttributeNameMatch(attrName, "AVERAGE-BANDWIDTH"))
	{
		streamInfo->averageBandwidth = atol(valuePtr);
	}
	else if (AttributeNameMatch(attrName, "FRAME-RATE"))
	{
		streamInfo->resolution.framerate = atof(valuePtr);
	}
	else if (AttributeNameMatch(attrName, "CLOSED-CAPTIONS"))
	{
		streamInfo->closedCaptions = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "SUBTITLES"))
	{
		streamInfo->subtitles = GetAttributeValueString(valuePtr, fin);
	}
	else
	{
		AAMPLOG_INFO("unknown stream inf attribute %s", attrName);
	}
}

/***************************************************************************
* @fn ParseMediaAttributeCallback
* @brief Callback function to extract media tag attributes
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] StreamAbstractionAAMP_HLS pointer for storage
* @return void
***************************************************************************/
static void ParseMediaAttributeCallback(char *attrName, char *delimEqual, char *fin, void *arg)
{
	StreamAbstractionAAMP_HLS *context = (StreamAbstractionAAMP_HLS *) arg;
	char *valuePtr = delimEqual + 1;
	struct MediaInfo *mediaInfo = &context->mediaInfo[context->GetMediaCount()];
	/*
	#EXT - X - MEDIA:TYPE = AUDIO, GROUP - ID = "g117600", NAME = "English", LANGUAGE = "en", DEFAULT = YES, AUTOSELECT = YES
	#EXT - X - MEDIA:TYPE = AUDIO, GROUP - ID = "g117600", NAME = "Spanish", LANGUAGE = "es", URI = "HBOHD_HD_NAT_15152_0_5939026565177792163/format-hls-track-sap-bandwidth-117600-repid-root_audio103.m3u8"
	*/
	if (AttributeNameMatch(attrName, "TYPE"))
	{
		if (SubStringMatch(valuePtr, fin, "AUDIO"))
		{
			mediaInfo->type = eMEDIATYPE_AUDIO;
		}
		else if (SubStringMatch(valuePtr, fin, "VIDEO"))
		{
			mediaInfo->type = eMEDIATYPE_VIDEO;
		}
		else if (SubStringMatch(valuePtr, fin, "SUBTITLES"))
		{
			mediaInfo->type = eMEDIATYPE_SUBTITLE;
		}
		else if (SubStringMatch(valuePtr, fin, "CLOSED-CAPTIONS"))
		{
			mediaInfo->type = eMEDIATYPE_SUBTITLE;
			mediaInfo->isCC = true;
		}
	}
	else if (AttributeNameMatch(attrName, "GROUP-ID"))
	{
		mediaInfo->group_id = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "NAME"))
	{
		mediaInfo->name = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "LANGUAGE"))
	{
		mediaInfo->language = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "AUTOSELECT"))
	{
		if (SubStringMatch(valuePtr, fin, "YES"))
		{
			mediaInfo->autoselect = true;
		}
	}
	else if (AttributeNameMatch(attrName, "DEFAULT"))
	{
		if (SubStringMatch(valuePtr, fin, "YES"))
		{
			mediaInfo->isDefault = true;
		}
	}
	else if (AttributeNameMatch(attrName, "URI"))
	{
		mediaInfo->uri = GetAttributeValueString(valuePtr, fin);
	}
	// rarely present
	else if (AttributeNameMatch(attrName, "CHANNELS"))
	{
		mediaInfo->channels = atoi(valuePtr);
	}
	else if (AttributeNameMatch(attrName, "INSTREAM-ID"))
	{
		mediaInfo->instreamID = GetAttributeValueString(valuePtr, fin);
	}
	else if (AttributeNameMatch(attrName, "FORCED"))
	{
		if (SubStringMatch(valuePtr, fin, "YES"))
		{
			mediaInfo->forced = true;
		}
	}
	else if (AttributeNameMatch(attrName, "CHARACTERISTICS"))
	{
		mediaInfo->characteristics = GetAttributeValueString(valuePtr, fin);
	}
	else
	{
		AAMPLOG_WARN("unk MEDIA attr %s", attrName);
	}
}

/***************************************************************************
* @fn mystrpbrk
* @brief Function to extract string pointer afer LF
*
* @param ptr[in] input string
* @return char * - Next string pointer
***************************************************************************/
static char *mystrpbrk(char *ptr)
{ // lines are terminated by either a single LF character or a CR character followed by an LF character
	char *next = NULL;
	char *fin = strchr(ptr, CHAR_LF);
	if (fin)
	{
		next = fin + 1;
		//handles lines terminated by CR characters followed by LF character
		while(fin > ptr && fin[-1] == CHAR_CR)
		{
			fin--;
		}
		*fin = 0x00;
	}
	return next;
}

/**
* @param attrName pointer to HLS Attribute List, as a NUL-terminated CString
*/
/***************************************************************************
* @fn ParseAttrList
* @brief Function to parse attribute list from tag string
*
* @param attrName[in] input string
* @param cb[in] callback function to store parsed attributes
* @param context[in] void pointer context
* @return void
***************************************************************************/
static void ParseAttrList(char *attrName, void(*cb)(char *attrName, char *delim, char *fin, void *context), void *context)
{
	char *origParseStr = attrName;
	while (*attrName)
	{
		while (*attrName == ' ')
		{ // strip leading whitespace, if any
			attrName++;
		}
		char *delimEqual = attrName;
		if(*delimEqual == '\r')
		{	// break out on CR
			break;
		}
		while (*delimEqual != '=')
		{ // An AttributeName is an unquoted string containing characters from the set [A..Z], [0..9] and '-'
			char c = *delimEqual++;
			if(c < ' ')
			{
				// if 0x00, newline, or other unprintable characters, bail out
				AAMPLOG_WARN("Missing equal delimiter %s",origParseStr);
				return;
			}
		}
		char *fin = delimEqual;
		bool inQuote = false;
		for (;;)
		{
			char c = *fin;
			if (c == 0)
			{ // end of input
				break;
			}
			else if (c == '\"')
			{
				if (inQuote)
				{ 	// trailing quote
					inQuote = false;
					fin++;
					break;
				}
				else
				{	// leading quote
					inQuote = true;
				}
			}
			else if (c == ',' && !inQuote)
			{	// next attribute/value pair
				break;
			}
			fin++;
		}
		cb(attrName, delimEqual, fin, context);
		if (*fin == ',')
		{
			fin++;
		}
		attrName = fin;
	}
}


/***************************************************************************
* @fn ParseXStartTimeOffset
* @brief Helper function to Parse XStart Tag and attributes
*
* @param arg[in] char *ptr , string with X-START
* @return double offset value
***************************************************************************/
static double ParseXStartTimeOffset(const char* ptr)
{
	double retOffSet = 0.0;
	double offset = 0.0;  //CID:119528 - Intialization
	if(ptr)
	{
		size_t len = FindLineLength(ptr);
		char* xstartStr =(char*) malloc (len+1);
		if(xstartStr)
		{
			memcpy(xstartStr,ptr,len);
			xstartStr[len]='\0';

			HLSXStart xstart;
			ParseAttrList(xstartStr, ParseXStartAttributeCallback, &xstart);
			free(xstartStr);
			retOffSet = xstart.offset;
		}
	}
	return retOffSet;
}
/***************************************************************************
* @fn TrackPLDownloader
* @brief Thread function for download
*
* @param arg[in] void ptr , thread arguement
* @return void ptr
***************************************************************************/
static void * TrackPLDownloader(void *arg)
{
	TrackState* ts = (TrackState*)arg;
	if(aamp_pthread_setname(pthread_self(), "aampHLSAudPLDL"))
	{
		AAMPLOG_WARN("aamp_pthread_setname failed");
	}
	ts->FetchPlaylist();
	return NULL;
}

/***************************************************************************
* @fn InitiateDrmProcess
* @brief Function to initiate drm process
*
* @param aamp[in] PrivateInstanceAAMP pointer
*
* @return None
***************************************************************************/
static void InitiateDrmProcess(PrivateInstanceAAMP* aamp){
#ifdef AAMP_HLS_DRM
		/** If fragments are CDM encrypted KC **/
		if (aamp->fragmentCdmEncrypted && ISCONFIGSET(eAAMPConfig_Fragmp4PrefetchLicense)){
			pthread_mutex_lock(&aamp->drmParserMutex);
			DrmSessionDataInfo* drmDataToUse = NULL;
			for (int i=0; i < aamp->aesCtrAttrDataList.size(); i++ ){
				if (!aamp->aesCtrAttrDataList.at(i).isProcessed){
					aamp->aesCtrAttrDataList.at(i).isProcessed = true;
					DrmSessionDataInfo* drmData = ProcessContentProtection(aamp, aamp->aesCtrAttrDataList.at(i).attrName);
					if (NULL != drmData){
/* This needs effort from MSO as to what they want to do viz-a-viz preferred DRM, */						
	               					if (NULL != drmDataToUse) {
		                                        free(drmDataToUse);//CID:178316 RESOURCE_LEAK
                                                   	}
							drmDataToUse = drmData;
						}
					}
				}
			if (drmDataToUse != nullptr) 
			{
				SpawnDRMLicenseAcquireThread(aamp, drmDataToUse);
			}
			pthread_mutex_unlock(&aamp->drmParserMutex);
			if (NULL != drmDataToUse) {
			    free(drmDataToUse);
			}
		}
#endif
}

void static setupStreamInfo(struct HlsStreamInfo * streamInfo, int streamNo)
{
	memset(streamInfo, 0, sizeof(HlsStreamInfo));	
	streamInfo->resolution.width = DEFAULT_STREAM_WIDTH;
	streamInfo->resolution.height = DEFAULT_STREAM_HEIGHT;
	streamInfo->resolution.framerate = DEFAULT_STREAM_FRAMERATE;
}

/***************************************************************************
* @fn ParseMainManifest
* @brief Function to parse main manifest
*
* @param ptr[in] Manifest file content string
*
* @return AAMPStatusType
***************************************************************************/
AAMPStatusType StreamAbstractionAAMP_HLS::ParseMainManifest()
{
	int vProfileCount, iFrameCount , lineNum;
	AAMPStatusType retval = eAAMPSTATUS_OK;
	char* ptr = mainManifest.ptr;
	mMediaCount = 0;
	mProfileCount = 0;
	vProfileCount = iFrameCount = lineNum = 0;
	mAbrManager.clearProfiles();
	bool useavgbw = ISCONFIGSET(eAAMPConfig_AvgBWForABR);
#ifdef AVE_DRM
	//clear previouse data
	setCustomLicensePayLoad(NULL);
#endif
	while (ptr)
	{
		char *next = mystrpbrk(ptr);
		if (*ptr)
		{
			if (startswith(&ptr, "#EXT"))
			{
				if (startswith(&ptr, "-X-I-FRAME-STREAM-INF:"))
				{
					HlsStreamInfo *streamInfo = &this->streamInfo[mProfileCount];
					memset(streamInfo, 0, sizeof(*streamInfo));
					ParseAttrList(ptr, ParseStreamInfCallback, this);
					if (streamInfo->uri == NULL)
					{ // uri on following line
						streamInfo->uri = next;
						next = mystrpbrk(next);
					}

					if(streamInfo->averageBandwidth !=0 && useavgbw)
					{
						streamInfo->bandwidthBitsPerSecond = streamInfo->averageBandwidth;
					}

					streamInfo->isIframeTrack = true;
					streamInfo->enabled = false;
					iFrameCount++;
					mProfileCount++;
					mIframeAvailable = true;
				}
				else if (startswith(&ptr, "-X-IMAGE-STREAM-INF:"))
				{
					HlsStreamInfo *streamInfo = &this->streamInfo[mProfileCount];
					memset(streamInfo, 0, sizeof(*streamInfo));
					ParseAttrList(ptr, ParseStreamInfCallback, this);
					if (streamInfo->uri == NULL)
					{ // uri on following line
						streamInfo->uri = next;
						next = mystrpbrk(next);
					}

					if(streamInfo->averageBandwidth !=0 && useavgbw)
					{
						streamInfo->bandwidthBitsPerSecond = streamInfo->averageBandwidth;
					}

					streamInfo->isIframeTrack = true;
					streamInfo->enabled = false;
					mProfileCount++;
				}
				else if (startswith(&ptr, "-X-STREAM-INF:"))
				{
					struct HlsStreamInfo *streamInfo = &this->streamInfo[mProfileCount];
					setupStreamInfo(streamInfo, mProfileCount);
					ParseAttrList(ptr, ParseStreamInfCallback, this);
					if (streamInfo->uri == NULL)
					{ // uri on following line
						streamInfo->uri = next;
						next = mystrpbrk(next);
					}
					if(streamInfo->averageBandwidth!=0 && useavgbw)
					{
						streamInfo->bandwidthBitsPerSecond = streamInfo->averageBandwidth;
					}
					const FormatMap *map = GetAudioFormatForCodec(streamInfo->codecs);
					if( map )
					{
						streamInfo->audioFormat = map->format;
					}
					else
					{
						streamInfo->audioFormat = FORMAT_UNKNOWN;
					}
					streamInfo->enabled = false;
					mProfileCount++;
					vProfileCount++;
				}
				else if (startswith(&ptr, "-X-MEDIA:"))
				{
					memset(&this->mediaInfo[mMediaCount], 0, sizeof(MediaInfo));
					ParseAttrList(ptr, ParseMediaAttributeCallback, this);
					if(!mediaInfo[mMediaCount].language)
					{ // handle non-compliant manifest missing language attribute
						mediaInfo[mMediaCount].language =  mediaInfo[mMediaCount].name;
					}
					if (mediaInfo[mMediaCount].type == eMEDIATYPE_AUDIO && mediaInfo[mMediaCount].language)
					{	
						mLangList.insert(GetLanguageCode(mMediaCount));
					}
					mMediaCount++;
				}
				else if (startswith(&ptr, "-X-VERSION:"))
				{
					// followed by integer
				}
				else if (startswith(&ptr, "-X-INDEPENDENT-SEGMENTS"))
				{
					// followed by integer
				}
				else if (startswith(&ptr, "-X-FAXS-CM"))
				{ // not needed - present in playlist
					hasDrm = true;
					AveDrmManager::ApplySessionToken();
				}
				else if (startswith(&ptr, "M3U"))
				{
					// Spec :: 4.3.1.1.  EXTM3U - It MUST be the first line of every Media Playlist and every Master Playlist
					if(lineNum)
					{
						AAMPLOG_WARN("M3U tag not the first line[%d] of Manifest",lineNum);
						retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
						break;
					}
				}
				else if (startswith(&ptr, "-X-CONTENT-IDENTIFIER:"))
				{
#ifdef AVE_DRM
					std::string vssServiceZone = aamp->GetServiceZone();

					if(!vssServiceZone.empty())
					{
						char * ptrContentID = ptr;

						if(startswith(&ptr, VSS_VIRTUAL_STREAM_ID_PREFIX))
						{
							std::string vssData = " \"client:accessAttributes\": [\"";
							vssData.append(VSS_VIRTUAL_STREAM_ID_KEY_STR);
							vssData.append("\",\"");
							vssData.append(ptr);
							vssData.append("\",\"");
							vssData.append(VSS_SERVICE_ZONE_KEY_STR);
							vssData.append("\",\"");
							vssData.append(vssServiceZone);
							vssData.append("\"]");
							setCustomLicensePayLoad(vssData.c_str());
							AAMPLOG_INFO("custom vss data:%s",vssData.c_str());
						}
						else
						{
							AAMPLOG_WARN("Invalid VirtualStreamID:%s", ptrContentID);
						}
					}
#endif
				}
				else if (startswith(&ptr, "-X-FOG"))
				{
				}
				else if (startswith(&ptr, "-X-XCAL-CONTENTMETADATA"))
				{ // placeholder for new Super8 DRM Agnostic Metadata
				}
				else if (startswith(&ptr, "-NOM-I-FRAME-DISTANCE"))
				{ // placeholder for nominal distance between IFrames
				}
				else if (startswith(&ptr, "-X-ADVERTISING"))
				{ // placeholder for advertising zone for linear (soon to be deprecated)
				}
				else if (startswith(&ptr, "-UPLYNK-LIVE"))
				{ // related to uplynk streaming service
				}
				else if (startswith(&ptr, "-X-START:"))
				{ // i.e. "TIME-OFFSET=2.336, PRECISE=YES" - specifies the preferred point in the video to start playback; not yet supported

					// check if App has not configured any liveoffset
					{
						double offsetval = ParseXStartTimeOffset(ptr);
						if (offsetval != 0)
						{
							if(!aamp->IsLiveAdjustRequired())
							{
								SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_CDVRLiveOffset,(double)abs(offsetval));
							}
							else
							{
								SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_LiveOffset,(double)abs(offsetval));
							}
							aamp->UpdateLiveOffset();
							AAMPLOG_WARN("WARNING:found EXT-X-START in MainManifest Offset:%f  liveOffset:%f",offsetval,aamp->mLiveOffset);
						}
					}
				}
				else if (startswith(&ptr, "INF:"))
				{
					// its not a main manifest, instead its playlist given for playback . Consider not a error
					// Report it , so that Init flow can be changed accordingly
					retval = eAAMPSTATUS_PLAYLIST_PLAYBACK;
					break;
				}
#ifdef AAMP_HLS_DRM
				else if (startswith(&ptr, "-X-SESSION-KEY:"))
				{
						if (ISCONFIGSET(eAAMPConfig_Fragmp4PrefetchLicense))
						{
							size_t len;
							len = FindLineLength(ptr);
							std::string KeyTagStr(ptr,len);

							pthread_mutex_lock(&aamp->drmParserMutex);
							attrNameData* aesCtrAttrData = new attrNameData(KeyTagStr);
							if (std::find(aamp->aesCtrAttrDataList.begin(), aamp->aesCtrAttrDataList.end(),
									*aesCtrAttrData) == aamp->aesCtrAttrDataList.end()) {
								AAMPLOG_INFO("Adding License data from Main Manifest %s",KeyTagStr.c_str());
								aamp->aesCtrAttrDataList.push_back(*aesCtrAttrData);
							}
							SAFE_DELETE(aesCtrAttrData);
							pthread_mutex_unlock(&aamp->drmParserMutex);
							aamp->fragmentCdmEncrypted = true;
							InitiateDrmProcess(this->aamp);
						}
				}
#endif
				else
				{
					std::string unknowTag= ptr;
					AAMPLOG_INFO("***unknown tag:%s", unknowTag.substr(0,24).c_str());
				}
				lineNum++;
			}
		}
		ptr = next;
	}// while till end of file

	if(retval == eAAMPSTATUS_OK)
	{
		// Check if there are are valid profiles to do playback
		if(vProfileCount == 0)
		{
			AAMPLOG_WARN("ERROR No video profiles available in manifest for playback");
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}
		else
		{	// If valid video profiles , check for Media and iframe and report warnings only
			if(mMediaCount == 0)
			{
				// just a warning .Play the muxed content with default audio
				AAMPLOG_WARN("WARNING !!! No media definitions in manifest for playback");
			}
			if(iFrameCount == 0)
			{
				// just a warning
				AAMPLOG_WARN("WARNING !!! No iframe definitions .Trickplay not supported");
			}

			// if separate Media exists then find the codec for the audio type
			for (int i = 0; i < mMediaCount; i++)
			{
				struct MediaInfo *media = &(mediaInfo[i]);
				if (media->type == eMEDIATYPE_AUDIO)
				{
					std::string codec;
					//Find audio codec from X-STREAM-INF: or streamInfo
					for (int j = 0; j < mProfileCount; j++)
					{
						struct HlsStreamInfo *stream = &this->streamInfo[j];

						//Find the X-STREAM_INF having same group id as audio track to parse codec info
						if (!stream->isIframeTrack && stream->audio != NULL && media->group_id != NULL &&
							strcmp(stream->audio, media->group_id) == 0)
						{
								// assign the audioformat from streamInfo to mediaInfo
								media->audioFormat = stream->audioFormat;
								break;
						}
					}
				}
			}
		}
		aamp->StoreLanguageList(mLangList); // For playlist playback, there will be no languages available
	}
	return retval;
}


/***************************************************************************
* @fn GetFragmentUriFromIndex
* @brief Function to get fragment URI from index count
*
* @return string fragment URI pointer
***************************************************************************/
char *TrackState::GetFragmentUriFromIndex(bool &bSegmentRepeated)
{
	char * uri = NULL;
	const IndexNode *index = (IndexNode *) this->index.ptr;
	const IndexNode *idxNode = NULL;
	int idx;
	if (context->rate > 0)
	{

		const IndexNode *lastIndexNode = &index[indexCount - 1];
		double seekWindowEnd = lastIndexNode->completionTimeSecondsFromStart - aamp->mLiveOffset;
		if (IsLive() && playTarget > seekWindowEnd)
		{
			AAMPLOG_WARN("rate - %f playTarget(%f) > seekWindowEnd(%f), forcing EOS",
                                context->rate, playTarget, seekWindowEnd);
			return NULL;
		}

		if (currentIdx == -1)
		{ // search forward from beginning
			currentIdx = 0;
		}
		for (idx = currentIdx; idx < indexCount; idx++)
		{ // search in direction until out-of-bounds
			const IndexNode *node = &index[idx];
			//AAMPLOG_WARN("rate %f completionTimeSecondsFromStart %f playTarget %f", rate, node->completionTimeSecondsFromStart, playTarget);
			if (node->completionTimeSecondsFromStart >= playTarget)
			{ // found target iframe
#ifdef TRACE
				AAMPLOG_WARN("Found node - rate %f completionTimeSecondsFromStart %f playTarget %f",
				        context->rate, node->completionTimeSecondsFromStart, playTarget);
#endif
				idxNode = node;
				break;
			}
		}
	}
	else
	{
		if (-1 == currentIdx)
		{ // search backward from end
			currentIdx = indexCount - 1;
		}
		for (idx = currentIdx; idx >= 0; idx--)
		{ // search in direction until out-of-bounds
			const IndexNode *node = &index[idx];
			if (node->completionTimeSecondsFromStart <= playTarget)
			{ // found target iframe
#ifdef TRACE
				AAMPLOG_WARN("Found node - rate %f completionTimeSecondsFromStart %f playTarget %f",
						context->rate, node->completionTimeSecondsFromStart, playTarget);
#endif
				idxNode = node;
				break;
			}
		}
	}
	if (idxNode)
	{
		// For Fragmented MP4 check if initFragment injection is required
		if ( idxNode->initFragmentPtr
				&& ( (!mInitFragmentInfo) || strcmp(mInitFragmentInfo, idxNode->initFragmentPtr) != 0))
		{
			AAMPLOG_TRACE( "init fragment injection required (%s)", idxNode->initFragmentPtr );
			mInitFragmentInfo = idxNode->initFragmentPtr;
			mInjectInitFragment = true;
		}

		currentIdx = idx;
		byteRangeOffset = 0;
		byteRangeLength = 0;
		//AAMPLOG_WARN("fragmentinfo %s", idxNode->pFragmentInfo);
		const char *fragmentInfo = idxNode->pFragmentInfo;
		fragmentDurationSeconds = idxNode->completionTimeSecondsFromStart;
		if (idx > 0)
		{
			fragmentDurationSeconds -= index[idx - 1].completionTimeSecondsFromStart;
		}

		if(lastDownloadedIFrameTarget != -1 && idxNode->completionTimeSecondsFromStart == lastDownloadedIFrameTarget)
		{
			// found playtarget  and lastdownloaded target on same segment .
			bSegmentRepeated = true;
		}
		else
		{       // diff segment
			bSegmentRepeated = false;
			lastDownloadedIFrameTarget = idxNode->completionTimeSecondsFromStart;
		}
	
		while (fragmentInfo[0] == '#')
		{
			if (!memcmp(fragmentInfo, "#EXT-X-BYTERANGE:", 17))
			{
				sscanf( fragmentInfo+17, "%zu@%zu", & byteRangeLength, &byteRangeOffset );
			}
			/*Skip to next line*/
			while (fragmentInfo[0] != CHAR_LF)
			{
				fragmentInfo++;
			}
			fragmentInfo++;
		}
		const char *urlEnd = strchr(fragmentInfo, CHAR_LF);
		if (urlEnd)
		{
			if (*(urlEnd - 1) == CHAR_CR)
			{
				urlEnd--;
			}
			int urlLen = urlEnd - fragmentInfo;
			mFragmentURIFromIndex.assign(fragmentInfo, urlLen);
			if(!mFragmentURIFromIndex.empty()){
				uri = (char *)mFragmentURIFromIndex.c_str();
			}
			
			//The EXT-X-TARGETDURATION tag specifies the maximum Media Segment   duration.  
			//The EXTINF duration of each Media Segment in the Playlist   file, when rounded to the nearest integer, 
			//MUST be less than or equal   to the target duration
			if(uri && std::round(fragmentDurationSeconds) > targetDurationSeconds) 
			{
				AAMPLOG_WARN("WARN - Fragment duration[%f] > TargetDuration[%f] for URI:%s",fragmentDurationSeconds ,targetDurationSeconds,uri);
			}
		}
		else
		{
			AAMPLOG_WARN("unable to find end");
		}
		if (-1 == idxNode->drmMetadataIdx)
		{
			fragmentEncrypted = false;
		}
		else
		{
			fragmentEncrypted = true;
			// for each iframe , need to see if KeyTag changed and get the drminfo .
			// Get the key Index position .
			int keyIndexPosn = idxNode->drmMetadataIdx;
			if(keyIndexPosn != mLastKeyTagIdx)
			{
				AAMPLOG_WARN("[%d] KeyTable Size [%zu] keyIndexPosn[%d] lastKeyIdx[%d]",type, mKeyHashTable.size(), keyIndexPosn, mLastKeyTagIdx);
				if(keyIndexPosn < mKeyHashTable.size() && mKeyHashTable[keyIndexPosn].mKeyTagStr.size())
				{
					// ParseAttrList function modifies the input string ,hence cannot pass mKeyTagStr
					// modifying const memory will cause crash . So had to copy locally
					char* key =(char*) malloc (mKeyHashTable[keyIndexPosn].mKeyTagStr.size());
                                        memcpy(key,mKeyHashTable[keyIndexPosn].mKeyTagStr.c_str(),mKeyHashTable[keyIndexPosn].mKeyTagStr.size());

					//AAMPLOG_WARN("[%d] Parse the Key attribute for new KeyIndex[%d][%s] ",type,keyIndexPosn,mKeyHashTable[keyIndexPosn].mShaID.c_str());
					ParseAttrList((char *)key, ParseKeyAttributeCallback, this);
					free(key);
				}
				mKeyTagChanged = true;
				mLastKeyTagIdx = keyIndexPosn;
			}

		}
	}
	else
	{
		AAMPLOG_WARN("Couldn't find node - rate %f playTarget %f",
				context->rate, playTarget);
	}
	return uri;
}

/***************************************************************************
* @fn GetNextFragmentUriFromPlaylist
* @brief Function to get next fragment URI from playlist based on playtarget
* @param ignoreDiscontinuity Ignore discontinuity
* @return string fragment URI pointer
***************************************************************************/
char *TrackState::GetNextFragmentUriFromPlaylist(bool ignoreDiscontinuity)
{
	char *ptr = fragmentURI;
	char *rc = NULL;
	size_t byteRangeLength = 0; // default, when optional byterange offset is left unspecified
	size_t byteRangeOffset = 0;
	bool discontinuity = false;
	const char* programDateTime = NULL;

	AAMPLOG_TRACE("GetNextFragmentUriFromPlaylist %s playTarget %f playlistPosition %f fragmentURI %p",name, playTarget, playlistPosition, fragmentURI);

	if (playTarget < 0)
	{
		AAMPLOG_WARN("[!!!!! WARNING !!!!] Invalid playTarget %f, so set playTarget to 0", playTarget);
		playTarget = 0;
		//return fragmentURI; // leads to buffer overrun/crash
	}
	if (playlistPosition == playTarget)
	{
		//AAMPLOG_WARN("[PLAYLIST_POSITION==PLAY_TARGET]");
		return fragmentURI;
	}
	if ((playlistPosition != -1) && (fragmentURI != NULL))
	{ // already presenting - skip past previous segment
		//AAMPLOG_WARN("[PLAYLIST_POSITION!= -1]");
		ptr += strlen(fragmentURI) + 1;
	}
	if ((playlistPosition > playTarget) && (fragmentDurationSeconds > PLAYLIST_TIME_DIFF_THRESHOLD_SECONDS) &&
		((playlistPosition - playTarget) > fragmentDurationSeconds))
	{
		AAMPLOG_WARN("playlistPosition[%f] > playTarget[%f] more than last fragmentDurationSeconds[%f]",
					playlistPosition, playTarget, fragmentDurationSeconds);
	}
	if (-1 == playlistPosition)
	{
		// Starts parsing from beginning, so change to default
		fragmentEncrypted = false;
	}
	//AAMPLOG_WARN("before loop, ptr = %p fragmentURI %p", ptr, fragmentURI);
	while (ptr)
	{
		char *next = mystrpbrk(ptr);
		//AAMPLOG_WARN("ptr %s next %.*s", ptr, 10, next);
		if (*ptr)
		{
			if (startswith(&ptr, "#EXT"))
			{ // tags begins with #EXT
				if (startswith(&ptr, "M3U"))
				{ // "Extended M3U file" - always first line
				}
				else if (startswith(&ptr, "INF:"))
				{// preceeds each advertised fragment in a playlist
					if (-1 != playlistPosition)
					{
						playlistPosition += fragmentDurationSeconds;
					}
					else
					{
						playlistPosition = 0;
					}
					fragmentDurationSeconds = atof(ptr);
#ifdef TRACE
					AAMPLOG_WARN("Next - EXTINF - playlistPosition updated to %f", playlistPosition);
					// optionally followed by human-readable title
#endif
				}
				else if (startswith(&ptr, "-X-BYTERANGE:"))
				{
					char  temp[1024];
					strncpy(temp, ptr, 1023);
					temp[1023] = 0x00;
					char * offsetDelim = strchr(temp, '@'); // optional
					if (offsetDelim)
					{
						*offsetDelim++ = 0x00;
						sscanf(offsetDelim, "%zu", &byteRangeOffset);
					}
					sscanf(temp, "%zu", &byteRangeLength);

					mByteOffsetCalculation = true;
					if (0 != byteRangeLength && 0 == byteRangeOffset)
					{
						byteRangeOffset = this->byteRangeOffset + this->byteRangeLength;
					}
					AAMPLOG_TRACE("byteRangeOffset:%zu Last played fragment Offset:%zu byteRangeLength:%zu Last fragment Length:%zu", byteRangeOffset, this->byteRangeOffset, byteRangeLength, this->byteRangeLength);
				}
				else if (startswith(&ptr, "-X-TARGETDURATION:"))
				{ // max media segment duration; required; appears once
					targetDurationSeconds = atof(ptr);
				}
				else if (startswith(&ptr, "-X-MEDIA-SEQUENCE:"))
				{// first media URI's unique integer sequence number
					nextMediaSequenceNumber = atoll(ptr);
				}
				else if (startswith(&ptr, "-X-KEY:"))
				{ // identifies licensing server to contact for authentication
					ParseAttrList(ptr, ParseKeyAttributeCallback, this);
				}
				else if(startswith(&ptr,"-X-MAP:"))
				{
					AAMPLOG_TRACE("Old-Init : %s, New-Init:%s", mInitFragmentInfo, ptr);
					if ((!mInitFragmentInfo) || (mInitFragmentInfo && ptr && strcmp(mInitFragmentInfo, ptr) != 0))
					{
						mInitFragmentInfo = ptr;
						mInjectInitFragment = true;
						AAMPLOG_INFO("Found #EXT-X-MAP data: %s", mInitFragmentInfo);
					}
				}
				else if (startswith(&ptr, "-X-PROGRAM-DATE-TIME:"))
				{ // associates following media URI with absolute date/time
					// if used, should supplement any EXT-X-DISCONTINUITY tags
					AAMPLOG_TRACE("Got EXT-X-PROGRAM-DATE-TIME: %s ", ptr);
					if (context->mNumberOfTracks > 1)
					{
						programDateTime = ptr;
						// The first X-PROGRAM-DATE-TIME tag holds the start time for each track
						if (startTimeForPlaylistSync == 0.0 )
						{
							/* discarding timezone assuming audio and video tracks has same timezone and we use this time only for synchronization*/
							startTimeForPlaylistSync = ISO8601DateTimeToUTCSeconds(ptr);
							AAMPLOG_WARN("%s StartTimeForPlaylistSync : %f ",name, startTimeForPlaylistSync);							
						}
					}
				}
				else if (startswith(&ptr, "-X-ALLOW-CACHE:"))
				{ // YES or NO - authorizes client to cache segments for later replay
					if (startswith(&ptr, "YES"))
					{
						context->allowsCache = true;
					}
					else if (startswith(&ptr, "NO"))
					{
						context->allowsCache = false;
					}
					else
					{
						AAMPLOG_ERR("unknown ALLOW-CACHE setting");
					}
				}
				else if (startswith(&ptr, "-X-PLAYLIST-TYPE:"))
				{
					//PlaylistType is handled during indexing.
				}
				else if (startswith(&ptr, "-X-ENDLIST"))
				{ // indicates that no more media segments are available
					AAMPLOG_WARN("#EXT-X-ENDLIST");
					mReachedEndListTag = true;
				}
				else if (startswith(&ptr, "-X-DISCONTINUITY-SEQUENCE"))
				{
					// ignore this tag for now 
				}
				else if (startswith(&ptr, "-X-DISCONTINUITY"))
				{
					discontinuity = true;
				}
				else if (startswith(&ptr, "-X-I-FRAMES-ONLY"))
				{
					AAMPLOG_WARN("#EXT-X-I-FRAMES-ONLY");
				}
				else if (startswith(&ptr, "-X-VERSION:"))
				{	//CID:101256 - set not used
				}
				// custom tags follow:
				else if (startswith(&ptr, "-X-FAXS-CM:"))
				{
					//DRM meta data is stored during indexing.
				}
				else if (startswith(&ptr, "-X-FAXS-PACKAGINGCERT"))
				{
				}
				else if (startswith(&ptr, "-X-FAXS-SIGNATURE"))
				{
				}
				else if (startswith(&ptr, "-X-CUE"))
				{
				}
				else if (startswith(&ptr, "-X-CM-SEQUENCE"))
				{
				}
				else if (startswith(&ptr, "-X-MARKER"))
				{
				}
				else if (startswith(&ptr, "-X-MAP"))
				{
				}
				else if (startswith(&ptr, "-X-MEDIA-TIME"))
				{
				}
				else if (startswith(&ptr, "-X-END-TOP-TAGS"))
				{
				}
				else if (startswith(&ptr, "-X-CONTENT-IDENTIFIER"))
				{
				}
				else if (startswith(&ptr, "-X-TRICKMODE-RESTRICTION"))
				{
				}
				else if (startswith(&ptr, "-X-INDEPENDENT-SEGMENTS"))
				{
				}
				else if (startswith(&ptr, "-X-BITRATE"))
				{
				}
				else if (startswith(&ptr, "-X-FOG"))
				{
				}
				else if (startswith(&ptr,"-UPLYNK-LIVE"))
				{//tag related to uplynk streaming service
				}
				else if (startswith(&ptr, "-X-START:"))
				{
				}
				else if (startswith(&ptr, "-X-XCAL-CONTENTMETADATA"))
				{ // placeholder for new Super8 DRM Agnostic Metadata
				}
				else if (startswith(&ptr, "-NOM-I-FRAME-DISTANCE"))
				{ // placeholder for nominal distance between IFrames
				}
				else if (startswith(&ptr, "-X-ADVERTISING"))
				{ // placeholder for advertising zone for linear (soon to be deprecated)
				}
				else if (startswith(&ptr, "-X-SOURCE-STREAM"))
				{ // placeholder for vss source stream id
				}
				else if (startswith(&ptr, "-X-X1-LIN-CK"))
				{ // placeholder for deferred drm information
				}
				else if (startswith(&ptr, "-X-SCTE35"))
				{ // placeholder for DAI tag processing
				}
				else if (startswith(&ptr, "-X-ASSET") || startswith(&ptr, "-X-CUE-OUT") || startswith(&ptr, "-X-CUE-IN") ||
						startswith(&ptr, "-X-DATERANGE") || startswith(&ptr, "-X-SPLICEPOINT-SCTE35"))
				{ // placeholder for HLS ad markers used by MediaTailor
				}
				else
				{
					std::string unknowTag= ptr;
					AAMPLOG_INFO("***unknown tag:%s", unknowTag.substr(0,24).c_str());	
				}
			}
			else if (*ptr == '#')
			{ // all other lines beginning with # are comments
			}
			else if (*ptr == '\0')
			{ // skip inserted null char
			}
			else
			{ // URI
				nextMediaSequenceNumber++;
				if (((playlistPosition + fragmentDurationSeconds) > playTarget) || ((playTarget - playlistPosition) < PLAYLIST_TIME_DIFF_THRESHOLD_SECONDS))
				{
					//AAMPLOG_WARN("Return fragment %s playlistPosition %f playTarget %f", ptr, playlistPosition, playTarget);
					this->byteRangeOffset = byteRangeOffset;
					this->byteRangeLength = byteRangeLength;
					mByteOffsetCalculation = false;
					if (discontinuity)
					{
						if (!ignoreDiscontinuity)
						{
							AAMPLOG_WARN("#EXT-X-DISCONTINUITY in track[%d] playTarget %f total mCulledSeconds %f", type, playTarget, mCulledSeconds);
							// Check if X-DISCONTINUITY tag is seen without explicit X-MAP tag
							// Reuse last parsed/seen X-MAP tag in such cases
							if (mInitFragmentInfo != NULL && mInjectInitFragment == false)
							{
								mInjectInitFragment = true;
								AAMPLOG_WARN("Reusing last seen #EXT-X-MAP for this discontinuity, data: %s", mInitFragmentInfo);
							}

							TrackType otherType = (type == eTRACK_VIDEO)? eTRACK_AUDIO: eTRACK_VIDEO;
							TrackState *other = context->trackState[otherType];
							if (other->enabled)
							{
								double diff;
								double position;
								double playPosition = playTarget - mCulledSeconds;
								if (!programDateTime)
								{
									position = playPosition;
								}
								else
								{
									position = ISO8601DateTimeToUTCSeconds(programDateTime );
									AAMPLOG_WARN("[%s] Discontinuity - position from program-date-time %f", name, position);
								}
								AAMPLOG_WARN("%s Checking HasDiscontinuity for position :%f, playposition :%f playtarget:%f mDiscontinuityCheckingOn:%d",name,position,playPosition,playTarget,mDiscontinuityCheckingOn);								
								if (!mDiscontinuityCheckingOn) 
								{
									if(false == other->HasDiscontinuityAroundPosition(position, (NULL != programDateTime), diff, playPosition,mCulledSeconds,mProgramDateTime))
									{
										AAMPLOG_WARN("[%s] Ignoring discontinuity as %s track does not have discontinuity", name, other->name);
										discontinuity = false;
									}
									else if (programDateTime)
									{
										AAMPLOG_WARN("[WARN] [%s] diff %f with other track discontinuity position!!", name, diff);

										/*If other track's discontinuity is in advanced position more than the targetDuration, then skip it.*/
										if (diff > targetDurationSeconds)
										{
											/*Skip fragment*/
											AAMPLOG_WARN("[WARN!! Can be a Stream Issue!!] [%s] Other track's discontinuity time greater by %f. updating playTarget %f to %f",
													name, diff, playTarget, playlistPosition + diff);
											mSyncAfterDiscontinuityInProgress = true;
											playTarget = playlistPosition + diff;
											discontinuity = false;
											programDateTime = NULL;
											ptr = next;
											continue;
										}
									}
								}
								
							}
						}
						else
						{
							discontinuity = false;
						}
					}
					this->discontinuity = discontinuity || mSyncAfterDiscontinuityInProgress;
					mSyncAfterDiscontinuityInProgress = false;
					AAMPLOG_TRACE(" [%s] Discontinuity - %d", name, (int)this->discontinuity);
					rc = ptr;
					//The EXT-X-TARGETDURATION tag specifies the maximum Media Segment   duration.  
					//The EXTINF duration of each Media Segment in the Playlist   file, 
					//when rounded to the nearest integer, 
					//MUST be less than or equal   to the target duration
					if(rc && std::round(fragmentDurationSeconds) > targetDurationSeconds) 
					{
						AAMPLOG_WARN("WARN - Fragment duration[%f] > TargetDuration[%f] for URI:%s",fragmentDurationSeconds ,targetDurationSeconds,rc);
					}
					break;
				}
				else
				{
					if(mByteOffsetCalculation)
					{
						byteRangeOffset += byteRangeLength;
					}
					discontinuity = false;
					programDateTime = NULL;
					// AAMPLOG_WARN("Skipping fragment %s playlistPosition %f playTarget %f", ptr, playlistPosition, playTarget);
				}
			}
		}
		ptr = next;
	}
#ifdef TRACE
	AAMPLOG_WARN("GetNextFragmentUriFromPlaylist %s:  pos %f returning %s", name,playlistPosition, rc);
	AAMPLOG_WARN("GetNextFragmentUriFromPlaylist %s: seqNo=%lld", name,nextMediaSequenceNumber - 1);
#endif
	return rc;
}


/***************************************************************************
* @fn FindMediaForSequenceNumber
* @brief Get fragment tag based on media sequence number
*
* @return string fragment tag line pointer
***************************************************************************/
char *TrackState::FindMediaForSequenceNumber()
{
	char *ptr = playlist.ptr;
	long long mediaSequenceNumber = nextMediaSequenceNumber - 1;
	char *key = NULL;
	char *initFragment = NULL;

	long long seq = 0;
	while (ptr)
	{
		char *next = mystrpbrk(ptr);
		if (*ptr)
		{
			if (startswith(&ptr, "#EXTINF:"))
			{
				fragmentDurationSeconds = atof(ptr);
			}
			else if (startswith(&ptr, "#EXT-X-MEDIA-SEQUENCE:"))
			{
				seq = atoll(ptr);
			}
			else if (startswith(&ptr, "#EXT-X-KEY:"))
			{
				key = ptr;
			}
			else if (startswith(&ptr, "#EXT-X-MAP:"))
			{
				initFragment = ptr;
			}
			else if (ptr[0] != '#')
			{ // URI
				if (seq >= mediaSequenceNumber)
				{
					if ((mDrmKeyTagCount >1) && key)
					{
						ParseAttrList(key, ParseKeyAttributeCallback, this);
					}
					if (initFragment)
					{
						// mInitFragmentInfo will be cleared after calling FlushIndex() from IndexPlaylist()
						if (!mInitFragmentInfo)
						{
							mInitFragmentInfo = initFragment;
							AAMPLOG_INFO("Found #EXT-X-MAP data: %s", mInitFragmentInfo);
						}
					}
					if (seq != mediaSequenceNumber)
					{
						AAMPLOG_WARN("seq gap %lld!=%lld", seq, mediaSequenceNumber);
						nextMediaSequenceNumber = seq + 1;
					}
					return ptr;
				}
				seq++;
			}
		}
		ptr = next;
	}
	return NULL;
}
/***************************************************************************
* @fn FetchFragmentHelper
* @brief Helper function to download fragment
*
* @param http_error[out] http error string
* @param decryption_error[out] decryption error
* @return bool true on success else false
***************************************************************************/
bool TrackState::FetchFragmentHelper(long &http_error, bool &decryption_error, bool & bKeyChanged, int * fogError, double &downloadTime)
{
#ifdef TRACE
		AAMPLOG_WARN("FetchFragmentHelper Enter: pos %f start %f frag-duration %f fragmentURI %s",
				playlistPosition, playTarget, fragmentDurationSeconds, fragmentURI );
#endif
		assert (fragmentURI);
		http_error = 0;
		bool bSegmentRepeated = false;
		if (context->trickplayMode && ABRManager::INVALID_PROFILE != context->GetIframeTrack())
		{
			// Note :: only for IFrames , there is a possibility of same segment getting downloaded again 
			// Target of next download is not based on segment duration but fixed interval .
			// If iframe segment duration is 1.001sec , and based on rate if increment is happening at 1sec 
			// same segment will be downloaded twice .
			double delta = context->rate / context->mTrickPlayFPS;
			fragmentURI = GetFragmentUriFromIndex(bSegmentRepeated);
			if (context->rate < 0)
			{ // rewind
				if (!fragmentURI || (playTarget == 0))
				{
					AAMPLOG_WARN("aamp rew to beginning");
					eosReached = true;
				}
				else if (playTarget > -delta)
				{
					playTarget += delta;
				}
				else
				{
					playTarget = 0;
				}
			}
			else
			{// fast-forward
				if (!fragmentURI)
				{
					AAMPLOG_WARN("aamp ffw to end");
					eosReached = true;
				}
				playTarget += delta;
			}
			
			//AAMPLOG_WARN("Updated playTarget to %f", playTarget);
		}
		else
		{// normal speed
			fragmentURI = GetNextFragmentUriFromPlaylist();
			if (fragmentURI != NULL)
			{
				if (!mInjectInitFragment)
					playTarget = playlistPosition + fragmentDurationSeconds;

				if (IsLive())
				{
					context->CheckForPlaybackStall(true);
				}
			}
			else
			{
				if ((!IsLive() || mReachedEndListTag) && (playlistPosition != -1))
				{
					AAMPLOG_WARN("aamp play to end. playTarget %f fragmentURI %p ReachedEndListTag %d Type %d", playTarget, fragmentURI, mReachedEndListTag,type);
					eosReached = true;
				}
				else if (IsLive() && type == eTRACK_VIDEO)
				{
					context->CheckForPlaybackStall(false);
				}
			}
		}

		if (!mInjectInitFragment && fragmentURI && !bSegmentRepeated)
		{
			std::string fragmentUrl;
			CachedFragment* cachedFragment = GetFetchBuffer(true);
			aamp_ResolveURL(fragmentUrl, mEffectiveUrl, fragmentURI , ISCONFIGSET(eAAMPConfig_PropogateURIParam));
			AAMPLOG_TRACE("Got next fragment url %s fragmentEncrypted %d discontinuity %d mDrmMethod %d", fragmentUrl.c_str(), fragmentEncrypted, (int)discontinuity, mDrmMethod);

			aamp->profiler.ProfileBegin(mediaTrackBucketTypes[type]);
			const char *range;
			char rangeStr[128];
			if (byteRangeLength)
			{
				size_t next = byteRangeOffset + byteRangeLength;
				sprintf(rangeStr, "%zu-%zu", byteRangeOffset, next - 1);
				AAMPLOG_WARN("FetchFragmentHelper rangeStr %s ", rangeStr);

				range = rangeStr;
			}
			else
			{
				range = NULL;
			}
#ifdef TRACE
			AAMPLOG_WARN("FetchFragmentHelper: fetching %s", fragmentUrl.c_str());
#endif
			// patch for http://bitdash-a.akamaihd.net/content/sintel/hls/playlist.m3u8
			// if fragment URI uses relative path, we don't want to replace effective URI
			std::string tempEffectiveUrl;
			AAMPLOG_TRACE(" Calling Getfile . buffer %p avail %d", &cachedFragment->fragment, (int)cachedFragment->fragment.avail);
			bool fetched = aamp->GetFile(fragmentUrl, &cachedFragment->fragment,
			 tempEffectiveUrl, &http_error, &downloadTime, range, type, false, (MediaType)(type), NULL, NULL, fragmentDurationSeconds);
			//Workaround for 404 of subtitle fragments
			//TODO: This needs to be handled at server side and this workaround has to be removed
			if (!fetched && http_error == 404 && type == eTRACK_SUBTITLE)
			{
				aamp_AppendBytes(&cachedFragment->fragment, "WEBVTT", 7);
				fetched = true;
			}
			if (!fetched)
			{
				//cleanup is done in aamp_GetFile itself

				aamp->profiler.ProfileError(mediaTrackBucketTypes[type], http_error);
				aamp->profiler.ProfileEnd(mediaTrackBucketTypes[type]);
				if (mSkipSegmentOnError)
				{
					// Skipping segment on error, increase fail count
					segDLFailCount += 1;
				}
				else
				{
					// Already attempted rampdown on same segment
					// Skip segment if there is no profile to rampdown.
					mSkipSegmentOnError = true;
				}
				if (AAMP_IS_LOG_WORTHY_ERROR(http_error))
				{
					AAMPLOG_WARN("FetchFragmentHelper aamp_GetFile failed");
				}
				//Adding logic to report error if fragment downloads are failing continuously
				//Avoid sending error for failure to download subtitle fragments
				int FragmentDownloadFailThreshold;
				GETCONFIGVALUE(eAAMPConfig_FragmentDownloadFailThreshold,FragmentDownloadFailThreshold);
				if((FragmentDownloadFailThreshold <= segDLFailCount) && aamp->DownloadsAreEnabled() && type != eTRACK_SUBTITLE)
				{
					AAMPLOG_ERR("Not able to download fragments; reached failure threshold sending tune failed event");
					aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, http_error);
				}
				aamp_Free(&cachedFragment->fragment);
				lastDownloadedIFrameTarget = -1;
				return false;
			}
			else
			{
				// increment the buffer value after download 
				playTargetBufferCalc += fragmentDurationSeconds;
			}

			if((eTRACK_VIDEO == type)  && (aamp->IsTSBSupported()))
			{
				std::size_t pos = fragmentUrl.find(FOG_FRAG_BW_IDENTIFIER);
				if (pos != std::string::npos)
				{
					std::string bwStr = fragmentUrl.substr(pos + FOG_FRAG_BW_IDENTIFIER_LEN);
					if (!bwStr.empty())
					{
						pos = bwStr.find(FOG_FRAG_BW_DELIMITER);
						if (pos != std::string::npos)
						{
							bwStr = bwStr.substr(0, pos);
							context->SetTsbBandwidth(std::stol(bwStr));
						}
					}
				}
			}

			aamp->profiler.ProfileEnd(mediaTrackBucketTypes[type]);
			segDLFailCount = 0;

			if (cachedFragment->fragment.len && fragmentEncrypted && mDrmMethod == eDRM_KEY_METHOD_AES_128)
			{
				// DrmDecrypt resets mKeyTagChanged , take a back up here to give back to caller
				bKeyChanged = mKeyTagChanged;
				{	
					/* XRE-18526 
					 * From RFC8216 - Section 5.2,
					 * An EXT-X-KEY tag with a KEYFORMAT of "identity" that does not have an
					 * IV attribute indicates that the Media Sequence Number is to be used
					 * as the IV when decrypting a Media Segment, by putting its big-endian
					 * binary representation into a 16-octet (128-bit) buffer and padding
					 * (on the left) with zeros.
					 * Eg:
					 * When manifest has no IV attribute as below, client has to create IV data.
					 * #EXT-X-KEY:METHOD=AES-128,URI="https://someserver.com/hls.key" 
					 * When manifest has IV attribute as below, client will use this IV, no need to create IV data.
					 * #EXT-X-KEY:METHOD=AES-128,URI="https://someserver.com/hls.key",IV=0xABCDABCD
					 */
					if ( eMETHOD_AES_128 == mDrmInfo.method && true == mDrmInfo.bUseMediaSequenceIV )
					{
						if ( true == CreateInitVectorByMediaSeqNo(nextMediaSequenceNumber-1) )
						{
							// Set this flag to seed the newly created IV to corresponding DRM instance
							mKeyTagChanged = true;
						}
						else
						{
							AAMPLOG_WARN ( "FetchFragmentHelper : Create Init Vector failed");
						}
					}

					AAMPLOG_TRACE(" [%s] uri %s - calling  DrmDecrypt()",name, fragmentURI);
					DrmReturn drmReturn = DrmDecrypt(cachedFragment, mediaTrackDecryptBucketTypes[type]);

					if(eDRM_SUCCESS != drmReturn)
					{
						if (aamp->DownloadsAreEnabled())
						{
							AAMPLOG_WARN("FetchFragmentHelper : drm_Decrypt failed. fragmentURI %s - RetryCount %d", fragmentURI, segDrmDecryptFailCount);
							if (eDRM_KEY_ACQUSITION_TIMEOUT == drmReturn)
							{
								decryption_error = true;
								AAMPLOG_WARN("FetchFragmentHelper : drm_Decrypt failed due to license acquisition timeout");
								aamp->SendErrorEvent(AAMP_TUNE_LICENCE_TIMEOUT, NULL, true);
							}
							else
							{
								/* Added to send tune error when fragments decryption failed */
								segDrmDecryptFailCount +=1;

								if(aamp->mDrmDecryptFailCount <= segDrmDecryptFailCount)
								{
									decryption_error = true;
									AAMPLOG_ERR("FetchFragmentHelper : drm_Decrypt failed for fragments, reached failure threshold (%d) sending failure event", aamp->mDrmDecryptFailCount);
									aamp->SendErrorEvent(AAMP_TUNE_DRM_DECRYPT_FAILED);
								}
							}
						}
						aamp_Free(&cachedFragment->fragment);
						lastDownloadedIFrameTarget = -1;
						return false;
					}
#ifdef TRACE
					else
					{
						AAMPLOG_WARN("aamp: hls - eMETHOD_AES_128 not set for %s", fragmentURI);
					}
#endif
					segDrmDecryptFailCount = 0; /* Resetting the retry count in the case of decryption success */
				}
				if (!context->firstFragmentDecrypted)
				{
					aamp->NotifyFirstFragmentDecrypted();
					context->firstFragmentDecrypted = true;
				}
			}
			else if(!cachedFragment->fragment.len)
			{
				AAMPLOG_WARN("fragment. len zero for %s", fragmentURI);
			}
		}
		else
		{
			bool ret = false;
			if (mInjectInitFragment)
			{
				AAMPLOG_INFO("FetchFragmentHelper : Found init fragment playTarget(%f), playlistPosition(%f)", playTarget, playlistPosition);
				ret = true; // we need to ret success here to avoid failure cases in FetchFragment
			}
			else
			{
				// null fragment URI technically not an error - live manifest may simply not have updated yet
				// if real problem exists, underflow will eventually be detected/reported
				AAMPLOG_INFO("FetchFragmentHelper : fragmentURI %s playTarget(%f), playlistPosition(%f)", fragmentURI, playTarget, playlistPosition);
			}
			return ret;
		}
		return true;
}
/***************************************************************************
* @fn FetchFragment
* @brief Function to fetch fragment
*
* @return void
***************************************************************************/
void TrackState::FetchFragment()
{
	int timeoutMs = -1;
	long http_error = 0;
	double downloadTime = 0;
	bool decryption_error = false;
	if (IsLive())
	{
		timeoutMs = context->maxIntervalBtwPlaylistUpdateMs - (int) (aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
		if(timeoutMs < 0)
		{
			timeoutMs = 0;
		}
	}
	if (!WaitForFreeFragmentAvailable(timeoutMs))
	{
		return;
	}
	//DELIA-33346 -- always set the rampdown flag to false .
	context->mCheckForRampdown = false;
        bool bKeyChanged = false;
        int iFogErrorCode = -1;
	int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
	if (aamp->DownloadsAreEnabled() && !abort)
	{
		if (false == FetchFragmentHelper(http_error, decryption_error,bKeyChanged,&iFogErrorCode, downloadTime))
		{
			if (fragmentURI )
			{
				// DELIA-32287 - Profile RampDown check and rampdown is needed only for Video . If audio fragment download fails
				// should continue with next fragment,no retry needed .
				if (eTRACK_VIDEO == type && http_error != 0 && aamp->CheckABREnabled())
				{
					context->lastSelectedProfileIndex = context->currentProfileIndex;
					// Check whether player reached rampdown limit, then rampdown
					if(!context->CheckForRampDownLimitReached())
					{
						if (context->CheckForRampDownProfile(http_error))
						{
							if (context->rate == AAMP_NORMAL_PLAY_RATE)
							{
								playTarget -= fragmentDurationSeconds;
							}
							else
							{
								playTarget -= context->rate / context->mTrickPlayFPS;
							}
							//DELIA-33346 -- if rampdown attempted , then set the flag so that abr is not attempted.
							context->mCheckForRampdown = true;
							// Rampdown attempt success, download same segment from lower profile.
							mSkipSegmentOnError = false;
						
							AAMPLOG_WARN("Error while fetching fragment:%s, failedCount:%d. decrementing profile", name, segDLFailCount);
						}
						else
						{
							AAMPLOG_WARN("%s Already at the lowest profile, skipping segment", name);
							context->mRampDownCount = 0;
						}
					}
				}
				else if (decryption_error)
				{
					AAMPLOG_WARN("%s Error while decrypting fragments. failedCount:%d", name, segDLFailCount);
				}
				else if (AAMP_IS_LOG_WORTHY_ERROR(http_error))
				{
					AAMPLOG_WARN("Error on fetching %s fragment. failedCount:%d", name, segDLFailCount);
				}

			}
			else
			{
				// technically not an error - live manifest may simply not have updated yet
				// if real problem exists, underflow will eventually be detected/reported
				AAMPLOG_TRACE("NULL fragmentURI for %s track ", name);
			}

			// in case of tsb, GetCurrentBandWidth does not return correct bandwidth as it is updated after this point
			// hence getting from context which is updated in FetchFragmentHelper
			long lbwd = aamp->IsTSBSupported() ? context->GetTsbBandwidth() : this->GetCurrentBandWidth();
			//update videoend info
			aamp->UpdateVideoEndMetrics( (IS_FOR_IFRAME(iCurrentRate,type)? eMEDIATYPE_IFRAME:(MediaType)(type) ),
									lbwd,
									((iFogErrorCode > 0 ) ? iFogErrorCode : http_error),this->mEffectiveUrl,fragmentDurationSeconds,downloadTime, bKeyChanged,fragmentEncrypted);

			return;
		}

		if (mInjectInitFragment)
		{
			return;
		}

		if (eTRACK_VIDEO == type)
		{
			// reset rampdown count on success
			context->mRampDownCount = 0;
		}

		CachedFragment* cachedFragment = GetFetchBuffer(false);
		if (cachedFragment->fragment.ptr)
		{
			double duration = fragmentDurationSeconds;
			double position = playTarget - playTargetOffset;
			if (type == eTRACK_SUBTITLE)
			{
				aamp_AppendNulTerminator(&cachedFragment->fragment);
			}
			if (context->rate == AAMP_NORMAL_PLAY_RATE)
			{
				position -= fragmentDurationSeconds;
				cachedFragment->discontinuity = discontinuity;
			}
			else
			{
				position -= context->rate / context->mTrickPlayFPS;
				cachedFragment->discontinuity = true;
				AAMPLOG_TRACE("rate %f position %f", context->rate, position);
			}

			if (context->trickplayMode && (0 != context->rate))
			{
				duration = (int)(duration*context->rate / context->mTrickPlayFPS);
			}
			cachedFragment->duration = duration;
			cachedFragment->position = position;

			// in case of tsb, GetCurrentBandWidth does not return correct bandwidth as it is updated after this point
			// hence getting from context which is updated in FetchFragmentHelper
			long lbwd = aamp->IsTSBSupported() ? context->GetTsbBandwidth() : this->GetCurrentBandWidth();

			//update videoend info
			aamp->UpdateVideoEndMetrics( (IS_FOR_IFRAME(iCurrentRate,type)? eMEDIATYPE_IFRAME:(MediaType)(type) ),
									lbwd,
									((iFogErrorCode > 0 ) ? iFogErrorCode : http_error),this->mEffectiveUrl,cachedFragment->duration,downloadTime,bKeyChanged,fragmentEncrypted);
		}
		else
		{
			AAMPLOG_WARN("%s cachedFragment->fragment.ptr is NULL", name);
		}
#ifdef AAMP_DEBUG_INJECT
		if ((1 << type) & AAMP_DEBUG_INJECT)
		{
			cachedFragment->uri = fragmentURI;
		}
#endif
		mSkipAbr = false; //To enable ABR since we have cached fragment after init fragment
		UpdateTSAfterFetch();
	}
}
/***************************************************************************
* @fn InjectFragmentInternal
* @brief Injected decrypted fragment for playback
*
* @param cachedFragment[in] CachedFragment structure
* @param fragmentDiscarded[out] bool to indicate fragment successfully injected
* @return void
***************************************************************************/
void TrackState::InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded)
{
#ifndef SUPRESS_DECODE
#ifndef FOG_HAMMER_TEST // support aamp stress-tests of fog without video decoding/presentation
	if (playContext)
	{
		double position = 0;
		if(!context->mStartTimestampZero || streamOutputFormat == FORMAT_ISO_BMFF)
		{
			position = cachedFragment->position;
		}
		fragmentDiscarded = !playContext->sendSegment(cachedFragment->fragment.ptr, cachedFragment->fragment.len,
				position, cachedFragment->duration, cachedFragment->discontinuity, ptsError);
	}
	else
	{
		fragmentDiscarded = false;
		aamp->SendStreamCopy((MediaType)type, cachedFragment->fragment.ptr, cachedFragment->fragment.len,
		        cachedFragment->position, cachedFragment->position, cachedFragment->duration);
	}
#endif
#endif
} // InjectFragmentInternal
/***************************************************************************
* @fn GetCompletionTimeForFragment
* @brief Function to get end time of fragment
*
* @param trackState[in] TrackState structure
* @param mediaSequenceNumber[in] sequence number
* @return double end time
***************************************************************************/
static double GetCompletionTimeForFragment(const TrackState *trackState, long long mediaSequenceNumber)
{
	double rc = 0.0;
	int indexCount = trackState->indexCount; // number of fragments
	if (indexCount>0)
	{
		int idx = (int)(mediaSequenceNumber - trackState->indexFirstMediaSequenceNumber);
		if (idx >= 0)
		{
			if (idx >= indexCount)
			{ // clamp
				idx = indexCount - 1;
			}
			const IndexNode *node = &((IndexNode *)trackState->index.ptr)[idx];
			rc = node->completionTimeSecondsFromStart; // pick up from indexed playlist
		}
		else
		{
			AAMPLOG_WARN("bad index! mediaSequenceNumber=%lld, indexFirstMediaSequenceNumber=%lld", mediaSequenceNumber, trackState->indexFirstMediaSequenceNumber);
		}
	}
	return rc;
}

#ifdef TRACE
/***************************************************************************
* @fn DumpIndex
* @brief Function to log stored media information
*
* @param trackState[in] TrackState structure
* @return void
***************************************************************************/
static void DumpIndex(TrackState *trackState)
{
	AAMPLOG_WARN("index (%d fragments)", trackState->indexCount);
	long long mediaSequenceNumber = trackState->indexFirstMediaSequenceNumber;
	for (int idx = 0; idx < trackState->indexCount; idx++)
	{
		const IndexNode *node = &((IndexNode *)trackState->index.ptr)[idx];
		AAMPLOG_WARN("%lld: %f %d", mediaSequenceNumber, node->completionTimeSecondsFromStart, node->drmMetadataIdx);
		mediaSequenceNumber++;
	}
}
#endif
/***************************************************************************
* @fn FlushIndex
* @brief Function to flush all stored data before refresh and stop
*
* @return void
***************************************************************************/
void TrackState::FlushIndex()
{
	aamp_Free(&index);
	indexFirstMediaSequenceNumber = 0;
	mProgramDateTime = 0.0; // new member - stored first program date time (if any) from playlist
	indexCount = 0;
	index.len = 0;
	index.avail = 0;
	currentIdx = -1;
	mDrmKeyTagCount = 0;
	mLastKeyTagIdx = -1;
	mDeferredDrmKeyMaxTime = 0;
	mKeyHashTable.clear();
	mDiscontinuityIndexCount = 0;
	aamp_Free(&mDiscontinuityIndex);
	memset(&mDiscontinuityIndex, 0, sizeof(mDiscontinuityIndex));
	if (mDrmMetaDataIndexCount)
	{
		AAMPLOG_TRACE("TrackState::[%s]mDrmMetaDataIndexCount %d", name,
		        mDrmMetaDataIndexCount);
		DrmMetadataNode* drmMetadataNode = (DrmMetadataNode*) mDrmMetaDataIndex.ptr;
		assert(NULL != drmMetadataNode);
		for (int i = 0; i < mDrmMetaDataIndexCount; i++)
		{
			AAMPLOG_TRACE("TrackState::drmMetadataNode[%d].metaData.metadataPtr %p",i,
			        drmMetadataNode[i].metaData.metadataPtr);

			if ((NULL == drmMetadataNode[i].metaData.metadataPtr || NULL == drmMetadataNode[i].sha1Hash) && mDrmMetaDataIndexCount)
			{
				AAMPLOG_WARN ("TrackState: **** metadataPtr/sha1Hash is NULL, give attention and analyze it... mDrmMetaDataIndexCount[%d]", mDrmMetaDataIndexCount);
			}

			if (drmMetadataNode[i].metaData.metadataPtr)
			{
				free(drmMetadataNode[i].metaData.metadataPtr);
				drmMetadataNode[i].metaData.metadataPtr = NULL;
			}

			if (drmMetadataNode[i].sha1Hash)
			{
				free(drmMetadataNode[i].sha1Hash);
				drmMetadataNode[i].sha1Hash = NULL;
			}
		}
		aamp_Free(&mDrmMetaDataIndex);
		memset(&mDrmMetaDataIndex, 0, sizeof(mDrmMetaDataIndex));
		mDrmMetaDataIndexCount = 0;
		mDrmMetaDataIndexPosition = 0;
	}
	mInitFragmentInfo = NULL;
}

/***************************************************************************
* @fn ComputeDeferredKeyRequestTime
* @brief Function to compute Deferred key request time for VSS Stream Meta data
***************************************************************************/
void TrackState::ComputeDeferredKeyRequestTime()
{
	// This function will be called only if special tag -X-X1-LIN is present to differ the Key acquisition
	//  on random value based on the Max refresh time interval
	// From gathered information , Meta data gets added to playlist and then Key tag follows after certain time interval
	// defined in X-X1-LIN tag . So if a new Meta appears , before the KeyTag comes up , Key need to be acquired on a random
	// timeout.
	// Assumption 1:There is no deferring required between 2 or more meta present in the playlist if there is equivalent KeyTag
	// 				with Sha is present .For all the Metas with KeyTags , key request will happen immediately
	// Assumption 2 not considered : Not sure if new Meta gets added always at the end or can get it added in any index ,
	//			 either by Fog or Source server. So going worst case, if it can added any position , need to run the loop completely

	bool foundFlag = false;
	if(mDrmMetaDataIndexCount > 1)
	{
		DrmMetadataNode* drmMetadataNode = (DrmMetadataNode*)mDrmMetaDataIndex.ptr;
		int foundMetaCounter = 0;
		for (int idx = 0; idx < mDrmMetaDataIndexCount; idx++)
		{
			// Check if the Meta is already present with AveDrmManager. If there its already configured for deferred time,no need to add again
			// If not present , check if Key Tag is present or not .
			// a)if Key tag present , then no deferring . Key requested immediately.
			// b)if Key tag not present , then calculate deferred time
			if(AveDrmManager::IsMetadataAvailable(drmMetadataNode[idx].sha1Hash) == -1)
			{
				foundFlag = false;
				// checking Hash availability in KeyTags
				KeyHashTableIter iter = mKeyHashTable.begin();
				for(;iter != mKeyHashTable.end();iter++)
				{
					if(iter->mShaID.size() && (0 == memcmp(iter->mShaID.c_str(), drmMetadataNode[idx].sha1Hash , DRM_SHA1_HASH_LEN)))
					{
						// Key tag present , not to defer
						foundFlag = true;
						foundMetaCounter++;
						break;
					}
				}

				if(!foundFlag)
				{
					// Found new Meta with no key Mapping. Need to defer key request for this Meta
					int deferredTimeMs = aamp_GetDeferTimeMs(drmMetadataNode[idx].deferredInterval);
					drmMetadataNode[idx].drmKeyReqTime = aamp_GetCurrentTimeMS() + deferredTimeMs;

					AAMPLOG_WARN("[%s] Found New Meta[%d] without KeyTag mapping.Defer license request[%d]",
						name,idx,deferredTimeMs);
				}
				else
				{
					// This is preventive measure to avoid overloading of DRM and cpu.
					// For any reason process crashes and comes back ,fog may give complete TSB with so many
					// Metadata with KeyTag available. This will cause sudden rush of Meta add and key requset
					// Differ the key in staggered manner. If particular Meta-Key is needed for decrypt, then it will
					// be requested on Emergency mode
					if(foundMetaCounter > 2)
					{
						int deferredTimeMs = aamp_GetDeferTimeMs(30);
						drmMetadataNode[idx].drmKeyReqTime = aamp_GetCurrentTimeMS() + deferredTimeMs;
						AAMPLOG_WARN("[%s] Found New Meta[%d] with KeyTag mapping.Deferring license request due to load[%d]",name,idx,deferredTimeMs);
					}
				}
			}
		}
	}
}

/***************************************************************************
* @fn ProcessDrmMetadata
* @brief Process Drm Metadata after indexing
***************************************************************************/
void TrackState::ProcessDrmMetadata()
{

	// This function after indexplaylist to be called , this will store the metadata into AveDrmManager
	// Storing will not invoke key request . If meta already present , no update happens
	DrmMetadataNode* drmMetadataNode = (DrmMetadataNode*)mDrmMetaDataIndex.ptr;
	if(mDrmMetaDataIndexCount)
	{
		// WARNING ::: Dont put condition here for optimizaion , if Metaavailable !!!!
		// Meta may be added for other track . Also this is the method by which DrmManger knows Meta
		// is still available after refresh . Else it will flush out the Meta from DrmManager thinking
		// Source removed the source
		for (int idx = 0; idx < mDrmMetaDataIndexCount; idx++)
		{
			AAMPLOG_TRACE("[%s] Setting  metadata for index %d/%d",name, idx,mDrmMetaDataIndexCount);
			AveDrmManager::SetMetadata(context->aamp, &drmMetadataNode[idx],(int)type, mLogObj);
		}
	}
}

/***************************************************************************
* @fn InitiateDRMKeyAcquisition
* @brief Function to initiate key request for all Meta data
***************************************************************************/
void TrackState::InitiateDRMKeyAcquisition(int indexPosn)
{
	// WARNING :: Dont put optimization condition here to check if MetaAvailable or KeyAvailable.
	// This is the call by which DrmManager runs the loops to initiate key request for deferred

	// Initiate Key Request will happen after every refresh for all the Meta as there is no pre-refresh data stored to compare
	// Inside AveDrmManager, check is done if Key is already acquired or not . Also if any deferred request is needed or not
	// Second caller of this function is SetDrmContext,if Key is not acquired then for specific meta index key request is made
	//AAMPLOG_WARN("[%s] mDrmMetaDataIndexCount %d ",name, mDrmMetaDataIndexCount);
	DrmMetadataNode* drmMetadataNode = (DrmMetadataNode*)mDrmMetaDataIndex.ptr;
	bool retStatus = true;  //CID:101604 - currentTime is initialized but not used
	// Function to initiate key request with DRM
	// if indexPosn == -1 , its required to check for all the Metadata stored in the list
	// 		this call will be made after playlist update .
	// if indexPosn != -1 , its required to initiate key request immediately for particular Meta
	if(mDrmMetaDataIndexCount > 0)
	{
		// if it is for Adobe DRM only , upfront Key request is done ,
		// If Clear , nothing to do
		if(indexPosn == -1)
		{
			for (int idx = 0; idx < mDrmMetaDataIndexCount; idx++)
			{
				// Request key for all Meta's received in playlist refresh .
				// Deferring logic is with DRM Manager ..lets make it little more intelligent instead of FragCollector
				// doing all checks n calculcation .
				// Every refresh of playlist , DRM Meta will calculate new refresh time and request. DRM Manager knows the
				// first request with deferred time , so it will request key when time comes.
				AAMPLOG_TRACE("[%s]Request DRM Key for indexPosn[%d]",name,idx);
				retStatus = AveDrmManager::AcquireKey(context->aamp, &drmMetadataNode[idx],(int)type,mLogObj);
				if(retStatus == false)
					break;
			}
		}
		else
		{
			// on an emergency may have to request Key for certain index only
			AAMPLOG_WARN("[%s]Request DRM Key immediately for indexPosn[%d]",name,indexPosn);
			retStatus = AveDrmManager::AcquireKey(context->aamp, &drmMetadataNode[indexPosn],(int)type,mLogObj,true);
		}

		if(retStatus == false)
		{
			// Something wrong , why should AveDrmManager return false when Key is requested,
			// May be Meta is not stored before requesting Key or Meta may not be available for ShaId looking for
			AAMPLOG_ERR("[%s] Failure to Get Key ",name);
			aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE, NULL, true);
		}
	}
}

/***************************************************************************
* @fn SetDrmContext
* @brief Function to set DRM Context when KeyTag changes
* @return None
***************************************************************************/
void TrackState::SetDrmContext()
{
	// Set the appropriate DrmContext for Decryption
	// This function need to be called where KeyMethod != None is found after indexplaylist
	// or when new KeyMethod is found , None to AES or between AES with different Method
	// or between KeyMethond when IV or URL changes (for Vanilla AES)

	//CID:93939 - Removed the drmContextUpdated variable which is initialized but not used
	DrmMetadataNode* drmMetadataIdx = (DrmMetadataNode*)mDrmMetaDataIndex.ptr;
	mDrmInfo.bPropagateUriParams = ISCONFIGSET(eAAMPConfig_PropogateURIParam);
	if(drmMetadataIdx)
	{
		AAMPLOG_WARN("TrackState:[%s] Enter mCMSha1Hash [%p] mDrmMetaDataIndexPosition %d",name, mCMSha1Hash,
			mDrmMetaDataIndexPosition);
		// Get the DRM Instance based on current Sha1
		// a) If Multi Key involved mCMSha1Hash will have value ,based on which DRM Instance is picked
		// b) If Single Key invloved (no mCMSha1Hash), but with diff Meta for audio and video , based on track type appropriate DRM instance picked
		// c) If Single Key involved (no mCMSha1Hash) , audio/video having same meta, AveDrmManager will get the the only on Drm Instance
		aamp->setCurrentDrm(std::make_shared<AampAveDrmHelper>(mLogObj));
		mDrm = AveDrmManager::GetAveDrm(mCMSha1Hash,type,mLogObj);		
		if(mDrm && mDrm->GetState() != DRMState::eDRM_KEY_ACQUIRED)
			{
				// Need of the hour ,initiate the key before the decrypt function is called
				AAMPLOG_WARN("[%s] Initiating Key Request as Key is not available for index [%d]",name,mDrmMetaDataIndexPosition);
				InitiateDRMKeyAcquisition(mDrmMetaDataIndexPosition);
			}
	}
#ifdef USE_OPENCDM
	else if (AampHlsDrmSessionManager::getInstance().isDrmSupported(mDrmInfo))
	{
		// OCDM-based DRM decryption is available via the HLS OCDM bridge
		AAMPLOG_INFO("Drm support available");		
		mDrm = AampHlsDrmSessionManager::getInstance().createSession(aamp, mDrmInfo,(MediaType)(type),mLogObj);
		if (!mDrm)
		{
			AAMPLOG_WARN("Failed to create Drm Session");
		}
	}
#endif
	else
	{
		// No DRM helper located, assuming standard AES encryption
#ifdef AAMP_VANILLA_AES_SUPPORT
		AAMPLOG_INFO("StreamAbstractionAAMP_HLS::Get AesDec");
		mDrm = AesDec::GetInstance();
		aamp->setCurrentDrm(std::make_shared<AampVanillaDrmHelper>(mLogObj));
		
#else
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS: AAMP_VANILLA_AES_SUPPORT not defined");
#endif
	}

	if(mDrm)
	{
		mDrm->SetDecryptInfo(aamp, &mDrmInfo, mLogObj);
	}
}

/***************************************************************************
* @fn GetNextLineStart
* @brief Function to get start of the next line
* @param[in] ptr buffer to do the operation
* @return start of the next line
***************************************************************************/
static char* GetNextLineStart(char* ptr)
{
	ptr = strchr(ptr, CHAR_LF);
	if( ptr )
	{
	    ptr++;
	}
	return ptr;
}

/***************************************************************************
* @fn FindLineLength
* @brief Function to get the length of line.
* @param[in] ptr start of line
* @return length of line
***************************************************************************/
static size_t FindLineLength(const char* ptr)
{
	size_t len;
        /*
        Lines in a Playlist file are terminated by either a single line feed
        character or a carriage return character followed by an line feed
        */
	const char * delim = strchr(ptr, CHAR_LF);
	if (delim)
	{
		len = delim-ptr;
		if (delim > ptr && delim[-1] == CHAR_CR)
		{
			len--;
		}
	}
	else
	{
		len = strlen(ptr);
	}
	return len;
}

/***************************************************************************
* @fn IndexPlaylist
* @brief Function to parse playlist
*
* @return double total duration from playlist
***************************************************************************/
void TrackState::IndexPlaylist(bool IsRefresh, double &culledSec)
{
	double totalDuration = 0.0;
	pthread_mutex_lock(&mPlaylistMutex);
	double prevProgramDateTime = mProgramDateTime;
	long long commonPlayPosition = nextMediaSequenceNumber - 1; 
	double prevSecondsBeforePlayPoint; 
	const char *initFragmentPtr = NULL;
	std::string discStr;
	
	if(IsRefresh && !UseProgramDateTimeIfAvailable())
	{
		prevSecondsBeforePlayPoint = GetCompletionTimeForFragment(this, commonPlayPosition); 
	}

	FlushIndex();
	mIndexingInProgress = true;
	if (playlist.ptr )
	{
		char *ptr;
		if(memcmp(playlist.ptr,"#EXTM3U",7)!=0)
		{
		    int tempDataLen = (MANIFEST_TEMP_DATA_LENGTH - 1);
		    char temp[MANIFEST_TEMP_DATA_LENGTH];
		    strncpy(temp, playlist.ptr, tempDataLen);
		    temp[tempDataLen] = 0x00;
		    AAMPLOG_ERR("ERROR: Invalid Playlist URL:%s ", mPlaylistUrl.c_str());
		    AAMPLOG_ERR("ERROR: Invalid Playlist DATA:%s ", temp);
		    aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
		    mDuration = totalDuration;
		    pthread_cond_signal(&mPlaylistIndexed);
		    pthread_mutex_unlock(&mPlaylistMutex);
		    return;
		}
		DrmMetadataNode drmMetadataNode;
		IndexNode node;
		node.completionTimeSecondsFromStart = 0.0;
		node.pFragmentInfo = NULL;
		int drmMetadataIdx = -1;
		//CID:100252,131 , 93918 - Removed the deferDrmVal,endOfTopTags and deferDrmTagPresent variable whcih is initialized but never used
		bool mediaSequence = false;
		const char* programDateTimeIdxOfFragment = NULL;
		bool discontinuity = false;
		bool pdtAtTopAvailable=false;

		mDrmInfo.mediaFormat = eMEDIAFORMAT_HLS;
		mDrmInfo.manifestURL = mEffectiveUrl;
		mDrmInfo.masterManifestURL = aamp->GetManifestUrl();
		mDrmInfo.initData = aamp->GetDrmInitData();
		double fragDuration = 0;
		ptr = GetNextLineStart(playlist.ptr);
		while (ptr)
		{
			if(startswith(&ptr,"#EXT"))
			{
				if (startswith(&ptr,"INF:"))
				{
					if (discontinuity)
					{
						char str[32];
						snprintf(str,32,"[%d]-[%.3f],",indexCount,totalDuration);
						discStr.append(str);
						DiscontinuityIndexNode discontinuityIndexNode;
						discontinuityIndexNode.fragmentIdx = indexCount;
						discontinuityIndexNode.position = totalDuration;
						discontinuityIndexNode.programDateTime = programDateTimeIdxOfFragment;
						discontinuityIndexNode.fragmentDuration = atof(ptr);
						aamp_AppendBytes(&mDiscontinuityIndex, &discontinuityIndexNode, sizeof(DiscontinuityIndexNode));
						mDiscontinuityIndexCount++;
						discontinuity = false;
					}
					programDateTimeIdxOfFragment = NULL;
					node.pFragmentInfo = ptr-8;//Point to beginning of #EXTINF
					fragDuration = atof(ptr);
					// enable logging for all stream type , for top 10 segments .This will help to find diff between
					// playlist
					if(ISCONFIGSET(eAAMPConfig_StreamLogging) && indexCount < 10)
					{
						std::string urlname;
						char *urlstrstart=GetNextLineStart(ptr);
						char *urlstrend=GetNextLineStart(urlstrstart);
						urlname.assign(urlstrstart,(urlstrend-urlstrstart));
						AAMPLOG_WARN("%s [%d]:[%f]:[%f]:%s",name,indexCount,fragDuration,totalDuration,urlname.c_str());
					}
					indexCount++;
					totalDuration += fragDuration;
					node.completionTimeSecondsFromStart = totalDuration;
					node.drmMetadataIdx = drmMetadataIdx;
					node.initFragmentPtr = initFragmentPtr;
					aamp_AppendBytes(&index, &node, sizeof(node));
				}
				else if(startswith(&ptr,"-X-MEDIA-SEQUENCE:"))
				{
					indexFirstMediaSequenceNumber = atoll(ptr);
					mediaSequence = true;
					if(ISCONFIGSET(eAAMPConfig_StreamLogging))
					{
						AAMPLOG_WARN("%s First Media Sequence Number :%lld",name,indexFirstMediaSequenceNumber);
					}
				}
				else if(startswith(&ptr,"-X-TARGETDURATION:"))
				{
					targetDurationSeconds = atof(ptr);
					AAMPLOG_INFO("aamp: EXT-X-TARGETDURATION = %f", targetDurationSeconds);
				}
				else if(startswith(&ptr,"-X-X1-LIN-CK:"))
				{
					// get the deferred drm key acquisition time
					mDeferredDrmKeyMaxTime = atoi(ptr);
					AAMPLOG_INFO("#EXT-X-LIN [%d]",mDeferredDrmKeyMaxTime);
				}
				else if(startswith(&ptr,"-X-PLAYLIST-TYPE:"))
				{
					// EVENT or VOD (optional); VOD if playlist will never change
					if (startswith(&ptr, "VOD"))
					{
						AAMPLOG_WARN("aamp: EXT-X-PLAYLIST-TYPE - VOD");
						mPlaylistType = ePLAYLISTTYPE_VOD;
					}
					else if (startswith(&ptr, "EVENT"))
					{
						AAMPLOG_WARN("aamp: EXT-X-PLAYLIST-TYPE = EVENT");
						mPlaylistType = ePLAYLISTTYPE_EVENT;
					}
					else
					{
						AAMPLOG_ERR("unknown PLAYLIST-TYPE");
					}
				}
				else if(startswith(&ptr,"-X-FAXS-CM:"))
				{
					size_t srcLen;
					AAMPLOG_TRACE("aamp: #EXT-X-FAXS-CM:");
					srcLen = FindLineLength(ptr);
					unsigned char hash[SHA_DIGEST_LENGTH] = {0};
					drmMetadataNode.deferredInterval = mDeferredDrmKeyMaxTime;
					drmMetadataNode.drmKeyReqTime = 0;
					drmMetadataNode.metaData.metadataPtr =  base64_Decode(ptr, &drmMetadataNode.metaData.metadataSize, srcLen);
					SHA1(drmMetadataNode.metaData.metadataPtr, drmMetadataNode.metaData.metadataSize, hash);
					drmMetadataNode.sha1Hash = base16_Encode(hash, SHA_DIGEST_LENGTH);
	#ifdef TRACE
					AAMPLOG_WARN("[%s] drmMetadataNode[%d].sha1Hash -- ", name, mDrmMetaDataIndexCount);
					for (int i = 0; i < DRM_SHA1_HASH_LEN; i++)
					{
						printf("%c", drmMetadataNode.sha1Hash[i]);
					}
					printf("\n");
	#endif
					aamp_AppendBytes(&mDrmMetaDataIndex, &drmMetadataNode, sizeof(drmMetadataNode));
					AAMPLOG_TRACE("mDrmMetaDataIndex.ptr %p", mDrmMetaDataIndex.ptr);
					mDrmMetaDataIndexCount++;
				}
				else if(startswith(&ptr,"-X-DISCONTINUITY-SEQUENCE"))
				{
					// ignore sequence
				}
				else if(startswith(&ptr,"-X-DISCONTINUITY"))
				{
					discontinuity = true;
					if(ISCONFIGSET(eAAMPConfig_StreamLogging))
					{
						AAMPLOG_WARN("%s [%d] Discontinuity Posn : %f ",name,indexCount,totalDuration);
					}
				}
				else if (startswith(&ptr, "-X-PROGRAM-DATE-TIME:"))
				{
					programDateTimeIdxOfFragment = ptr;					
					if(ISCONFIGSET(eAAMPConfig_StreamLogging))
					{
						AAMPLOG_INFO("%s EXT-X-PROGRAM-DATE-TIME: %.*s ",name, 30, programDateTimeIdxOfFragment);
					}
					if(!pdtAtTopAvailable)
					{
						// Fix : mProgramDateTime to be updated only for playlist at top of playlist , not with each segment of Discontinuity						
						if(indexCount)
						{
							// Found PDT in between , not at the top . Need to extrapolate and find the ProgramDateTime of first segment 
							double tmppdt = ISO8601DateTimeToUTCSeconds(ptr);
							mProgramDateTime = tmppdt - totalDuration;
							AAMPLOG_WARN("%s mProgramDateTime From Extrapolation : %f ",name, mProgramDateTime);
						}
						else
						{
							mProgramDateTime = ISO8601DateTimeToUTCSeconds(ptr);
							AAMPLOG_INFO("%s mProgramDateTime From Start : %f ",name, mProgramDateTime);
						}
						pdtAtTopAvailable = true;
					}
					// The first X-PROGRAM-DATE-TIME tag holds the start time for each track
					if (startTimeForPlaylistSync == 0.0 )
					{
						/* discarding timezone assuming audio and video tracks has same timezone and we use this time only for synchronization*/
						startTimeForPlaylistSync = mProgramDateTime; 
						AAMPLOG_WARN("%s StartTimeForPlaylistSync : %f ",name, startTimeForPlaylistSync);
					}
				}
				else if (startswith(&ptr, "-X-KEY:"))
				{
					size_t len;
					AAMPLOG_TRACE("aamp: EXT-X-KEY");
					len = FindLineLength(ptr);
					char* key =(char*) malloc (len+1);
					memcpy(key,ptr,len);
					key[len]='\0';

					// Need to store the Key tag to a list . Needs listed below
					// a) When a new Meta is added , its hash need to be compared
					//with available keytags to determine if its a deferred KeyAcquisition or not(VSS)
					// b) If there is a stream with varying IV in keytag with single Meta,
					// check if during trickplay drmInfo is considered.
					KeyTagStruct keyinfo;
					keyinfo.mKeyStartDuration = totalDuration;
					keyinfo.mKeyTagStr.resize(len);
					memcpy((char*)keyinfo.mKeyTagStr.data(),key,len);

					ParseAttrList(key, ParseKeyAttributeCallback, this);
					//Each fragment should store the corresponding keytag indx to decrypt, MetaIdx may work with
					// adobe mapping , then if or any attribute of Key tag is different ?
					// At present , second Key parsing is done inside GetNextFragmentUriFromPlaylist(that saved)
					//Need keytag idx to pick the corresponding keytag and get drmInfo,so that second parsing can be removed
					//drmMetadataIdx = mDrmMetaDataIndexPosition;
					if(mDrmMethod == eDRM_KEY_METHOD_SAMPLE_AES_CTR){
#ifdef AAMP_HLS_DRM
						if (ISCONFIGSET(eAAMPConfig_Fragmp4PrefetchLicense)){
							pthread_mutex_lock(&aamp->drmParserMutex);
							attrNameData* aesCtrAttrData = new attrNameData(keyinfo.mKeyTagStr); 
							if (std::find(aamp->aesCtrAttrDataList.begin(), aamp->aesCtrAttrDataList.end(), 
									*aesCtrAttrData) == aamp->aesCtrAttrDataList.end()) {
								// attrName not in aesCtrAttrDataList, add it
								aamp->aesCtrAttrDataList.push_back(*aesCtrAttrData);
							}
							/** No more use **/
							SAFE_DELETE(aesCtrAttrData);
							pthread_mutex_unlock(&aamp->drmParserMutex);
							/** Mark as CDM encryption is found in HLS **/
							aamp->fragmentCdmEncrypted = true;
							InitiateDrmProcess(this->aamp);
						}
#endif
					}

					drmMetadataIdx = mDrmKeyTagCount;
					if(!fragmentEncrypted || mDrmMethod == eDRM_KEY_METHOD_SAMPLE_AES_CTR)
					{
						drmMetadataIdx = -1;
						AAMPLOG_TRACE("Not encrypted - fragmentEncrypted %d mCMSha1Hash %p mDrmMethod %d", fragmentEncrypted, mCMSha1Hash, mDrmMethod);
					}

					// mCMSha1Hash is populated after ParseAttrList , hence added here
					if(mCMSha1Hash)
					{
						keyinfo.mShaID.resize(DRM_SHA1_HASH_LEN);
						memcpy((char*)keyinfo.mShaID.data(), mCMSha1Hash, DRM_SHA1_HASH_LEN);
					}
					mKeyHashTable.push_back(keyinfo);
					mKeyTagChanged = false;

					free (key);
					mDrmKeyTagCount++;
				}
				else if(startswith(&ptr,"-X-MAP:"))
				{
					initFragmentPtr = ptr;
					if (mCheckForInitialFragEnc)
					{
						AAMPLOG_TRACE("fragmentEncrypted-%d drmMethod-%d and ptr - %s", fragmentEncrypted, mDrmMethod, ptr);
						// Map tag present indicates ISOBMFF fragments. We need to store an encrypted fragment's init header
						// Ensure order of tags 1. EXT-X-KEY, 2. EXT-X-MAP
						if (fragmentEncrypted && mDrmMethod == eDRM_KEY_METHOD_SAMPLE_AES_CTR && mFirstEncInitFragmentInfo == NULL)
						{
							AAMPLOG_TRACE("mFirstEncInitFragmentInfo - %s", ptr);
							mFirstEncInitFragmentInfo = ptr;
						}
					}
				}
				else if (startswith(&ptr, "-X-START:"))
				{
					// X-Start can have two attributes . Time-Offset & Precise .
					// check if App has not configured any liveoffset
					{
						double offsetval = ParseXStartTimeOffset(ptr);
						if(!aamp->IsLiveAdjustRequired())
						{
							SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_CDVRLiveOffset,offsetval);
						}
						else
						{
							SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_LiveOffset,offsetval);
						}
						aamp->UpdateLiveOffset();
						SetXStartTimeOffset(aamp->mLiveOffset);
					}
				}
				else if (startswith(&ptr,"-X-ENDLIST"))
				{
					// ENDLIST found .Check playlist tag with vod was missing or not.If playlist still undefined
					// mark it as VOD
					if (IsLive())
					{
						//required to avoid live adjust kicking in
						AAMPLOG_WARN("aamp: Changing playlist type from[%d] to ePLAYLISTTYPE_VOD as ENDLIST tag present.",mPlaylistType);
						mPlaylistType = ePLAYLISTTYPE_VOD;
					}
				}
			}
			ptr=GetNextLineStart(ptr);
		}

		if (mDrmMetaDataIndexCount > 1)
		{
			AAMPLOG_WARN("[%d] Indexed %d drm metadata",type, mDrmMetaDataIndexCount);
			AveDrmManager::ApplySessionToken();	
		}

		// DELIA-33434
		// Update DRM Manager for stored indexes so that it can be removed after playlist update
		// Update is required only for multi key stream, where Sha1 is set ,for single key stream,
		// SetMetadata is not called across playlist update , hence Update is not needed
		// Have to check for normal playback only. SAP Audio in VSS sometimes having total different
		// Meta set from video.So when iframe manifest is parsed, audio Meta shouldnt be cleared.
		// Trickplay is for short duration , when back to normal rate,it will flush iframe specific Metas
		if(mCMSha1Hash && context->rate == AAMP_NORMAL_PLAY_RATE)
		{
			AveDrmManager::UpdateBeforeIndexList(name,(int)type);
		}

		if(mediaSequence==false)
		{ // for Sling content
			AAMPLOG_INFO("warning: no EXT-X-MEDIA-SEQUENCE tag");
			ptr = playlist.ptr;
			indexFirstMediaSequenceNumber = 0;
		}
		// DELIA-35008 When setting live status to stream , check the playlist type of both video/audio(demuxed)
		aamp->SetIsLive(context->IsLive());
		if(!IsLive())
		{
			aamp->getAampCacheHandler()->InsertToPlaylistCache(mPlaylistUrl, &playlist, mEffectiveUrl,IsLive(),(MediaType)type);
		}
		if(eTRACK_VIDEO == type)
		{
			aamp->UpdateDuration(totalDuration);
		}

	}
#ifdef TRACE
	DumpIndex(this);
#endif

	if(mDeferredDrmKeyMaxTime != 0)
	{
		// Special stream with Deferred DRM Key Acquisition required
		ComputeDeferredKeyRequestTime();
	}
	// No condition checks for call . All checks n balances inside the module
	// which is been called.
	// Store the all the Metadata received from playlist indexing .
	// IF already stored , AveDrmManager will ignore it
	// ProcessDrmMetadata -> to be called only from one place , after playlist indexing. Not to call from other places
	if(mDrmMethod != eDRM_KEY_METHOD_SAMPLE_AES_CTR)
	{
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
		ProcessDrmMetadata();
		// Initiating key request for Meta present.If already key received ,call will be ignored.
		InitiateDRMKeyAcquisition();
		// default MetaIndex is 0 , for single Meta . If Multi Meta is there ,then Hash is the criteria
		// for selection
		mDrmMetaDataIndexPosition = 0;
	}
	firstIndexDone = true;
	mIndexingInProgress = false;
	AAMPLOG_TRACE("Exit indexCount %d mDrmMetaDataIndexCount %d", indexCount, mDrmMetaDataIndexCount);
	mDuration = totalDuration;
	// DELIA-33434
	// Update is required only for multi key stream, where Sha1 is set ,for single key stream,
	// SetMetadata is not called across playlist update hence flush is not needed
	if(mCMSha1Hash && context->rate == AAMP_NORMAL_PLAY_RATE)
	{
		AveDrmManager::FlushAfterIndexList(name,(int)type);
	}

	if(IsRefresh)
	{
		if(!UseProgramDateTimeIfAvailable())
		{
			double newSecondsBeforePlayPoint = GetCompletionTimeForFragment(this, commonPlayPosition);
			culledSec = prevSecondsBeforePlayPoint - newSecondsBeforePlayPoint;

			if (culledSec > 0)
			{
				// Only positive values
				mCulledSeconds += culledSec;
			}
			else
			{
				culledSec = 0;
			}

			AAMPLOG_INFO("(%s) Prev:%f Now:%f culled with sequence:(%f -> %f) TrackCulled:%f",
				 name, prevSecondsBeforePlayPoint, newSecondsBeforePlayPoint, aamp->culledSeconds,(aamp->culledSeconds+culledSec), mCulledSeconds);
		}
		else if (prevProgramDateTime > 0)
		{
			if(mProgramDateTime > 0)
			{
				culledSec = mProgramDateTime - prevProgramDateTime;
				// Both negative and positive values added
				mCulledSeconds += culledSec;
				AAMPLOG_INFO("(%s) Prev:%f Now:%f culled with ProgramDateTime:(%f -> %f) TrackCulled:%f",
					 name, prevProgramDateTime, mProgramDateTime, aamp->culledSeconds, (aamp->culledSeconds+culledSec),mCulledSeconds);
			}
			else
			{			
				AAMPLOG_INFO("(%s) Failed to read ProgramDateTime:(%f) Retained last PDT (%f) TrackCulled:%f",
					 name, mProgramDateTime, prevProgramDateTime, mCulledSeconds);
				mProgramDateTime = prevProgramDateTime;
			}
		}

		if (culledSec != 0 && discStr.size())
		{
			AAMPLOG_WARN("DISCONTINUITY in track[%d]%s",type,discStr.c_str());
		}

	}
	else  if(discStr.size())
	{
		AAMPLOG_WARN("DISCONTINUITY in track[%d]%s",type,discStr.c_str());
	}
	

	pthread_cond_signal(&mPlaylistIndexed);
	pthread_mutex_unlock(&mPlaylistMutex);
}

/***************************************************************************
* @fn ABRProfileChanged
* @brief Function to handle Profile change after ABR
*
* @return void
***************************************************************************/

void TrackState::ABRProfileChanged()
{
	// If not live, reset play position since sequence number doesn't ensure the fragments
	// from multiple streams are in sync
	const char* pcontext = context->GetPlaylistURI(type);
	if(pcontext != NULL)
	{
		AAMPLOG_TRACE("playlistPosition %f", playlistPosition);
		aamp_ResolveURL(mPlaylistUrl, aamp->GetManifestUrl(), pcontext , ISCONFIGSET(eAAMPConfig_PropogateURIParam));
		pthread_mutex_lock(&mutex);
		//playlistPosition reset will be done by RefreshPlaylist once playlist downloaded successfully
		//refreshPlaylist is used to reset the profile index if playlist download fails! Be careful with it.
		//Video profile change will definitely require new init headers
		mInjectInitFragment = true;
		refreshPlaylist = true;
		/*For some VOD assets, different video profiles have different DRM meta-data.*/
		mForceProcessDrmMetadata = true;
	}
	else
	{
		AAMPLOG_WARN(" GetPlaylistURI  is null");  //CID:83060 - Null Returns
	}
	pthread_mutex_unlock(&mutex);

}
/***************************************************************************
* @fn RefreshPlaylist
* @brief Function to redownload playlist after refresh interval .
*
* @return void
***************************************************************************/
void TrackState::RefreshPlaylist(void)
{
	GrowableBuffer tempBuff;
	long http_error = 0;

	// note: this used to be updated only upon succesful playlist download
	// this can lead to back-to-back playlist download retries
	lastPlaylistDownloadTimeMS = aamp_GetCurrentTimeMS();


	if(playlist.ptr)
	{
		tempBuff.len = playlist.len;
		tempBuff.avail = playlist.avail;
		tempBuff.ptr = playlist.ptr;
		memset(&playlist, 0, sizeof(playlist));
	}
	else
	{
		memset(&tempBuff, 0, sizeof(tempBuff));
	}

	// DELIA-34993 -> Refresh playlist gets called on ABR profile change . For VOD if already present , pull from cache.
	bool bCacheRead = false;
	if (!IsLive())
	{
		bCacheRead = aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(mPlaylistUrl, &playlist, mEffectiveUrl);
	}
	// failed to read from cache , then download it
	if(!bCacheRead)
	{
		if(!ISCONFIGSET(eAAMPConfig_PlaylistParallelRefresh))
		{
			// Lock the mutex if parallel fetch is disabled. So that other thread blocks here
			pthread_mutex_lock(&aamp->mParallelPlaylistFetchLock);
		}

		int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
		//update videoend info
		MediaType actualType = eMEDIATYPE_PLAYLIST_VIDEO;
		if(IS_FOR_IFRAME(iCurrentRate,type))
		{
			actualType = eMEDIATYPE_PLAYLIST_IFRAME;
		}
		else if (type == eTRACK_AUDIO )
		{
			actualType = eMEDIATYPE_PLAYLIST_AUDIO;
		}
		else if (type == eTRACK_SUBTITLE)
		{
			actualType = eMEDIATYPE_PLAYLIST_SUBTITLE;
		}
		else if (type == eTRACK_AUX_AUDIO)
		{
			actualType = eMEDIATYPE_PLAYLIST_AUX_AUDIO;
		}

		double downloadTime;
		AampCurlInstance dnldCurlInstance = aamp->GetPlaylistCurlInstance(actualType, false);
		aamp->SetCurlTimeout(aamp->mPlaylistTimeoutMs,dnldCurlInstance);
		(void) aamp->GetFile (mPlaylistUrl, &playlist, mEffectiveUrl, &http_error, &downloadTime, NULL, (unsigned int)dnldCurlInstance, true, actualType);  //CID:89271 - checked return
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs,dnldCurlInstance);

		if(!ISCONFIGSET(eAAMPConfig_PlaylistParallelRefresh))
		{
			pthread_mutex_unlock(&aamp->mParallelPlaylistFetchLock);
		}
		//To send playback stats with partner apps
		ManifestData manifestData(downloadTime * 1000, playlist.len);
		aamp->UpdateVideoEndMetrics( actualType,
								(this->GetCurrentBandWidth()),
									http_error,mEffectiveUrl, downloadTime, &manifestData);
	}
	if (playlist.len)
	{ // download successful
		//lastPlaylistDownloadTimeMS = aamp_GetCurrentTimeMS();
		if (context->mNetworkDownDetected)
		{
			context->mNetworkDownDetected = false;
		}
		aamp_Free(&tempBuff);
		aamp_AppendNulTerminator(&playlist); // hack: make safe for cstring operations
#ifdef TRACE
		if (gpGlobalConfig->logging.trace)
		{
			printf("***New Playlist:**************\n\n%s\n*************\n", playlist.ptr);
		}
#endif

		double culled=0;
		IndexPlaylist(true, culled);
		// Update culled seconds if playlist download was successful
		// DELIA-40121: We need culledSeconds to find the timedMetadata position in playlist
		// culledSeconds and FindTimedMetadata have been moved up here, because FindMediaForSequenceNumber
		// uses mystrpbrk internally which modifies line terminators in playlist.ptr and results in 
		// FindTimedMetadata failing to parse playlist
		if (IsLive())
		{
			if(eTRACK_VIDEO == type)
			{
				AAMPLOG_INFO("Updating PDT (%f) and culled (%f)",mProgramDateTime,culled); 
				aamp->mProgramDateTime = mProgramDateTime;
				aamp->UpdateCullingState(culled); // report amount of content that was implicitly culled since last playlist download		
			}
			// Metadata refresh is needed for live content only , not for VOD
			// Across ABR , for VOD no metadata change is expected from initial reported ones
			FindTimedMetadata();
		}
	
		if( mDuration > 0.0f )
		{
			if (IsLive())
			{
				fragmentURI = FindMediaForSequenceNumber();
			}
			else
			{
				fragmentURI = playlist.ptr;
				playlistPosition = -1;
			}
			manifestDLFailCount = 0;
		}
	}
	else
	{
		//Restore playlist in case of failure
		if (tempBuff.ptr)
		{
			playlist.ptr = tempBuff.ptr;
			playlist.len = tempBuff.len;
			playlist.avail = tempBuff.avail;
			//Refresh happened due to ABR switching, we need to reset the profileIndex
			//so that ABR can be attempted later
			if (refreshPlaylist)
			{
				context->currentProfileIndex = context->lastSelectedProfileIndex;
			}
		}

		if (aamp->DownloadsAreEnabled())
		{
			if (CURLE_OPERATION_TIMEDOUT == http_error || CURLE_COULDNT_CONNECT == http_error)
			{
				context->mNetworkDownDetected = true;
				AAMPLOG_WARN(" Ignore curl timeout");
				return;
			}
			manifestDLFailCount++;

			if(fragmentURI == NULL && (manifestDLFailCount > MAX_MANIFEST_DOWNLOAD_RETRY))//No more fragments to download
			{
				aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
				return;
			}
		}
	}
}

/***************************************************************************
* @fn FilterAudioCodecBasedOnConfig
* @brief Function to filter the audio codec based on the configuration
*
* @param[in] audioFormat Audio codec type
* @return bool false if the audio codec type is allowed to process
***************************************************************************/
bool StreamAbstractionAAMP_HLS::FilterAudioCodecBasedOnConfig(StreamOutputFormat audioFormat)
{
	bool ignoreProfile = false;
	bool bDisableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
	bool bDisableAC3 = bDisableEC3;
	// bringing in parity with DASH , if EC3 is disabled ,then ATMOS also will be disabled
	bool bDisableATMOS = (bDisableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);

	switch (audioFormat)
	{
		case FORMAT_AUDIO_ES_AC3:
			if (bDisableAC3)
			{
				ignoreProfile = true;
			}
			break;

		case FORMAT_AUDIO_ES_ATMOS:
			if (bDisableATMOS)
			{
				ignoreProfile = true;
			}
			break;

		case FORMAT_AUDIO_ES_EC3:
			if (bDisableEC3)
			{
				ignoreProfile = true;
			}
			break;
			
		default:
			break;
	}

	return ignoreProfile;
}

/***************************************************************************
* @fn GetBestAudioTrackByLanguage
* @brief Function to get best audio track based on the profile availability and language setting.
*
* @return int index of the audio track selected
***************************************************************************/
int StreamAbstractionAAMP_HLS::GetBestAudioTrackByLanguage( void )
{
	int bestTrack = -1;
	int bestScore = -1;
	for( int i=0; i<mMediaCount; i++ )
	{
		if(this->mediaInfo[i].type == eMEDIATYPE_AUDIO)
		{
			int score = 0;
			if (!FilterAudioCodecBasedOnConfig(this->mediaInfo[i].audioFormat))
			{ // allowed codec
				std::string trackLanguage = GetLanguageCode(i);
				if( aamp->preferredLanguagesList.size() > 0 )
				{
					auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), trackLanguage);
					if(iter != aamp->preferredLanguagesList.end())
					{ // track is in preferred language list
						int distance = std::distance(aamp->preferredLanguagesList.begin(),iter);
						score += (aamp->preferredLanguagesList.size()-distance)*100000; // big bonus for language match
					}
				}
				if( !aamp->preferredRenditionString.empty() &&
				   aamp->preferredRenditionString.compare(this->mediaInfo[i].group_id)==0 )
				{
					score += 1000; // medium bonus for rendition match
				}
				if( aamp->preferredCodecList.size() > 0 )
				{
					auto iter = std::find(aamp->preferredCodecList.begin(), aamp->preferredCodecList.end(), GetAudioFormatStringForCodec(this->mediaInfo[i].audioFormat) );
					if(iter != aamp->preferredCodecList.end())
					{ // track is in preferred codec list
						int distance = std::distance(aamp->preferredCodecList.begin(),iter);
						score += (aamp->preferredCodecList.size()-distance)*100; //  bonus for codec match
					}
				}
				else
				{
					score += this->mediaInfo[i].audioFormat; // small bonus for better codecs like ATMOS
				}
				if( this->mediaInfo[i].isDefault || this->mediaInfo[i].autoselect )
				{ // bonus for designated "default"
					score += 10;
				}
			}
		
			AAMPLOG_INFO( "track#%d score = %d", i, score );
			if( score > bestScore )
			{
				bestScore = score;
				bestTrack = i;
			}
		} // next track
	}
	return bestTrack;
}

/***************************************************************************
* @fn GetPlaylistURI
* @brief Function to get playlist URI based on media selection
*
* @param trackType[in] Track type
* @param format[in] stream output type
* @return string playlist URI
***************************************************************************/
const char *StreamAbstractionAAMP_HLS::GetPlaylistURI(TrackType trackType, StreamOutputFormat* format)
{
	const char *playlistURI = NULL;

	switch (trackType)
	{
	case eTRACK_VIDEO:
		{
			HlsStreamInfo *streamInfo  = (HlsStreamInfo *)GetStreamInfo(currentProfileIndex);
			playlistURI = streamInfo->uri;		
			if (format)
			{
				*format = FORMAT_MPEGTS;
			}
		}
		break;
	case eTRACK_AUDIO:
		{
			if (currentAudioProfileIndex >= 0)
			{
				//aamp->UpdateAudioLanguageSelection( GetLanguageCode(currentAudioProfileIndex).c_str() );
				AAMPLOG_WARN("GetPlaylistURI : AudioTrack: language selected is %s", GetLanguageCode(currentAudioProfileIndex).c_str());
				playlistURI = this->mediaInfo[currentAudioProfileIndex].uri;
				mAudioTrackIndex = std::to_string(currentAudioProfileIndex);
				if (format)
				{
					*format = GetStreamOutputFormatForTrack(trackType);
				}
			}
		}
		break;  
	case eTRACK_SUBTITLE:
		{
			if (currentTextTrackProfileIndex != -1)
			{
				playlistURI = mediaInfo[currentTextTrackProfileIndex].uri;
				mTextTrackIndex = std::to_string(currentTextTrackProfileIndex);
				SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_SubTitleLanguage,(std::string)mediaInfo[currentTextTrackProfileIndex].language);
				if (format) *format = (mediaInfo[currentTextTrackProfileIndex].type == eMEDIATYPE_SUBTITLE) ? FORMAT_SUBTITLE_WEBVTT : FORMAT_UNKNOWN;
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: subtitle found language %s, uri %s", mediaInfo[currentTextTrackProfileIndex].language, playlistURI);
			}
			else
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Couldn't find subtitle URI for preferred language: %s", aamp->mSubLanguage.c_str());
				*format = FORMAT_INVALID;
			}
		}
		break;
	case eTRACK_AUX_AUDIO:
		{
			int index = -1;
			// Plain comparison to get the audio track with matching language
			index = GetMediaIndexForLanguage(aamp->GetAuxiliaryAudioLanguage(), trackType);
			if (index != -1)
			{
				playlistURI = mediaInfo[index].uri;
				AAMPLOG_WARN("GetPlaylistURI : Auxiliary Track: Audio selected name is %s", GetLanguageCode(index).c_str());
				//No need to update back, matching track is either there or not
				if (format)
				{
					*format = GetStreamOutputFormatForTrack(trackType);
				}
			}
		}
		break;
	}
	return playlistURI;
}

/***************************************************************************
* @fn GetFormatFromFragmentExtension
* @brief Function to get media format based on fragment extension
*
* @param trackState[in] TrackStatr structure pointer
* @return StreamOutputFormat stream format
***************************************************************************/
static StreamOutputFormat GetFormatFromFragmentExtension(TrackState *trackState)
{
	StreamOutputFormat format = FORMAT_INVALID;
	std::istringstream playlistStream(trackState->playlist.ptr);
	for (std::string line; std::getline(playlistStream, line); )
	{
		if( line.empty() )
		{
			continue;
		}
		if( line[0]=='#' )
		{
			if( line.rfind("#EXT-X-MAP",0) == 0)
			{ // starts-with
				format = FORMAT_ISO_BMFF;
				break;
			}
		}
		else
		{
			while(isspace(line.back()))
			{
				line.pop_back();
				if (line.empty())
				{
				    break;
				}
			}
			if (line.empty())
			{
			    continue;
			}
			AAMPLOG_TRACE("line === %s ====",   line.c_str());
			size_t end = line.find("?");
			if (end != std::string::npos)
			{ // strip any URI paratmeters
				line = line.substr(0, end);
			}
			size_t extenstionStart = line.find_last_of('.');
			if (extenstionStart != std::string::npos)
			{
				std::string extension = line.substr(extenstionStart);
				// parsed extension of first advertised fragment, now compare
				if ( extension == ".ts" )
				{
					format = FORMAT_MPEGTS;
				}
				else if ( extension == ".aac" )
				{
					format = FORMAT_AUDIO_ES_AAC;
				}
				else if ( extension == ".ac3" )
				{
					format = FORMAT_AUDIO_ES_AC3;
				}
				else if ( extension == ".ec3" )
				{
					format = FORMAT_AUDIO_ES_EC3;
				}
				else if ( extension == ".vtt" || extension == ".webvtt" )
				{
					format = FORMAT_SUBTITLE_WEBVTT;
				}
				else
				{
					AAMPLOG_WARN("Not TS or MP4 extension, probably ES. fragment extension %s len %zu", extension.c_str(), strlen(extension.c_str()));
				}
			}
			else
			{
				AAMPLOG_WARN("Could not find extension from line %s", line.c_str());
			}
			break;
		}
	}
	return format;
}

/***************************************************************************
* @fn IsLive
* @brief Function to check if both tracks in demuxed HLS are in live mode
*
* @return True if both or any track in live mode
***************************************************************************/
bool StreamAbstractionAAMP_HLS::IsLive()
{
	// Check for both the tracks if its in Live state
	// In Demuxed content , Hot CDVR playlist update for audio n video happens at a small time delta
	// To avoid missing contents ,until both tracks are not moved to VOD , stream has to be in Live mode
	bool retValIsLive = false;
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			retValIsLive |= track->IsLive();
		}
	}
	return retValIsLive;
}

/***************************************************************************
* @fn CheckDiscontinuityAroundPlaytarget
* @brief Function to update play target based on audio video exact discontinuity positions.
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::CheckDiscontinuityAroundPlaytarget(void)
{
	TrackState *audio = trackState[eMEDIATYPE_AUDIO];
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	DiscontinuityIndexNode* videoDiscontinuityIndex = (DiscontinuityIndexNode*) video->mDiscontinuityIndex.ptr;
	DiscontinuityIndexNode* audioDiscontinuityIndex = (DiscontinuityIndexNode*) audio->mDiscontinuityIndex.ptr;

	for (int i = 0; i < video->mDiscontinuityIndexCount; i++)
	{
		if ((int)videoDiscontinuityIndex[i].position == video->playTarget)
		{
			if (videoDiscontinuityIndex[i].position < audioDiscontinuityIndex[i].position)
			{
				AAMPLOG_WARN("video->playTarget %f -> %f audio->playTarget %f -> %f", 
								video->playTarget, videoDiscontinuityIndex[i].position, audio->playTarget, audioDiscontinuityIndex[i].position);
				video->playTarget = videoDiscontinuityIndex[i].position;
				audio->playTarget = audioDiscontinuityIndex[i].position;
			}
			else
			{
				AAMPLOG_WARN("video->playTarget %f -> %d audio->playTarget %f -> %d", 
								video->playTarget, (int)audioDiscontinuityIndex[i].position, audio->playTarget, (int)audioDiscontinuityIndex[i].position);
				video->playTarget = audio->playTarget = (int)audioDiscontinuityIndex[i].position;
			}

			break;
		}
	}
}


/***************************************************************************
* @fn SyncTracksForDiscontinuity
* @brief Function to synchronize time between audio & video for VOD stream
*
* @return eAAMPSTATUS_OK on success
***************************************************************************/
AAMPStatusType StreamAbstractionAAMP_HLS::SyncTracksForDiscontinuity()
{
	TrackState *audio = trackState[eMEDIATYPE_AUDIO];
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	TrackState *aux = NULL;
	if (!audio->enabled)
	{
		AAMPLOG_WARN("Attempting to sync between muxed track and auxiliary audio track");
		audio = trackState[eMEDIATYPE_AUX_AUDIO];
	}
	else
	{
		aux = trackState[eMEDIATYPE_AUX_AUDIO];
	}
	AAMPStatusType retVal = eAAMPSTATUS_GENERIC_ERROR;

	double roundedPlayTarget = std::round(video->playTarget);
	// Offset value to add . By default it will playtarget
	double offsetVideoToAdd = roundedPlayTarget;
	double offsetAudioToAdd = roundedPlayTarget;
	// Start of audio and Period for current Period and previous period
	double audioPeriodStartCurrentPeriod = 0.0; 
	double videoPeriodStartCurrentPeriod = 0.0;
	double audioPeriodStartPrevPeriod = 0.0;
	double videoPeriodStartPrevPeriod = 0.0;

	int discIndex = -1;

	if (audio->GetNumberOfPeriods() != video->GetNumberOfPeriods())
	{
		AAMPLOG_WARN("WARNING audio's number of period %d video number of period: %d", audio->GetNumberOfPeriods(), video->GetNumberOfPeriods());
	}

	if (video->playTarget !=0)
	{
		/*If video playTarget is just before a discontinuity, move playTarget to the discontinuity position*/
		DiscontinuityIndexNode* videoDiscontinuityIndex = (DiscontinuityIndexNode*) video->mDiscontinuityIndex.ptr;
		DiscontinuityIndexNode* audioDiscontinuityIndex = (DiscontinuityIndexNode*) audio->mDiscontinuityIndex.ptr;	

		for (int i = 0; i < video->mDiscontinuityIndexCount; i++)
		{
			double roundedIndexPosition = std::round(videoDiscontinuityIndex[i].position);
			double roundedFragDurtion = std::round(videoDiscontinuityIndex[i].fragmentDuration);
			// check if playtarget is on discontinuity , around it or away from discontinuity
			double diff = (roundedIndexPosition - roundedPlayTarget);
			
			videoPeriodStartCurrentPeriod = videoDiscontinuityIndex[i].position;
			audioPeriodStartCurrentPeriod = audioDiscontinuityIndex[i].position;
	
			// if play position is same as start of discontinuity , just start there , no checks 
			// if play position is within fragmentduration window , just start at discontinuity
			if(fabs(diff) <= roundedFragDurtion || diff == 0)
			{
				// in this case , no offset to add . On discontinuity index position 
				offsetVideoToAdd = offsetAudioToAdd = 0; 
				AAMPLOG_WARN ("PlayTarget around the discontinuity window,rounding position to discontinuity index");//,videoPeriodStartCurrentPeriod,audioPeriodStartCurrentPeriod);				
				break;
			}
			else if(diff < 0 )
			{
				// this case : playtarget is after the discontinuity , but not sure if this is within
				// current period . 
				offsetVideoToAdd = (roundedPlayTarget - roundedIndexPosition);
				offsetAudioToAdd = (roundedPlayTarget - std::round(audioDiscontinuityIndex[i].position));
				// Not sure if this is last period or not ,so update the Offset 
			}
			else if(diff > 0 )
			{
				// this case : discontinuity Index is after the playtarget
				// need to break the loop. Before that get offset with ref to prev period
				audioPeriodStartCurrentPeriod  = audioPeriodStartPrevPeriod;
				videoPeriodStartCurrentPeriod  = videoPeriodStartPrevPeriod; 
				// Get offset from last period start 
				offsetVideoToAdd = (roundedPlayTarget - std::round(videoPeriodStartPrevPeriod));
				offsetAudioToAdd = (roundedPlayTarget - std::round(audioPeriodStartPrevPeriod));
				break;
			}
			// store the current period as prev period before moving to next
			videoPeriodStartPrevPeriod = videoPeriodStartCurrentPeriod;
			audioPeriodStartPrevPeriod = audioPeriodStartCurrentPeriod;
		}

		// Calculate Audio and Video playtarget 
		audio->playTarget = audioPeriodStartCurrentPeriod + offsetAudioToAdd;
		video->playTarget = videoPeriodStartCurrentPeriod + offsetVideoToAdd;
		// Based on above playtarget , find the exact segment to pick to reduce audio loss
		{
			int periodIdx;
			double offsetFromPeriod;
			double audioOffsetFromPeriod;
			int fragmentIdx;
			video->GetNextFragmentPeriodInfo (periodIdx, offsetFromPeriod, fragmentIdx);

			if(-1 != periodIdx)
			{
				double audioPeriodStart = audio->GetPeriodStartPosition(periodIdx);
				double videoPeriodStart = video->GetPeriodStartPosition(periodIdx);
				int audioFragmentIdx;

				audio->GetNextFragmentPeriodInfo (periodIdx, audioOffsetFromPeriod, audioFragmentIdx);

				AAMPLOG_WARN("video periodIdx: %d, video-offsetFromPeriod: %f, videoPeriodStart: %f, audio-offsetFromPeriod: %f, audioPeriodStart: %f",
								periodIdx, offsetFromPeriod, videoPeriodStart, audioOffsetFromPeriod, audioPeriodStart);

				if (0 != audioPeriodStart)
				{
					if ((fragmentIdx != -1) && (audioFragmentIdx != -1) && (fragmentIdx != audioFragmentIdx) && ((int)audioPeriodStart == (int)videoPeriodStart))
					{
						if (audioPeriodStart > videoPeriodStart)
						{
							audio->playTarget = audioPeriodStart + audioOffsetFromPeriod;
							video->playTarget = videoPeriodStart + audioOffsetFromPeriod;
							AAMPLOG_WARN("(audio > video) - vid start: %f, audio start: %f", video->playTarget, audio->playTarget );
						}
						else
						{
							audio->playTarget = audioPeriodStart + offsetFromPeriod;
							video->playTarget = videoPeriodStart + offsetFromPeriod;
							AAMPLOG_WARN("(video > audio) - vid start: %f, audio start: %f",  video->playTarget, audio->playTarget );
						}
					}
					else
					{
						audio->playTarget = audioPeriodStart + audioOffsetFromPeriod;
						video->playTarget = videoPeriodStart + offsetFromPeriod;
						AAMPLOG_WARN("(audio != video) - vid start: %f, audio start: %f", video->playTarget, audio->playTarget );
					}

					SeekPosUpdate(video->playTarget);					
					
					AAMPLOG_WARN("VP: %f, AP: %f, seek_pos_seconds changed to %f based on video playTarget", video->playTarget, audio->playTarget, seekPosition);

					retVal = eAAMPSTATUS_OK;
				}
				else
				{
					AAMPLOG_WARN("audioDiscontinuityOffset: 0");
				}
			}
			else
			{
				AAMPLOG_WARN("WARNING audio's number of period %d subtitle number of period %d", audio->GetNumberOfPeriods(), subtitle->GetNumberOfPeriods());
			}
		}

		//RDK-27996, lets go with a simple sync operation for the moment for subtitle and aux
		for (int index = eMEDIATYPE_SUBTITLE; index <= eMEDIATYPE_AUX_AUDIO; index++)
		{
			TrackState *track = trackState[index];
			if (index == eMEDIATYPE_AUX_AUDIO && !trackState[eMEDIATYPE_AUDIO]->enabled)
			{
				// Case of muxed track and separate aux track - its already sync'ed
				break;
			}
			if (track->enabled)
			{
				if (audio->GetNumberOfPeriods() == track->GetNumberOfPeriods())
				{
					int periodIdx;
					double offsetFromPeriod;
					int audioFragmentIdx;
					audio->GetNextFragmentPeriodInfo(periodIdx, offsetFromPeriod, audioFragmentIdx);
					if (-1 != periodIdx)
					{
						AAMPLOG_WARN("audio periodIdx: %d, offsetFromPeriod: %f", periodIdx, offsetFromPeriod);
						double trackPeriodStart = track->GetPeriodStartPosition(periodIdx);
						if (0 != trackPeriodStart)
						{
							track->playTarget = trackPeriodStart + offsetFromPeriod;
						}
						else
						{
							AAMPLOG_WARN("subtitleDiscontinuityOffset: 0");
						}
					}
				}
				else
				{
					AAMPLOG_WARN("WARNING audio's number of period %d, %s number of period: %d",
							audio->GetNumberOfPeriods(), track->name, track->GetNumberOfPeriods());
				}
			}
		}

		if (!trackState[eMEDIATYPE_AUDIO]->enabled)
		{
			AAMPLOG_WARN("Exit : aux track start %f, muxed track start %f sub track start %f",
					audio->playTarget, video->playTarget, subtitle->playTarget);
		}
		else if (aux)
		{
			AAMPLOG_WARN("Exit : audio track start %f, vid track start %f sub track start %f aux track start %f",
					audio->playTarget, video->playTarget, subtitle->playTarget, aux->playTarget);
		}
	}

	return retVal;
}

/***************************************************************************
* @fn SyncTracks
* @brief Function to synchronize time between A/V for Live/Event assets
* @param useProgramDateTimeIfAvailable use program date time tag to sync if available
* @return eAAMPSTATUS_OK on success
***************************************************************************/
AAMPStatusType StreamAbstractionAAMP_HLS::SyncTracks(void)
{
	bool useProgramDateTimeIfAvailable = UseProgramDateTimeIfAvailable();
	AAMPStatusType retval = eAAMPSTATUS_OK;
	bool startTimeAvailable = true;
	bool syncedUsingSeqNum = false;
	long long mediaSequenceNumber[AAMP_TRACK_COUNT];
	TrackState *audio = trackState[eMEDIATYPE_AUDIO];
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	TrackState *aux = NULL;
	double diffBetweenStartTimes = 0.0;

	for(int i = 0; i<AAMP_TRACK_COUNT; i++)
	{
		TrackState *ts = trackState[i];
		if (ts->enabled)
		{
			ts->fragmentURI = trackState[i]->GetNextFragmentUriFromPlaylist(true); //To parse track playlist
			/*Update playTarget to playlistPostion to correct the seek position to start of a fragment*/
			ts->playTarget = ts->playlistPosition;
			AAMPLOG_WARN("syncTracks loop : track[%d] pos %f start %f frag-duration %f trackState->fragmentURI %s ts->nextMediaSequenceNumber %lld", i, ts->playlistPosition, ts->playTarget, ts->fragmentDurationSeconds, ts->fragmentURI, ts->nextMediaSequenceNumber);
			if (ts->startTimeForPlaylistSync == 0.0 )
			{
				AAMPLOG_WARN("startTime not available for track %d", i);
				startTimeAvailable = false;
			}
			mediaSequenceNumber[i] = ts->nextMediaSequenceNumber - 1;
		}
	}

	if (audio->enabled)
	{
		aux = trackState[eMEDIATYPE_AUX_AUDIO];
	}
	else
	{
		mediaSequenceNumber[eMEDIATYPE_AUDIO] = mediaSequenceNumber[eMEDIATYPE_AUX_AUDIO];
		audio = trackState[eMEDIATYPE_AUX_AUDIO];
	}

	if (startTimeAvailable)
	{
		//Logging irregularities in the playlist for debugging purposes
		diffBetweenStartTimes = audio->startTimeForPlaylistSync - video->startTimeForPlaylistSync;
		AAMPLOG_WARN("Difference in PDT between A/V: %f Audio:%f Video:%f ", diffBetweenStartTimes, audio->startTimeForPlaylistSync,
								video->startTimeForPlaylistSync);
		if (!useProgramDateTimeIfAvailable)
		{
			if (video->targetDurationSeconds != audio->targetDurationSeconds)
			{
				AAMPLOG_WARN("WARNING seqno based track synchronization when video->targetDurationSeconds[%f] != audio->targetDurationSeconds[%f]",
				        video->targetDurationSeconds, audio->targetDurationSeconds);
			}
			else
			{
				double diffBasedOnSeqNumber = (mediaSequenceNumber[eMEDIATYPE_AUDIO]
				        - mediaSequenceNumber[eMEDIATYPE_VIDEO]) * video->fragmentDurationSeconds;
				if (fabs(diffBasedOnSeqNumber - diffBetweenStartTimes) > video->fragmentDurationSeconds)
				{
					AAMPLOG_WARN("WARNING - inconsistency between startTime and seqno  startTime diff %f diffBasedOnSeqNumber %f",
					        diffBetweenStartTimes, diffBasedOnSeqNumber);
				}
			}
		}

		if((diffBetweenStartTimes < -10 || diffBetweenStartTimes > 10))
		{
			AAMPLOG_WARN("syncTracks diff debug : Audio start time : %f  Video start time : %f ",
					audio->startTimeForPlaylistSync, video->startTimeForPlaylistSync );
		}
	}

	//Sync using sequence number since startTime is not available or not desired
	if (!startTimeAvailable || !useProgramDateTimeIfAvailable)
	{
		MediaType mediaType;
#ifdef TRACE
		AAMPLOG_WARN("sync using sequence number. A %lld V %lld a-f-uri %s v-f-uri %s", mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[eMEDIATYPE_VIDEO],
				audio->fragmentURI, video->fragmentURI);
#endif
		TrackState *laggingTS = NULL;
		long long diff = 0;
		if (mediaSequenceNumber[eMEDIATYPE_AUDIO] > mediaSequenceNumber[eMEDIATYPE_VIDEO])
		{
			laggingTS = video;
			diff = mediaSequenceNumber[eMEDIATYPE_AUDIO] - mediaSequenceNumber[eMEDIATYPE_VIDEO];
			mediaType = eMEDIATYPE_VIDEO;
			AAMPLOG_WARN("video track lag in seqno. diff %lld", diff);
		}
		else if (mediaSequenceNumber[eMEDIATYPE_VIDEO] > mediaSequenceNumber[eMEDIATYPE_AUDIO])
		{
			laggingTS = audio;
			diff = mediaSequenceNumber[eMEDIATYPE_VIDEO] - mediaSequenceNumber[eMEDIATYPE_AUDIO];
			mediaType = eMEDIATYPE_AUDIO;
			AAMPLOG_WARN("audio track lag in seqno. diff %lld", diff);
		}
		if (laggingTS)
		{
			if (startTimeAvailable && (diff > MAX_SEQ_NUMBER_DIFF_FOR_SEQ_NUM_BASED_SYNC))
			{
				AAMPLOG_WARN("falling back to synchronization based on start time as diff = %lld", diff);
			}
			else if ((diff <= MAX_SEQ_NUMBER_LAG_COUNT) && (diff > 0))
			{
				AAMPLOG_WARN("sync using sequence number. diff [%lld] A [%lld] V [%lld] a-f-uri [%s] v-f-uri [%s]",
						diff, mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[eMEDIATYPE_VIDEO],
						audio->fragmentURI, video->fragmentURI);
				while (diff > 0)
				{
					laggingTS->playTarget += laggingTS->fragmentDurationSeconds;
					laggingTS->playTargetOffset += laggingTS->fragmentDurationSeconds;
					if (laggingTS->fragmentURI)
					{
						laggingTS->fragmentURI = laggingTS->GetNextFragmentUriFromPlaylist(true);
					}
					else
					{
						AAMPLOG_WARN("laggingTS->fragmentURI NULL, seek might be out of window");
					}
					diff--;
				}
				syncedUsingSeqNum = true;
			}
			else
			{
				AAMPLOG_WARN("Lag in '%s' seq no, diff[%lld] > maxValue[%d]",
						((eMEDIATYPE_VIDEO == mediaType) ? "video" : "audio"), diff, MAX_SEQ_NUMBER_LAG_COUNT);
			}
		}
		else
		{
			AAMPLOG_WARN("No lag in seq no b/w AV");
			syncedUsingSeqNum = true;
		}

		//RDK-27996, lets go with a simple sync operation for the moment for subtitle and aux
		for (int index = eMEDIATYPE_SUBTITLE; (syncedUsingSeqNum && index <= eMEDIATYPE_AUX_AUDIO); index++)
		{
			TrackState *track = trackState[index];
			if (index == eMEDIATYPE_AUX_AUDIO && !trackState[eMEDIATYPE_AUDIO]->enabled)
			{
				// Case of muxed track and separate aux track and its already sync'ed
				break;
			}
			if (track->enabled)
			{
				long long diff = mediaSequenceNumber[eMEDIATYPE_AUDIO] - mediaSequenceNumber[index];
				//We can only support track to catch-up to audio. The opposite will cause a/v sync issues
				if (diff > 0 && diff <= MAX_SEQ_NUMBER_LAG_COUNT)
				{
					AAMPLOG_WARN("sync %s using sequence number. diff [%lld] A [%lld] T [%lld] a-f-uri [%s] t-f-uri [%s]",
							track->name, diff, mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[index],
							audio->fragmentURI, track->fragmentURI);
					//Track catch up to audio
					while (diff > 0)
					{
						track->playTarget += track->fragmentDurationSeconds;
						track->playTargetOffset += track->fragmentDurationSeconds;
						if (track->fragmentURI)
						{
							track->fragmentURI = track->GetNextFragmentUriFromPlaylist();
						}
						else
						{
							AAMPLOG_WARN("%s fragmentURI NULL, seek might be out of window", track->name);
						}
						diff--;
					}
				}
				else if (diff < 0)
				{
					//Audio can't catch up with track, since its already sync-ed with video.
					AAMPLOG_WARN("sync using sequence number failed, %s will be starting late. diff [%lld] A [%lld] T [%lld] a-f-uri [%s] t-f-uri [%s]",
							track->name, diff, mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[index],
							audio->fragmentURI, track->fragmentURI);
				}
				else
				{
					AAMPLOG_WARN("No lag in seq no b/w audio and %s", track->name);
				}
			}
		}
	}

	if (!syncedUsingSeqNum)
	{
		if (startTimeAvailable)
		{
			if (diffBetweenStartTimes > 0)
			{
				TrackState *ts = trackState[eMEDIATYPE_VIDEO];
				if (diffBetweenStartTimes > (ts->fragmentDurationSeconds / 2))
				{
					if (video->mDuration > (ts->playTarget + diffBetweenStartTimes))
					{
						ts->playTarget += diffBetweenStartTimes;
						ts->playTargetOffset = diffBetweenStartTimes;
						AAMPLOG_WARN("Audio track in front, catchup videotrack video playTarget:%f playTargetOffset:%f",ts->playTarget ,ts->playTargetOffset);
					}
					else
					{
						AAMPLOG_WARN("invalid diff %f ts->playTarget %f trackDuration %f",
							diffBetweenStartTimes, ts->playTarget, video->mDuration);
						retval = eAAMPSTATUS_TRACKS_SYNCHRONISATION_ERROR;
					}
				}
				else
				{
					AAMPLOG_WARN("syncTracks : Skip playTarget updation diff %f, vid track start %f fragmentDurationSeconds %f",
					        diffBetweenStartTimes, ts->playTarget, ts->fragmentDurationSeconds);
				}
			}
			else if (diffBetweenStartTimes < 0)
			{
				TrackState *ts = trackState[eMEDIATYPE_AUDIO];
				if (fabs(diffBetweenStartTimes) > (ts->fragmentDurationSeconds / 2))
				{
					if (audio->mDuration > (ts->playTarget - diffBetweenStartTimes))
					{
						ts->playTarget -= diffBetweenStartTimes;
						ts->playTargetOffset = -diffBetweenStartTimes;
						AAMPLOG_WARN("Video track in front, catchup audiotrack audio playTarget:%f playTargetOffset:%f",ts->playTarget ,ts->playTargetOffset);
					}
					else
					{
						AAMPLOG_ERR("invalid diff %f ts->playTarget %f trackDuration %f",
						        diffBetweenStartTimes, ts->playTarget, audio->mDuration);
						retval = eAAMPSTATUS_TRACKS_SYNCHRONISATION_ERROR;
					}
				}
				else
				{
					AAMPLOG_WARN("syncTracks : Skip playTarget updation diff %f, aud track start %f fragmentDurationSeconds %f",
					        fabs(diffBetweenStartTimes), ts->playTarget, ts->fragmentDurationSeconds);
				}
			}

			//RDK-27996, lets go with a simple sync operation for the moment for subtitle and aux
			for (int index = eMEDIATYPE_SUBTITLE; (syncedUsingSeqNum && index <= eMEDIATYPE_AUX_AUDIO); index++)
			{
				TrackState *track =  trackState[index];
				if (index == eMEDIATYPE_AUX_AUDIO && !trackState[eMEDIATYPE_AUDIO]->enabled)
				{
					// Case of muxed track and separate aux track and its already sync'ed
					break;
				}
				if (track->enabled)
				{
					//Compare track and audio start time
					const double diff = audio->startTimeForPlaylistSync - subtitle->startTimeForPlaylistSync;
					if (diff > 0)
					{
						//Audio is at a higher start time that track. Track needs to catch-up
						if (diff > (track->fragmentDurationSeconds / 2))
						{
							if (track->mDuration > (track->playTarget + diff))
							{	
								track->playTarget += diff;
								track->playTargetOffset = diff;
								AAMPLOG_WARN("Audio track in front, catchup %s playTarget:%f playTargetOffset:%f",
										track->name,
										track->playTarget, track->playTargetOffset);
							}
							else
							{
								AAMPLOG_WARN("invalid diff(%f) greater than duration, ts->playTarget %f trackDuration %f, %s may start early",
										track->name, diff, track->playTarget, track->mDuration);
							}
						}
						else
						{
							AAMPLOG_WARN("syncTracks : Skip %s playTarget updation diff %f, track start %f fragmentDurationSeconds %f",
									track->name, diff, track->playTarget, track->fragmentDurationSeconds);
						}
					}
					else if (diff < 0)
					{
						//Can't catch-up audio to subtitle, since audio and video are already sync-ed
						AAMPLOG_WARN("syncTracks : Skip %s sync to audio for subtitle startTime %f, audio startTime %f. Subtitle will be starting late",
								track->name, track->startTimeForPlaylistSync, audio->startTimeForPlaylistSync);
					}
				}
			}
		}
		else
		{
			AAMPLOG_ERR("Could not sync using seq num and start time not available., cannot play this content.!!");
			retval = eAAMPSTATUS_TRACKS_SYNCHRONISATION_ERROR;
		}
	}
	// New calculated playTarget assign back for buffer calculation
	video->playTargetBufferCalc = video->playTarget;
	if (!trackState[eMEDIATYPE_AUDIO]->enabled)
	{
		AAMPLOG_WARN("Exit : aux track start %f, muxed track start %f sub track start %f",
				audio->playTarget, video->playTarget, subtitle->playTarget);
	}
	else if (aux)
	{
		AAMPLOG_WARN("Exit : audio track start %f, vid track start %f sub track start %f aux track start %f",
				audio->playTarget, video->playTarget, subtitle->playTarget, aux->playTarget);
	}

	return retval;
}

std::string StreamAbstractionAAMP_HLS::GetLanguageCode(int iMedia)
{
	std::string lang = this->mediaInfo[iMedia].language;
	lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
	return lang;
}

/***************************************************************************
* @fn Init
* @brief Function to initialize member variables,download main manifest and parse
*
* @param tuneType[in] Tune type
* @return bool true on success
***************************************************************************/
AAMPStatusType StreamAbstractionAAMP_HLS::Init(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_GENERIC_ERROR;
	bool needMetadata = true;
	mTuneType = tuneType;
	bool newTune = aamp->IsNewTune();
    /* START: Added As Part of DELIA-28363 and DELIA-28247 */
	aamp->IsTuneTypeNew = newTune;
    /* END: Added As Part of DELIA-28363 and DELIA-28247 */

	TSProcessor* audioQueuedPC = NULL;
	long http_error = 0;   //CID:81873 - Initialization
	memset(&mainManifest, 0, sizeof(mainManifest));
	if (newTune)
	{
		AveDrmManager::ResetAll();
	}

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs, (AampCurlInstance)i);
	}

	if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(aamp->GetManifestUrl(), &mainManifest, aamp->GetManifestUrl()))
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Main manifest retrieved from cache");
	}
	
	bool updateVideoEndMetrics = false;
	double mainManifestdownloadTime;
	int parseTimeMs = 0;
	if (!this->mainManifest.len)
	{
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_MANIFEST);
		AAMPLOG_TRACE("StreamAbstractionAAMP_HLS::downloading manifest");
		// take the original url before its gets changed in GetFile
		std::string mainManifestOrigUrl = aamp->GetManifestUrl();
		double downloadTime;
		aamp->SetCurlTimeout(aamp->mManifestTimeoutMs, eCURLINSTANCE_MANIFEST_PLAYLIST);
		(void) aamp->GetFile(aamp->GetManifestUrl(), &this->mainManifest, aamp->GetManifestUrl(), &http_error, &mainManifestdownloadTime, NULL, eCURLINSTANCE_MANIFEST_PLAYLIST, true, eMEDIATYPE_MANIFEST);//CID:82578 - checked return
		updateVideoEndMetrics = true;
		aamp->SetCurlTimeout(aamp->mPlaylistTimeoutMs, eCURLINSTANCE_MANIFEST_PLAYLIST);
		if (this->mainManifest.len)
		{
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
			AAMPLOG_TRACE("StreamAbstractionAAMP_HLS::downloaded manifest");
			aamp->getAampCacheHandler()->InsertToPlaylistCache(mainManifestOrigUrl, &mainManifest, aamp->GetManifestUrl(),false,eMEDIATYPE_MANIFEST);
		}
		else
		{
			aamp->UpdateDuration(0);
			AAMPLOG_ERR("Manifest download failed : http response : %d", (int) http_error);
			retval = eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		}
	}
	if (!this->mainManifest.len && aamp->DownloadsAreEnabled()) //!aamp->GetFile(aamp->GetManifestUrl(), &this->mainManifest, aamp->GetManifestUrl()))
	{
		aamp->profiler.ProfileError(PROFILE_BUCKET_MANIFEST, http_error);
		aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
		aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
	}
	if (this->mainManifest.len)
	{
		aamp_AppendNulTerminator(&this->mainManifest); // make safe for cstring operations
		if (gpGlobalConfig->logging.trace )
		{
			printf("***Main Manifest***:\n\n%s\n************\n", this->mainManifest.ptr);
		}

#ifdef AAMP_HLS_DRM
		AampDRMSessionManager *sessionMgr = aamp->mDRMSessionManager;
		bool forceClearSession = (!ISCONFIGSET(eAAMPConfig_SetLicenseCaching) && (tuneType == eTUNETYPE_NEW_NORMAL));
		sessionMgr->clearDrmSession(forceClearSession);
		sessionMgr->clearFailedKeyIds();
		sessionMgr->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
		sessionMgr->setLicenseRequestAbort(false);
#endif
		// Parse the Main manifest ( As Parse function modifies the original data,InsertCache had to be called before it . 
		long long tStartTime = NOW_STEADY_TS_MS;
		AAMPStatusType mainManifestResult = ParseMainManifest();
		parseTimeMs = NOW_STEADY_TS_MS - tStartTime;
		// Check if Main manifest is good or not 
		if(mainManifestResult != eAAMPSTATUS_OK)
		{
			if(mainManifestResult == eAAMPSTATUS_PLAYLIST_PLAYBACK)
			{ // RDK-28245 - support tune to playlist, without main manifest
				if(mProfileCount == 0)
				{
					struct HlsStreamInfo *streamInfo = &this->streamInfo[mProfileCount];
					setupStreamInfo(streamInfo, mProfileCount);
					streamInfo->uri = aamp->GetManifestUrl().c_str();
					SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_EnableABR,false);
					mainManifestResult = eAAMPSTATUS_OK;
					AAMPLOG_INFO("StreamAbstractionAAMP_HLS::Playlist only playback.");
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Invalid manifest format.");
					mainManifestResult = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
				}
			}
			// check for the error type , if critical error return immediately
			if(mainManifestResult == eAAMPSTATUS_MANIFEST_CONTENT_ERROR || mainManifestResult == eAAMPSTATUS_MANIFEST_PARSE_ERROR)
			{
				// Dump the invalid manifest content before reporting error
				int tempDataLen = (MANIFEST_TEMP_DATA_LENGTH - 1);
				char temp[MANIFEST_TEMP_DATA_LENGTH];
				strncpy(temp, this->mainManifest.ptr, tempDataLen);
				temp[tempDataLen] = 0x00;
				// this will print only one line :(
				printf("ERROR: Invalid Main Manifest : %s \n", temp);
				return mainManifestResult;
			}
		}

		if(mProfileCount)
		{
			if (!newTune)
			{
				long persistedBandwidth = aamp->GetPersistedBandwidth();
				long defaultBitRate 	= aamp->GetDefaultBitrate();
				//We were tuning to a lesser profile previously, so we use it as starting profile
				// XIONE-2039 If bitrate to be persisted during trickplay is true, set persisted BW as default init BW
				if (persistedBandwidth > 0 && (persistedBandwidth < defaultBitRate || aamp->IsBitRatePersistedOverSeek()))
				{
					mAbrManager.setDefaultInitBitrate(persistedBandwidth);
				}
			}

			if(rate == AAMP_NORMAL_PLAY_RATE)
			{
				// Step 1: Configure the Audio for the playback .Get the audio index/group
				ConfigureAudioTrack();
			}

			// Step 3: Based on the audio selection done , configure the profiles required
			ConfigureVideoProfiles();

			if(rate == AAMP_NORMAL_PLAY_RATE)
			{
				// Step 2: Configure Subtitle track for the playback
				ConfigureTextTrack();
				// Generate audio and text track structures
				PopulateAudioAndTextTracks();
			}



			currentProfileIndex = GetDesiredProfile(false);
			HlsStreamInfo *streamInfo = (HlsStreamInfo*)GetStreamInfo(currentProfileIndex);
			long bandwidthBitsPerSecond = streamInfo->bandwidthBitsPerSecond;
			lastSelectedProfileIndex = currentProfileIndex;
			aamp->ResetCurrentlyAvailableBandwidth(bandwidthBitsPerSecond, trickplayMode, currentProfileIndex);
			aamp->profiler.SetBandwidthBitsPerSecondVideo(bandwidthBitsPerSecond);
			/* START: Added As Part of DELIA-28363 and DELIA-28247 */
			AAMPLOG_INFO("Selected BitRate: %ld, Max BitRate: %ld", bandwidthBitsPerSecond, GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond);
			/* END: Added As Part of DELIA-28363 and DELIA-28247 */
		}
		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			const char* trackName = "subs";
			if (eTRACK_VIDEO == iTrack)
			{
				if(trackState[eTRACK_AUDIO]->enabled)
				{
					trackName = "video";
				}
				else if (rate != AAMP_NORMAL_PLAY_RATE)
				{
					trackName = "iframe";
				}
				else
				{
					trackName = "muxed";
				}
			}
			else if (eTRACK_AUDIO == iTrack)
			{
				trackName = "audio";
			}
			else if (eTRACK_AUX_AUDIO == iTrack)
			{
				trackName = "aux-audio";
			}
			trackState[iTrack] = new TrackState(mLogObj, (TrackType)iTrack, this, aamp, trackName);
			TrackState *ts = trackState[iTrack];
			ts->playlistPosition = -1;
			ts->playTarget = seekPosition;
			ts->playTargetBufferCalc = seekPosition;
			if (iTrack == eTRACK_SUBTITLE && !aamp->IsSubtitleEnabled())
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::subtitles disabled by application");
				ts->enabled = false;
				ts->streamOutputFormat = FORMAT_INVALID;
				continue;
			}
			if (iTrack == eTRACK_AUX_AUDIO)
			{
				if (!aamp->IsAuxiliaryAudioEnabled())
				{
					AAMPLOG_INFO("StreamAbstractionAAMP_HLS::auxiliary audio disabled");
					ts->enabled = false;
					ts->streamOutputFormat = FORMAT_INVALID;
					continue;
				}
				else if (aamp->GetAuxiliaryAudioLanguage() == aamp->mAudioTuple.language)
				{
					AAMPLOG_INFO("StreamAbstractionAAMP_HLS::auxiliary audio same as primary audio, set forward audio flag");
					ts->enabled = false;
					ts->streamOutputFormat = FORMAT_INVALID;
					SetAudioFwdToAuxStatus(true);
					continue;
				}
			}
			const char *uri = GetPlaylistURI((TrackType)iTrack, &ts->streamOutputFormat);
			if (uri)
			{
				aamp_ResolveURL(ts->mPlaylistUrl, aamp->GetManifestUrl(), uri, ISCONFIGSET(eAAMPConfig_PropogateURIParam));
				if(ts->streamOutputFormat != FORMAT_INVALID)
				{
					ts->enabled = true;
					mNumberOfTracks++;
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: %s format could not be determined. codecs %s", ts->name, streamInfo[currentProfileIndex].codecs);
				}
			}
		}

		TrackState *audio = trackState[eMEDIATYPE_AUDIO];
		TrackState *video = trackState[eMEDIATYPE_VIDEO];
		TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
		TrackState *aux = trackState[eMEDIATYPE_AUX_AUDIO];

		//Store Bitrate info to Video Track
		if(video) 
		{
		    video->SetCurrentBandWidth(GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond);
                }
		if(ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
		{
			if(audio->enabled)
			{
				if(video)    //CID:136262 - Forward null
				{	
				   video->enabled = false;
				   video->streamOutputFormat = FORMAT_INVALID;
				}
			}
			else
			{
				trackState[eTRACK_VIDEO]->type = eTRACK_AUDIO;
			}
			subtitle->enabled = false;
			subtitle->streamOutputFormat = FORMAT_INVALID;

			//RDK-27996 No need to enable auxiliary audio feature for audio only playback scenarios
			aux->enabled = false;
			aux->streamOutputFormat = FORMAT_INVALID;
		}
		aamp->profiler.SetBandwidthBitsPerSecondAudio(audio->GetCurrentBandWidth());

		pthread_t trackPLDownloadThreadID;
		bool trackPLDownloadThreadStarted = false;
		if (audio->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(audio->mPlaylistUrl, &audio->playlist, audio->mEffectiveUrl))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::audio playlist retrieved from cache");
			}
			if(!audio->playlist.len)
			{
				if (ISCONFIGSET(eAAMPConfig_PlaylistParallelFetch))
				{
					int ret = pthread_create(&trackPLDownloadThreadID, NULL, TrackPLDownloader, audio);
					if(ret != 0)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS: pthread_create failed for TrackPLDownloader with errno = %d, %s", errno, strerror(errno));
					}
					else
					{
						trackPLDownloadThreadStarted = true;
					}
				}
				else
				{
					audio->FetchPlaylist();
				}
			}
		}
		if (video->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(video->mPlaylistUrl, &video->playlist, video->mEffectiveUrl))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::video playlist retrieved from cache");
			}
			if(!video->playlist.len)
			{
				/* START: Added As Part of DELIA-39963 */
				int limitCount = 0;
				int numberOfLimit = 0;

				GETCONFIGVALUE(eAAMPConfig_InitRampDownLimit,numberOfLimit);
				do{
					video->FetchPlaylist();
					limitCount++;
					if ((!video->playlist.len) && (limitCount <= numberOfLimit) ){
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Video playlist download failed, rettrying with rampdown logic : %d ( %d )", 
						 limitCount, numberOfLimit );
						/** Choose rampdown profile for next retry */
						currentProfileIndex = mAbrManager.getRampedDownProfileIndex(currentProfileIndex);
						long bandwidthBitsPerSecond = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
						if(lastSelectedProfileIndex == currentProfileIndex){
							AAMPLOG_INFO("Failed to rampdown from bandwidth : %ld", bandwidthBitsPerSecond);
							break;
						}

						lastSelectedProfileIndex = currentProfileIndex;
						AAMPLOG_INFO("Trying BitRate: %ld, Max BitRate: %ld", bandwidthBitsPerSecond,
						GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond);
						const char *uri = GetPlaylistURI(eTRACK_VIDEO, &video->streamOutputFormat);
						if (uri){
							aamp_ResolveURL(video->mPlaylistUrl, aamp->GetManifestUrl(), uri ,ISCONFIGSET(eAAMPConfig_PropogateURIParam));

						}else{
							AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Failed to get URL after %d rampdown attempts", 
								 limitCount);
							break;
						}

					}else if (video->playlist.len){
						long bandwidthBitsPerSecond = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
						aamp->ResetCurrentlyAvailableBandwidth(
							bandwidthBitsPerSecond,
							trickplayMode,currentProfileIndex);
						aamp->profiler.SetBandwidthBitsPerSecondVideo(
							bandwidthBitsPerSecond);
						AAMPLOG_INFO("Selected BitRate: %ld, Max BitRate: %ld", 
							bandwidthBitsPerSecond,
							GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond);
						break;
					}
				}while(limitCount <= numberOfLimit);
				if (!video->playlist.len){
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Video playlist download failed still after %d rampdown attempts", 
				            limitCount);
				}
				/* END: Added As Part of DELIA-39963 */
			}
		}
		if (subtitle->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(subtitle->mPlaylistUrl, &subtitle->playlist, subtitle->mEffectiveUrl))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::subtitle playlist retrieved from cache");
			}
			if (!subtitle->playlist.len)
			{
				subtitle->FetchPlaylist();
			}
			if (!subtitle->playlist.len)
			{
				//This is logged as a warning. Not critical to playback
				AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Subtitle playlist download failed");
				subtitle->enabled = false;
			}
		}
		if (aux->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(aux->mPlaylistUrl, &aux->playlist, aux->mEffectiveUrl))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::auxiliary audio playlist retrieved from cache");
			}
			if (!aux->playlist.len)
			{
				aux->FetchPlaylist();
			}
			if (!aux->playlist.len)
			{
				//TODO: This is logged as a warning. Decide if its critical for playback
				AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Auxiliary audio playlist download failed");
				aux->enabled = false;
				aux->streamOutputFormat = FORMAT_INVALID;
			}
		}

		if (trackPLDownloadThreadStarted)
		{
			pthread_join(trackPLDownloadThreadID, NULL);
		}
		if (video->enabled && !video->playlist.len)
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Video Playlist download failed");
			return eAAMPSTATUS_PLAYLIST_VIDEO_DOWNLOAD_ERROR;
		}
		else if (audio->enabled && !audio->playlist.len)
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Audio Playlist download failed");
			return eAAMPSTATUS_PLAYLIST_AUDIO_DOWNLOAD_ERROR;
		}

		if (rate != AAMP_NORMAL_PLAY_RATE)
		{
			trickplayMode = true;
			if(aamp->IsTSBSupported())
			{
				GETCONFIGVALUE(eAAMPConfig_LinearTrickPlayFPS,mTrickPlayFPS);				
			}
			else
			{
				GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS,mTrickPlayFPS); 				
			}
		}
		else
		{
			trickplayMode = false;
		}

		double programStartTime = -1;

		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			TrackState *ts = trackState[iTrack];

			if(ts->enabled)
			{
				double culled=0;
				bool playContextConfigured = false;
				aamp_AppendNulTerminator(&ts->playlist); // make safe for cstring operations
				if (gpGlobalConfig->logging.trace  )
				{
					printf("***Initial Playlist:******\n\n%s\n*****************\n", ts->playlist.ptr);
				}
				// Flag also denotes if first encrypted init fragment was pushed or not
				ts->mCheckForInitialFragEnc = true; //force encrypted header at the start
				ts->IndexPlaylist(!newTune,culled);
				if (IsLive() && eTRACK_VIDEO == ts->type)
				{
					programStartTime = ts->mProgramDateTime;

					if( culled > 0)
					{
						AAMPLOG_INFO("Updating PDT (%f) and culled (%f) Updated seek_pos_seconds:%f ",ts->mProgramDateTime,culled,(seekPosition - culled));
						aamp->mProgramDateTime = ts->mProgramDateTime;
						aamp->UpdateCullingState(culled); // report amount of content that was implicitly culled since last playlist download		
						SeekPosUpdate((seekPosition - culled));
					}
				}


#ifndef AVE_DRM
				if(ts->mDrmMetaDataIndexCount > 0)
				{
					AAMPLOG_ERR("TrackState: Sending Error event DRM unsupported");
					return eAAMPSTATUS_UNSUPPORTED_DRM_ERROR;
				}
#endif
				if (ts->mDuration == 0.0f)
				{
					//TODO: Confirm if aux audio playlist has issues, it should be deemed as a playback failure
					if (iTrack == eTRACK_SUBTITLE || iTrack == eTRACK_AUX_AUDIO)
					{
						//Subtitle is optional and not critical to playback
						ts->enabled = false;
						ts->streamOutputFormat = FORMAT_INVALID;
						AAMPLOG_ERR("StreamAbstractionAAMP_HLS::%s playlist duration is zero!!",
								 ts->name);
					}
					else
					{
						break;
					}
				}
				// Send Metadata for Video playlist
				if(iTrack == eTRACK_VIDEO)
				{
					bool bMetadata = ISCONFIGSET(eAAMPConfig_BulkTimedMetaReport);
					ts->FindTimedMetadata(bMetadata, true);
					if(bMetadata && newTune)
					{
						// Send bulk report
						aamp->ReportBulkTimedMetadata();
					}
				}

				if (iTrack == eMEDIATYPE_VIDEO)
				{
					maxIntervalBtwPlaylistUpdateMs = 2 * ts->targetDurationSeconds * 1000; //Time interval for periodic playlist update
					if (maxIntervalBtwPlaylistUpdateMs > DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS)
					{
						maxIntervalBtwPlaylistUpdateMs = DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS;
					}
					aamp->UpdateRefreshPlaylistInterval(maxIntervalBtwPlaylistUpdateMs/1000.0);
				}

				ts->fragmentURI = ts->playlist.ptr;
				StreamOutputFormat format = GetFormatFromFragmentExtension(ts);
				if (FORMAT_ISO_BMFF == format)
				{
					//Disable subtitle in mp4 format, as we don't support it for now
					if (eMEDIATYPE_SUBTITLE == iTrack)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Unsupported subtitle format from fragment extension:%d",  format);
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI = NULL;
						ts->enabled = false;
					}
					//TODO: Extend auxiliary audio support for fragmented mp4 asset in future
					else if (eMEDIATYPE_AUX_AUDIO == iTrack)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Auxiliary audio not supported for FORMAT_ISO_BMFF, disabling!");
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI = NULL;
						ts->enabled = false;
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_ISO_BMFF", ts->name);
						ts->streamOutputFormat = FORMAT_ISO_BMFF;
						IsoBmffProcessor *processor = NULL;
						if (eMEDIATYPE_VIDEO == iTrack)
						{
							processor = static_cast<IsoBmffProcessor*> (trackState[eMEDIATYPE_AUDIO]->playContext);
						}
						ts->playContext = new IsoBmffProcessor(aamp, mLogObj, (IsoBmffProcessorType) iTrack, processor);
						ts->playContext->setRate(this->rate, PlayMode_normal);
						//Disable subtitle for fragmented MP4 assets, as we need tsprocessor support for webvtt parsing now
						if (subtitle->enabled)
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Unsupported media format for audio or video - FORMAT_ISO_BMFF");
							subtitle->streamOutputFormat = FORMAT_INVALID;
							subtitle->fragmentURI = NULL;
							subtitle->enabled = false;
						}
					}
					continue;
				}
				// Not ISOBMFF, no need for encrypted header check and associated logic
				// But header identification might have been already done, if EXT-X-MAP is present in playlist
				ts->mCheckForInitialFragEnc = false;
				// Elementary stream, we can skip playContext creation
				if (FORMAT_AUDIO_ES_AAC == format)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_AUDIO_ES_AAC", ts->name);
					ts->streamOutputFormat = FORMAT_AUDIO_ES_AAC;
					aamp->SetAudioPlayContextCreationSkipped( true );
					continue;
				}

				// Elementary stream, we can skip playContext creation
				if (FORMAT_AUDIO_ES_AC3 == format)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_AUDIO_ES_AC3", ts->name);
					ts->streamOutputFormat = FORMAT_AUDIO_ES_AC3;
					aamp->SetAudioPlayContextCreationSkipped( true );
					continue;
				}

				// Elementary stream, we can skip playContext creation
				if (FORMAT_AUDIO_ES_EC3 == format)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_AUDIO_ES_EC3", ts->name);
					ts->streamOutputFormat = FORMAT_AUDIO_ES_EC3;
					aamp->SetAudioPlayContextCreationSkipped( true );
					continue;
				}

				if (eMEDIATYPE_SUBTITLE == iTrack)
				{
					bool subtitleDisabled = false;
					if (this->rate != AAMP_NORMAL_PLAY_RATE)
					{
						subtitleDisabled = true;
					}
					else if (format != FORMAT_SUBTITLE_WEBVTT)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Unsupported subtitle format from fragment extension:%d", format);
						subtitleDisabled = true;
					}

					//Configure parser for subtitle
					if (!subtitleDisabled)
					{
						ts->streamOutputFormat = format;
						SubtitleMimeType type = (format == FORMAT_SUBTITLE_WEBVTT) ? eSUB_TYPE_WEBVTT : eSUB_TYPE_UNKNOWN;
						ts->mSubtitleParser = SubtecFactory::createSubtitleParser(mLogObj, aamp, type);
						if (!ts->mSubtitleParser) 
						{
							ts->streamOutputFormat = FORMAT_INVALID;
							ts->fragmentURI = NULL;
							ts->enabled = false;
						}
						aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
					}
					else
					{
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI = NULL;
						ts->enabled = false;
					}
					continue; //no playcontext config for subtitle
				}
				else if (eMEDIATYPE_AUX_AUDIO == iTrack)
				{
					if (this->rate == AAMP_NORMAL_PLAY_RATE)
					{
						if (format == FORMAT_MPEGTS)
						{
							AAMPLOG_WARN("Configure auxiliary audio TS track demuxing");
							ts->playContext = new TSProcessor(mLogObj, aamp, eStreamOp_DEMUX_AUX);
							if (ts->playContext)
							{
								ts->playContext->setRate(this->rate, PlayMode_normal);
								ts->playContext->setThrottleEnable(false);
								playContextConfigured = true;
							}
							else
							{
								ts->streamOutputFormat = format;
							}
						}
						else if (FORMAT_INVALID != format)
						{
							AAMPLOG_WARN("Configure auxiliary audio format based on extension");
							ts->streamOutputFormat = format;
						}
						else
						{
							AAMPLOG_WARN("Keeping auxiliary audio format from playlist");
						}
					}
					else
					{
						AAMPLOG_WARN("Disable auxiliary audio format - trick play");
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI = NULL;
						ts->enabled = false;
					}
				}
				else if (eMEDIATYPE_AUDIO == iTrack)
				{
					if (this->rate == AAMP_NORMAL_PLAY_RATE)
					{
						// Creation of playContext is required only for TS fragments
						if (format == FORMAT_MPEGTS)
						{
							if (ISCONFIGSET(eAAMPConfig_DemuxAudioHLSTrack))
							{
								AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Configure audio TS track demuxing");
								ts->playContext = new TSProcessor(mLogObj, aamp, eStreamOp_DEMUX_AUDIO);
							}							
							else
							{
								AAMPLOG_WARN("Configure audio TS track to queue");
								ts->playContext = new TSProcessor(mLogObj, aamp, eStreamOp_QUEUE_AUDIO);
								// Audio is muxed with video, no need to configure pipeline for the same
								ts->streamOutputFormat = FORMAT_INVALID;
								audioQueuedPC = static_cast<TSProcessor*> (ts->playContext);
							}
							if (ts->playContext)
							{
								ts->playContext->setRate(this->rate, PlayMode_normal);
								ts->playContext->setThrottleEnable(false);
								playContextConfigured = true;
							}
							else
							{
								ts->streamOutputFormat = format;
							}
						}
						else if (FORMAT_INVALID != format)
						{
							AAMPLOG_WARN("Configure audio format based on extension");
							ts->streamOutputFormat = format;
						}
						else
						{
							AAMPLOG_WARN("Keeping audio format from playlist");
						}
					}
					else
					{
						AAMPLOG_WARN("Disable audio format - trick play");
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI = NULL;
						ts->enabled = false;
					}
				}
				else if ((ISCONFIGSET(eAAMPConfig_DemuxVideoHLSTrack) && (rate == AAMP_NORMAL_PLAY_RATE))
						|| (ISCONFIGSET(eAAMPConfig_DemuxHLSVideoTsTrackTM)&& (rate != AAMP_NORMAL_PLAY_RATE)))
				{
					/*Populate format from codec data*/
					format = GetStreamOutputFormatForTrack(eTRACK_VIDEO);

					if (FORMAT_INVALID != format)
					{
						StreamOperation demuxOp;
						ts->streamOutputFormat = format;
						// Check if auxiliary audio is muxed here, by confirming streamOutputFormat != FORMAT_INVALID
						if (!aux->enabled && (aux->streamOutputFormat != FORMAT_INVALID) && (AAMP_NORMAL_PLAY_RATE == rate))
						{
							demuxOp = eStreamOp_DEMUX_VIDEO_AND_AUX;
						}
						else if ((trackState[eTRACK_AUDIO]->enabled) || (AAMP_NORMAL_PLAY_RATE != rate))
						{
							demuxOp = eStreamOp_DEMUX_VIDEO;
						}
						else
						{
							// In case of muxed, where there is no X-MEDIA tag but CODECS show presence of audio
							// This could be changed later, once we let TSProcessor configure tracks based on demux status
							StreamOutputFormat audioFormat = GetStreamOutputFormatForTrack(eTRACK_AUDIO);
							if (audioFormat != FORMAT_UNKNOWN)
							{
								trackState[eMEDIATYPE_AUDIO]->streamOutputFormat = audioFormat;
							}

							// Even if audio info is not present in manifest, we let TSProcessor run a full sweep
							// If audio is found, then TSProcessor will configure stream sink accordingly
							if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
							{
								// For muxed tracks, demux audio and video
								demuxOp = eStreamOp_DEMUX_ALL;
							}
							else
							{
								// Audio only playback, disable video
								demuxOp = eStreamOp_DEMUX_AUDIO;
								video->streamOutputFormat = FORMAT_INVALID;
							}
						}
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Configure video TS track demuxing demuxOp %d", demuxOp);
						ts->playContext = new TSProcessor(mLogObj, aamp, demuxOp, eMEDIATYPE_VIDEO, static_cast<TSProcessor*> (trackState[eMEDIATYPE_AUDIO]->playContext), static_cast<TSProcessor*> (trackState[eMEDIATYPE_AUX_AUDIO]->playContext));
						ts->playContext->setThrottleEnable(this->enableThrottle);
						if (this->rate == AAMP_NORMAL_PLAY_RATE)
						{
							ts->playContext->setRate(this->rate, PlayMode_normal);
						}
						else
						{
							ts->playContext->setRate(this->rate, PlayMode_retimestamp_Ionly);
							ts->playContext->setFrameRateForTM(mTrickPlayFPS);
						}
						playContextConfigured = true;
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : VideoTrack -couldn't determine format from streamInfo->codec %s",
							streamInfo->codecs);
					}
				}
				else /*Video Track - No demuxing*/
				{
					if (audioQueuedPC)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Configure video TS track eStreamOp_SEND_VIDEO_AND_QUEUED_AUDIO");
						ts->playContext = new TSProcessor(mLogObj, aamp, eStreamOp_SEND_VIDEO_AND_QUEUED_AUDIO, eMEDIATYPE_VIDEO, audioQueuedPC);
						ts->playContext->setThrottleEnable(this->enableThrottle);
						ts->playContext->setRate(this->rate, PlayMode_normal);
						playContextConfigured = true;
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Configure video TS track %p : No streamops", ts);
					}
				}
				if (!playContextConfigured && (ts->streamOutputFormat == FORMAT_MPEGTS))
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : track %p context configuring for eStreamOp_NONE", ts);
					ts->playContext = new TSProcessor(mLogObj, aamp, eStreamOp_NONE, iTrack);
					ts->playContext->setThrottleEnable(this->enableThrottle);
					if (this->rate == AAMP_NORMAL_PLAY_RATE)
					{
						this->trickplayMode = false;
						ts->playContext->setRate(this->rate, PlayMode_normal);
					}
					else
					{
						this->trickplayMode = true;						
						if(aamp->IsTSBSupported())
						{
							GETCONFIGVALUE(eAAMPConfig_LinearTrickPlayFPS,mTrickPlayFPS);							
						}
						else
						{
							GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS,mTrickPlayFPS); 							
						}
						ts->playContext->setRate(this->rate, PlayMode_retimestamp_Ionly);
						ts->playContext->setFrameRateForTM(mTrickPlayFPS);
					}
				}
			}
		}
		// Set mIsLiveStream to keep live the history.
		if(newTune)
		{
			aamp->SetIsLiveStream(aamp->IsLive());
		}

		//reiterate loop when player receive an update in seek position
		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			trackState[iTrack]->playTarget = seekPosition;
			trackState[iTrack]->playTargetBufferCalc = seekPosition;
		}

		if ((video->enabled && video->mDuration == 0.0f) || (audio->enabled && audio->mDuration == 0.0f))
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_HLS: Track Duration is 0. Cannot play this content");
			return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}

		if (newTune && needMetadata)
		{
			needMetadata = false;
			aamp->mIsIframeTrackPresent = mIframeAvailable;
			mProgramStartTime = programStartTime;
			// Delay "preparing" state until all tracks have been processed.
			// JS Player assumes all onTimedMetadata event fire before "preparing" state.
			aamp->SetState(eSTATE_PREPARING);
		}

		//Currently un-used playlist indexed event, might save some JS overhead
		if (!ISCONFIGSET(eAAMPConfig_DisablePlaylistIndexEvent))
		{
			aamp->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_PLAYLIST_INDEXED),AAMP_EVENT_ASYNC_MODE);
		}
		if (newTune)
		{
			if(ContentType_UNKNOWN == aamp->GetContentType())
			{
				if(aamp->IsLive())
				{
					aamp->SetContentType("LINEAR_TV");
				}
				else
				{
					aamp->SetContentType("VOD");
				}	
			}	

			
			if (eTUNED_EVENT_ON_PLAYLIST_INDEXED == aamp->GetTuneEventConfig(aamp->IsLive()))
			{
				if (aamp->SendTunedEvent())
				{
					AAMPLOG_WARN("aamp: hls - sent tune event after indexing playlist");
				}
			}
		}

		/*Do live adjust on live streams on 1. eTUNETYPE_NEW_NORMAL, 2. eTUNETYPE_SEEKTOLIVE,
		 * 3. Seek to a point beyond duration*/
		bool liveAdjust = (eTUNETYPE_NEW_NORMAL == tuneType)  && aamp->IsLiveAdjustRequired() && (aamp->IsLive());
		if ((eTUNETYPE_SEEKTOLIVE == tuneType) && aamp->IsLive())
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_HLS: eTUNETYPE_SEEKTOLIVE, reset playTarget and enable liveAdjust");
			liveAdjust = true;

			audio->playTarget = 0;
			video->playTarget = 0;
			subtitle->playTarget = 0;
			aux->playTarget = 0;
			aamp->NotifyOnEnteringLive();
		}
		else if (((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_RETUNE == tuneType) || (eTUNETYPE_NEW_SEEK == tuneType)) && (this->rate > 0))
		{
			double seekWindowEnd = video->mDuration;
			if(aamp->IsLive())
			{
				seekWindowEnd -= aamp->mLiveOffset ;
			}
			// check if seek beyond live point
			if (round(video->playTarget) >= round(seekWindowEnd))
			{
				if (aamp->IsLive())
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: playTarget > seekWindowEnd , playTarget:%f and seekWindowEnd:%f",
							video->playTarget , seekWindowEnd);
					liveAdjust = true;

					audio->playTarget = 0;
					video->playTarget = 0;
					subtitle->playTarget = 0;
					aux->playTarget = 0;
					if (eTUNETYPE_SEEK == tuneType)
					{
						aamp->NotifyOnEnteringLive();
					}
				}
				else
				{
					video->eosReached = true;
					video->fragmentURI = NULL;
					audio->eosReached = true;
					audio->fragmentURI = NULL;
					subtitle->eosReached = true;
					subtitle->fragmentURI = NULL;
					aux->eosReached = true;
					aux->fragmentURI = NULL;
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: seek target out of range, mark EOS. playTarget:%f End:%f. ",
							video->playTarget, seekWindowEnd);

					return eAAMPSTATUS_SEEK_RANGE_ERROR;
				}
			}
		}

		// RDK-27796, in case of muxed a/v and auxiliary track scenario
		// For demuxed a/v, we will handle it in SyncTracks...() function
		if (audio->enabled || aux->enabled)
		{
			TrackState *other = audio->enabled ? audio : aux;
			if (!aamp->IsLive())
			{
				SyncTracksForDiscontinuity();
			}
			else
			{
				if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
				{
					bool syncDone = false;
					if (!liveAdjust && video->mDiscontinuityIndexCount && (video->mDiscontinuityIndexCount == other->mDiscontinuityIndexCount))
					{
						if (eAAMPSTATUS_OK == SyncTracksForDiscontinuity())
						{
							syncDone = true;
						}
					}
				}
			}
		}
		else if (subtitle->enabled)
		{
			//TODO:Muxed track with subtitles. Need to sync tracks
		}

		if (liveAdjust)
		{
			double xStartOffset = video->GetXStartTimeOffset();
			double offsetFromLive = aamp->mLiveOffset ; 
			// check if there is xStartOffSet , if non zero value present ,check if it is > 3 times TD(Spec requirement)
			if(xStartOffset != 0 && abs(xStartOffset) > (3*video->targetDurationSeconds))
			{
				// DELIA-40177 -> For now code added for negative offset values
				// that is offset from last duration 
				if(xStartOffset < 0)
				{
					offsetFromLive = abs(xStartOffset);
					AAMPLOG_WARN("liveOffset modified with X-Start to :%f",offsetFromLive);
				}
				// if xStartOffset is positive value , then playposition to be considered from beginning 
				// TBD for later.Only offset from end is supported now . That too only for live . Not for VOD!!!!
			}
			
			if (video->mDuration > (offsetFromLive + video->playTargetOffset))
			{
				//DELIA-28451
				// a) Get OffSet to Live for Video and Audio separately.
				// b) Set to minimum value among video /audio instead of setting to 0 position
				double offsetToLiveVideo,offsetToLiveAudio,offsetToLive;
				offsetToLiveVideo = offsetToLiveAudio = video->mDuration - offsetFromLive - video->playTargetOffset;
				//TODO: Handle case for muxed a/v and aux track
				if (audio->enabled)
				{
					offsetToLiveAudio = 0;
					// if audio is not having enough total duration to adjust , then offset value set to 0
					if( audio->mDuration > (offsetFromLive + audio->playTargetOffset))
						offsetToLiveAudio = audio->mDuration - offsetFromLive -  audio->playTargetOffset;
					else
						AAMPLOG_WARN("aamp: live adjust not possible ATotal[%f]< (AoffsetFromLive[%f] + AplayTargetOffset[%f]) A-target[%f]", audio->mDuration,offsetFromLive,audio->playTargetOffset,audio->playTarget);
				}
				// pick the min of video/audio offset
				offsetToLive = (std::min)(offsetToLiveVideo,offsetToLiveAudio);
				video->playTarget += offsetToLive;
				video->playTargetBufferCalc = video->playTarget;
				if (audio->enabled )
				{
					audio->playTarget += offsetToLive;
					audio->playTargetBufferCalc = audio->playTarget;
				}
				if (subtitle->enabled)
				{
					subtitle->playTarget += offsetToLive;
					subtitle->playTargetBufferCalc = subtitle->playTarget;
				}
				if (aux->enabled)
				{
					aux->playTarget += offsetToLive;
					aux->playTargetBufferCalc = aux->playTarget;
				}
				// Entering live will happen if offset is adjusted , if its 0 playback is starting from beginning
				if(offsetToLive)
					mIsAtLivePoint = true;
				AAMPLOG_WARN("aamp: after live adjust - V-target %f A-target %f S-target %f Aux-target %f offsetFromLive %f offsetToLive %f offsetVideo[%f] offsetAudio[%f] AtLivePoint[%d]",
				        video->playTarget, audio->playTarget, subtitle->playTarget, aux->playTarget, offsetFromLive, offsetToLive,offsetToLiveVideo,offsetToLiveAudio,mIsAtLivePoint);
			}
			else
			{
				AAMPLOG_WARN("aamp: live adjust not possible VTotal[%f] < (VoffsetFromLive[%f] + VplayTargetOffset[%f]) V-target[%f]", 
					video->mDuration,offsetFromLive,video->playTargetOffset,video->playTarget);
			}
			//Set live adusted position to seekPosition
			SeekPosUpdate(video->playTarget);

		}
		/*Adjust for discontinuity*/
		if ((audio->enabled || aux->enabled) && (aamp->IsLive()) && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
		{
			TrackState *otherTrack = audio->enabled ? audio : aux;
			int discontinuityIndexCount = video->mDiscontinuityIndexCount;
			if (discontinuityIndexCount > 0)
			{
				if (discontinuityIndexCount == otherTrack->mDiscontinuityIndexCount)
				{
					if (liveAdjust)
					{
						SyncTracksForDiscontinuity();
					}
					float videoPrevDiscontinuity = 0;
					float audioPrevDiscontinuity = 0;
					float videoNextDiscontinuity;
					float audioNextDiscontinuity;
					DiscontinuityIndexNode* videoDiscontinuityIndex = (DiscontinuityIndexNode*)video->mDiscontinuityIndex.ptr;
					DiscontinuityIndexNode* audioDiscontinuityIndex = (DiscontinuityIndexNode*)otherTrack->mDiscontinuityIndex.ptr;
					for (int i = 0; i <= discontinuityIndexCount; i++)
					{
						if (i < discontinuityIndexCount)
						{
							videoNextDiscontinuity = videoDiscontinuityIndex[i].position;
							audioNextDiscontinuity = audioDiscontinuityIndex[i].position;
						}
						else
						{
							videoNextDiscontinuity = aamp->GetDurationMs() / 1000;
							audioNextDiscontinuity = videoNextDiscontinuity;
						}
						if ((videoNextDiscontinuity > (video->playTarget + 5))
						        && (audioNextDiscontinuity > (otherTrack->playTarget + 5)))
						{

							AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: video->playTarget %f videoPrevDiscontinuity %f videoNextDiscontinuity %f",
									video->playTarget, videoPrevDiscontinuity, videoNextDiscontinuity);
							AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: %s->playTarget %f audioPrevDiscontinuity %f audioNextDiscontinuity %f",
									otherTrack->name, otherTrack->playTarget, audioPrevDiscontinuity, audioNextDiscontinuity);
							if (video->playTarget < videoPrevDiscontinuity)
							{
								AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: [video] playTarget(%f) advance to discontinuity(%f)",
										video->playTarget, videoPrevDiscontinuity);
								video->playTarget = videoPrevDiscontinuity;
								video->playTargetBufferCalc = video->playTarget;
							}
							if (otherTrack->playTarget < audioPrevDiscontinuity)
							{
								AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: [%s] playTarget(%f) advance to discontinuity(%f)",
										otherTrack->name, otherTrack->playTarget, audioPrevDiscontinuity);
								otherTrack->playTarget = audioPrevDiscontinuity;
								otherTrack->playTargetBufferCalc = otherTrack->playTarget;
							}
							break;
						}
						videoPrevDiscontinuity = videoNextDiscontinuity;
						audioPrevDiscontinuity = audioNextDiscontinuity;
					}
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: videoPeriodPositionIndex.size %d audioPeriodPositionIndex.size %d",
							video->mDiscontinuityIndexCount, otherTrack->mDiscontinuityIndexCount);
				}
			}
			else
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: videoPeriodPositionIndex.size 0");
			}
		}

		audio->lastPlaylistDownloadTimeMS = aamp_GetCurrentTimeMS();
		video->lastPlaylistDownloadTimeMS = audio->lastPlaylistDownloadTimeMS;
		subtitle->lastPlaylistDownloadTimeMS = audio->lastPlaylistDownloadTimeMS;
		aux->lastPlaylistDownloadTimeMS = audio->lastPlaylistDownloadTimeMS;
		/*Use start timestamp as zero when audio is not elementary stream*/
		mStartTimestampZero = ((video->streamOutputFormat == FORMAT_ISO_BMFF || audio->streamOutputFormat == FORMAT_ISO_BMFF) || (rate == AAMP_NORMAL_PLAY_RATE && (!audio->enabled || audio->playContext)));
		if (subtitle->enabled && subtitle->mSubtitleParser)
		{
			//Need to set reportProgressOffset to subtitleParser
			//playTarget becomes seek_pos_seconds and playlistPosition is the acutal position in playlist
			//TODO: move call GetNextFragmentUriFromPlaylist() to the sync operation btw muxed and subtitle
			if (!audio->enabled)
			{
				video->fragmentURI = video->GetNextFragmentUriFromPlaylist(true);
				video->playTarget = video->playlistPosition;
				video->playTargetBufferCalc = video->playTarget;
			}
			double offset = (video->playlistPosition - seekPosition) * 1000.0;
			AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Setting setProgressEventOffset value of %.3f ms", offset);
			subtitle->mSubtitleParser->setProgressEventOffset(offset);
		}
	
		if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			// this functionality needed for normal playback , not for trickplay .
			// After calling GetNextFragmentUriFromPlaylist , all LFs are removed from fragment info
			// inside GetFragmentUriFromIndex , there is check for LF which fails as its already removed and ends up returning NULL uri
			// So enforcing this strictly for normal playrate

			// DELIA-42052 
			for (int iTrack = 0; iTrack <= AAMP_TRACK_COUNT - 1; iTrack++)
			{
				TrackState *ts = trackState[iTrack];
				if(ts->enabled)
				{
					ts->fragmentURI = ts->GetNextFragmentUriFromPlaylist(true);
					ts->playTarget = ts->playlistPosition;
					ts->playTargetBufferCalc = ts->playTarget;
				}

				// To avoid audio loss while seeking HLS/TS AV of different duration w/o affecting VOD Discontinuities
				if(iTrack == 0 && ISCONFIGSET(eAAMPConfig_SyncAudioFragments) && !(ISCONFIGSET(eAAMPConfig_MidFragmentSeek) || audio->mDiscontinuityIndexCount))
				{
					AAMPLOG_TRACE("Setting audio playtarget %f to video playtarget %f", audio->playTarget, ts->playTarget);
					audio->playTarget = ts->playTarget;
				}
			}
			if (IsLive() && audio->enabled && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
			{
				AAMPStatusType retValue = SyncTracks();
				if (eAAMPSTATUS_OK != retValue)
					return retValue;
			}

			//Set live adusted position to seekPosition
			if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
			{
				midSeekPtsOffset = seekPosition - video->playTarget;
				if(midSeekPtsOffset > video->fragmentDurationSeconds/2)
				{
					if(aamp->GetInitialBufferDuration() == 0)
					{
						PrivAAMPState state;
						aamp->GetState(state);
						if(state == eSTATE_SEEKING)
						{
							// To prevent underflow when seeked to end of fragment.
							// Added +1 to ensure next fragment is fetched.
							SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,(int)video->fragmentDurationSeconds + 1);	
							aamp->midFragmentSeekCache = true;
						}
					}
				}
				else if(aamp->midFragmentSeekCache)
				{
					// Resetting fragment cache when seeked to first half of the fragment duration.
					SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,0);
					aamp->midFragmentSeekCache = false;
				}

				if(midSeekPtsOffset > 0.0)
				{
					midSeekPtsOffset += 0.5 ;  // Adding 0.5 to neutralize PTS-500ms in BasePTS calculation.
				}
				SeekPosUpdate(seekPosition);
			}
			else
			{
				SeekPosUpdate(video->playTarget);
			}
			AAMPLOG_WARN("seekPosition updated with corrected playtarget : %f midSeekPtsOffset : %f",seekPosition,midSeekPtsOffset);
		}

		// negative buffer calculation fix
		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			if (aamp->culledSeconds > 0)
			{
				trackState[iTrack]->playTargetBufferCalc = aamp->culledSeconds + seekPosition;
			}
		}


		if (newTune && ISCONFIGSET(eAAMPConfig_PrefetchIFramePlaylistDL))
		{
			int iframeStreamIdx = GetIframeTrack();
			if (0 <= iframeStreamIdx)
			{
				std::string defaultIframePlaylistUrl;
				std::string defaultIframePlaylistEffectiveUrl;
				//To avoid clashing with the http error for master manifest
				long http_error = 0;
				GrowableBuffer defaultIframePlaylist;
				HlsStreamInfo *streamInfo = (HlsStreamInfo *)GetStreamInfo(iframeStreamIdx);
				aamp_ResolveURL(defaultIframePlaylistUrl, aamp->GetManifestUrl(), streamInfo->uri, ISCONFIGSET(eAAMPConfig_PropogateURIParam));
				AAMPLOG_TRACE("StreamAbstractionAAMP_HLS:: Downloading iframe playlist");
				bool bFiledownloaded = false;
				if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(defaultIframePlaylistUrl, &defaultIframePlaylist, defaultIframePlaylistEffectiveUrl) == false){
					double downloadTime;
					bFiledownloaded = aamp->GetFile(defaultIframePlaylistUrl, &defaultIframePlaylist, defaultIframePlaylistEffectiveUrl, &http_error, &downloadTime, NULL,eCURLINSTANCE_MANIFEST_PLAYLIST);				
					//update videoend info
					ManifestData manifestData(downloadTime * 1000, defaultIframePlaylist.len);
					aamp->UpdateVideoEndMetrics( eMEDIATYPE_MANIFEST,streamInfo->bandwidthBitsPerSecond,http_error,defaultIframePlaylistEffectiveUrl, downloadTime, &manifestData);
				}
				if (defaultIframePlaylist.len && bFiledownloaded)
				{
					aamp->getAampCacheHandler()->InsertToPlaylistCache(defaultIframePlaylistUrl, &defaultIframePlaylist, defaultIframePlaylistEffectiveUrl,aamp->IsLive(),eMEDIATYPE_IFRAME);
					AAMPLOG_TRACE("StreamAbstractionAAMP_HLS:: Cached iframe playlist");
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Error Download iframe playlist. http_error %ld",
					       http_error);
				}
			}
		}

		if (newTune && !aamp->IsLive() && (aamp->mPreCacheDnldTimeWindow > 0) && (aamp->durationSeconds > aamp->mPreCacheDnldTimeWindow*60))
		{
			// Special requirement
			// DELIA-41566 [PEACOCK] temporary hack required to work around Adobe SSAI session lifecycle problem
			// If stream is VOD ( SSAI) , and if App configures PreCaching enabled ,
			// then all the playlist needs to be downloaded lazily and cached . This is to overcome gap
			// in VOD Server as it looses the Session Context after playback starts
			// This caching is for all substream ( video/audio/webvtt)
			PreCachePlaylist();
		}


		retval = eAAMPSTATUS_OK;
	}
	
	if(updateVideoEndMetrics)
	{
		//update videoend info
		ManifestData manifestData(mainManifestdownloadTime * 1000, this->mainManifest.len, parseTimeMs);
		aamp->UpdateVideoEndMetrics( eMEDIATYPE_MANIFEST,0,http_error,aamp->GetManifestUrl(), mainManifestdownloadTime, &manifestData);
	}
	return retval;
}

/***************************************************************************
* @fn CachePlaylistThreadFunction
* @brief Thread function created for PreCaching playlist
*
* @param This[in] PrivateAampInstance Context
* @return none
***************************************************************************/
static void * CachePlaylistThreadFunction(void * This) 
{
	// DELIA-41566 [PEACOCK] temporary hack required to work around Adobe SSAI session lifecycle problem
	// Temporary workaround code to address Peacock/Adobe Server issue 
	((PrivateInstanceAAMP*)This)->PreCachePlaylistDownloadTask(); 
	return NULL;
}



/***************************************************************************
* @fn PreCachePlaylist
* @brief Function to PreCache Playlist 
*
* @return none
***************************************************************************/
void StreamAbstractionAAMP_HLS::PreCachePlaylist()
{
	// DELIA-41566 [PEACOCK] temporary hack required to work around Adobe SSAI session lifecycle problem
	// Tasks to be done
	// Run thru all the streamInfo and get uri for download , push to a download list
	// Start a thread and return back . This thread will wake up after Tune completion
	// and start downloading the uri in the list
	int szUrlList = mMediaCount + mProfileCount;
	PreCacheUrlList dnldList ;
	for (int idx=0;idx < mProfileCount; idx++)
	{
		// Add Video and IFrame Profiles
		PreCacheUrlStruct newelem;
		aamp_ResolveURL(newelem.url, aamp->GetManifestUrl(), streamInfo[idx].uri, ISCONFIGSET(eAAMPConfig_PropogateURIParam));
		newelem.type = streamInfo[idx].isIframeTrack?eMEDIATYPE_PLAYLIST_IFRAME:eMEDIATYPE_PLAYLIST_VIDEO;
		dnldList.push_back(newelem);
	}

	for(int cnt=0;cnt<mMediaCount;cnt++)
	{
		// Add Media uris ( Audio and WebVTT)
		if(mediaInfo[cnt].uri)
		{
			//std::string url;
			PreCacheUrlStruct newelem;
			aamp_ResolveURL(newelem.url, aamp->GetManifestUrl(), mediaInfo[cnt].uri ,ISCONFIGSET(eAAMPConfig_PropogateURIParam));
			newelem.type = mediaInfo[cnt].type;
			dnldList.push_back(newelem);
		}
	}
	
	// Set the download list to PrivateInstance to download it 
	aamp->SetPreCacheDownloadList(dnldList);
	int ret = pthread_create(&aamp->mPreCachePlaylistThreadId, NULL, CachePlaylistThreadFunction,(void *)aamp );
	if(ret != 0)
	{
		AAMPLOG_ERR("pthread_create failed for PreCachePlaylist with errno = %d, %s", errno, strerror(errno));
	}
	else
	{
		aamp->mPreCachePlaylistThreadFlag = true;
	}
}



/***************************************************************************
* @fn GetFirstPTS
* @brief Function to return first PTS
*
* @return double PTS value
***************************************************************************/
double StreamAbstractionAAMP_HLS::GetFirstPTS()
{
	double pts = 0.0;
	if(mStartTimestampZero)
	{
		// For CMAF assets, we employ isobmffprocessor to get the PTS value since its not
		// known from manifest. mFirstPTS will be populated only if platform has qtdemux override enabled.
		// We check for only video, since mFirstPTS is first video frame's PTS.
		if (trackState[eMEDIATYPE_VIDEO]->streamOutputFormat == FORMAT_ISO_BMFF && mFirstPTS != 0)
		{
			pts += mFirstPTS;
		}
		if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
		{
			pts += midSeekPtsOffset;
		}
	}
	else
	{
		pts = seekPosition;
	}
	return pts;
}

double StreamAbstractionAAMP_HLS::GetBufferedDuration()
{
	TrackState *video = trackState[eTRACK_VIDEO];
	double retval = -1.0;
	if (video && video->enabled)
	{
		retval = video->GetBufferedDuration();
	}
	return retval;
}

double TrackState::GetBufferedDuration()
{
	return (playTargetBufferCalc - (aamp->GetPositionMs() / 1000));
}

/***************************************************************************
* @fn SwitchSubtitleTrack
* @brief Flushes out all old segments and sets up new playlist
*        Used to switch subtitle tracks without restarting the pipeline
*
* @return void
***************************************************************************/
void TrackState::SwitchSubtitleTrack()
{
	if (eTRACK_SUBTITLE == type && mSubtitleParser)
	{
		pthread_mutex_lock(&mutex);
		
		AAMPLOG_INFO("Preparing to flush fragments and switch playlist");
		// Flush all counters, reset the playlist URL and refresh the playlist
		FlushFragments();
		aamp_ResolveURL(mPlaylistUrl, aamp->GetManifestUrl(), context->GetPlaylistURI(type),ISCONFIGSET(eAAMPConfig_PropogateURIParam));
		RefreshPlaylist();

		playTarget = 0.0;
		fragmentURI = GetNextFragmentUriFromPlaylist();
		context->AbortWaitForAudioTrackCatchup(true);

		mSubtitleParser->init(aamp->GetPositionMilliseconds() / 1000.0, aamp->GetBasePTS());

		pthread_mutex_unlock(&mutex);		
	}
}

/***************************************************************************
* @fn RunFetchLoop
* @brief Fragment collector thread execution function to download fragments
*
* @return void
***************************************************************************/
void TrackState::RunFetchLoop()
{
	bool skipFetchFragment = false;
	bool abortedDownload = false;

	for (;;)
	{
		while (!abortedDownload && fragmentURI && aamp->DownloadsAreEnabled())
		{
			skipFetchFragment = false;
			if (mInjectInitFragment)
			{
				// DELIA-40273: mInjectInitFragment marks if init fragment has to be pushed whereas mInitFragmentInfo
				// holds the init fragment URL. Both has to be present for init fragment fetch & injection to work.
				// During ABR, mInjectInitFragment is set and for live assets,  mInitFragmentInfo is found
				// in FindMediaForSequenceNumber() and for VOD its found in GetNextFragmentUriFromPlaylist()
				// which also sets mInjectInitFragment to true, so below reset will not have an impact
				if (mInitFragmentInfo)
				{
					FetchInitFragment();
					//Inject init fragment failed due to no free cache
					if (mInjectInitFragment)
					{
						skipFetchFragment = true;
					}
					else
					{
						skipFetchFragment = false;
					}
				}
				else
				{
					mInjectInitFragment = false;
				}
			}
			
			if (!skipFetchFragment)
			{
				FetchFragment();
			}

			// FetchFragment involves multiple wait operations, so check download status again
			if (!aamp->DownloadsAreEnabled())
			{
				break;
			}

			/*Check for profile change only for video track*/
			// Avoid ABR if we have seen or just pushed an init fragment
			if((eTRACK_VIDEO == type) && (!context->trickplayMode) && !(mInjectInitFragment || mSkipAbr))
			{
				context->lastSelectedProfileIndex = context->currentProfileIndex;
				//DELIA-33346 -- if rampdown is attempted to any failure , no abr change to be attempted .
				// else profile be resetted to top one leading to looping of bad fragment
				if(!context->mCheckForRampdown)
				{
					if (aamp->CheckABREnabled())
					{
						context->CheckForProfileChange();
					}
				}
			}

			if (IsLive())
			{
				int timeSinceLastPlaylistDownload = (int) (aamp_GetCurrentTimeMS()
				        - lastPlaylistDownloadTimeMS);
				if (context->maxIntervalBtwPlaylistUpdateMs <= timeSinceLastPlaylistDownload)
				{
					AAMPLOG_INFO("Refreshing '%s' playlist as maximum refresh delay exceeded", name);
					RefreshPlaylist();
					refreshPlaylist = false;
				}
#ifdef TRACE
				else
				{
					AAMPLOG_WARN("Not refreshing timeSinceLastPlaylistDownload = %d", timeSinceLastPlaylistDownload);
				}
#endif
			}
			
			// This will switch the subtitle track without restarting AV
			// Should be a smooth transition to new language
			if (refreshSubtitles)
			{
				// Reset abort flag (this was set to exit the fetch loop)
				abort = false;
				refreshSubtitles = false;
				SwitchSubtitleTrack();
			}
			
			pthread_mutex_lock(&mutex);
			if(refreshPlaylist)
			{
				//Refreshing playlist
				RefreshPlaylist();
				refreshPlaylist = false;
			}
			pthread_mutex_unlock(&mutex);
		}
		// reached end of vod stream
		//teststreamer_EndOfStreamReached();
                if(!abortedDownload && context->aamp->IsTSBSupported() && eosReached){
			AbortWaitForCachedAndFreeFragment(false);
                        /* Make the aborted variable to true to avoid 
                         * further fragment fetch loop running and abort sending multiple time */
                        abortedDownload = true;
                }
                else if ((eosReached && !context->aamp->IsTSBSupported()) || mReachedEndListTag || !context->aamp->DownloadsAreEnabled())
		{
                        /* Check whether already aborted or not */
                        if(!abortedDownload){
			        AbortWaitForCachedAndFreeFragment(false);
                        }
			break;
		}
		if (lastPlaylistDownloadTimeMS)
		{
			// if not present, new playlist wih at least one additional segment will be available
			// no earlier than 0.5*EXT-TARGETDURATION and no later than 1.5*EXT-TARGETDURATION
			// relative to previous playlist fetch.
			int timeSinceLastPlaylistDownload = (int)(aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
			int minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			long long currentPlayPosition = aamp->GetPositionMilliseconds();
			long long endPositionAvailable = (aamp->culledSeconds + aamp->durationSeconds)*1000;
			// playTarget value will vary if TSB is full and trickplay is attempted. Cant use for buffer calculation
			// So using the endposition in playlist - Current playing position to get the buffer availability
			long bufferAvailable = (endPositionAvailable - currentPlayPosition);
			// If buffer Available is > 2*targetDuration
			if(bufferAvailable  > (targetDurationSeconds*2*1000) )
			{
				// may be 1.0 times also can be set ???
				minDelayBetweenPlaylistUpdates = (int)(1.5 * 1000 * targetDurationSeconds);
			}
			// if buffer is between 2*target & targetDuration
			else if(bufferAvailable  > (targetDurationSeconds*1000))
			{
				minDelayBetweenPlaylistUpdates = (int)(0.5 * 1000 * targetDurationSeconds);
			}
			// This is to handle the case where target duration is high value(>Max delay)  but buffer is available just above the max update inteval
			else if(bufferAvailable > (2*MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS))
			{
				minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			}
			// if buffer < targetDuration && buffer < MaxDelayInterval
			else
			{
				// if bufferAvailable is less than targetDuration ,its in RED alert . Close to freeze
				// need to refresh soon ..
				if(bufferAvailable)
				{
					minDelayBetweenPlaylistUpdates = (int)(bufferAvailable / 3) ;
				}
				else
				{
					minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS; // 500mSec
				}
				// limit the logs when buffer is low
				{
					static int bufferlowCnt;
					if((bufferlowCnt++ & 5) == 0)
					{ 
						AAMPLOG_WARN("Buffer is running low(%ld).Type(%d) Refreshing playlist(%d).Target(%f) PlayPosition(%lld) End(%lld)",
							bufferAvailable,type,minDelayBetweenPlaylistUpdates,playTarget,currentPlayPosition,endPositionAvailable);
					}
				}
			}
			// adjust with last refreshed time interval
			minDelayBetweenPlaylistUpdates -= timeSinceLastPlaylistDownload;
			// restrict to Max delay interval
			if (minDelayBetweenPlaylistUpdates > MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
			{
				minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			}
			else if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
			{
				// minimum of 500 mSec needed to avoid too frequent download.
				minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			}
			AAMPLOG_INFO("aamp playlist end refresh type(%d) bufferMs(%ld) playtarget(%f) delay(%d) End(%lld) PlayPosition(%lld)",
					 type,bufferAvailable,playTarget,minDelayBetweenPlaylistUpdates,endPositionAvailable,currentPlayPosition);
			aamp->InterruptableMsSleep(minDelayBetweenPlaylistUpdates);
		}
		//Refreshing playlist
		RefreshPlaylist();

		AAMPLOG_FAILOVER("fragmentURI [%s] timeElapsedSinceLastFragment [%f]",
			 fragmentURI, (aamp_GetCurrentTimeMS() - context->LastVideoFragParsedTimeMS()));

		/* Added to handle an edge case for cdn failover, where we found valid sub-manifest but no valid fragments.
		 * In this case we have to stall the playback here. */
		if( fragmentURI == NULL && IsLive() && type == eTRACK_VIDEO)
		{
			AAMPLOG_FAILOVER("fragmentURI is NULL, playback may stall in few seconds..");
			context->CheckForPlaybackStall(false);
		}
	}
	AAMPLOG_WARN("fragment collector done. track %s", name);
}
/***************************************************************************
* @fn FragmentCollector
* @brief Fragment collector thread function
*
* @param arg[in] TrackState pointer
* @return void
***************************************************************************/

static void *FragmentCollector(void *arg)
{
	TrackState *track = (TrackState *)arg;
	if(aamp_pthread_setname(pthread_self(), "aampHLSFetcher"))
	{
		AAMPLOG_WARN("aamp_pthread_setname failed");
	}
	track->RunFetchLoop();
	return NULL;
}
/***************************************************************************
* @fn StreamAbstractionAAMP_HLS
* @brief Constructor function
*
* @param aamp[in] PrivateInstanceAAMP pointer
* @param seekpos[in] Seek position
* @param rate[in] Rate of playback
* @return void
***************************************************************************/
StreamAbstractionAAMP_HLS::StreamAbstractionAAMP_HLS(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate) : StreamAbstractionAAMP(logObj, aamp),
	rate(rate), maxIntervalBtwPlaylistUpdateMs(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS), mainManifest(), allowsCache(false), seekPosition(seekpos), mTrickPlayFPS(),
	enableThrottle(false), firstFragmentDecrypted(false), mStartTimestampZero(false), mNumberOfTracks(0), midSeekPtsOffset(0),
	lastSelectedProfileIndex(0), segDLFailCount(0), segDrmDecryptFailCount(0), mMediaCount(0),mProfileCount(0),
	mLangList(),mIframeAvailable(false), thumbnailManifest(), indexedTileInfo(),
	mFirstPTS(0)
{
#ifndef AVE_DRM
       AAMPLOG_WARN("PlayerInstanceAAMP() : AVE DRM disabled");
#endif
	trickplayMode = false;
	enableThrottle = ISCONFIGSET(eAAMPConfig_Throttle);
	AAMPLOG_WARN("hls fragment collector seekpos = %f", seekpos);
	if (rate == AAMP_NORMAL_PLAY_RATE)
	{
		this->trickplayMode = false;
	}
	else
	{
		this->trickplayMode = true;
	}
	//targetDurationSeconds = 0.0;
	mAbrManager.clearProfiles();
	memset(&trackState[0], 0x00, sizeof(trackState));
	aamp->CurlInit(eCURLINSTANCE_VIDEO, DEFAULT_CURL_INSTANCE_COUNT,aamp->GetNetworkProxy());
	memset(streamInfo, 0, sizeof(*streamInfo));

}
/***************************************************************************
* @fn TrackState
* @brief TrackState Constructor
*
* @param type[in] Type of the track
* @param parent[in] StreamAbstractionAAMP_HLS instance
* @param aamp[in] PrivateInstanceAAMP pointer
* @param name[in] Name of the track
* @return void
***************************************************************************/
TrackState::TrackState(AampLogManager *logObj, TrackType type, StreamAbstractionAAMP_HLS* parent, PrivateInstanceAAMP* aamp, const char* name) :
		MediaTrack(logObj, type, aamp, name),
		indexCount(0), currentIdx(0), indexFirstMediaSequenceNumber(0), fragmentURI(NULL), lastPlaylistDownloadTimeMS(0),
		byteRangeLength(0), byteRangeOffset(0), nextMediaSequenceNumber(0), playlistPosition(0), playTarget(0),playTargetBufferCalc(0),lastDownloadedIFrameTarget(-1),
		streamOutputFormat(FORMAT_INVALID), playContext(NULL),
		playTargetOffset(0),
		discontinuity(false),
		refreshPlaylist(false), fragmentCollectorThreadID(0),
		fragmentCollectorThreadStarted(false),
		manifestDLFailCount(0),
		mCMSha1Hash(NULL), mDrmTimeStamp(0), mDrmMetaDataIndexCount(0),firstIndexDone(false), mDrm(NULL), mDrmLicenseRequestPending(false),
		mInjectInitFragment(false), mInitFragmentInfo(NULL), mDrmKeyTagCount(0), mIndexingInProgress(false), mForceProcessDrmMetadata(false),
		mDuration(0), mLastMatchedDiscontPosition(-1), mCulledSeconds(0),mCulledSecondsOld(0),
		mEffectiveUrl(""), mPlaylistUrl(""), mFragmentURIFromIndex(""),
		mDiscontinuityIndexCount(0), mSyncAfterDiscontinuityInProgress(false), playlist(),
		index(), targetDurationSeconds(1), mDeferredDrmKeyMaxTime(0), startTimeForPlaylistSync(),
		context(parent), fragmentEncrypted(false), mKeyTagChanged(false), mLastKeyTagIdx(0), mDrmInfo(),
		mDrmMetaDataIndexPosition(0), mDrmMetaDataIndex(), mDiscontinuityIndex(), mKeyHashTable(), mPlaylistMutex(),
		mPlaylistIndexed(), mTrackDrmMutex(), mPlaylistType(ePLAYLISTTYPE_UNDEFINED), mReachedEndListTag(false),
		mByteOffsetCalculation(false),mSkipAbr(false),
		mCheckForInitialFragEnc(false), mFirstEncInitFragmentInfo(NULL), mDrmMethod(eDRM_KEY_METHOD_NONE)
		,mXStartTimeOFfset(0), mCulledSecondsAtStart(0.0)
		,mProgramDateTime(0.0)
		,mDiscontinuityCheckingOn(false)
		,mSkipSegmentOnError(true)
{
	memset(&playlist, 0, sizeof(playlist));
	memset(&index, 0, sizeof(index));
	memset(&startTimeForPlaylistSync, 0, sizeof(struct timeval));
	memset(&mDrmMetaDataIndex, 0, sizeof(mDrmMetaDataIndex));
	memset(&mDiscontinuityIndex, 0, sizeof(mDiscontinuityIndex));
	pthread_cond_init(&mPlaylistIndexed, NULL);
	pthread_mutex_init(&mPlaylistMutex, NULL);
	pthread_mutex_init(&mTrackDrmMutex, NULL);
	mCulledSecondsAtStart = aamp->culledSeconds;
	mProgramDateTime = aamp->mProgramDateTime;
	AAMPLOG_INFO("Restore PDT (%f) ",mProgramDateTime);
}
/***************************************************************************
* @fn ~TrackState
* @brief Destructor function
*
* @return void
***************************************************************************/
TrackState::~TrackState()
{
	aamp_Free(&playlist);
	int  maxCachedFragmentsPerTrack;
	GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached,maxCachedFragmentsPerTrack); 
	for (int j=0; j< maxCachedFragmentsPerTrack; j++)
	{
		aamp_Free(&cachedFragment[j].fragment);
	}
	FlushIndex();
	SAFE_DELETE(playContext);

	if (mCMSha1Hash)
	{
		free(mCMSha1Hash);
		mCMSha1Hash = NULL;
	}
	if (mDrmInfo.iv)
	{
		free(mDrmInfo.iv);
		mDrmInfo.iv = NULL;
	}
	pthread_cond_destroy(&mPlaylistIndexed);
	pthread_mutex_destroy(&mPlaylistMutex);
	pthread_mutex_destroy(&mTrackDrmMutex);
	
}
/***************************************************************************
* @fn Stop
* @brief Function to stop track download/playback
*
* @return void
***************************************************************************/
void TrackState::Stop(bool clearDRM)
{
	AbortWaitForCachedAndFreeFragment(true);

	if (playContext)
	{
		playContext->abort();
	}
	if (fragmentCollectorThreadStarted)
	{
		void *value_ptr = NULL;
		int rc = pthread_join(fragmentCollectorThreadID, &value_ptr);
		if (rc != 0)
		{
			AAMPLOG_WARN("***pthread_join fragmentCollectorThread returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			AAMPLOG_WARN("joined fragmentCollectorThread");
		}
#endif
		fragmentCollectorThreadStarted = false;
	}
	StopInjectLoop();

	//To be called after StopInjectLoop to avoid cues to be injected after cleanup
	if (mSubtitleParser)
	{
		mSubtitleParser->reset();
		mSubtitleParser->close();
	}

	// XIONE-2208: While waiting on fragmentCollectorThread to join the mDrm
	// can get initialized in fragmentCollectorThread.
	// Clear DRM data after join if this is required.
	if(mDrm && clearDRM)
	{
		mDrm->Release();
	}
}
/***************************************************************************
* @fn ~StreamAbstractionAAMP_HLS
* @brief Destructor function for StreamAbstractionAAMP_HLS
*
* @return void
***************************************************************************/
StreamAbstractionAAMP_HLS::~StreamAbstractionAAMP_HLS()
{
	/*Exit from ongoing  http fetch, drm operation,throttle. Mark fragment collector exit*/

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		TrackState *track = trackState[i];
		SAFE_DELETE(track);
	}

	aamp->SyncBegin();
	aamp_Free(&this->thumbnailManifest);
	aamp_Free(&this->mainManifest);
	aamp->CurlTerm(eCURLINSTANCE_VIDEO, DEFAULT_CURL_INSTANCE_COUNT);
	aamp->SyncEnd();
}
/***************************************************************************
* @fn Start
* @brief Function to create threads for track donwload
*
* @return void
***************************************************************************/
void TrackState::Start(void)
{
	if(playContext)
	{
		playContext->reset();
	}
	assert(!fragmentCollectorThreadStarted);
	if (0 == pthread_create(&fragmentCollectorThreadID, NULL, &FragmentCollector, this))
	{
		fragmentCollectorThreadStarted = true;
	}
	else
	{
		AAMPLOG_WARN("Failed to create FragmentCollector thread");
	}
	if(aamp->IsPlayEnabled())
	{
		StartInjectLoop();
	}
}
/***************************************************************************
* @fn Start
* @brief Function to start track initiaziation
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::Start(void)
{
#ifdef AAMP_HLS_DRM 
	aamp->mDRMSessionManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
#endif
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track->Enabled())
		{
			track->Start();
		}
	}
}
/***************************************************************************
* @fn Stop
* @brief Function to stop the HLS streaming
*
* @param clearChannelData[in] flag indicating to full stop or temporary stop
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::Stop(bool clearChannelData)
{
	aamp->DisableDownloads();
	ReassessAndResumeAudioTrack(true);
	AbortWaitForAudioTrackCatchup(false);

	//This is purposefully kept in a separate loop to avoid being hung
	//on pthread_join of fragmentCollectorThread
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			track->CancelDrmOperation(clearChannelData);
		}
	}

	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];

		/*Stop any waits for other track's playlist update*/
		TrackState *otherTrack = trackState[(iTrack == eTRACK_VIDEO)? eTRACK_AUDIO: eTRACK_VIDEO];
		if(otherTrack && otherTrack->Enabled())
		{
			otherTrack->StopWaitForPlaylistRefresh();
		}

		if(track && track->Enabled())
		{
			track->Stop(clearChannelData);
			if (!clearChannelData)
			{
				//Restore drm key state which was reset by drm_CancelKeyWait earlier since drm data is persisted
				track->RestoreDrmState();
			}
		}
	}

	if (clearChannelData)
	{
		if (aamp->GetCurrentDRM() != nullptr)
		{
			aamp->GetCurrentDRM()->cancelDrmSession();
		}
#ifdef AAMP_HLS_DRM 
		if(aamp->fragmentCdmEncrypted)
	        {
			// check for WV and PR , if anything to be flushed
			aamp->mStreamSink->ClearProtectionEvent();
		}
		if(ISCONFIGSET(eAAMPConfig_UseSecManager))
		{
			aamp->mDRMSessionManager->notifyCleanup();
		}
		aamp->mDRMSessionManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_INACTIVE);
		ReleaseDRMLicenseAcquireThread(aamp);
#endif
	}
	if(!clearChannelData)
	{
		aamp->EnableDownloads();
	}
}
/***************************************************************************
* @fn DumpProfiles
* @brief Function to log debug information on Stream/Media information
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::DumpProfiles(void)
{
	int profileCount = mProfileCount;
	if (profileCount)
	{
		for (int i = 0; i < profileCount; i++)
		{
			struct HlsStreamInfo *streamInfo = &this->streamInfo[i];
			AAMPLOG_WARN("stream[%d]:", i);
			if (streamInfo->uri) AAMPLOG_WARN("\tURI:%s", streamInfo->uri);
			AAMPLOG_WARN("\tBANDWIDTH:%ld", streamInfo->bandwidthBitsPerSecond);
			AAMPLOG_WARN("\tPROGRAM-ID:%ld", streamInfo->program_id);
			if (streamInfo->audio) AAMPLOG_WARN("\tAUDIO:%s", streamInfo->audio);
			if (streamInfo->codecs) AAMPLOG_WARN("\tCODECS:%s", streamInfo->codecs);
			AAMPLOG_WARN("\tRESOLUTION: %dx%d FPS:%f", streamInfo->resolution.width, streamInfo->resolution.height,streamInfo->resolution.framerate);
		}
		AAMPLOG_WARN("");
	}

	if (mMediaCount)
	{
		for (int i = 0; i < mMediaCount; i++)
		{
			MediaInfo *mediaInfo = &this->mediaInfo[i];
			AAMPLOG_WARN("media[%d]:", i);
			if (mediaInfo->uri) AAMPLOG_WARN("\tURI:%s", mediaInfo->uri);
			switch (mediaInfo->type)
			{
			case eMEDIATYPE_AUDIO:
				AAMPLOG_WARN("type:AUDIO");
				break;
			case eMEDIATYPE_VIDEO:
				AAMPLOG_WARN("type:VIDEO");
				break;
			default:
				break;
			}
			if (mediaInfo->group_id) AAMPLOG_WARN("\tgroup-id:%s", mediaInfo->group_id);
			if (mediaInfo->name) AAMPLOG_WARN("\tname:%s", mediaInfo->name);
			if (mediaInfo->language) AAMPLOG_WARN("\tlanguage:%s", mediaInfo->language);
			if (mediaInfo->autoselect)
			{
				AAMPLOG_WARN("\tAUTOSELECT");
			}
			if (mediaInfo->isDefault)
			{
				AAMPLOG_WARN("\tDEFAULT");
			}
		}
		AAMPLOG_WARN("");
	}
}

/***************************************************************************
* @fn GetStreamFormat
* @brief Function to get stream format
*
* @param primaryOutputFormat[out] video format
* @param audioOutputFormat[out] audio format
* @param audioOutputFormat[out] auxiliary audio format
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat)
{
	primaryOutputFormat = trackState[eMEDIATYPE_VIDEO]->streamOutputFormat;
	audioOutputFormat = trackState[eMEDIATYPE_AUDIO]->streamOutputFormat;
	auxOutputFormat = trackState[eMEDIATYPE_AUX_AUDIO]->streamOutputFormat;
}
/***************************************************************************
* @fn GetVideoBitrates
* @brief Function to get available video bitrates
*
* @return available video bitrates
***************************************************************************/
std::vector<long> StreamAbstractionAAMP_HLS::GetVideoBitrates(void)
{
	std::vector<long> bitrates;
	bitrates.reserve(GetProfileCount());
	if (mProfileCount)
	{
		for (int i = 0; i < mProfileCount; i++)
		{
			struct HlsStreamInfo *streamInfo = &this->streamInfo[i];
			//Not send iframe bw info, since AAMP has ABR disabled for trickmode
			if (!streamInfo->isIframeTrack && streamInfo->enabled)
			{
				bitrates.push_back(streamInfo->bandwidthBitsPerSecond);
			}
		}
	}
	return bitrates;
}
/***************************************************************************
* @fn GetAudioBitrates
* @brief Function to get available audio bitrates
*
* @return available audio bitrates
***************************************************************************/
std::vector<long> StreamAbstractionAAMP_HLS::GetAudioBitrates(void)
{
	//TODO: Impl audio bitrate getter
	return std::vector<long>();
}

/***************************************************************************
* @fn isThumbnailStream
* @brief Function to check if the provided stream is a thumbnail stream
*
* @return bool true on success
***************************************************************************/
static bool isThumbnailStream( const struct HlsStreamInfo *streamInfo )
{
	bool ret = false;
	if (streamInfo->codecs)
	{
		ret = SubStringMatch(streamInfo->codecs, streamInfo->codecs+4, "jpeg");
	}
	return ret;
}

/***************************************************************************
* @fn GetAvailableThumbnailTracks
* @brief Function to get available thumbnail tracks
*
* @return vector of available thumbnail tracks.
***************************************************************************/
std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableThumbnailTracks(void)
{
	std::vector<StreamInfo*> thumbnailTracks;
	for( int i = 0; i < mProfileCount; i++ )
	{
		struct HlsStreamInfo *streamInfo = &this->streamInfo[i];
		if( streamInfo->isIframeTrack && isThumbnailStream(streamInfo) )
		{
			struct StreamInfo *sptr = streamInfo;
			thumbnailTracks.push_back(sptr);
		}

	}
	return thumbnailTracks;
}

/***************************************************************************
* @fn IndexThumbnails
* @brief Function to index thumbnail manifest.
*
* @param *ptr pointer to thumbnail manifest
* @return Updated vector of available thumbnail tracks.
***************************************************************************/
static std::vector<TileInfo> IndexThumbnails( char *ptr )
{ // TODO: do we need to append nul pointer to downloaded playlist here?
	std::vector<TileInfo> rc;

	double startTime = 0;
	TileInfo tileInfo;
	memset( &tileInfo,0,sizeof(tileInfo) );

	while(ptr)
	{
		char *next = mystrpbrk(ptr);
		if(*ptr)
		{
			if (startswith(&ptr, "#EXT"))
			{
				if (startswith(&ptr, "INF:"))
				{
					tileInfo.tileSetDuration = atof(ptr);
				}
				else if (startswith(&ptr, "-X-TILES:"))
				{
					ParseAttrList(ptr, ParseTileInfCallback, &tileInfo);
				}
			}
			else
			{
				tileInfo.url = ptr;
				tileInfo.startTime = startTime;
				startTime += tileInfo.tileSetDuration;
				rc.push_back( tileInfo );
			}
		}
		ptr = next;
	}
	return rc;
}

/***************************************************************************
* @fn SetThumbnailTrack
* @brief Function to set thumbnail track for processing
*
* @param thumbnail index value indicating the track to select
* @return bool true on success.
***************************************************************************/
bool StreamAbstractionAAMP_HLS::SetThumbnailTrack( int thumbIndex )
{
	bool rc = false;
	indexedTileInfo.clear();
	aamp_Free( &thumbnailManifest );
	thumbnailManifest.len = 0;

	for( int iProfile=0; iProfile<mProfileCount; iProfile++ )
	{
		const HlsStreamInfo *streamInfo = &this->streamInfo[iProfile];
		if( streamInfo->isIframeTrack && isThumbnailStream(streamInfo) )
		{
			if( thumbIndex>0 )
			{
				thumbIndex--;
			}
			else
			{
				aamp->mthumbIndexValue = iProfile;

				std::string url;
				aamp_ResolveURL(url, aamp->GetManifestUrl(), streamInfo->uri,ISCONFIGSET(eAAMPConfig_PropogateURIParam));
				long http_error = 0;
				double downloadTime = 0;
				std::string tempEffectiveUrl;
				if( aamp->GetFile(url, &thumbnailManifest, tempEffectiveUrl, &http_error, &downloadTime, NULL, eCURLINSTANCE_MANIFEST_PLAYLIST,true,eMEDIATYPE_PLAYLIST_IFRAME) )
				{
					AAMPLOG_WARN("In StreamAbstractionAAMP_HLS: Configured Thumbnail");
					aamp_AppendNulTerminator( &thumbnailManifest );
					aamp->getAampCacheHandler()->InsertToPlaylistCache(streamInfo->uri, &thumbnailManifest, tempEffectiveUrl,false,eMEDIATYPE_PLAYLIST_IFRAME);
					indexedTileInfo = IndexThumbnails( thumbnailManifest.ptr );
					rc = true;
				}
				else
				{
					AAMPLOG_WARN("In StreamAbstractionAAMP_HLS: Unable to fetch the Thumbnail Manifest");
				}
				break;
			}
		}
	}
	return rc;
}

/***************************************************************************
* @fn GetThumbnailRangeData
* @brief Function to fetch the thumbnail data.
*
* @param tStart start duration of thumbnail data.
* @param tEnd end duration of thumbnail data.
* @param *baseurl base url of thumbnail images.
* @param *raw_w absolute width of the thumbnail spritesheet.
* @param *raw_h absolute height of the thumbnail spritesheet.
* @param *width width of each thumbnail tile.
* @param *height height of each thumbnail tile.
* @return Updated vector of available thumbnail data.
***************************************************************************/
std::vector<ThumbnailData> StreamAbstractionAAMP_HLS::GetThumbnailRangeData(double tStart, double tEnd, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
	std::vector<ThumbnailData> data;
	HlsStreamInfo *streaminfo = &this->streamInfo[aamp->mthumbIndexValue];
	if(!thumbnailManifest.ptr)
	{
		std::string tmpurl;
		if(aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(streaminfo->uri, &thumbnailManifest, tmpurl))
		{
			indexedTileInfo = IndexThumbnails( thumbnailManifest.ptr );
		}
		else
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Failed to retrieve the thumbnail playlist from cache.");
		}
	}

	ThumbnailData tmpdata;
	double totalSetDuration = 0;
	for( TileInfo &tileInfo : indexedTileInfo )
	{
		tmpdata.t = tileInfo.startTime;
		if( tmpdata.t > tEnd )
		{ // done
			break;
		}
		double tileSetEndTime = tmpdata.t + tileInfo.tileSetDuration;
		totalSetDuration += tileInfo.tileSetDuration;
		if( tileSetEndTime < tStart )
		{ // skip over
			continue;
		}
		tmpdata.url = tileInfo.url;
		*raw_w = streaminfo->resolution.width * tileInfo.numCols;
		*raw_h = streaminfo->resolution.height * tileInfo.numRows;
		tmpdata.d = tileInfo.posterDuration;
		bool done = false;
		for( int row=0; row<tileInfo.numRows && !done; row++ )
		{
			for( int col=0; col<tileInfo.numCols && !done; col++ )
			{
				double tNext = tmpdata.t+tileInfo.posterDuration;
				if( tNext >= tileSetEndTime )
				{ // clamp & bail
					tmpdata.d = tileSetEndTime - tmpdata.t;
					done = true;
				}
				if( tEnd >= tmpdata.t && tStart < tNext  )
				{
					tmpdata.x = col * streaminfo->resolution.width;
					tmpdata.y = row * streaminfo->resolution.height;
					data.push_back(tmpdata);
				}
				tmpdata.t = tNext;
			}
		}

		std::string url;
		aamp_ResolveURL(url, aamp->GetManifestUrl(), streaminfo->uri, ISCONFIGSET(eAAMPConfig_PropogateURIParam));
		*baseurl = url.substr(0,url.find_last_of("/\\")+1);
		*width = streaminfo->resolution.width;
		*height = streaminfo->resolution.height;
	}
	return data;
}

/***************************************************************************
* @fn NotifyFirstVideoPTS
* @brief Function to notify first video pts value from tsprocessor
*
* @param pts[in] base pts
* @param timeScale[in] time scale
* @return none
***************************************************************************/
void StreamAbstractionAAMP_HLS::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
	mFirstPTS = ((double)pts / (double)timeScale);
}

/**
 * @brief Signal start of subtitle renderering - should be sent at start of video presentation
 * 
 */
void StreamAbstractionAAMP_HLS::StartSubtitleParser()
{
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("sending init %.3f", mFirstPTS * 1000.0);
		subtitle->mSubtitleParser->init(seekPosition, static_cast<unsigned long long>(mFirstPTS * 1000.0));
		subtitle->mSubtitleParser->mute(aamp->subtitles_muted);
	}
}

/**
 * @brief Set subtitle pause state
 *
 */
void StreamAbstractionAAMP_HLS::PauseSubtitleParser(bool pause)
{
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("setting subtitles pause state = %d", pause);
		subtitle->mSubtitleParser->pause(pause);
	}
}

/***************************************************************************
* @fn DrmDecrypt
* @brief Function to decrypt the fragment for playback
*
* @param cachedFragment[in] CachedFragment struction pointer
* @param bucketTypeFragmentDecrypt[in] ProfilerBucketType enum
* @return bool true if successfully decrypted
***************************************************************************/
DrmReturn TrackState::DrmDecrypt( CachedFragment * cachedFragment, ProfilerBucketType bucketTypeFragmentDecrypt)
{
		DrmReturn drmReturn = eDRM_ERROR;

		pthread_mutex_lock(&mTrackDrmMutex);
		if (aamp->DownloadsAreEnabled())
		{
			// Update the DRM Context , if current active Drm Session is not received (mDrm)
			// or if Key Tag changed ( either with hash change )
			// For DAI scenaio-> Clear to Encrypted or Encrypted to Clear can happen.
			//      For Encr to clear,not to set SetDrmContext
			//      For Clear to Encr,SetDrmContext is called.If same hash then not SetDecrypto called
			if (fragmentEncrypted && (!mDrm || mKeyTagChanged))
			{
				SetDrmContext();
				mKeyTagChanged = false;
			}
			if(mDrm)
			{
				drmReturn = mDrm->Decrypt(bucketTypeFragmentDecrypt, cachedFragment->fragment.ptr,
						cachedFragment->fragment.len, MAX_LICENSE_ACQ_WAIT_TIME);

			}
		}
		pthread_mutex_unlock(&mTrackDrmMutex);

		if (drmReturn != eDRM_SUCCESS)
		{
			aamp->profiler.ProfileError(bucketTypeFragmentDecrypt, drmReturn);
		}
		return drmReturn;
}

/***************************************************************************
* @fn CreateInitVectorByMediaSeqNo
* @brief Function to create init vector using current media sequence number
*
* @param ui32Seqno[in] Current fragment's sequence number
* @return bool true if successfully created, false otherwise.
***************************************************************************/
bool TrackState::CreateInitVectorByMediaSeqNo ( unsigned int ui32Seqno )
{
	unsigned char *pui8IV=NULL;
	int i32loop=0;

	if (mDrmInfo.iv)
	{
		// Re-using memory if allocated from earlier call, instead of multiple malloc & free for every fragments.
		pui8IV = mDrmInfo.iv;
	}
	else
	{
		pui8IV = (unsigned char*)malloc(DRM_IV_LEN);
		if ( NULL == pui8IV )
		{
			AAMPLOG_WARN("IV MemAlloc error, Size:%d",DRM_IV_LEN);
			return false;
		}
	
		mDrmInfo.iv = pui8IV;
	}
	memset(pui8IV, 0x00, DRM_IV_LEN);

	/* From RFC8216 - Section 5.2,
	 * Keeping the Media Sequence Number's big-endian binary
	 * representation into a 16-octet (128-bit) buffer and padding
	 * (on the left) with zeros.
	 * Eg: Assume Media Seq No is 0x00045d23, then IV data will hold
	 * value : 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x04,0x5d,0x23.
     */
	for(i32loop=12; i32loop< 16; ++i32loop)
	{
		pui8IV[i32loop]=((ui32Seqno)>> (8*(15-i32loop)))&0xff;
	}

	return true;
}

/***************************************************************************
* @fn GetContext
* @brief Function to get current StreamAbstractionAAMP instance value
*
* @return StreamAbstractionAAMP instance
***************************************************************************/
StreamAbstractionAAMP* TrackState::GetContext()
{
	return context;
}
/***************************************************************************
* @fn GetMediaTrack
* @brief Function to get Media information for track type
*
* @param type[in] TrackType input
* @return MediaTrack structure pointer
***************************************************************************/
MediaTrack* StreamAbstractionAAMP_HLS::GetMediaTrack(TrackType type)
{
	return trackState[(int)type];
}
/***************************************************************************
* @fn UpdateDrmCMSha1Hash
* @brief Function to Update SHA1 Id for DRM Metadata
*
* @param ptr[in] ShaID string from DRM attribute
* @return void
***************************************************************************/
void TrackState::UpdateDrmCMSha1Hash(const char *ptr)
{
	bool drmDataChanged = false;
	if (NULL == ptr)
	{
		if (mCMSha1Hash)
		{
			free(mCMSha1Hash);
			mCMSha1Hash = NULL;
		}
	}
	else if (mCMSha1Hash)
	{
		if (0 != memcmp(ptr, (char*) mCMSha1Hash, DRM_SHA1_HASH_LEN))
		{
			if (!mIndexingInProgress)
			{
				AAMPLOG_WARN("[%s] Different DRM metadata hash. old - ", name);
				for (int i = 0; i< DRM_SHA1_HASH_LEN; i++)
				{
					printf("%c", mCMSha1Hash[i]);
				}
				printf(" new - ");
				for (int i = 0; i< DRM_SHA1_HASH_LEN; i++)
				{
					printf("%c", ptr[i]);
				}
				printf("\n");
			}
			drmDataChanged = true;
			memcpy(mCMSha1Hash, ptr, DRM_SHA1_HASH_LEN);
		}
		else if (!mIndexingInProgress)
		{
			AAMPLOG_INFO("Same DRM Metadata");
		}
	}
	else
	{
		if (!mIndexingInProgress)
		{
			AAMPLOG_WARN("[%s] New DRM metadata hash - ",name);
			for (int i = 0; i < DRM_SHA1_HASH_LEN; i++)
			{
				printf("%c", ptr[i]);
			}
			printf("\n");
		}
		mCMSha1Hash = (char*)malloc(DRM_SHA1_HASH_LEN);
		if(!mCMSha1Hash)
		{
			AAMPLOG_WARN("mCMSha1Hash  is null");  //CID:84607 - Null Returns
		}
		else
		{
			memcpy(mCMSha1Hash, ptr, DRM_SHA1_HASH_LEN);
			drmDataChanged = true;
		}
	}
	if(drmDataChanged)
	{
		int i;
		DrmMetadataNode* drmMetadataNode = (DrmMetadataNode*)mDrmMetaDataIndex.ptr;
		for (i = 0; i < mDrmMetaDataIndexCount; i++)
		{
			if(drmMetadataNode[i].sha1Hash)
			{
				if (0 == memcmp(mCMSha1Hash, drmMetadataNode[i].sha1Hash, DRM_SHA1_HASH_LEN))
				{
					if (!mIndexingInProgress)
					{
						AAMPLOG_INFO("mDrmMetaDataIndexPosition %d->%d", mDrmMetaDataIndexPosition, i);
					}
					mDrmMetaDataIndexPosition = i;
					break;
				}
			}
		}
		if (i == mDrmMetaDataIndexCount)
		{
			AAMPLOG_WARN("[%s] Couldn't find matching hash mDrmMetaDataIndexCount %d ",
			        name, mDrmMetaDataIndexCount);
			for (int j = 0; j < mDrmMetaDataIndexCount; j++)
			{
				if (drmMetadataNode[j].sha1Hash)
				{
					AAMPLOG_WARN("drmMetadataNode[%d].sha1Hash -- \n", j);
					for (int i = 0; i < DRM_SHA1_HASH_LEN; i++)
					{
						printf("%c", drmMetadataNode[j].sha1Hash[i]);
					}
					printf("\n");
				}
				else
				{
					AAMPLOG_WARN("drmMetadataNode[%d].sha1Hash NULL", j);
				}
			}
			for (int i = 0; i < playlist.len; i++)
			{
				char temp = playlist.ptr[i];
				if (temp == '\0')
				{
					temp = '\n';
				}
				putchar(temp);
			}
			assert(false);
		}
	}
}
/***************************************************************************
* @fn UpdateDrmIV
* @brief Function to update IV from DRM
*
* @param ptr[in] IV string from DRM attribute
* @return void
***************************************************************************/
void TrackState::UpdateDrmIV(const char *ptr)
{
	size_t len;
	unsigned char *iv = base16_Decode(ptr, (DRM_IV_LEN*2), &len); // 32 characters encoding 128 bits (16 bytes)
	assert(len == DRM_IV_LEN);
	if(mDrmInfo.iv)
	{
		if(0 != memcmp(mDrmInfo.iv, iv, DRM_IV_LEN))
		{
			AAMPLOG_TRACE("Different DRM IV - ");
#ifdef TRACE
			for (int i = 0; i< DRM_IV_LEN*2; i++)
			{
				printf("%c", ptr[i]);
			}
			printf("\n");
#endif
		}
		else
		{
			AAMPLOG_TRACE(" Same DRM IV");
		}
		free(mDrmInfo.iv);
	}
	mDrmInfo.iv = iv;
	AAMPLOG_TRACE(" [%s] Exit mDrmInfo.iv %p", name, mDrmInfo.iv);
}
/***************************************************************************
* @fn FetchPlaylist
* @brief Function to fetch playlist file
*
* @return void
***************************************************************************/

void TrackState::FetchPlaylist()
{

	long http_error = 0;   //CID:81884 - Initialization
	double downloadTime;
	long  main_error = 0;

	ProfilerBucketType bucketId = PROFILE_BUCKET_PLAYLIST_VIDEO; //type == eTRACK_VIDEO, eTRACK_AUDIO,...
	MediaType mType = eMEDIATYPE_PLAYLIST_VIDEO;

	if (type == eTRACK_AUDIO)
	{
		bucketId = PROFILE_BUCKET_PLAYLIST_AUDIO;
		mType = eMEDIATYPE_PLAYLIST_AUDIO;
	}
	else if (type == eTRACK_SUBTITLE)
	{
		bucketId = PROFILE_BUCKET_PLAYLIST_SUBTITLE;
		mType = eMEDIATYPE_PLAYLIST_SUBTITLE;
	}
	else if (type == eTRACK_AUX_AUDIO)
	{
		bucketId = PROFILE_BUCKET_PLAYLIST_AUXILIARY;
		mType = eMEDIATYPE_PLAYLIST_AUX_AUDIO;
	}

	int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
	AampCurlInstance dnldCurlInstance = aamp->GetPlaylistCurlInstance(mType , true);
	aamp->SetCurlTimeout(aamp->mPlaylistTimeoutMs,dnldCurlInstance);
	aamp->profiler.ProfileBegin(bucketId);

		(void) aamp->GetFile(mPlaylistUrl, &playlist, mEffectiveUrl, &http_error, &downloadTime, NULL, (unsigned int)dnldCurlInstance, true, mType);
		//update videoend info
		main_error = context->getOriginalCurlError(http_error);
	
		ManifestData manifestData(downloadTime * 1000, playlist.len);
		aamp->UpdateVideoEndMetrics( (IS_FOR_IFRAME(iCurrentRate,this->type) ? eMEDIATYPE_PLAYLIST_IFRAME :mType),this->GetCurrentBandWidth(),
									main_error,mEffectiveUrl, downloadTime, &manifestData);
		if(playlist.len)
			aamp->profiler.ProfileEnd(bucketId);

	aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs,dnldCurlInstance);
	if (!playlist.len)
	{
		AAMPLOG_WARN("Playlist download failed : %s  http response : %d", mPlaylistUrl.c_str(), (int)http_error);
		aamp->mPlaylistFetchFailError = http_error;
		aamp->profiler.ProfileError(bucketId, main_error);
		aamp->profiler.ProfileEnd(bucketId);
	}

}


/***************************************************************************
* @fn GetBWIndex
* @brief Function to get bandwidth index corresponding to bitrate
*
* @param bitrate Bitrate in bits per second
* @return bandwidth index
***************************************************************************/
int StreamAbstractionAAMP_HLS::GetBWIndex(long bitrate)
{
	int topBWIndex = 0;
	if (mProfileCount)
	{
		for (int i = 0; i < mProfileCount; i++)
		{
			struct HlsStreamInfo *streamInfo = &this->streamInfo[i];
			if (!streamInfo->isIframeTrack && streamInfo->enabled && streamInfo->bandwidthBitsPerSecond > bitrate)
			{
				--topBWIndex;
			}
		}
	}
	return topBWIndex;
}

/***************************************************************************
* @fn GetNextFragmentPeriodInfo
* @brief Function to get next playback position from start, to handle discontinuity
*
* @param periodIdx[out] Period Index
* @param offsetFromPeriodStart[out] Offset value
* @return void
***************************************************************************/
void TrackState::GetNextFragmentPeriodInfo(int &periodIdx, double &offsetFromPeriodStart, int &fragmentIdx)
{
	const IndexNode *index = (IndexNode *) this->index.ptr;
	const IndexNode *idxNode = NULL;
	periodIdx = -1;
	fragmentIdx = -1;
	offsetFromPeriodStart = 0;
	int idx;
	double prevCompletionTimeSecondsFromStart = 0;
	assert(context->rate > 0);
	for (idx = 0; idx < indexCount; idx++)
	{
		const IndexNode *node = &index[idx];
		if (node->completionTimeSecondsFromStart > playTarget)
		{
			AAMPLOG_WARN("(%s) Found node - rate %f completionTimeSecondsFromStart %f playTarget %f", name,
			        context->rate, node->completionTimeSecondsFromStart, playTarget);
			idxNode = node;
			break;
		}
		prevCompletionTimeSecondsFromStart = node->completionTimeSecondsFromStart;
	}
	if (idxNode)
	{
		if (idx > 0)
		{
			offsetFromPeriodStart = prevCompletionTimeSecondsFromStart;
			double periodStartPosition = 0;
			DiscontinuityIndexNode* discontinuityIndex = (DiscontinuityIndexNode*)mDiscontinuityIndex.ptr;
			for (int i = 0; i < mDiscontinuityIndexCount; i++)
			{
				AAMPLOG_TRACE("TrackState:: [%s] Loop periodItr %d idx %d first %d second %f",  name, i, idx,
				        discontinuityIndex[i].fragmentIdx, discontinuityIndex[i].position);
				if (discontinuityIndex[i].fragmentIdx > idx)
				{
					AAMPLOG_WARN("TrackState: [%s] Found periodItr %d idx %d first %d offsetFromPeriodStart %f",
					        name, i, idx, discontinuityIndex[i].fragmentIdx, periodStartPosition);

					fragmentIdx = discontinuityIndex[i].fragmentIdx;
					break;
				}
				periodIdx = i;
				periodStartPosition = discontinuityIndex[i].position;
			}
			offsetFromPeriodStart -= periodStartPosition;
		}
		AAMPLOG_WARN("TrackState: [%s] periodIdx %d offsetFromPeriodStart %f", name, periodIdx,
		        offsetFromPeriodStart);
	}
	else
	{
		AAMPLOG_WARN("TrackState: [%s] idxNode NULL", name);
	}
}
/***************************************************************************
* @fn GetPeriodStartPosition
* @brief Function to get Period start position for given period index,to handle discontinuity
*
* @param periodIdx[in] Period Index
* @return void
***************************************************************************/
double TrackState::GetPeriodStartPosition(int periodIdx)
{
	double offset = 0;
	AAMPLOG_WARN("TrackState: [%s] periodIdx %d periodCount %d", name, periodIdx,
	        (int) mDiscontinuityIndexCount);
	if (periodIdx < mDiscontinuityIndexCount)
	{
		int count = 0;
		DiscontinuityIndexNode* discontinuityIndex = (DiscontinuityIndexNode*)mDiscontinuityIndex.ptr;
		for (int i = 0; i < mDiscontinuityIndexCount; i++)
		{
			if (count == periodIdx)
			{
				offset = discontinuityIndex[i].position;
				AAMPLOG_WARN("TrackState: [%s] offset %f periodCount %d", name, offset,
				        (int) mDiscontinuityIndexCount);
				break;
			}
			else
			{
				count++;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("TrackState: [%s] WARNING periodIdx %d periodCount %d", name, periodIdx,
		        mDiscontinuityIndexCount);
	}
	return offset;
}
/***************************************************************************
* @fn GetNumberOfPeriods
* @brief Function to return number of periods stored in playlist
*
* @return int number of periods
***************************************************************************/
int TrackState::GetNumberOfPeriods()
{
	return mDiscontinuityIndexCount;
}

/***************************************************************************
* @fn HasDiscontinuityAroundPosition
* @brief Check if discontinuity present around given position
* @param[in] position Position to check for discontinuity
* @param[out] diffBetweenDiscontinuities discontinuity position minus input position
* @return true if discontinuity present around given position
***************************************************************************/
bool TrackState::HasDiscontinuityAroundPosition(double position, bool useDiscontinuityDateTime, double &diffBetweenDiscontinuities, double playPosition,double inputCulledSec,double inputProgramDateTime)
{
	bool discontinuityFound = false;
	bool useProgramDateTimeIfAvailable = UseProgramDateTimeIfAvailable();
	double discDiscardTolreanceInSec = (3 * targetDurationSeconds); /* Used by discontinuity handling logic to ensure both tracks have discontinuity tag around same area */
	double low = position - discDiscardTolreanceInSec;
	double high = position + discDiscardTolreanceInSec;
	int playlistRefreshCount = 0;
	diffBetweenDiscontinuities = DBL_MAX;
	bool newDiscHandling = true;
	pthread_mutex_lock(&mPlaylistMutex);
	mDiscontinuityCheckingOn = true;

	while (aamp->DownloadsAreEnabled())
	{
		if(ISCONFIGSET(eAAMPConfig_NewDiscontinuity))
		{
			// No condition to check DiscontinuityCount.Possible that in next refresh it will be available, 
			// Case where one discontinnuity in one track ,but other track not having it	
			// New code -enabled by config 
			DiscontinuityIndexNode* discontinuityIndex = (DiscontinuityIndexNode*)mDiscontinuityIndex.ptr;
			double deltaCulledSec = inputCulledSec - mCulledSeconds;
			bool foundmatchingdisc = false;
			for (int i = 0; i < mDiscontinuityIndexCount; i++)
			{
				// Live is complicated lets finish that 
					double discdatetime = 0.0;
					if(discontinuityIndex[i].programDateTime)
						discdatetime = ISO8601DateTimeToUTCSeconds(discontinuityIndex[i].programDateTime);


					if (IsLive())
					{
						AAMPLOG_WARN("[%s] Host loop %d mDiscontinuityIndexCount %d discontinuity-pos %f mCulledSeconds %f playlistRefreshTime:%f",name, i,
							mDiscontinuityIndexCount, discontinuityIndex[i].position, mCulledSeconds,mProgramDateTime);

						AAMPLOG_WARN("Visitor loop %d Input track position:%f useDateTime:%d CulledSeconds :%f playlistRefreshTime :%f DeltaCulledSec:%f", i,
							position ,useDiscontinuityDateTime, inputCulledSec , inputProgramDateTime , deltaCulledSec);
					}
					// check if date and time for discontinuity tag exists 
					if(useDiscontinuityDateTime && discdatetime)
					{
						// unfortunately date and time of calling track is passed in position arguement
						AAMPLOG_WARN("Comparing two disc date&time input pdt:%f pdt:%f diff:%f",position, discdatetime, fabs(discdatetime - position));
						if( fabs( discdatetime - position ) <= targetDurationSeconds )
						{
							foundmatchingdisc = true;
							diffBetweenDiscontinuities = discdatetime - position;
							AAMPLOG_WARN("[%s] Found the matching discontinuity with pdt at position:%f", name,position);
							break;
						}			
					}
					else
					{
						// No PDT , now compare the position based on culled delta 
						// Additional fragmentDuration is considered as rounding with decimal is missing the position when culled delta is same 
						// Ignore milli second accuracy 
						int limit1 = (int)(discontinuityIndex[i].position - abs(deltaCulledSec) - targetDurationSeconds - 1.0);
						int limit2 = (int)(discontinuityIndex[i].position + abs(deltaCulledSec) + targetDurationSeconds + 1.0);
						// DELIA-46385 
						// Due to increase in fragment duration and mismatch between audio and video,
						// Discontinuity pairing is missed 
						// Example : input posn:2290 index[12] position:2293 deltaCulled:0.000000 limit1:2291 limit2:2295
						// As a workaround , adding a buffer of +/- 1sec around the limit check .
						// Also instead of int conversion ,round is called for better 
						int roundedPosn = std::round(position);
						
						AAMPLOG_INFO("Comparing position input posn:%d index[%d] position:%d deltaCulled:%f limit1:%d limit2:%d  ",roundedPosn,i,(int)(discontinuityIndex[i].position),deltaCulledSec,
										limit1, limit2);
						if(roundedPosn >= limit1 && roundedPosn <= limit2 )
						{
							foundmatchingdisc = true;	
							AAMPLOG_WARN("[%s] Found the matching discontinuity at position:%f for position:%f",name,discontinuityIndex[i].position,position);
							break;
						}
					}
			}

			// Now the worst part . Not found matching discontinuity.How long to wait ??? 
			if(!foundmatchingdisc)
			{
				AAMPLOG_WARN("##[%s] Discontinuity not found mDuration %f playPosition %f  playlistType %d useStartTime %d ",
					 name, mDuration, playPosition, (int)mPlaylistType, (int)useDiscontinuityDateTime);
				if (IsLive())
				{						
					int maxPlaylistRefreshCount;
					bool liveNoTSB; 						
					if (aamp->IsTSBSupported() || aamp->IsInProgressCDVR())
					{
						maxPlaylistRefreshCount = MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_EVENT;
						liveNoTSB = false;
					}
					else
					{
						maxPlaylistRefreshCount = MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_LIVE;
						liveNoTSB = true;
					}
					// how long to wait?? Two ways to check .
					// 1. using Program Date and Time of playlist update .
					// 2. using the position and count of target duration.
					if(useProgramDateTimeIfAvailable)
					{
						// check if the track called have higher PDT or not 
						// if my refresh time is higher to calling track playlist track by an extra target duration,no point in waiting							
						if (mProgramDateTime >= inputProgramDateTime+targetDurationSeconds || playlistRefreshCount > maxPlaylistRefreshCount)
						{
							AAMPLOG_WARN("%s Discontinuity not found mProgramDateTime:%f > inputProgramDateTime:%f playlistRefreshCount:%d maxPlaylistRefreshCount:%d",name,
								mProgramDateTime,inputProgramDateTime,playlistRefreshCount,maxPlaylistRefreshCount);								
							break;
						}
					}
					else
					{

						if(!((playlistRefreshCount < maxPlaylistRefreshCount) && (liveNoTSB || (mDuration < (playPosition + discDiscardTolreanceInSec)))))
						{
							AAMPLOG_WARN("%s Discontinuity not found After playlistRefreshCount:%d",name,playlistRefreshCount);	
							break;
						}							
					}
					AAMPLOG_WARN("Wait for [%s] playlist update over for playlistRefreshCount %d", name, playlistRefreshCount);
					pthread_cond_wait(&mPlaylistIndexed, &mPlaylistMutex);
					playlistRefreshCount++;
				}
				else
				{
					break;
				}
			}
			else
			{
				discontinuityFound = true;
				break;
			}
		}
		else
		{
			bool waitForRefresh = false;

			// existing code logic 
			if (0 != mDiscontinuityIndexCount)
			{
				DiscontinuityIndexNode* discontinuityIndex = (DiscontinuityIndexNode*)mDiscontinuityIndex.ptr;
				for (int i = 0; i < mDiscontinuityIndexCount; i++)
				{
					if (IsLive())
					{
						AAMPLOG_WARN("[%s] loop %d mLastMatchedDiscontPosition %f mDiscontinuityIndexCount %d discontinuity-pos %f mCulledSeconds %f",
							 name, i, mLastMatchedDiscontPosition, mDiscontinuityIndexCount, discontinuityIndex[i].position, mCulledSeconds);
					}

					if ((mLastMatchedDiscontPosition < 0) || (discontinuityIndex[i].position + mCulledSeconds > mLastMatchedDiscontPosition))
					{
						if (!useDiscontinuityDateTime)
						{
							AAMPLOG_WARN("[%s] low %f high %f position %f discontinuity-pos %f discontinuity-discardTolreanceInSec %f mDiscontinuityIndexCount %d",
									 name, low, high, position, discontinuityIndex[i].position, discDiscardTolreanceInSec, mDiscontinuityIndexCount);
							if (low < discontinuityIndex[i].position && high > discontinuityIndex[i].position)
							{
								mLastMatchedDiscontPosition = discontinuityIndex[i].position + mCulledSeconds;
								discontinuityFound = true;
								AAMPLOG_WARN("[%s] Break :: mLastMatchedDiscontPosition %f", name, mLastMatchedDiscontPosition);
								break;
							}
						}
						else
						{
							double discPos = ISO8601DateTimeToUTCSeconds(discontinuityIndex[i].programDateTime);
							{
								AAMPLOG_WARN ("[%s] low %f high %f position %f discontinuity %f discontinuity-discardTolreanceInSec %f",
									name, low, high, position, discPos, discDiscardTolreanceInSec);

								if (low < discPos && high > discPos)
								{
									double diff = discPos - position;
									discontinuityFound = true;
									if (fabs(diff) < fabs(diffBetweenDiscontinuities))
									{
										diffBetweenDiscontinuities = diff;
										mLastMatchedDiscontPosition = discontinuityIndex[i].position + mCulledSeconds;
									}
									else
									{
										AAMPLOG_WARN("[%s] Break :: mLastMatchedDiscontPosition %f", name, mLastMatchedDiscontPosition);
										break;
									}
								}
							}
						}
					}
				}

				if (!discontinuityFound)
				{
					AAMPLOG_WARN("##[%s] Discontinuity not found in window low %f high %f position %f mLastMatchedDiscontPosition %f mDuration %f playPosition %f playlistRefreshCount %d playlistType %d useStartTime %d discontinuity-discardTolreanceInSec %f",
						name, low, high, position, mLastMatchedDiscontPosition, mDuration, playPosition, playlistRefreshCount, (int)mPlaylistType, (int)useDiscontinuityDateTime, discDiscardTolreanceInSec);

					waitForRefresh = true;
				}
				else
				{
					AAMPLOG_WARN("[%s] Break :: mLastMatchedDiscontPosition %f", name, mLastMatchedDiscontPosition);
					break;
				}
			}
			else
			{
				waitForRefresh = true;
				AAMPLOG_WARN("##[%s] Discontinuity not found, wait for next playlist refresh, present mDiscontinuityIndexCount %d", name, mDiscontinuityIndexCount);
			}

			if (waitForRefresh)
			{
				if (IsLive())
				{
					int maxPlaylistRefreshCount;
					bool liveNoTSB;
					if (aamp->IsTSBSupported() || aamp->IsInProgressCDVR())
					{
						maxPlaylistRefreshCount = MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_EVENT;
						liveNoTSB = false;
					}
					else
					{
						maxPlaylistRefreshCount = MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_LIVE;
						liveNoTSB = true;
					}

					if ((playlistRefreshCount < maxPlaylistRefreshCount) && (liveNoTSB || (mDuration < (playPosition + discDiscardTolreanceInSec))))
					{
						AAMPLOG_WARN("Waiting for [%s] playlist update mDuration %f mCulledSeconds %f playlistRefreshCount %d",
						        name, mDuration, mCulledSeconds, playlistRefreshCount);
						pthread_cond_wait(&mPlaylistIndexed, &mPlaylistMutex);
						AAMPLOG_WARN("Wait for [%s] playlist update over for playlistRefreshCount %d", name, playlistRefreshCount);
						playlistRefreshCount++;
					}
					else
					{
						AAMPLOG_INFO("[%s] Break", name);
						break;
					}
				}
				else
				{
					AAMPLOG_INFO("[%s] Break", name);
					break;
				}
			}
			else
			{
				AAMPLOG_INFO("[%s] Break", name);
				break;
			}
		}

	}
	mDiscontinuityCheckingOn = false;
	pthread_mutex_unlock(&mPlaylistMutex);
	return discontinuityFound;
}

/***************************************************************************
* @fn FetchInitFragment
* @brief Function to fetch init fragment
*
* @return void
***************************************************************************/
void TrackState::FetchInitFragment()
{
	int timeoutMs = -1;

	if (IsLive())
	{
		timeoutMs = context->maxIntervalBtwPlaylistUpdateMs - (int) (aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
		if(timeoutMs < 0)
		{
			timeoutMs = 0;
		}
	}
	if (mInjectInitFragment && mInitFragmentInfo)
	{
		if (!WaitForFreeFragmentAvailable(timeoutMs))
		{
			return;
		}

		long http_code = -1;
		bool forcePushEncryptedHeader = (!fragmentEncrypted && mCheckForInitialFragEnc);
		// Check if we have encrypted header successfully parsed to push ahead
		if (forcePushEncryptedHeader && mFirstEncInitFragmentInfo == NULL)
		{
			AAMPLOG_WARN("TrackState::[%s] first encrypted init-fragment is NULL! fragmentEncrypted-%d",  name, fragmentEncrypted);
			forcePushEncryptedHeader = false;
		}

		ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia((MediaType)type, true);
		aamp->profiler.ProfileBegin(bucketType);
		if(FetchInitFragmentHelper(http_code, forcePushEncryptedHeader))
		{
			aamp->profiler.ProfileEnd(bucketType);

			CachedFragment* cachedFragment = GetFetchBuffer(false);
			if (cachedFragment->fragment.ptr)
			{
				cachedFragment->duration = 0;
				cachedFragment->position = playTarget - playTargetOffset;
				cachedFragment->discontinuity = discontinuity;
			}

			// If forcePushEncryptedHeader, don't reset the playTarget as the original init header has to be pushed next
			if (!forcePushEncryptedHeader)
			{
				mInjectInitFragment = false;
			}

			discontinuity = false; //reset discontinuity which has been set for init fragment now
			mSkipAbr = true; //Skip ABR, since last fragment cached is init fragment.
			mCheckForInitialFragEnc = false; //Push encrypted header is a one-time operation
			mFirstEncInitFragmentInfo = NULL; //reset init fragemnt, since ecnypted header already pushed

			UpdateTSAfterFetch();
		}
		else if (type == eTRACK_VIDEO && aamp->CheckABREnabled() && !context->CheckForRampDownLimitReached())
		{
			// Attempt rampdown for init fragment to get playable profiles.
			// TODO: Remove profile if init fragment is not available from ABR.

			mFirstEncInitFragmentInfo = NULL; // need to reset the previous profile's first encrypted init fragment in case of init fragment rampdown.
			AAMPLOG_WARN("Reset mFirstEncInitFragmentInfo since rampdown for another profile");

			if (context->CheckForRampDownProfile(http_code))
			{
				AAMPLOG_INFO("Init fragment fetch failed, Successfully ramped down to lower profile");
				context->mCheckForRampdown = true;
			}
			else
			{
				// Failed to get init framgent from all attempted profiles
				if (aamp->DownloadsAreEnabled())
				{
					AAMPLOG_ERR("TrackState::Init fragment fetch failed");
					aamp->profiler.ProfileError(bucketType, http_code);
					aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
				}
				context->mRampDownCount = 0;
			}
			AAMPLOG_WARN("Error while fetching init fragment:%s, failedCount:%d. decrementing profile", name, segDLFailCount);
		}
		else if (aamp->DownloadsAreEnabled())
		{
			long http_error = context->getOriginalCurlError(http_code);
			AAMPLOG_ERR("TrackState::Init fragment fetch failed");
			aamp->profiler.ProfileError(bucketType, http_error);
			aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
		}
	}
	else if (!mInitFragmentInfo)
	{
		AAMPLOG_ERR("TrackState::Need to push init fragment but fragment info is missing! mInjectInitFragment(%d)", mInjectInitFragment);
		mInjectInitFragment = false;
	}

}

/***************************************************************************
* @brief Helper to fetch init fragment for fragmented mp4 format
* @return true if success
***************************************************************************/
bool TrackState::FetchInitFragmentHelper(long &http_code, bool forcePushEncryptedHeader)
{
	bool ret = false;
	std::istringstream initFragmentUrlStream;

	// If the first init fragment is of a clear fragment, we push an encrypted fragment's
	// init data first to let qtdemux know we will need decryptor plugins
	AAMPLOG_TRACE("TrackState::[%s] fragmentEncrypted-%d mFirstEncInitFragmentInfo-%s", name, fragmentEncrypted, mFirstEncInitFragmentInfo);
	if (forcePushEncryptedHeader)
	{
		//Push encrypted fragment's init data first
		AAMPLOG_WARN("TrackState::[%s] first init-fragment is unencrypted.! Pushing encrypted init-header", name);
		initFragmentUrlStream = std::istringstream(std::string(mFirstEncInitFragmentInfo));
	}
	else
	{
		initFragmentUrlStream = std::istringstream(std::string(mInitFragmentInfo));
	}
	std::string line;
	std::getline(initFragmentUrlStream, line);
	if (!line.empty())
	{
		const char *range = NULL;
		char rangeStr[128];
		std::string uri;
		AAMPLOG_TRACE(" line %s", line.c_str());
		size_t uriTagStart = line.find("URI=");
		if (uriTagStart != std::string::npos)
		{
			std::string uriStart = line.substr(uriTagStart + 5);
			AAMPLOG_TRACE(" uriStart %s", uriStart.c_str());
			size_t uriTagEnd = uriStart.find("\"");
			if (uriTagEnd != std::string::npos)
			{
				AAMPLOG_TRACE(" uriTagEnd %d", (int) uriTagEnd);
				uri = uriStart.substr(0, uriTagEnd);
				AAMPLOG_TRACE(" uri %s", uri.c_str());
			}
			else
			{
				AAMPLOG_ERR("URI parse error. Tag end not found");
			}
		}
		else
		{
			AAMPLOG_ERR("URI parse error. URI= not found");
		}
		size_t byteRangeTagStart = line.find("BYTERANGE=");
		if (byteRangeTagStart != std::string::npos)
		{
			std::string byteRangeStart = line.substr(byteRangeTagStart + 11);
			size_t byteRangeTagEnd = byteRangeStart.find("\"");
			if (byteRangeTagEnd != std::string::npos)
			{
				std::string byteRange = byteRangeStart.substr(0, byteRangeTagEnd);
				AAMPLOG_TRACE(" byteRange %s", byteRange.c_str());
				if (!byteRange.empty())
				{
					size_t offsetIdx = byteRange.find("@");
					if (offsetIdx != std::string::npos)
					{
						int offsetVal = stoi(byteRange.substr(offsetIdx + 1));
						int rangeVal = stoi(byteRange.substr(0, offsetIdx));
						int next = offsetVal + rangeVal;
						sprintf(rangeStr, "%d-%d", offsetVal, next - 1);
						AAMPLOG_INFO("TrackState::rangeStr %s", rangeStr);
						range = rangeStr;
					}
				}
			}
			else
			{
				AAMPLOG_ERR("TrackState::byteRange parse error. Tag end not found byteRangeStart %s",
						 byteRangeStart.c_str());
			}
		}
		if (!uri.empty())
		{
			std::string fragmentUrl;
			aamp_ResolveURL(fragmentUrl, mEffectiveUrl, uri.c_str(), ISCONFIGSET(eAAMPConfig_PropogateURIParam));
			std::string tempEffectiveUrl;
			CachedFragment* cachedFragment = GetFetchBuffer(true);
			AAMPLOG_WARN("TrackState::[%s] init-fragment = %s", name, fragmentUrl.c_str());
			int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play

			MediaType actualType = eMEDIATYPE_INIT_VIDEO;
			if(IS_FOR_IFRAME(iCurrentRate,type))
			{
				actualType = eMEDIATYPE_INIT_IFRAME;
			}
			else if (eTRACK_AUDIO == type)
			{
				actualType = eMEDIATYPE_INIT_AUDIO;
			}
			else if (eTRACK_SUBTITLE == type)
			{
				actualType = eMEDIATYPE_INIT_SUBTITLE;
			}
			else if (eTRACK_AUX_AUDIO == type)
			{
				actualType = eMEDIATYPE_INIT_AUX_AUDIO;
			}

#ifdef CHECK_PERFORMANCE
			long long ts_start, ts_end;
			ts_start = aamp_GetCurrentTimeMS();
#endif /* CHECK_PERFORMANCE */
			bool fetched = aamp->getAampCacheHandler()->RetrieveFromInitFragCache(fragmentUrl, &cachedFragment->fragment, tempEffectiveUrl);

#ifdef CHECK_PERFORMANCE
			ts_end = aamp_GetCurrentTimeMS();
			if(fetched)
			AAMPLOG_TRACE("---------------CacheRead Time diff:%llu---------------" , ts_end-ts_start);
#endif /* CHECK_PERFORMANCE */

			if ( !fetched )
			{
				double downloadTime;
				fetched = aamp->GetFile(fragmentUrl, &cachedFragment->fragment, tempEffectiveUrl, &http_code, &downloadTime, range,
						type, false,  actualType);

#ifdef CHECK_PERFORMANCE
				if(fetched)
				AAMPLOG_TRACE("---------------CurlReq Time diff:%llu---------------" , downloadTime);
#endif /* CHECK_PERFORMANCE */
	
				long main_error = context->getOriginalCurlError(http_code);
				aamp->UpdateVideoEndMetrics(actualType, this->GetCurrentBandWidth(), main_error, mEffectiveUrl, downloadTime);

				if ( fetched )
				aamp->getAampCacheHandler()->InsertToInitFragCache ( fragmentUrl, &cachedFragment->fragment, tempEffectiveUrl, actualType);
			}
			if (!fetched)
			{
				AAMPLOG_ERR("TrackState::aamp_GetFile failed");
				aamp_Free(&cachedFragment->fragment);
			}
			else
			{
				ret = true;
			}
		}
		else
		{
			AAMPLOG_ERR("TrackState::Could not parse init fragment URI. line %s", line.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("TrackState::Init fragment URI parse error");
	}
	return ret;
}

/***************************************************************************
* @fn StopInjection
* @brief Function to stop fragment injection
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::StopInjection(void)
{
	//invoked at times of discontinuity. Audio injection loop might have already exited here
	ReassessAndResumeAudioTrack(true);

	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			track->StopInjection();
		}
	}
}

/***************************************************************************
* @fn StopInjection
* @brief Function to stop fragment injection
*
* @return void
***************************************************************************/
void TrackState::StopInjection()
{
	AbortWaitForCachedFragment();
	aamp->StopTrackInjection((MediaType) type);
	if (playContext)
	{
		playContext->abort();
	}
	StopInjectLoop();
}

/***************************************************************************
* @fn StartInjection
* @brief starts fragment injection
*
* @return void
***************************************************************************/
void TrackState::StartInjection()
{
	aamp->ResumeTrackInjection((MediaType) type);
	if (playContext)
	{
		playContext->reset();
	}
	StartInjectLoop();
}

/***************************************************************************
* @fn StartInjection
* @brief Function to start fragment injection
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::StartInjection(void)
{
	mTrackState = eDISCONTIUITY_FREE;
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			track->StartInjection();
		}
	}
}

/***************************************************************************
* @fn StopWaitForPlaylistRefresh
* @brief Stop wait for playlist refresh
*
* @return void
***************************************************************************/
void TrackState::StopWaitForPlaylistRefresh()
{
	AAMPLOG_WARN("track [%s]", name);
	pthread_mutex_lock(&mPlaylistMutex);
	pthread_cond_signal(&mPlaylistIndexed);
	pthread_mutex_unlock(&mPlaylistMutex);
}

/***************************************************************************
* @fn CancelDrmOperation
* @brief Cancel all DRM operations
*
* @param[in] clearDRM flag indicating if DRM resources to be freed or not
* @return void
***************************************************************************/
void TrackState::CancelDrmOperation(bool clearDRM)
{
	//Calling mDrm is required for AES encrypted assets which doesn't have AveDrmManager
	if (mDrm)
	{
		//To force release mTrackDrmMutex mutex held by drm_Decrypt in case of clearDRM
		mDrm->CancelKeyWait();
		if (clearDRM)
		{
			if ((aamp->GetCurrentDRM() == nullptr) || (!aamp->GetCurrentDRM()->canCancelDrmSession()))
			{
				pthread_mutex_lock(&mTrackDrmMutex);
				mDrm->Release();
				pthread_mutex_unlock(&mTrackDrmMutex);
			}
		}
	}
}

/***************************************************************************
* @fn RestoreDrmState
* @brief Restore DRM states
*
* @return void
***************************************************************************/
void TrackState::RestoreDrmState()
{
	if (mDrm)
	{
		mDrm->RestoreKeyState();
	}
}

/***************************************************************************
* @fn FindTimedMetadata
* @brief Function to search playlist for subscribed tags
*
* @return void
***************************************************************************/
void TrackState::FindTimedMetadata(bool reportBulkMeta, bool bInitCall)
{
	double totalDuration = 0.0;
	if (ISCONFIGSET(eAAMPConfig_EnableSubscribedTags) && (eTRACK_VIDEO == type))
	{
		pthread_mutex_lock(&mPlaylistMutex);
		if (playlist.ptr)
		{
			char *ptr = GetNextLineStart(playlist.ptr);
			while (ptr)
			{
				if(startswith(&ptr,"#EXT"))
				{
					if (startswith(&ptr, "INF:"))
					{
						totalDuration += atof(ptr);
					}
					for (int i = 0; i < aamp->subscribedTags.size(); i++)
					{
						const char* data = aamp->subscribedTags.at(i).data();
						if(startswith(&ptr, (data + 4))) // remove the TAG and only keep value(content) in PTR
						{
							ptr++; // skip the ":"
							int nb = (int)FindLineLength(ptr);
							long long positionMilliseconds = (long long) std::round((mCulledSecondsAtStart + mCulledSeconds + totalDuration) * 1000.0);
							//AAMPLOG_INFO("mCulledSecondsAtStart:%f mCulledSeconds :%f totalDuration: %f posnMs:%lld playposn:%lld",mCulledSecondsAtStart,mCulledSeconds,totalDuration,positionMilliseconds,aamp->GetPositionMs());
							//AAMPLOG_WARN("Found subscribedTag[%d]: @%f cull:%f Posn:%lld '%.*s'", i, totalDuration, mCulledSeconds, positionMilliseconds, nb, ptr);
							if(reportBulkMeta)
							{
								aamp->SaveTimedMetadata(positionMilliseconds, data, ptr, nb);
							}
							else
							{
								aamp->ReportTimedMetadata(positionMilliseconds, data, ptr, nb,bInitCall);
							}
							break;
						}
					}
				}
				ptr=GetNextLineStart(ptr);
			}
		}
		pthread_mutex_unlock(&mPlaylistMutex);
	}
	AAMPLOG_TRACE(" Exit");
}

/***************************************************************************
* @fn ConfigureAudioTrack
* @brief Function to select the audio track and update AudioProfileIndex
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::ConfigureAudioTrack()
{
	//AudioTrackInfo track = aamp->GetPreferredAudioTrack();
	currentAudioProfileIndex = -1;
	//if (!track.index.empty())
	//{
	//	currentAudioProfileIndex = std::stoi(track.index);
	//}
	//else
	if(mMediaCount)
	{
		currentAudioProfileIndex = GetBestAudioTrackByLanguage();
	}
	AAMPLOG_WARN("Audio profileIndex selected :%d", currentAudioProfileIndex);
}

/***************************************************************************
* @fn ConfigureVideoProfiles
* @brief Function to select the best match video profiles based on audio and filters
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::ConfigureVideoProfiles()
{
	std::string audiogroupId ;
	long minBitrate = aamp->GetMinimumBitrate();
	long maxBitrate = aamp->GetMaximumBitrate();
	bool iProfileCapped = false;
	bool resolutionCheckEnabled = ISCONFIGSET(eAAMPConfig_LimitResolution); 
	if(resolutionCheckEnabled && (0 == aamp->mDisplayWidth || 0 == aamp->mDisplayHeight))
	{
		resolutionCheckEnabled = false;
	}

	if(rate != AAMP_NORMAL_PLAY_RATE && mIframeAvailable)
	{
		// Add all the iframe tracks
		int iFrameSelectedCount = 0;
		int iFrameAvailableCount = 0;
		for (;;)
		{
			bool loopAgain = false;
			for (int j = 0; j < mProfileCount; j++)
			{
				struct HlsStreamInfo *streamInfo = &this->streamInfo[j];
				streamInfo->enabled = false;
				if(streamInfo->isIframeTrack && !(isThumbnailStream(streamInfo)))
				{
					iFrameAvailableCount++;
					if (resolutionCheckEnabled && (streamInfo->resolution.width > aamp->mDisplayWidth))
					{
						AAMPLOG_INFO("Iframe Video Profile ignoring higher res=%d:%d display=%d:%d BW=%ld", streamInfo->resolution.width, streamInfo->resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, streamInfo->bandwidthBitsPerSecond);
						iProfileCapped = true;
					}
					else if ((streamInfo->bandwidthBitsPerSecond >= minBitrate) && (streamInfo->bandwidthBitsPerSecond <= maxBitrate))
					{
						if (ISCONFIGSET(eAAMPConfig_Disable4K) &&
							(streamInfo->resolution.height > 1080 || streamInfo->resolution.width > 1920))
						{
							continue;
						}
						//Update profile resolution with VideoEnd Metrics object.
						aamp->UpdateVideoEndProfileResolution( eMEDIATYPE_IFRAME,
								streamInfo->bandwidthBitsPerSecond,
								streamInfo->resolution.width,
								streamInfo->resolution.height );

						mAbrManager.addProfile({
								streamInfo->isIframeTrack,
								streamInfo->bandwidthBitsPerSecond,
								streamInfo->resolution.width,
								streamInfo->resolution.height,
								"",
								j});

						streamInfo->enabled = true;
						iFrameSelectedCount++;

						AAMPLOG_INFO("Video Profile added to ABR for Iframe, userData=%d BW =%ld res=%d:%d display=%d:%d pc:%d", j, streamInfo->bandwidthBitsPerSecond, streamInfo->resolution.width, streamInfo->resolution.height,aamp->mDisplayWidth,aamp->mDisplayHeight,iProfileCapped);
					}
				}
			}
			if (iFrameAvailableCount > 0 && 0 == iFrameSelectedCount && resolutionCheckEnabled)
			{
				resolutionCheckEnabled = iProfileCapped = false;
				loopAgain = true;
			}
			if (false == loopAgain)
			{
				break;
			}
		}
		if (!aamp->IsTSBSupported() && iProfileCapped)
		{
			aamp->mProfileCappedStatus = true;
		}
		if(iFrameSelectedCount == 0 && iFrameAvailableCount !=0)
		{
			// Something wrong , though iframe available , but not selected due to bitrate restriction
			AAMPLOG_WARN("No Iframe available matching bitrate criteria Low[%ld] High[%ld]. Total Iframe available:%d",minBitrate,maxBitrate,iFrameAvailableCount);
		}
		else if(iFrameSelectedCount)
		{
			// this is to sort the iframe tracks
			mAbrManager.updateProfile();
		}
	}
	else if(rate == AAMP_NORMAL_PLAY_RATE || rate == AAMP_RATE_PAUSE)
	{
		// Filters to add a video track
		// 1. It should match the audio groupId selected
		// 2. Last filter for min and max bitrate
		// 3. Make sure filters for disableATMOS/disableEC3/disableAAC is applied

		// Get the initial configuration to filter the profiles
		bool bDisableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
		bool bDisableAC3 = bDisableEC3;
		// bringing in parity with DASH , if EC3 is disabled ,then ATMOS also will be disabled
		bool bDisableATMOS = (bDisableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);
		bool bDisableAAC = false;

		// Check if any demuxed audio exists , if muxed it will be -1
		if (currentAudioProfileIndex >= 0 )
		{
			// Check if audio group id exists
			audiogroupId = mediaInfo[currentAudioProfileIndex].group_id;
			AAMPLOG_WARN("Audio groupId selected:%s", audiogroupId.c_str());
		}

		int vProfileCountSelected = 0;
		do{
			int aacProfiles = 0, ac3Profiles = 0, ec3Profiles = 0, atmosProfiles = 0;
			vProfileCountSelected = 0;
			int vProfileCountAvailable = 0;
			int audioProfileMatchedCount = 0;
			int bitrateMatchedCount = 0;
			int resolutionMatchedCount = 0;
			bool ignoreBitRateRangeCheck = false;
			int availableCountATMOS = 0, availableCountEC3 = 0, availableCountAC3 = 0;
			StreamOutputFormat selectedAudioType = FORMAT_INVALID;
			bool bVideoResolutionCheckEnabled = false;
			bool bVideoThumbnailResolutionCheckEnabled = false;

			for (int j = 0; j < mProfileCount; j++)
			{
				struct HlsStreamInfo *streamInfo = &this->streamInfo[j];
				streamInfo->enabled = false;
				bool ignoreProfile = false;
				bool clearProfiles = false;
				if(!streamInfo->isIframeTrack)
				{
					vProfileCountAvailable++;

					// complex criteria
					// 1. First check if same audio group available
					//		1.1 If available , pick the profiles for the bw range
					//		1.2 Pick the best audio type

					if((!audiogroupId.empty() && streamInfo->audio && !audiogroupId.compare(streamInfo->audio)) || audiogroupId.empty())
					{
						audioProfileMatchedCount++;
						if(resolutionCheckEnabled && (streamInfo->resolution.width > aamp->mDisplayWidth))
						{
							iProfileCapped = true;
							AAMPLOG_INFO("Video Profile ignored Bw=%ld res=%d:%d display=%d:%d", streamInfo->bandwidthBitsPerSecond, streamInfo->resolution.width, streamInfo->resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight);
						}
						else if(ISCONFIGSET(eAAMPConfig_Disable4K) && (streamInfo->resolution.height > 1080 || streamInfo->resolution.width > 1920))
						{
							AAMPLOG_INFO("Video Profile ignored for disabled 4k content");
						}
						else
						{
							resolutionMatchedCount++;
							if (((streamInfo->bandwidthBitsPerSecond < minBitrate) || (streamInfo->bandwidthBitsPerSecond > maxBitrate)) && !ignoreBitRateRangeCheck)
							{
								iProfileCapped = true;
							}
							else
							{
								bitrateMatchedCount++;

								switch(streamInfo->audioFormat)
								{
									case FORMAT_AUDIO_ES_AAC:
										if(bDisableAAC)
										{
											AAMPLOG_INFO("AAC Profile ignored[%s]", streamInfo->uri);
											ignoreProfile = true;
										}
										else
										{
											aacProfiles++;
										}
										break;

									case FORMAT_AUDIO_ES_AC3:
										availableCountAC3++;
										if(bDisableAC3)
										{
											AAMPLOG_INFO("AC3 Profile ignored[%s]",streamInfo->uri);
											ignoreProfile = true;
										}
										else
										{
											// found AC3 profile , disable AAC profiles from adding
											ac3Profiles++;
											bDisableAAC = true;
											if(aacProfiles)
											{
												// if already aac profiles added , clear it from local table and ABR table
												aacProfiles = 0;
												clearProfiles = true;
											}
										}
										break;

									case FORMAT_AUDIO_ES_EC3:
										availableCountEC3++;
										if(bDisableEC3)
										{
											AAMPLOG_INFO("EC3 Profile ignored[%s]", streamInfo->uri);
											ignoreProfile = true;
										}
										else
										{ // found EC3 profile , disable AAC and AC3 profiles from adding
											ec3Profiles++;
											bDisableAAC = true;
											bDisableAC3 = true;
											if(aacProfiles || ac3Profiles)
											{
												// if already aac or ac3 profiles added , clear it from local table and ABR table
												aacProfiles = ac3Profiles = 0;
												clearProfiles = true;
											}
										}
										break;

									case FORMAT_AUDIO_ES_ATMOS:
										availableCountATMOS++;
										if(bDisableATMOS)
										{
											AAMPLOG_INFO("ATMOS Profile ignored[%s]", streamInfo->uri);
											ignoreProfile = true;
										}
										else
										{ // found ATMOS Profile , disable AC3, EC3 and AAC profile from adding
											atmosProfiles++;
											bDisableAAC = true;
											bDisableAC3 = true;
											bDisableEC3 = true;
											if(aacProfiles || ac3Profiles || ec3Profiles)
											{
												// if already aac or ac3 or ec3 profiles added , clear it from local table and ABR table
												aacProfiles = ac3Profiles = ec3Profiles = 0;
												clearProfiles = true;
											}
										}
										break;

									default:
										AAMPLOG_WARN("unknown codec string to categorize :%s ",streamInfo->codecs);
										break;
								}

								if(clearProfiles)
								{
									j = 0;
									vProfileCountAvailable = 0;
									audioProfileMatchedCount = 0;
									bitrateMatchedCount = 0;
									vProfileCountSelected = 0;
									availableCountEC3 = 0;
									availableCountAC3 = 0;
									availableCountATMOS = 0;
									// Continue the loop from start of profile
									continue;
								}

								if(!ignoreProfile)
								{
									streamInfo->enabled = true;
									vProfileCountSelected ++;
									selectedAudioType = streamInfo->audioFormat;
								}
							}
						}
					}
				}
				else if( isThumbnailStream(streamInfo) )
				{
					vProfileCountSelected ++;
				}
			}

			if (aamp->mPreviousAudioType != selectedAudioType)
			{
				AAMPLOG_WARN("AudioType Changed %d -> %d",
						 aamp->mPreviousAudioType, selectedAudioType);
				aamp->mPreviousAudioType = selectedAudioType;
				SetESChangeStatus();
			}

			// Now comes next set of complex checks for bad streams
			if(vProfileCountSelected)
			{
				for (int j = 0; j < mProfileCount; j++)
				{
					struct HlsStreamInfo *streamInfo = &this->streamInfo[j];
					if(streamInfo->enabled)
					{
						//Update profile resolution with VideoEnd Metrics object.
						aamp->UpdateVideoEndProfileResolution( eMEDIATYPE_VIDEO,
								streamInfo->bandwidthBitsPerSecond,
								streamInfo->resolution.width,
								streamInfo->resolution.height );

						mAbrManager.addProfile({
								streamInfo->isIframeTrack,
								streamInfo->bandwidthBitsPerSecond,
								streamInfo->resolution.width,
								streamInfo->resolution.height,
								"",
								j});
						AAMPLOG_INFO("Video Profile added to ABR, userData=%d BW=%ld res=%d:%d display=%d:%d pc=%d", j, streamInfo->bandwidthBitsPerSecond, streamInfo->resolution.width, streamInfo->resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, iProfileCapped);
					}
					else if( isThumbnailStream(streamInfo) )
					{
						//Updating Thumbnail profiles along with Video profiles.
						aamp->UpdateVideoEndProfileResolution( eMEDIATYPE_IFRAME,
								streamInfo->bandwidthBitsPerSecond,
								streamInfo->resolution.width,
								streamInfo->resolution.height );

						mAbrManager.addProfile({
								streamInfo->isIframeTrack,
								streamInfo->bandwidthBitsPerSecond,
								streamInfo->resolution.width,
								streamInfo->resolution.height,
								"",
								j});
						AAMPLOG_INFO("Adding image track, userData=%d BW = %ld ", j, streamInfo->bandwidthBitsPerSecond);
					}
				}
				if (!aamp->IsTSBSupported() && iProfileCapped)
				{
					aamp->mProfileCappedStatus = true;
				}
				break;
			}
			else
			{
				if(vProfileCountAvailable && audioProfileMatchedCount==0)
				{
					// Video Profiles available , but not finding anything with audio group .
					// As fallback recovery ,lets play with any other available video profiles
					AAMPLOG_WARN("ERROR No Video Profile found for matching audio group [%s]", audiogroupId.c_str());
					audiogroupId.clear();
					continue;
				}
				else if(vProfileCountAvailable && audioProfileMatchedCount && resolutionMatchedCount==0)
                                {
                                        // Video Profiles available , but not finding anything within configured display resolution
                                        // As fallback recovery ,lets ignore display resolution check and add available video profiles for playback to happen
                                        AAMPLOG_WARN("ERROR No Video Profile found for display res = %d:%d",aamp->mDisplayWidth, aamp->mDisplayHeight);
                                        resolutionCheckEnabled = false;
					iProfileCapped = false;
                                        continue;
                                }
				else if(vProfileCountAvailable && audioProfileMatchedCount && bitrateMatchedCount==0)
				{
					// Video Profiles available , but not finding anything within bitrate range configured
					// As fallback recovery ,lets ignore bitrate limit check and add available video profiles for playback to happen
					AAMPLOG_WARN("ERROR No video profiles available in manifest for playback, minBitrate:%ld maxBitrate:%ld", minBitrate, maxBitrate);
					ignoreBitRateRangeCheck = true;
					continue;
				}
				else if(vProfileCountAvailable && bitrateMatchedCount)
				{
					// No profiles selected due to disable config added
					if(bDisableATMOS && availableCountATMOS)
					{
						AAMPLOG_WARN("Resetting DisableATMOS flag as no Video Profile could be selected. ATMOS Count[%d]", availableCountATMOS);
						bDisableATMOS = false;
						continue;
					}
					else if(bDisableEC3 && availableCountEC3)
					{
						AAMPLOG_WARN("Resetting DisableEC3 flag as no Video Profile could be selected. EC3 Count[%d]", availableCountEC3);
						bDisableEC3 = false;
						continue;
					}
					else if(bDisableAC3 && availableCountAC3)
					{
						AAMPLOG_WARN("Resetting DisableAC3 flag as no Video Profile could be selected. AC3 Count[%d]", availableCountAC3);
						bDisableAC3 = false;
						continue;
					}
					else
					{
						AAMPLOG_WARN("Unable to select any video profiles due to unknown codec selection , mProfileCount : %d vProfileCountAvailable:%d", mProfileCount,vProfileCountAvailable);
						break;
					}

				}
				else
				{
					AAMPLOG_WARN("Unable to select any video profiles , mProfileCount : %d vProfileCountAvailable:%d", mProfileCount,vProfileCountAvailable);
					break;
				}
			}
		}while(vProfileCountSelected == 0);
	}
}




/***************************************************************************
* @fn ConfigureTextTrack
* @brief Function to select the text track and update TextTrackProfileIndex
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::ConfigureTextTrack()
{
	TextTrackInfo track = aamp->GetPreferredTextTrack();
	currentTextTrackProfileIndex = -1;
	if (!track.index.empty())
	{
		currentTextTrackProfileIndex = std::stoi(track.index);
	}
	else
	{
		if (!aamp->mSubLanguage.empty())
		{
			currentTextTrackProfileIndex = GetMediaIndexForLanguage(aamp->mSubLanguage, eTRACK_SUBTITLE);
		}
	}
	AAMPLOG_WARN("TextTrack Selected :%d", currentTextTrackProfileIndex);
}
/***************************************************************************
* @fn PopulateAudioAndTextTracks
* @brief Function to populate available audio and text tracks info from manifest
*
* @return void
***************************************************************************/
void StreamAbstractionAAMP_HLS::PopulateAudioAndTextTracks()
{
	if (mMediaCount > 0 && mProfileCount > 0)
	{
		for (int i = 0; i < mMediaCount; i++)
		{
			struct MediaInfo *media = &(mediaInfo[i]);
			if (media->type == eMEDIATYPE_AUDIO)
			{
				std::string index = std::to_string(i);
				std::string language = (media->language != NULL) ? GetLanguageCode(i) : std::string();
				std::string group_id = (media->group_id != NULL) ? std::string(media->group_id) : std::string();
				std::string name = (media->name != NULL) ? std::string(media->name) : std::string();
				std::string characteristics = (media->characteristics != NULL) ? std::string(media->characteristics) : std::string();
				std::string codec = GetAudioFormatStringForCodec(media->audioFormat) ;
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS:: Audio Track - lang:%s, group_id:%s, name:%s, codec:%s, characteristics:%s, channels:%d",
						language.c_str(), group_id.c_str(), name.c_str(), codec.c_str(), characteristics.c_str(), media->channels);
				mAudioTracks.push_back(AudioTrackInfo(index, language, group_id, name, codec, characteristics, media->channels));
			}
			else if (media->type == eMEDIATYPE_SUBTITLE)
			{
				std::string index = std::to_string(i);
				std::string language = (media->language != NULL) ? GetLanguageCode(i) : std::string();
				std::string group_id = (media->group_id != NULL) ? std::string(media->group_id) : std::string();
				std::string name = (media->name != NULL) ? std::string(media->name) : std::string();
				std::string instreamID = (media->instreamID != NULL) ? std::string(media->instreamID) : std::string();
				std::string characteristics = (media->characteristics != NULL) ? std::string(media->characteristics) : std::string();
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS:: Text Track - lang:%s, isCC:%d, group_id:%s, name:%s, instreamID:%s, characteristics:%s",
						  language.c_str(), media->isCC, group_id.c_str(), name.c_str(), instreamID.c_str(), characteristics.c_str());
				mTextTracks.push_back(TextTrackInfo(index, language, media->isCC, group_id, name, instreamID, characteristics));
			}
		}
#ifdef AAMP_CC_ENABLED
		std::vector<TextTrackInfo> textTracksCopy;
		std::copy_if(begin(mTextTracks), end(mTextTracks), back_inserter(textTracksCopy), [](const TextTrackInfo& e){return e.isCC;});
		AampCCManager::GetInstance()->updateLastTextTracks(textTracksCopy);
#endif

	}
	else
	{
#ifdef AAMP_CC_ENABLED
		AampCCManager::GetInstance()->updateLastTextTracks({});
#endif
		AAMPLOG_ERR("StreamAbstractionAAMP_HLS:: Fail to get available audio/text tracks, mMediaCount=%d and profileCount=%d!", mMediaCount, mProfileCount);
	}

}

/***************************************************************************
* @fn SeekPosUpdate
* @brief Function to update seek position
*
* @param ptr[in] seek position time
***************************************************************************/
void StreamAbstractionAAMP_HLS::SeekPosUpdate(double secondsRelativeToTuneTime)
{
	seekPosition = secondsRelativeToTuneTime;
}

/***************************************************************************
* @fn GetMediaIndexForLanguage
* @brief Function to get matching mediaInfo index for a language and track type
*
* @param[in] lang language
* @param[in] type track type
* @return int mediaInfo index of track with matching language
***************************************************************************/
int StreamAbstractionAAMP_HLS::GetMediaIndexForLanguage(std::string lang, TrackType type)
{
	int index = -1;
	const char* group = NULL;
	HlsStreamInfo* streamInfo = (HlsStreamInfo*)GetStreamInfo(this->currentProfileIndex);

	if (type == eTRACK_AUX_AUDIO)
	{
		group = streamInfo->audio;
	}
	else if (type == eTRACK_SUBTITLE)
	{
		group = streamInfo->subtitles;
	}

	if (group)
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS:: track [%d] group [%s], language [%s]", type, group, lang.c_str());
		for (int i = 0; i < mMediaCount; i++)
		{
			if (this->mediaInfo[i].group_id && !strcmp(group, this->mediaInfo[i].group_id))
			{
				std::string mediaLang = GetLanguageCode(i);
				if (lang == mediaLang)
				{
					//Found media tag with preferred language
					index = i;
					break;
				}
			}
		}
	}
	
	return index;
}

/***************************************************************************
* @fn GetStreamOutputFormatForAudio
* @brief Function to get output format for audio track
*
* @param[in] type track type
* @return StreamOutputFormat for the audio codec selected
***************************************************************************/
StreamOutputFormat StreamAbstractionAAMP_HLS::GetStreamOutputFormatForTrack(TrackType type)
{
	StreamOutputFormat format = FORMAT_UNKNOWN;

	HlsStreamInfo *streamInfo = (HlsStreamInfo *)GetStreamInfo(this->currentProfileIndex);
	const FormatMap *map = NULL;
	if (type == eTRACK_VIDEO)
	{
		map = GetVideoFormatForCodec(streamInfo->codecs);
	}
	else if ((type == eTRACK_AUDIO) || (type ==  eTRACK_AUX_AUDIO))
	{
		map = GetAudioFormatForCodec(streamInfo->codecs);
	}
	if (map)
	{ // video profile specifies audio format
		format = map->format;
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Track[%d] format is %d [%s]", type, map->format, map->codec);
	}
	else if ((type == eTRACK_AUDIO) || (type ==  eTRACK_AUX_AUDIO))
	{ // HACK
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS::assuming stereo");
		format = FORMAT_AUDIO_ES_AAC;
	}
	return format;
}

/***************************************************************************
* @fn GetStreamInfo
* @brief Function to get streamInfo for the profileIndex
*
* @param[in] int profileIndex
* @return StreamInfo for the index
***************************************************************************/
StreamInfo * StreamAbstractionAAMP_HLS::GetStreamInfo(int idx)
{
	int userData = 0;

	if (mProfileCount) // avoid calling getUserDataOfProfile() for playlist only URL playback.
	{
		userData = mAbrManager.getUserDataOfProfile(idx);
	}

	return &streamInfo[userData];
}

/**
 * @}
 */
