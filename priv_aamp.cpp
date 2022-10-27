/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file priv_aamp.cpp
 * @brief Advanced Adaptive Media Player (AAMP) PrivateInstanceAAMP impl
 */
#include "isobmffprocessor.h"
#include "priv_aamp.h"
#include "AampJsonObject.h"
#include "isobmffbuffer.h"
#include "AampFnLogger.h"
#include "AampConstants.h"
#include "AampCacheHandler.h"
#include "AampUtils.h"
#include "iso639map.h"
#include "fragmentcollector_mpd.h"
#include "admanager_mpd.h"
#include "fragmentcollector_hls.h"
#include "fragmentcollector_progressive.h"
#include "MediaStreamContext.h"
#include "hdmiin_shim.h"
#include "compositein_shim.h"
#include "ota_shim.h"
#include "_base64.h"
#include "base16.h"
#include "aampgstplayer.h"
#include "AampDRMSessionManager.h"
#include "SubtecFactory.hpp"

#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif
#ifdef USE_OPENCDM // AampOutputProtection is compiled when this  flag is enabled
#include "aampoutputprotection.h"
#endif

#include "AampCurlStore.h"

#ifdef IARM_MGR
#include "host.hpp"
#include "manager.hpp"
#include "libIBus.h"
#include "libIBusDaemon.h"
#include <hostIf_tr69ReqHandler.h>
#include <sstream>
#endif
#include <sys/time.h>
#include <cmath>
#include <regex>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include <string.h>

#define LOCAL_HOST_IP       "127.0.0.1"
#define AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS (20*1000LL)
#define AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS (AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS/10)

//Description size
#define MAX_DESCRIPTION_SIZE 128

//Stringification of Macro :  use two levels of macros
#define MACRO_TO_STRING(s) X_STR(s)
#define X_STR(s) #s

// Uncomment to test GetMediaFormatType without locator inspection
#define TRUST_LOCATOR_EXTENSION_IF_PRESENT

#define VALIDATE_INT(param_name, param_value, default_value)        \
    if ((param_value <= 0) || (param_value > INT_MAX))  { \
        AAMPLOG_WARN("Parameter '%s' not within INTEGER limit. Using default value instead.", param_name); \
        param_value = default_value; \
    }

#define VALIDATE_LONG(param_name, param_value, default_value)        \
    if ((param_value <= 0) || (param_value > LONG_MAX))  { \
        AAMPLOG_WARN("Parameter '%s' not within LONG INTEGER limit. Using default value instead.", param_name); \
        param_value = default_value; \
    }

#define VALIDATE_DOUBLE(param_name, param_value, default_value)        \
    if ((param_value <= 0) || (param_value > DBL_MAX))  { \
        AAMPLOG_WARN("Parameter '%s' not within DOUBLE limit. Using default value instead.", param_name); \
        param_value = default_value; \
    }

#define CURL_EASY_SETOPT(curl, CURLoption, option)\
    if (curl_easy_setopt(curl, CURLoption, option) != 0) {\
          logprintf("Failed at curl_easy_setopt ");\
    }  //CID:132698,135078 - checked return

#define FOG_REASON_STRING			"Fog-Reason:"
#define CURLHEADER_X_REASON			"X-Reason:"
#define BITRATE_HEADER_STRING		"X-Bitrate:"
#define CONTENTLENGTH_STRING		"Content-Length:"
#define SET_COOKIE_HEADER_STRING	"Set-Cookie:"
#define LOCATION_HEADER_STRING		"Location:"
#define CONTENT_ENCODING_STRING		"Content-Encoding:"
#define FOG_RECORDING_ID_STRING		"Fog-Recording-Id:"
#define CAPPED_PROFILE_STRING 		"Profile-Capped:"
#define TRANSFER_ENCODING_STRING		"Transfer-Encoding:"

#define MAX_DOWNLOAD_DELAY_LIMIT_MS 30000

//CMCD realated key names
#define CMCD_BITRATE "br="
#define CMCD_TOP_BITRATE "tb="
#define CMCD_SESSIONID "sid="
#define CMCD_BUFFERLENGTH "bl="
#define CMCD_NEXTOBJECTREQUEST "nor="
#define CMCD_OBJECT "ot="


/**
 * New state for treating a VOD asset as a "virtual linear" stream
 */
// Note that below state/impl currently assumes single profile, and so until fixed should be tested with "abr" in aamp.cfg to disable ABR
static long long simulation_start; // time at which main manifest was downloaded.
// Simulation_start is used to calculate elapsed time, used to advance virtual live window
static char *full_playlist_video_ptr = NULL; // Cache of initial full vod video playlist
static size_t full_playlist_video_len = 0; // Size (bytes) of initial full vod video playlist
static char *full_playlist_audio_ptr = NULL; // Cache of initial full vod audio playlist
static size_t full_playlist_audio_len = 0; // Size (bytes) of initial full vod audio playlist

/**
 * @struct gActivePrivAAMP_t
 * @brief Used for storing active PrivateInstanceAAMPs
 */
struct gActivePrivAAMP_t
{
	PrivateInstanceAAMP* pAAMP;
	bool reTune;
	int numPtsErrors;
};

static std::list<gActivePrivAAMP_t> gActivePrivAAMPs = std::list<gActivePrivAAMP_t>();

static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gCond = PTHREAD_COND_INITIALIZER;

static int PLAYERID_CNTR = 0;

static const char* strAAMPPipeName = "/tmp/ipc_aamp";

static bool activeInterfaceWifi = false;

static char previousInterface[20] = {'\0'};

static unsigned int ui32CurlTrace = 0;
/**
 * @struct CurlCbContextSyncTime 
 * @brief context during curl callbacks
 */
struct CurlCbContextSyncTime
{
    PrivateInstanceAAMP *aamp;
    GrowableBuffer *buffer;

    CurlCbContextSyncTime() : aamp(NULL), buffer(NULL){}
    CurlCbContextSyncTime(PrivateInstanceAAMP *_aamp, GrowableBuffer *_buffer) : aamp(_aamp),buffer(_buffer){}
    ~CurlCbContextSyncTime() {}

    CurlCbContextSyncTime(const CurlCbContextSyncTime &other) = delete;
    CurlCbContextSyncTime& operator=(const CurlCbContextSyncTime& other) = delete;
};

/**
 * @brief Enumeration for net_srv_mgr active interface event callback
 */
typedef enum _NetworkManager_EventId_t {
        IARM_BUS_NETWORK_MANAGER_EVENT_SET_INTERFACE_ENABLED=50,
        IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS=55,
        IARM_BUS_NETWORK_MANAGER_MAX
} IARM_Bus_NetworkManager_EventId_t;

/**
 * @struct _IARM_BUS_NetSrvMgr_Iface_EventData_t
 * @brief IARM Bus struct contains active streaming interface, origional definition present in homenetworkingservice.h
 */
typedef struct _IARM_BUS_NetSrvMgr_Iface_EventData_t {
	union{
		char activeIface[10];
		char allNetworkInterfaces[50];
		char enableInterface[10];
	};
	char interfaceCount;
	bool isInterfaceEnabled;
} IARM_BUS_NetSrvMgr_Iface_EventData_t;

static TuneFailureMap tuneFailureMap[] =
{
	{AAMP_TUNE_INIT_FAILED, 10, "AAMP: init failed"}, //"Fragmentcollector initialization failed"
	{AAMP_TUNE_INIT_FAILED_MANIFEST_DNLD_ERROR, 10, "AAMP: init failed (unable to download manifest)"},
	{AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR, 10, "AAMP: init failed (manifest missing tracks)"},
	{AAMP_TUNE_INIT_FAILED_MANIFEST_PARSE_ERROR, 10, "AAMP: init failed (corrupt/invalid manifest)"},
	{AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR, 10, "AAMP: init failed (unable to download video playlist)"},
	{AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR, 10, "AAMP: init failed (unable to download audio playlist)"},
	{AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR, 10, "AAMP: init failed (unsynchronized tracks)"},
	{AAMP_TUNE_MANIFEST_REQ_FAILED, 10, "AAMP: Manifest Download failed"}, //"Playlist refresh failed"
	{AAMP_TUNE_AUTHORISATION_FAILURE, 40, "AAMP: Authorization failure"},
	{AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, 10, "AAMP: fragment download failures"},
	{AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, 10, "AAMP: init fragment download failed"},
	{AAMP_TUNE_UNTRACKED_DRM_ERROR, 50, "AAMP: DRM error untracked error"},
	{AAMP_TUNE_DRM_INIT_FAILED, 50, "AAMP: DRM Initialization Failed"},
	{AAMP_TUNE_DRM_DATA_BIND_FAILED, 50, "AAMP: InitData-DRM Binding Failed"},
	{AAMP_TUNE_DRM_SESSIONID_EMPTY, 50, "AAMP: DRM Session ID Empty"},
	{AAMP_TUNE_DRM_CHALLENGE_FAILED, 50, "AAMP: DRM License Challenge Generation Failed"},
	{AAMP_TUNE_LICENCE_TIMEOUT, 50, "AAMP: DRM License Request Timed out"},
	{AAMP_TUNE_LICENCE_REQUEST_FAILED, 50, "AAMP: DRM License Request Failed"},
	{AAMP_TUNE_INVALID_DRM_KEY, 50, "AAMP: Invalid Key Error, from DRM"},
	{AAMP_TUNE_UNSUPPORTED_STREAM_TYPE, 60, "AAMP: Unsupported Stream Type"}, //"Unable to determine stream type for DRM Init"
	{AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE, 60, "AAMP: No supported Audio Types in Manifest"},
	{AAMP_TUNE_FAILED_TO_GET_KEYID, 50, "AAMP: Failed to parse key id from PSSH"},
	{AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN, 50, "AAMP: Failed to get access token from Auth Service"},
	{AAMP_TUNE_CORRUPT_DRM_DATA, 51, "AAMP: DRM failure due to Corrupt DRM files"},
	{AAMP_TUNE_CORRUPT_DRM_METADATA, 50, "AAMP: DRM failure due to Bad DRMMetadata in stream"},
	{AAMP_TUNE_DRM_DECRYPT_FAILED, 50, "AAMP: DRM Decryption Failed for Fragments"},
	{AAMP_TUNE_DRM_UNSUPPORTED, 50, "AAMP: DRM format Unsupported"},
	{AAMP_TUNE_DRM_SELF_ABORT, 50, "AAMP: DRM license request aborted by player"},
	{AAMP_TUNE_GST_PIPELINE_ERROR, 80, "AAMP: Error from gstreamer pipeline"},
	{AAMP_TUNE_PLAYBACK_STALLED, 7600, "AAMP: Playback was stalled due to lack of new fragments"},
	{AAMP_TUNE_CONTENT_NOT_FOUND, 20, "AAMP: Resource was not found at the URL(HTTP 404)"},
	{AAMP_TUNE_DRM_KEY_UPDATE_FAILED, 50, "AAMP: Failed to process DRM key"},
	{AAMP_TUNE_DEVICE_NOT_PROVISIONED, 52, "AAMP: Device not provisioned"},
	{AAMP_TUNE_HDCP_COMPLIANCE_ERROR, 53, "AAMP: HDCP Compliance Check Failure"},
	{AAMP_TUNE_INVALID_MANIFEST_FAILURE, 10, "AAMP: Invalid Manifest, parse failed"},
	{AAMP_TUNE_FAILED_PTS_ERROR, 80, "AAMP: Playback failed due to PTS error"},
	{AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING, 10, "AAMP: init fragments missing in playlist"},
	{AAMP_TUNE_FAILURE_UNKNOWN, 100, "AAMP: Unknown Failure"}
};

static constexpr const char *BITRATECHANGE_STR[] =
{
	(const char *)"BitrateChanged - Network adaptation",				// eAAMP_BITRATE_CHANGE_BY_ABR
	(const char *)"BitrateChanged - Rampdown due to network failure",		// eAAMP_BITRATE_CHANGE_BY_RAMPDOWN
	(const char *)"BitrateChanged - Reset to default bitrate due to tune",		// eAAMP_BITRATE_CHANGE_BY_TUNE
	(const char *)"BitrateChanged - Reset to default bitrate due to seek",		// eAAMP_BITRATE_CHANGE_BY_SEEK
	(const char *)"BitrateChanged - Reset to default bitrate due to trickplay",	// eAAMP_BITRATE_CHANGE_BY_TRICKPLAY
	(const char *)"BitrateChanged - Rampup since buffers are full",			// eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL
	(const char *)"BitrateChanged - Rampdown since buffers are empty",		// eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY
	(const char *)"BitrateChanged - Network adaptation by FOG",			// eAAMP_BITRATE_CHANGE_BY_FOG_ABR
	(const char *)"BitrateChanged - Information from OTA",                          // eAAMP_BITRATE_CHANGE_BY_OTA
	(const char *)"BitrateChanged - Video stream information from HDMIIN",          // eAAMP_BITRATE_CHANGE_BY_HDMIIN
	(const char *)"BitrateChanged - Unknown reason"					// eAAMP_BITRATE_CHANGE_MAX
};

#define BITRATEREASON2STRING(id) BITRATECHANGE_STR[id]

static constexpr const char *ADEVENT_STR[] =
{
	(const char *)"AAMP_EVENT_AD_RESERVATION_START",
	(const char *)"AAMP_EVENT_AD_RESERVATION_END",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_START",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_END",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_ERROR",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_PROGRESS"
};

#define ADEVENT2STRING(id) ADEVENT_STR[id - AAMP_EVENT_AD_RESERVATION_START]

/**
 * @brief Get the idle task's source ID
 * @retval source ID
 */
static guint aamp_GetSourceID()
{
	guint callbackId = 0;
	GSource *source = g_main_current_source();
	if (source != NULL)
	{
		callbackId = g_source_get_id(source);
	}
	return callbackId;
}

/**
 * @brief Idle task to resume aamp
 * @param ptr pointer to PrivateInstanceAAMP object
 * @retval True/False
 */
static gboolean PrivateInstanceAAMP_Resume(gpointer ptr)
{
	bool retValue = true;
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP* )ptr;
	aamp->NotifyFirstBufferProcessed();
	TuneType tuneType = eTUNETYPE_SEEK;

	if (!aamp->mSeekFromPausedState && (aamp->rate == AAMP_NORMAL_PLAY_RATE))
	{
		retValue = aamp->mStreamSink->Pause(false, false);
		aamp->pipeline_paused = false;
	}
	else
	{
		// Live immediate : seek to live position from paused state.
		if (aamp->mPausedBehavior == ePAUSED_BEHAVIOR_LIVE_IMMEDIATE)
		{
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}
		aamp->rate = AAMP_NORMAL_PLAY_RATE;
		aamp->pipeline_paused = false;
		aamp->mSeekFromPausedState = false;
		aamp->AcquireStreamLock();
		aamp->TuneHelper(tuneType);
		aamp->ReleaseStreamLock();
	}

	aamp->ResumeDownloads();
	if(retValue)
	{
		aamp->NotifySpeedChanged(aamp->rate);
	}
	aamp->mAutoResumeTaskPending = false;
	return G_SOURCE_REMOVE;
}

/**
 * @brief Idle task to process discontinuity
 * @param ptr pointer to PrivateInstanceAAMP object
 * @retval G_SOURCE_REMOVE
 */
static gboolean PrivateInstanceAAMP_ProcessDiscontinuity(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP*) ptr;

	GSource *src = g_main_current_source();
	if (src == NULL || !g_source_is_destroyed(src))
	{
		bool ret = aamp->ProcessPendingDiscontinuity();
		// This is to avoid calling cond signal, in case Stop() interrupts the ProcessPendingDiscontinuity
		if (ret)
		{
			aamp->SyncBegin();
			aamp->mDiscontinuityTuneOperationId = 0;
			aamp->SyncEnd();
		}
		pthread_cond_signal(&aamp->mCondDiscontinuity);
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Tune again to currently viewing asset. Used for internal error handling
 * @param ptr pointer to PrivateInstanceAAMP object
 * @retval G_SOURCE_REMOVE
 */
static gboolean PrivateInstanceAAMP_Retune(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP*) ptr;
	bool activeAAMPFound = false;
	bool reTune = false;
	gActivePrivAAMP_t *gAAMPInstance = NULL;
	pthread_mutex_lock(&gMutex);
	for (std::list<gActivePrivAAMP_t>::iterator iter = gActivePrivAAMPs.begin(); iter != gActivePrivAAMPs.end(); iter++)
	{
		if (aamp == iter->pAAMP)
		{
			gAAMPInstance = &(*iter);
			activeAAMPFound = true;
			reTune = gAAMPInstance->reTune;
			break;
		}
	}
	if (!activeAAMPFound)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: %p not in Active AAMP list", aamp);
	}
	else if (!reTune)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: %p reTune flag not set", aamp);
	}
	else
	{
		if (aamp->pipeline_paused)
		{
			aamp->pipeline_paused = false;
		}

		aamp->mIsRetuneInProgress = true;
		pthread_mutex_unlock(&gMutex);

		aamp->AcquireStreamLock();
		aamp->TuneHelper(eTUNETYPE_RETUNE);
		aamp->ReleaseStreamLock();

		pthread_mutex_lock(&gMutex);
		aamp->mIsRetuneInProgress = false;
		gAAMPInstance->reTune = false;
		pthread_cond_signal(&gCond);
	}
	pthread_mutex_unlock(&gMutex);
	return G_SOURCE_REMOVE;
}


/**
 * @brief Simulate VOD asset as a "virtual linear" stream.
 */
static void SimulateLinearWindow( struct GrowableBuffer *buffer, const char *ptr, size_t len )
{
	// Calculate elapsed time in seconds since virtual linear stream started
	float cull = (aamp_GetCurrentTimeMS() - simulation_start)/1000.0;
	buffer->len = 0; // Reset Growable Buffer length
	float window = 20.0; // Virtual live window size; can be increased/decreasedint
	const char *fin = ptr+len;
	bool wroteHeader = false; // Internal state used to decide whether HLS playlist header has already been output
	int seqNo = 0;

	while (ptr < fin)
	{
		int count = 0;
		char line[1024];
		float fragmentDuration;

		for(;;)
		{
			char c = *ptr++;
			line[count++] = c;
			if( ptr>=fin || c<' ' ) break;
		}

		line[count] = 0x00;

		if (sscanf(line,"#EXTINF:%f",&fragmentDuration) == 1)
		{
			if (cull > 0)
			{
				cull -= fragmentDuration;
				seqNo++;
				continue; // Not yet in active window
			}

			if (!wroteHeader)
			{
				// Write a simple linear HLS header, without the type:VOD, and with dynamic media sequence number
				wroteHeader = true;
				char header[1024];
				sprintf( header,
					"#EXTM3U\n"
					"#EXT-X-VERSION:3\n"
					"#EXT-X-TARGETDURATION:2\n"
					"#EXT-X-MEDIA-SEQUENCE:%d\n", seqNo );
				aamp_AppendBytes(buffer, header, strlen(header) );
			}

			window -= fragmentDuration;

			if (window < 0.0)
			{
				// Finished writing virtual linear window
				break;
			}
		}

		if (wroteHeader)
		{
			aamp_AppendBytes(buffer, line, count );
		}
	}

	// Following can be used to debug
	// aamp_AppendNulTerminator( buffer );
	// printf( "Virtual Linear Playlist:\n%s\n***\n", buffer->ptr );
}

/**
 * @brief Get the telemetry type for a media type
 * @param type media type
 * @retval telemetry type
 */
static MediaTypeTelemetry aamp_GetMediaTypeForTelemetry(MediaType type)
{
	MediaTypeTelemetry ret;
	switch(type)
	{
			case eMEDIATYPE_VIDEO:
			case eMEDIATYPE_AUDIO:
			case eMEDIATYPE_SUBTITLE:
			case eMEDIATYPE_AUX_AUDIO:
			case eMEDIATYPE_IFRAME:
						ret = eMEDIATYPE_TELEMETRY_AVS;
						break;
			case eMEDIATYPE_MANIFEST:
			case eMEDIATYPE_PLAYLIST_VIDEO:
			case eMEDIATYPE_PLAYLIST_AUDIO:
			case eMEDIATYPE_PLAYLIST_SUBTITLE:
			case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			case eMEDIATYPE_PLAYLIST_IFRAME:
						ret = eMEDIATYPE_TELEMETRY_MANIFEST;
						break;
			case eMEDIATYPE_INIT_VIDEO:
			case eMEDIATYPE_INIT_AUDIO:
			case eMEDIATYPE_INIT_SUBTITLE:
			case eMEDIATYPE_INIT_AUX_AUDIO:
			case eMEDIATYPE_INIT_IFRAME:
						ret = eMEDIATYPE_TELEMETRY_INIT;
						break;
			case eMEDIATYPE_LICENCE:
						ret = eMEDIATYPE_TELEMETRY_DRM;
						break;
			default:
						ret = eMEDIATYPE_TELEMETRY_UNKNOWN;
						break;
	}
	return ret;
}

/**
 * @brief de-fog playback URL to play directly from CDN instead of fog
 * @param[in][out] dst Buffer containing URL
 */
static void DeFog(std::string& url)
{
	std::string prefix("&recordedUrl=");
	size_t startPos = url.find(prefix);
	if( startPos != std::string::npos )
	{
		startPos += prefix.size();
		size_t len = url.find( '&',startPos );
		if( len != std::string::npos )
		{
			len -= startPos;
		}
		url = url.substr(startPos,len);
		aamp_DecodeUrlParameter(url);
	}
}

// Helper functions for loading configuration (from file/TR181)

#ifdef IARM_MGR
/**
 * @brief
 * @param paramName
 * @param iConfigLen
 * @retval
 */
char * GetTR181AAMPConfig(const char * paramName, size_t & iConfigLen)
{
	char *  strConfig = NULL;
	IARM_Result_t result; 
	HOSTIF_MsgData_t param;
	memset(&param,0,sizeof(param));
	snprintf(param.paramName,TR69HOSTIFMGR_MAX_PARAM_LEN,"%s",paramName);
	param.reqType = HOSTIF_GET;

	result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME,IARM_BUS_TR69HOSTIFMGR_API_GetParams,
                    (void *)&param,	sizeof(param));
	if(result  == IARM_RESULT_SUCCESS)
	{
		if(fcNoFault == param.faultCode)
		{
			if(param.paramtype == hostIf_StringType && param.paramLen > 0 )
			{
				std::string strforLog(param.paramValue,param.paramLen);

				iConfigLen = param.paramLen;
				const char *src = (const char*)(param.paramValue);
				strConfig = (char * ) base64_Decode(src,&iConfigLen);

				AAMPLOG_WARN("GetTR181AAMPConfig: Got:%s En-Len:%d Dec-len:%d",strforLog.c_str(),param.paramLen,iConfigLen);
			}
			else
			{
				AAMPLOG_WARN("GetTR181AAMPConfig: Not a string param type=%d or Invalid len:%d ",param.paramtype, param.paramLen);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("GetTR181AAMPConfig: Failed to retrieve value result=%d",result);
	}
	return strConfig;
}
/**
 * @brief Active interface state change from netsrvmgr
 * @param owner reference to net_srv_mgr
 * @param IARM eventId received
 * @data pointer reference to interface struct
 */
void getActiveInterfaceEventHandler (const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{

	if (strcmp (owner, "NET_SRV_MGR") != 0)
		return;

	IARM_BUS_NetSrvMgr_Iface_EventData_t *param = (IARM_BUS_NetSrvMgr_Iface_EventData_t *) data;

	if (NULL == strstr (param->activeIface, previousInterface) || (strlen(previousInterface) == 0))
	{
		memset(previousInterface, 0, sizeof(previousInterface));
		strncpy(previousInterface, param->activeIface, sizeof(previousInterface) - 1);
		AAMPLOG_WARN("getActiveInterfaceEventHandler EventId %d activeinterface %s", eventId,  param->activeIface);
	}

	if (NULL != strstr (param->activeIface, "wlan"))
	{
		 activeInterfaceWifi = true;
	}
	else if (NULL != strstr (param->activeIface, "eth"))
	{
		 activeInterfaceWifi = false;
	}
}
#endif

/**
 * @brief Active streaming interface is wifi
 *
 * @return bool - true if wifi interface connected
 */
static bool IsActiveStreamingInterfaceWifi (void)
{
        bool wifiStatus = false;
#ifdef IARM_MGR
        IARM_Result_t ret = IARM_RESULT_SUCCESS;
        IARM_BUS_NetSrvMgr_Iface_EventData_t param;

        ret = IARM_Bus_Call("NET_SRV_MGR", "getActiveInterface", (void*)&param, sizeof(param));
        if (ret != IARM_RESULT_SUCCESS) {
                AAMPLOG_ERR("NET_SRV_MGR getActiveInterface read failed : %d", ret);
        }
        else
        {
                AAMPLOG_WARN("NET_SRV_MGR getActiveInterface = %s", param.activeIface);
                if (!strcmp(param.activeIface, "WIFI")){
                        wifiStatus = true;
                }
        }
        IARM_Bus_RegisterEventHandler("NET_SRV_MGR", IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, getActiveInterfaceEventHandler);
#endif
        return wifiStatus;
}

/**
* @brief helper function to avoid dependency on unsafe sscanf while reading strings
* @param buf pointer to CString buffer to scan
* @param prefixPtr - prefix string to match in bufPtr
* @param valueCopyPtr receives allocated copy of string following prefix (skipping delimiting whitesace) if prefix found
* @retval 0 if prefix not present or error
* @retval 1 if string extracted/copied to valueCopyPtr
*/
static int ReadConfigStringHelper(std::string buf, const char *prefixPtr, const char **valueCopyPtr)
{
	int ret = 0;
	if (buf.find(prefixPtr) != std::string::npos)
	{
		std::size_t pos = strlen(prefixPtr);
		if (*valueCopyPtr != NULL)
		{
			free((void *)*valueCopyPtr);
			*valueCopyPtr = NULL;
		}
		*valueCopyPtr = strdup(buf.substr(pos).c_str());
		if (*valueCopyPtr)
		{
			ret = 1;
		}
	}
	return ret;
}

/**
* @brief helper function to extract numeric value from given buf after removing prefix
* @param buf String buffer to scan
* @param prefixPtr - prefix string to match in bufPtr
* @param value - receives numeric value after extraction
* @retval 0 if prefix not present or error
* @retval 1 if string converted to numeric value
*/
template<typename T>
static int ReadConfigNumericHelper(std::string buf, const char* prefixPtr, T& value)
{
	int ret = 0;

	try
	{
		std::size_t pos = buf.rfind(prefixPtr,0); // starts with check
		if (pos != std::string::npos)
		{
			pos += strlen(prefixPtr);
			std::string valStr = buf.substr(pos);
			if (std::is_same<T, int>::value)
				value = std::stoi(valStr);
			else if (std::is_same<T, long>::value)
				value = std::stol(valStr);
			else if (std::is_same<T, float>::value)
				value = std::stof(valStr);
			else
				value = std::stod(valStr);
			ret = 1;
		}
	}
	catch(exception& e)
	{
		// NOP
	}

	return ret;
}


// End of helper functions for loading configuration


static std::string getTimeStamp(time_t epochTime, const char* format = "%Y-%m-%dT%H:%M:%S.%f%Z")
{
   char timestamp[64] = {0};
   strftime(timestamp, sizeof(timestamp), format, localtime(&epochTime));
   return timestamp;
}

static time_t convertTimeToEpoch(const char* theTime, const char* format = "%Y-%m-%dT%H:%M:%S.%f%Z")
{
   std::tm tmTime;
   memset(&tmTime, 0, sizeof(tmTime));
   strptime(theTime, format, &tmTime);
   return mktime(&tmTime);
}

// Curl callback functions

/**
 * @brief write callback to be used by CURL
 * @param ptr pointer to buffer containing the data
 * @param size size of the buffer
 * @param nmemb number of bytes
 * @param userdata CurlCallbackContext pointer
 * @retval size consumed or 0 if interrupted
 */
static size_t SyncTime_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t ret = 0;
    CurlCbContextSyncTime *context = (CurlCbContextSyncTime *)userdata;
    pthread_mutex_lock(&context->aamp->mLock);
    size_t numBytesForBlock = size*nmemb;
    aamp_AppendBytes(context->buffer, ptr, numBytesForBlock);
    ret = numBytesForBlock;
    pthread_mutex_unlock(&context->aamp->mLock);
    return ret;
}

/**
 * @brief HandleSSLWriteCallback - Handle write callback from CURL
 */
size_t PrivateInstanceAAMP::HandleSSLWriteCallback ( char *ptr, size_t size, size_t nmemb, void* userdata )
{
    size_t ret = 0;
    CurlCallbackContext *context = (CurlCallbackContext *)userdata;
    if(!context) return ret;
    pthread_mutex_lock(&context->aamp->mLock);
    if (context->aamp->mDownloadsEnabled && context->aamp->mMediaDownloadsEnabled[context->fileType])
    {
		if ((NULL == context->buffer->ptr) && (context->contentLength > 0))
		{
			size_t len = context->contentLength + 2;
			/*Add 2 additional characters to take care of extra characters inserted by aamp_AppendNulTerminator*/
			if(context->downloadIsEncoded && (len < DEFAULT_ENCODED_CONTENT_BUFFER_SIZE))
			{
				// Allocate a fixed buffer for encoded contents. Content length is not trusted here
				len = DEFAULT_ENCODED_CONTENT_BUFFER_SIZE;
			}
			assert(!context->buffer->ptr);
			context->buffer->ptr = (char *)g_malloc( len );
			context->buffer->avail = len;
		}
        size_t numBytesForBlock = size*nmemb;
        aamp_AppendBytes(context->buffer, ptr, numBytesForBlock);
        ret = numBytesForBlock;

        if(context->aamp->GetLLDashServiceData()->lowLatencyMode &&
                 (context->fileType == eMEDIATYPE_VIDEO ||
                  context->fileType ==  eMEDIATYPE_AUDIO ||
                  context->fileType ==  eMEDIATYPE_SUBTITLE))
        {
		MediaStreamContext *mCtx = context->aamp->GetMediaStreamContext(context->fileType);
		if(mCtx)
		{
			mCtx->CacheFragmentChunk(context->fileType, ptr, numBytesForBlock,context->remoteUrl,context->downloadStartTime);
		}
        }
    }
    else
    {
		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && mOrigManifestUrl.isRemotehost)
		{
			ret = (size*nmemb);
		}
		else
		{
			AAMPLOG_WARN("CurlTrace write_callback - interrupted, ret:%d", ret);
		}
    }
    pthread_mutex_unlock(&context->aamp->mLock);

    return ret;
}
/**
 * @brief write callback to be used by CURL
 * @param ptr pointer to buffer containing the data
 * @param size size of the buffer
 * @param nmemb number of bytes
 * @param userdata CurlCallbackContext pointer
 * @retval size consumed or 0 if interrupted
 */
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t ret = 0;
    CurlCallbackContext *context = (CurlCallbackContext *)userdata;
    if(!context) return ret;

	ret = context->aamp->HandleSSLWriteCallback( ptr, size, nmemb, userdata);
    return ret;
}

/**
 * @brief function to print header response during download failure and latency.
 * @param fileType current media type
 */
static void print_headerResponse(std::vector<std::string> &allResponseHeadersForErrorLogging, MediaType fileType)
{
	if (gpGlobalConfig->logging.curlHeader && (eMEDIATYPE_VIDEO == fileType || eMEDIATYPE_PLAYLIST_VIDEO == fileType))
	{
		int size = allResponseHeadersForErrorLogging.size();
		AAMPLOG_WARN("################ Start :: Print Header response ################");
		for (int i=0; i < size; i++)
		{
			AAMPLOG_WARN("* %s", allResponseHeadersForErrorLogging.at(i).c_str());
		}
		AAMPLOG_WARN("################ End :: Print Header response ################");
	}

	allResponseHeadersForErrorLogging.clear();
}

/**
 * @brief HandleSSLHeaderCallback - Hanlde header callback from SSL
 */
size_t PrivateInstanceAAMP::HandleSSLHeaderCallback ( const char *ptr, size_t size, size_t nmemb, void* user_data )
{
	CurlCallbackContext *context = static_cast<CurlCallbackContext *>(user_data);
	httpRespHeaderData *httpHeader = context->responseHeaderData;
	size_t len = nmemb * size;
	size_t startPos = 0;
	size_t endPos = len-2; // strip CRLF

	bool isBitrateHeader = false;
	bool isFogRecordingIdHeader = false;
	bool isProfileCapHeader = false;

	if( len<2 || ptr[endPos] != '\r' || ptr[endPos+1] != '\n' )
	{ // only proceed if this is a CRLF terminated curl header, as expected
		return len;
	}

	if (context->aamp->mConfig->IsConfigSet(eAAMPConfig_CurlHeader) && ptr[0] &&
		(eMEDIATYPE_VIDEO == context->fileType || eMEDIATYPE_PLAYLIST_VIDEO == context->fileType))
	{
		std::string temp = std::string(ptr,endPos);
		context->allResponseHeadersForErrorLogging.push_back(temp);
	}

	// As per Hypertext Transfer Protocol ==> Field names are case-insensitive
	// HTTP/1.1 4.2 Message Headers : Each header field consists of a name followed by a colon (":") and the field value. Field names are case-insensitive
	if (STARTS_WITH_IGNORE_CASE(ptr, FOG_REASON_STRING))
	{
		httpHeader->type = eHTTPHEADERTYPE_FOG_REASON;
		startPos = STRLEN_LITERAL(FOG_REASON_STRING);
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, CURLHEADER_X_REASON))
	{
		httpHeader->type = eHTTPHEADERTYPE_XREASON;
		startPos = STRLEN_LITERAL(CURLHEADER_X_REASON);
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, BITRATE_HEADER_STRING))
	{
		startPos = STRLEN_LITERAL(BITRATE_HEADER_STRING);
		isBitrateHeader = true;
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, SET_COOKIE_HEADER_STRING))
	{
		httpHeader->type = eHTTPHEADERTYPE_COOKIE;
		startPos = STRLEN_LITERAL(SET_COOKIE_HEADER_STRING);
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, LOCATION_HEADER_STRING))
	{
		httpHeader->type = eHTTPHEADERTYPE_EFF_LOCATION;
		startPos = STRLEN_LITERAL(LOCATION_HEADER_STRING);
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, FOG_RECORDING_ID_STRING))
	{
		startPos = STRLEN_LITERAL(FOG_RECORDING_ID_STRING);
		isFogRecordingIdHeader = true;
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, CONTENT_ENCODING_STRING ))
	{
		// Enabled IsEncoded as Content-Encoding header is present
		// The Content-Encoding entity header incidcates media is compressed
		context->downloadIsEncoded = true;
	}
	else if (context->aamp->mConfig->IsConfigSet(eAAMPConfig_LimitResolution) && context->aamp->IsFirstRequestToFog() && STARTS_WITH_IGNORE_CASE(ptr, CAPPED_PROFILE_STRING ))
	{
		startPos = STRLEN_LITERAL(CAPPED_PROFILE_STRING);
		isProfileCapHeader = true;
	}
	else if (STARTS_WITH_IGNORE_CASE(ptr, TRANSFER_ENCODING_STRING ))
	{
		context->chunkedDownload = true;
	}
	else if (0 == context->buffer->avail)
	{
		if (STARTS_WITH_IGNORE_CASE(ptr, CONTENTLENGTH_STRING))
		{
			int contentLengthStartPosition = STRLEN_LITERAL(CONTENTLENGTH_STRING);
			const char * contentLengthStr = ptr + contentLengthStartPosition;
			context->contentLength = atoi(contentLengthStr);
		}
	}

	// Check for http header tags, only if event listener for HTTPResponseHeaderEvent is available
	if (eMEDIATYPE_MANIFEST == context->fileType && context->aamp->IsEventListenerAvailable(AAMP_EVENT_HTTP_RESPONSE_HEADER))
	{
		std::vector<std::string> responseHeaders = context->aamp->responseHeaders;

		if (responseHeaders.size() > 0)
		{
			for (int header=0; header < responseHeaders.size(); header++) {
				std::string headerType = responseHeaders[header].c_str();
				// check if subscribed header is available
				if (0 == strncasecmp(ptr, headerType.c_str() , headerType.length()))
				{
					startPos = headerType.length();
					// strip only the header value from the response
					context->aamp->httpHeaderResponses[headerType] = std::string( ptr + startPos + 2, endPos - startPos - 2).c_str();
					AAMPLOG_INFO("httpHeaderResponses");
					for (auto const& pair: context->aamp->httpHeaderResponses) {
						AAMPLOG_INFO("{ %s, %s }", pair.first.c_str(), pair.second.c_str());
					}
				}
			}
		}
	}

	if(startPos > 0)
	{
		while( endPos>startPos && ptr[endPos-1] == ' ' )
		{ // strip trailing whitespace
			endPos--;
		}
		while( startPos < endPos && ptr[startPos] == ' ')
		{ // strip leading whitespace
			startPos++;
		}

		if(isBitrateHeader)
		{
			const char * strBitrate = ptr + startPos;
			context->bitrate = atol(strBitrate);
			AAMPLOG_TRACE("Parsed HTTP %s: %ld", isBitrateHeader? "Bitrate": "False", context->bitrate);
		}
		else if(isFogRecordingIdHeader)
		{
			context->aamp->mTsbRecordingId = string( ptr + startPos, endPos - startPos );
			AAMPLOG_TRACE("Parsed Fog-Id : %s", context->aamp->mTsbRecordingId.c_str());
		}
		else if(isProfileCapHeader)
		{
			const char * strProfileCap = ptr + startPos;
			context->aamp->mProfileCappedStatus = atol(strProfileCap)? true : false;
			AAMPLOG_TRACE("Parsed Profile-Capped Header : %d", context->aamp->mProfileCappedStatus);
		}
		else
		{
			httpHeader->data = string( ptr + startPos, endPos - startPos );
			if(httpHeader->type != eHTTPHEADERTYPE_EFF_LOCATION)
			{ //Append delimiter ";"
				httpHeader->data += ';';
			}
		}

		if(gpGlobalConfig->logging.trace)
		{
			AAMPLOG_TRACE("Parsed HTTP %s header: %s", httpHeader->type==eHTTPHEADERTYPE_COOKIE? "Cookie": "X-Reason", httpHeader->data.c_str());
		}
	}

	return len;
}

/**
 * @brief Convert to string and add suffix k, M, G
 * @param bps bytes Speed
 * @param str ptr String buffer
 * @retval ptr Converted String buffer
 */
char* ConvertSpeedToStr(long bps, char *str)
{
    #define ONE_KILO  1024
    #define ONE_MEGA ((1024) * ONE_KILO)

    if(bps < 100000)
        snprintf(str, 6, "%5ld", bps);

    else if(bps < (10000 * ONE_KILO))
      snprintf(str, 6, "%4ld" "k", bps/ONE_KILO);

    else if(bps < (100 * ONE_MEGA))
        snprintf(str, 6, "%2ld" ".%0ld" "M", bps/ONE_MEGA,
            (bps%ONE_MEGA) / (ONE_MEGA/10) );
    else
      snprintf(str, 6, "%4ld" "M", bps/ONE_MEGA);

    return str;
}


/**
 * @brief Get Current Content Download Speed
 * @param aamp ptr aamp context
 * @param fileType File Type
 * @param bDownloadStart Download start flag
 * @param start Download start time
 * @param dlnow current downloaded bytes
 * @retval bps bits per second
 */
long getCurrentContentDownloadSpeed(PrivateInstanceAAMP *aamp,
                                    MediaType fileType, //File Type Download
                                    bool bDownloadStart,
                                    long start,
                                    double dlnow) // downloaded bytes so far)
{
    long bitsPerSecond = 0;
    long time_now = 0;
    long time_diff = 0;
    long dl_diff = 0;
    char buffer[2][6] = {0,};

    struct SpeedCache* speedcache = NULL;
    speedcache = aamp->GetLLDashSpeedCache();

    if(!aamp->mhAbrManager.GetLowLatencyStartABR())
    {
        speedcache->last_sample_time_val = start;
    }

    time_now = NOW_STEADY_TS_MS;
    time_diff = (time_now - speedcache->last_sample_time_val);

    if(bDownloadStart)
    {
        speedcache->prev_dlnow = 0;
    }

    dl_diff = (long)dlnow -  speedcache->prev_dlnow;

    long prevdlnow = speedcache->prev_dlnow;
    speedcache->prev_dlnow = dlnow;

    long currentTotalDownloaded = 0;
    long total_dl_diff  = 0;
    currentTotalDownloaded = speedcache->totalDownloaded + dl_diff;
    total_dl_diff = currentTotalDownloaded - speedcache->prevSampleTotalDownloaded;
    if(total_dl_diff<=0) total_dl_diff = 0;

    //AAMPLOG_INFO("[%d] prev_dlnow: %ld dlnow: %ld dl_diff: %ld total_dl_diff: %ld Current Total Download: %ld Previous Total Download: %ld",fileType, prevdlnow, (long)dlnow, dl_diff,total_dl_diff,currentTotalDownloaded, speedcache->prevSampleTotalDownloaded);

    if(aamp->mhAbrManager.IsABRDataGoodToEstimate(time_diff))
    {
	    aamp->mhAbrManager.CheckLLDashABRSpeedStoreSize(speedcache,bitsPerSecond,time_now,total_dl_diff,time_diff,currentTotalDownloaded);
    }
    else
    {
        AAMPLOG_TRACE("[%d] Ignore Speed Calculation -> time_diff [%ld]",fileType, time_diff);
    }
    
    speedcache->totalDownloaded += dl_diff;
    
    return bitsPerSecond;
}

/**
 * @brief HandleSSLProgressCallback - Process progress callback from CURL
 *
 * @param clientp opaque context passed by caller
 * @param dltotal total number of bytes libcurl expects to download
 * @param dlnow number of bytes downloaded so far
 * @param ultotal total number of bytes libcurl expects to upload
 * @param ulnow number of bytes uploaded so far
 *
 * @retval -1 to cancel in progress download
 */
int PrivateInstanceAAMP::HandleSSLProgressCallback ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;
	PrivateInstanceAAMP *aamp = context->aamp;
	AampConfig *mConfig = context->aamp->mConfig;

	if(context->aamp->GetLLDashServiceData()->lowLatencyMode &&
		context->fileType == eMEDIATYPE_VIDEO &&
		context->aamp->CheckABREnabled() &&
		!(ISCONFIGSET_PRIV(eAAMPConfig_DisableLowLatencyABR)))
	{
		//AAMPLOG_WARN("[%d] dltotal: %.0f , dlnow: %.0f, ultotal: %.0f, ulnow: %.0f, time: %.0f\n", context->fileType,
		//	dltotal, dlnow, ultotal, ulnow, difftime(time(NULL), 0));

		int  AbrChunkThresholdSize = 0;
		GETCONFIGVALUE(eAAMPConfig_ABRChunkThresholdSize,AbrChunkThresholdSize);

		if (/*(dlnow > AbrChunkThresholdSize) &&*/ (context->downloadNow != dlnow))
		{
			long downloadbps = 0;

			context->downloadNow = dlnow;
			context->downloadNowUpdatedTime = NOW_STEADY_TS_MS;

			if(!aamp->mhAbrManager.GetLowLatencyStartABR())
			{
				//Reset speedcache when Fragment download Starts
				struct SpeedCache* speedcache = NULL;
				speedcache = aamp->GetLLDashSpeedCache();
				memset(speedcache, 0x00, sizeof(struct SpeedCache));
			}

			downloadbps = getCurrentContentDownloadSpeed(aamp, context->fileType, context->dlStarted, (long)context->downloadStartTime, dlnow);

			if(context->dlStarted)
			{
				context->dlStarted = false;
			}

			if(!aamp->mhAbrManager.GetLowLatencyStartABR())
			{
				aamp->mhAbrManager.SetLowLatencyStartABR(true);
			}

			if(downloadbps)
			{
				long currentProfilebps  = context->aamp->mpStreamAbstractionAAMP->GetVideoBitrate();

				pthread_mutex_lock(&context->aamp->mLock);
				aamp->mhAbrManager.UpdateABRBitrateDataBasedOnCacheLength(context->aamp->mAbrBitrateData,downloadbps,true);
				pthread_mutex_unlock(&context->aamp->mLock);
			}
		}
	}

	int rc = 0;
	context->aamp->SyncBegin();
	if (!context->aamp->mDownloadsEnabled && context->aamp->mMediaDownloadsEnabled[context->fileType])
	{
		rc = -1; // CURLE_ABORTED_BY_CALLBACK
	}

	context->aamp->SyncEnd();
	if( rc==0 )
	{ // only proceed if not an aborted download
		if (dlnow > 0 && context->stallTimeout > 0)
		{
			if (context->downloadSize == -1)
			{ // first byte(s) downloaded
				context->downloadSize = dlnow;
				context->downloadUpdatedTime = NOW_STEADY_TS_MS;
			}
			else
			{
				if (dlnow == context->downloadSize)
				{ // no change in downloaded bytes - check time since last update to infer stall
					double timeElapsedSinceLastUpdate = (NOW_STEADY_TS_MS - context->downloadUpdatedTime) / 1000.0; //in secs
					if (timeElapsedSinceLastUpdate >= context->stallTimeout)
					{ // no change for at least <stallTimeout> seconds - consider download stalled and abort
						AAMPLOG_WARN("Abort download as mid-download stall detected for %.2f seconds, download size:%.2f bytes", timeElapsedSinceLastUpdate, dlnow);
						context->abortReason = eCURL_ABORT_REASON_STALL_TIMEDOUT;
						rc = -1;
					}
				}
				else
				{ // received additional bytes - update state to track new size/time
					context->downloadSize = dlnow;
					context->downloadUpdatedTime = NOW_STEADY_TS_MS;
				}
			}
		}
		if (dlnow == 0 && context->startTimeout > 0)
		{ // check to handle scenario where <startTimeout> seconds delay occurs without any bytes having been downloaded (stall at start)
			double timeElapsedInSec = (double)(NOW_STEADY_TS_MS - context->downloadStartTime) / 1000; //in secs  //CID:85922 - UNINTENDED_INTEGER_DIVISION
			if (timeElapsedInSec >= context->startTimeout)
			{
				AAMPLOG_WARN("Abort download as no data received for %.2f seconds", timeElapsedInSec);
				context->abortReason = eCURL_ABORT_REASON_START_TIMEDOUT;
				rc = -1;
			}
		}
		if (dlnow > 0 && context->lowBWTimeout> 0 && eMEDIATYPE_VIDEO == context->fileType)
		{
			double elapsedTimeMs = (double)(NOW_STEADY_TS_MS - context->downloadStartTime);
			if( elapsedTimeMs >= context->lowBWTimeout*1000 )
			{
				double predictedTotalDownloadTimeMs = elapsedTimeMs*dltotal/dlnow;
				if( predictedTotalDownloadTimeMs > aamp->mNetworkTimeoutMs )
				{
					AAMPLOG_WARN("lowBWTimeout=%ds; predictedTotalDownloadTime=%fs>%fs (network timeout)",
								 context->lowBWTimeout,
								 predictedTotalDownloadTimeMs/1000.0,
								 aamp->mNetworkTimeoutMs/1000.0 );
					context->abortReason = eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT;
					rc = -1;
				}
			}
		}
	}

	if(rc)
	{
		if( !( eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT == context->abortReason || eCURL_ABORT_REASON_START_TIMEDOUT == context->abortReason ||\
			eCURL_ABORT_REASON_STALL_TIMEDOUT == context->abortReason ) && (ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && mOrigManifestUrl.isRemotehost ) )
		{
			rc = 0;
		}
		else
		{
			AAMPLOG_WARN("CurlTrace Progress interrupted, ret:%d", rc);
		}
	}
	return rc;
}

/**
 * @brief
 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
 * @param dltotal total bytes expected to download
 * @param dlnow downloaded bytes so far
 * @param ultotal total bytes expected to upload
 * @param ulnow uploaded bytes so far
 * @retval
 */
static int progress_callback(
	void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
	double dltotal, // total bytes expected to download
	double dlnow, // downloaded bytes so far
	double ultotal, // total bytes expected to upload
	double ulnow // uploaded bytes so far
	)
{
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;

	return context->aamp->HandleSSLProgressCallback ( clientp, dltotal, dlnow, ultotal, ulnow );
}

// End of curl callback functions

/**
 * @brief PrivateInstanceAAMP Constructor
 */
PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig *config) : mReportProgressPosn(0.0), mAbrBitrateData(), mLock(), mMutexAttr(),
	mpStreamAbstractionAAMP(NULL), mInitSuccess(false), mVideoFormat(FORMAT_INVALID), mAudioFormat(FORMAT_INVALID), mDownloadsDisabled(),
	mDownloadsEnabled(true), mStreamSink(NULL), profiler(), licenceFromManifest(false), previousAudioType(eAUDIO_UNKNOWN),isPreferredDRMConfigured(false),
	mbDownloadsBlocked(false), streamerIsActive(false), mTSBEnabled(false), mIscDVR(false), mLiveOffset(AAMP_LIVE_OFFSET), 
	seek_pos_seconds(-1), rate(0), pipeline_paused(false), mMaxLanguageCount(0), zoom_mode(VIDEO_ZOOM_FULL),
	video_muted(false), subtitles_muted(true), audio_volume(100), subscribedTags(), responseHeaders(), httpHeaderResponses(), timedMetadata(), timedMetadataNew(), IsTuneTypeNew(false), trickStartUTCMS(-1),mLogTimetoTopProfile(true),
	durationSeconds(0.0), culledSeconds(0.0), culledOffset(0.0), maxRefreshPlaylistIntervalSecs(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS/1000),
	mEventListener(NULL), mNewSeekPos(0.0), mNewSeekPosTime(0), discardEnteringLiveEvt(false),
	mIsRetuneInProgress(false), mCondDiscontinuity(), mDiscontinuityTuneOperationId(0), mIsVSS(false),
	m_fd(-1), mIsLive(false), mIsAudioContextSkipped(false), mLogTune(false), mTuneCompleted(false), mFirstTune(true), mfirstTuneFmt(-1), mTuneAttempts(0), mPlayerLoadTime(0),
	mState(eSTATE_RELEASED), mMediaFormat(eMEDIAFORMAT_HLS), mPersistedProfileIndex(0), mAvailableBandwidth(0),
	mDiscontinuityTuneOperationInProgress(false), mContentType(ContentType_UNKNOWN), mTunedEventPending(false),
	mSeekOperationInProgress(false), mTrickplayInProgress(false), mPendingAsyncEvents(), mCustomHeaders(),
	mCMCDNextObjectRequest(""),mCMCDBandwidth(0),
	mManifestUrl(""), mTunedManifestUrl(""), mOrigManifestUrl(), mServiceZone(), mVssVirtualStreamId(),
	mCurrentLanguageIndex(0),
	preferredLanguagesString(), preferredLanguagesList(), preferredLabelList(),mhAbrManager(),
#ifdef SESSION_STATS
	mVideoEnd(NULL),
#endif
	mTimeToTopProfile(0),mTimeAtTopProfile(0),mPlaybackDuration(0),mTraceUUID(),
	mIsFirstRequestToFOG(false),
	mPausePositionMonitorMutex(), mPausePositionMonitorCV(), mPausePositionMonitoringThreadID(), mPausePositionMonitoringThreadStarted(false),
	mTuneType(eTUNETYPE_NEW_NORMAL)
	,mCdaiObject(NULL), mAdEventsQ(),mAdEventQMtx(), mAdPrevProgressTime(0), mAdCurOffset(0), mAdDuration(0), mAdProgressId("")
	,mBufUnderFlowStatus(false), mVideoBasePTS(0)
	,mCustomLicenseHeaders(), mIsIframeTrackPresent(false), mManifestTimeoutMs(-1), mNetworkTimeoutMs(-1)
	,mbPlayEnabled(true), mPlayerPreBuffered(false), mPlayerId(PLAYERID_CNTR++),mAampCacheHandler(NULL)
	,mAsyncTuneEnabled(false) 
	,waitforplaystart() 
	,mCurlShared(NULL)
	,mDrmDecryptFailCount(MAX_SEG_DRM_DECRYPT_FAIL_COUNT)
	,mPlaylistTimeoutMs(-1)
	,mMutexPlaystart()
#ifdef AAMP_HLS_DRM
    , fragmentCdmEncrypted(false) ,drmParserMutex(), aesCtrAttrDataList()
	, drmSessionThreadStarted(false), createDRMSessionThreadID(0)
#endif
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM)
	, mDRMSessionManager(NULL)
#endif
	,  mPreCachePlaylistThreadId(0), mPreCachePlaylistThreadFlag(false) , mPreCacheDnldList()
	, mPreCacheDnldTimeWindow(0), mParallelPlaylistFetchLock(), mAppName()
	, mProgressReportFromProcessDiscontinuity(false)
	, prevPositionMiliseconds(-1)
	, mPausePositionMilliseconds(AAMP_PAUSE_POSITION_INVALID_POSITION)
	, mPlaylistFetchFailError(0L),mAudioDecoderStreamSync(true)
	, mCurrentDrm(), mDrmInitData(), mMinInitialCacheSeconds(DEFAULT_MINIMUM_INIT_CACHE_SECONDS)
	//, mLicenseServerUrls()
	, mFragmentCachingRequired(false), mFragmentCachingLock()
	, mPauseOnFirstVideoFrameDisp(false)
	, mPreferredTextTrack(), mFirstVideoFrameDisplayedEnabled(false)
	, mSessionToken() 
	, vDynamicDrmData()
	, midFragmentSeekCache(false)
	, mPreviousAudioType (FORMAT_INVALID)
	, mTsbRecordingId()
	, mthumbIndexValue(-1)
	, mManifestRefreshCount (0)
	, mJumpToLiveFromPause(false), mPausedBehavior(ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE), mSeekFromPausedState(false)
	, mProgramDateTime (0), mMPDPeriodsInfo()
	, mProfileCappedStatus(false),schemeIdUriDai("")
	, mDisplayWidth(0)
	, mDisplayHeight(0)
	, preferredRenditionString("")
	, preferredRenditionList()
	, preferredTypeString("")
	, preferredCodecString("")
	, preferredCodecList()
	, mAudioTuple()
	, preferredLabelsString("")
	, preferredAudioAccessibilityNode()
	, preferredTextLanguagesString("")
	, preferredTextLanguagesList()
	, preferredTextRenditionString("")
	, preferredTextTypeString("")
	, preferredTextLabelString("")
	, preferredTextAccessibilityNode()
	, mProgressReportOffset(-1)
	, mAutoResumeTaskId(AAMP_TASK_ID_INVALID), mAutoResumeTaskPending(false), mScheduler(NULL), mEventLock(), mEventPriority(G_PRIORITY_DEFAULT_IDLE)
	, mStreamLock()
	, mConfig (config),mSubLanguage(), mHarvestCountLimit(0), mHarvestConfig(0)
	, mIsWVKIDWorkaround(false)
	, mAuxFormat(FORMAT_INVALID), mAuxAudioLanguage()
	, mAbsoluteEndPosition(0), mIsLiveStream(false)
	, mbUsingExternalPlayer (false)
	, mCCId(0)
	, seiTimecode()
	, contentGaps()
	, mAampLLDashServiceData{}
	, bLowLatencyServiceConfigured(false)
	, bLLDashAdjustPlayerSpeed(false)
	, mLLDashCurrentPlayRate(AAMP_NORMAL_PLAY_RATE)
	, mLLDashRateCorrectionCount(0)
	, mLLDashRetuneCount(0)
	, vidTimeScale(0)
	, audTimeScale(0)
	, speedCache {}
	, mTime (0)
	, mCurrentLatency(0)
	, mLiveOffsetAppRequest(false)
	, bLowLatencyStartABR(false)
	, mWaitForDiscoToComplete()
	, mDiscoCompleteLock()
	, mIsPeriodChangeMarked(false)
	, mLogObj(NULL)
	, mEventManager (NULL)
	, mbDetached(false)
	, mIsFakeTune(false)
	, mbSeeked(false)
	, mCurrentAudioTrackId(-1)
	, mCurrentVideoTrackId(-1)
	, mIsTrackIdMismatch(false)
	, mIsDefaultOffset(false)
	, mNextPeriodDuration(0)
	, mNextPeriodStartTime(0)
	, mNextPeriodScaledPtoStartTime(0)
	, mOffsetFromTunetimeForSAPWorkaround(0)
	, mLanguageChangeInProgress(false)
	, mSupportedTLSVersion(0)
	, mFailureReason("")
	, mTimedMetadataStartTime(0)
	, mTimedMetadataDuration(0)
	, mPlaybackMode("UNKNOWN")
	, playerStartedWithTrickPlay(false)
	, mApplyVideoRect(false)
	, mVideoRect{}
	, mData(NULL)
	, bitrateList()
	, userProfileStatus(false)
	, mApplyCachedVideoMute(false)
	, mFirstProgress(false)
	, mTsbSessionRequestUrl()
	, mWaitForDynamicDRMToUpdate()
	, mDynamicDrmUpdateLock()
	, mDynamicDrmCache()
	, mAudioComponentCount(-1)
	, mVideoComponentCount(-1)
	, mAudioOnlyPb(false)
	, mVideoOnlyPb(false)
	, mCurrentAudioTrackIndex(-1)
	, mCurrentTextTrackIndex(-1)
	, mcurrent_keyIdArray()
	, mDynamicDrmDefaultconfig()
	, mMediaDownloadsEnabled()
	, playerrate(1.0)
	, mSetPlayerRateAfterFirstframe(false)
	, mEncryptedPeriodFound(false)
	, mPipelineIsClear(false)
{
	for(int i=0; i<eMEDIATYPE_DEFAULT; i++)
	{
		lastId3Data[i] = NULL;
		lastId3DataLen[i] = 0;
	}
	mLogObj = mConfig->GetLoggerInstance();
	//LazilyLoadConfigIfNeeded();
	mAampCacheHandler = new AampCacheHandler(mConfig->GetLoggerInstance());
#ifdef AAMP_CC_ENABLED
	AampCCManager::GetInstance()->SetLogger(mConfig->GetLoggerInstance());
#endif
	profiler.SetLogger(mConfig->GetLoggerInstance());
	// Create the event manager for player instance 
	mEventManager = new AampEventManager(mLogObj);
	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_UserAgent, (std::string )AAMP_USERAGENT_BASE_STRING);
	int maxDrmSession;
	GETCONFIGVALUE_PRIV(eAAMPConfig_MaxDASHDRMSessions,maxDrmSession);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLanguage,preferredLanguagesString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioRendition,preferredRenditionString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioCodec,preferredCodecString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLabel,preferredLabelsString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioType,preferredTypeString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextRendition,preferredTextRenditionString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLanguage,preferredTextLanguagesString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLabel,preferredTextLabelString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextType,preferredTextTypeString);
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM)
	mDRMSessionManager = new AampDRMSessionManager(mLogObj, maxDrmSession);
#endif
	pthread_cond_init(&mDownloadsDisabled, NULL);
   	GETCONFIGVALUE_PRIV(eAAMPConfig_SubTitleLanguage,mSubLanguage); 
	pthread_mutexattr_init(&mMutexAttr);
	pthread_mutexattr_settype(&mMutexAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mLock, &mMutexAttr);
	pthread_mutex_init(&mParallelPlaylistFetchLock, &mMutexAttr);
	pthread_mutex_init(&mFragmentCachingLock, &mMutexAttr);
	pthread_mutex_init(&mEventLock, &mMutexAttr);
	pthread_mutex_init(&mDynamicDrmUpdateLock,&mMutexAttr);
	pthread_mutex_init(&mStreamLock, &mMutexAttr);
	pthread_mutex_init(&mDiscoCompleteLock,&mMutexAttr);

	for (int i = 0; i < eCURLINSTANCE_MAX; i++)
	{
		curl[i] = NULL;
		//cookieHeaders[i].clear();
		httpRespHeaders[i].type = eHTTPHEADERTYPE_UNKNOWN;
		httpRespHeaders[i].data.clear();
		curlDLTimeout[i] = 0;
	}

	if( ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) )
	{
		for (int i = 0; i < eCURLINSTANCE_MAX; i++)
		{
			curlhost[i] = new eCurlHostMap();
		}
	}

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		mbTrackDownloadsBlocked[i] = false;
		mTrackInjectionBlocked[i] = false;
		lastUnderFlowTimeMs[i] = 0;
		mProcessingDiscontinuity[i] = false;
		mIsDiscontinuityIgnored[i] = false;
	}

	pthread_mutex_lock(&gMutex);
	gActivePrivAAMP_t gAAMPInstance = { this, false, 0 };
	gActivePrivAAMPs.push_back(gAAMPInstance);
	pthread_mutex_unlock(&gMutex);
	mPendingAsyncEvents.clear();

	if (ISCONFIGSET_PRIV(eAAMPConfig_WifiCurlHeader)) {
		if (true == IsActiveStreamingInterfaceWifi()) {
			mCustomHeaders["Wifi:"] = std::vector<std::string> { "1" };
			activeInterfaceWifi = true;
		}
		else
		{
			mCustomHeaders["Wifi:"] = std::vector<std::string> { "0" };
			activeInterfaceWifi = false;
		}
	}
	// Add Connection: Keep-Alive custom header - DELIA-26832
	mCustomHeaders["Connection:"] = std::vector<std::string> { "Keep-Alive" };
	pthread_cond_init(&mCondDiscontinuity, NULL);
	pthread_cond_init(&waitforplaystart, NULL);
	pthread_cond_init(&mWaitForDynamicDRMToUpdate,NULL);
	pthread_mutex_init(&mMutexPlaystart, NULL);
	pthread_cond_init(&mWaitForDiscoToComplete,NULL);
	preferredLanguagesList.push_back("en");

#ifdef AAMP_HLS_DRM
	memset(&aesCtrAttrDataList, 0, sizeof(aesCtrAttrDataList));
	pthread_mutex_init(&drmParserMutex, NULL);
#endif
	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestCountLimit,mHarvestCountLimit);
	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestConfig,mHarvestConfig);
	mAsyncTuneEnabled = ISCONFIGSET_PRIV(eAAMPConfig_AsyncTune);
	}

/**
 * @brief PrivateInstanceAAMP Destructor
 */
PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
	StopPausePositionMonitoring();
#ifdef AAMP_CC_ENABLED
    AampCCManager::GetInstance()->Release(mCCId);
    mCCId = 0;
#endif
    pthread_mutex_lock(&gMutex);
	auto iter = std::find_if(std::begin(gActivePrivAAMPs), std::end(gActivePrivAAMPs), [this](const gActivePrivAAMP_t& el)
	{
		return el.pAAMP == this;
	});
	if(iter != gActivePrivAAMPs.end())
	{
		gActivePrivAAMPs.erase(iter);
	}
	pthread_mutex_unlock(&gMutex);

	mMediaDownloadsEnabled.clear();
	pthread_mutex_lock(&mLock);

#ifdef SESSION_STATS
	SAFE_DELETE(mVideoEnd);
#endif
	pthread_mutex_unlock(&mLock);

	pthread_cond_destroy(&mDownloadsDisabled);
	pthread_cond_destroy(&mWaitForDynamicDRMToUpdate);
	pthread_cond_destroy(&mCondDiscontinuity);
	pthread_cond_destroy(&waitforplaystart);
	pthread_cond_destroy(&mWaitForDiscoToComplete);
	pthread_mutex_destroy(&mMutexPlaystart);
	pthread_mutex_destroy(&mLock);
	pthread_mutex_destroy(&mDynamicDrmUpdateLock);
	pthread_mutex_destroy(&mParallelPlaylistFetchLock);
	pthread_mutex_destroy(&mFragmentCachingLock);
	pthread_mutex_destroy(&mEventLock);
	pthread_mutex_destroy(&mStreamLock);
	pthread_mutex_destroy(&mDiscoCompleteLock);
	pthread_mutexattr_destroy(&mMutexAttr);
#ifdef AAMP_HLS_DRM
	aesCtrAttrDataList.clear();
	pthread_mutex_destroy(&drmParserMutex);
#endif
	SAFE_DELETE(mAampCacheHandler);

#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM)
	SAFE_DELETE(mDRMSessionManager);
#endif

	if( ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) )
	{
		for (int i = 0; i < eCURLINSTANCE_MAX; i++)
		{
			SAFE_DELETE(curlhost[i]);
		}
	}

	if ( !(ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore)) )
	{
		if(mCurlShared)
		{
			curl_share_cleanup(mCurlShared);
			mCurlShared = NULL;
		}
	}
#ifdef IARM_MGR
	IARM_Bus_RemoveEventHandler("NET_SRV_MGR", IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, getActiveInterfaceEventHandler);
#endif //IARM_MGR
	SAFE_DELETE(mEventManager);

	if (HasSidecarData())
	{ // has sidecar data
		SAFE_DELETE_ARRAY(mData);
		mData = NULL;
		if (mpStreamAbstractionAAMP)
			mpStreamAbstractionAAMP->ResetSubtitle();
	}
}

/**
 * @brief perform pause of the pipeline and notifications for PauseAt functionality
 */
static gboolean PrivateInstanceAAMP_PausePosition(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP* )ptr;
	long long pausePositionMilliseconds = aamp->mPausePositionMilliseconds;
	aamp->mPausePositionMilliseconds = AAMP_PAUSE_POSITION_INVALID_POSITION;

	if  (pausePositionMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION)
	{
		if(aamp->mStreamSink->Pause(true, false))
		{
			aamp->pipeline_paused = true;
			aamp->NotifySpeedChanged(0, true);
		}

		aamp->StopDownloads();

		if (aamp->mpStreamAbstractionAAMP)
		{
			aamp->mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
		}

		AAMPLOG_INFO("paused at pos %lldms requested pos %lldms",
					 aamp->GetPositionMilliseconds(), pausePositionMilliseconds);

		if ((aamp->rate > AAMP_NORMAL_PLAY_RATE) || (aamp->rate < AAMP_RATE_PAUSE))
		{
			aamp->seek_pos_seconds = pausePositionMilliseconds / 1000.0;
			aamp->trickStartUTCMS = -1;
			AAMPLOG_INFO("Updated seek pos %fs", aamp->seek_pos_seconds);
		}
		else
		{
			// (See SetRateInternal)
			if (!ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery) && !aamp->mbDetached)
			{
				aamp->seek_pos_seconds = aamp->GetPositionSeconds();
				aamp->trickStartUTCMS = -1;
				AAMPLOG_INFO("Updated seek pos %fs", aamp->seek_pos_seconds);
			}
		}
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief the PositionMonitoring thread used for PauseAt functionality
 */
void PrivateInstanceAAMP::RunPausePositionMonitoring(void)
{
	long long localPauseAtMilliseconds = mPausePositionMilliseconds;
	long long posMs = GetPositionMilliseconds();

	while(localPauseAtMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION)
	{
		int pollPeriodMs = AAMP_PAUSE_POSITION_POLL_PERIOD_MS;
		long long trickplayTargetPosMs = localPauseAtMilliseconds;
		bool forcePause = false;

		if ((rate == AAMP_RATE_PAUSE) || pipeline_paused)
		{
			// Shouldn't get here if already paused
			AAMPLOG_WARN("Already paused, exiting loop");
			mPausePositionMilliseconds = AAMP_PAUSE_POSITION_INVALID_POSITION;
			break;
		}
		// If normal speed or slower, i.e. not iframe trick mode
		else if ((rate > AAMP_RATE_PAUSE) && (rate <= AAMP_NORMAL_PLAY_RATE))
		{
			// If current pos is within a poll period of the target position,
			// set the sleep time to be the difference, and then perform
			// the pause.
			if (posMs >= (localPauseAtMilliseconds - pollPeriodMs))
			{
				pollPeriodMs = (localPauseAtMilliseconds - posMs) / rate;
				forcePause = true;
				AAMPLOG_INFO("Requested pos %lldms current pos %lldms rate %f, pausing in %dms",
							localPauseAtMilliseconds, posMs, rate, pollPeriodMs);
			}
			else
			{
				AAMPLOG_INFO("Requested pos %lldms current pos %lldms rate %f, polling period %dms",
							localPauseAtMilliseconds, posMs, rate, pollPeriodMs);
			}
		}
		else
		{
			int vodTrickplayFPS = 0;
			bool config_valid = GETCONFIGVALUE_PRIV(eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS);

			assert (config_valid && (vodTrickplayFPS != 0));

			// Poll at half the frame period (twice the frame rate)
			pollPeriodMs = (1000 / vodTrickplayFPS) / 2;

			// If rate > 0, the target position should be earlier than requested pos
			// If rate < 0, the target position should be later than requested pos
			trickplayTargetPosMs -= ((rate * 1000) / vodTrickplayFPS);

			AAMPLOG_INFO("Requested pos %lldms current pos %lldms target pos %lld rate %f, fps %d, polling period %dms",
						 localPauseAtMilliseconds, posMs, trickplayTargetPosMs, rate, vodTrickplayFPS, pollPeriodMs);
		}

		// The calculation of pollPeriodMs for playback speeds, could result in a negative value
		if (pollPeriodMs > 0)
		{
			std::unique_lock<std::mutex> lock(mPausePositionMonitorMutex);
			std::cv_status cvStatus = std::cv_status::no_timeout;
			std::chrono::time_point<std::chrono::system_clock> waitUntilMs = std::chrono::system_clock::now() +
																			 std::chrono::milliseconds(pollPeriodMs);

			// Wait until now + pollPeriodMs, unless pauseAt is being cancelled
			while ((localPauseAtMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION) &&
				   (cvStatus == std::cv_status::no_timeout))
			{
				cvStatus = mPausePositionMonitorCV.wait_until(lock, waitUntilMs);
				localPauseAtMilliseconds = mPausePositionMilliseconds;
			}
			if (localPauseAtMilliseconds == AAMP_PAUSE_POSITION_INVALID_POSITION)
			{
				break;
			}
		}

		// Only need to get an updated pos if not forcing pause
		if (!forcePause)
		{
			posMs = GetPositionMilliseconds();
		}

		// Check if forcing pause at playback, or exceeded target position for trickplay
		if (forcePause ||
			((rate > AAMP_NORMAL_PLAY_RATE) && (posMs >= trickplayTargetPosMs)) ||
			((rate < AAMP_RATE_PAUSE) && (posMs <= trickplayTargetPosMs)))
		{
			(void)ScheduleAsyncTask(PrivateInstanceAAMP_PausePosition, this, "PrivateInstanceAAMP_PausePosition");
			break;
		}
	}
}

/**
 * @brief call the PausePositionMonitoring thread loop
 */
static void *PausePositionMonitor(void *arg)
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)arg;

	// Thread name restricted to 16 characters, including null
	if(aamp_pthread_setname(pthread_self(), "aampPauseMon"))
	{
		AAMPLOG_WARN("aamp_pthread_setname failed");
	}
	aamp->RunPausePositionMonitoring();
	return nullptr;
}

/**
 * @brief start the PausePositionMonitoring thread used for PauseAt functionality
 */
void PrivateInstanceAAMP::StartPausePositionMonitoring(long long pausePositionMilliseconds)
{
	if (mPausePositionMonitoringThreadStarted)
	{
		StopPausePositionMonitoring();
	}

	if (pausePositionMilliseconds < 0)
	{
		AAMPLOG_ERR("The position (%lld) must be >= 0", pausePositionMilliseconds);
	}
	else
	{
		mPausePositionMilliseconds = pausePositionMilliseconds;

		AAMPLOG_INFO("Start PausePositionMonitoring at position %lld", pausePositionMilliseconds);

		if (0 == pthread_create(&mPausePositionMonitoringThreadID, nullptr, &PausePositionMonitor, this))
		{
			mPausePositionMonitoringThreadStarted = true;
		}
		else
		{
			AAMPLOG_ERR("Failed to create PausePositionMonitor thread");
		}
	}
}

/**
 * @brief stop the PausePositionMonitoring thread used for PauseAt functionality
 */
void PrivateInstanceAAMP::StopPausePositionMonitoring(void)
{
	if (mPausePositionMonitoringThreadStarted)
	{
		AAMPLOG_INFO("Stopping PausePositionMonitoring");

		std::unique_lock<std::mutex> lock(mPausePositionMonitorMutex);
		mPausePositionMilliseconds = AAMP_PAUSE_POSITION_INVALID_POSITION;
		mPausePositionMonitorCV.notify_one();
		lock.unlock();

		int rc = pthread_join(mPausePositionMonitoringThreadID, NULL);
		if (rc != 0)
		{
			AAMPLOG_ERR("***pthread_join PausePositionMonitor returned %d(%s)", rc, strerror(rc));
		}
		else
		{
			AAMPLOG_TRACE("joined PositionMonitor");
		}
		mPausePositionMonitoringThreadStarted = false;
	}
}

/**
 * @brief wait for Discontinuity handling complete
 */
void PrivateInstanceAAMP::WaitForDiscontinuityProcessToComplete(void)
{
	pthread_mutex_lock(&mDiscoCompleteLock);
	pthread_cond_wait(&mWaitForDiscoToComplete, &mDiscoCompleteLock);
	pthread_mutex_unlock(&mDiscoCompleteLock);
}

/**
 * @brief unblock wait for Discontinuity handling complete
 */
void PrivateInstanceAAMP::UnblockWaitForDiscontinuityProcessToComplete(void)
{
	mIsPeriodChangeMarked = false;

	pthread_mutex_lock(&mDiscoCompleteLock);
	pthread_cond_signal(&mWaitForDiscoToComplete);
	pthread_mutex_unlock(&mDiscoCompleteLock);
}

/**
 *   @brief GStreamer operation start
 */
void PrivateInstanceAAMP::SyncBegin(void)
{
	pthread_mutex_lock(&mLock);
}

/**
 * @brief GStreamer operation end
 *
 */
void PrivateInstanceAAMP::SyncEnd(void)
{
	pthread_mutex_unlock(&mLock);
}

/**
 * @brief Report progress event
 */
long long PrivateInstanceAAMP::GetVideoPTS(bool bAddVideoBasePTS)
{
    /*For HLS, tsprocessor.cpp removes the base PTS value and sends to gstreamer.
    **In order to report PTS of video currently being played out, we add the base PTS
    **to video PTS received from gstreamer
    */
    /*For DASH,mVideoBasePTS value will be zero */
    long long videoPTS = -1;
    videoPTS = mStreamSink->GetVideoPTS();
    if(bAddVideoBasePTS)
       videoPTS += mVideoBasePTS;
    AAMPLOG_WARN("Video-PTS=%lld, mVideoBasePTS=%lld Add VideoBase PTS[%d]",videoPTS,mVideoBasePTS,bAddVideoBasePTS);
    return videoPTS;
}

/**
 * @brief Report progress event to listeners
 */
void PrivateInstanceAAMP::ReportProgress(bool sync, bool beginningOfStream)
{
	PrivAAMPState state;
	GetState(state);

	if (state == eSTATE_SEEKING)
	{
		AAMPLOG_WARN("Progress reporting skipped whilst seeking.");
	}

	//Once GST_MESSAGE_EOS is received, AAMP does not want any stray progress to be sent to player. so added the condition state != eSTATE_COMPLETE
	if (mDownloadsEnabled && (state != eSTATE_IDLE) && (state != eSTATE_RELEASED) && (state != eSTATE_COMPLETE) && (state != eSTATE_SEEKING))
	{
		ReportAdProgress(sync);

		// set position to 0 if the rewind operation has reached Beginning Of Stream
		double position = beginningOfStream? 0: GetPositionMilliseconds();
		double duration = durationSeconds * 1000.0;
		float speed = pipeline_paused ? 0 : rate;
		double start = -1;
		double end = -1;
		long long videoPTS = -1;
		double bufferedDuration = 0.0;
		bool bProcessEvent = true;

		// If tsb is not available for linear send -1  for start and end
		// so that xre detect this as tsbless playabck
		// Override above logic if mEnableSeekableRange is set, used by third-party apps
		if (!ISCONFIGSET_PRIV(eAAMPConfig_EnableSeekRange) && (mContentType == ContentType_LINEAR && !mTSBEnabled))
		{
			start = -1;
			end = -1;
		}
		else
		{	//DELIA-49735 - Report Progress report position based on Availability Start Time
			start = (culledSeconds*1000.0);
			if(ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) && (mProgressReportOffset >= 0) && IsLiveStream() && !IsUninterruptedTSB())
			{
				end = (mAbsoluteEndPosition * 1000);
			}
			else
			{
				end = start + duration;
			}

			if (position > end)
			{ // clamp end
				//AAMPLOG_WARN("aamp clamp end");
				position = end;
			}
			else if (position < start)
			{ // clamp start
				//AAMPLOG_WARN("aamp clamp start");
				position = start;
			}
		}

		if(ISCONFIGSET_PRIV(eAAMPConfig_ReportVideoPTS))
		{
				/*For HLS, tsprocessor.cpp removes the base PTS value and sends to gstreamer.
				**In order to report PTS of video currently being played out, we add the base PTS
				**to video PTS received from gstreamer
				*/
				/*For DASH,mVideoBasePTS value will be zero */
				videoPTS = mStreamSink->GetVideoPTS() + mVideoBasePTS;
		}

		pthread_mutex_lock(&mStreamLock);
		if (mpStreamAbstractionAAMP)
		{
			bufferedDuration = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec() * 1000.0;
		}
		pthread_mutex_unlock(&mStreamLock);

		if ((mReportProgressPosn == position) && !pipeline_paused && beginningOfStream != true)
		{
			// Avoid sending the progress event, if the previous position and the current position is same when pipeline is in playing state.
                	// Addded exception if it's beginning of stream to prevent JSPP not loading previous AD while rewind
			bProcessEvent = false;
		}

		/*LLAMA-7142
		**These variables are:
		**  -Used by PlayerInstanceAAMP::SetRateInternal() to calculate seek position.
		**  -Included here for consistency with previous code but aren't directly related to reporting.
		**  -A good candidate for future refactoring*/
		mNewSeekPos = position;
		mNewSeekPosTime = aamp_GetCurrentTimeMS();

		double reportFormatPosition = position;
		if(ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) && ISCONFIGSET_PRIV(eAAMPConfig_InterruptHandling) && mTSBEnabled)
		{
			start -= (mProgressReportOffset * 1000);
			reportFormatPosition -= (mProgressReportOffset * 1000);
			end -= (mProgressReportOffset * 1000);
		}

		ProgressEventPtr evt = std::make_shared<ProgressEvent>(duration, reportFormatPosition, start, end, speed, videoPTS, bufferedDuration, seiTimecode.c_str());

		if (trickStartUTCMS >= 0 && (bProcessEvent || mFirstProgress))
		{
			if (mFirstProgress)
			{
				mFirstProgress = false;
				AAMPLOG_WARN("Send first progress event with position %ld", (long)(reportFormatPosition / 1000));
			}

			if (ISCONFIGSET_PRIV(eAAMPConfig_ProgressLogging))
			{
				static int tick;
				if ((tick++ % 4) == 0)
				{
					AAMPLOG_WARN("aamp pos: [%ld..%ld..%ld..%lld..%ld..%s]",
						(long)(start / 1000),
						(long)(reportFormatPosition / 1000),
						(long)(end / 1000),
						(long long) videoPTS,
						(long)(bufferedDuration / 1000),
						seiTimecode.c_str() );
				}
			}

			if (sync)
			{
				mEventManager->SendEvent(evt,AAMP_EVENT_SYNC_MODE);
			}
			else
			{
				mEventManager->SendEvent(evt);
			}

			mReportProgressPosn = position;
		}
	}
}

/**
 *   @brief Report Ad progress event to listeners
 *          Sending Ad progress percentage to JSPP
 */
void PrivateInstanceAAMP::ReportAdProgress(bool sync)
{
	if (mDownloadsEnabled && !mAdProgressId.empty())
	{
		long long curTime = NOW_STEADY_TS_MS;
		if (!pipeline_paused)
		{
			//Update the percentage only if the pipeline is in playing.
			mAdCurOffset += (uint32_t)(curTime - mAdPrevProgressTime);
			if(mAdCurOffset > mAdDuration) mAdCurOffset = mAdDuration;
		}
		mAdPrevProgressTime = curTime;

		AdPlacementEventPtr evt = std::make_shared<AdPlacementEvent>(AAMP_EVENT_AD_PLACEMENT_PROGRESS, mAdProgressId, (uint32_t)(mAdCurOffset * 100) / mAdDuration);
		if(sync)
		{
			mEventManager->SendEvent(evt,AAMP_EVENT_SYNC_MODE);
		}
		else
		{
			mEventManager->SendEvent(evt);
		}
	}
}

/**
 * @brief Update playlist duration
 */
void PrivateInstanceAAMP::UpdateDuration(double seconds)
{
	AAMPLOG_INFO("aamp_UpdateDuration(%f)", seconds);
	durationSeconds = seconds;
}

/**
 * @brief Update playlist culling
 */
void PrivateInstanceAAMP::UpdateCullingState(double culledSecs)
{
	if (culledSecs == 0)
	{
		return;
	}

	if((!this->culledSeconds) && culledSecs)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: culling started, first value %f", culledSecs);
	}

	this->culledSeconds += culledSecs;
	long long limitMs = (long long) std::round(this->culledSeconds * 1000.0);

	for (auto iter = timedMetadata.begin(); iter != timedMetadata.end(); )
	{
		// If the timed metadata has expired due to playlist refresh, remove it from local cache
		// For X-CONTENT-IDENTIFIER, -X-IDENTITY-ADS, X-MESSAGE_REF in DASH which has _timeMS as 0
		if (iter->_timeMS != 0 && iter->_timeMS < limitMs)
		{
			//AAMPLOG_WARN("ERASE(limit:%lld) aamp_ReportTimedMetadata(%lld, '%s', '%s', nb)", limitMs,iter->_timeMS, iter->_name.c_str(), iter->_content.c_str());
			//AAMPLOG_WARN("ERASE(limit:%lld) aamp_ReportTimedMetadata(%lld)", limitMs,iter->_timeMS);
			iter = timedMetadata.erase(iter);
		}
		else
		{
			iter++;
		}
	}

	// Remove contentGaps vector based on culling.
	if(ISCONFIGSET_PRIV(eAAMPConfig_InterruptHandling))
	{
		for (auto iter = contentGaps.begin(); iter != contentGaps.end();)
		{
			if (iter->_timeMS != 0 && iter->_timeMS < limitMs)
			{
				iter = contentGaps.erase(iter);
			}
			else
			{
				iter++;
			}
		}
	}

	// Check if we are paused and culled past paused playback position
	// AAMP internally caches fragments in sw and gst buffer, so we should be good here
	// Pipeline will be in Paused state when Lightning trickplay is done. During this state XRE will send the resume position to exit pause state .
	// Issue observed when culled position reaches the paused position during lightning trickplay and player resumes the playback with paused position as playback position ignoring XRE shown position.
	// Fix checks if the player is put into paused state with lighting mode(by checking last stored rate). 
  	// In this state player will not come out of Paused state, even if the culled position reaches paused position.
	// The rate check is a special case for a specific player, if this is contradicting to other players, we will have to add a config to enable/disable
	if (pipeline_paused && mpStreamAbstractionAAMP && (abs(rate) != AAMP_RATE_TRICKPLAY_MAX))
	{
		double position = GetPositionSeconds();
		double minPlaylistPositionToResume = (position < maxRefreshPlaylistIntervalSecs) ? position : (position - maxRefreshPlaylistIntervalSecs);
		if (this->culledSeconds >= position)
		{
			if (mPausedBehavior <= ePAUSED_BEHAVIOR_LIVE_IMMEDIATE)
			{
				// Immediate play from paused state, Execute player resume.
				// Live immediate - Play from live position
				// Autoplay immediate - Play from start of live window
				if(ePAUSED_BEHAVIOR_LIVE_IMMEDIATE == mPausedBehavior)
				{
					// Enable this flag to perform seek to live.
					mSeekFromPausedState = true;
				}
				AAMPLOG_WARN("Resume playback since playlist start position(%f) has moved past paused position(%f) ", this->culledSeconds, position);
				if (!mAutoResumeTaskPending)
				{
					mAutoResumeTaskPending = true;
					mAutoResumeTaskId = ScheduleAsyncTask(PrivateInstanceAAMP_Resume, (void *)this, "PrivateInstanceAAMP_Resume");
				}
				else
				{
					AAMPLOG_WARN("Auto resume playback task already exists, avoid creating duplicates for now!");
				}
			}
			else if(mPausedBehavior >= ePAUSED_BEHAVIOR_AUTOPLAY_DEFER)
			{
				// Wait for play() call to resume, enable mSeekFromPausedState for reconfigure.
				// Live differ - Play from live position
				// Autoplay differ -Play from eldest part (start of live window)
				mSeekFromPausedState = true;
				if(ePAUSED_BEHAVIOR_LIVE_DEFER == mPausedBehavior)
				{
					mJumpToLiveFromPause = true;
				}
			}
		}
		else if (this->culledSeconds >= minPlaylistPositionToResume)
		{
			// Here there is a chance that paused position will be culled after next refresh playlist
			// AAMP internally caches fragments in sw bufffer after paused position, so we are at less risk
			// Make sure that culledSecs is within the limits of maxRefreshPlaylistIntervalSecs
			// This check helps us to avoid initial culling done by FOG after channel tune

			if (culledSecs <= maxRefreshPlaylistIntervalSecs)
			{
				if (mPausedBehavior <= ePAUSED_BEHAVIOR_LIVE_IMMEDIATE)
				{
					if(ePAUSED_BEHAVIOR_LIVE_IMMEDIATE == mPausedBehavior)
					{
						mSeekFromPausedState = true;
					}
					AAMPLOG_WARN("Resume playback since start position(%f) moved very close to minimum resume position(%f) ", this->culledSeconds, minPlaylistPositionToResume);
					if (!mAutoResumeTaskPending)
					{
						mAutoResumeTaskPending = true;
						mAutoResumeTaskId = ScheduleAsyncTask(PrivateInstanceAAMP_Resume, (void *)this, "PrivateInstanceAAMP_Resume");
					}
					else
					{
						AAMPLOG_WARN("Auto resume playback task already exists, avoid creating duplicates for now!");
					}
				}
				else if(mPausedBehavior >= ePAUSED_BEHAVIOR_AUTOPLAY_DEFER)
				{
					mSeekFromPausedState = true;
					if(ePAUSED_BEHAVIOR_LIVE_DEFER == mPausedBehavior)
					{
						mJumpToLiveFromPause = true;
					}
				}
			}
			else
			{
				AAMPLOG_WARN("Auto resume playback task already exists, avoid creating duplicates for now!");
			}
		}
	}
}

/**
 * @brief Add listener to aamp events
 */
void PrivateInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	mEventManager->AddEventListener(eventType,eventListener);
}


/**
 * @brief Deregister event lister, Remove listener to aamp events
 */
void PrivateInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	mEventManager->RemoveEventListener(eventType,eventListener);
}

/**
 * @brief IsEventListenerAvailable Check if Event is registered
 */
bool PrivateInstanceAAMP::IsEventListenerAvailable(AAMPEventType eventType)
{
	return mEventManager->IsEventListenerAvailable(eventType);
}

/**
 * @brief Handles DRM errors and sends events to application if required.
 */
void PrivateInstanceAAMP::SendDrmErrorEvent(DrmMetaDataEventPtr event, bool isRetryEnabled)
{
	if (event)
	{
		AAMPTuneFailure tuneFailure = event->getFailure();
		long error_code = event->getResponseCode();
		bool isSecClientError = event->getSecclientError();
		long secManagerReasonCode = event->getSecManagerReasonCode();
		 
		if(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN == tuneFailure || AAMP_TUNE_LICENCE_REQUEST_FAILED == tuneFailure)
		{
			char description[128] = {};
			//When using secmanager the erro_code would not be less than 100
			if(AAMP_TUNE_LICENCE_REQUEST_FAILED == tuneFailure && (error_code < 100 || ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager)))
			{
				
				if (isSecClientError)
				{
					if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager))
					{
						snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : SecManager Error Code %ld:%ld", tuneFailureMap[tuneFailure].description,error_code, secManagerReasonCode);
					}
					else
					{
						snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Secclient Error Code %ld", tuneFailureMap[tuneFailure].description, error_code);
					}
				}
				else
				{
					snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Curl Error Code %ld", tuneFailureMap[tuneFailure].description, error_code);
				}
			}
			else if (AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN == tuneFailure && eAUTHTOKEN_TOKEN_PARSE_ERROR == (AuthTokenErrors)error_code)
			{
				snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Access Token Parse Error", tuneFailureMap[tuneFailure].description);
			}
			else if(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN == tuneFailure && eAUTHTOKEN_INVALID_STATUS_CODE == (AuthTokenErrors)error_code)
			{
				snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Invalid status code", tuneFailureMap[tuneFailure].description);
			}
			else
			{
				snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Http Error Code %ld", tuneFailureMap[tuneFailure].description, error_code);
			}
			SendErrorEvent(tuneFailure, description, isRetryEnabled, event->getSecManagerClassCode(),event->getSecManagerReasonCode(), event->getBusinessStatus());
		}
		else if(tuneFailure >= 0 && tuneFailure < AAMP_TUNE_FAILURE_UNKNOWN)
		{
			SendErrorEvent(tuneFailure, NULL, isRetryEnabled, event->getSecManagerClassCode(),event->getSecManagerReasonCode(), event->getBusinessStatus());
		}
		else
		{
			AAMPLOG_WARN("Received unknown error event %d", tuneFailure);
			SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN);
		}
	}
}

/**
 * @brief Handles download errors and sends events to application if required.
 */
void PrivateInstanceAAMP::SendDownloadErrorEvent(AAMPTuneFailure tuneFailure, long error_code)
{
	AAMPTuneFailure actualFailure = tuneFailure;
	bool retryStatus = true;

	if(tuneFailure >= 0 && tuneFailure < AAMP_TUNE_FAILURE_UNKNOWN)
	{
		char description[MAX_DESCRIPTION_SIZE] = {};
		if (((error_code >= PARTIAL_FILE_CONNECTIVITY_AAMP) && (error_code <= PARTIAL_FILE_START_STALL_TIMEOUT_AAMP)) || error_code == CURLE_OPERATION_TIMEDOUT)
		{
			switch(error_code)
			{
				case PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP:
						error_code = CURLE_PARTIAL_FILE;
				case CURLE_OPERATION_TIMEDOUT:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %ld, Download time expired", tuneFailureMap[tuneFailure].description, error_code);
						break;
				case PARTIAL_FILE_START_STALL_TIMEOUT_AAMP:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Start/Stall timeout", tuneFailureMap[tuneFailure].description, CURLE_PARTIAL_FILE);
						break;
				case OPERATION_TIMEOUT_CONNECTIVITY_AAMP:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Connectivity failure", tuneFailureMap[tuneFailure].description, CURLE_OPERATION_TIMEDOUT);
						break;
				case PARTIAL_FILE_CONNECTIVITY_AAMP:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Connectivity failure", tuneFailureMap[tuneFailure].description, CURLE_PARTIAL_FILE);
						break;
			}
		}
		else if(error_code < 100)
		{
			snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %ld", tuneFailureMap[tuneFailure].description, error_code);  //CID:86441 - DC>STRING_BUFFER
		}
		else
		{
			snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Http Error Code %ld", tuneFailureMap[tuneFailure].description, error_code);
			if (error_code == 404)
			{
				actualFailure = AAMP_TUNE_CONTENT_NOT_FOUND;
			}
			else if (error_code == 421)	// http 421 - Fog power saving mode failure
			{
				 retryStatus = false;
			}
		}
		if( IsTSBSupported() )
		{
			strcat(description, "(FOG)");
		}

		SendErrorEvent(actualFailure, description, retryStatus);
	}
	else
	{
		AAMPLOG_WARN("Received unknown error event %d", tuneFailure);
		SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN);
	}
}

/**
 * @brief Sends Anomaly Error/warning messages
 */
void PrivateInstanceAAMP::SendAnomalyEvent(AAMPAnomalyMessageType type, const char* format, ...)
{
	if(NULL != format && mEventManager->IsEventListenerAvailable(AAMP_EVENT_REPORT_ANOMALY))
	{
		va_list args;
		va_start(args, format);

		char msgData[MAX_ANOMALY_BUFF_SIZE];

		msgData[(MAX_ANOMALY_BUFF_SIZE-1)] = 0;
		vsnprintf(msgData, (MAX_ANOMALY_BUFF_SIZE-1), format, args);

		AnomalyReportEventPtr e = std::make_shared<AnomalyReportEvent>(type, msgData);

		AAMPLOG_INFO("Anomaly evt:%d msg:%s", e->getSeverity(), msgData);
		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		va_end(args);  //CID:82734 - VARAGAS
	}
}


/**
 *   @brief  Update playlist refresh interval
 */
void PrivateInstanceAAMP::UpdateRefreshPlaylistInterval(float maxIntervalSecs)
{
	AAMPLOG_INFO("maxRefreshPlaylistIntervalSecs (%f)", maxIntervalSecs);
	maxRefreshPlaylistIntervalSecs = maxIntervalSecs;
}

/**
 * @brief Sends UnderFlow Event messages
 */
void PrivateInstanceAAMP::SendBufferChangeEvent(bool bufferingStopped)
{
	// Buffer Change event indicate buffer availability 
	// Buffering stop notification need to be inverted to indicate if buffer available or not 
	// BufferChangeEvent with False = Underflow / non-availability of buffer to play 
	// BufferChangeEvent with True  = Availability of buffer to play 
	BufferingChangedEventPtr e = std::make_shared<BufferingChangedEvent>(!bufferingStopped); 

	SetBufUnderFlowStatus(bufferingStopped);
	AAMPLOG_INFO("PrivateInstanceAAMP: Sending Buffer Change event status (Buffering): %s", (e->buffering() ? "End": "Start"));
	SendEvent(e,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief To change the the gstreamer pipeline to pause/play 
 */
bool PrivateInstanceAAMP::PausePipeline(bool pause, bool forceStopGstreamerPreBuffering)
{
	if (true != mStreamSink->Pause(pause, forceStopGstreamerPreBuffering))
	{
		return false;
	}
	pipeline_paused = pause;
	return true;
}

/**
 * @brief Handles errors and sends events to application if required.
 * For download failures, use SendDownloadErrorEvent instead.
 */
void PrivateInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus)
{
	bool sendErrorEvent = false;
	pthread_mutex_lock(&mLock);
	if(mState != eSTATE_ERROR)
	{
		if(IsTSBSupported() && mState <= eSTATE_PREPARED)
		{
			// Send a TSB delete request when player is not tuned successfully.
			// If player is once tuned, retune happens with same content and player can reuse same TSB.
			std::string remoteUrl = "127.0.0.1:9080/tsb";
			long http_error = -1;
			ProcessCustomCurlRequest(remoteUrl, NULL, &http_error, eCURL_DELETE);
		}
		sendErrorEvent = true;
		mState = eSTATE_ERROR;
	}
	pthread_mutex_unlock(&mLock);
	if (sendErrorEvent)
	{
		int code;
		const char *errorDescription = NULL;
		DisableDownloads();
		if(tuneFailure >= 0 && tuneFailure < AAMP_TUNE_FAILURE_UNKNOWN)
		{
			if (tuneFailure == AAMP_TUNE_PLAYBACK_STALLED)
			{ // allow config override for stall detection error code
				GETCONFIGVALUE_PRIV(eAAMPConfig_StallErrorCode,code);
			}
			else
			{
				code = tuneFailureMap[tuneFailure].code;
			}
			if(description)
			{
				errorDescription = description;
			}
			else
			{
				errorDescription = tuneFailureMap[tuneFailure].description;
			}
		}
		else
		{
			code = tuneFailureMap[AAMP_TUNE_FAILURE_UNKNOWN].code;
			errorDescription = tuneFailureMap[AAMP_TUNE_FAILURE_UNKNOWN].description;
		}

		MediaErrorEventPtr e = std::make_shared<MediaErrorEvent>(tuneFailure, code, errorDescription, isRetryEnabled, secManagerClassCode, secManagerReasonCode, secClientBusinessStatus);
		SendAnomalyEvent(ANOMALY_ERROR, "Error[%d]:%s", tuneFailure, e->getDescription().c_str());
		if (!mAppName.empty())
		{
			AAMPLOG_ERR("%s PLAYER[%d] APP: %s Sending error %s",(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mAppName.c_str(), e->getDescription().c_str());
		}
		else
		{
			AAMPLOG_ERR("%s PLAYER[%d] Sending error %s",(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, e->getDescription().c_str());
		}

		if (rate != AAMP_NORMAL_PLAY_RATE)
		{
			NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE, false); // During trick play if the playback failed, send speed change event to XRE to reset its current speed rate.
		}

		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		mFailureReason=tuneFailureMap[tuneFailure].description;
	}
	else
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: Ignore error %d[%s]", (int)tuneFailure, description);
	}
}

/**
 * @brief Send event to listeners
 */
void PrivateInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
	mEventManager->SendEvent(eventData, eventMode);
}

/**
 * @brief Notify bit rate change event to listeners
 */
void PrivateInstanceAAMP::NotifyBitRateChangeEvent(int bitrate, BitrateChangeReason reason, int width, int height, double frameRate, double position, bool GetBWIndex, VideoScanType scantype, int aspectRatioWidth, int aspectRatioHeight)
{
	if(mEventManager->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED))
	{
		AAMPEventPtr event = std::make_shared<BitrateChangeEvent>((int)aamp_GetCurrentTimeMS(), bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);

		/* START: Added As Part of DELIA-28363 and DELIA-28247 */
		if(GetBWIndex)
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent :: bitrate:%d desc:%s width:%d height:%d fps:%f position:%f IndexFromTopProfile: %d%s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, mpStreamAbstractionAAMP->GetBWIndex(bitrate), (IsTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
		else
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent :: bitrate:%d desc:%s width:%d height:%d fps:%f position:%f %s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, (IsTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
		/* END: Added As Part of DELIA-28363 and DELIA-28247 */

		SendEvent(event,AAMP_EVENT_ASYNC_MODE);
	}
	else
	{
		/* START: Added As Part of DELIA-28363 and DELIA-28247 */
		if(GetBWIndex)
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent ::NO LISTENERS bitrate:%d desc:%s width:%d height:%d, fps:%f position:%f IndexFromTopProfile: %d%s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, mpStreamAbstractionAAMP->GetBWIndex(bitrate), (IsTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
		else
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent ::NO LISTENERS bitrate:%d desc:%s width:%d height:%d fps:%f position:%f %s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, (IsTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
		/* END: Added As Part of DELIA-28363 and DELIA-28247 */
	}

	AAMPLOG_WARN("BitrateChanged:%d", reason);
}


/**
 * @brief Notify speed change event to listeners
 */
void PrivateInstanceAAMP::NotifySpeedChanged(float rate, bool changeState)
{
	if (changeState)
	{
		if (rate == 0)
		{
			SetState(eSTATE_PAUSED);
			if (HasSidecarData())
			{ // has sidecar data
				if (mpStreamAbstractionAAMP)
					mpStreamAbstractionAAMP->MuteSubtitleOnPause();
			}
		}
		else if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			if (mTrickplayInProgress)
			{
				mTrickplayInProgress = false;
			}
			else
			{
				if (HasSidecarData())
				{ // has sidecar data
					if (mpStreamAbstractionAAMP)
						mpStreamAbstractionAAMP->ResumeSubtitleOnPlay(subtitles_muted, mData);
				}
			}
			SetState(eSTATE_PLAYING);
		}
		else
		{
			mTrickplayInProgress = true;
			if (HasSidecarData())
			{ // has sidecar data
				if (mpStreamAbstractionAAMP)
					mpStreamAbstractionAAMP->MuteSidecarSubtitles(true);
			}
		}
	}

#ifdef AAMP_CC_ENABLED
	if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
	{
		if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			AampCCManager::GetInstance()->SetTrickplayStatus(false);
		}
		else
		{
			AampCCManager::GetInstance()->SetTrickplayStatus(true);
		}
	}
#endif
	//Hack For DELIA-51318 convert the incoming rates into acceptable rates
	if(ISCONFIGSET_PRIV(eAAMPConfig_RepairIframes))
	{
		AAMPLOG_WARN("mRepairIframes is set, sending pseudo rate %d for the actual rate %d", getPseudoTrickplayRate(rate), rate);
		SendEvent(std::make_shared<SpeedChangedEvent>(getPseudoTrickplayRate(rate)),AAMP_EVENT_ASYNC_MODE);
	}
	else
	{
		SendEvent(std::make_shared<SpeedChangedEvent>(rate),AAMP_EVENT_ASYNC_MODE);
	}
#ifdef USE_SECMANAGER
	if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager))
	{
		mDRMSessionManager->setPlaybackSpeedState(rate,seek_pos_seconds);
	}
#endif
}

/**
 * @brief Send DRM metadata event
 */
void PrivateInstanceAAMP::SendDRMMetaData(DrmMetaDataEventPtr e)
{
	SendEvent(e,AAMP_EVENT_ASYNC_MODE);
        AAMPLOG_WARN("SendDRMMetaData name = %s value = %x", e->getAccessStatus().c_str(), e->getAccessStatusValue());
}

/**
 *   @brief Check if discontinuity processing is pending 
 */
bool PrivateInstanceAAMP::IsDiscontinuityProcessPending()
{
	bool vidDiscontinuity = (mVideoFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_VIDEO]);
	bool audDiscontinuity = (mAudioFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_AUDIO]);
	return (vidDiscontinuity || audDiscontinuity);
}

/**
 *   @brief Process pending discontinuity and continue playback of stream after discontinuity
 *
 *   @return true if pending discontinuity was processed successful, false if interrupted
 */
bool PrivateInstanceAAMP::ProcessPendingDiscontinuity()
{
	bool ret = true;
	SyncBegin();
	if (mDiscontinuityTuneOperationInProgress)
	{
		SyncEnd();
		AAMPLOG_WARN("PrivateInstanceAAMP: Discontinuity Tune Operation already in progress");
		UnblockWaitForDiscontinuityProcessToComplete();
		return ret; // true so that PrivateInstanceAAMP_ProcessDiscontinuity can cleanup properly
	}
	SyncEnd();
	
	if (!(DiscontinuitySeenInAllTracks()))
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: Discontinuity status of video - (%d), audio - (%d) and aux - (%d)", mProcessingDiscontinuity[eMEDIATYPE_VIDEO], mProcessingDiscontinuity[eMEDIATYPE_AUDIO], mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO]);
		UnblockWaitForDiscontinuityProcessToComplete();
		return ret; // true so that PrivateInstanceAAMP_ProcessDiscontinuity can cleanup properly
	}

	SyncBegin();
	mDiscontinuityTuneOperationInProgress = true;
	SyncEnd();
	
	if (DiscontinuitySeenInAllTracks())
	{
		bool continueDiscontProcessing = true;
		AAMPLOG_WARN("PrivateInstanceAAMP: mProcessingDiscontinuity set");
		// DELIA-46559, there is a chance that synchronous progress event sent will take some time to return back to AAMP
		// This can lead to discontinuity stall detection kicking in. So once we start discontinuity processing, reset the flags
		ResetDiscontinuityInTracks();
		ResetTrackDiscontinuityIgnoredStatus();
		lastUnderFlowTimeMs[eMEDIATYPE_VIDEO] = 0;
		lastUnderFlowTimeMs[eMEDIATYPE_AUDIO] = 0;
		lastUnderFlowTimeMs[eMEDIATYPE_AUX_AUDIO] = 0;

		{
			double newPosition = GetPositionSeconds();
			double injectedPosition = seek_pos_seconds + mpStreamAbstractionAAMP->GetLastInjectedFragmentPosition();
			double startTimeofFirstSample = 0;
			AAMPLOG_WARN("PrivateInstanceAAMP: last injected position:%f position calcualted: %f", injectedPosition, newPosition);

			// Reset with injected position from StreamAbstractionAAMP. This ensures that any drift in
			// GStreamer position reporting is taken care of.
			// BCOM-4765: Set seek_pos_seconds to injected position only in case of westerossink. In cases with
			// brcmvideodecoder, we have noticed a drift of 500ms for HLS-TS assets (due to PTS restamping
			if (injectedPosition != 0 && (fabs(injectedPosition - newPosition) < 5.0) && ISCONFIGSET_PRIV(eAAMPConfig_UseWesterosSink))
			{
				seek_pos_seconds = injectedPosition;
			}
			else
			{
				seek_pos_seconds = newPosition;
			}

			if(!IsUninterruptedTSB() && (mMediaFormat == eMEDIAFORMAT_DASH))
			{
				startTimeofFirstSample = mpStreamAbstractionAAMP->GetStartTimeOfFirstPTS() / 1000;
				if(startTimeofFirstSample > 0)
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: Position is updated to start time of discontinuity : %lf", startTimeofFirstSample);
					seek_pos_seconds = startTimeofFirstSample;
				}
			}
			AAMPLOG_WARN("PrivateInstanceAAMP: Updated seek_pos_seconds:%f", seek_pos_seconds);
		}
		trickStartUTCMS = -1;

		SyncBegin();
		mProgressReportFromProcessDiscontinuity = true;
		SyncEnd();

		// To notify app of discontinuity processing complete
		ReportProgress();

		// There is a chance some other operation maybe invoked from JS/App because of the above ReportProgress
		// Make sure we have still mDiscontinuityTuneOperationInProgress set
		SyncBegin();
		AAMPLOG_WARN("Progress event sent as part of ProcessPendingDiscontinuity, mDiscontinuityTuneOperationInProgress:%d", mDiscontinuityTuneOperationInProgress);
		mProgressReportFromProcessDiscontinuity = false;
		continueDiscontProcessing = mDiscontinuityTuneOperationInProgress;
		SyncEnd();

		if (continueDiscontProcessing)
		{
			// mStreamLock is not exactly required here, this will be called from Scheduler/GMainLoop based on AAMP config
			// The same thread will be executing operations involving TeardownStream.
			mpStreamAbstractionAAMP->StopInjection();
#ifndef AAMP_STOP_SINK_ON_SEEK
			if (mMediaFormat != eMEDIAFORMAT_HLS_MP4) // Avoid calling flush for fmp4 playback.
			{
				mStreamSink->Flush(mpStreamAbstractionAAMP->GetFirstPTS(), rate);
			}
#else
			mStreamSink->Stop(true);
#endif
			mpStreamAbstractionAAMP->GetStreamFormat(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat);
			mStreamSink->Configure(
				mVideoFormat,
				mAudioFormat,
				mAuxFormat,
				mSubtitleFormat,
				mpStreamAbstractionAAMP->GetESChangeStatus(),
				mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus(),
				mIsTrackIdMismatch /*setReadyAfterPipelineCreation*/);
			mpStreamAbstractionAAMP->ResetESChangeStatus();
			mpStreamAbstractionAAMP->StartInjection();
			mStreamSink->Stream();
			mIsTrackIdMismatch = false;			
		}
		else
		{
			ret = false;
			AAMPLOG_WARN("PrivateInstanceAAMP: mDiscontinuityTuneOperationInProgress was reset during operation, since other command received from app!");
		}
	}

	if (ret)
	{
		SyncBegin();
		mDiscontinuityTuneOperationInProgress = false;
		SyncEnd();
	}

	UnblockWaitForDiscontinuityProcessToComplete();
	return ret;
}

/**
 * @brief Get the Current Audio Track Id 
 * Currently it is implimented for AC4 track selection only
 * @return int return the index number of current audio track selected
 */ 
int PrivateInstanceAAMP::GetCurrentAudioTrackId()
{
	int trackId = -1;
	AudioTrackInfo currentAudioTrack;

	/** Only select track Id for setting gstplayer in case of muxed ac4 stream */
	if (mpStreamAbstractionAAMP->GetCurrentAudioTrack(currentAudioTrack) && (currentAudioTrack.codec.find("ac4") == std::string::npos) && currentAudioTrack.isMuxed )
	{
		AAMPLOG_INFO("Found AC4 track as current Audio track  index = %s language - %s role - %s codec %s type %s bandwidth = %ld",
		currentAudioTrack.index.c_str(), currentAudioTrack.language.c_str(), currentAudioTrack.rendition.c_str(), 
		currentAudioTrack.codec.c_str(), currentAudioTrack.contentType.c_str(), currentAudioTrack.bandwidth);
		trackId = std::stoi( currentAudioTrack.index );

	}

	return trackId;
}

/**
 * @brief Process EOS from Sink and notify listeners if required
 */
void PrivateInstanceAAMP::NotifyEOSReached()
{
	bool isDiscontinuity = IsDiscontinuityProcessPending();
	bool isLive = IsLive();
	
	AAMPLOG_WARN("Enter . processingDiscontinuity %d isLive %d", isDiscontinuity, isLive);
	
	if (!isDiscontinuity)
	{
		/*
		A temporary work around intended to reduce occurrences of LLAMA-6113.
		This appears to be caused by late calls to previously stopped/destroyed objects due to a scheduling issue.
		In this case it makes sense to exit this function ASAP.
		A more complete (larger, higher risk, more time consuming, threadsafe) change to scheduling is required in the future.
		*/
		if(!mpStreamAbstractionAAMP)
		{
			AAMPLOG_ERR("null Stream Abstraction AAMP");
			return;
		}

		if (!mpStreamAbstractionAAMP->IsEOSReached())
		{
			AAMPLOG_ERR("Bogus EOS event received from GStreamer, discarding it!");
			return;
		}
		if (!isLive && rate > 0)
		{
			SetState(eSTATE_COMPLETE);
			SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS),AAMP_EVENT_ASYNC_MODE);
			if (ContentType_EAS == mContentType) //Fix for DELIA-25590
			{
				mStreamSink->Stop(false);
			}
			SendAnomalyEvent(ANOMALY_TRACE, "Generating EOS event");
			trickStartUTCMS = -1;
			return;
		}

		if (rate < 0)
		{
			seek_pos_seconds = culledSeconds;
			AAMPLOG_WARN("Updated seek_pos_seconds %f on BOS", seek_pos_seconds);
			if (trickStartUTCMS == -1)
			{
				// Resetting trickStartUTCMS if it's default due to no first frame on high speed rewind. This enables ReportProgress to 
				// send BOS event to JSPP
				ResetTrickStartUTCTime();
				AAMPLOG_INFO("Resetting trickStartUTCMS to %lld since no first frame on trick play rate %d", trickStartUTCMS, rate);
			}
			// A new report progress event to be emitted with position 0 when rewind reaches BOS
			ReportProgress(true, true);
			rate = AAMP_NORMAL_PLAY_RATE;
			AcquireStreamLock();
			TuneHelper(eTUNETYPE_SEEK);
			ReleaseStreamLock();
		}
		else
		{
			rate = AAMP_NORMAL_PLAY_RATE;
			AcquireStreamLock();
			TuneHelper(eTUNETYPE_SEEKTOLIVE);
			ReleaseStreamLock();
		}

		NotifySpeedChanged(rate);
	}
	else
	{
		ProcessPendingDiscontinuity();
		pthread_cond_signal(&mCondDiscontinuity);
		DeliverAdEvents();
		AAMPLOG_WARN("PrivateInstanceAAMP:  EOS due to discontinuity handled");
	}
}

/**
 * @brief Notify when entering live point to listeners
 */
void PrivateInstanceAAMP::NotifyOnEnteringLive()
{
	if (discardEnteringLiveEvt)
	{
		return;
	}
	SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_ENTERING_LIVE),AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief track description string from TrackType enum
 */
static std::string TrackTypeString(const int track)
{
	switch(track)
	{
		case eTRACK_VIDEO:
			return "Video";
		case eTRACK_AUDIO:
			return "Audio";
		case eTRACK_SUBTITLE:
			return "Subtitle";
		case eTRACK_AUX_AUDIO:
			return "Aux Audio";
	}
	return "unknown track type";
}

/**
* @brief Additional Tune Fail Diagnostics
 */
void PrivateInstanceAAMP::AdditionalTuneFailLogEntries()
{
	mStreamSink->DumpStatus();

	{
		std::string downloadsBlockedMessage = "Downloads";
		if (!mbDownloadsBlocked)
		{
			downloadsBlockedMessage += " not";
		}
		downloadsBlockedMessage += " blocked, track download status: ";
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			downloadsBlockedMessage+=TrackTypeString(i);
			if (!mbTrackDownloadsBlocked[i])
			{
				downloadsBlockedMessage += " not";
			}
			downloadsBlockedMessage += " blocked, ";
		}
		AAMPLOG_WARN(downloadsBlockedMessage.c_str());
	}

	{
		std::string injectionBlockedMessage = "Track injection status: ";
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			injectionBlockedMessage+=TrackTypeString(i);
			if (!mTrackInjectionBlocked[i])
			{
				injectionBlockedMessage += " not";
			}
			injectionBlockedMessage += " blocked, ";
		}
		AAMPLOG_WARN(injectionBlockedMessage.c_str());
	}
	
	if(mpStreamAbstractionAAMP)
	{
		std::string trackBufferStatusMessage = "Track buffer status: ";
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			trackBufferStatusMessage+=TrackTypeString(i);
			auto track = mpStreamAbstractionAAMP->GetMediaTrack(static_cast<TrackType>(i));
			if(nullptr != track)
			{
				const auto status = track->GetBufferStatus();
				trackBufferStatusMessage += " ";
				switch (status)
				{
					case BUFFER_STATUS_GREEN:
						trackBufferStatusMessage += "green";
						break;
					case BUFFER_STATUS_YELLOW:
						trackBufferStatusMessage += "yellow";
						break;
					case BUFFER_STATUS_RED:
						trackBufferStatusMessage += "red";
						break;
					default:
						trackBufferStatusMessage += "unknown";
						break;
				}
			}
			else
			{
				trackBufferStatusMessage += "invalid track";
			}
			trackBufferStatusMessage += ", ";
		}
		AAMPLOG_WARN(trackBufferStatusMessage.c_str());
	}
}

/**
 * @brief Profiler for failure tune
 */
void PrivateInstanceAAMP::TuneFail(bool fail)
{
	PrivAAMPState state;
	GetState(state);
	TuneEndMetrics mTuneMetrics = {0, 0, 0,0,0,0,0,0,0,(ContentType)0};	
	mTuneMetrics.mTotalTime                 = NOW_STEADY_TS_MS ;
	mTuneMetrics.success         	 	= ((state != eSTATE_ERROR) ? -1 : !fail);
	int streamType 				= getStreamType();
	mTuneMetrics.mFirstTune			= mFirstTune;
	mTuneMetrics.mTimedMetadata 	 	= timedMetadata.size();
	mTuneMetrics.mTimedMetadataStartTime 	= mTimedMetadataStartTime;
	mTuneMetrics.mTimedMetadataDuration  	= mTimedMetadataDuration;
	mTuneMetrics.mTuneAttempts 		= mTuneAttempts;
	mTuneMetrics.contentType 		= mContentType;
	mTuneMetrics.streamType 		= streamType;
	mTuneMetrics.mTSBEnabled             	= mTSBEnabled;
	if(mTuneMetrics.success  == -1 && mPlayerPreBuffered)
	{
		LogPlayerPreBuffered();        //Need to calculate prebufferedtime when tune interruption happens with playerprebuffer
	}
	profiler.TuneEnd(mTuneMetrics, mAppName,(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mPlayerPreBuffered, durationSeconds, activeInterfaceWifi,mFailureReason);
	AdditionalTuneFailLogEntries();
}

/**
 *  @brief Notify tune end for profiling/logging
 */
void PrivateInstanceAAMP::LogTuneComplete(void)
{
	TuneEndMetrics mTuneMetrics = {0, 0, 0,0,0,0,0,0,0,(ContentType)0};
	
	mTuneMetrics.success 		 	 = true; 
	int streamType 				 = getStreamType();
	mTuneMetrics.contentType 		 = mContentType;
	mTuneMetrics.mTimedMetadata 	 	 = timedMetadata.size();
	mTuneMetrics.mTimedMetadataStartTime 	 = mTimedMetadataStartTime;
	mTuneMetrics.mTimedMetadataDuration      = mTimedMetadataDuration;
	mTuneMetrics.mTuneAttempts 		 = mTuneAttempts;
	mTuneMetrics.streamType 		 = streamType;
	mTuneMetrics.mTSBEnabled                 = mTSBEnabled;
	profiler.TuneEnd(mTuneMetrics,mAppName,(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mPlayerPreBuffered, durationSeconds, activeInterfaceWifi,mFailureReason);
	//update tunedManifestUrl if FOG was NOT used as manifestUrl might be updated with redirected url.
	if(!IsTSBSupported())
	{
		SetTunedManifestUrl(); /* Redirect URL in case on VOD */
	}

	if (!mTuneCompleted)
	{
		if(mLogTune)
		{
			char classicTuneStr[AAMP_MAX_PIPE_DATA_SIZE];
			mLogTune = false;
			if (ISCONFIGSET_PRIV(eAAMPConfig_XRESupportedTune)) { 
				profiler.GetClassicTuneTimeInfo(mTuneMetrics.success, mTuneAttempts, mfirstTuneFmt, mPlayerLoadTime, streamType, IsLive(), durationSeconds, classicTuneStr);
				SendMessage2Receiver(E_AAMP2Receiver_TUNETIME,classicTuneStr);
			}
			mTuneCompleted = true;
			mFirstTune = false;
		}
		AAMPAnomalyMessageType eMsgType = AAMPAnomalyMessageType::ANOMALY_TRACE;
		if(mTuneAttempts > 1 )
		{
		    eMsgType = AAMPAnomalyMessageType::ANOMALY_WARNING;
		}
		std::string playbackType = GetContentTypString();

		if(mContentType == ContentType_LINEAR)
		{
			if(mTSBEnabled)
			{
				playbackType.append(":TSB=true");
			}
			else
			{
				playbackType.append(":TSB=false");
			}
		}

		SendAnomalyEvent(eMsgType, "Tune attempt#%d. %s:%s URL:%s", mTuneAttempts,playbackType.c_str(),getStreamTypeString().c_str(),GetTunedManifestUrl());
	}
	
	mConfig->logging.setLogLevel(eLOGLEVEL_WARN);
	gpGlobalConfig->logging.setLogLevel(eLOGLEVEL_WARN);	
}

/**
 *  @brief Notifies profiler that first frame is presented
 */
void PrivateInstanceAAMP::LogFirstFrame(void)
{
	profiler.ProfilePerformed(PROFILE_BUCKET_FIRST_FRAME);
}

/**
 *  @brief Profile Player changed from background to foreground i.e prebuffred
 */
void PrivateInstanceAAMP::LogPlayerPreBuffered(void)
{
       profiler.ProfilePerformed(PROFILE_BUCKET_PLAYER_PRE_BUFFERED);
}

/**
 *   @brief Notifies profiler that drm initialization is complete 
 */
void PrivateInstanceAAMP::LogDrmInitComplete(void)
{
	profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
}

/**
 *   @brief Notifies profiler that decryption has started
 */
void PrivateInstanceAAMP::LogDrmDecryptBegin(ProfilerBucketType bucketType)
{
	profiler.ProfileBegin(bucketType);
}

/**
 *   @brief Notifies profiler that decryption has ended
 */
void PrivateInstanceAAMP::LogDrmDecryptEnd(ProfilerBucketType bucketType)
{
	profiler.ProfileEnd(bucketType);
}

/**
 * @brief Stop downloads of all tracks.
 * Used by aamp internally to manage states
 */
void PrivateInstanceAAMP::StopDownloads()
{
	AAMPLOG_TRACE ("PrivateInstanceAAMP");
	if (!mbDownloadsBlocked)
	{
		pthread_mutex_lock(&mLock);
		mbDownloadsBlocked = true;
		pthread_mutex_unlock(&mLock);
	}
}

/**
 * @brief Resume downloads of all tracks.
 * Used by aamp internally to manage states
 */
void PrivateInstanceAAMP::ResumeDownloads()
{
	AAMPLOG_TRACE ("PrivateInstanceAAMP");
	if (mbDownloadsBlocked)
	{
		pthread_mutex_lock(&mLock);
		mbDownloadsBlocked = false;
		//log_current_time("gstreamer-needs-data");
		pthread_mutex_unlock(&mLock);
	}
}

/**
 * @brief Stop downloads for a track.
 * Called from StreamSink to control flow
 */
void PrivateInstanceAAMP::StopTrackDownloads(MediaType type)
{ // called from gstreamer main event loop
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		AAMPLOG_WARN ("PrivateInstanceAAMP: Enter. type = %d", (int) type);
	}
#endif
	if (!mbTrackDownloadsBlocked[type])
	{
		AAMPLOG_TRACE("gstreamer-enough-data from source[%d]", type);
		pthread_mutex_lock(&mLock);
		mbTrackDownloadsBlocked[type] = true;
		pthread_mutex_unlock(&mLock);
		NotifySinkBufferFull(type);
	}
	AAMPLOG_TRACE ("PrivateInstanceAAMP:: Enter. type = %d",  (int) type);
}

/**
 * @brief Resume downloads for a track.
 * Called from StreamSink to control flow
 */
void PrivateInstanceAAMP::ResumeTrackDownloads(MediaType type)
{ // called from gstreamer main event loop
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		AAMPLOG_WARN ("PrivateInstanceAAMP: Enter. type = %d", (int) type);
	}
#endif
	if (mbTrackDownloadsBlocked[type])
	{
		AAMPLOG_TRACE("gstreamer-needs-data from source[%d]", type);
		pthread_mutex_lock(&mLock);
		mbTrackDownloadsBlocked[type] = false;
		//log_current_time("gstreamer-needs-data");
		pthread_mutex_unlock(&mLock);
	}
	AAMPLOG_TRACE ("PrivateInstanceAAMP::Exit. type = %d",  (int) type);
}

/**
 *  @brief Block the injector thread until the gstreanmer needs buffer/more data.
 */
void PrivateInstanceAAMP::BlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track)
{ // called from FragmentCollector thread; blocks until gstreamer wants data
	AAMPLOG_TRACE("PrivateInstanceAAMP::Enter. type = %d and downloads:%d",  track, mbTrackDownloadsBlocked[track]);
	int elapsedMs = 0;
	while (mbDownloadsBlocked || mbTrackDownloadsBlocked[track])
	{
		if (!mDownloadsEnabled || mTrackInjectionBlocked[track])
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: interrupted. mDownloadsEnabled:%d mTrackInjectionBlocked:%d", mDownloadsEnabled, mTrackInjectionBlocked[track]);
			break;
		}
		if (cb && periodMs)
		{ // support for background tasks, i.e. refreshing manifest while gstreamer doesn't need additional data
			if (elapsedMs >= periodMs)
			{
				cb();
				elapsedMs -= periodMs;
			}
			elapsedMs += 10;
		}
		InterruptableMsSleep(10);
	}
	AAMPLOG_TRACE("PrivateInstanceAAMP::Exit. type = %d",  track);
}

/**
 * @brief Curl initialization function
 */
void PrivateInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName)
{
	int instanceEnd = startIdx + instanceCount;
	std::string UserAgentString;
	UserAgentString=mConfig->GetUserAgentString();
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	CurlStore::GetCurlStoreInstance(this)->CurlInit(this, startIdx, instanceCount, proxyName);
}

/**
 * @brief Storing audio language list
 */
void PrivateInstanceAAMP::StoreLanguageList(const std::set<std::string> &langlist)
{
	// store the language list
	int langCount = langlist.size();
	if (langCount > MAX_LANGUAGE_COUNT)
	{
		langCount = MAX_LANGUAGE_COUNT; //boundary check
	}
	mMaxLanguageCount = langCount;
	std::set<std::string>::const_iterator iter = langlist.begin();
	for (int cnt = 0; cnt < langCount; cnt++, iter++)
	{
		strncpy(mLanguageList[cnt], iter->c_str(), MAX_LANGUAGE_TAG_LENGTH);
		mLanguageList[cnt][MAX_LANGUAGE_TAG_LENGTH-1] = 0;
#ifdef SESSION_STATS
		if( this->mVideoEnd )
		{
			mVideoEnd->Setlanguage(VideoStatTrackType::STAT_AUDIO, (*iter), cnt+1);
		}
#endif
	}
}

/**
 * @brief Checking whether audio language supported
 */
bool PrivateInstanceAAMP::IsAudioLanguageSupported (const char *checkLanguage)
{
	bool retVal =false;
	for (int cnt=0; cnt < mMaxLanguageCount; cnt ++)
	{
		if(strncmp(mLanguageList[cnt], checkLanguage, MAX_LANGUAGE_TAG_LENGTH) == 0)
		{
			retVal = true;
			break;
		}
	}

	if(mMaxLanguageCount == 0)
	{
                AAMPLOG_WARN("IsAudioLanguageSupported No Audio language stored !!!");
	}
	else if(!retVal)
	{
		AAMPLOG_WARN("IsAudioLanguageSupported lang[%s] not available in list",checkLanguage);
	}
	return retVal;
}

/**
 * @brief Set curl timeout(CURLOPT_TIMEOUT)
 */
void PrivateInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
	if(ContentType_EAS == mContentType)
		return;
	if(instance < eCURLINSTANCE_MAX && curl[instance])
	{
		CURL_EASY_SETOPT(curl[instance], CURLOPT_TIMEOUT_MS, timeoutMS);
		curlDLTimeout[instance] = timeoutMS;
	}
	else
	{
		AAMPLOG_WARN("Failed to update timeout for curl instace %d",instance);
	}
}

/**
 * @brief Terminate curl contexts
 */
void PrivateInstanceAAMP::CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount)
{
	int instanceEnd = startIdx + instanceCount;
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	if (ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && \
		( startIdx == eCURLINSTANCE_VIDEO ) && (eCURLINSTANCE_AUX_AUDIO < instanceEnd) )
	{
		for(int i=0; i<eCURLINSTANCE_MAX;++i)
		{
			if(curlhost[i]->curl)
			{
				CurlStore::GetCurlStoreInstance(this)->CurlTerm(this, (AampCurlInstance)i, 1, curlhost[i]->hostname);
			}
			curlhost[i]->isRemotehost=true;
			curlhost[i]->redirect=true;
		}
	}

	CurlStore::GetCurlStoreInstance(this)->CurlTerm(this, startIdx, instanceCount);
}

/**
 * @brief GetPlaylistCurlInstance -  Function to return the curl instance for playlist download
 * Considers parallel download to decide the curl instance 
 * @return AampCurlInstance - curl instance for download
 */
AampCurlInstance PrivateInstanceAAMP::GetPlaylistCurlInstance(MediaType type, bool isInitialDownload)
{
	AampCurlInstance retType = eCURLINSTANCE_MANIFEST_MAIN;
	bool indivCurlInstanceFlag = false;

	// Removed condition check to get config value of parrallel playlist download, Now by default select parrallel playlist for non init downloads
	indivCurlInstanceFlag = isInitialDownload ? false : true;
	if(indivCurlInstanceFlag)
	{
		switch(type)
		{
			case eMEDIATYPE_PLAYLIST_VIDEO:
			case eMEDIATYPE_PLAYLIST_IFRAME:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
				break;
			case eMEDIATYPE_PLAYLIST_AUDIO:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_AUDIO;
				break;
			case eMEDIATYPE_PLAYLIST_SUBTITLE:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_SUBTITLE;
				break;
			case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_AUX_AUDIO;
				break;
			default:
				break;
		}
	}
	return retType;
}

/**
 * @brief Reset bandwidth value
 * Artificially resetting the bandwidth. Low for quicker tune times
 */
void PrivateInstanceAAMP::ResetCurrentlyAvailableBandwidth(long bitsPerSecond , bool trickPlay,int profile)
{
	pthread_mutex_lock(&mLock);
	if (mAbrBitrateData.size())
	{
		mAbrBitrateData.erase(mAbrBitrateData.begin(),mAbrBitrateData.end());
	}
	pthread_mutex_unlock(&mLock);
}

/**
 * @brief Get the current network bandwidth
 * using most recently recorded 3 samples
 * @return Available bandwidth in bps
 */
long PrivateInstanceAAMP::GetCurrentlyAvailableBandwidth(void)
{
	// 1. Check for any old bitrate beyond threshold time . remove those before calculation
	// 2. Sort and get median 
	// 3. if any outliers  , remove those entries based on a threshold value.
	// 4. Get the average of remaining data. 
	// 5. if no item in the list , return -1 . Caller to ignore bandwidth based processing
	
	std::vector< long> tmpData;
	long ret = -1;
	pthread_mutex_lock(&mLock);
	mhAbrManager.UpdateABRBitrateDataBasedOnCacheLife(mAbrBitrateData,tmpData);
	pthread_mutex_unlock(&mLock);
		
		if (tmpData.size())
		{
			//AAMPLOG_WARN("NwBW with newlogic size[%d] avg[%ld] ",tmpData.size(), avg/tmpData.size());
			ret =mhAbrManager.UpdateABRBitrateDataBasedOnCacheOutlier(tmpData);
			mAvailableBandwidth = ret;
			//Store the PersistBandwidth and UpdatedTime on ABRManager
			//Bitrate Update only for foreground player
			if(ISCONFIGSET_PRIV(eAAMPConfig_PersistLowNetworkBandwidth)||ISCONFIGSET_PRIV(eAAMPConfig_PersistHighNetworkBandwidth))
			{
				if(mAvailableBandwidth  > 0 && mbPlayEnabled)
				{
					ABRManager::setPersistBandwidth(mAvailableBandwidth );
					ABRManager::mPersistBandwidthUpdatedTime = aamp_GetCurrentTimeMS();
				}
			}
		}
		else
		{
			//AAMPLOG_WARN("No prior data available for abr , return -1 ");
			ret = -1;
		}
	
	return ret;
}

/**
 * @brief get Media Type in string
 */ 
const char* PrivateInstanceAAMP::MediaTypeString(MediaType fileType)
{
	switch(fileType)
	{
		case eMEDIATYPE_VIDEO:
		case eMEDIATYPE_INIT_VIDEO:
			return "VIDEO";
		case eMEDIATYPE_AUDIO:
		case eMEDIATYPE_INIT_AUDIO:
			return "AUDIO";
		case eMEDIATYPE_SUBTITLE:
		case eMEDIATYPE_INIT_SUBTITLE:
			return "SUBTITLE";
		case eMEDIATYPE_AUX_AUDIO:
		case eMEDIATYPE_INIT_AUX_AUDIO:
			return "AUX-AUDIO";
		case eMEDIATYPE_MANIFEST:
			return "MANIFEST";
		case eMEDIATYPE_LICENCE:
			return "LICENCE";
		case eMEDIATYPE_IFRAME:
			return "IFRAME";
		case eMEDIATYPE_PLAYLIST_VIDEO:
			return "PLAYLIST_VIDEO";
		case eMEDIATYPE_PLAYLIST_AUDIO:
			return "PLAYLIST_AUDIO";
		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			return "PLAYLIST_SUBTITLE";
		case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			return "PLAYLIST_AUX-AUDIO";
		default:
			return "Unknown";
	}
}

/**
 * @brief Download a file from the server
 */
bool PrivateInstanceAAMP::GetNetworkTime(enum UtcTiming timingType, const std::string& remoteUrl, long *http_error, CurlRequest request)
{
    bool ret = false;

    CURLcode res;
    long httpCode = -1;

    if(eCURL_GET != request)
        return ret;

    CURL *curl = curl_easy_init();
    if(curl)
    {
        AAMPLOG_TRACE("%s, %d", remoteUrl.c_str(), request);
        GrowableBuffer buffer = {0,};

        CurlCbContextSyncTime context(this, &buffer);

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SyncTime_write_callback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_CURL_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if(!ISCONFIGSET_PRIV(eAAMPConfig_SslVerifyPeer)){
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        }
	else {
	    curl_easy_setopt(curl, CURLOPT_SSLVERSION, mSupportedTLSVersion);
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	}

        curl_easy_setopt(curl, CURLOPT_URL, remoteUrl.c_str());

        res = curl_easy_perform(curl);
        if (res == CURLE_OK)
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if ((httpCode == 204) || (httpCode == 200))
            {
                if(buffer.len)
                {
                    time_t currentTime;
                    currentTime     = time(0);
                    struct tm *localTime;
                    localTime       = localtime(&currentTime);
                    struct tm *stGMT;
                    stGMT           = gmtime(&currentTime);
                    time_t currentTimeGMT;
                    currentTimeGMT  = mktime(stGMT);

                    //2021-06-15T18:11:39Z - UTC Zulu
                    //2021-06-15T19:03:48.795Z - <ProducerReferenceTime> WallClk UTC Zulu
                    const char* format = "%Y-%m-%dT%H:%M:%SZ";
                    mTime = convertTimeToEpoch((const char*)buffer.ptr, format);
                    AAMPLOG_WARN("ProducerReferenceTime Wallclock (Epoch): [%ld]", mTime);

                    aamp_Free(&buffer);

                    ret = true;
                }
            }
            else
            {
                AAMPLOG_ERR(" Returned [%d]", httpCode);
            }
        }
        else
        {
            AAMPLOG_ERR("Failed to perform curl request, result:%d", res);
        }

        if(httpCode > 0)
        {
            *http_error = httpCode;
        }

        curl_easy_cleanup(curl);
    }

    return ret;
}

/**
 * @brief Download a file from the CDN
 */
bool PrivateInstanceAAMP::GetFile(std::string remoteUrl,struct GrowableBuffer *buffer, std::string& effectiveUrl,
				long * http_error, double *downloadTime, const char *range, unsigned int curlInstance, 
				bool resetBuffer, MediaType fileType, long *bitrate, int * fogError,
				double fragmentDurationSeconds)
{
	MediaType simType = fileType; // remember the requested specific file type; fileType gets overridden later with simple VIDEO/AUDIO
	MediaTypeTelemetry mediaType = aamp_GetMediaTypeForTelemetry(fileType);
	std::unordered_map<std::string, std::vector<std::string>> mCMCDCustomHeaders;
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableCMCD))
	{
		BuildCMCDCustomHeaders(fileType,mCMCDCustomHeaders);
	}
	long http_code = -1;
	double fileDownloadTime = 0;
	bool ret = false;
	int downloadAttempt = 0;
	int maxDownloadAttempt = 1;
	CURL* curl = this->curl[curlInstance];
	struct curl_slist* httpHeaders = NULL;
	CURLcode res = CURLE_OK;
	int fragmentDurationMs = (int)(fragmentDurationSeconds*1000);/*convert to MS */
	if (simType == eMEDIATYPE_INIT_VIDEO || simType == eMEDIATYPE_INIT_AUDIO || simType == eMEDIATYPE_INIT_AUX_AUDIO)
	{
		int InitFragmentRetryCount;
		GETCONFIGVALUE_PRIV(eAAMPConfig_InitFragmentRetryCount,InitFragmentRetryCount); 
		maxDownloadAttempt += InitFragmentRetryCount;
	}
	else
	{
		maxDownloadAttempt += DEFAULT_DOWNLOAD_RETRY_COUNT;
	}

	if (resetBuffer)
	{
		if(buffer->avail)
        	{
            		AAMPLOG_TRACE("reset buffer %p avail %d", buffer, (int)buffer->avail);
        	}	
		memset(buffer, 0x00, sizeof(*buffer));
	}
	if (mDownloadsEnabled)
	{
		int downloadTimeMS = 0;
		bool isDownloadStalled = false;
		CurlAbortReason abortReason = eCURL_ABORT_REASON_NONE;
		double connectTime = 0;
		pthread_mutex_unlock(&mLock);
		std::string uriParameter;
		GETCONFIGVALUE_PRIV(eAAMPConfig_URIParameter,uriParameter);
		// append custom uri parameter with remoteUrl at the end before curl request if curlHeader logging enabled.
		if (ISCONFIGSET_PRIV(eAAMPConfig_CurlHeader) && (!uriParameter.empty()) && simType == eMEDIATYPE_MANIFEST)
		{
			if (remoteUrl.find("?") == std::string::npos)
			{
				uriParameter[0] = '?';
			}

			remoteUrl.append(uriParameter.c_str());
			//printf ("URL after appending uriParameter :: %s\n", remoteUrl.c_str());
		}

		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && mOrigManifestUrl.isRemotehost )
		{
			if( curlhost[curlInstance]->isRemotehost && curlhost[curlInstance]->redirect &&
				( NULL == curlhost[curlInstance]->curl || std::string::npos == remoteUrl.find(curlhost[curlInstance]->hostname)) )
			{
				if(NULL != curlhost[curlInstance]->curl)
				{
					CurlStore::GetCurlStoreInstance(this)->CurlTerm(this, (AampCurlInstance)curlInstance, 1, curlhost[curlInstance]->hostname);
				}

				curlhost[curlInstance]->hostname = aamp_getHostFromURL(remoteUrl);
				curlhost[curlInstance]->isRemotehost =!(aamp_IsLocalHost(curlhost[curlInstance]->hostname));
				curlhost[curlInstance]->redirect = false;

				if( curlhost[curlInstance]->isRemotehost && (std::string::npos == mOrigManifestUrl.hostname.find(curlhost[curlInstance]->hostname)) )
				{
					CurlStore::GetCurlStoreInstance(this)->CurlInit(this, (AampCurlInstance)curlInstance, 1, GetNetworkProxy(), curlhost[curlInstance]->hostname);
					CURL_EASY_SETOPT(curlhost[curlInstance]->curl, CURLOPT_TIMEOUT_MS, curlDLTimeout[curlInstance]);
				}
			}

			if ( curlhost[curlInstance]->curl )
			curl=curlhost[curlInstance]->curl;
		}

		AAMPLOG_INFO("aamp url:%d,%d,%d,%f,%s", mediaType, simType, curlInstance,fragmentDurationSeconds, remoteUrl.c_str());
		CurlCallbackContext context;
		if (curl)
		{
			CURL_EASY_SETOPT(curl, CURLOPT_URL, remoteUrl.c_str());
                        if(this->mAampLLDashServiceData.lowLatencyMode)
			{
				CURL_EASY_SETOPT(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
				context.remoteUrl = remoteUrl;
			}
			context.aamp = this;
			context.buffer = buffer;
			context.responseHeaderData = &httpRespHeaders[curlInstance];
			context.fileType = simType;
			
			if(!this->mAampLLDashServiceData.lowLatencyMode)
			{
				CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, &context);
				CURL_EASY_SETOPT(curl, CURLOPT_HEADERDATA, &context);
			}
			if(!ISCONFIGSET_PRIV(eAAMPConfig_SslVerifyPeer))
			{
				CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYHOST, 0L);
				CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			}
			else
			{
				CURL_EASY_SETOPT(curl, CURLOPT_SSLVERSION, mSupportedTLSVersion);
				CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 1L);
			}

			CurlProgressCbContext progressCtx;
			progressCtx.aamp = this;
			progressCtx.fileType = simType;
			progressCtx.dlStarted = true;
			progressCtx.fragmentDurationMs = fragmentDurationMs;

			if(this->mAampLLDashServiceData.lowLatencyMode &&
			(simType == eMEDIATYPE_VIDEO))
			{
				progressCtx.remoteUrl = remoteUrl;
			}

			//Disable download stall detection checks for FOG playback done by JS PP
			if(simType == eMEDIATYPE_MANIFEST || simType == eMEDIATYPE_PLAYLIST_VIDEO ||
				simType == eMEDIATYPE_PLAYLIST_AUDIO || simType == eMEDIATYPE_PLAYLIST_SUBTITLE ||
				simType == eMEDIATYPE_PLAYLIST_IFRAME || simType == eMEDIATYPE_PLAYLIST_AUX_AUDIO)
			{
				// For Manifest file : Set starttimeout to 0 ( no wait for first byte). Playlist/Manifest with DAI
				// contents take more time , hence to avoid frequent timeout, its set as 0
				progressCtx.startTimeout = 0;
			}
			else
			{
				// for Video/Audio segments , set the start timeout as configured by Application
				GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadStartTimeout,progressCtx.startTimeout);
				GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadLowBWTimeout,progressCtx.lowBWTimeout);
			}
			GETCONFIGVALUE_PRIV(eAAMPConfig_CurlStallTimeout,progressCtx.stallTimeout);

			// note: win32 curl lib doesn't support multi-part range
			CURL_EASY_SETOPT(curl, CURLOPT_RANGE, range);

			if ((httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_COOKIE) && (httpRespHeaders[curlInstance].data.length() > 0))
			{
				AAMPLOG_TRACE("Appending cookie headers to HTTP request");
				//curl_easy_setopt(curl, CURLOPT_COOKIE, cookieHeaders[curlInstance].c_str());
				CURL_EASY_SETOPT(curl, CURLOPT_COOKIE, httpRespHeaders[curlInstance].data.c_str());
			}
			if ( ISCONFIGSET_PRIV(eAAMPConfig_EnableCMCD) && mCMCDCustomHeaders.size() > 0)
			{
				std::string customHeader;
				std::string headerValue;
				for (std::unordered_map<std::string, std::vector<std::string>>::iterator it = mCMCDCustomHeaders.begin();it != mCMCDCustomHeaders.end(); it++)
				{
					customHeader.clear();
					headerValue.clear();
					customHeader.insert(0, it->first);
					customHeader.push_back(' ');
					if (it->first.compare("CMCD-Session:") == 0)
					{
						headerValue = it->second.at(0);
					}
					if (it->first.compare("CMCD-Object:") == 0)
					{
						headerValue = it->second.at(0);
					}
					if (it->first.compare("CMCD-Request:") == 0)
					{
						headerValue = it->second.at(0);
					}
					if (it->first.compare("CMCD-Status:") == 0)
					{
						headerValue = it->second.at(0);
					}
					customHeader.append(headerValue);
					httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
				}
				if (httpHeaders != NULL)
				{
					CURL_EASY_SETOPT(curl, CURLOPT_HTTPHEADER, httpHeaders);
				}

			}
			if (mCustomHeaders.size() > 0)
			{
				std::string customHeader;
				std::string headerValue;
				for (std::unordered_map<std::string, std::vector<std::string>>::iterator it = mCustomHeaders.begin();
									it != mCustomHeaders.end(); it++)
				{
					customHeader.clear();
					headerValue.clear();
					customHeader.insert(0, it->first);
					customHeader.push_back(' ');
					headerValue = it->second.at(0);
					if (it->first.compare("X-MoneyTrace:") == 0)
					{
						if (mTSBEnabled && !mIsFirstRequestToFOG)
						{
							continue;
						}
						char buf[512];
						memset(buf, '\0', 512);
						if (it->second.size() >= 2)
						{
							snprintf(buf, 512, "trace-id=%s;parent-id=%s;span-id=%lld",
									(const char*)it->second.at(0).c_str(),
									(const char*)it->second.at(1).c_str(),
									aamp_GetCurrentTimeMS());
						}
						else if (it->second.size() == 1)
						{
							snprintf(buf, 512, "trace-id=%s;parent-id=%lld;span-id=%lld",
									(const char*)it->second.at(0).c_str(),
									aamp_GetCurrentTimeMS(),
									aamp_GetCurrentTimeMS());
						}
						headerValue = buf;
					}
					if (it->first.compare("Wifi:") == 0)
					{
						if (true == activeInterfaceWifi)
						{
							headerValue = "1";
						}
						else
						{
							 headerValue = "0";
						}
					}
					customHeader.append(headerValue);
					httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
				}
				if (ISCONFIGSET_PRIV(eAAMPConfig_LimitResolution) && mIsFirstRequestToFOG && mTSBEnabled && eMEDIATYPE_MANIFEST == simType)
				{
					std::string customHeader;
					customHeader.clear();
					customHeader = "width: " +  std::to_string(mDisplayWidth);
					httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
					customHeader.clear();
					customHeader = "height: " + std::to_string(mDisplayHeight);
					httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
				}

				if (ISCONFIGSET_PRIV(eAAMPConfig_CurlHeader) && (eMEDIATYPE_VIDEO == simType || eMEDIATYPE_PLAYLIST_VIDEO == simType))
				{
					std::string customheaderstr;
					GETCONFIGVALUE_PRIV(eAAMPConfig_CustomHeader,customheaderstr);					
					if(!customheaderstr.empty())
					{				
						//AAMPLOG_WARN ("Custom Header Data: Index( %d ) Data( %s )", i, &customheaderstr.at(i));
						httpHeaders = curl_slist_append(httpHeaders, customheaderstr.c_str());					
					}
				}

				if (mIsFirstRequestToFOG && mTSBEnabled && eMEDIATYPE_MANIFEST == simType)
				{
					std::string customHeader = "4k: 1";
					if (ISCONFIGSET_PRIV(eAAMPConfig_Disable4K))
					{
						customHeader = "4k: 0";
					}                                        
                                        httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
				}

				if (httpHeaders != NULL)
				{
					CURL_EASY_SETOPT(curl, CURLOPT_HTTPHEADER, httpHeaders);
				}
			}

			while(downloadAttempt < maxDownloadAttempt)
			{
				progressCtx.downloadStartTime = NOW_STEADY_TS_MS;
								
				if(this->mAampLLDashServiceData.lowLatencyMode)
				{
					context.downloadStartTime = progressCtx.downloadStartTime;
					CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, &context);
					CURL_EASY_SETOPT(curl, CURLOPT_HEADERDATA, &context);
				}
				progressCtx.downloadUpdatedTime = -1;
				progressCtx.downloadSize = -1;
				progressCtx.abortReason = eCURL_ABORT_REASON_NONE;
				CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSDATA, &progressCtx);
				if(buffer->ptr != NULL)
				{
					AAMPLOG_TRACE(" reset length. buffer %p avail %d",  buffer, (int)buffer->avail);
					buffer->len = 0;
				}

				isDownloadStalled = false;
				abortReason = eCURL_ABORT_REASON_NONE;

				long long tStartTime = NOW_STEADY_TS_MS;
				CURLcode res = curl_easy_perform(curl); // synchronous; callbacks allow interruption

				if(!mAampLLDashServiceData.lowLatencyMode)
				{
					int insertDownloadDelay=0;
					GETCONFIGVALUE_PRIV(eAAMPConfig_DownloadDelay,insertDownloadDelay);
					/* optionally locally induce extra per-download latency */
					if( insertDownloadDelay > 0 )
					{
						InterruptableMsSleep( insertDownloadDelay );
					}
				}

				long long tEndTime = NOW_STEADY_TS_MS;
				downloadAttempt++;

				downloadTimeMS = (int)(tEndTime - tStartTime);
				bool loopAgain = false;
				if (res == CURLE_OK)
				{ // all data collected
					if( memcmp(remoteUrl.c_str(), "file:", 5) == 0 )
					{ // file uri scheme
						// libCurl does not provide CURLINFO_RESPONSE_CODE for 'file:' protocol.
						// Handle CURL_OK to http_code mapping here, other values handled below (see http_code = res).
						http_code = 200;
					}
					else
					{
						curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
					}
					char *effectiveUrlPtr = NULL;
					if (http_code != 200 && http_code != 204 && http_code != 206)
					{
						AAMP_LOG_NETWORK_ERROR (remoteUrl.c_str(), AAMPNetworkErrorHttp, (int)http_code, simType);
						print_headerResponse(context.allResponseHeadersForErrorLogging, simType);

						if((http_code >= 500 && http_code != 502) && downloadAttempt < maxDownloadAttempt)
						{
                                                        int waitTimeBeforeRetryHttp5xxMSValue;
                                                        GETCONFIGVALUE_PRIV(eAAMPConfig_Http5XXRetryWaitInterval,waitTimeBeforeRetryHttp5xxMSValue);
							InterruptableMsSleep(waitTimeBeforeRetryHttp5xxMSValue);
							AAMPLOG_WARN("Download failed due to Server error. Retrying Attempt:%d!", downloadAttempt);
							loopAgain = true;
						}
					}
					if(http_code == 204)
					{
						if ( (httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_EFF_LOCATION) && (httpRespHeaders[curlInstance].data.length() > 0) )
						{
							AAMPLOG_WARN("Received Location header: '%s'", httpRespHeaders[curlInstance].data.c_str());
							effectiveUrlPtr =  const_cast<char *>(httpRespHeaders[curlInstance].data.c_str());
						}
					}
					//When Fog is having tsb write error , then it will respond back with 302 with direct CDN url,In this case alone TSB should be disabled
					if(http_code == 302)
					{
						mTSBEnabled = false;
					}
					else
					{
						res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrlPtr);

						if(GetLLDashServiceData()->lowLatencyMode &&
						(simType == eMEDIATYPE_INIT_VIDEO || simType ==  eMEDIATYPE_INIT_AUDIO))
						{
							IsoBmffBuffer isobuf(mLogObj);
							isobuf.setBuffer(reinterpret_cast<uint8_t *>(context.buffer->ptr), context.buffer->len);

							bool bParse = false;
							try
							{
								bParse = isobuf.parseBuffer();
							}
							catch( std::bad_alloc& ba)
							{
								AAMPLOG_ERR("Bad allocation: %s", ba.what() );
							}
							catch( std::exception &e)
							{
								AAMPLOG_ERR("Unhandled exception: %s", e.what() );
							}
							catch( ... )
							{
								AAMPLOG_ERR("Unknown exception");
							}

							if(!bParse)
							{
								AAMPLOG_ERR("[%d] Cant Find TimeScale. No Box available in Init File !!!", simType);
							}
							else
							{
								AAMPLOG_INFO("[%d] Buffer Length: %d", simType, context.buffer->len);

								//Print box details
								//isobuf.printBoxes();
								if(isobuf.isInitSegment())
								{
									uint32_t timeScale = 0;
									isobuf.getTimeScale(timeScale);
									if(simType == eMEDIATYPE_INIT_VIDEO)
									{
										AAMPLOG_INFO("Video TimeScale  [%d]", timeScale);
										SetVidTimeScale(timeScale);
									}
									else
									{
										AAMPLOG_INFO("Audio TimeScale  [%d]", timeScale);
										SetAudTimeScale(timeScale);
									}
								}
								isobuf.destroyBoxes();
							}
						}
					}

					if(effectiveUrlPtr)
					{
						effectiveUrl.assign(effectiveUrlPtr);    //CID:81493 - Resolve Forward null

						if( ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && (remoteUrl!=effectiveUrl) )
						{
							curlhost[curlInstance]->redirect = true;
						}
					}

					// check if redirected url is pointing to fog / local ip
					if(mIsFirstRequestToFOG)
					{
						if(mTsbRecordingId.empty())
						{
							AAMPLOG_INFO("TSB not avaialble from fog, playing from:%s ", effectiveUrl.c_str());
						}
						this->UpdateVideoEndTsbStatus(mTSBEnabled);
					}

					/*
					 * Latency should be printed in the case of successful download which exceeds the download threshold value,
					 * other than this case is assumed as network error and those will be logged with AAMP_LOG_NETWORK_ERROR.
					 */
					if (fragmentDurationSeconds != 0.0)
					{ 
						/*in case of fetch fragment this will be non zero value */
						if (downloadTimeMS > fragmentDurationMs )
						{
							AAMP_LOG_NETWORK_LATENCY (effectiveUrl.c_str(), downloadTimeMS, fragmentDurationMs, simType);
						}
					}
					else if (downloadTimeMS > FRAGMENT_DOWNLOAD_WARNING_THRESHOLD )
					{
						AAMP_LOG_NETWORK_LATENCY (effectiveUrl.c_str(), downloadTimeMS, FRAGMENT_DOWNLOAD_WARNING_THRESHOLD, simType);
						print_headerResponse(context.allResponseHeadersForErrorLogging, simType);
					}
				}
				else
				{
					long curlDownloadTimeoutMS = curlDLTimeout[curlInstance]; // curlDLTimeout is in msec
					//abortReason for progress_callback exit scenarios
					// curl sometimes exceeds the wait time by few milliseconds.Added buffer of 10msec
					isDownloadStalled = ((res == CURLE_PARTIAL_FILE) || (progressCtx.abortReason != eCURL_ABORT_REASON_NONE));
					// set flag if download aborted with start/stall timeout.
					abortReason = progressCtx.abortReason;

					/* Curl 23 and 42 is not a real network error, so no need to log it here */
					//Log errors due to curl stall/start detection abort
					if (AAMP_IS_LOG_WORTHY_ERROR(res) || progressCtx.abortReason != eCURL_ABORT_REASON_NONE)
					{
						AAMP_LOG_NETWORK_ERROR (remoteUrl.c_str(), AAMPNetworkErrorCurl, (int)(progressCtx.abortReason == eCURL_ABORT_REASON_NONE ? res : CURLE_PARTIAL_FILE), simType);
						print_headerResponse(context.allResponseHeadersForErrorLogging, simType);
					}

					//Attempt retry for partial downloads, which have a higher chance to succeed
					if((res == CURLE_COULDNT_CONNECT || res == CURLE_OPERATION_TIMEDOUT || (isDownloadStalled && (eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT != abortReason))) && downloadAttempt < maxDownloadAttempt)
					{
						if(mpStreamAbstractionAAMP)
						{
							if( simType == eMEDIATYPE_MANIFEST ||
								simType == eMEDIATYPE_AUDIO ||
								simType == eMEDIATYPE_VIDEO ||
							    simType == eMEDIATYPE_INIT_VIDEO ||
							    simType == eMEDIATYPE_PLAYLIST_AUDIO ||
							    simType == eMEDIATYPE_INIT_AUDIO ||
								simType == eMEDIATYPE_AUX_AUDIO || simType == eMEDIATYPE_INIT_AUX_AUDIO)
							{ // always retry small, critical fragments on timeout
								loopAgain = true;
							}
							else
							{
								double buffervalue = mpStreamAbstractionAAMP->GetBufferedDuration();
								// buffer is -1 when sesssion not created . buffer is 0 when session created but playlist not downloaded
								if( buffervalue == -1.0 || buffervalue == 0 || buffervalue*1000 > (curlDownloadTimeoutMS + fragmentDurationMs))
								{
									// GetBuffer will return -1 if session is not created
									// Check if buffer is available and more than timeout interval then only reattempt
									// Not to retry download if there is no buffer left									
									loopAgain = true;
									if(simType == eMEDIATYPE_VIDEO)
									{
										if(buffer->len)
										{
											long downloadbps = ((long)(buffer->len / downloadTimeMS)*8000);
											long currentProfilebps	= mpStreamAbstractionAAMP->GetVideoBitrate();
											if(currentProfilebps - downloadbps >  BITRATE_ALLOWED_VARIATION_BAND)
												loopAgain = false;
										}
										curlDownloadTimeoutMS = mNetworkTimeoutMs;	
									}																		
								}
							}
						}						
						AAMPLOG_WARN("Download failed due to curl timeout or isDownloadStalled:%d Retrying:%d Attempt:%d", isDownloadStalled, loopAgain, downloadAttempt);
					}

					/*
					* Assigning curl error to http_code, for sending the error code as
					* part of error event if required
					* We can distinguish curl error and http error based on value
					* curl errors are below 100 and http error starts from 100
					*/
					if( res == CURLE_FILE_COULDNT_READ_FILE )
					{
						http_code = 404; // translate file not found to URL not found
					}
					else if(abortReason == eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT)
					{
						http_code = CURLE_OPERATION_TIMEDOUT; // Timed out wrt configured low bandwidth timeout.
					}
					else
					{
						http_code = res;
					}
					#if 0
					if (isDownloadStalled)
					{
						AAMPLOG_INFO("Curl download stall detected - curl result:%d abortReason:%d downloadTimeMS:%lld curlTimeout:%ld", res, progressCtx.abortReason,
									downloadTimeMS, curlDownloadTimeoutMS);
						//To avoid updateBasedonFragmentCached being called on rampdown and to be discarded from ABR
						http_code = CURLE_PARTIAL_FILE;
					}
					#endif
				}
				

				double total, connect, startTransfer, resolve, appConnect, preTransfer, redirect, dlSize;
				long reqSize, downloadbps = 0;
				AAMP_LogLevel reqEndLogLevel = eLOGLEVEL_INFO;
				if(downloadTimeMS != 0 && buffer->len != 0)
					downloadbps = ((long)(buffer->len / downloadTimeMS)*8000);

				curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME , &total);
				curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connect);
				connectTime = connect;
				fileDownloadTime = total;
				if(res != CURLE_OK || http_code == 0 || http_code >= 400 || total > 2.0 /*seconds*/)
				{
					reqEndLogLevel = eLOGLEVEL_WARN;
				}
				if (gpGlobalConfig->logging.isLogLevelAllowed(reqEndLogLevel))
				{
					double totalPerformRequest = (double)(downloadTimeMS)/1000;
					curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &resolve);
					curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &appConnect);
					curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME, &preTransfer);
					curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &startTransfer);
					curl_easy_getinfo(curl, CURLINFO_REDIRECT_TIME, &redirect);
					curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &dlSize);
					curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE, &reqSize);

					std::string appName, timeoutClass;
					if (!mAppName.empty())
					{
						// append app name with class data
						appName = mAppName + ",";
					}
					if (CURLE_OPERATION_TIMEDOUT == res || CURLE_PARTIAL_FILE == res || CURLE_COULDNT_CONNECT == res)
					{
						// introduce  extra marker for connection status curl 7/18/28,
						// example 18(0) if connection failure with PARTIAL_FILE code
						timeoutClass = "(" + to_string(reqSize > 0) + ")";
					}
					AAMPLOG(mLogObj, reqEndLogLevel, "WARN", "HttpRequestEnd: %s%d,%d,%ld%s,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%g,%ld,%ld,%ld,%.500s",
						appName.c_str(), mediaType, simType, http_code, timeoutClass.c_str(), totalPerformRequest, total, connect, startTransfer, resolve, appConnect, preTransfer, redirect, dlSize, reqSize,downloadbps,
						(((simType == eMEDIATYPE_VIDEO) || (simType == eMEDIATYPE_INIT_VIDEO) || (simType == eMEDIATYPE_PLAYLIST_VIDEO)) ? mpStreamAbstractionAAMP->GetVideoBitrate() : 0), // Video fragment current bitrate
						((res == CURLE_OK) ? effectiveUrl.c_str() : remoteUrl.c_str())); // Effective URL could be different than remoteURL and it is updated only for CURLE_OK case
					if(ui32CurlTrace < 10 )
					{
						AAMPLOG_INFO("%d.CurlTrace:Dns:%2.4f, Conn:%2.4f, Ssl:%2.4f, Redir:%2.4f, Pre:Start[%2.4f:%2.4f], Hdl:%p, Url:%s",
							ui32CurlTrace, resolve, connect, appConnect, redirect, preTransfer, startTransfer, curl,((res==CURLE_OK)?effectiveUrl.c_str():remoteUrl.c_str()));
						++ui32CurlTrace;
					}
				}

				if(!loopAgain)
					break;
			}
		}

		if (http_code == 200 || http_code == 206 || http_code == CURLE_OPERATION_TIMEDOUT)
		{
			if (http_code == CURLE_OPERATION_TIMEDOUT && buffer->len > 0)
			{
				AAMPLOG_WARN("Download timedout and obtained a partial buffer of size %d for a downloadTime=%d and isDownloadStalled:%d", buffer->len, downloadTimeMS, isDownloadStalled);
			}

			if (downloadTimeMS > 0 && fileType == eMEDIATYPE_VIDEO && CheckABREnabled())
			{
				int  AbrThresholdSize;
				GETCONFIGVALUE_PRIV(eAAMPConfig_ABRThresholdSize,AbrThresholdSize);
				//HybridABRManager mhABRManager;
				HybridABRManager::CurlAbortReason hybridabortReason = (HybridABRManager::CurlAbortReason) abortReason;
				if((buffer->len > AbrThresholdSize) && (!GetLLDashServiceData()->lowLatencyMode ||
                                        ( GetLLDashServiceData()->lowLatencyMode  && ISCONFIGSET_PRIV(eAAMPConfig_DisableLowLatencyABR))))
				{
					long currentProfilebps  = mpStreamAbstractionAAMP->GetVideoBitrate();
					long downloadbps = mhAbrManager.CheckAbrThresholdSize(buffer->len,downloadTimeMS,currentProfilebps,fragmentDurationMs,hybridabortReason);
						pthread_mutex_lock(&mLock);
						mhAbrManager.UpdateABRBitrateDataBasedOnCacheLength(mAbrBitrateData,downloadbps,false);
						pthread_mutex_unlock(&mLock);
					
				}
			}
		}
		if (http_code == 200 || http_code == 206)
		{
			if((mHarvestCountLimit > 0) && (mHarvestConfig & getHarvestConfigForMedia(fileType)))
			{
				/* Avoid chance of overwriting , in case of manifest and playlist, name will be always same */
				if(fileType == eMEDIATYPE_MANIFEST || fileType == eMEDIATYPE_PLAYLIST_AUDIO 
				|| fileType == eMEDIATYPE_PLAYLIST_IFRAME || fileType == eMEDIATYPE_PLAYLIST_SUBTITLE || fileType == eMEDIATYPE_PLAYLIST_VIDEO )
				{
					mManifestRefreshCount++;
				}
				
				AAMPLOG_WARN("aamp harvestCountLimit: %d mManifestRefreshCount %d", mHarvestCountLimit,mManifestRefreshCount);
				std::string harvestPath;
				GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestPath,harvestPath);
				if(harvestPath.empty() )
				{
					getDefaultHarvestPath(harvestPath);
					AAMPLOG_WARN("Harvest path has not configured, taking default path %s", harvestPath.c_str());
				}
				if(buffer->ptr)
				{
					if(aamp_WriteFile(remoteUrl, buffer->ptr, buffer->len, fileType, mManifestRefreshCount,harvestPath.c_str()))
						mHarvestCountLimit--;
				}  //CID:168113 - forward null
			}
			double expectedContentLength = 0;
			if ((!context.downloadIsEncoded) && CURLE_OK==curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &expectedContentLength) && ((int)expectedContentLength>0) && ((int)expectedContentLength != (int)buffer->len))
			{
				//Note: For non-compressed data, Content-Length header and buffer size should be same. For gzipped data, 'Content-Length' will be <= deflated data.
				AAMPLOG_WARN("AAMP Content-Length=%d actual=%d", (int)expectedContentLength, (int)buffer->len);
				http_code       =       416; // Range Not Satisfiable
				ret             =       false; // redundant, but harmless
				aamp_Free(buffer);
				memset(buffer, 0x00, sizeof(*buffer));
			}
			else
			{
				if(fileType == eMEDIATYPE_MANIFEST)
				{
					fileType = (MediaType)curlInstance;
				}
				else if (remoteUrl.find("iframe") != std::string::npos)
				{
					fileType = eMEDIATYPE_IFRAME;
				}
				ret = true;
			}
		}
		else
		{
			if (AAMP_IS_LOG_WORTHY_ERROR(res))
			{
				AAMPLOG_WARN("BAD URL:%s", remoteUrl.c_str());
			}
			aamp_Free(buffer);
			memset(buffer, 0x00, sizeof(*buffer));

			if (rate != 1.0)
			{
				fileType = eMEDIATYPE_IFRAME;
			}

			// dont generate anomaly reports for write and aborted errors
			// these are generated after trick play options,
			if( !(http_code == CURLE_ABORTED_BY_CALLBACK || http_code == CURLE_WRITE_ERROR || http_code == 204))
			{
				SendAnomalyEvent(ANOMALY_WARNING, "%s:%s,%s-%d url:%s", (mTSBEnabled ? "FOG" : "CDN"),
					MediaTypeString(fileType), (http_code < 100) ? "Curl" : "HTTP", http_code, remoteUrl.c_str());
			}
            
			if ( (httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_XREASON) && (httpRespHeaders[curlInstance].data.length() > 0) )
			{
				AAMPLOG_WARN("Received X-Reason header from %s: '%s'", mTSBEnabled?"Fog":"CDN Server", httpRespHeaders[curlInstance].data.c_str());
				SendAnomalyEvent(ANOMALY_WARNING, "%s X-Reason:%s", mTSBEnabled ? "Fog" : "CDN", httpRespHeaders[curlInstance].data.c_str());
			}
			else if ( (httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_FOG_REASON) && (httpRespHeaders[curlInstance].data.length() > 0) )
			{
				//extract error and url used by fog to download content from cdn
				// it is part of fog-reason
				if(fogError)
				{
					std::regex errRegx("-(.*),");
					std::smatch match;
					if (std::regex_search(httpRespHeaders[curlInstance].data, match, errRegx) && match.size() > 1) {
						if (!match.str(1).empty())
						{
							*fogError = std::stoi(match.str(1));
							AAMPLOG_INFO("Received FOG-Reason fogError: '%d'", *fogError);
						}
					}
				}

				//	get failed url from fog reason and update effectiveUrl
				if(!effectiveUrl.empty())
				{
					std::regex fromRegx("from:(.*),");
					std::smatch match;

					if (std::regex_search(httpRespHeaders[curlInstance].data, match, fromRegx) && match.size() > 1) {
						if (!match.str(1).empty())
						{
							effectiveUrl.assign(match.str(1).c_str());
							AAMPLOG_INFO("Received FOG-Reason effectiveUrl: '%s'", effectiveUrl.c_str());
						}
					}
				}


                AAMPLOG_WARN("Received FOG-Reason header: '%s'", httpRespHeaders[curlInstance].data.c_str());
                SendAnomalyEvent(ANOMALY_WARNING, "FOG-Reason:%s", httpRespHeaders[curlInstance].data.c_str());
            }
		}

		if (bitrate && (context.bitrate > 0))
		{
			AAMPLOG_INFO("Received getfile Bitrate : %ld", context.bitrate);
			*bitrate = context.bitrate;
		}

		if(abortReason != eCURL_ABORT_REASON_NONE && abortReason != eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT)
		{
			http_code = PARTIAL_FILE_START_STALL_TIMEOUT_AAMP;
		}
		else if (connectTime == 0.0)
		{
			//curl connection is failure
			if(CURLE_PARTIAL_FILE == http_code)
			{
				http_code = PARTIAL_FILE_CONNECTIVITY_AAMP;
			}
			else if(CURLE_OPERATION_TIMEDOUT == http_code)
			{
				http_code = OPERATION_TIMEOUT_CONNECTIVITY_AAMP;
			}
		}
		else if (CURLE_PARTIAL_FILE == http_code)
		{
			// download time expired with partial file for playlists/init fragments
			http_code = PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP;
		}
		pthread_mutex_lock(&mLock);
	}
	else
	{
		AAMPLOG_WARN("downloads disabled");
	}
	pthread_mutex_unlock(&mLock);
	if (http_error)
	{
		*http_error = http_code;
		if(downloadTime)
		{
			*downloadTime = fileDownloadTime;
		}
	}
	if (httpHeaders != NULL)
	{
		curl_slist_free_all(httpHeaders);
	}
	if (mIsFirstRequestToFOG)
	{
		mIsFirstRequestToFOG = false;
	}

    if (ISCONFIGSET_PRIV(eAAMPConfig_EnableLinearSimulator))
	{
		// NEW! note that for simulated playlists, ideally we'd not bother re-downloading playlist above

		AAMPLOG_INFO("*** Simulated Linear URL: %s\n", remoteUrl.c_str()); // Log incoming request

		switch( simType )
		{
			case eMEDIATYPE_MANIFEST:
			{
				// Reset state after requesting main manifest
				if( full_playlist_video_ptr )
				{
					// Flush old cached video playlist

					free( full_playlist_video_ptr );
					full_playlist_video_ptr = NULL;
					full_playlist_video_len = 0;
				}

				if( full_playlist_audio_ptr )
				{
					// Flush old cached audio playlist

					free( full_playlist_audio_ptr );
					full_playlist_audio_ptr = NULL;
					full_playlist_audio_len = 0;
				}

				simulation_start = aamp_GetCurrentTimeMS();
			}
				break; /* eMEDIATYPE_MANIFEST */

			case eMEDIATYPE_PLAYLIST_AUDIO:
			{
				if( !full_playlist_audio_ptr )
				{
					// Cache the full vod audio playlist

					full_playlist_audio_len = buffer->len;
					full_playlist_audio_ptr = (char *)malloc(buffer->len);
					memcpy( full_playlist_audio_ptr, buffer->ptr, buffer->len );
				}

				SimulateLinearWindow( buffer, full_playlist_audio_ptr, full_playlist_audio_len );
			}
				break; /* eMEDIATYPE_PLAYLIST_AUDIO */

			case eMEDIATYPE_PLAYLIST_VIDEO:
			{
				if( !full_playlist_video_ptr )
				{
					// Cache the full vod video playlist

					full_playlist_video_len = buffer->len;
					full_playlist_video_ptr = (char *)malloc(buffer->len);
					memcpy( full_playlist_video_ptr, buffer->ptr, buffer->len );
				}

				SimulateLinearWindow( buffer, full_playlist_video_ptr, full_playlist_video_len );
			}
				break; /* eMEDIATYPE_PLAYLIST_VIDEO */

			default:
				break;
		}
	}
	//Stip downloaded chunked Iframes when ranged requests receives 200 as HTTP response for HLS MP4
	if(  mConfig->IsConfigSet(eAAMPConfig_RepairIframes) && NULL != range && '\0' != range[0] && 200 == http_code && NULL != buffer->ptr && FORMAT_ISO_BMFF == this->mVideoFormat)
	{
		AAMPLOG_INFO( "Received HTTP 200 for ranged request (chunked iframe: %s: %s), starting to strip the fragment", range, remoteUrl.c_str() );
		size_t start;
		size_t end;
		try {
			if(2 == sscanf(range, "%zu-%zu", &start, &end))
			{
				// #EXT-X-BYTERANGE:19301@88 from manifest is equivalent to 88-19388 in HTTP range request
				size_t len = (end - start) + 1;
				if( buffer->len >= len)
				{	
					memmove(buffer->ptr, buffer->ptr + start, len);
					buffer->len=len;
				}
			
				// hack - repair wrong size in box
				IsoBmffBuffer repair(mLogObj);
				repair.setBuffer((uint8_t *)buffer->ptr, buffer->len);
				repair.parseBuffer(true);  //correctBoxSize=true
				AAMPLOG_INFO("Stripping the fragment for range request completed");
			}
			else
			{
				AAMPLOG_ERR("Stripping the fragment for range request failed, failed to parse range string");
			}
		}
		catch (std::exception &e)
		{
				AAMPLOG_ERR("Stripping the fragment for ranged request failed (%s)", e.what());
		}
	}

	return ret;
}

/**
 * @brief Download VideoEnd Session statistics from fog
 *
 * @return string tsbSessionEnd data from fog
 */
char * PrivateInstanceAAMP::GetOnVideoEndSessionStatData()
{
	std::string remoteUrl = "127.0.0.1:9080/sessionstat";
	long http_error = -1;
	char* ret = NULL;
	if(!mTsbRecordingId.empty())
	{
		/* Request session statistics for current recording ID
		 *
		 * example request: 127.0.0.1:9080/sessionstat/<recordingID>
		 *
		 */
		remoteUrl.append("/");
		remoteUrl.append(mTsbRecordingId);
		GrowableBuffer data;
		if(ProcessCustomCurlRequest(remoteUrl, &data, &http_error))
		{
			// succesfully requested
			AAMPLOG_INFO("curl request %s success", remoteUrl.c_str());
			cJSON *root = cJSON_Parse(data.ptr);
			if (root == NULL)
			{
				const char *error_ptr = cJSON_GetErrorPtr();
				if (error_ptr != NULL)
				{
					AAMPLOG_ERR("Invalid Json format: %s", error_ptr);
				}
			}
			else
			{
				ret = cJSON_PrintUnformatted(root);
				cJSON_Delete(root);
			}

		}
		else
		{
			// Failure in request
			AAMPLOG_ERR("curl request %s failed[%d]", remoteUrl.c_str(), http_error);
		}

		aamp_Free(&data);
	}

	return ret;
}

/**
 * @brief Perform custom curl request
 */
bool PrivateInstanceAAMP::ProcessCustomCurlRequest(std::string& remoteUrl, GrowableBuffer* buffer, long *http_error, CurlRequest request, std::string pData)
{
	bool ret = false;
	CURLcode res;
	long httpCode = -1;
	CURL *curl = curl_easy_init();
	if(curl)
	{
		AAMPLOG_INFO("%s, %d", remoteUrl.c_str(), request);
		
		CURL_EASY_SETOPT(curl, CURLOPT_TIMEOUT, DEFAULT_CURL_TIMEOUT);
		CURL_EASY_SETOPT(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
		CURL_EASY_SETOPT(curl, CURLOPT_FOLLOWLOCATION, 1L);

		if(!ISCONFIGSET_PRIV(eAAMPConfig_SslVerifyPeer)){
			CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		}
		else {
			CURL_EASY_SETOPT(curl, CURLOPT_SSLVERSION, mSupportedTLSVersion);
				CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		}
		CURL_EASY_SETOPT(curl, CURLOPT_URL, remoteUrl.c_str());

		if(eCURL_GET == request)
		{
			CurlCallbackContext context(this, buffer);
			memset(buffer, 0x00, sizeof(*buffer));
			CurlProgressCbContext progressCtx(this, NOW_STEADY_TS_MS);

			CURL_EASY_SETOPT(curl, CURLOPT_NOSIGNAL, 1L);
			CURL_EASY_SETOPT(curl, CURLOPT_WRITEFUNCTION, write_callback);
			CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
			CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSDATA, &progressCtx);
			CURL_EASY_SETOPT(curl, CURLOPT_NOPROGRESS, 0L);
			CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, &context);
			CURL_EASY_SETOPT(curl, CURLOPT_HTTPGET, 1L);
			res = curl_easy_perform(curl); // DELIA-57728 - avoid out of scope use of progressCtx
		}
		else if(eCURL_DELETE == request)
		{
			CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
			res = curl_easy_perform(curl);
		}
		else if(eCURL_POST == request)
		{
			CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDSIZE, pData.size());
			CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDS,(uint8_t * )pData.c_str());
			res = curl_easy_perform(curl);
		}

		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			if ((httpCode == 204) || (httpCode == 200))
			{
				ret = true;
			}
			else
			{
				AAMPLOG_ERR("Returned [%d]", httpCode);
			}
		}
		else
		{
			AAMPLOG_ERR("Failed to perform curl request, result:%d", res);
		}

		if(httpCode > 0)
		{
			*http_error = httpCode;
		}

		curl_easy_cleanup(curl);
	}
	return ret;
}

/**
 * @brief Terminate the stream
 */
void PrivateInstanceAAMP::TeardownStream(bool newTune)
{
	pthread_mutex_lock(&mLock);
	//Have to perfom this for trick and stop operations but avoid ad insertion related ones
	AAMPLOG_WARN(" mProgressReportFromProcessDiscontinuity:%d mDiscontinuityTuneOperationId:%d newTune:%d", mProgressReportFromProcessDiscontinuity, mDiscontinuityTuneOperationId, newTune);
	if ((mDiscontinuityTuneOperationId != 0) && (!newTune || mState == eSTATE_IDLE))
	{
		bool waitForDiscontinuityProcessing = true;
		if (mProgressReportFromProcessDiscontinuity)
		{
			AAMPLOG_WARN("TeardownStream invoked while mProgressReportFromProcessDiscontinuity and mDiscontinuityTuneOperationId[%d] set!", mDiscontinuityTuneOperationId);
			guint callbackID = aamp_GetSourceID();
			if ((callbackID != 0 && mDiscontinuityTuneOperationId == callbackID) || mAsyncTuneEnabled)
			{
				AAMPLOG_WARN("TeardownStream idle callback id[%d] and mDiscontinuityTuneOperationId[%d] match. Ignore further discontinuity processing!", callbackID, mDiscontinuityTuneOperationId);
				waitForDiscontinuityProcessing = false; // to avoid deadlock
				mDiscontinuityTuneOperationInProgress = false;
				mDiscontinuityTuneOperationId = 0;
			}
		}
		if (waitForDiscontinuityProcessing)
		{
			//wait for discontinuity tune operation to finish before proceeding with stop
			if (mDiscontinuityTuneOperationInProgress)
			{
				AAMPLOG_WARN("TeardownStream invoked while mDiscontinuityTuneOperationInProgress set. Wait until the Discontinuity Tune operation to complete!!");
				pthread_cond_wait(&mCondDiscontinuity, &mLock);
			}
			else
			{
				RemoveAsyncTask(mDiscontinuityTuneOperationId);
				mDiscontinuityTuneOperationId = 0;
			}
		}
	}
	// Maybe mDiscontinuityTuneOperationId is 0, ProcessPendingDiscontinuity can be invoked from NotifyEOSReached too
	else if (mProgressReportFromProcessDiscontinuity || mDiscontinuityTuneOperationInProgress)
	{
		if(mDiscontinuityTuneOperationInProgress)
		{
			AAMPLOG_WARN("TeardownStream invoked while mDiscontinuityTuneOperationInProgress set. Wait until the pending discontinuity tune operation to complete !!");
			pthread_cond_wait(&mCondDiscontinuity, &mLock);
		}
		else
		{
			AAMPLOG_WARN("TeardownStream invoked while mProgressReportFromProcessDiscontinuity set!");
			mDiscontinuityTuneOperationInProgress = false;
		}
	}

	//reset discontinuity related flags
	ResetDiscontinuityInTracks();
	UnblockWaitForDiscontinuityProcessToComplete();
	ResetTrackDiscontinuityIgnoredStatus();
	pthread_mutex_unlock(&mLock);

	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->Stop(false);
		SAFE_DELETE(mpStreamAbstractionAAMP);
	}

	pthread_mutex_lock(&mLock);
	mVideoFormat = FORMAT_INVALID;
	pthread_mutex_unlock(&mLock);
	if (streamerIsActive)
	{
#ifdef AAMP_STOP_SINK_ON_SEEK
		const bool forceStop = true;
		// Don't send event if nativeCCRendering is ON
		if (!ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			CCHandleEventPtr event = std::make_shared<CCHandleEvent>(0);
			mEventManager->SendEvent(event);
			AAMPLOG_WARN("Sent AAMP_EVENT_CC_HANDLE_RECEIVED with NULL handle");
		}
#else
		const bool forceStop = false;
#endif
		if (!forceStop && ((!newTune && ISCONFIGSET_PRIV(eAAMPConfig_DemuxVideoHLSTrack)) || ISCONFIGSET_PRIV(eAAMPConfig_PreservePipeline)))
		{
			mStreamSink->Flush(0, rate);
		}
		else
		{
#ifdef AAMP_CC_ENABLED
			AAMPLOG_INFO("before CC Release - mTuneType:%d mbPlayEnabled:%d ", mTuneType, mbPlayEnabled);
			if (mbPlayEnabled && mTuneType != eTUNETYPE_RETUNE)
			{
				AampCCManager::GetInstance()->Release(mCCId);
				mCCId = 0;
			}
			else
			{
				AAMPLOG_WARN("CC Release - skipped ");
			}
#endif
			if(!mbUsingExternalPlayer)
			{
				mStreamSink->Stop(!newTune);
			}
		}
	}
	else
	{
		for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
		{
			mbTrackDownloadsBlocked[iTrack] = true;
		}
		streamerIsActive = true;
	}
	mAdProgressId = "";
	std::queue<AAMPEventPtr> emptyEvQ;
	{
		std::lock_guard<std::mutex> lock(mAdEventQMtx);
		std::swap( mAdEventsQ, emptyEvQ );
	}
}

/**
 * @brief Establish PIPE session with Receiver
 */
bool PrivateInstanceAAMP::SetupPipeSession()
{
	bool retVal = false;
	if(m_fd != -1)
	{
		retVal = true; //Pipe exists
		return retVal;
	}
	if(mkfifo(strAAMPPipeName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1)
	{
		if(errno == EEXIST)
		{
			// Pipe exists
			//AAMPLOG_WARN("CreatePipe: Pipe already exists");
			retVal = true;
		}
		else
		{
			// Error
			AAMPLOG_ERR("CreatePipe: Failed to create named pipe %s for reading errno = %d (%s)",
				strAAMPPipeName, errno, strerror(errno));
		}
	}
	else
	{
		// Success
		//AAMPLOG_WARN("CreatePipe: mkfifo succeeded");
		retVal = true;
	}

	if(retVal)
	{
		// Open the named pipe for writing
		m_fd = open(strAAMPPipeName, O_WRONLY | O_NONBLOCK  );
		if (m_fd == -1)
		{
			// error
			AAMPLOG_WARN("OpenPipe: Failed to open named pipe %s for writing errno = %d (%s)",
				strAAMPPipeName, errno, strerror(errno));
		}
		else
		{
			// Success
			retVal = true;
		}
	}
	return retVal;
}


/**
 * @brief Close PIPE session with Receiver
 */
void PrivateInstanceAAMP::ClosePipeSession()
{
	if(m_fd != -1)
	{
		close(m_fd);
		m_fd = -1;
	}
}
                            
/**
 * @brief Send messages to Receiver over PIPE
 */
void PrivateInstanceAAMP::SendMessageOverPipe(const char *str,int nToWrite)
{
	if(m_fd != -1)
	{
		// Write the packet data to the pipe
		int nWritten =  write(m_fd, str, nToWrite);
		if(nWritten != nToWrite)
		{
			// Error
			AAMPLOG_WARN("Error writing data written = %d, size = %d errno = %d (%s)",
				nWritten, nToWrite, errno, strerror(errno));
			if(errno == EPIPE)
			{
				// broken pipe, lets reset and open again when the pipe is avail
				ClosePipeSession();
			}
		}
	}
}

/**
 * @brief Send message to reciever over PIPE
 */
void PrivateInstanceAAMP::SendMessage2Receiver(AAMP2ReceiverMsgType type, const char *data)
{
#ifdef CREATE_PIPE_SESSION_TO_XRE
	if(SetupPipeSession())
	{
		int dataLen = strlen(data);
		int sizeToSend = AAMP2ReceiverMsgHdrSz + dataLen;
		std::vector<uint8_t> tmp(sizeToSend,0);
		AAMP2ReceiverMsg *msg = (AAMP2ReceiverMsg *)(tmp.data());
		msg->type = (unsigned int)type;
		msg->length = dataLen;
		memcpy(msg->data, data, dataLen);
		SendMessageOverPipe((char *)tmp.data(), sizeToSend);
	}
#else
	AAMPLOG_INFO("AAMP=>XRE: %s",data);
#endif
}

/**
 * @brief The helper function which perform tuning
 * Common tune operations used on Tune, Seek, SetRate etc
 */
void PrivateInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
	bool newTune;
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		lastUnderFlowTimeMs[i] = 0;
	}
	pthread_mutex_lock(&mFragmentCachingLock);
	EnableAllMediaDownloads();
	//LazilyLoadConfigIfNeeded();
	mFragmentCachingRequired = false;
	mPauseOnFirstVideoFrameDisp = false;
	mFirstVideoFrameDisplayedEnabled = false;
	pthread_mutex_unlock(&mFragmentCachingLock);

	if( seekWhilePaused )
	{ // XIONE-4261 Player state not updated correctly after seek
		// Prevent gstreamer callbacks from placing us back into playing state by setting these gate flags before CBs are triggered
		// in this routine. See NotifyFirstFrameReceived(), NotifyFirstBufferProcessed(), NotifyFirstVideoFrameDisplayed()
		mPauseOnFirstVideoFrameDisp = true;
		mFirstVideoFrameDisplayedEnabled = true;
	}


	if (tuneType == eTUNETYPE_SEEK || tuneType == eTUNETYPE_SEEKTOLIVE || tuneType == eTUNETYPE_SEEKTOEND)
	{
		mSeekOperationInProgress = true;
	}

	if (eTUNETYPE_LAST == tuneType)
	{
		tuneType = mTuneType;
	}
	else
	{
		mTuneType = tuneType;
	}

	newTune = IsNewTune();

	if ((ISCONFIGSET_PRIV(eAAMPConfig_EnableLowLatencyDash)) && ( ISCONFIGSET_PRIV( eAAMPConfig_EnableLowLatencyCorrection ) ) &&\
	       	(newTune || tuneType == eTUNETYPE_SEEKTOLIVE || ( eTUNETYPE_RETUNE == tuneType && GetLLDashAdjustSpeed() )) )
	{
		SetLLDashAdjustSpeed(true);
		AAMPLOG_WARN("LL Dash speed correction enabled");
	}
	else
	{
		SetLLDashAdjustSpeed(false);
		AAMPLOG_WARN("LL Dash speed correction disabled");
	}


	// DELIA-39530 - Get position before pipeline is teared down
	if (eTUNETYPE_RETUNE == tuneType)
	{
		seek_pos_seconds = GetPositionSeconds();
	}
	else
	{
		//Only trigger the clear to encrypted pipeline switch while on retune
		mEncryptedPeriodFound = false;
		mPipelineIsClear = false;
		AAMPLOG_INFO ("Resetting mClearPipeline & mEncryptedPeriodFound");
	}

	TeardownStream(newTune|| (eTUNETYPE_RETUNE == tuneType));

#if defined(AMLOGIC)
	// Send new SEGMENT event only on all trickplay and trickplay -> play, not on pause -> play / seek while paused
	// this shouldn't impact seekplay or ADs on Peacock & LLAMA
	if (tuneType == eTUNETYPE_SEEK && !(mbSeeked == true || rate == 0 || (rate == 1 && pipeline_paused == true)))
		for (int i = 0; i < AAMP_TRACK_COUNT; i++) mbNewSegmentEvtSent[i] = false;
#endif

	ui32CurlTrace=0;

	if (newTune)
	{

		// send previouse tune VideoEnd Metrics data
		// this is done here because events are cleared on stop and there is chance that event may not get sent
		// check for mEnableVideoEndEvent and call SendVideoEndEvent ,object mVideoEnd is created inside SendVideoEndEvent
		if(mTuneAttempts == 1) // only for first attempt, dont send event when JSPP retunes.
		{
			SendVideoEndEvent();
		}

		mTsbRecordingId.clear();
		// initialize defaults
		SetState(eSTATE_INITIALIZING);
		culledSeconds = 0;
		durationSeconds = 60 * 60; // 1 hour
		rate = AAMP_NORMAL_PLAY_RATE;
		StoreLanguageList(std::set<std::string>());
		mTunedEventPending = true;
		mProfileCappedStatus = false;
#ifdef USE_OPENCDM
		AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();
		pInstance->GetDisplayResolution(mDisplayWidth, mDisplayHeight);
		pInstance->Release();
#endif
		AAMPLOG_INFO ("Display Resolution width:%d height:%d", mDisplayWidth, mDisplayHeight);

		mOrigManifestUrl.hostname = aamp_getHostFromURL(mManifestUrl);
		mOrigManifestUrl.isRemotehost = !(aamp_IsLocalHost(mOrigManifestUrl.hostname));
		AAMPLOG_TRACE("CurlTrace OrigManifest url:%s", mOrigManifestUrl.hostname.c_str());
	}

	trickStartUTCMS = -1;

	double playlistSeekPos = seek_pos_seconds - culledSeconds;
	AAMPLOG_INFO("playlistSeek : %f seek_pos_seconds:%f culledSeconds : %f ",playlistSeekPos,seek_pos_seconds,culledSeconds);
	if (playlistSeekPos < 0)
	{
		playlistSeekPos = 0;
		seek_pos_seconds = culledSeconds;
		AAMPLOG_WARN("Updated seek_pos_seconds %f ", seek_pos_seconds);
	}
	
	if (mMediaFormat == eMEDIAFORMAT_DASH)
	{
		#if defined (INTELCE)
		AAMPLOG_WARN("Error: Dash playback not available");
		mInitSuccess = false;
		SendErrorEvent(AAMP_TUNE_UNSUPPORTED_STREAM_TYPE);
		return;
		#else
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(mLogObj,this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObjectMPD(mLogObj, this); // special version for DASH
		}
		#endif
	}
	else if (mMediaFormat == eMEDIAFORMAT_HLS || mMediaFormat == eMEDIAFORMAT_HLS_MP4)
	{ // m3u8
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_HLS(mLogObj,this, playlistSeekPos, rate);
		if(NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(mLogObj, this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
	{
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_PROGRESSIVE(mLogObj,this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(mLogObj, this);    //Placeholder to reject the SetAlternateContents()
		}
		// Set to false so that EOS events can be sent. Flag value was whatever previous asset had set it to.
		SetIsLive(false);
	}
	else if (mMediaFormat == eMEDIAFORMAT_HDMI)
	{
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_HDMIIN(mLogObj,this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(mLogObj, this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_OTA)
	{
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_OTA(mLogObj,this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(mLogObj, this);    //Placeholder to reject the SetAlternateContents()
		}
	}
        else if (mMediaFormat == eMEDIAFORMAT_COMPOSITE)
        {
                mpStreamAbstractionAAMP = new StreamAbstractionAAMP_COMPOSITEIN(mLogObj,this, playlistSeekPos, rate);
                if (NULL == mCdaiObject)
                {
                        mCdaiObject = new CDAIObject(mLogObj, this);    //Placeholder to reject the SetAlternateContents()
                }
        }
	else if (mMediaFormat == eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA)
	{
		AAMPLOG_ERR("Error: SmoothStreamingMedia playback not supported");
		mInitSuccess = false;
		SendErrorEvent(AAMP_TUNE_UNSUPPORTED_STREAM_TYPE);
		return;
	}

	mInitSuccess = true;
	AAMPStatusType retVal;
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->SetCDAIObject(mCdaiObject);
		retVal = mpStreamAbstractionAAMP->Init(tuneType);
	}
	else
	{
		retVal = eAAMPSTATUS_GENERIC_ERROR;
	}

	// Validate tune type
	// (need to find a better way to do this)
	if (tuneType == eTUNETYPE_NEW_NORMAL) // either no offset (mIsDefaultOffset = true) or -1 was specified
	{
		if(!IsLive() && !mIsDefaultOffset)
		{
			if (mMediaFormat == eMEDIAFORMAT_DASH) //curently only supported for dash
			{
				tuneType = eTUNETYPE_NEW_END;
			}
		}
	}
	mIsDefaultOffset = false;

	if ((tuneType == eTUNETYPE_NEW_END) ||
	    (tuneType == eTUNETYPE_SEEKTOEND))
	{
		if (mMediaFormat != eMEDIAFORMAT_DASH)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: tune to end not supported for format");
			retVal = eAAMPSTATUS_GENERIC_ERROR;
		}
	}

	if (retVal != eAAMPSTATUS_OK)
	{
		// Check if the seek position is beyond the duration
		if(retVal == eAAMPSTATUS_SEEK_RANGE_ERROR)
		{
			AAMPLOG_WARN("mpStreamAbstractionAAMP Init Failed.Seek Position(%f) out of range(%lld)",mpStreamAbstractionAAMP->GetStreamPosition(),(GetDurationMs()/1000));
			NotifyEOSReached();
		}
		else if(mIsFakeTune)
		{
			if(retVal == eAAMPSTATUS_FAKE_TUNE_COMPLETE)
			{
				AAMPLOG(mLogObj, eLOGLEVEL_FATAL, "FATAL", "Fake tune completed");
			}
			else
			{
				SetState(eSTATE_COMPLETE);
				mEventManager->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS));
				AAMPLOG(mLogObj, eLOGLEVEL_FATAL, "FATAL", "Stopping fake tune playback");
			}
		}
		else if (DownloadsAreEnabled())
		{
			AAMPLOG_ERR("mpStreamAbstractionAAMP Init Failed.Error(%d)",retVal);
			AAMPTuneFailure failReason = AAMP_TUNE_INIT_FAILED;
			switch(retVal)
			{
			case eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_MANIFEST_DNLD_ERROR; break;
			case eAAMPSTATUS_PLAYLIST_VIDEO_DOWNLOAD_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR; break;
			case eAAMPSTATUS_PLAYLIST_AUDIO_DOWNLOAD_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR; break;
			case eAAMPSTATUS_MANIFEST_CONTENT_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR; break;
			case eAAMPSTATUS_MANIFEST_PARSE_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_MANIFEST_PARSE_ERROR; break;
			case eAAMPSTATUS_TRACKS_SYNCHRONISATION_ERROR:
			case eAAMPSTATUS_INVALID_PLAYLIST_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR; break;
			case eAAMPSTATUS_UNSUPPORTED_DRM_ERROR:
				failReason = AAMP_TUNE_DRM_UNSUPPORTED; break;
			default :
				break;
			}

			if (failReason == AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR || failReason == AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR)
			{
				long http_error = mPlaylistFetchFailError;
				SendDownloadErrorEvent(failReason, http_error);
			}
			else
			{
				SendErrorEvent(failReason);
			}
		}
		mInitSuccess = false;
		return;
	}
	else
	{
		prevPositionMiliseconds = -1;
		int volume = audio_volume;
		double updatedSeekPosition = mpStreamAbstractionAAMP->GetStreamPosition();
		seek_pos_seconds = updatedSeekPosition + culledSeconds;
		// Adjust seek_pos_second based on adjusted stream position and discontinuity start time for absolute progress reports
		if(ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) && !ISCONFIGSET_PRIV(eAAMPConfig_MidFragmentSeek) && !IsUninterruptedTSB())
		{
			double startTimeOfDiscontinuity = mpStreamAbstractionAAMP->GetStartTimeOfFirstPTS() / 1000;
			if(startTimeOfDiscontinuity > 0)
			{
				seek_pos_seconds = startTimeOfDiscontinuity + updatedSeekPosition;
				if(!IsLive())
				{
					// For dynamic => static cases, add culled seconds
					// For normal VOD, culledSeconds is expected to be 0.
					seek_pos_seconds += culledSeconds;
				}
				AAMPLOG_WARN("Position adjusted discontinuity start: %lf, Abs position: %lf", startTimeOfDiscontinuity, seek_pos_seconds);
			}
		}
		culledOffset = culledSeconds;
		UpdateProfileCappedStatus();
#ifndef AAMP_STOP_SINK_ON_SEEK
		AAMPLOG_WARN("Updated seek_pos_seconds %f culledSeconds :%f", seek_pos_seconds,culledSeconds);
#endif
		mpStreamAbstractionAAMP->GetStreamFormat(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat);
		AAMPLOG_INFO("TuneHelper : mVideoFormat %d, mAudioFormat %d mAuxFormat %d", mVideoFormat, mAudioFormat, mAuxFormat);

		//Identify if HLS with mp4 fragments, to change media format
		if (mVideoFormat == FORMAT_ISO_BMFF && mMediaFormat == eMEDIAFORMAT_HLS)
		{
			mMediaFormat = eMEDIAFORMAT_HLS_MP4;
		}

		// Enable fragment initial caching. Retune not supported
		if(tuneType != eTUNETYPE_RETUNE
			&& GetInitialBufferDuration() > 0
			&& rate == AAMP_NORMAL_PLAY_RATE
			&& mpStreamAbstractionAAMP->IsInitialCachingSupported())
		{
			mFirstVideoFrameDisplayedEnabled = true;
			mFragmentCachingRequired = true;
		}

		// Set Pause on First Video frame if seeking and requested
		if( mSeekOperationInProgress && seekWhilePaused )
		{
			mFirstVideoFrameDisplayedEnabled = true;
			mPauseOnFirstVideoFrameDisp = true;
		}

#ifndef AAMP_STOP_SINK_ON_SEEK
		if (mMediaFormat == eMEDIAFORMAT_HLS)
		{
			//Live adjust or syncTrack occurred, sent an updated flush event
			if ((!newTune && ISCONFIGSET_PRIV(eAAMPConfig_DemuxVideoHLSTrack)) || ISCONFIGSET_PRIV(eAAMPConfig_PreservePipeline))
			{
				mStreamSink->Flush(mpStreamAbstractionAAMP->GetFirstPTS(), rate);
			}
		}
		else if (mMediaFormat == eMEDIAFORMAT_DASH)
		{
                        /*
                        commenting the Flush call with updatedSeekPosition as a work around for
                        Trick play freeze issues observed for rogers cDVR content (MBTROGERS-838)
                        @TODO Need to investigate and identify proper way to send Flush and segment 
                        events to avoid the freeze  
			if (!(newTune || (eTUNETYPE_RETUNE == tuneType)) && !IsTSBSupported())
			{
				mStreamSink->Flush(updatedSeekPosition, rate);
			}
			else
			{
				mStreamSink->Flush(0, rate);
			}
			*/
			mStreamSink->Flush(mpStreamAbstractionAAMP->GetFirstPTS(), rate);
		}
		else if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
		{
			mStreamSink->Flush(updatedSeekPosition, rate);
			// ff trick mode, mp4 content is single file and muted audio to avoid glitch
			if (rate > AAMP_NORMAL_PLAY_RATE)
			{
				volume = 0;
			}
			// reset seek_pos after updating playback start, since mp4 content provide absolute position value
			seek_pos_seconds = 0;
		}
#endif
		if (!mbUsingExternalPlayer)
		{
			mStreamSink->SetVideoZoom(zoom_mode);
			mStreamSink->SetVideoMute(video_muted);
			mStreamSink->SetAudioVolume(volume);
			if (mbPlayEnabled)
			{
				mStreamSink->Configure(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat, mpStreamAbstractionAAMP->GetESChangeStatus(), mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
			}
		}
		mpStreamAbstractionAAMP->ResetESChangeStatus();
		mpStreamAbstractionAAMP->Start();
		if (!mbUsingExternalPlayer)
		{
			if (mbPlayEnabled)
				mStreamSink->Stream();
		}
	}

	if (tuneType == eTUNETYPE_SEEK || tuneType == eTUNETYPE_SEEKTOLIVE || tuneType == eTUNETYPE_SEEKTOEND)
	{
		mSeekOperationInProgress = false;
		// Pipeline is not configured if mbPlayEnabled is false, so not required
		if (mbPlayEnabled && pipeline_paused == true)
		{
			if(!mStreamSink->Pause(true, false))
			{
				AAMPLOG_INFO("mStreamSink Pause failed");
			}
		}
	}

#ifdef AAMP_CC_ENABLED
	if(!mIsFakeTune)
	{
		AAMPLOG_INFO("mCCId: %d",mCCId);
		// if mCCId has non zero value means it is same instance and cc release was not calle then dont get id. if zero then call getid.
		if(mCCId == 0 )
		{
			mCCId = AampCCManager::GetInstance()->GetId();
		}
		//restore CC if it was enabled for previous content.
		AampCCManager::GetInstance()->RestoreCC();
	}
#endif

	if (tuneType == eTUNETYPE_SEEK || tuneType == eTUNETYPE_SEEKTOLIVE || tuneType == eTUNETYPE_SEEKTOEND)
	{
		if (HasSidecarData())
		{ // has sidecar data
			if (mpStreamAbstractionAAMP)
				mpStreamAbstractionAAMP->ResumeSubtitleAfterSeek(subtitles_muted, mData);
		}
	}

	if (newTune && !mIsFakeTune)
	{
		PrivAAMPState state;
		GetState(state);
		if((state != eSTATE_ERROR) && (mMediaFormat != eMEDIAFORMAT_OTA))
		{
			/*For OTA this event will be generated from StreamAbstractionAAMP_OTA*/
			SetState(eSTATE_PREPARED);
			SendMediaMetadataEvent();
		}
	}
}

/**
 * @brief Reload TSB for same URL .
 */
void PrivateInstanceAAMP::ReloadTSB()
{
	TuneType tuneType =  eTUNETYPE_SEEK;

	mEventManager->SetPlayerState(eSTATE_IDLE);
	mManifestUrl = mTsbSessionRequestUrl + "&reloadTSB=true";
	// To post player configurations to fog on 1st time tune
	long configPassCode = -1;
	if(mTSBEnabled && ISCONFIGSET_PRIV(eAAMPConfig_EnableAampConfigToFog))
	{
		configPassCode = LoadFogConfig();
	}
	if(configPassCode == 200 || configPassCode == 204 || configPassCode == 206)
	{
		mMediaFormat = GetMediaFormatType(mManifestUrl.c_str());
		ResumeDownloads();

		mIsFirstRequestToFOG = (mTSBEnabled == true);

		{
			AAMPLOG_WARN("Reloading TSB, URL: %s", mManifestUrl.c_str());
		}
	}
}

/**
 * @brief Tune API
 */
void PrivateInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *pTraceID,bool audioDecoderStreamSync)
{
	int iCacheMaxSize = 0;
	int maxDrmSession = 1;
	double tmpVar=0;
	int intTmpVar=0;
	
	TuneType tuneType =  eTUNETYPE_NEW_NORMAL;
	const char *remapUrl = mConfig->GetChannelOverride(mainManifestUrl);
	if (remapUrl )
	{
		const char *remapLicenseUrl = NULL;
		mainManifestUrl = remapUrl;
		remapLicenseUrl = mConfig->GetChannelLicenseOverride(mainManifestUrl);
		if (remapLicenseUrl )
		{
			AAMPLOG_INFO("Channel License Url Override: [%s]", remapLicenseUrl);
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_LicenseServerUrl,std::string(remapLicenseUrl));
		}
	}
	mEventManager->SetPlayerState(eSTATE_IDLE);
	mConfig->CustomSearch(mainManifestUrl,mPlayerId,mAppName);

	mConfig->logging.setLogLevel(eLOGLEVEL_INFO);
	gpGlobalConfig->logging.setLogLevel(eLOGLEVEL_INFO);

	GETCONFIGVALUE_PRIV(eAAMPConfig_PlaybackOffset,seek_pos_seconds);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioRendition,preferredRenditionString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioCodec,preferredCodecString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLanguage,preferredLanguagesString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLabel,preferredLabelsString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioType,preferredTypeString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextRendition,preferredTextRenditionString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLanguage,preferredTextLanguagesString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLabel,preferredTextLabelString);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextType,preferredTextTypeString);
	UpdatePreferredAudioList();
	GETCONFIGVALUE_PRIV(eAAMPConfig_DRMDecryptThreshold,mDrmDecryptFailCount);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreCachePlaylistTime,mPreCacheDnldTimeWindow);
	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestCountLimit,mHarvestCountLimit);
	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestConfig,mHarvestConfig);
	GETCONFIGVALUE_PRIV(eAAMPConfig_AuthToken,mSessionToken);
	GETCONFIGVALUE_PRIV(eAAMPConfig_SubTitleLanguage,mSubLanguage);
	GETCONFIGVALUE_PRIV(eAAMPConfig_TLSVersion,mSupportedTLSVersion);
	mAsyncTuneEnabled = ISCONFIGSET_PRIV(eAAMPConfig_AsyncTune);
	GETCONFIGVALUE_PRIV(eAAMPConfig_LivePauseBehavior,intTmpVar);
	mPausedBehavior = (PausedBehavior)intTmpVar;
	GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout,tmpVar);
	mNetworkTimeoutMs = (long)CONVERT_SEC_TO_MS(tmpVar);
	GETCONFIGVALUE_PRIV(eAAMPConfig_ManifestTimeout,tmpVar);
	mManifestTimeoutMs = (long)CONVERT_SEC_TO_MS(tmpVar);
	GETCONFIGVALUE_PRIV(eAAMPConfig_PlaylistTimeout,tmpVar);
	mPlaylistTimeoutMs = (long)CONVERT_SEC_TO_MS(tmpVar);
	if(mPlaylistTimeoutMs <= 0) mPlaylistTimeoutMs = mManifestTimeoutMs;
	mLogTimetoTopProfile = true;
	// Reset mProgramDateTime to 0 , to avoid spill over to next tune if same session is 
	// reused 
	mProgramDateTime = 0;
	mMPDPeriodsInfo.clear();
	// Reset current audio/text track index
	mCurrentAudioTrackIndex = -1;
	mCurrentTextTrackIndex = -1;

#ifdef AAMP_RFC_ENABLED
	schemeIdUriDai = RFCSettings::getSchemeIdUriDaiStream();
#endif
	// Set the EventManager config
	// TODO When faketune code is added later , push the faketune status here 
	mEventManager->SetAsyncTuneState(mAsyncTuneEnabled);
	mIsFakeTune = strcasestr(mainManifestUrl, "fakeTune=true");
	if(mIsFakeTune)
	{
		mConfig->logging.setLogLevel(eLOGLEVEL_FATAL);
		gpGlobalConfig->logging.setLogLevel(eLOGLEVEL_FATAL);
	}
	mEventManager->SetFakeTuneFlag(mIsFakeTune);

	mTSBEnabled = strcasestr(mainManifestUrl, "tsb?") && ISCONFIGSET_PRIV(eAAMPConfig_Fog);
	if (bFirstAttempt)
	{
		// To post player configurations to fog on 1st time tune
		if(mTSBEnabled && ISCONFIGSET_PRIV(eAAMPConfig_EnableAampConfigToFog))
		{
			LoadFogConfig();
		}
		else
		{
			LoadAampAbrConfig();
		}
	}
	//temporary hack for peacock
	if (STARTS_WITH_IGNORE_CASE(mAppName.c_str(), "peacock"))
	{
		if(NULL == mAampCacheHandler)
		{
			mAampCacheHandler = new AampCacheHandler(mConfig->GetLoggerInstance());
		}
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM)
		// read the configured max drm session
		GETCONFIGVALUE_PRIV(eAAMPConfig_MaxDASHDRMSessions,maxDrmSession);
		if(NULL == mDRMSessionManager)
		{
			mDRMSessionManager = new AampDRMSessionManager(mLogObj, maxDrmSession);
		}
#endif
	}

	/* Reset counter in new tune */
	mManifestRefreshCount = 0;
	
	// For PreCaching of playlist , no max limit set as size will vary for each playlist length
	GETCONFIGVALUE_PRIV(eAAMPConfig_MaxPlaylistCacheSize,iCacheMaxSize);
	if(iCacheMaxSize != MAX_PLAYLIST_CACHE_SIZE)
	{
		getAampCacheHandler()->SetMaxPlaylistCacheSize(iCacheMaxSize*1024); // convert KB inputs to bytes
	}
	else if(mPreCacheDnldTimeWindow > 0)
	{
		// if precaching enabled, then set cache to infinite
		// support download of all the playlist files
		getAampCacheHandler()->SetMaxPlaylistCacheSize(PLAYLIST_CACHE_SIZE_UNLIMITED);
	}

	// Set max no of init fragment to be maintained in cache table, ByDefault 5.
	GETCONFIGVALUE_PRIV(eAAMPConfig_MaxInitFragCachePerTrack,iCacheMaxSize);
	if(iCacheMaxSize != MAX_INIT_FRAGMENT_CACHE_PER_TRACK)
	{
		getAampCacheHandler()->SetMaxInitFragCacheSize(iCacheMaxSize);
	}

	mAudioDecoderStreamSync = audioDecoderStreamSync;


	mMediaFormat = GetMediaFormatType(mainManifestUrl);

	mbUsingExternalPlayer = (mMediaFormat == eMEDIAFORMAT_OTA) || (mMediaFormat== eMEDIAFORMAT_HDMI) || (mMediaFormat==eMEDIAFORMAT_COMPOSITE);

	if (NULL == mStreamSink)
	{
		mStreamSink = new AAMPGstPlayer(mLogObj, this);
	}
	/* Initialize gstreamer plugins with correct priority to co-exist with webkit plugins.
	 * Initial priority of aamp plugins is PRIMARY which is less than webkit plugins.
	 * if aamp->Tune is called, aamp plugins should be used, so set priority to a greater value
	 * than that of that of webkit plugins*/
	static bool gstPluginsInitialized = false;
	if ((!gstPluginsInitialized) && (!mbUsingExternalPlayer))
	{
		gstPluginsInitialized = true;
		AAMPGstPlayer::InitializeAAMPGstreamerPlugins(mLogObj);
	}

	mbPlayEnabled = autoPlay;
	mPlayerPreBuffered = !autoPlay ;
	
	ResumeDownloads();

	if (!autoPlay)
	{
		pipeline_paused = true;
		AAMPLOG_WARN("AutoPlay disabled; Just caching the stream now.");
	}

	mIsDefaultOffset = (AAMP_DEFAULT_PLAYBACK_OFFSET == seek_pos_seconds);
	if (mIsDefaultOffset)
	{
		// eTUNETYPE_NEW_NORMAL
		// default behaviour is play live streams from 'live' point and VOD streams from the start
		seek_pos_seconds = 0;
	}
	else if (-1 == seek_pos_seconds)
	{
		// eTUNETYPE_NEW_NORMAL
		// behaviour is play live streams from 'live' point and VOD streams skip to the end (this will
		// be corrected later for vod)
		seek_pos_seconds = 0;
	}
	else
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: seek position already set, so eTUNETYPE_NEW_SEEK");
		tuneType = eTUNETYPE_NEW_SEEK;
	}

	AAMPLOG_INFO("Paused behavior : %d", mPausedBehavior);

	for(int i = 0; i < eCURLINSTANCE_MAX; i++)
	{
		//cookieHeaders[i].clear();
		httpRespHeaders[i].type = eHTTPHEADERTYPE_UNKNOWN;
		httpRespHeaders[i].data.clear();
	}

        //Add Custom Header via config
        {
                std::string customLicenseHeaderStr;
                GETCONFIGVALUE_PRIV(eAAMPConfig_CustomHeaderLicense,customLicenseHeaderStr);
                if(!customLicenseHeaderStr.empty())
                {
			if (ISCONFIGSET_PRIV(eAAMPConfig_CurlLicenseLogging))
                        {
                                AAMPLOG_WARN("CustomHeader :%s",customLicenseHeaderStr.c_str());
                        }
                        char* token = NULL;
                        char* tokenHeader = NULL;
                        char* str = (char*) customLicenseHeaderStr.c_str();

                        while ((token = strtok_r(str, ";", &str)))
                        {
                                int headerTokenIndex = 0;
                                std::string headerName;
                                std::vector<std::string> headerValue;
    
				while ((tokenHeader = strtok_r(token, ":", &token)))
				{
					if(headerTokenIndex == 0)
						headerName = tokenHeader;
					else if(headerTokenIndex == 1)
						headerValue.push_back(std::string(tokenHeader));
					else
						break;

					headerTokenIndex++;
				}
				if(!headerName.empty() && !headerValue.empty())
				{
					AddCustomHTTPHeader(headerName, headerValue, true);
				}
			}
		}
	}
	/** Least priority operator setting will override the value only if it is not set from dev config **/ 
	SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_WideVineKIDWorkaround,IsWideVineKIDWorkaround(mainManifestUrl));
	mIsWVKIDWorkaround = ISCONFIGSET_PRIV(eAAMPConfig_WideVineKIDWorkaround);
	if (mIsWVKIDWorkaround)
	{
		/** Set prefered DRM as Widevine with highest configuration **/
		AAMPLOG_INFO("WideVine KeyID workaround present: Setting preferred DRM as Widevine");
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PreferredDRM,(int)eDRM_WideVine);
	}

	std::tie(mManifestUrl, mDrmInitData) = ExtractDrmInitData(mainManifestUrl);
	
	mIsVSS = (strstr(mainManifestUrl, VSS_MARKER) || strstr(mainManifestUrl, VSS_MARKER_FOG));
	mTuneCompleted 	=	false;
	mPersistedProfileIndex	=	-1;
	mServiceZone.clear(); //clear the value if present
	mIsIframeTrackPresent = false;
	mIsTrackIdMismatch = false;
	mCurrentAudioTrackId = -1;
	mCurrentVideoTrackId = -1;
	mCurrentDrm = nullptr;
	SETCONFIGVALUE_PRIV(AAMP_STREAM_SETTING, eAAMPConfig_InterruptHandling, (mTSBEnabled && strcasestr(mainManifestUrl, "networkInterruption=true")));
	if(!ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) && ISCONFIGSET_PRIV(eAAMPConfig_InterruptHandling))
	{
		AAMPLOG_INFO("Absolute timeline reporting enabled for interrupt enabled TSB stream");
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING, eAAMPConfig_UseAbsoluteTimeline, true);
	}

	// DELIA-47965: Calling SetContentType without checking contentType != NULL, so that
	// mContentType will be reset to ContentType_UNKNOWN at the start of tune by default
	SetContentType(contentType);
	if (ContentType_CDVR == mContentType)
	{
		mIscDVR = true;
	}

	SetLowLatencyServiceConfigured(false);

	UpdateLiveOffset();
#ifdef AAMP_CC_ENABLED
	if (eMEDIAFORMAT_OTA == mMediaFormat)
	{
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			AampCCManager::GetInstance()->SetParentalControlStatus(false);
		}
	}
#endif
	
	if(bFirstAttempt)
	{
		mTuneAttempts = 1;	//Only the first attempt is xreInitiated.
		mPlayerLoadTime = NOW_STEADY_TS_MS;
		mLogTune = true;
		mFirstProgress = true;
	}
	else
	{
		mTuneAttempts++;
	}
	profiler.TuneBegin();
	ResetBufUnderFlowStatus();

	if( !remapUrl )
	{
		std::string mapMPDStr, mapM3U8Str;
		GETCONFIGVALUE_PRIV(eAAMPConfig_MapMPD,mapMPDStr);
		GETCONFIGVALUE_PRIV(eAAMPConfig_MapM3U8,mapM3U8Str);
		if (!mapMPDStr.empty() && mMediaFormat == eMEDIAFORMAT_HLS && (mContentType != ContentType_EAS)) //Don't map, if it is dash and dont map if it is EAS
		{
			std::string hostName = aamp_getHostFromURL(mManifestUrl);
			if((hostName.find(mapMPDStr) != std::string::npos) || (mTSBEnabled && mManifestUrl.find(mapMPDStr) != std::string::npos))
			{
				replace(mManifestUrl, ".m3u8", ".mpd");
				mMediaFormat = eMEDIAFORMAT_DASH;
			}
		}
		else if (!mapM3U8Str.empty() && mMediaFormat == eMEDIAFORMAT_DASH)
		{
			std::string hostName = aamp_getHostFromURL(mManifestUrl);
			if((hostName.find(mapM3U8Str) != std::string::npos) || (mTSBEnabled && mManifestUrl.find(mapM3U8Str) != std::string::npos))
			{
				replace(mManifestUrl, ".mpd" , ".m3u8");
				mMediaFormat = eMEDIAFORMAT_HLS;
			}
		}
		//DELIA-47890 Fog can be disable by  having option fog=0 option in aamp.cfg,based on  that gpGlobalConfig->noFog is updated
		//Removed variable gpGlobalConfig->fogSupportsDash as it has similar usage
		if(!ISCONFIGSET_PRIV(eAAMPConfig_Fog))
		{
			DeFog(mManifestUrl);
		}

		if (ISCONFIGSET_PRIV(eAAMPConfig_ForceEC3))
		{
			replace(mManifestUrl,".m3u8", "-eac3.m3u8");
		}
		if (ISCONFIGSET_PRIV(eAAMPConfig_DisableEC3))
		{
			replace(mManifestUrl, "-eac3.m3u8", ".m3u8");
		}

		if(ISCONFIGSET_PRIV(eAAMPConfig_ForceHttp))
		{
			replace(mManifestUrl, "https://", "http://");
		}

		if (mManifestUrl.find("mpd")!= std::string::npos) // new - limit this option to linear content as part of DELIA-23975
		{
			replace(mManifestUrl, "-eac3.mpd", ".mpd");
		} // mpd
	} // !remap_url

 	mConfig->GetsubstrUrlOverride(mManifestUrl);
	mIsFirstRequestToFOG = (mTSBEnabled == true);

	{
		char tuneStrPrefix[64];
		mTsbSessionRequestUrl.clear();
		memset(tuneStrPrefix, '\0', sizeof(tuneStrPrefix));
		if (!mAppName.empty())
		{
			snprintf(tuneStrPrefix, sizeof(tuneStrPrefix), "%s PLAYER[%d] APP: %s",(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mAppName.c_str());
		}
		else
		{
			snprintf(tuneStrPrefix, sizeof(tuneStrPrefix), "%s PLAYER[%d]", (mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId);
		}

		if(mManifestUrl.length() < MAX_URL_LOG_SIZE)
		{
			AAMPLOG_WARN("%s aamp_tune: attempt: %d format: %s URL: %s", tuneStrPrefix, mTuneAttempts, mMediaFormatName[mMediaFormat], mManifestUrl.c_str());
		}
		else
		{
			AAMPLOG_WARN("%s aamp_tune: attempt: %d format: %s URL: (BIG)", tuneStrPrefix, mTuneAttempts, mMediaFormatName[mMediaFormat]);
			AAMPLOG_INFO("URL: %s", mManifestUrl.c_str());
		}
		if(IsTSBSupported())
		{
			mTsbSessionRequestUrl = mManifestUrl;
		}
	}

	// this function uses mIsVSS and mTSBEnabled, hence it should be called after these variables are updated.
	ExtractServiceZone(mManifestUrl);
	SetTunedManifestUrl(mTSBEnabled);

	if(bFirstAttempt)
	{ // TODO: make mFirstTuneFormat of type MediaFormat
		mfirstTuneFmt = (int)mMediaFormat;
	}
	mCdaiObject = NULL;
	AcquireStreamLock();
	TuneHelper(tuneType);

	//Apply the cached video mute call as it got invoked when stream lock was not available
	if(mApplyCachedVideoMute)
	{
		mApplyCachedVideoMute = false;
		AAMPLOG_INFO("Cached videoMute is being executed, mute value: %d", video_muted);
		if (mpStreamAbstractionAAMP)
		{
			//There two fns are being called in PlayerInstanceAAMP::SetVideoMute
			SetVideoMute(video_muted);
			SetCCStatus(video_muted ? false : !subtitles_muted);
		}
	}
	ReleaseStreamLock();

	// To check and apply stored video rectangle properties
	if (mApplyVideoRect)
	{
		if ((mMediaFormat == eMEDIAFORMAT_OTA) || (mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE))
		{
			mpStreamAbstractionAAMP->SetVideoRectangle(mVideoRect.horizontalPos, mVideoRect.verticalPos, mVideoRect.width, mVideoRect.height);
		}
		else
		{
			mStreamSink->SetVideoRectangle(mVideoRect.horizontalPos, mVideoRect.verticalPos, mVideoRect.width, mVideoRect.height);
		}
		AAMPLOG_INFO("Update SetVideoRectangle x:%d y:%d w:%d h:%d", mVideoRect.horizontalPos, mVideoRect.verticalPos, mVideoRect.width, mVideoRect.height);
		mApplyVideoRect = false;
	}
	// do not change location of this set, it should be done after sending perviouse VideoEnd data which
	// is done in TuneHelper->SendVideoEndEvent function.
	if(pTraceID)
	{
		this->mTraceUUID = pTraceID;
		AAMPLOG_WARN("CMCD Session Id:%s", this->mTraceUUID.c_str());
	}
	else
	{
		this->mTraceUUID = "unknown";
	}
	//generate uuid/session id for applications which do not send id along with tune.
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableCMCD) && pTraceID == NULL)
	{
		uuid_t uuid;
		uuid_generate(uuid);
		char sid[MAX_SESSION_ID_LENGTH];
		uuid_unparse(uuid, sid);
		for (char *ptr = sid; *ptr; ++ptr) {
			*ptr = tolower(*ptr);
		}
		this->mTraceUUID = sid;
		AAMPLOG_WARN("CMCD Session Id generated:%s", this->mTraceUUID.c_str());
	}
}

/**
 *  @brief Get Language preference from aamp.cfg.
 */
LangCodePreference PrivateInstanceAAMP::GetLangCodePreference()
{
	int langCodePreference;
	GETCONFIGVALUE_PRIV(eAAMPConfig_LanguageCodePreference,langCodePreference);
	return (LangCodePreference)langCodePreference;
}

/**
 *  @brief Get Mediaformat types by parsing the url
 */
MediaFormat PrivateInstanceAAMP::GetMediaFormatType(const char *url)
{
	MediaFormat rc = eMEDIAFORMAT_UNKNOWN;
	std::string urlStr(url); // for convenience, convert to std::string

#ifdef TRUST_LOCATOR_EXTENSION_IF_PRESENT // disable to exersize alternate path
	if( urlStr.rfind("hdmiin:",0)==0 )
	{
		rc = eMEDIAFORMAT_HDMI;
	}
        else if( urlStr.rfind("cvbsin:",0)==0 )
        {
                rc = eMEDIAFORMAT_COMPOSITE;
        }
	else if((urlStr.rfind("live:",0)==0) || (urlStr.rfind("tune:",0)==0)  || (urlStr.rfind("mr:",0)==0)) 
	{
		rc = eMEDIAFORMAT_OTA;
	}
	else if(urlStr.rfind("http://127.0.0.1", 0) == 0) // starts with localhost
	{ // where local host is used; inspect further to determine if this locator involves FOG

		size_t fogUrlStart = urlStr.find("recordedUrl=", 16); // search forward, skipping 16-char http://127.0.0.1

		if(fogUrlStart != std::string::npos)
		{ // definitely FOG - extension is inside recordedUrl URI parameter

			size_t fogUrlEnd = urlStr.find("&", fogUrlStart); // end of recordedUrl

			if(fogUrlEnd != std::string::npos)
			{
				if(urlStr.rfind("m3u8", fogUrlEnd) != std::string::npos)
				{
					rc = eMEDIAFORMAT_HLS;
				}
				else if(urlStr.rfind("mpd", fogUrlEnd)!=std::string::npos)
				{
					rc = eMEDIAFORMAT_DASH;
				}

				// should never get here with UNKNOWN format, but if we do, just fall through to normal locator scanning
			}
		}
	}
	
	if(rc == eMEDIAFORMAT_UNKNOWN)
	{ // do 'normal' (non-FOG) locator parsing

		size_t extensionEnd = urlStr.find("?"); // delimited for URI parameters, or end-of-string
		std::size_t extensionStart = urlStr.rfind(".", extensionEnd); // scan backwards to find final "."
		int extensionLength;

		if(extensionStart != std::string::npos)
		{ // found an extension
			if(extensionEnd == std::string::npos)
			{
				extensionEnd = urlStr.length();
			}

			extensionStart++; // skip past the "." - no reason to re-compare it

			extensionLength = (int)(extensionEnd - extensionStart); // bytes between "." and end of query delimiter/end of string

			if(extensionLength == 4 && urlStr.compare(extensionStart, extensionLength, "m3u8") == 0)
			{
				rc = eMEDIAFORMAT_HLS;
			}
			else if(extensionLength == 3)
			{
				if(urlStr.compare(extensionStart,extensionLength,"mpd") == 0)
				{
					rc = eMEDIAFORMAT_DASH;
				}
				else if(urlStr.compare(extensionStart,extensionLength,"mp3") == 0 || urlStr.compare(extensionStart,extensionLength,"mp4") == 0 ||
					urlStr.compare(extensionStart,extensionLength,"mkv") == 0)
				{
					rc = eMEDIAFORMAT_PROGRESSIVE;
				}
			}
			else if((extensionLength == 2) || (urlStr.rfind("srt:",0)==0)) 
			{
				if((urlStr.compare(extensionStart,extensionLength,"ts") == 0) || (urlStr.rfind("srt:",0)==0))
				{
					rc = eMEDIAFORMAT_PROGRESSIVE;
				}
			}
		}
	}
#endif // TRUST_LOCATOR_EXTENSION_IF_PRESENT

	if(rc == eMEDIAFORMAT_UNKNOWN)
	{
		// no extension - sniff first few bytes of file to disambiguate
		struct GrowableBuffer sniffedBytes = {0, 0, 0};
		std::string effectiveUrl;
		long http_error;
		double downloadTime;
		long bitrate;
		int fogError;
		
		mOrigManifestUrl.hostname=aamp_getHostFromURL(url);
		mOrigManifestUrl.isRemotehost = !(aamp_IsLocalHost(mOrigManifestUrl.hostname));
		CurlInit(eCURLINSTANCE_MANIFEST_MAIN, 1, GetNetworkProxy());
		EnableMediaDownloads(eMEDIATYPE_MANIFEST);
		bool gotManifest = GetFile(url,
							&sniffedBytes,
							effectiveUrl,
							&http_error,
							&downloadTime,
							"0-150", // download first few bytes only
							// TODO: ideally could use "0-6" for range but write_callback sometimes not called before curl returns http 206
							eCURLINSTANCE_MANIFEST_MAIN,
							false,
							eMEDIATYPE_MANIFEST,
							&bitrate,
							&fogError,
							0.0);

		if(gotManifest)
		{
			if(sniffedBytes.len >= 7 && memcmp(sniffedBytes.ptr, "#EXTM3U8", 7) == 0)
			{
				rc = eMEDIAFORMAT_HLS;
			}
			else if((sniffedBytes.len >= 6 && memcmp(sniffedBytes.ptr, "<?xml ", 6) == 0) || // can start with xml
					 (sniffedBytes.len >= 5 && memcmp(sniffedBytes.ptr, "<MPD ", 5) == 0)) // or directly with mpd
			{ // note: legal to have whitespace before leading tag
				aamp_AppendNulTerminator(&sniffedBytes);
				if (strstr(sniffedBytes.ptr, "SmoothStreamingMedia"))
				{
					rc = eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA;
				}
				else
				{
					rc = eMEDIAFORMAT_DASH;
				}
			}
			else
			{
				rc = eMEDIAFORMAT_PROGRESSIVE;
			}
		}
		aamp_Free(&sniffedBytes);
		CurlTerm(eCURLINSTANCE_MANIFEST_MAIN);
	}
	return rc;
}

/**
 *   @brief Check if AAMP is in stalled state after it pushed EOS to
 *   notify discontinuity
 */
void PrivateInstanceAAMP::CheckForDiscontinuityStall(MediaType mediaType)
{
	AAMPLOG_TRACE("Enter mediaType %d", mediaType);
	long discontinuityTimeoutValue;
	GETCONFIGVALUE_PRIV(eAAMPConfig_DiscontinuityTimeout,discontinuityTimeoutValue);
	if(!(mStreamSink->CheckForPTSChangeWithTimeout(discontinuityTimeoutValue)))
	{
		pthread_mutex_lock(&mLock);

		if (mDiscontinuityTuneOperationId != 0 || mDiscontinuityTuneOperationInProgress)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Ignored retune!! Discontinuity handler already spawned(%d) or inprogress(%d)",
							mDiscontinuityTuneOperationId, mDiscontinuityTuneOperationInProgress);
			pthread_mutex_unlock(&mLock);
			return;
		}

		pthread_mutex_unlock(&mLock);

		AAMPLOG_INFO("No change in PTS for more than %ld ms, schedule retune!", discontinuityTimeoutValue);
		ResetDiscontinuityInTracks();

		ResetTrackDiscontinuityIgnoredStatus();
		ScheduleRetune(eSTALL_AFTER_DISCONTINUITY, mediaType);
	}
	AAMPLOG_TRACE("Exit mediaType %d", mediaType);
}

/**
 * @brief updates mServiceZone (service zone) member with string extracted from locator &sz URI parameter
 */
void PrivateInstanceAAMP::ExtractServiceZone(std::string url)
{
	if(mIsVSS && !url.empty())
	{
		if(mTSBEnabled)
		{ // extract original locator from FOG recordedUrl URI parameter
			DeFog(url);
		}
		size_t vssStart = url.find(VSS_MARKER);
		if( vssStart != std::string::npos )
		{
			vssStart += VSS_MARKER_LEN; // skip "?sz="
			size_t vssLen = url.find('&',vssStart);
			if( vssLen != std::string::npos )
			{
				vssLen -= vssStart;
			}
			mServiceZone = url.substr(vssStart, vssLen );
			aamp_DecodeUrlParameter(mServiceZone); // DELIA-44703
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: ERROR: url does not have vss marker :%s ",url.c_str());
		}
	}
}

/**
 *  @brief Set Content Type
 */
std::string PrivateInstanceAAMP::GetContentTypString()
{
    std::string strRet;
    switch(mContentType)
    {
        case ContentType_CDVR :
        {
            strRet = "CDVR"; //cdvr
            break;
        }
        case ContentType_VOD :
        {
            strRet = "VOD"; //vod
            break;
        }    
        case ContentType_LINEAR :
        {
            strRet = "LINEAR"; //linear
            break;
        }    
        case ContentType_IVOD :
        {
            strRet = "IVOD"; //ivod
            break;
        }    
        case ContentType_EAS :
        {
            strRet ="EAS"; //eas
            break;
        }    
        case ContentType_CAMERA :
        {
            strRet = "XfinityHome"; //camera
            break;
        }    
        case ContentType_DVR :
        {
            strRet = "DVR"; //dvr
            break;
        }    
        case ContentType_MDVR :
        {
            strRet =  "MDVR" ; //mdvr
            break;
        }    
        case ContentType_IPDVR :
        {
            strRet ="IPDVR" ; //ipdvr
            break;
        }    
        case ContentType_PPV :
        {
            strRet =  "PPV"; //ppv
            break;
        }
        case ContentType_OTT :
        {
            strRet =  "OTT"; //ott
            break;
        }
        case ContentType_OTA :
        {
            strRet =  "OTA"; //ota
            break;
        }
        case ContentType_SLE :
        {
            strRet = "SLE"; // single live event
            break;
        }
        default:
        {
            strRet =  "Unknown";
            break;
        }
     }

    return strRet;
}

/**
 * @brief Notify about sink buffer full
 */
void PrivateInstanceAAMP::NotifySinkBufferFull(MediaType type)
{
	if(type != eMEDIATYPE_VIDEO)
		return;

	if(mpStreamAbstractionAAMP)
	{
		MediaTrack* video = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_VIDEO);
		if(video && video->enabled)
			video->OnSinkBufferFull();
	}
}

/**
 * @brief Set Content Type
 */
void PrivateInstanceAAMP::SetContentType(const char *cType)
{
	mContentType = ContentType_UNKNOWN; //default unknown
	if(NULL != cType)
	{
		mPlaybackMode = std::string(cType);
		if(mPlaybackMode == "CDVR")
		{
			mContentType = ContentType_CDVR; //cdvr
		}
		else if(mPlaybackMode == "VOD")
		{
			mContentType = ContentType_VOD; //vod
		}
		else if(mPlaybackMode == "LINEAR_TV")
		{
			mContentType = ContentType_LINEAR; //linear
		}
		else if(mPlaybackMode == "IVOD")
		{
			mContentType = ContentType_IVOD; //ivod
		}
		else if(mPlaybackMode == "EAS")
		{
			mContentType = ContentType_EAS; //eas
		}
		else if(mPlaybackMode == "xfinityhome")
		{
			mContentType = ContentType_CAMERA; //camera
		}
		else if(mPlaybackMode == "DVR")
		{
			mContentType = ContentType_DVR; //dvr
		}
		else if(mPlaybackMode == "MDVR")
		{
			mContentType = ContentType_MDVR; //mdvr
		}
		else if(mPlaybackMode == "IPDVR")
		{
			mContentType = ContentType_IPDVR; //ipdvr
		}
		else if(mPlaybackMode == "PPV")
		{
			mContentType = ContentType_PPV; //ppv
		}
		else if(mPlaybackMode == "OTT")
		{
			mContentType = ContentType_OTT; //ott
		}
		else if(mPlaybackMode == "OTA")
		{
			mContentType = ContentType_OTA; //ota
		}
		else if(mPlaybackMode == "HDMI_IN")
		{
			mContentType = ContentType_HDMIIN; //ota
		}
		else if(mPlaybackMode == "COMPOSITE_IN")
		{
			mContentType = ContentType_COMPOSITEIN; //ota
		}
		else if(mPlaybackMode == "SLE")
		{
			mContentType = ContentType_SLE; //single live event
		}
	}
}

/**
 * @brief Get Content Type
 */
ContentType PrivateInstanceAAMP::GetContentType() const
{
	return mContentType;
}

/**
 *   @brief Extract DRM init data from the provided URL
 *          If present, the init data will be removed from the returned URL
 *          and provided as a separate string
 *   @return tuple containing the modified URL and DRM init data
 */
const std::tuple<std::string, std::string> PrivateInstanceAAMP::ExtractDrmInitData(const char *url)
{
	std::string urlStr(url);
	std::string drmInitDataStr;

	const size_t queryPos = urlStr.find("?");
	std::string modUrl;
	if (queryPos != std::string::npos)
	{
		// URL contains a query string. Strip off & decode the drmInitData (if present)
		modUrl = urlStr.substr(0, queryPos);
		const std::string parameterDefinition("drmInitData=");
		std::string parameter;
		std::stringstream querySs(urlStr.substr(queryPos + 1, std::string::npos));
		while (std::getline(querySs, parameter, '&'))
		{ // with each URI parameter
			if (parameter.rfind(parameterDefinition, 0) == 0)
			{ // found drmInitData URI parameter
				drmInitDataStr = parameter.substr(parameterDefinition.length());
				aamp_DecodeUrlParameter( drmInitDataStr );
			}
			else
			{ // filter out drmInitData; reintroduce all other URI parameters
				modUrl.append((queryPos == modUrl.length()) ? "?" : "&");
				modUrl.append(parameter);
			}
		}
		urlStr = modUrl;
	}
	return std::tuple<std::string, std::string>(urlStr, drmInitDataStr);
}

/**
 *   @brief Check if autoplay enabled for current stream
 */
bool PrivateInstanceAAMP::IsPlayEnabled()
{
	return mbPlayEnabled;
}

/**
 * @brief Soft stop the player instance.
 *
 */
void PrivateInstanceAAMP::detach()
{
	if(mpStreamAbstractionAAMP && mbPlayEnabled) //Player is running
	{
		pipeline_paused = true;
		seek_pos_seconds  = GetPositionSeconds();
		AAMPLOG_WARN("Player %s=>%s and soft release.Detach at position %f", STRFGPLAYER, STRBGPLAYER,seek_pos_seconds );
		DisableDownloads(); //disable download
		mpStreamAbstractionAAMP->SeekPosUpdate(seek_pos_seconds );
		mpStreamAbstractionAAMP->StopInjection();
#ifdef AAMP_CC_ENABLED
		// Stop CC when pipeline is stopped
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			AampCCManager::GetInstance()->Release(mCCId);
			mCCId = 0;
		}
#endif
		if ISCONFIGSET_PRIV(eAAMPConfig_MultiPipelineDai)
		{
			mStreamSink->Detach();    // keep last frame detach for deferred call to Stop
		}
		else
		{
			mStreamSink->Stop(true);
		}
		mbPlayEnabled = false;
		mbDetached=true;
		mPlayerPreBuffered  = false;
		//EnableDownloads();// enable downloads
	}
}

/**
 * @brief Get AampCacheHandler instance
 */
AampCacheHandler * PrivateInstanceAAMP::getAampCacheHandler()
{
	return mAampCacheHandler;
}

/**
 * @brief Get maximum bitrate value.
 */
long PrivateInstanceAAMP::GetMaximumBitrate()
{
	long lMaxBitrate;
	GETCONFIGVALUE_PRIV(eAAMPConfig_MaxBitrate,lMaxBitrate);
	return lMaxBitrate;
}

/**
 * @brief Get minimum bitrate value.
 */
long PrivateInstanceAAMP::GetMinimumBitrate()
{
	long lMinBitrate;
	GETCONFIGVALUE_PRIV(eAAMPConfig_MinBitrate,lMinBitrate);
	return lMinBitrate;
}

/**
 * @brief Get default bitrate value.
 */
long PrivateInstanceAAMP::GetDefaultBitrate()
{
	long defaultBitRate;
	GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate,defaultBitRate);
	return defaultBitRate;
}

/**
 * @brief Get Default bitrate for 4K
 */
long PrivateInstanceAAMP::GetDefaultBitrate4K()
{
	long defaultBitRate;
	GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate4K,defaultBitRate);
	return defaultBitRate;
}

/**
 * @brief Get Default Iframe bitrate value.
 */
long PrivateInstanceAAMP::GetIframeBitrate()
{
	long defaultIframeBitRate;
	GETCONFIGVALUE_PRIV(eAAMPConfig_IFrameDefaultBitrate,defaultIframeBitRate);
	return defaultIframeBitRate;
}

/**
 * @brief Get Default Iframe bitrate 4K value.
 */
long PrivateInstanceAAMP::GetIframeBitrate4K()
{
	long defaultIframeBitRate4K;
	GETCONFIGVALUE_PRIV(eAAMPConfig_IFrameDefaultBitrate4K,defaultIframeBitRate4K);
	return defaultIframeBitRate4K;
}

/**
 * @brief Fetch a file from CDN and update profiler
 */
char *PrivateInstanceAAMP::LoadFragment(ProfilerBucketType bucketType, std::string fragmentUrl, std::string& effectiveUrl, size_t *len, unsigned int curlInstance, const char *range, long * http_code, double *downloadTime, MediaType fileType,int * fogError)
{
	profiler.ProfileBegin(bucketType);
	struct GrowableBuffer fragment = { 0, 0, 0 }; // TODO: leaks if thread killed
	if (!GetFile(fragmentUrl, &fragment, effectiveUrl, http_code, downloadTime, range, curlInstance, true, fileType,NULL,fogError))
	{
		profiler.ProfileError(bucketType, *http_code);
		profiler.ProfileEnd(bucketType);
	}
	else
	{
		profiler.ProfileEnd(bucketType);
	}
	*len = fragment.len;
	return fragment.ptr;
}

/**
 * @brief Fetch a file from CDN and update profiler
 */
bool PrivateInstanceAAMP::LoadFragment(ProfilerBucketType bucketType, std::string fragmentUrl,std::string& effectiveUrl, struct GrowableBuffer *fragment, 
					unsigned int curlInstance, const char *range, MediaType fileType,long * http_code, double *downloadTime, long *bitrate,int * fogError, double fragmentDurationSeconds)
{
	bool ret = true;
	profiler.ProfileBegin(bucketType);
	if (!GetFile(fragmentUrl, fragment, effectiveUrl, http_code, downloadTime, range, curlInstance, false,fileType, bitrate, NULL, fragmentDurationSeconds))
	{
		ret = false;
		profiler.ProfileError(bucketType, *http_code);
		profiler.ProfileEnd(bucketType);
	}
	else
	{
		profiler.ProfileEnd(bucketType);
	}
	return ret;
}

/**
 * @brief Push fragment to the gstreamer
 */
void PrivateInstanceAAMP::PushFragment(MediaType mediaType, char *ptr, size_t len, double fragmentTime, double fragmentDuration)
{
	BlockUntilGstreamerWantsData(NULL, 0, 0);
	SyncBegin();
	mStreamSink->SendCopy(mediaType, ptr, len, fragmentTime, fragmentTime, fragmentDuration);
	SyncEnd();
}

/**
 * @brief Push fragment to the gstreamer
 */
void PrivateInstanceAAMP::PushFragment(MediaType mediaType, GrowableBuffer* buffer, double fragmentTime, double fragmentDuration)
{
	BlockUntilGstreamerWantsData(NULL, 0, 0);
	SyncBegin();
	mStreamSink->SendTransfer(mediaType, buffer, fragmentTime, fragmentTime, fragmentDuration);
	SyncEnd();
}

/**
 * @brief End of stream reached
 */
void PrivateInstanceAAMP::EndOfStreamReached(MediaType mediaType)
{
	if (mediaType != eMEDIATYPE_SUBTITLE)
	{
		SyncBegin();
		mStreamSink->EndOfStreamReached(mediaType);
		SyncEnd();

		// If EOS during Buffering, set Playing and let buffer to dry out
		// Sink is already unpaused by EndOfStreamReached()
		pthread_mutex_lock(&mFragmentCachingLock);
		mFragmentCachingRequired = false;
		PrivAAMPState state;
		GetState(state);
		if(state == eSTATE_BUFFERING)
		{
			if(mpStreamAbstractionAAMP)
			{
				mpStreamAbstractionAAMP->NotifyPlaybackPaused(false);
			}
			SetState(eSTATE_PLAYING);
		}
		pthread_mutex_unlock(&mFragmentCachingLock);
	}
}

/**
 * @brief Get seek base position
 */
double PrivateInstanceAAMP::GetSeekBase(void)
{
	return seek_pos_seconds;
}

/**
 * @brief Get current drm
 */
std::shared_ptr<AampDrmHelper> PrivateInstanceAAMP::GetCurrentDRM(void)
{
	return mCurrentDrm;
}

/**
 *    @brief Get available thumbnail tracks.
 */
std::string PrivateInstanceAAMP::GetThumbnailTracks()
{
	std::string op;
	AcquireStreamLock();
	if(mpStreamAbstractionAAMP)
	{
		AAMPLOG_TRACE("Entering PrivateInstanceAAMP");
		std::vector<StreamInfo*> data = mpStreamAbstractionAAMP->GetAvailableThumbnailTracks();
		cJSON *root;
		cJSON *item;
		if(!data.empty())
		{
			root = cJSON_CreateArray();
			if(root)
			{
				for( int i = 0; i < data.size(); i++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if(data[i]->bandwidthBitsPerSecond >= 0)
					{
						char buf[32];
						sprintf(buf,"%dx%d",data[i]->resolution.width,data[i]->resolution.height);
						cJSON_AddStringToObject(item,"RESOLUTION",buf);
						cJSON_AddNumberToObject(item,"BANDWIDTH",data[i]->bandwidthBitsPerSecond);
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					op.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		AAMPLOG_TRACE("In PrivateInstanceAAMP::Json string:%s",op.c_str());
	}
	ReleaseStreamLock();
	return op;
}

/**
 *  @brief Get the Thumbnail Tile data.
 */
std::string PrivateInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	std::string rc;
	AcquireStreamLock();
	if(mpStreamAbstractionAAMP)
	{
		std::string baseurl;
		int raw_w, raw_h, width, height;
		std::vector<ThumbnailData> datavec = mpStreamAbstractionAAMP->GetThumbnailRangeData(tStart, tEnd, &baseurl, &raw_w, &raw_h, &width, &height);
		if( !datavec.empty() )
		{
			cJSON *root = cJSON_CreateObject();
			if(!baseurl.empty())
			{
				cJSON_AddStringToObject(root,"baseUrl",baseurl.c_str());
			}
			if(raw_w > 0)
			{
				cJSON_AddNumberToObject(root,"raw_w",raw_w);
			}
			if(raw_h > 0)
			{
				cJSON_AddNumberToObject(root,"raw_h",raw_h);
			}
			cJSON_AddNumberToObject(root,"width",width);
			cJSON_AddNumberToObject(root,"height",height);

			cJSON *tile = cJSON_AddArrayToObject(root,"tile");
			for( const ThumbnailData &iter : datavec )
			{
				cJSON *item;
				cJSON_AddItemToArray(tile, item = cJSON_CreateObject() );
				if(!iter.url.empty())
				{
					cJSON_AddStringToObject(item,"url",iter.url.c_str());
				}
				cJSON_AddNumberToObject(item,"t",iter.t);
				cJSON_AddNumberToObject(item,"d",iter.d);
				cJSON_AddNumberToObject(item,"x",iter.x);
				cJSON_AddNumberToObject(item,"y",iter.y);
			}
			char *jsonStr = cJSON_Print(root);
			if( jsonStr )
			{
				rc.assign( jsonStr );
			}
			cJSON_Delete(root);
		}
	}
	ReleaseStreamLock();
	return rc;
}


TunedEventConfig PrivateInstanceAAMP::GetTuneEventConfig(bool isLive)
{
	int tunedEventConfig;
	GETCONFIGVALUE_PRIV(eAAMPConfig_TuneEventConfig,tunedEventConfig);
	return (TunedEventConfig)tunedEventConfig;
}

/**
 * @brief to update the preferredaudio codec, rendition and languages  list
 */
void PrivateInstanceAAMP::UpdatePreferredAudioList()
{
	if(!preferredRenditionString.empty())
	{
		preferredRenditionList.clear();
		std::istringstream ss(preferredRenditionString);
		std::string rendition;
		while(std::getline(ss, rendition, ','))
		{
			preferredRenditionList.push_back(rendition);
			AAMPLOG_INFO("Parsed preferred rendition: %s",rendition.c_str());
		}
		AAMPLOG_INFO("Number of preferred Renditions: %d",
                        preferredRenditionList.size());
	}

	if(!preferredCodecString.empty())
	{
		preferredCodecList.clear();
        	std::istringstream ss(preferredCodecString);
        	std::string codec;
        	while(std::getline(ss, codec, ','))
        	{
                	preferredCodecList.push_back(codec);
                	AAMPLOG_INFO("Parsed preferred codec: %s",codec.c_str());
        	}
		AAMPLOG_INFO("Number of preferred codec: %d",
                        preferredCodecList.size());
	}

	if(!preferredLanguagesString.empty())
	{
		preferredLanguagesList.clear();
        	std::istringstream ss(preferredLanguagesString);
        	std::string lng;
        	while(std::getline(ss, lng, ','))
        	{
        	        preferredLanguagesList.push_back(lng);
        	        AAMPLOG_INFO("Parsed preferred lang: %s",lng.c_str());
        	}
		AAMPLOG_INFO("Number of preferred languages: %d",
                        preferredLanguagesList.size());
	}
	if(!preferredLabelsString.empty())
	{
		preferredLabelList.clear();
		std::istringstream ss(preferredLabelsString);
		std::string lng;
		while(std::getline(ss, lng, ','))
		{
				preferredLabelList.push_back(lng);
				AAMPLOG_INFO("Parsed preferred Label: %s",lng.c_str());
		}
		AAMPLOG_INFO("Number of preferred Labels: %d", preferredLabelList.size());
	}
	

}

/**
 *  @brief Set async tune configuration for EventPriority
 */
void PrivateInstanceAAMP::SetEventPriorityAsyncTune(bool bValue)
{
	if(bValue)
	{
		mEventPriority = AAMP_MAX_EVENT_PRIORITY;
	}
	else
	{
		mEventPriority = G_PRIORITY_DEFAULT_IDLE;
	}	
}

/**
 * @brief Get async tune configuration
 */
bool PrivateInstanceAAMP::GetAsyncTuneConfig()
{
        return mAsyncTuneEnabled;
}

/**
 * @brief Set video rectangle
 */
void PrivateInstanceAAMP::UpdateVideoRectangle (int x, int y, int w, int h)
{
	mVideoRect.horizontalPos = x;
	mVideoRect.verticalPos   = y;
	mVideoRect.width = w;
	mVideoRect.height = h;
	mApplyVideoRect = true;
	AAMPLOG_INFO("Backup VideoRectangle x:%d y:%d w:%d h:%d", x, y, w, h);
}

/**
 * @brief Set video rectangle
 */
void PrivateInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
	pthread_mutex_lock(&mStreamLock);
	PrivAAMPState state;
	GetState(state);
	if (mpStreamAbstractionAAMP && state > eSTATE_PREPARING)
	{
		if ((mMediaFormat == eMEDIAFORMAT_OTA) || (mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE))
		{
			mpStreamAbstractionAAMP->SetVideoRectangle(x, y, w, h);
		}
		else
		{
			mStreamSink->SetVideoRectangle(x, y, w, h);
		}
	}
	else
	{
		AAMPLOG_INFO("mpStreamAbstractionAAMP is not Ready, Backup video rect values, current player state: %d", state);
		UpdateVideoRectangle (x, y, w, h);
	}
	pthread_mutex_unlock(&mStreamLock);
}
/**
 *   @brief Set video zoom.
 */
void PrivateInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
	mStreamSink->SetVideoZoom(zoom);
}

/**
 *   @brief Enable/ Disable Video.
 */
void PrivateInstanceAAMP::SetVideoMute(bool muted)
{
	mStreamSink->SetVideoMute(muted);
}

/**
 *   @brief Enable/ Disable Subtitles.
 *
 *   @param  muted - true to disable subtitles, false to enable subtitles.
 */
void PrivateInstanceAAMP::SetSubtitleMute(bool muted)
{
	mStreamSink->SetSubtitleMute(muted);
}

/**
 *   @brief Set Audio Volume.
 *
 *   @param  volume - Minimum 0, maximum 100.
 */
void PrivateInstanceAAMP::SetAudioVolume(int volume)
{
	mStreamSink->SetAudioVolume(volume);
}

/**
 * @brief abort ongoing downloads and returns error on future downloads
 * called while stopping fragment collector thread
 */
void PrivateInstanceAAMP::DisableDownloads(void)
{
	pthread_mutex_lock(&mLock);
	mDownloadsEnabled = false;
	pthread_cond_broadcast(&mDownloadsDisabled);
	pthread_mutex_unlock(&mLock);
	// Notify playlist downloader threads
	if(mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->DisablePlaylistDownloads();
	}
}

/**
 * @brief Check if track can inject data into GStreamer.
 */
bool PrivateInstanceAAMP::DownloadsAreEnabled(void)
{
	return mDownloadsEnabled; // needs mutex protection?
}

/**
 * @brief Enable downloads after aamp_DisableDownloads.
 * Called after stopping fragment collector thread
 */
void PrivateInstanceAAMP::EnableDownloads()
{
	pthread_mutex_lock(&mLock);
	mDownloadsEnabled = true;
	pthread_mutex_unlock(&mLock);
}

/**
 * @brief Sleep until timeout is reached or interrupted
 */
void PrivateInstanceAAMP::InterruptableMsSleep(int timeInMs)
{
	if (timeInMs > 0)
	{
		struct timespec ts;
		int ret;
		ts = aamp_GetTimespec(timeInMs);
		pthread_mutex_lock(&mLock);
		if (mDownloadsEnabled)
		{
			ret = pthread_cond_timedwait(&mDownloadsDisabled, &mLock, &ts);
			if (0 == ret)
			{
				//AAMPLOG_WARN("sleep interrupted!");
			}
			else if (ETIMEDOUT != ret)
			{
				AAMPLOG_WARN("sleep - condition wait failed %s", strerror(ret));
			}
		}
		pthread_mutex_unlock(&mLock);
	}
}

/**
 * @brief Get asset duration in milliseconds
 */
long long PrivateInstanceAAMP::GetDurationMs()
{
	if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
	{
	 	long long ms = mStreamSink->GetDurationMilliseconds();
	 	durationSeconds = ms/1000.0;
	 	return ms;
	}
	else
	{
		return (long long)(durationSeconds*1000.0);
	}
}

/**
 *   @brief Get asset duration in milliseconds
 *   For VIDEO TAG Based playback, mainly when
 *   aamp is used as plugin
 */
long long PrivateInstanceAAMP::DurationFromStartOfPlaybackMs()
{
	if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
	{
		long long ms = mStreamSink->GetDurationMilliseconds();
		durationSeconds = ms/1000.0;
		return ms;
	}
	else
	{
		if( mIsLive )
		{
			long long ms = (culledSeconds * 1000.0) + (durationSeconds * 1000.0);
			return ms;
		}
		else
		{
			return (long long)(durationSeconds*1000.0);
		}
	}
}

/**
 *   @brief Get current stream position
 */
long long PrivateInstanceAAMP::GetPositionMs()
{
	return (prevPositionMiliseconds!=-1) ? prevPositionMiliseconds : GetPositionMilliseconds();
}

/**
 * @brief Get current stream playback position in milliseconds
 */
long long PrivateInstanceAAMP::GetPositionMilliseconds()
{
	long long positionMiliseconds = seek_pos_seconds != -1 ? seek_pos_seconds * 1000.0 : 0.0;
	if (trickStartUTCMS >= 0)
	{
		//DELIA-39530 - Audio only playback is un-tested. Hence disabled for now
		if (ISCONFIGSET_PRIV(eAAMPConfig_EnableGstPositionQuery) && !ISCONFIGSET_PRIV(eAAMPConfig_AudioOnlyPlayback) && !mAudioOnlyPb)
                {
			positionMiliseconds += mStreamSink->GetPositionMilliseconds();
		}
		else
		{
			long long elapsedTime = aamp_GetCurrentTimeMS() - trickStartUTCMS;
			positionMiliseconds += (((elapsedTime > 1000) ? elapsedTime : 0) * rate);
		}
		if ((-1 != prevPositionMiliseconds) && (AAMP_NORMAL_PLAY_RATE == rate))
		{
			long long diff = positionMiliseconds - prevPositionMiliseconds;

			if ((diff > MAX_DIFF_BETWEEN_PTS_POS_MS) || (diff < 0))
			{
				AAMPLOG_WARN("diff %lld prev-pos-ms %lld current-pos-ms %lld, restore prev-pos as current-pos!!", diff, prevPositionMiliseconds, positionMiliseconds);
				positionMiliseconds = prevPositionMiliseconds;
			}
		}

		if (positionMiliseconds < 0)
		{
			AAMPLOG_WARN("Correcting positionMiliseconds %lld to zero", positionMiliseconds);
			positionMiliseconds = 0;
		}
		else
		{
			if (!mIsLiveStream)
			{
				long long durationMs  = GetDurationMs();
				if(positionMiliseconds > durationMs)
				{
					AAMPLOG_WARN("Correcting positionMiliseconds %lld to duration %lld", positionMiliseconds, durationMs);
					positionMiliseconds = durationMs;
				}
			}
			else
			{
				long long tsbEndMs = GetDurationMs() + (culledSeconds * 1000.0);
				if(positionMiliseconds > tsbEndMs)
				{
					AAMPLOG_WARN("Correcting positionMiliseconds %lld to tsbEndMs %lld", positionMiliseconds, tsbEndMs);
					positionMiliseconds = tsbEndMs;
				}
			}
		}
	}
	prevPositionMiliseconds = positionMiliseconds;
	return positionMiliseconds;
}

/**
 * @brief  API to send audio/video stream into the sink.
 */
void PrivateInstanceAAMP::SendStreamCopy(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
	mStreamSink->SendCopy(mediaType, ptr, len, fpts, fdts, fDuration);
}

/**
 * @brief  API to send audio/video stream into the sink.
 */
void PrivateInstanceAAMP::SendStreamTransfer(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double fDuration, bool initFragment)
{
	mStreamSink->SendTransfer(mediaType, buffer, fpts, fdts, fDuration, initFragment);
}

/**
 * @brief Setting the stream sink
 */
void PrivateInstanceAAMP::SetStreamSink(StreamSink* streamSink)
{
	mStreamSink = streamSink;
}

/**
 * @brief Checking if the stream is live or not
 */
bool PrivateInstanceAAMP::IsLive()
{
	return mIsLive;
}

/**
 * @brief Check if audio playcontext creation skipped for Demuxed HLS file.
 * @retval true if playcontext creation skipped, false if not.
 */
bool PrivateInstanceAAMP::IsAudioPlayContextCreationSkipped()
{
	return mIsAudioContextSkipped;
}

/**
 * @brief Check if stream is live
 */
bool PrivateInstanceAAMP::IsLiveStream()
{
        return mIsLiveStream;
}

/**
 * @brief Stop playback and release resources.
 *
 */
void PrivateInstanceAAMP::Stop()
{
	// Clear all the player events in the queue and sets its state to RELEASED as everything is done
	mEventManager->SetPlayerState(eSTATE_RELEASED);
	mEventManager->FlushPendingEvents();

	pthread_mutex_lock(&gMutex);
	auto iter = std::find_if(std::begin(gActivePrivAAMPs), std::end(gActivePrivAAMPs), [this](const gActivePrivAAMP_t& el)
	{
		return el.pAAMP == this;
	});

	if(iter != gActivePrivAAMPs.end())
	{
		if (iter->reTune && mIsRetuneInProgress)
		{
			// Wait for any ongoing re-tune operation to complete
			pthread_cond_wait(&gCond, &gMutex);
		}
		iter->reTune = false;
	}
	pthread_mutex_unlock(&gMutex);

	if (mAutoResumeTaskPending)
	{
		RemoveAsyncTask(mAutoResumeTaskId);
		mAutoResumeTaskId = AAMP_TASK_ID_INVALID;
		mAutoResumeTaskPending = false;
	}

	DisableDownloads();
	UnblockWaitForDiscontinuityProcessToComplete();

	// Stopping the playback, release all DRM context
	if (mpStreamAbstractionAAMP)
	{
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM)
		if (mDRMSessionManager)
		{
			mDRMSessionManager->setLicenseRequestAbort(true);
		}
#endif
		mpStreamAbstractionAAMP->Stop(true);
		if (HasSidecarData())
		{ // has sidecar data
			mpStreamAbstractionAAMP->ResetSubtitle();
		}
		//Deleting mpStreamAbstractionAAMP here will prevent the extra stop call in TeardownStream()
		//and will avoid enableDownlaod() call being made unnecessarily
		SAFE_DELETE(mpStreamAbstractionAAMP);
	}

	TeardownStream(true);

	for(int i=0; i<eMEDIATYPE_DEFAULT; i++)
	{
		FlushLastId3Data((MediaType)i);
	}
	pthread_mutex_lock(&mEventLock);
	if (mPendingAsyncEvents.size() > 0)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: mPendingAsyncEvents.size - %d", mPendingAsyncEvents.size());
		for (std::map<guint, bool>::iterator it = mPendingAsyncEvents.begin(); it != mPendingAsyncEvents.end(); it++)
		{
			if (it->first != 0)
			{
				if (it->second)
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: remove id - %d", (int) it->first);
					g_source_remove(it->first);
				}
				else
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: Not removing id - %d as not pending", (int) it->first);
				}
			}
		}
		mPendingAsyncEvents.clear();
	}
	pthread_mutex_unlock(&mEventLock);

	// Streamer threads are stopped when we reach here, thread synchronization not required
	if (timedMetadata.size() > 0)
	{
		timedMetadata.clear();
	}
	mFailureReason="";
	seek_pos_seconds = -1;
	prevPositionMiliseconds = -1;
	culledSeconds = 0;
	mIsLiveStream = false;
	durationSeconds = 0;
	mProgressReportOffset = -1;
	rate = 1;
	// Set the state to eSTATE_IDLE
	// directly setting state variable . Calling SetState will trigger event :(
	mState = eSTATE_IDLE;
  
	mSeekOperationInProgress = false;
	mTrickplayInProgress = false;
	mMaxLanguageCount = 0; // reset language count
	//mPreferredAudioTrack = AudioTrackInfo(); // reset
	mPreferredTextTrack = TextTrackInfo(); // reset
	// send signal to any thread waiting for play
	pthread_mutex_lock(&mMutexPlaystart);
	pthread_cond_broadcast(&waitforplaystart);
	pthread_mutex_unlock(&mMutexPlaystart);
	if(mPreCachePlaylistThreadFlag)
	{
		pthread_join(mPreCachePlaylistThreadId,NULL);
		mPreCachePlaylistThreadFlag=false;
		mPreCachePlaylistThreadId = 0;
	}
	getAampCacheHandler()->StopPlaylistCache();


	if (pipeline_paused)
	{
		pipeline_paused = false;
	}

	//temporary hack for peacock
	if (STARTS_WITH_IGNORE_CASE(mAppName.c_str(), "peacock"))
	{
		SAFE_DELETE(mAampCacheHandler);

#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM)
		SAFE_DELETE(mDRMSessionManager);
#endif
	}

	SAFE_DELETE(mCdaiObject);

#if 0
	/* Clear the session data*/
	if(!mSessionToken.empty()){
		mSessionToken.clear();
	}
#endif

	EnableDownloads();
}

/**
 * @brief SaveTimedMetadata Function to store Metadata for bulk reporting during Initialization 
 */
void PrivateInstanceAAMP::SaveTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
	std::string content(szContent, nb);
	timedMetadata.push_back(TimedMetadata(timeMilliseconds, std::string((szName == NULL) ? "" : szName), content, std::string((id == NULL) ? "" : id), durationMS));
}

/**
 * @brief SaveNewTimedMetadata Function to store Metadata and reporting event one by one after DRM Initialization
 */
void PrivateInstanceAAMP::SaveNewTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
	std::string content(szContent, nb);
	timedMetadataNew.push_back(TimedMetadata(timeMilliseconds, std::string((szName == NULL) ? "" : szName), content, std::string((id == NULL) ? "" : id), durationMS));
}

/**
 * @brief Report timed metadata Function to send timedMetadata 
 */
void PrivateInstanceAAMP::ReportTimedMetadata(bool init)
{
	bool bMetadata = ISCONFIGSET_PRIV(eAAMPConfig_BulkTimedMetaReport);
	if(bMetadata && init && IsNewTune())
	{
		ReportBulkTimedMetadata();
	}
	else
	{
		std::vector<TimedMetadata>::iterator iter;
		mTimedMetadataStartTime = NOW_STEADY_TS_MS ;
		for (iter = timedMetadataNew.begin(); iter != timedMetadataNew.end(); iter++)
		{
			ReportTimedMetadata(iter->_timeMS, iter->_name.c_str(), iter->_content.c_str(), iter->_content.size(), init, iter->_id.c_str(), iter->_durationMS);
		}
		timedMetadataNew.clear();
		mTimedMetadataDuration = (NOW_STEADY_TS_MS - mTimedMetadataStartTime);
	}	
}

/**
 * @brief Report bulk timedMetadata Function to send bulk timedMetadata in json format 
 */
void PrivateInstanceAAMP::ReportBulkTimedMetadata()
{
	mTimedMetadataStartTime = NOW_STEADY_TS_MS;
	std::vector<TimedMetadata>::iterator iter;
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableSubscribedTags) && timedMetadata.size())
	{
		AAMPLOG_INFO("Sending bulk Timed Metadata");

		cJSON *root;
		cJSON *item;
		root = cJSON_CreateArray();
		if(root)
		{
			for (iter = timedMetadata.begin(); iter != timedMetadata.end(); iter++)
			{
				cJSON_AddItemToArray(root, item = cJSON_CreateObject());
				cJSON_AddStringToObject(item, "name", iter->_name.c_str());
				cJSON_AddStringToObject(item, "id", iter->_id.c_str());
				cJSON_AddNumberToObject(item, "timeMs", iter->_timeMS);
				cJSON_AddNumberToObject (item, "durationMs",iter->_durationMS);
				cJSON_AddStringToObject(item, "data", iter->_content.c_str());
			}

			char* bulkData = cJSON_PrintUnformatted(root);
			if(bulkData)
			{
				BulkTimedMetadataEventPtr eventData = std::make_shared<BulkTimedMetadataEvent>(std::string(bulkData));
				AAMPLOG_INFO("Sending bulkTimedData");
				if (ISCONFIGSET_PRIV(eAAMPConfig_MetadataLogging))
				{
					AAMPLOG_INFO("bulkTimedData : %s", bulkData);
				}
				// Sending BulkTimedMetaData event as synchronous event.
				// SCTE35 events are async events in TimedMetadata, and this event is sending only from HLS
				mEventManager->SendEvent(eventData);
				free(bulkData);
			}
			cJSON_Delete(root);
		}
		mTimedMetadataDuration = (NOW_STEADY_TS_MS - mTimedMetadataStartTime);
	}
}

/**
 * @brief Report timed metadata Function to send timedMetadata events
 */
void PrivateInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName, const char *szContent, int nb, bool bSyncCall, const char *id, double durationMS)
{
	std::string content(szContent, nb);
	bool bFireEvent = false;

	// Check if timedMetadata was already reported
	std::vector<TimedMetadata>::iterator i;
	bool ignoreMetaAdd = false;

	for (i = timedMetadata.begin(); i != timedMetadata.end(); i++)
	{
		if ((timeMilliseconds >= i->_timeMS-1000 && timeMilliseconds <= i->_timeMS+1000 ))
		{
			if((i->_name.compare(szName) == 0) && (i->_content.compare(content) == 0))
			{
				// Already same exists , ignore
				ignoreMetaAdd = true;
				break;
			}
			else
			{
				continue;
			}
		}
		else if (i->_timeMS < timeMilliseconds)
		{
			// move to next entry
			continue;
		}		
		else if (i->_timeMS > timeMilliseconds)
		{
			break;
		}		
	}

	if(!ignoreMetaAdd) 
	{
		bFireEvent = true;
		if(i == timedMetadata.end())
		{
			// Comes here for
			// 1.No entry in the table
			// 2.Entries available which is only having time < NewMetatime
			timedMetadata.push_back(TimedMetadata(timeMilliseconds, szName, content, id, durationMS));
		}
		else
		{
			// New entry in between saved entries.
			// i->_timeMS >= timeMilliseconds && no similar entry in table
			timedMetadata.insert(i, TimedMetadata(timeMilliseconds, szName, content, id, durationMS));
		}
	}


	if (bFireEvent)
	{
		//DELIA-40019: szContent should not contain any tag name and ":" delimiter. This is not checked in JS event listeners
		TimedMetadataEventPtr eventData = std::make_shared<TimedMetadataEvent>(((szName == NULL) ? "" : szName), ((id == NULL) ? "" : id), timeMilliseconds, durationMS, content);

		if (ISCONFIGSET_PRIV(eAAMPConfig_MetadataLogging))
		{
			AAMPLOG_WARN("aamp timedMetadata: [%ld] '%s'", (long)(timeMilliseconds), content.c_str());
		}

		if (!bSyncCall)
		{
			mEventManager->SendEvent(eventData,AAMP_EVENT_ASYNC_MODE);
		}
		else
		{
			mEventManager->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);
		}
	}
}


/**
 * @brief Report content gap events
 */
void PrivateInstanceAAMP::ReportContentGap(long long timeMilliseconds, std::string id, double durationMS)
{
	bool bFireEvent = false;
	bool ignoreMetaAdd = false;
	// Check if contentGap was already reported
	std::vector<ContentGapInfo>::iterator iter;

	for (iter = contentGaps.begin(); iter != contentGaps.end(); iter++)
	{
		if ((timeMilliseconds >= iter->_timeMS-1000 && timeMilliseconds <= iter->_timeMS+1000 ))
		{
			if(iter->_id == id)
			{
				// Already same exists , ignore if periodGap information is complete.
				if(iter->_complete)
				{
					ignoreMetaAdd = true;
					break;
				}
				else
				{
					if(durationMS >= 0)
					{
						// New request with duration, mark complete and report it.
						iter->_durationMS = durationMS;
						iter->_complete = true;
					}
					else
					{
						// Duplicate report request, already processed
						ignoreMetaAdd = true;
						break;
					}
				}
			}
			else
			{
				continue;
			}
		}
		else if (iter->_timeMS < timeMilliseconds)
		{
			// move to next entry
			continue;
		}
		else if (iter->_timeMS > timeMilliseconds)
		{
			break;
		}
	}

	if(!ignoreMetaAdd)
	{
		bFireEvent = true;
		if(iter == contentGaps.end())
		{
			contentGaps.push_back(ContentGapInfo(timeMilliseconds, id, durationMS));
		}
		else
		{
			contentGaps.insert(iter, ContentGapInfo(timeMilliseconds, id, durationMS));
		}
	}


	if (bFireEvent)
	{
		ContentGapEventPtr eventData = std::make_shared<ContentGapEvent>(timeMilliseconds, durationMS);
		AAMPLOG_INFO("aamp contentGap: start: %lld duration: %ld", timeMilliseconds, (long) durationMS);
		mEventManager->SendEvent(eventData);
	}
}

/**
 *   @brief Initialize CC after first frame received
 *          Sends CC handle event to listeners when first frame receives or video_dec handle rests
 */
void PrivateInstanceAAMP::InitializeCC()
{
#ifdef AAMP_STOP_SINK_ON_SEEK
	/*Do not send event on trickplay as CC is not enabled*/
	if (AAMP_NORMAL_PLAY_RATE != rate)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: not sending cc handle as rate = %f", rate);
		return;
	}
#endif
	if (mStreamSink != NULL)
	{
#ifdef AAMP_CC_ENABLED
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			AampCCManager::GetInstance()->Init((void *)mStreamSink->getCCDecoderHandle());

                        int overrideCfg;
                        GETCONFIGVALUE_PRIV(eAAMPConfig_CEAPreferred,overrideCfg);
                        if (overrideCfg == 0)
                        {
                            AAMPLOG_WARN("PrivateInstanceAAMP: CC format override to 608 present, selecting 608CC");
                            AampCCManager::GetInstance()->SetTrack("CC1");
                        }

		}
		else
#endif
		{
			CCHandleEventPtr event = std::make_shared<CCHandleEvent>(mStreamSink->getCCDecoderHandle());
			mEventManager->SendEvent(event);
		}
	}
}


/**
 *  @brief Notify first frame is displayed. Sends CC handle event to listeners.
 */
void PrivateInstanceAAMP::NotifyFirstFrameReceived()
{
	// In the middle of stop processing we can receive state changing callback (xione-7331)
	PrivAAMPState state;
	GetState(state);
	if (state == eSTATE_IDLE)
	{
		AAMPLOG_WARN( "skipped as in IDLE state" );
		return;
	}
	
	// If mFirstVideoFrameDisplayedEnabled, state will be changed in NotifyFirstVideoDisplayed()
	if(!mFirstVideoFrameDisplayedEnabled)
	{
		SetState(eSTATE_PLAYING);
	}
	pthread_mutex_lock(&mMutexPlaystart);
	pthread_cond_broadcast(&waitforplaystart);
	pthread_mutex_unlock(&mMutexPlaystart);

	if (eTUNED_EVENT_ON_GST_PLAYING == GetTuneEventConfig(IsLive()))
	{
		// This is an idle callback, so we can sent event synchronously
		if (SendTunedEvent())
		{
			AAMPLOG_WARN("aamp: - sent tune event on Tune Completion.");
		}
	}
	InitializeCC();
}

/**
 *   @brief Signal discontinuity of track.
 *   Called from StreamAbstractionAAMP to signal discontinuity
 */
bool PrivateInstanceAAMP::Discontinuity(MediaType track, bool setDiscontinuityFlag)
{
	bool ret;

	if (setDiscontinuityFlag)
	{
		ret = true;
	}
	else
	{
		SyncBegin();
		ret = mStreamSink->Discontinuity(track);
		SyncEnd();
	}

	if (ret)
	{
		mProcessingDiscontinuity[track] = true;
	}
	return ret;
}

/**
 * @brief Schedules retune or discontinuity processing based on state.
 */
void PrivateInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, MediaType trackType)
{
	if (AAMP_NORMAL_PLAY_RATE == rate && ContentType_EAS != mContentType)
	{
		PrivAAMPState state;
		GetState(state);
		if (((state != eSTATE_PLAYING) && (eGST_ERROR_VIDEO_BUFFERING != errorType)) || mSeekOperationInProgress)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Not processing reTune since state = %d, mSeekOperationInProgress = %d",
						state, mSeekOperationInProgress);
			return;
		}

		pthread_mutex_lock(&gMutex);
		if (this->mIsRetuneInProgress)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP:: Already Retune inprogress");
			pthread_mutex_unlock(&gMutex);
			return;
		}
		pthread_mutex_unlock(&gMutex);

		/*If underflow is caused by a discontinuity processing, continue playback from discontinuity*/
		// If discontinuity process in progress, skip further processing
		// DELIA-46559 Since discontinuity flags are reset a bit earlier, additional checks added below to check if discontinuity processing in progress
		pthread_mutex_lock(&mLock);
		if ((errorType != eGST_ERROR_PTS) &&
				(IsDiscontinuityProcessPending() || mDiscontinuityTuneOperationId != 0 || mDiscontinuityTuneOperationInProgress))
		{
			if (mDiscontinuityTuneOperationId != 0 || mDiscontinuityTuneOperationInProgress)
			{
				AAMPLOG_WARN("PrivateInstanceAAMP: Discontinuity Tune handler already spawned(%d) or inprogress(%d)",
					mDiscontinuityTuneOperationId, mDiscontinuityTuneOperationInProgress);
				pthread_mutex_unlock(&mLock);
				return;
			}
			mDiscontinuityTuneOperationId = ScheduleAsyncTask(PrivateInstanceAAMP_ProcessDiscontinuity, (void *)this, "PrivateInstanceAAMP_ProcessDiscontinuity");
			pthread_mutex_unlock(&mLock);

			AAMPLOG_WARN("PrivateInstanceAAMP: Underflow due to discontinuity handled");
			return;
		}

		pthread_mutex_unlock(&mLock);

		if (mpStreamAbstractionAAMP && mpStreamAbstractionAAMP->IsStreamerStalled())
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Ignore reTune due to playback stall");
			return;
		}
		else if (!ISCONFIGSET_PRIV(eAAMPConfig_InternalReTune))
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Ignore reTune as disabled in configuration");
			return;
		}

		MediaTrack* mediaTrack = (mpStreamAbstractionAAMP != NULL) ? (mpStreamAbstractionAAMP->GetMediaTrack((TrackType)trackType)) : NULL;
		
		if((ISCONFIGSET_PRIV(eAAMPConfig_ReportBufferEvent)) &&
		(errorType == eGST_ERROR_UNDERFLOW) &&
		(trackType == eMEDIATYPE_VIDEO) &&
		(mediaTrack) &&
		(mediaTrack->GetBufferStatus() == BUFFER_STATUS_RED))
		{
			SendBufferChangeEvent(true);  // Buffer state changed, buffer Under flow started
			if (!pipeline_paused &&  !PausePipeline(true, true))
			{
					AAMPLOG_ERR("Failed to pause the Pipeline");
			}
		}

		const char* errorString  =  (errorType == eGST_ERROR_PTS) ? "PTS ERROR" :
									(errorType == eGST_ERROR_UNDERFLOW) ? "Underflow" :
									(errorType == eSTALL_AFTER_DISCONTINUITY) ? "Stall After Discontinuity" :
									(errorType == eDASH_LOW_LATENCY_MAX_CORRECTION_REACHED)?"LL DASH Max Correction Reached":
									(errorType == eDASH_LOW_LATENCY_INPUT_PROTECTION_ERROR)?"LL DASH Input Protection Error":
									(errorType == eDASH_RECONFIGURE_FOR_ENC_PERIOD)?"Enrypted period found":
									(errorType == eGST_ERROR_GST_PIPELINE_INTERNAL) ? "GstPipeline Internal Error" : "STARTTIME RESET";

		SendAnomalyEvent(ANOMALY_WARNING, "%s %s", (trackType == eMEDIATYPE_VIDEO ? "VIDEO" : "AUDIO"), errorString);
		bool activeAAMPFound = false;
		pthread_mutex_lock(&gMutex);
		for (std::list<gActivePrivAAMP_t>::iterator iter = gActivePrivAAMPs.begin(); iter != gActivePrivAAMPs.end(); iter++)
		{
			if (this == iter->pAAMP)
			{
				gActivePrivAAMP_t *gAAMPInstance = &(*iter);
				if (gAAMPInstance->reTune)
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: Already scheduled");
				}
				else
				{
					if(eGST_ERROR_PTS == errorType || eGST_ERROR_UNDERFLOW == errorType)
					{
						long long now = aamp_GetCurrentTimeMS();
						long long lastErrorReportedTimeMs = lastUnderFlowTimeMs[trackType];
						int ptsErrorThresholdValue = 0;
						GETCONFIGVALUE_PRIV(eAAMPConfig_PTSErrorThreshold,ptsErrorThresholdValue);
						if (lastErrorReportedTimeMs)
						{
							bool isRetuneRequried = false;
							long long diffMs = (now - lastErrorReportedTimeMs);
							if(GetLLDashServiceData()->lowLatencyMode )
							{
								if (diffMs < AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS)
								{
									isRetuneRequried = true;
								}
							}
							else
							{
								if (diffMs < AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS)
								{
									isRetuneRequried = true;
								}
							}
							if(isRetuneRequried)
							{
								gAAMPInstance->numPtsErrors++;
								if (gAAMPInstance->numPtsErrors >= ptsErrorThresholdValue)
								{
									AAMPLOG_WARN("PrivateInstanceAAMP: numPtsErrors %d, ptsErrorThreshold %d",
									gAAMPInstance->numPtsErrors, ptsErrorThresholdValue);	
									gAAMPInstance->numPtsErrors = 0;
									gAAMPInstance->reTune = true;
									AAMPLOG_WARN("PrivateInstanceAAMP: Schedule Retune. diffMs %lld < threshold %lld",
										diffMs, GetLLDashServiceData()->lowLatencyMode?
										AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS:AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS);
									AdditionalTuneFailLogEntries();
									ScheduleAsyncTask(PrivateInstanceAAMP_Retune, (void *)this, "PrivateInstanceAAMP_Retune");
								}
							}
							else
							{
								gAAMPInstance->numPtsErrors = 0;
								AAMPLOG_WARN("PrivateInstanceAAMP: Not scheduling reTune since (diff %lld > threshold %lld) numPtsErrors %d, ptsErrorThreshold %d.",
									diffMs, GetLLDashServiceData()->lowLatencyMode?
									AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS:AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS,
									gAAMPInstance->numPtsErrors, ptsErrorThresholdValue);
							}
						}
						else
						{
							gAAMPInstance->numPtsErrors = 0;
							AAMPLOG_WARN("PrivateInstanceAAMP: Not scheduling reTune since first %s.", errorString);
						}
						lastUnderFlowTimeMs[trackType] = now;
					}
					else
					{
						AAMPLOG_WARN("PrivateInstanceAAMP: Schedule Retune errorType %d error %s", errorType, errorString);
						gAAMPInstance->reTune = true;
						AdditionalTuneFailLogEntries();
						ScheduleAsyncTask(PrivateInstanceAAMP_Retune, (void *)this, "PrivateInstanceAAMP_Retune");
					}
				}
				activeAAMPFound = true;
				break;
			}
		}
		pthread_mutex_unlock(&gMutex);
		if (!activeAAMPFound)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: %p not in Active AAMP list", this);
		}
	}
}

/**
 * @brief Set player state
 */
void PrivateInstanceAAMP::SetState(PrivAAMPState state)
{
	//bool sentSync = true;

	if (mState == state)
	{ // noop
		return;
	}

	if ( (state == eSTATE_PLAYING || state == eSTATE_BUFFERING || state == eSTATE_PAUSED)
		&& mState == eSTATE_SEEKING && (mEventManager->IsEventListenerAvailable(AAMP_EVENT_SEEKED)))
	{
		SeekedEventPtr event = std::make_shared<SeekedEvent>(GetPositionMilliseconds());
		mEventManager->SendEvent(event,AAMP_EVENT_SYNC_MODE);

	}

	pthread_mutex_lock(&mLock);
	mState = state;
	pthread_mutex_unlock(&mLock);

	mScheduler->SetState(mState);
	if (mEventManager->IsEventListenerAvailable(AAMP_EVENT_STATE_CHANGED))
	{
		if (mState == eSTATE_PREPARING)
		{
			StateChangedEventPtr eventData = std::make_shared<StateChangedEvent>(eSTATE_INITIALIZED);
			mEventManager->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);
		}

		StateChangedEventPtr eventData = std::make_shared<StateChangedEvent>(mState);
		mEventManager->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);
	}
}

/**
 * @brief Get player state
 */
void PrivateInstanceAAMP::GetState(PrivAAMPState& state)
{
	pthread_mutex_lock(&mLock);
	state = mState;
	pthread_mutex_unlock(&mLock);
}

/**
 *  @brief Add high priority idle task to the gstreamer
 *  @note task shall return 0 to be removed, 1 to be repeated
 */
gint PrivateInstanceAAMP::AddHighIdleTask(IdleTask task, void* arg,DestroyTask dtask)
{
	gint callbackID = g_idle_add_full(G_PRIORITY_HIGH_IDLE, task, (gpointer)arg, dtask);
	return callbackID;
}

/**
 *   @brief Check sink cache empty
 */
bool PrivateInstanceAAMP::IsSinkCacheEmpty(MediaType mediaType)
{
	return mStreamSink->IsCacheEmpty(mediaType);
}

/**
 * @brief Reset EOS SignalledFlag
 */
void PrivateInstanceAAMP::ResetEOSSignalledFlag()
{
	return mStreamSink->ResetEOSSignalledFlag();
}

/**
 * @brief Notify fragment caching complete
 */
void PrivateInstanceAAMP::NotifyFragmentCachingComplete()
{
	pthread_mutex_lock(&mFragmentCachingLock);
	mFragmentCachingRequired = false;
	mStreamSink->NotifyFragmentCachingComplete();
	PrivAAMPState state;
	GetState(state);
	if (state == eSTATE_BUFFERING)
	{
		if(mpStreamAbstractionAAMP)
		{
			mpStreamAbstractionAAMP->NotifyPlaybackPaused(false);
		}
		SetState(eSTATE_PLAYING);
	}
	pthread_mutex_unlock(&mFragmentCachingLock);
}

/**
 * @brief Send tuned event to listeners if required
 */
bool PrivateInstanceAAMP::SendTunedEvent(bool isSynchronous)
{
	bool ret = false;

	// Required for synchronising btw audio and video tracks in case of cdmidecryptor
	pthread_mutex_lock(&mLock);

	ret = mTunedEventPending;
	mTunedEventPending = false;

	pthread_mutex_unlock(&mLock);

	if(ret)
	{
		AAMPEventPtr ev = std::make_shared<AAMPEventObject>(AAMP_EVENT_TUNED);
		mEventManager->SendEvent(ev , AAMP_EVENT_SYNC_MODE);
	}
	return ret;
}

/**
 *   @brief Send VideoEndEvent
 */
bool PrivateInstanceAAMP::SendVideoEndEvent()
{
	bool ret = false;
#ifdef SESSION_STATS
	char * strVideoEndJson = NULL;
	// Required for protecting mVideoEnd object
	pthread_mutex_lock(&mLock);
	if(mVideoEnd)
	{
		//Update VideoEnd Data
		if(mTimeAtTopProfile > 0)
		{
			// Losing milisecons of data in conversion from double to long
			mVideoEnd->SetTimeAtTopProfile(mTimeAtTopProfile);
			mVideoEnd->SetTimeToTopProfile(mTimeToTopProfile);
		}
		mVideoEnd->SetTotalDuration(mPlaybackDuration);

		// re initialize for next tune collection
		mTimeToTopProfile = 0;
		mTimeAtTopProfile = 0;
		mPlaybackDuration = 0;
		mCurrentLanguageIndex = 0;

		//Memory of this string will be deleted after sending event by destructor of AsyncMetricsEventDescriptor
		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableVideoEndEvent) && mEventManager->IsEventListenerAvailable(AAMP_EVENT_REPORT_METRICS_DATA))
		{
			if(mTSBEnabled)
			{
				char* data = GetOnVideoEndSessionStatData();
				if(data)
				{
					AAMPLOG_INFO("TsbSessionEnd:%s", data);
					strVideoEndJson = mVideoEnd->ToJsonString(data);
					cJSON_free( data );
					data = NULL;
				}
			}
			else
			{
				strVideoEndJson = mVideoEnd->ToJsonString();
			}
		}
		SAFE_DELETE(mVideoEnd);
	}
	mVideoEnd = new CVideoStat(mMediaFormatName[mMediaFormat]);
	mVideoEnd->SetDisplayResolution(mDisplayWidth,mDisplayHeight);
	pthread_mutex_unlock(&mLock);

	if(strVideoEndJson)
	{
		AAMPLOG_INFO("VideoEnd:%s", strVideoEndJson);
		MetricsDataEventPtr e = std::make_shared<MetricsDataEvent>(MetricsDataType::AAMP_DATA_VIDEO_END, this->mTraceUUID, strVideoEndJson);
		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		free(strVideoEndJson);
		ret = true;
	}
#endif
	return ret;
}

/**
 * @brief updates profile Resolution to VideoStat object
 */
void PrivateInstanceAAMP::UpdateVideoEndProfileResolution(MediaType mediaType, long bitrate, int width, int height)
{
#ifdef SESSION_STATS
	pthread_mutex_lock(&mLock);
	if(mVideoEnd)
	{
		VideoStatTrackType trackType = VideoStatTrackType::STAT_VIDEO;
		if(mediaType == eMEDIATYPE_IFRAME)
		{
			trackType = VideoStatTrackType::STAT_IFRAME;
		}
		mVideoEnd->SetProfileResolution(trackType,bitrate,width,height);
	}
	pthread_mutex_unlock(&mLock);
#endif
}

/**
 *  @brief updates download metrics to VideoStat object, this is used for VideoFragment as it takes duration for calcuation purpose.
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(MediaType mediaType, long bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime)
{
    UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl,duration,curlDownloadTime, false,false);
}

/**
 *   @brief updates time shift buffer status
 *
 */
void PrivateInstanceAAMP::UpdateVideoEndTsbStatus(bool btsbAvailable)
{
#ifdef SESSION_STATS
	pthread_mutex_lock(&mLock);
	if(mVideoEnd)
	{
		
		mVideoEnd->SetTsbStatus(btsbAvailable);
	}
	pthread_mutex_unlock(&mLock);
#endif
}   

/**
 * @brief updates profile capped status
 */
void PrivateInstanceAAMP::UpdateProfileCappedStatus(void)
{
#ifdef SESSION_STATS
	pthread_mutex_lock(&mLock);
	if(mVideoEnd)
	{
		mVideoEnd->SetProfileCappedStatus(mProfileCappedStatus);
	}
	pthread_mutex_unlock(&mLock);
#endif
}

/**
 * @brief updates download metrics to VideoStat object, this is used for VideoFragment as it takes duration for calcuation purpose.
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(MediaType mediaType, long bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime, bool keyChanged, bool isEncrypted, ManifestData * manifestData)
{
#ifdef SESSION_STATS
	int audioIndex = 1;
	// ignore for write and aborted errors
	// these are generated after trick play options,
	if( curlOrHTTPCode > 0 &&  !(curlOrHTTPCode == CURLE_ABORTED_BY_CALLBACK || curlOrHTTPCode == CURLE_WRITE_ERROR) )
	{
		VideoStatDataType dataType = VideoStatDataType::VE_DATA_UNKNOWN;
		
		VideoStatTrackType trackType = VideoStatTrackType::STAT_UNKNOWN;
		VideoStatCountType eCountType = VideoStatCountType::COUNT_UNKNOWN;
		
		/*	COUNT_UNKNOWN,
		 COUNT_LIC_TOTAL,
		 COUNT_LIC_ENC_TO_CLR,
		 COUNT_LIC_CLR_TO_ENC,
		 COUNT_STALL,
		 */
		
		switch(mediaType)
		{
			case eMEDIATYPE_MANIFEST:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_MAIN;
			}
				break;
				
			case eMEDIATYPE_PLAYLIST_VIDEO:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_VIDEO;
			}
				break;
				
			case eMEDIATYPE_PLAYLIST_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
				
			case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
				
			case eMEDIATYPE_PLAYLIST_IFRAME:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = STAT_IFRAME;
			}
				break;
				
			case eMEDIATYPE_VIDEO:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_VIDEO;
				// always Video fragment will be from same thread so mutex required
				
				// !!!!!!!!!! To Do : Support this stats for Audio Only streams !!!!!!!!!!!!!!!!!!!!!
				//Is success
				if (((curlOrHTTPCode == 200) || (curlOrHTTPCode == 206))  && duration > 0)
				{
					if(mpStreamAbstractionAAMP->GetProfileCount())
					{
						long maxBitrateSupported = mpStreamAbstractionAAMP->GetMaxBitrate();
						if(maxBitrateSupported == bitrate)
						{
							mTimeAtTopProfile += duration;
							
						}
						
					}
					if(mTimeAtTopProfile == 0) // we havent achived top profile yet
					{
						mTimeToTopProfile += duration; // started at top profile
					}
					
					mPlaybackDuration += duration;
				}
				
			}
				break;
			case eMEDIATYPE_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
			case eMEDIATYPE_AUX_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
			case eMEDIATYPE_IFRAME:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_IFRAME;
			}
				break;
				
			case eMEDIATYPE_INIT_IFRAME:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_IFRAME;
			}
				break;
				
			case eMEDIATYPE_INIT_VIDEO:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_VIDEO;
			}
				break;
				
			case eMEDIATYPE_INIT_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
				
			case eMEDIATYPE_INIT_AUX_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
				
			case eMEDIATYPE_SUBTITLE:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_SUBTITLE;
			}
				break;
				
			default:
				break;
		}
		
		
		// Required for protecting mVideoStat object
		if( dataType != VideoStatDataType::VE_DATA_UNKNOWN
		   && trackType != VideoStatTrackType::STAT_UNKNOWN)
		{
			pthread_mutex_lock(&mLock);
			if(mVideoEnd)
			{
				//curl download time is in seconds, convert it into milliseconds for video end metrics
				mVideoEnd->Increment_Data(dataType,trackType,bitrate,curlDownloadTime * 1000,curlOrHTTPCode,false,audioIndex, manifestData);
				if((curlOrHTTPCode != 200) && (curlOrHTTPCode != 206) && strUrl.c_str())
				{
					//set failure url
					mVideoEnd->SetFailedFragmentUrl(trackType,bitrate,strUrl);
				}
				if(dataType == VideoStatDataType::VE_DATA_FRAGMENT)
				{
					mVideoEnd->Record_License_EncryptionStat(trackType,isEncrypted,keyChanged);
				}
			}
			pthread_mutex_unlock(&mLock);
			
		}
		else
		{
			AAMPLOG_INFO("PrivateInstanceAAMP: Could Not update VideoEnd Event dataType:%d trackType:%d response:%d",
						 dataType,trackType,curlOrHTTPCode);
		}
	}
#endif
}

/**
 * @brief updates abr metrics to VideoStat object,
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo & info)
{
#ifdef SESSION_STATS
	//only for Ramp down case
	if(info.desiredProfileIndex < info.currentProfileIndex)
	{
		AAMPLOG_INFO("UpdateVideoEnd:abrinfo currIdx:%d desiredIdx:%d for:%d",  info.currentProfileIndex,info.desiredProfileIndex,info.abrCalledFor);
		
		if(info.abrCalledFor == AAMPAbrType::AAMPAbrBandwidthUpdate)
		{
			pthread_mutex_lock(&mLock);
			if(mVideoEnd)
			{
				mVideoEnd->Increment_NetworkDropCount();
			}
			pthread_mutex_unlock(&mLock);
		}
		else if (info.abrCalledFor == AAMPAbrType::AAMPAbrFragmentDownloadFailed
				 || info.abrCalledFor == AAMPAbrType::AAMPAbrFragmentDownloadFailed)
		{
			pthread_mutex_lock(&mLock);
			if(mVideoEnd)
			{
				mVideoEnd->Increment_ErrorDropCount();
			}
			pthread_mutex_unlock(&mLock);
		}
	}
#endif
}

/**
 * @brief updates download metrics to VideoStat object, this is used for VideoFragment as it takes duration for calcuation purpose.
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(MediaType mediaType, long bitrate, int curlOrHTTPCode, std::string& strUrl, double curlDownloadTime, ManifestData * manifestData )
{
	UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl,0,curlDownloadTime, false, false, manifestData);
}

/**
 *   @brief Check if fragment caching is required
 */
bool PrivateInstanceAAMP::IsFragmentCachingRequired()
{
	//Prevent enabling Fragment Caching during Seek While Pause
	return (!mPauseOnFirstVideoFrameDisp && mFragmentCachingRequired);
}

/**
 * @brief Get player video size
 */
void PrivateInstanceAAMP::GetPlayerVideoSize(int &width, int &height)
{
	mStreamSink->GetVideoSize(width, height);
}

/**
 * @brief Set an idle callback as event dispatched state
 */
void PrivateInstanceAAMP::SetCallbackAsDispatched(guint id)
{
	pthread_mutex_lock(&mEventLock);
	std::map<guint, bool>::iterator  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		assert (itr->second);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		AAMPLOG_WARN("id not in mPendingAsyncEvents, insert and mark as not pending", id);
		mPendingAsyncEvents[id] = false;
	}
	pthread_mutex_unlock(&mEventLock);
}

/**
 *   @brief Set an idle callback as event pending state
 */
void PrivateInstanceAAMP::SetCallbackAsPending(guint id)
{
	pthread_mutex_lock(&mEventLock);
	std::map<guint, bool>::iterator  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		assert (!itr->second);
		AAMPLOG_WARN("id already in mPendingAsyncEvents and completed, erase it", id);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		mPendingAsyncEvents[id] = true;
	}
	pthread_mutex_unlock(&mEventLock);
}

/**
 * @brief Collect all key-value pairs for CMCD headers.
 */
void PrivateInstanceAAMP::BuildCMCDCustomHeaders(MediaType fileType,std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	MediaType simType = fileType;
	std::string headerName;
	long temp=0;
	std::vector<std::string> headerValue;
	std::string delimiter = ",";
	std::string buffer;
	std::vector<long> bitrateList;
	bool vBufferStarvation = false;
	bool aBufferStarvation = false;
	buffer = CMCD_SESSIONID+this->mTraceUUID;
	headerValue.push_back(buffer);
	mCMCDCustomHeaders["CMCD-Session:"] = headerValue;
	headerValue.clear();
	buffer.clear();
	//For manifest sessionid and object type is passed as a part of CMCD Headers
	if(simType == eMEDIATYPE_MANIFEST)
	{
		headerName="m";
		buffer = CMCD_OBJECT+headerName;
		headerValue.push_back(buffer);
		mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
	}
	//For video sessionid,object type,currentvideobitrate,maximum videobitrate,bufferlength are send as a part of CMCD Headers
	if(simType == eMEDIATYPE_VIDEO)
	{
		headerName="v";
		MediaTrack *video = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_VIDEO);
		if(video && video->enabled)
		{
			if(video->GetBufferStatus() == BUFFER_STATUS_RED)//Bufferstarvation is send only when buffering turns red
			{
				vBufferStarvation = true;
				if(vBufferStarvation)
				{
					headerValue.push_back("bs");
					mCMCDCustomHeaders["CMCD-Status:"] = headerValue;
				}
			}
			int videoBufferLength = ((int)video->GetBufferedDuration())*1000;
			int videoBitrate  = ((int)mpStreamAbstractionAAMP->GetVideoBitrate())/1000;
			bitrateList = mpStreamAbstractionAAMP->GetVideoBitrates();
			for(int i = 0; i < bitrateList.size(); i++)
			{
				if(bitrateList[i]>temp)
				{
					temp=bitrateList[i];
				}
			}
			headerValue.clear();
			buffer.clear();
			buffer = CMCD_BITRATE+to_string(videoBitrate)+delimiter+CMCD_OBJECT+headerName+delimiter+CMCD_TOP_BITRATE+to_string((temp/1000));
			headerValue.push_back(buffer);
			mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
			headerValue.clear();
			buffer.clear();
			AAMPLOG_INFO("video bufferlength %d video bitrate %d",videoBufferLength,videoBitrate);
			buffer = CMCD_BUFFERLENGTH+to_string(videoBufferLength)+delimiter+CMCD_NEXTOBJECTREQUEST+mCMCDNextObjectRequest;
			headerValue.push_back(buffer);
			mCMCDCustomHeaders["CMCD-Request:"] = headerValue;
		}
	}
	//For audio sessionid,object type,currentaudiobitrate,maximum audiobitrate,bufferlength are send as a part of CMCD Headers
	if(simType == eMEDIATYPE_AUDIO)
	{
		headerName="a";
    		MediaTrack *audio = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_AUDIO);
    		if(audio && audio->enabled)
    		{
			if(audio->GetBufferStatus() == BUFFER_STATUS_RED)//Bufferstarvation is send only when buffering turns red
			{
				aBufferStarvation = true;
				if(aBufferStarvation)
				{
					headerValue.push_back("bs");
					mCMCDCustomHeaders["CMCD-Status:"] = headerValue;
				}
			}
    		}
		bitrateList = mpStreamAbstractionAAMP->GetAudioBitrates();
		for(int i = 0; i < bitrateList.size(); i++)
		{
			if(bitrateList[i]>temp)
			{
				temp=bitrateList[i];
			}
		}
		headerValue.clear();
		buffer.clear();
		int audioBufferLength = (int)mpStreamAbstractionAAMP->GetBufferedDuration();
		buffer = CMCD_BITRATE+to_string((mCMCDBandwidth/1000))+delimiter+CMCD_OBJECT+headerName+delimiter+CMCD_TOP_BITRATE+to_string((temp/1000));
		headerValue.push_back(buffer);
		mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
		headerValue.clear();
		buffer.clear();
		AAMPLOG_INFO("audio bufferlength %daudio bitrate %d",audioBufferLength*1000,((int)mCMCDBandwidth)/1000);
		buffer = CMCD_BUFFERLENGTH+to_string(audioBufferLength*1000)+delimiter+CMCD_NEXTOBJECTREQUEST+mCMCDNextObjectRequest;
		headerValue.push_back(buffer);
		mCMCDCustomHeaders["CMCD-Request:"] = headerValue;
	}
	//For subtitle sessionid and object type are send as a part of CMCD Headers
	if(simType == eMEDIATYPE_SUBTITLE)
	{
		headerName="s";
		buffer.clear();
		headerValue.clear();
		buffer = CMCD_OBJECT+headerName;
		headerValue.push_back(buffer);
		mCMCDCustomHeaders["CMCD-Object:"] = headerValue;

	}
	//For init fragment sessionid,object type,bitrate,maximum bitrate are send as a part of CMCD Headers
	if( (simType == eMEDIATYPE_INIT_VIDEO) || (simType == eMEDIATYPE_INIT_AUDIO))
	{
		headerName="i";
		if(simType == eMEDIATYPE_INIT_VIDEO)
		{
			int initVideoBitrate  = ((int)mpStreamAbstractionAAMP->GetVideoBitrate())/1000;
			bitrateList = mpStreamAbstractionAAMP->GetVideoBitrates();
			for(int i = 0; i < bitrateList.size(); i++)
			{
				if(bitrateList[i]>temp)
				{
					temp=bitrateList[i];
				}
			}
			headerValue.clear();
			buffer.clear();
			buffer = CMCD_BITRATE+to_string(initVideoBitrate)+delimiter+CMCD_OBJECT+headerName+delimiter+CMCD_TOP_BITRATE+to_string((temp/1000));
			headerValue.push_back(buffer);
			mCMCDCustomHeaders["CMCD-Object:"] = headerValue;

		}
		if(simType == eMEDIATYPE_INIT_AUDIO)
		{
			bitrateList = mpStreamAbstractionAAMP->GetAudioBitrates();
			for(int i = 0; i < bitrateList.size(); i++)
			{
				if(bitrateList[i]>temp)
				{
					temp=bitrateList[i];
				}
			}
			headerValue.clear();
			buffer.clear();
			buffer = CMCD_BITRATE+to_string((mCMCDBandwidth)/1000)+delimiter+CMCD_OBJECT+headerName+delimiter+CMCD_TOP_BITRATE+to_string((temp/1000));
			headerValue.push_back(buffer);
			mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
		}
	}
	//For muxed streams sessionid,object type,bitrate,maximum bitrate,bufferlength are send as a part of CMCD Headers
	if(mpStreamAbstractionAAMP->IsMuxedStream())
	{
		headerName="av";
		bitrateList = mpStreamAbstractionAAMP->GetVideoBitrates();
		for(int i = 0; i < bitrateList.size(); i++)
		{
			if(bitrateList[i]>temp)
			{
				temp=bitrateList[i];
			}
		}
		int muxedBitrate  = ((int)mpStreamAbstractionAAMP->GetVideoBitrate())/1000;
		headerValue.clear();
		buffer.clear();
		buffer = CMCD_BITRATE+to_string(muxedBitrate)+delimiter+CMCD_OBJECT+headerName+delimiter+CMCD_TOP_BITRATE+to_string((temp/1000));
		headerValue.push_back(buffer);
		mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
		headerValue.clear();
		buffer.clear();
    		MediaTrack *video = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_VIDEO);
    		if(video)
		{
			if(video->GetBufferStatus() == BUFFER_STATUS_RED)///Bufferstarvation is send only when buffering turns red
			{
				vBufferStarvation = true;
				if(vBufferStarvation)
				{
					headerValue.push_back("bs");
					mCMCDCustomHeaders["CMCD-Status:"] = headerValue;
				}
			}
		}
		headerValue.clear();
		buffer.clear();
		int muxedBufferLength = ((int)mpStreamAbstractionAAMP->GetBufferedDuration())*1000;
		AAMPLOG_INFO("muxed bufferlength %d muxed bitrate %d",muxedBufferLength,muxedBitrate);
		buffer = CMCD_BUFFERLENGTH+to_string(muxedBufferLength)+delimiter+CMCD_NEXTOBJECTREQUEST+mCMCDNextObjectRequest;
		headerValue.push_back(buffer);
		mCMCDCustomHeaders["CMCD-Request:"] = headerValue;
	}
	headerValue.clear();
}

/**
 * @brief Add/Remove a custom HTTP header and value.
 */
void PrivateInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
	// Header name should be ending with :
	if(headerName.back() != ':')
	{
		headerName += ':';
	}

	if (isLicenseHeader)
	{
		if (headerValue.size() != 0)
		{
			mCustomLicenseHeaders[headerName] = headerValue;
		}
		else
		{
			mCustomLicenseHeaders.erase(headerName);
		}
	}
	else
	{
		if (headerValue.size() != 0)
		{
			mCustomHeaders[headerName] = headerValue;
		}
		else
		{
			mCustomHeaders.erase(headerName);
		}
	}
}

/**
 *  @brief UpdateLiveOffset live offset [Sec]
 */
void PrivateInstanceAAMP::UpdateLiveOffset()
{
	if(!IsLiveAdjustRequired()) /* Ideally checking the content is either "ivod/cdvr" to adjust the liveoffset on trickplay. */
	{
		GETCONFIGVALUE_PRIV(eAAMPConfig_CDVRLiveOffset,mLiveOffset);
	}
	else
	{
		GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffset,mLiveOffset);
	}
}

/**
 * @brief Send stalled event to listeners
 */
void PrivateInstanceAAMP::SendStalledErrorEvent()
{
	char description[MAX_ERROR_DESCRIPTION_LENGTH];
	memset(description, '\0', MAX_ERROR_DESCRIPTION_LENGTH);
	int stalltimeout;
	GETCONFIGVALUE_PRIV(eAAMPConfig_StallTimeoutMS,stalltimeout);
	snprintf(description, (MAX_ERROR_DESCRIPTION_LENGTH - 1), "Playback has been stalled for more than %d ms due to lack of new fragments", stalltimeout);
	SendErrorEvent(AAMP_TUNE_PLAYBACK_STALLED, description);
}

/**
 * @brief Sets up the timestamp sync for subtitle renderer
 */
void PrivateInstanceAAMP::UpdateSubtitleTimestamp()
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->StartSubtitleParser();
	}
}

/**
 * @brief pause/un-pause subtitles
 * 
 */
void PrivateInstanceAAMP::PauseSubtitleParser(bool pause)
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->PauseSubtitleParser(pause);
	}
}

/**
 * @brief Notify if first buffer processed by gstreamer
 */
void PrivateInstanceAAMP::NotifyFirstBufferProcessed()
{
	// If mFirstVideoFrameDisplayedEnabled, state will be changed in NotifyFirstVideoDisplayed()
	PrivAAMPState state;
	GetState(state);
	
	// In the middle of stop processing we can receive state changing callback (xione-7331)
	if (state == eSTATE_IDLE)
	{
		AAMPLOG_WARN( "skipped as in IDLE state" );
		return;
	}
	
	if (!mFirstVideoFrameDisplayedEnabled
			&& state == eSTATE_SEEKING)
	{
		SetState(eSTATE_PLAYING);
	}
	trickStartUTCMS = aamp_GetCurrentTimeMS();
	AAMPLOG_WARN("seek pos %.3f", seek_pos_seconds);


#ifdef USE_SECMANAGER
	if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager))
	{
		//Thread to call setplayback speed as it needs a 500ms delay here
		std::thread t([&](){
			mDRMSessionManager->setPlaybackSpeedState(rate,seek_pos_seconds, true);});
		t.detach();
		int x,y,w,h;
		sscanf(mStreamSink->GetVideoRectangle().c_str(),"%d,%d,%d,%d",&x,&y,&w,&h);
        AAMPLOG_WARN("calling setVideoWindowSize  w:%d x h:%d ",w,h);
        mDRMSessionManager->setVideoWindowSize(w,h);
	}
#endif
	
}

/**
 * @brief Reset trick start position
 */
void PrivateInstanceAAMP::ResetTrickStartUTCTime()
{
	trickStartUTCMS = aamp_GetCurrentTimeMS();
}

/**
 * @brief Get stream type
 */
int PrivateInstanceAAMP::getStreamType()
{

	int type = 0;

	if(mMediaFormat == eMEDIAFORMAT_DASH)
	{
		type = 20;
	}
	else if( mMediaFormat == eMEDIAFORMAT_HLS)
	{
		type = 10;
	}
	else if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)// eMEDIAFORMAT_PROGRESSIVE
	{
		type = 30;
	}
	else if (mMediaFormat == eMEDIAFORMAT_HLS_MP4)
	{
		type = 40;
	}
	else
	{
		type = 0;
	}

	if (mCurrentDrm != nullptr) {
		type += mCurrentDrm->getDrmCodecType();
	}
	return type;
}

/**
 * @brief Get Mediaformat type
 *
 * @returns eMEDIAFORMAT
 */
MediaFormat PrivateInstanceAAMP::GetMediaFormatTypeEnum() const
{
        return mMediaFormat;
}

#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)

/**
 * @brief Extracts / Generates MoneyTrace string
 */
void PrivateInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
{
	char moneytracebuf[512];
	memset(moneytracebuf, 0, sizeof(moneytracebuf));

	if (mCustomHeaders.size() > 0)
	{
		for (std::unordered_map<std::string, std::vector<std::string>>::const_iterator it = mCustomHeaders.begin();
			it != mCustomHeaders.end(); it++)
		{
			if (it->first.compare("X-MoneyTrace:") == 0)
			{
				if (it->second.size() >= 2)
				{
					snprintf(moneytracebuf, sizeof(moneytracebuf), "trace-id=%s;parent-id=%s;span-id=%lld",
					(const char*)it->second.at(0).c_str(),
					(const char*)it->second.at(1).c_str(),
					aamp_GetCurrentTimeMS());
				}
				else if (it->second.size() == 1)
				{
					snprintf(moneytracebuf, sizeof(moneytracebuf), "trace-id=%s;parent-id=%lld;span-id=%lld",
						(const char*)it->second.at(0).c_str(),
						aamp_GetCurrentTimeMS(),
						aamp_GetCurrentTimeMS());
				}
				customHeader.append(moneytracebuf);
				break;
			}
		}
	}
	// No money trace is available in customheader from JS , create a new moneytrace locally
	if(customHeader.size() == 0)
	{
		// No Moneytrace info available in tune data 
		AAMPLOG_WARN("No Moneytrace info available in tune request,need to generate one");
		uuid_t uuid;
		uuid_generate(uuid);
		char uuidstr[128];
		uuid_unparse(uuid, uuidstr);
		for (char *ptr = uuidstr; *ptr; ++ptr) {
			*ptr = tolower(*ptr);
		}
		snprintf(moneytracebuf,sizeof(moneytracebuf),"trace-id=%s;parent-id=%lld;span-id=%lld",uuidstr,aamp_GetCurrentTimeMS(),aamp_GetCurrentTimeMS());
		customHeader.append(moneytracebuf);
	}	
	AAMPLOG_TRACE("[GetMoneyTraceString] MoneyTrace[%s]",customHeader.c_str());
}
#endif /* USE_SECCLIENT || USE_SECMANAGER */

/**
 * @brief Notify the decryption completion of the fist fragment.
 */
void PrivateInstanceAAMP::NotifyFirstFragmentDecrypted()
{
	if(mTunedEventPending)
	{
		if (eTUNED_EVENT_ON_FIRST_FRAGMENT_DECRYPTED == GetTuneEventConfig(IsLive()))
		{
			// For HLS - This is invoked by fetcher thread, so we have to sent asynchronously
			if (SendTunedEvent(false))
			{
				AAMPLOG_WARN("aamp: %s - sent tune event after first fragment fetch and decrypt", mMediaFormatName[mMediaFormat]);
			}
		}
	}
}

/**
 * @brief  Get PTS of first sample.
 */
double PrivateInstanceAAMP::GetFirstPTS()
{
	assert(NULL != mpStreamAbstractionAAMP);
	return mpStreamAbstractionAAMP->GetFirstPTS();
}

/**
 * @brief Check if Live Adjust is required for current content. ( For "vod/ivod/ip-dvr/cdvr/eas", Live Adjust is not required ).
 */
bool PrivateInstanceAAMP::IsLiveAdjustRequired()
{
	bool retValue;

	switch (mContentType)
	{
		case ContentType_IVOD:
		case ContentType_VOD:
		case ContentType_CDVR:
		case ContentType_IPDVR:
		case ContentType_EAS:
			retValue = false;
			break;

		case ContentType_SLE:
			retValue = true;
			break;

		default:
			retValue = true;
			break;
	}
	return retValue;
}

/**
 * @brief Generate http header response event
 */
void PrivateInstanceAAMP::SendHTTPHeaderResponse()
{
	for (auto const& pair: httpHeaderResponses) {
		HTTPResponseHeaderEventPtr event = std::make_shared<HTTPResponseHeaderEvent>(pair.first.c_str(), pair.second);
		AAMPLOG_INFO("HTTPResponseHeader evt Header:%s Response:%s", event->getHeader().c_str(), event->getResponse().c_str());
		SendEvent(event,AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 * @brief  Generate media metadata event based on parsed attribute values.
 *
 */
void PrivateInstanceAAMP::SendMediaMetadataEvent(void)
{
	std::vector<long> bitrateList;
	std::set<std::string> langList;
	std::vector<float> supportedPlaybackSpeeds { -64, -32, -16, -4, -1, 0, 0.5, 1, 4, 16, 32, 64 };
	int langCount = 0;
	int bitrateCount = 0;
	int supportedSpeedCount = 0;
	int width  = 1280;
	int height = 720;

	bitrateList = mpStreamAbstractionAAMP->GetVideoBitrates();
	for (int i = 0; i <mMaxLanguageCount; i++)
	{
		langList.insert(mLanguageList[i]);
	}

	GetPlayerVideoSize(width, height);

	std::string drmType = "NONE";
	std::shared_ptr<AampDrmHelper> helper = GetCurrentDRM();
	if (helper)
	{
		drmType = helper->friendlyName();
	}

	MediaMetadataEventPtr event = std::make_shared<MediaMetadataEvent>(CONVERT_SEC_TO_MS(durationSeconds), width, height, mpStreamAbstractionAAMP->hasDrm, IsLive(), drmType, mpStreamAbstractionAAMP->mProgramStartTime);

	for (auto iter = langList.begin(); iter != langList.end(); iter++)
	{
		if (!iter->empty())
		{
			// assert if size >= < MAX_LANGUAGE_TAG_LENGTH 
			assert(iter->size() < MAX_LANGUAGE_TAG_LENGTH);
			event->addLanguage((*iter));
		}
	}

	for (int i = 0; i < bitrateList.size(); i++)
	{
		event->addBitrate(bitrateList[i]);
	}

	//Iframe track present and hence playbackRate change is supported
	if (mIsIframeTrackPresent)
	{
		if(!(ISCONFIGSET_PRIV(eAAMPConfig_EnableSlowMotion)))
		{
			auto position = std::find(supportedPlaybackSpeeds.begin(), supportedPlaybackSpeeds.end(), 0.5);
			if(position != supportedPlaybackSpeeds.end())
			{
				supportedPlaybackSpeeds.erase(position); //remove 0.5 from supported speeds
			}
		}

		for(int i = 0; i < supportedPlaybackSpeeds.size(); i++)
		{
			event->addSupportedSpeed(supportedPlaybackSpeeds[i]);
		}
	}
	else
	{
		//Supports only pause and play
		event->addSupportedSpeed(0);
		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableSlowMotion))
		{
			event->addSupportedSpeed(0.5);
		}
		event->addSupportedSpeed(1);
	}

	event->setMediaFormat(mMediaFormatName[mMediaFormat]);

	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief  Generate supported speeds changed event based on arg passed.
 */
void PrivateInstanceAAMP::SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent)
{
	SupportedSpeedsChangedEventPtr event = std::make_shared<SupportedSpeedsChangedEvent>();
	std::vector<float> supportedPlaybackSpeeds { -64, -32, -16, -4, -1, 0, 0.5, 1, 4, 16, 32, 64 };
	int supportedSpeedCount = 0;

	//Iframe track present and hence playbackRate change is supported
	if (isIframeTrackPresent)
	{
		if(!(ISCONFIGSET_PRIV(eAAMPConfig_EnableSlowMotion)))
		{
			auto position = std::find(supportedPlaybackSpeeds.begin(), supportedPlaybackSpeeds.end(), 0.5);
			if(position != supportedPlaybackSpeeds.end())
			{
				supportedPlaybackSpeeds.erase(position); //remove 0.5 from supported speeds
			}
		}

		for(int i = 0; i < supportedPlaybackSpeeds.size(); i++)
		{
			event->addSupportedSpeed(supportedPlaybackSpeeds[i]);;
		}
	}
	else
	{
		//Supports only pause and play
		event->addSupportedSpeed(0);
		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableSlowMotion))
		{
			event->addSupportedSpeed(0.5);
		}
		event->addSupportedSpeed(1);
	}

	AAMPLOG_WARN("aamp: sending supported speeds changed event with count %d", event->getSupportedSpeedCount());
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Generate Blocked event based on args passed.
 */
void PrivateInstanceAAMP::SendBlockedEvent(const std::string & reason)
{
	BlockedEventPtr event = std::make_shared<BlockedEvent>(reason);
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
#ifdef AAMP_CC_ENABLED
	if (0 == reason.compare("SERVICE_PIN_LOCKED"))
	{
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			AampCCManager::GetInstance()->SetParentalControlStatus(true);
		}
	}
#endif
}

/**
 * @brief  Generate WatermarkSessionUpdate event based on args passed.
 */
void PrivateInstanceAAMP::SendWatermarkSessionUpdateEvent(uint32_t sessionHandle, uint32_t status, const std::string &system)
{
	WatermarkSessionUpdateEventPtr event = std::make_shared<WatermarkSessionUpdateEvent>(sessionHandle, status, system);
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief  Check if tune completed or not.
 */
bool PrivateInstanceAAMP::IsTuneCompleted()
{
	return mTuneCompleted;
}

/**
 * @brief Get Preferred DRM.
 */
DRMSystems PrivateInstanceAAMP::GetPreferredDRM()
{
	int drmType;
	GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredDRM,drmType);
	return (DRMSystems)drmType;
}

/**
 * @brief Notification from the stream abstraction that a new SCTE35 event is found.
 */
void PrivateInstanceAAMP::FoundEventBreak(const std::string &adBreakId, uint64_t startMS, EventBreakInfo brInfo)
{
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableClientDai) && !adBreakId.empty())
	{
		AAMPLOG_WARN("[CDAI] Found Adbreak on period[%s] Duration[%d]", adBreakId.c_str(), brInfo.duration);
		std::string adId("");
		std::string url("");
		mCdaiObject->SetAlternateContents(adBreakId, adId, url, startMS, brInfo.duration);	//A placeholder to avoid multiple scte35 event firing for the same adbreak
		SaveNewTimedMetadata((long long) startMS, brInfo.name.c_str(), brInfo.payload.c_str(), brInfo.payload.size(), adBreakId.c_str(), brInfo.duration);
	}
}

/**
 *  @brief Setting the alternate contents' (Ads/blackouts) URL
 */
void PrivateInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableClientDai))
	{
		mCdaiObject->SetAlternateContents(adBreakId, adId, url);
	}
	else
	{
		AAMPLOG_WARN("is called! CDAI not enabled!! Rejecting the promise.");
		SendAdResolvedEvent(adId, false, 0, 0);
	}
}

/**
 * @brief Send status of Ad manifest downloading & parsing
 */
void PrivateInstanceAAMP::SendAdResolvedEvent(const std::string &adId, bool status, uint64_t startMS, uint64_t durationMs)
{
	if (mDownloadsEnabled)	//Send it, only if Stop not called
	{
		AdResolvedEventPtr e = std::make_shared<AdResolvedEvent>(status, adId, startMS, durationMs);
		AAMPLOG_WARN("PrivateInstanceAAMP: [CDAI] Sent resolved status=%d for adId[%s]", status, adId.c_str());
		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 * @brief Deliver all pending Ad events to JSPP
 */
void PrivateInstanceAAMP::DeliverAdEvents(bool immediate)
{
	std::lock_guard<std::mutex> lock(mAdEventQMtx);
	while (!mAdEventsQ.empty())
	{
		AAMPEventPtr e = mAdEventsQ.front();
		if(immediate)
		{
			mEventManager->SendEvent(e,AAMP_EVENT_SYNC_MODE);
		}
		else
		{
			mEventManager->SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		}
		AAMPEventType evtType = e->getType();
		AAMPLOG_WARN("PrivateInstanceAAMP:, [CDAI] Delivered AdEvent[%s] to JSPP.", ADEVENT2STRING(evtType));
		if(AAMP_EVENT_AD_PLACEMENT_START == evtType)
		{
			AdPlacementEventPtr placementEvt = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			mAdProgressId       = placementEvt->getAdId();
			mAdPrevProgressTime = NOW_STEADY_TS_MS;
			mAdCurOffset        = placementEvt->getOffset();
			mAdDuration         = placementEvt->getDuration();
		}
		else if(AAMP_EVENT_AD_PLACEMENT_END == evtType || AAMP_EVENT_AD_PLACEMENT_ERROR == evtType)
		{
			mAdProgressId = "";
		}
		mAdEventsQ.pop();
	}
}

/**
 * @brief Send Ad reservation event
 */
void PrivateInstanceAAMP::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId, uint64_t position, bool immediate)
{
	if(AAMP_EVENT_AD_RESERVATION_START == type || AAMP_EVENT_AD_RESERVATION_END == type)
	{
		AAMPLOG_INFO("PrivateInstanceAAMP: [CDAI] Pushed [%s] of adBreakId[%s] to Queue.", ADEVENT2STRING(type), adBreakId.c_str());

		AdReservationEventPtr e = std::make_shared<AdReservationEvent>(type, adBreakId, position);

		{
			{
				std::lock_guard<std::mutex> lock(mAdEventQMtx);
				mAdEventsQ.push(e);
			}
			if(immediate)
			{
				//Despatch all ad events now
				DeliverAdEvents(true);
			}
		}
	}
}

/**
 * @brief Send Ad placement event
 */
void PrivateInstanceAAMP::SendAdPlacementEvent(AAMPEventType type, const std::string &adId, uint32_t position, uint32_t adOffset, uint32_t adDuration, bool immediate, long error_code)
{
	if(AAMP_EVENT_AD_PLACEMENT_START <= type && AAMP_EVENT_AD_PLACEMENT_ERROR >= type)
	{
		AAMPLOG_INFO("PrivateInstanceAAMP: [CDAI] Pushed [%s] of adId[%s] to Queue.", ADEVENT2STRING(type), adId.c_str());

		AdPlacementEventPtr e = std::make_shared<AdPlacementEvent>(type, adId, position, adOffset * 1000 /*MS*/, adDuration, error_code);

		{
			{
				std::lock_guard<std::mutex> lock(mAdEventQMtx);
				mAdEventsQ.push(e);
			}
			if(immediate)
			{
				//Despatch all ad events now
				DeliverAdEvents(true);
			}
		}
	}
}

/**
 *  @brief Get stream type as printable format
 */
std::string PrivateInstanceAAMP::getStreamTypeString()
{
	std::string type = mMediaFormatName[mMediaFormat];

	if(mCurrentDrm != nullptr) //Incomplete Init won't be set the DRM
	{
		type += "/";
		type += mCurrentDrm->friendlyName();
	}
	else
	{
		type += "/Clear";
	}
	return type;
}

/**
 * @brief Convert media file type to profiler bucket type
 */
ProfilerBucketType PrivateInstanceAAMP::mediaType2Bucket(MediaType fileType)
{
	ProfilerBucketType pbt;
	switch(fileType)
	{
		case eMEDIATYPE_VIDEO:
			pbt = PROFILE_BUCKET_FRAGMENT_VIDEO;
			break;
		case eMEDIATYPE_AUDIO:
			pbt = PROFILE_BUCKET_FRAGMENT_AUDIO;
			break;
		case eMEDIATYPE_SUBTITLE:
			pbt = PROFILE_BUCKET_FRAGMENT_SUBTITLE;
			break;
		case eMEDIATYPE_AUX_AUDIO:
			pbt = PROFILE_BUCKET_FRAGMENT_AUXILIARY;
			break;
		case eMEDIATYPE_MANIFEST:
			pbt = PROFILE_BUCKET_MANIFEST;
			break;
		case eMEDIATYPE_INIT_VIDEO:
			pbt = PROFILE_BUCKET_INIT_VIDEO;
			break;
		case eMEDIATYPE_INIT_AUDIO:
			pbt = PROFILE_BUCKET_INIT_AUDIO;
			break;
		case eMEDIATYPE_INIT_SUBTITLE:
			pbt = PROFILE_BUCKET_INIT_SUBTITLE;
			break;
		case eMEDIATYPE_INIT_AUX_AUDIO:
			pbt = PROFILE_BUCKET_INIT_AUXILIARY;
			break;
		case eMEDIATYPE_PLAYLIST_VIDEO:
			pbt = PROFILE_BUCKET_PLAYLIST_VIDEO;
			break;
		case eMEDIATYPE_PLAYLIST_AUDIO:
			pbt = PROFILE_BUCKET_PLAYLIST_AUDIO;
			break;
		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			pbt = PROFILE_BUCKET_PLAYLIST_SUBTITLE;
			break;
		case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			pbt = PROFILE_BUCKET_PLAYLIST_AUXILIARY;
			break;
		default:
			pbt = (ProfilerBucketType)fileType;
			break;
	}
	return pbt;
}

/**
 * @brief Sets Recorded URL from Manifest received form XRE
 */
void PrivateInstanceAAMP::SetTunedManifestUrl(bool isrecordedUrl)
{
	mTunedManifestUrl.assign(mManifestUrl);
	AAMPLOG_TRACE("mManifestUrl: %s",mManifestUrl.c_str());
	if(isrecordedUrl)
	{
		DeFog(mTunedManifestUrl);
		mTunedManifestUrl.replace(0,4,"_fog");
	}
	AAMPLOG_TRACE("PrivateInstanceAAMP::tunedManifestUrl:%s ", mTunedManifestUrl.c_str());
}

/**
 * @brief Gets Recorded URL from Manifest received form XRE.
 */
const char* PrivateInstanceAAMP::GetTunedManifestUrl()
{
	AAMPLOG_TRACE("PrivateInstanceAAMP::tunedManifestUrl:%s ", mTunedManifestUrl.c_str());
	return mTunedManifestUrl.c_str();
}

/**
 *  @brief To get the network proxy
 */
std::string PrivateInstanceAAMP::GetNetworkProxy()
{
	std::string proxy;
	GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkProxy,proxy);
	return proxy;
}

/**
 * @brief To get the proxy for license request
 */
std::string PrivateInstanceAAMP::GetLicenseReqProxy()
{
	std::string proxy;
	GETCONFIGVALUE_PRIV(eAAMPConfig_LicenseProxy,proxy);
	return proxy;
}


/**
 * @brief Signal trick mode discontinuity to stream sink
 */
void PrivateInstanceAAMP::SignalTrickModeDiscontinuity()
{
	if (mStreamSink)
	{
		mStreamSink->SignalTrickModeDiscontinuity();
	}
}

/**
 *   @brief Check if current stream is muxed
 */
bool PrivateInstanceAAMP::IsMuxedStream()
{
	bool ret = false;
	if (mpStreamAbstractionAAMP)
	{
		ret = mpStreamAbstractionAAMP->IsMuxedStream();
	}
	return ret;
}

/**
 * @brief Stop injection for a track.
 * Called from StopInjection
 */
void PrivateInstanceAAMP::StopTrackInjection(MediaType type)
{
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		AAMPLOG_WARN ("PrivateInstanceAAMP: Enter. type = %d", (int) type);
	}
#endif
	if (!mTrackInjectionBlocked[type])
	{
		AAMPLOG_TRACE("PrivateInstanceAAMP: for type %s", (type == eMEDIATYPE_AUDIO) ? "audio" : "video");
		pthread_mutex_lock(&mLock);
		mTrackInjectionBlocked[type] = true;
		pthread_mutex_unlock(&mLock);
	}
	AAMPLOG_TRACE ("PrivateInstanceAAMP::Exit. type = %d", (int) type);
}

/**
 * @brief Resume injection for a track.
 * Called from StartInjection
 */
void PrivateInstanceAAMP::ResumeTrackInjection(MediaType type)
{
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		AAMPLOG_WARN ("PrivateInstanceAAMP: Enter. type = %d", (int) type);
	}
#endif
	if (mTrackInjectionBlocked[type])
	{
		AAMPLOG_TRACE("PrivateInstanceAAMP: for type %s", (type == eMEDIATYPE_AUDIO) ? "audio" : "video");
		pthread_mutex_lock(&mLock);
		mTrackInjectionBlocked[type] = false;
		pthread_mutex_unlock(&mLock);
	}
	AAMPLOG_TRACE ("PrivateInstanceAAMP::Exit. type = %d", (int) type);
}

/**
 * @brief Receives first video PTS of the current playback
 */
void PrivateInstanceAAMP::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->NotifyFirstVideoPTS(pts, timeScale);
	}
}

/**
 * @brief Notifies base PTS of the HLS video playback
 */
void PrivateInstanceAAMP::NotifyVideoBasePTS(unsigned long long basepts, unsigned long timeScale)
{
	mVideoBasePTS = basepts;
	AAMPLOG_INFO("mVideoBasePTS::%llus", mVideoBasePTS);
}

/**
 * @brief To send webvtt cue as an event
 */
void PrivateInstanceAAMP::SendVTTCueDataAsEvent(VTTCue* cue)
{
	//This function is called from an idle handler and hence we call SendEventSync
	if (mEventManager->IsEventListenerAvailable(AAMP_EVENT_WEBVTT_CUE_DATA))
	{
		WebVttCueEventPtr ev = std::make_shared<WebVttCueEvent>(cue);
		mEventManager->SendEvent(ev,AAMP_EVENT_SYNC_MODE);
	}
}

/**
 * @brief To check if subtitles are enabled
 */
bool PrivateInstanceAAMP::IsSubtitleEnabled(void)
{
	// Assumption being that enableSubtec and event listener will not be registered at the same time
	// in which case subtec gets priority over event listener
	return (ISCONFIGSET_PRIV(eAAMPConfig_Subtec_subtitle)  || 	WebVTTCueListenersRegistered());
}

/**
 * @brief To check if JavaScript cue listeners are registered
 */
bool PrivateInstanceAAMP::WebVTTCueListenersRegistered(void)
{
	return mEventManager->IsSpecificEventListenerAvailable(AAMP_EVENT_WEBVTT_CUE_DATA);
}

/**
 * @brief To get any custom license HTTP headers that was set by application
 */
void PrivateInstanceAAMP::GetCustomLicenseHeaders(std::unordered_map<std::string, std::vector<std::string>>& customHeaders)
{
	customHeaders.insert(mCustomLicenseHeaders.begin(), mCustomLicenseHeaders.end());
}

/**
 * @brief Sends an ID3 metadata event.
 */
void PrivateInstanceAAMP::SendId3MetadataEvent(Id3CallbackData* id3Metadata)
{
	if(id3Metadata) {
	ID3MetadataEventPtr e = std::make_shared<ID3MetadataEvent>(id3Metadata->data, id3Metadata->schemeIdUri, id3Metadata->value, id3Metadata->timeScale, id3Metadata->presentationTime, id3Metadata->eventDuration, id3Metadata->id, id3Metadata->timestampOffset);
	if (ISCONFIGSET_PRIV(eAAMPConfig_ID3Logging))
	{
		std::vector<uint8_t> metadata = e->getMetadata();
		int metadataLen = e->getMetadataSize();
		int printableLen = 0;
		std::ostringstream tag;

		tag << "ID3 tag length: " << metadataLen;
	
		if (metadataLen > 0 )
		{
			tag << " payload: ";

			for (int i = 0; i < metadataLen; i++)
			{
				if (std::isprint(metadata[i]))
				{
					tag << metadata[i];
					printableLen++;
				}
			}
		}
		// Logger has a maximum message size limit, warn if too big
		// Current large ID3 tag size is 1055, but printable < MAX_DEBUG_LOG_BUFF_SIZE.
		std::string tagLog(tag.str());
		AAMPLOG_INFO("%s", tag.str().c_str());
		AAMPLOG_INFO("{schemeIdUri:\"%s\",value:\"%s\",presentationTime:%" PRIu64 ",timeScale:%" PRIu32 ",eventDuration:%" PRIu32 ",id:%" PRIu32 ",timestampOffset:%" PRIu64 "}",e->getSchemeIdUri().c_str(), e->getValue().c_str(), e->getPresentationTime(), e->getTimeScale(), e->getEventDuration(), e->getId(), e->getTimestampOffset());

		if (printableLen > MAX_DEBUG_LOG_BUFF_SIZE)
		{
			AAMPLOG_WARN("ID3 log was truncated, original size %d (printable %d)" , metadataLen, printableLen);
		}
	}
	mEventManager->SendEvent(e,AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 * @brief Sending a flushing seek to stream sink with given position
 */
void PrivateInstanceAAMP::FlushStreamSink(double position, double rate)
{
#ifndef AAMP_STOP_SINK_ON_SEEK
	if (mStreamSink)
	{
		if(ISCONFIGSET_PRIV(eAAMPConfig_MidFragmentSeek) && position != 0 )
		{
			//RDK-26957 Adding midSeekPtsOffset to position value.
			//Enables us to seek to the desired position in the mp4 fragment.
			mStreamSink->SeekStreamSink(position + GetFirstPTS(), rate);
		}
		else
		{
			mStreamSink->SeekStreamSink(position, rate);
		}
	}
#endif
}

/**
 * @brief PreCachePlaylistDownloadTask Thread function for PreCaching Playlist 
 */
void PrivateInstanceAAMP::PreCachePlaylistDownloadTask()
{
	// This is the thread function to download all the HLS Playlist in a 
	// differed manner
	int maxWindowforDownload = mPreCacheDnldTimeWindow * 60; // convert to seconds  
	int szPlaylistCount = mPreCacheDnldList.size();
	if(szPlaylistCount)
	{
		PrivAAMPState state;
		// First wait for Tune to complete to start this functionality
		pthread_mutex_lock(&mMutexPlaystart);
		pthread_cond_wait(&waitforplaystart, &mMutexPlaystart);
		pthread_mutex_unlock(&mMutexPlaystart);
		// May be Stop is called to release all resources .
		// Before download , check the state 
		GetState(state);
		// Check for state not IDLE also to avoid DELIA-46092
		if(state != eSTATE_RELEASED && state != eSTATE_IDLE && state != eSTATE_ERROR)
		{
			CurlInit(eCURLINSTANCE_PLAYLISTPRECACHE, 1, GetNetworkProxy());
			SetCurlTimeout(mPlaylistTimeoutMs, eCURLINSTANCE_PLAYLISTPRECACHE);
			int sleepTimeBetweenDnld = (maxWindowforDownload / szPlaylistCount) * 1000; // time in milliSec 
			int idx = 0;
			do
			{
				if(DownloadsAreEnabled())
				{
					InterruptableMsSleep(sleepTimeBetweenDnld);

					// First check if the file is already in Cache
					PreCacheUrlStruct newelem = mPreCacheDnldList.at(idx);
					
					// check if url cached ,if not download
					if(getAampCacheHandler()->IsUrlCached(newelem.url)==false)
					{
						AAMPLOG_WARN("Downloading Playlist Type:%d for PreCaching:%s",
							newelem.type, newelem.url.c_str());
						std::string playlistUrl;
						std::string playlistEffectiveUrl;
						GrowableBuffer playlistStore ;
						long http_error;
						double downloadTime;
						if(GetFile(newelem.url, &playlistStore, playlistEffectiveUrl, &http_error, &downloadTime, NULL, eCURLINSTANCE_PLAYLISTPRECACHE, true, newelem.type))
						{
							// If successful download , then insert into Cache 
							getAampCacheHandler()->InsertToPlaylistCache(newelem.url, &playlistStore, playlistEffectiveUrl, false, newelem.type);
							aamp_Free(&playlistStore);
						}	
					}
					idx++;
				}
				else
				{
					// this can come here if trickplay is done or play started late
					if(state == eSTATE_SEEKING || state == eSTATE_PREPARED)
					{
						// wait for seek to complete 
						sleep(1);
					}
					else
					{
						usleep(500000); // call sleep for other stats except seeking and prepared, otherwise this thread will run in highest priority until the state changes.
					}
				}
				GetState(state);
			}while (idx < mPreCacheDnldList.size() && state != eSTATE_RELEASED && state != eSTATE_IDLE && state != eSTATE_ERROR);
			mPreCacheDnldList.clear();
			CurlTerm(eCURLINSTANCE_PLAYLISTPRECACHE);
		}
	}
	AAMPLOG_WARN("End of PreCachePlaylistDownloadTask ");
}

/**
 * @brief SetPreCacheDownloadList - Function to assign the PreCaching file list
 */
void PrivateInstanceAAMP::SetPreCacheDownloadList(PreCacheUrlList &dnldListInput)
{
	mPreCacheDnldList = dnldListInput;
	if(mPreCacheDnldList.size())
	{
		AAMPLOG_WARN("Got Playlist PreCache list of Size : %d", mPreCacheDnldList.size());
	}
	
}

/**
 *   @brief get the current text preference set by user
 *
 *   @return json string with preference data
 */
std::string PrivateInstanceAAMP::GetPreferredTextProperties()
{
	//Convert to JSON format
	std::string preferrence;
	cJSON *item;
	item = cJSON_CreateObject();
	if(!preferredTextLanguagesString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-languages", preferredTextLanguagesString.c_str());
	}
	if(!preferredTextLabelString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-labels", preferredTextLabelString.c_str());
	}
	if(!preferredTextRenditionString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-rendition", preferredTextRenditionString.c_str());
	}
	if(!preferredTextTypeString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-type", preferredTextTypeString.c_str());
	}
	if(!preferredTextAccessibilityNode.getSchemeId().empty())
	{
		cJSON *accessibility = cJSON_AddObjectToObject(item, "preferred-text-accessibility");
		cJSON_AddStringToObject(accessibility, "schemeId", preferredTextAccessibilityNode.getSchemeId().c_str());
		if (preferredTextAccessibilityNode.getTypeName() == "int_value")
		{
			cJSON_AddNumberToObject(accessibility, preferredTextAccessibilityNode.getTypeName().c_str(), preferredTextAccessibilityNode.getIntValue());
		}else
		{
			cJSON_AddStringToObject(accessibility, preferredTextAccessibilityNode.getTypeName().c_str(), preferredTextAccessibilityNode.getStrValue().c_str());
		}
	}
	char *jsonStr = cJSON_Print(item);
	if (jsonStr)
	{
		preferrence.assign(jsonStr);
		free(jsonStr);
	}
	cJSON_Delete(item);
	return preferrence;
}

/**
 *   @brief get the current audio preference set by user
 */
std::string PrivateInstanceAAMP::GetPreferredAudioProperties()
{
	//Convert to JSON format
	std::string preferrence;
	cJSON *item;
	item = cJSON_CreateObject();
	if(!preferredLanguagesString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-languages", preferredLanguagesString.c_str());
	}
	if(!preferredLabelsString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-labels", preferredLabelsString.c_str());
	}
	if(!preferredCodecString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-codecs", preferredCodecString.c_str());
	}
	if(!preferredRenditionString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-rendition", preferredRenditionString.c_str());
	}
	if(!preferredTypeString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-type", preferredTypeString.c_str());
	}
	if(!preferredAudioAccessibilityNode.getSchemeId().empty())
	{
		cJSON * accessibility = cJSON_AddObjectToObject(item, "preferred-audio-accessibility");
		cJSON_AddStringToObject(accessibility, "schemeId", preferredAudioAccessibilityNode.getSchemeId().c_str());
		if (preferredAudioAccessibilityNode.getTypeName() == "int_value")
		{
			cJSON_AddNumberToObject(accessibility, preferredAudioAccessibilityNode.getTypeName().c_str(), preferredAudioAccessibilityNode.getIntValue());
		}else
		{
			cJSON_AddStringToObject(accessibility, preferredAudioAccessibilityNode.getTypeName().c_str(), preferredAudioAccessibilityNode.getStrValue().c_str());
		}
	}
	char *jsonStr = cJSON_Print(item);
	if (jsonStr)
	{
		preferrence.assign(jsonStr);
		free(jsonStr);
	}
	cJSON_Delete(item);
	return preferrence;
}

/**
 * @brief Get available video tracks.
 */
std::string PrivateInstanceAAMP::GetAvailableVideoTracks()
{
	std::string tracks;

	pthread_mutex_lock(&mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		std::vector <StreamInfo*> trackInfo = mpStreamAbstractionAAMP->GetAvailableVideoTracks();
		if (!trackInfo.empty())
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				for (int i = 0; i < trackInfo.size(); i++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if (trackInfo[i]->bandwidthBitsPerSecond != -1)
					{
						cJSON_AddNumberToObject(item, "bandwidth", trackInfo[i]->bandwidthBitsPerSecond);
					}
					if (trackInfo[i]->resolution.width != -1)
					{
						cJSON_AddNumberToObject(item, "width", trackInfo[i]->resolution.width);
					}
					if (trackInfo[i]->resolution.height != -1)
					{
						cJSON_AddNumberToObject(item, "height", trackInfo[i]->resolution.height);
					}
					if (trackInfo[i]->resolution.framerate != -1)
					{
						cJSON_AddNumberToObject(item, "framerate", trackInfo[i]->resolution.framerate);
					}

					cJSON_AddNumberToObject(item, "enabled", trackInfo[i]->enabled);

					if (trackInfo[i]->codecs)
					{
						cJSON_AddStringToObject(item, "codec", trackInfo[i]->codecs);
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					tracks.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available video track information!");
		}
	}
	pthread_mutex_unlock(&mStreamLock);
	return tracks;
}

/**
 * @brief  set birate for video tracks selection
 */
void PrivateInstanceAAMP::SetVideoTracks(std::vector<long> bitrateList)
{
	int bitrateSize = bitrateList.size();
	//clear cached bitrate list
	this->bitrateList.clear();
	// user profile stats enabled only for valid bitrate list, otherwise disabled for empty bitrates
	this->userProfileStatus = (bitrateSize > 0) ? true : false;
	AAMPLOG_INFO("User Profile filtering bitrate size:%d status:%d", bitrateSize, this->userProfileStatus);
	for (int i = 0; i < bitrateSize; i++)
	{
		this->bitrateList.push_back(bitrateList.at(i));
		AAMPLOG_WARN("User Profile Index : %d(%d) Bw : %ld", i, bitrateSize, bitrateList.at(i));
	}
	PrivAAMPState state;
	GetState(state);
	if (state > eSTATE_PREPARING)
	{
		AcquireStreamLock();
		TuneHelper(eTUNETYPE_RETUNE);
		ReleaseStreamLock();
	}
}

/**
 * @brief Get available audio tracks.
 */
std::string PrivateInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	std::string tracks;

	pthread_mutex_lock(&mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		std::vector<AudioTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableAudioTracks(allTrack);
		if (!trackInfo.empty())
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			cJSON *accessbilityArray = NULL;
			root = cJSON_CreateArray();
			if(root)
			{
				for (auto iter = trackInfo.begin(); iter != trackInfo.end(); iter++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if (!iter->name.empty())
					{
						cJSON_AddStringToObject(item, "name", iter->name.c_str());
					}
					if (!iter->label.empty())
					{
						cJSON_AddStringToObject(item, "label", iter->label.c_str());
					}
					if (!iter->language.empty())
					{
						cJSON_AddStringToObject(item, "language", iter->language.c_str());
					}
					if (!iter->codec.empty())
					{
						cJSON_AddStringToObject(item, "codec", iter->codec.c_str());
					}
					if (!iter->rendition.empty())
					{
						cJSON_AddStringToObject(item, "rendition", iter->rendition.c_str());
					}
					if (!iter->accessibilityType.empty())
					{
						cJSON_AddStringToObject(item, "accessibilityType", iter->accessibilityType.c_str());
					}
					if (!iter->characteristics.empty())
					{
						cJSON_AddStringToObject(item, "characteristics", iter->characteristics.c_str());
					}
					if (iter->channels != 0)
					{
						cJSON_AddNumberToObject(item, "channels", iter->channels);
					}
					if (iter->bandwidth != -1)
					{
						cJSON_AddNumberToObject(item, "bandwidth", iter->bandwidth);
					}
					if (!iter->contentType.empty())
					{
						cJSON_AddStringToObject(item, "contentType", iter->contentType.c_str());
					}
					if (!iter->mixType.empty())
					{
						cJSON_AddStringToObject(item, "mixType", iter->mixType.c_str());
					}
					if (!iter->mType.empty())
					{
						cJSON_AddStringToObject(item, "Type", iter->mType.c_str());
					}
					cJSON_AddBoolToObject(item, "availability", iter->isAvailable);
					if (!iter->accessibilityItem.getSchemeId().empty())
					{
						cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
						cJSON_AddStringToObject(accessibility, "scheme", iter->accessibilityItem.getSchemeId().c_str());
						std::string valueType = iter->accessibilityItem.getTypeName();
						if (valueType == "int_value")
						{
							cJSON_AddNumberToObject(accessibility, valueType.c_str(), iter->accessibilityItem.getIntValue());
						}else
						{
							cJSON_AddStringToObject(accessibility, valueType.c_str(), iter->accessibilityItem.getStrValue().c_str());
						}
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					tracks.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available audio track information!");
		}
	}
	pthread_mutex_unlock(&mStreamLock);
	return tracks;
}

/**
 *   @brief Get available text tracks.
 */
std::string PrivateInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	std::string tracks;

	pthread_mutex_lock(&mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		std::vector<TextTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableTextTracks(allTrack);

#ifdef AAMP_CC_ENABLED
		std::vector<TextTrackInfo> textTracksCopy;
		std::copy_if(begin(trackInfo), end(trackInfo), back_inserter(textTracksCopy), [](const TextTrackInfo& e){return e.isCC;});
		AampCCManager::GetInstance()->updateLastTextTracks(textTracksCopy);
#endif
		if (!trackInfo.empty())
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				for (auto iter = trackInfo.begin(); iter != trackInfo.end(); iter++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if (!iter->name.empty())
					{
						cJSON_AddStringToObject(item, "name", iter->name.c_str());
					}
					if (!iter->label.empty())
					{
						cJSON_AddStringToObject(item, "label", iter->label.c_str());
					}
					if (iter->isCC)
					{
						cJSON_AddStringToObject(item, "sub-type", "CLOSED-CAPTIONS");
					}
					else
					{
						cJSON_AddStringToObject(item, "sub-type", "SUBTITLES");
					}
					if (!iter->language.empty())
					{
						cJSON_AddStringToObject(item, "language", iter->language.c_str());
					}
					if (!iter->rendition.empty())
					{
						cJSON_AddStringToObject(item, "rendition", iter->rendition.c_str());
					}
					if (!iter->accessibilityType.empty())
					{
						cJSON_AddStringToObject(item, "accessibilityType", iter->accessibilityType.c_str());
					}
					if (!iter->instreamId.empty())
					{
						cJSON_AddStringToObject(item, "instreamId", iter->instreamId.c_str());
					}
					if (!iter->characteristics.empty())
					{
						cJSON_AddStringToObject(item, "characteristics", iter->characteristics.c_str());
					}
					if (!iter->mType.empty())
					{
						cJSON_AddStringToObject(item, "type", iter->mType.c_str());
					}
					if (!iter->codec.empty())
					{
						cJSON_AddStringToObject(item, "codec", iter->codec.c_str());
					}
					cJSON_AddBoolToObject(item, "availability", iter->isAvailable);
					if (!iter->accessibilityItem.getSchemeId().empty())
					{
						cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
						cJSON_AddStringToObject(accessibility, "scheme", iter->accessibilityItem.getSchemeId().c_str());
						
						std::string valueType = iter->accessibilityItem.getTypeName();
						if (valueType == "int_value")
						{
							cJSON_AddNumberToObject(accessibility, valueType.c_str(), iter->accessibilityItem.getIntValue());

						}else
						{
							cJSON_AddStringToObject(accessibility, valueType.c_str(), iter->accessibilityItem.getStrValue().c_str());
						}
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					tracks.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available text track information!");
		}
	}
	pthread_mutex_unlock(&mStreamLock);
	return tracks;
}

/*
 * @brief Get the video window co-ordinates
 */
std::string PrivateInstanceAAMP::GetVideoRectangle()
{
	return mStreamSink->GetVideoRectangle();
}

/**
 * @brief Set the application name which has created PlayerInstanceAAMP, for logging purposes
 */
void PrivateInstanceAAMP::SetAppName(std::string name)
{
	mAppName = name;
}

/**
 *   @brief Get the application name
 */
std::string PrivateInstanceAAMP::GetAppName()
{
	return mAppName;
}

/**
 * @brief DRM individualization callback
 */
void PrivateInstanceAAMP::individualization(const std::string& payload)
{
	DrmMessageEventPtr event = std::make_shared<DrmMessageEvent>(payload);
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Get current initial buffer duration in seconds
 */
int PrivateInstanceAAMP::GetInitialBufferDuration()
{
	GETCONFIGVALUE_PRIV(eAAMPConfig_InitialBuffer,mMinInitialCacheSeconds);
	return mMinInitialCacheSeconds;
}

/**
 *   @brief Check if First Video Frame Displayed Notification
 *          is required.
 */
bool PrivateInstanceAAMP::IsFirstVideoFrameDisplayedRequired()
{
	return mFirstVideoFrameDisplayedEnabled;
}

/**
 *   @brief Notify First Video Frame was displayed
 */
void PrivateInstanceAAMP::NotifyFirstVideoFrameDisplayed()
{
	if(!mFirstVideoFrameDisplayedEnabled)
	{
		return;
	}

	mFirstVideoFrameDisplayedEnabled = false;

	// In the middle of stop processing we can receive state changing callback (xione-7331)
	PrivAAMPState state;
	GetState(state);
	if (state == eSTATE_IDLE)
	{
		AAMPLOG_WARN( "skipped as in IDLE state" );
		return;
	}
	
	// Seek While Paused - pause on first Video frame displayed
	if(mPauseOnFirstVideoFrameDisp)
	{
		mPauseOnFirstVideoFrameDisp = false;
		PrivAAMPState state;
		GetState(state);
		if(state != eSTATE_SEEKING)
		{
			return;
		}

		AAMPLOG_INFO("Pausing Playback on First Frame Displayed");
		if(mpStreamAbstractionAAMP)
		{
			mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
		}
		StopDownloads();
		if(PausePipeline(true, false))
		{
			SetState(eSTATE_PAUSED);
		}
		else
		{
			AAMPLOG_ERR("Failed to pause pipeline for first frame displayed!");
		}
	}
	// Otherwise check for setting BUFFERING state
	else if(!SetStateBufferingIfRequired())
	{
		// If Buffering state was not needed, set PLAYING state
		SetState(eSTATE_PLAYING);
	}
}

/**
 * @brief Set eSTATE_BUFFERING if required
 */
bool PrivateInstanceAAMP::SetStateBufferingIfRequired()
{
	bool bufferingSet = false;

	pthread_mutex_lock(&mFragmentCachingLock);
	if(IsFragmentCachingRequired())
	{
		bufferingSet = true;
		PrivAAMPState state;
		GetState(state);
		if(state != eSTATE_BUFFERING)
		{
			if(mpStreamAbstractionAAMP)
			{
				mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
			}

			if(mStreamSink)
			{
				mStreamSink->NotifyFragmentCachingOngoing();
			}
			SetState(eSTATE_BUFFERING);
		}
	}
	pthread_mutex_unlock(&mFragmentCachingLock);

	return bufferingSet;
}

/**
 * @brief Check to media track downloads are enabled
 */
bool PrivateInstanceAAMP::TrackDownloadsAreEnabled(MediaType type)
{
	bool ret = true;
	if (type >= AAMP_TRACK_COUNT)  //CID:142718 - overrun
	{
		AAMPLOG_ERR("type[%d] is un-supported, returning default as false!", type);
		ret = false;
	}
	else
	{
		pthread_mutex_lock(&mLock);
		// If blocked, track downloads are disabled
		ret = !mbTrackDownloadsBlocked[type];
		pthread_mutex_unlock(&mLock);
	}
	return ret;
}

/**
 * @brief Stop buffering in AAMP and un-pause pipeline.
 */
void PrivateInstanceAAMP::StopBuffering(bool forceStop)
{
	mStreamSink->StopBuffering(forceStop);
}

/**
 * @brief Get license server url for a drm type
 */
std::string PrivateInstanceAAMP::GetLicenseServerUrlForDrm(DRMSystems type)
{
	std::string url;
	if (type == eDRM_PlayReady)
	{
		GETCONFIGVALUE_PRIV(eAAMPConfig_PRLicenseServerUrl,url);
	}
	else if (type == eDRM_WideVine)
	{	
		GETCONFIGVALUE_PRIV(eAAMPConfig_WVLicenseServerUrl,url);
	}
	else if (type == eDRM_ClearKey)
	{
		GETCONFIGVALUE_PRIV(eAAMPConfig_CKLicenseServerUrl,url);
	}

	if(url.empty())
	{
		GETCONFIGVALUE_PRIV(eAAMPConfig_LicenseServerUrl,url);
	}
	return url;
}

/**
 * @brief Get current audio track index
 */
int PrivateInstanceAAMP::GetAudioTrack()
{
	int idx = -1;
	AcquireStreamLock();
	if (mpStreamAbstractionAAMP)
	{
		idx = mpStreamAbstractionAAMP->GetAudioTrack();
	}
	ReleaseStreamLock();
	return idx;
}

/**
 * @brief Get current audio track index
 */
std::string PrivateInstanceAAMP::GetAudioTrackInfo()
{
	std::string track;
	

	pthread_mutex_lock(&mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		AudioTrackInfo trackInfo;
		if (mpStreamAbstractionAAMP->GetCurrentAudioTrack(trackInfo))
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				cJSON_AddItemToArray(root, item = cJSON_CreateObject());
				if (!trackInfo.name.empty())
				{
					cJSON_AddStringToObject(item, "name", trackInfo.name.c_str());
				}
				if (!trackInfo.language.empty())
				{
					cJSON_AddStringToObject(item, "language", trackInfo.language.c_str());
				}
				if (!trackInfo.codec.empty())
				{
					cJSON_AddStringToObject(item, "codec", trackInfo.codec.c_str());
				}
				if (!trackInfo.rendition.empty())
				{
					cJSON_AddStringToObject(item, "rendition", trackInfo.rendition.c_str());
				}
				if (!trackInfo.label.empty())
				{
					cJSON_AddStringToObject(item, "label", trackInfo.label.c_str());
				}
				if (!trackInfo.accessibilityType.empty())
				{
					cJSON_AddStringToObject(item, "accessibilityType", trackInfo.accessibilityType.c_str());
				}
				if (!trackInfo.characteristics.empty())
				{
					cJSON_AddStringToObject(item, "characteristics", trackInfo.characteristics.c_str());
				}
				if (trackInfo.channels != 0)
				{
					cJSON_AddNumberToObject(item, "channels", trackInfo.channels);
				}
				if (trackInfo.bandwidth != -1)
				{
					cJSON_AddNumberToObject(item, "bandwidth", trackInfo.bandwidth);
				}
				if (!trackInfo.contentType.empty())
				{
					cJSON_AddStringToObject(item, "contentType", trackInfo.contentType.c_str());
				}
				if (!trackInfo.mixType.empty())
				{
					cJSON_AddStringToObject(item, "mixType", trackInfo.mixType.c_str());
				}
				if (!trackInfo.mType.empty())
				{
					cJSON_AddStringToObject(item, "type", trackInfo.mType.c_str());
				}
				if (!trackInfo.accessibilityItem.getSchemeId().empty())
				{
					cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
					cJSON_AddStringToObject(accessibility, "scheme", trackInfo.accessibilityItem.getSchemeId().c_str());
					if (trackInfo.accessibilityItem.getTypeName() == "int_value")
					{
						cJSON_AddNumberToObject(accessibility, trackInfo.accessibilityItem.getTypeName().c_str(), trackInfo.accessibilityItem.getIntValue());
					}
					else
					{
						cJSON_AddStringToObject(accessibility, trackInfo.accessibilityItem.getTypeName().c_str(), trackInfo.accessibilityItem.getStrValue().c_str());
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					track.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available Text track information!");
		}
	}
	else
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: Not in playing state!");
	}
	pthread_mutex_unlock(&mStreamLock);
	return track;
}

/**
 * @brief Get current audio track index
 */
std::string PrivateInstanceAAMP::GetTextTrackInfo()
{
	std::string track;
	pthread_mutex_lock(&mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		TextTrackInfo trackInfo;
		if (mpStreamAbstractionAAMP->GetCurrentTextTrack(trackInfo))
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				cJSON_AddItemToArray(root, item = cJSON_CreateObject());
				if (!trackInfo.name.empty())
				{
					cJSON_AddStringToObject(item, "name", trackInfo.name.c_str());
				}
				if (!trackInfo.language.empty())
				{
					cJSON_AddStringToObject(item, "language", trackInfo.language.c_str());
				}
				if (!trackInfo.codec.empty())
				{
					cJSON_AddStringToObject(item, "codec", trackInfo.codec.c_str());
				}
				if (!trackInfo.rendition.empty())
				{
					cJSON_AddStringToObject(item, "rendition", trackInfo.rendition.c_str());
				}
				if (!trackInfo.label.empty())
				{
					cJSON_AddStringToObject(item, "label", trackInfo.label.c_str());
				}
				if (!trackInfo.accessibilityType.empty())
				{
					cJSON_AddStringToObject(item, "accessibilityType", trackInfo.accessibilityType.c_str());
				}
				if (!trackInfo.characteristics.empty())
				{
					cJSON_AddStringToObject(item, "characteristics", trackInfo.characteristics.c_str());
				}
				if (!trackInfo.mType.empty())
				{
					cJSON_AddStringToObject(item, "type", trackInfo.mType.c_str());
				}
				if (!trackInfo.accessibilityItem.getSchemeId().empty())
				{
					cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
					cJSON_AddStringToObject(accessibility, "scheme", trackInfo.accessibilityItem.getSchemeId().c_str());
					if (trackInfo.accessibilityItem.getTypeName() == "int_value")
					{
						cJSON_AddNumberToObject(accessibility, trackInfo.accessibilityItem.getTypeName().c_str(), trackInfo.accessibilityItem.getIntValue());
					}
					else
					{
						cJSON_AddStringToObject(accessibility, trackInfo.accessibilityItem.getTypeName().c_str(), trackInfo.accessibilityItem.getStrValue().c_str());
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					track.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available Text track information!");
		}
	}
	else
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: Not in playing state!");
	}
	pthread_mutex_unlock(&mStreamLock);
	return track;
}


/**
 * @brief Create json data from track Info
 */
static char* createJsonData(TextTrackInfo& track)
{
	char *jsonStr = NULL;
	cJSON *item = cJSON_CreateObject();
	if (!track.name.empty())
	{
		cJSON_AddStringToObject(item, "name", track.name.c_str());
	}
	if (!track.language.empty())
	{
		cJSON_AddStringToObject(item, "languages", track.language.c_str());
	}
	if (!track.codec.empty())
	{
		cJSON_AddStringToObject(item, "codec", track.codec.c_str());
	}
	if (!track.rendition.empty())
	{
		cJSON_AddStringToObject(item, "rendition", track.rendition.c_str());
	}
	if (!track.label.empty())
	{
		cJSON_AddStringToObject(item, "label", track.label.c_str());
	}
	if (!track.accessibilityType.empty())
	{
		cJSON_AddStringToObject(item, "accessibilityType", track.accessibilityType.c_str());
	}
	if (!track.characteristics.empty())
	{
		cJSON_AddStringToObject(item, "characteristics", track.characteristics.c_str());
	}
	if (!track.mType.empty())
	{
		cJSON_AddStringToObject(item, "type", track.mType.c_str());
	}
	if (!track.accessibilityItem.getSchemeId().empty())
	{
		cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
		cJSON_AddStringToObject(accessibility, "scheme", track.accessibilityItem.getSchemeId().c_str());
		if (track.accessibilityItem.getTypeName() == "int_value")
		{
			cJSON_AddNumberToObject(accessibility, track.accessibilityItem.getTypeName().c_str(), track.accessibilityItem.getIntValue());
		}
		else
		{
			cJSON_AddStringToObject(accessibility, track.accessibilityItem.getTypeName().c_str(), track.accessibilityItem.getStrValue().c_str());
		}
	}
	
	jsonStr = cJSON_Print(item);
	cJSON_Delete(item);
	return jsonStr;
}

/**
 * @brief Set text track
 */
void PrivateInstanceAAMP::SetTextTrack(int trackId, char *data)
{
	AAMPLOG_INFO("trackId: %d", trackId);
	if (mpStreamAbstractionAAMP)
	{
		// Passing in -1 as the track ID mutes subs
		if (MUTE_SUBTITLES_TRACKID == trackId)
		{
			SetCCStatus(false);
			return;
		}

		if (data == NULL)
		{
			std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
			if (!tracks.empty() && (trackId >= 0 && trackId < tracks.size()))
			{
				TextTrackInfo track = tracks[trackId];
				// Check if CC / Subtitle track
				if (track.isCC)
				{
#ifdef AAMP_CC_ENABLED
					if (!track.instreamId.empty())
					{
						CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT;
						// AampCCManager expects the CC type, ie 608 or 708
						// For DASH, there is a possibility that instreamId is just an integer so we infer rendition
						if (mMediaFormat == eMEDIAFORMAT_DASH && (std::isdigit(static_cast<unsigned char>(track.instreamId[0])) == 0) && !track.rendition.empty())
						{
							if (track.rendition.find("608") != std::string::npos)
							{
								format = eCLOSEDCAPTION_FORMAT_608;
							}
							else if (track.rendition.find("708") != std::string::npos)
							{
								format = eCLOSEDCAPTION_FORMAT_708;
							}
						}	

						// preferredCEA708 overrides whatever we infer from track. USE WITH CAUTION
						int overrideCfg;
						GETCONFIGVALUE_PRIV(eAAMPConfig_CEAPreferred,overrideCfg);
						if (overrideCfg != -1)
						{
							format = (CCFormat)(overrideCfg & 1);
							AAMPLOG_WARN("PrivateInstanceAAMP: CC format override present, override format to: %d", format);
						}	
						AampCCManager::GetInstance()->SetTrack(track.instreamId, format);
					}
					else
					{
						AAMPLOG_ERR("PrivateInstanceAAMP: Track number/instreamId is empty, skip operation");
					}
#endif
				}
				else
				{
					//Unmute subtitles
					SetCCStatus(true);

					//TODO: Effective handling between subtitle and CC tracks
					int textTrack = mpStreamAbstractionAAMP->GetTextTrack();
					AAMPLOG_WARN("GetPreferredTextTrack %d trackId %d", textTrack, trackId);
					if (trackId != textTrack)
					{

						const char* jsonData = createJsonData(track);
						if(NULL != jsonData)
						{ 
							SetPreferredTextLanguages(jsonData);
						}
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("webvtt data received from application");
			mData = data;
			SetCCStatus(true);

			mpStreamAbstractionAAMP->InitSubtitleParser(data);

		}
	}
}


/**
 * @brief Switch the subtitle track following a change to the 
 *                      preferredTextTrack
 */
void PrivateInstanceAAMP::RefreshSubtitles()
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->RefreshSubtitles();
	}
}

/**
 * @brief Get current text track index
 */
int PrivateInstanceAAMP::GetTextTrack()
{
	int idx = -1;
	AcquireStreamLock();
#ifdef AAMP_CC_ENABLED
	if (AampCCManager::GetInstance()->GetStatus() && mpStreamAbstractionAAMP)
	{
		std::string trackId = AampCCManager::GetInstance()->GetTrack();
		if (!trackId.empty())
		{
			std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
			for (auto it = tracks.begin(); it != tracks.end(); it++)
			{
				if (it->instreamId == trackId)
				{
					idx = std::distance(tracks.begin(), it);
				}
			}
		}
	}
#endif
	if (mpStreamAbstractionAAMP && idx == -1 && !subtitles_muted)
	{
		idx = mpStreamAbstractionAAMP->GetTextTrack();
	}
	ReleaseStreamLock();
	return idx;
}

/**
 * @brief Set CC visibility on/off
 */
void PrivateInstanceAAMP::SetCCStatus(bool enabled)
{
#ifdef AAMP_CC_ENABLED
	AampCCManager::GetInstance()->SetStatus(enabled);
#endif
	AcquireStreamLock();
	subtitles_muted = !enabled;
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->MuteSubtitles(subtitles_muted);
		if (HasSidecarData())
		{ // has sidecar data
			mpStreamAbstractionAAMP->MuteSidecarSubtitles(subtitles_muted);
		}
	}
	SetSubtitleMute(subtitles_muted);
	ReleaseStreamLock();
}

/**
 * @brief Get CC visibility on/off
 */
bool PrivateInstanceAAMP::GetCCStatus(void)
{
	return !(subtitles_muted);
}

/**
 * @brief Function to notify available audio tracks changed
 */
void PrivateInstanceAAMP::NotifyAudioTracksChanged()
{
	SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_AUDIO_TRACKS_CHANGED),AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Function to notify available text tracks changed
 */
void PrivateInstanceAAMP::NotifyTextTracksChanged()
{
	SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_TEXT_TRACKS_CHANGED),AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Set style options for text track rendering
 *
 */
void PrivateInstanceAAMP::SetTextStyle(const std::string &options)
{
	//TODO: This can be later extended to subtitle rendering
	// Right now, API is not available for subtitle
#ifdef AAMP_CC_ENABLED
	AampCCManager::GetInstance()->SetStyle(options);
#endif
}

/**
 * @brief Get style options for text track rendering
 */
std::string PrivateInstanceAAMP::GetTextStyle()
{
	//TODO: This can be later extended to subtitle rendering
	// Right now, API is not available for subtitle
#ifdef AAMP_CC_ENABLED
	return AampCCManager::GetInstance()->GetStyle();
#else
	return std::string();
#endif
}

/**
 * @brief Check if any active PrivateInstanceAAMP available
 */
bool PrivateInstanceAAMP::IsActiveInstancePresent()
{
	return !gActivePrivAAMPs.empty();
}

/**
 *  @brief Set discontinuity ignored flag for given track
 */
void PrivateInstanceAAMP::SetTrackDiscontinuityIgnoredStatus(MediaType track)
{
	mIsDiscontinuityIgnored[track] = true;
}

/**
 *  @brief Check whether the given track discontinuity ignored earlier.
 */
bool PrivateInstanceAAMP::IsDiscontinuityIgnoredForOtherTrack(MediaType track)
{
	return (mIsDiscontinuityIgnored[track]);
}

/**
 *  @brief Reset discontinuity ignored flag for audio and video tracks
 */
void PrivateInstanceAAMP::ResetTrackDiscontinuityIgnoredStatus(void)
{
	mIsDiscontinuityIgnored[eTRACK_VIDEO] = false;
	mIsDiscontinuityIgnored[eTRACK_AUDIO] = false;
}

/**
 * @brief Set stream format for audio/video tracks
 */
void PrivateInstanceAAMP::SetStreamFormat(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat)
{
	bool reconfigure = false;
	AAMPLOG_WARN("Got format - videoFormat %d and audioFormat %d", videoFormat, audioFormat);

	// 1. Modified Configure() not to recreate all playbins if there is a change in track's format.
	// 2. For a demuxed scenario, this function will be called twice for each audio and video, so double the trouble.
	// Hence call Configure() only for following scenarios to reduce the overhead,
	// i.e FORMAT_INVALID to any KNOWN/FORMAT_UNKNOWN, FORMAT_UNKNOWN to any KNOWN and any FORMAT_KNOWN to FORMAT_KNOWN if it's not same.
	// Truth table
	// mVideFormat   videoFormat  reconfigure
	// *		  INVALID	false
	// INVALID        INVALID       false
	// INVALID        UNKNOWN       true
	// INVALID	  KNOWN         true
	// UNKNOWN	  INVALID	false
	// UNKNOWN        UNKNOWN       false
	// UNKNOWN	  KNOWN		true
	// KNOWN	  INVALID	false
	// KNOWN          UNKNOWN	false
	// KNOWN          KNOWN         true if format changes, false if same
	pthread_mutex_lock(&mLock);
	if (videoFormat != FORMAT_INVALID && mVideoFormat != videoFormat && (videoFormat != FORMAT_UNKNOWN || mVideoFormat == FORMAT_INVALID))
	{
		reconfigure = true;
		mVideoFormat = videoFormat;
	}
	if (audioFormat != FORMAT_INVALID && mAudioFormat != audioFormat && (audioFormat != FORMAT_UNKNOWN || mAudioFormat == FORMAT_INVALID))
	{
		reconfigure = true;
		mAudioFormat = audioFormat;
	}
	if (auxFormat != mAuxFormat && (mAuxFormat == FORMAT_INVALID || (mAuxFormat != FORMAT_UNKNOWN && auxFormat != FORMAT_UNKNOWN)) && auxFormat != FORMAT_INVALID)
	{
		reconfigure = true;
		mAuxFormat = auxFormat;
	}
	if (IsMuxedStream() && (mVideoComponentCount == 0 || mAudioComponentCount == 0)) //Can be a Muxed stream/Demuxed with either of audio or video-only stream
	{
		AAMPLOG_INFO(" TS Processing Done. Number of Audio Components : %d and Video Components : %d",mAudioComponentCount,mVideoComponentCount);
		if (IsAudioOrVideoOnly(videoFormat, audioFormat, auxFormat))
		{
			bool newTune = IsNewTune();
			pthread_mutex_unlock(&mLock);
			mStreamSink->Stop(!newTune);
			pthread_mutex_lock(&mLock);
			reconfigure = true;
		}
	}
	if (reconfigure)
	{
		// Configure pipeline as TSProcessor might have detected the actual stream type
		// or even presence of audio
		mStreamSink->Configure(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat, false, mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
	}
	pthread_mutex_unlock(&mLock);
}

/**
 * @brief To check for audio/video only Playback
 */

bool PrivateInstanceAAMP::IsAudioOrVideoOnly(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat)
{
	AAMPLOG_WARN("Old Stream format - videoFormat %d and audioFormat %d",mVideoFormat,mAudioFormat);
	bool ret = false;
	if (mVideoComponentCount == 0 && (mVideoFormat != videoFormat && videoFormat == FORMAT_INVALID))
	{
		mAudioOnlyPb = true;
		mVideoFormat = videoFormat;
		AAMPLOG_INFO("Audio-Only PlayBack");
		ret = true;
	}

	else if (mAudioComponentCount == 0)
	{
		if (mAudioFormat != audioFormat && audioFormat == FORMAT_INVALID)
		{
			mAudioFormat = audioFormat;
		}
		else if (mAuxFormat != auxFormat && auxFormat == FORMAT_INVALID)
		{
			mAuxFormat = auxFormat;
		}
		mVideoOnlyPb = true;
		AAMPLOG_INFO("Video-Only PlayBack");
		ret = true;
	}

	return ret;
}
/**
 *  @brief Disable Content Restrictions - unlock
 */
void PrivateInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	AcquireStreamLock();
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->DisableContentRestrictions(grace, time, eventChange);
#ifdef AAMP_CC_ENABLED
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			AampCCManager::GetInstance()->SetParentalControlStatus(false);
		}
#endif
	}
	ReleaseStreamLock();
}

/**
 *  @brief Enable Content Restrictions - lock
 */
void PrivateInstanceAAMP::EnableContentRestrictions()
{
	AcquireStreamLock();
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->EnableContentRestrictions();
	}
	ReleaseStreamLock();
}


/**
 *  @brief Add async task to scheduler
 */
int PrivateInstanceAAMP::ScheduleAsyncTask(IdleTask task, void *arg, std::string taskName)
{
	int taskId = AAMP_TASK_ID_INVALID;
	if (mScheduler)
	{
		taskId = mScheduler->ScheduleTask(AsyncTaskObj(task, arg, taskName));
		if (taskId == AAMP_TASK_ID_INVALID)
		{
			AAMPLOG_ERR("mScheduler returned invalid ID, dropping the schedule request!");
		}
	}
	else
	{
		taskId = g_idle_add(task, (gpointer)arg);
	}
	return taskId;
}

/**
 * @brief Remove async task scheduled earlier
 */
bool PrivateInstanceAAMP::RemoveAsyncTask(int taskId)
{
	bool ret = false;
	if (mScheduler)
	{
		ret = mScheduler->RemoveTask(taskId);
	}
	else
	{
		ret = g_source_remove(taskId);
	}
	return ret;
}


/**
 *  @brief acquire streamsink lock
 */
void PrivateInstanceAAMP::AcquireStreamLock()
{
	pthread_mutex_lock(&mStreamLock);
}

/**
 * @brief try to acquire streamsink lock
 *
 */
bool PrivateInstanceAAMP::TryStreamLock()
{
	return (pthread_mutex_trylock(&mStreamLock) == 0);
}

/**
 * @brief release streamsink lock
 *
 */
void PrivateInstanceAAMP::ReleaseStreamLock()
{
	pthread_mutex_unlock(&mStreamLock);
}

/**
 * @brief To check if auxiliary audio is enabled
 */
bool PrivateInstanceAAMP::IsAuxiliaryAudioEnabled(void)
{
	return !mAuxAudioLanguage.empty();
}

/**
 * @brief Check if discontinuity processed in all tracks
 *
 */
bool PrivateInstanceAAMP::DiscontinuitySeenInAllTracks()
{
	// Check if track is disabled or if mProcessingDiscontinuity is set
	// Split off the logical expression for better clarity
	bool vidDiscontinuity = (mVideoFormat == FORMAT_INVALID || mProcessingDiscontinuity[eMEDIATYPE_VIDEO]);
	bool audDiscontinuity = (mAudioFormat == FORMAT_INVALID || mProcessingDiscontinuity[eMEDIATYPE_AUDIO]);
	bool auxDiscontinuity = (mAuxFormat == FORMAT_INVALID || mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO]);

	return (vidDiscontinuity && auxDiscontinuity && auxDiscontinuity);
}

/**
 *   @brief Check if discontinuity processed in any track
 */
bool PrivateInstanceAAMP::DiscontinuitySeenInAnyTracks()
{
	// Check if track is enabled and if mProcessingDiscontinuity is set
	// Split off the logical expression for better clarity
	bool vidDiscontinuity = (mVideoFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_VIDEO]);
	bool audDiscontinuity = (mAudioFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_AUDIO]);
	bool auxDiscontinuity = (mAuxFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO]);

	return (vidDiscontinuity || auxDiscontinuity || auxDiscontinuity);
}

/**
 * @brief Reset discontinuity flag for all tracks
 */
void PrivateInstanceAAMP::ResetDiscontinuityInTracks()
{
	mProcessingDiscontinuity[eMEDIATYPE_VIDEO] = false;
	mProcessingDiscontinuity[eMEDIATYPE_AUDIO] = false;
	mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO] = false;
}

/**
 *  @brief set preferred Audio Language properties like language, rendition, type and codec
 */
void PrivateInstanceAAMP::SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList )
{

	/**< First argment is Json data then parse it and and assign the variables properly*/
	AampJsonObject* jsObject = NULL;
	bool isJson = false;
	bool isRetuneNeeded = false;
	bool accessibilityPresent = false;
	try
	{	
		jsObject = new AampJsonObject(languageList);
		if (jsObject)
		{
			AAMPLOG_INFO("Preferred Language Properties recieved as json : %s", languageList);
			isJson = true;
		}
	}
	catch(const std::exception& e)
	{
		/**<Nothing to do exclude it*/
	}

	if (isJson)
	{
		std::vector<std::string> inputLanguagesList;
		std::string inputLanguagesString;
		
		/** Get language Properties*/
		if (jsObject->isArray("languages"))
		{
			if (jsObject->get("languages", inputLanguagesList))
			{
				for (auto preferredLanguage : inputLanguagesList)
				{
					if (!inputLanguagesString.empty())
					{
						inputLanguagesString += ",";
					}
					inputLanguagesString += preferredLanguage; 
				}
			}
		}
		else if (jsObject->isString("languages"))
		{
			if (jsObject->get("languages", inputLanguagesString))
			{
				inputLanguagesList.push_back(inputLanguagesString);
			}
		}
		else
		{
			AAMPLOG_ERR("Preferred Audio Language Field Only support String or String Array");
		}

		AAMPLOG_INFO("Number of preferred languages recieved: %d", inputLanguagesList.size());
		AAMPLOG_INFO("Preferred language string recieved: %s", inputLanguagesString.c_str());

		std::string inputLabelsString;
		/** Get Label Properties*/
		if (jsObject->isString("label"))
		{
			if (jsObject->get("label", inputLabelsString))
			{
				AAMPLOG_INFO("Preferred Label string: %s", inputLabelsString.c_str());	
			}
		}

		string inputRenditionString;

		/** Get rendition or role Properties*/
		if (jsObject->isString("rendition"))
		{
			if (jsObject->get("rendition", inputRenditionString))
			{
				AAMPLOG_INFO("Preferred rendition string: %s", inputRenditionString.c_str());	
			}
		}

		Accessibility  inputAudioAccessibilityNode;
		/** Get accessibility Properties*/
		if (jsObject->isObject("accessibility"))
		{
			AampJsonObject accessNode;
			if (jsObject->get("accessibility", accessNode))
			{
				inputAudioAccessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode(accessNode);
				if (!inputAudioAccessibilityNode.getSchemeId().empty())
				{
					AAMPLOG_INFO("Preferred accessibility SchemeId: %s", inputAudioAccessibilityNode.getSchemeId().c_str());	
					if (inputAudioAccessibilityNode.getTypeName() == "string_value")
					{
						AAMPLOG_INFO("Preferred accessibility Value Type %s and Value: %s", inputAudioAccessibilityNode.getTypeName().c_str(), 
							inputAudioAccessibilityNode.getStrValue().c_str());
					}
					else
					{
						AAMPLOG_INFO("Preferred accessibility Value Type %s and Value : %d", inputAudioAccessibilityNode.getTypeName().c_str(), 
					 		inputAudioAccessibilityNode.getIntValue());
					}
				}	
			}
			if(preferredAudioAccessibilityNode != inputAudioAccessibilityNode )
			{
				accessibilityPresent = true;
			}
		}

		/**< Release json object **/
		delete jsObject;
		jsObject = NULL;

		if ((preferredAudioAccessibilityNode != inputAudioAccessibilityNode ) || (preferredRenditionString != inputRenditionString ) || 
		(preferredLabelsString != inputLabelsString) || (inputLanguagesList != preferredLanguagesList ))
		{
			isRetuneNeeded = true;
		}

		/** Clear the cache **/
		preferredAudioAccessibilityNode.clear();
		preferredLabelsString.clear();
		preferredRenditionString.clear();
		preferredLanguagesString.clear();
		preferredLanguagesList.clear();
		
		/** Reload the new values **/
		preferredAudioAccessibilityNode = inputAudioAccessibilityNode;
		preferredRenditionString = inputRenditionString;
		preferredLabelsString = inputLabelsString;
		preferredLanguagesList = inputLanguagesList;
		preferredLanguagesString = inputLanguagesString;

		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioRendition,preferredRenditionString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLabel,preferredLabelsString);	
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLanguage,preferredLanguagesString);
	}
	else
	{
		if((languageList && preferredLanguagesString != languageList) ||
		(preferredRendition && preferredRenditionString != preferredRendition) ||
		(preferredType && preferredTypeString != preferredType) ||
		(codecList && preferredCodecString != codecList) ||
		(labelList && preferredLabelsString != labelList)
		) 
		{
			isRetuneNeeded = true;
			if(languageList != NULL)
			{
				preferredLanguagesString.clear();
				preferredLanguagesList.clear();
				preferredLanguagesString = std::string(languageList);
				std::istringstream ss(preferredLanguagesString);
				std::string lng;
				while(std::getline(ss, lng, ','))
				{
					preferredLanguagesList.push_back(lng);
					AAMPLOG_INFO("Parsed preferred lang: %s", lng.c_str());
				}

				preferredLanguagesString = std::string(languageList);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLanguage,preferredLanguagesString);
			}

			AAMPLOG_INFO("Number of preferred languages: %d", preferredLanguagesList.size());
			
			if(labelList != NULL)
			{
				preferredLabelsString.clear();
				preferredLabelList.clear();
				preferredLabelsString = std::string(labelList);
				std::istringstream ss(preferredLabelsString);
				std::string lab;
				while(std::getline(ss, lab, ','))
				{
					preferredLabelList.push_back(lab);
					AAMPLOG_INFO("Parsed preferred label: %s", lab.c_str());
				}

				preferredLabelsString = std::string(labelList);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLabel,preferredLabelsString);
				AAMPLOG_INFO("Number of preferred labels: %d", preferredLabelList.size());
			}

			if( preferredRendition )
			{
				AAMPLOG_INFO("Setting rendition %s", preferredRendition);
				preferredRenditionString = std::string(preferredRendition);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioRendition,preferredRenditionString);
			}
			else
			{
				preferredRenditionString.clear();
			}

			if( preferredType )
			{
				preferredTypeString = std::string(preferredType);
				std::string delim = "_";
				auto pos = preferredTypeString.find(delim);
				auto end = preferredTypeString.length();
				if (pos != std::string::npos)
				{
					preferredTypeString =  preferredTypeString.substr(pos+1, end);
				}
				AAMPLOG_INFO("Setting accessibility type %s", preferredTypeString.c_str());
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING, eAAMPConfig_PreferredAudioType, preferredTypeString);
			}
			else
			{
				preferredTypeString.clear();
			}

			if(codecList != NULL)
			{
				preferredCodecString.clear();
				preferredCodecList.clear();
				preferredCodecString = std::string(codecList);
				std::istringstream ss(preferredCodecString);
				std::string codec;
				while(std::getline(ss, codec, ','))
				{
					preferredCodecList.push_back(codec);
					AAMPLOG_INFO("Parsed preferred codec: %s", codec.c_str());
				}
				preferredCodecString = std::string(codecList);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioCodec,preferredCodecString);
			}
			AAMPLOG_INFO("Number of preferred codecs: %d", preferredCodecList.size());
		}
		else
		{
			AAMPLOG_INFO("Discarding Retune set lanuage(s) (%s) , rendition (%s) and accessibility (%s) since already set",
			languageList?languageList:"", preferredRendition?preferredRendition:"", preferredType?preferredType:"");
		}
	}

	PrivAAMPState state;
	GetState(state);
	if (state != eSTATE_IDLE && state != eSTATE_RELEASED && state != eSTATE_ERROR && isRetuneNeeded)
	{ // active playback session; apply immediately
		if (mpStreamAbstractionAAMP)
		{
			bool languagePresent = false;
			bool renditionPresent = false;
			bool accessibilityTypePresent = false;
			bool codecPresent = false;
			bool labelPresent = false;
			int trackIndex = GetAudioTrack();

			bool languageAvailabilityInManifest = false;
			bool renditionAvailabilityInManifest = false;
			bool accessibilityAvailabilityInManifest = false;
			bool labelAvailabilityInManifest = false;
			std::string trackIndexStr;

			if (trackIndex >= 0)
			{
				std::vector<AudioTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableAudioTracks();
				char *currentPrefLanguage = const_cast<char*>(trackInfo[trackIndex].language.c_str());
				char *currentPrefRendition = const_cast<char*>(trackInfo[trackIndex].rendition.c_str());
				char *currentPrefAccessibility = const_cast<char*>(trackInfo[trackIndex].accessibilityType.c_str());
				char *currentPrefCodec = const_cast<char*>(trackInfo[trackIndex].codec.c_str());
				char *currentPrefLabel = const_cast<char*>(trackInfo[trackIndex].label.c_str());

				// Logic to check whether the given language is present in the available tracks,
				// if available, it should not match with current preferredLanguagesString, then call tune to reflect the language change.
				// if not available, then avoid calling tune.
				if(preferredLanguagesList.size() > 0)
				{
					std::string firstLanguage = preferredLanguagesList.at(0);
					auto language = std::find_if(trackInfo.begin(), trackInfo.end(),
								[firstLanguage, currentPrefLanguage](AudioTrackInfo& temp)
								{ return ((temp.language == firstLanguage) && (temp.language != currentPrefLanguage)); });
					languagePresent = (language != end(trackInfo) || (preferredLanguagesList.size() > 1)); /* If multiple value of language is present then retune */
					auto languageAvailable = std::find_if(trackInfo.begin(), trackInfo.end(),
								[firstLanguage, currentPrefLanguage](AudioTrackInfo& temp)
								{ return ((temp.language == firstLanguage) && (temp.language != currentPrefLanguage) && (temp.isAvailable)); });
					languageAvailabilityInManifest = (languageAvailable != end(trackInfo) && languageAvailable->isAvailable);
                                        if(languagePresent && (language != end(trackInfo)))
					{
						trackIndexStr = language->index;
					}
				}

				// Logic to check whether the given label is present in the available tracks,
				// if available, it should not match with current preferredLabelsString, then call retune to reflect the language change.
				// if not available, then avoid calling tune. Call retune if multiple labels is present
				if(!preferredLabelsString.empty())
				{
					std::string curLabel = preferredLabelsString;
					auto label = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curLabel, currentPrefLabel](AudioTrackInfo& temp)
								{ return ((temp.label == curLabel) && (temp.label != currentPrefLabel)); });
					labelPresent = (label != end(trackInfo) || (preferredLabelList.size() > 1)); /* If multiple value of label is present then retune */
					auto labelAvailable = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curLabel, currentPrefLabel](AudioTrackInfo& temp)
								{ return ((temp.label == curLabel) && (temp.label != currentPrefLabel) && (temp.isAvailable)); });
					labelAvailabilityInManifest = ((labelAvailable != end(trackInfo) ) && labelAvailable->isAvailable);

				}

				
				// Logic to check whether the given rendition is present in the available tracks,
				// if available, it should not match with current preferredRenditionString, then call tune to reflect the rendition change.
				// if not available, then avoid calling tune.
				if(!preferredRenditionString.empty())
				{
					std::string curRendition = preferredRenditionString;
					auto rendition = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curRendition, currentPrefRendition](AudioTrackInfo& temp)
								{ return ((temp.rendition == curRendition) && (temp.rendition != currentPrefRendition)); });
					renditionPresent = (rendition != end(trackInfo));
					auto renditionAvailable = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curRendition, currentPrefRendition](AudioTrackInfo& temp)
								{ return ((temp.rendition == curRendition) && (temp.rendition != currentPrefRendition) && (temp.isAvailable)); });
					renditionAvailabilityInManifest = ((renditionAvailable != end(trackInfo)) && renditionAvailable->isAvailable);
				}

				// Logic to check whether the given accessibility is present in the available tracks,
				// if available, it should not match with current preferredTypeString, then call tune to reflect the accessibility change.
				// if not available, then avoid calling tune.
				if(!preferredTypeString.empty())
				{
					std:;string curType = preferredTypeString;
					auto accessType = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curType, currentPrefAccessibility](AudioTrackInfo& temp)
								{ return ((temp.accessibilityType == curType) && (temp.accessibilityType != currentPrefAccessibility)); });
					accessibilityTypePresent = (accessType != end(trackInfo));
					auto accessTypeAvailable = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curType, currentPrefAccessibility](AudioTrackInfo& temp)
								{ return ((temp.accessibilityType == curType) && (temp.accessibilityType != currentPrefAccessibility) && (temp.isAvailable)); });
					accessibilityAvailabilityInManifest = ((accessTypeAvailable != end(trackInfo)) && accessTypeAvailable->isAvailable);
				}

				// Logic to check whether the given codec is present in the available tracks,
				// if available, it should not match with current preferred codec, then call tune to reflect the codec change.
				// if not available, then avoid calling tune.
				if(preferredCodecList.size() > 0)
				{
					std::string firstCodec = preferredCodecList.at(0);
					auto codec = std::find_if(trackInfo.begin(), trackInfo.end(),
								[firstCodec, currentPrefCodec](AudioTrackInfo& temp)
								{ return ((temp.codec == firstCodec) && (temp.codec != currentPrefCodec) && (temp.isAvailable)); });
					codecPresent = (codec != end(trackInfo) || (preferredCodecList.size() > 1) ); /* If multiple value of codec is present then retune */
				}
			}

			bool clearPreference = false;
			if(isRetuneNeeded && preferredCodecList.size() == 0 && preferredTypeString.empty() && preferredRenditionString.empty() \
				&& preferredLabelsString.empty() && preferredLanguagesList.size() == 0)
			{
				/** Previouse preference set and API called to clear all preferences; so retune to make effect **/
				AAMPLOG_INFO("API to clear all preferences; retune to make it affect");
				clearPreference = true;
			}

			if(mMediaFormat == eMEDIAFORMAT_OTA)
			{
				mpStreamAbstractionAAMP->SetPreferredAudioLanguages();
			}
			else if((mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE))
			{
				/*Avoid retuning in case of HEMIIN and COMPOSITE IN*/
			}
			else if (languagePresent || renditionPresent || accessibilityTypePresent || codecPresent || labelPresent || accessibilityPresent || clearPreference) // call the tune only if there is a change in the language, rendition or accessibility.
			{
				if(!ISCONFIGSET_PRIV(eAAMPConfig_ChangeTrackWithoutRetune))
				{
					discardEnteringLiveEvt = true;

					seek_pos_seconds = GetPositionSeconds();
					mOffsetFromTunetimeForSAPWorkaround = (double)(aamp_GetCurrentTimeMS() / 1000) - mLiveOffset;
					mLanguageChangeInProgress = true;
					AcquireStreamLock();
					TeardownStream(false);
					if(IsTSBSupported() &&
					 ((languagePresent && !languageAvailabilityInManifest) ||
					 (renditionPresent && !renditionAvailabilityInManifest) ||
					 (accessibilityTypePresent && !accessibilityAvailabilityInManifest) ||
					 (labelPresent && !labelAvailabilityInManifest)))
					{
						ReloadTSB();
					}
					TuneHelper(eTUNETYPE_SEEK);
					discardEnteringLiveEvt = false;
					ReleaseStreamLock();
				}
				else if(!trackIndexStr.empty())
				{
					mpStreamAbstractionAAMP->ChangeMuxedAudioTrackIndex(trackIndexStr);
				}
			}
		}
	}
}

/**
 *  @brief Set Preferred Text Language
 */
void PrivateInstanceAAMP::SetPreferredTextLanguages(const char *param )
{

	/**< First argment is Json data then parse it and and assign the variables properly*/
	AampJsonObject* jsObject = NULL;
	bool isJson = false;
	bool isRetuneNeeded = false;
	bool accessibilityPresent = false;
	try
	{
		jsObject = new AampJsonObject(param);
		if (jsObject)
		{
			AAMPLOG_INFO("Preferred Text Language Properties recieved as json : %s", param);
			isJson = true;
		}
	}
	catch(const std::exception& e)
	{
		/**<Nothing to do exclude it*/
	}

	if (isJson)
	{
		std::vector<std::string> inputTextLanguagesList;
		std::string inputTextLanguagesString;
		
		/** Get language Properties*/
		if(jsObject->isArray("languages"))
		{ // if starting with array, join to string
			if (jsObject->get("languages", inputTextLanguagesList))
			{
				for (auto preferredLanguage : inputTextLanguagesList)
				{
					if (!inputTextLanguagesString.empty())
					{
						inputTextLanguagesString += "," ;
					}
					inputTextLanguagesString += preferredLanguage; 	
				}
			}
		}
		else if (jsObject->isString("languages"))
		{ // if starting with string, create simple array
			if (jsObject->get("languages", inputTextLanguagesString))
			{
				inputTextLanguagesList.push_back(inputTextLanguagesString);
			}
		}
		else
		{
			AAMPLOG_ERR("Preferred Text Language Field Only support String or String Array");
		}

		AAMPLOG_INFO("Number of preferred Text languages: %d", inputTextLanguagesList.size());
		AAMPLOG_INFO("Preferred Text languages string: %s", inputTextLanguagesString.c_str());

		std::string inputTextRenditionString;
		/** Get rendition or role Properties*/
		if (jsObject->isString("rendition"))
		{
			if (jsObject->get("rendition", inputTextRenditionString))
			{
				AAMPLOG_INFO("Preferred text rendition string: %s", inputTextRenditionString.c_str());	
			}
		}

		std::string inputTextLabelString;
		/** Get label Properties*/
		if (jsObject->isString("label"))
		{
			if (jsObject->get("label", inputTextLabelString))
			{
				AAMPLOG_INFO("Preferred text label string: %s", inputTextLabelString.c_str());	
			}
		}

		std::string inputTextTypeString;
		/** Get accessibility type Properties*/
		if (jsObject->isString("accessibilityType"))
		{
			if (jsObject->get("accessibilityType", inputTextTypeString))
			{
				AAMPLOG_INFO("Preferred text type string: %s", inputTextTypeString.c_str());	
			}
		}

		Accessibility  inputTextAccessibilityNode;
		/** Get accessibility Properties*/
		if (jsObject->isObject("accessibility"))
		{
			AampJsonObject accessNode;
			if (jsObject->get("accessibility", accessNode))
			{
				inputTextAccessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode(accessNode);
				if (!inputTextAccessibilityNode.getSchemeId().empty())
				{
					AAMPLOG_INFO("Preferred accessibility SchemeId: %s", inputTextAccessibilityNode.getSchemeId().c_str());	
					if (inputTextAccessibilityNode.getTypeName() == "string_value")
					{
						AAMPLOG_INFO("Preferred accessibility Value Type %s and Value: %s", inputTextAccessibilityNode.getTypeName().c_str(), 
							inputTextAccessibilityNode.getStrValue().c_str());
					}
					else
					{
						AAMPLOG_INFO("Preferred accessibility Value Type %s and Value : %d", inputTextAccessibilityNode.getTypeName().c_str(), 
					 		inputTextAccessibilityNode.getIntValue());
					}
				}
				if(inputTextAccessibilityNode != preferredTextAccessibilityNode)
				{
					accessibilityPresent = true;
				}
			}	
		}
		
		/**< Release json object **/
		delete jsObject;
		jsObject = NULL;

		if((inputTextLanguagesList != preferredTextLanguagesList) || (inputTextRenditionString != preferredTextRenditionString) || 
		(inputTextAccessibilityNode != preferredTextAccessibilityNode))
		{
			isRetuneNeeded = true;
		}

		preferredTextLanguagesList.clear();
		preferredTextLanguagesString.clear();
		preferredTextRenditionString.clear();
		preferredTextAccessibilityNode.clear();
		preferredTextLabelString.clear();

		preferredTextLanguagesList = inputTextLanguagesList;
		preferredTextLanguagesString = inputTextLanguagesString;
		preferredTextRenditionString = inputTextRenditionString;
		preferredTextAccessibilityNode = inputTextAccessibilityNode;
		preferredTextLabelString = inputTextLabelString;
		preferredTextTypeString = inputTextTypeString;
		
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextLanguage,preferredTextLanguagesString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextRendition,preferredTextRenditionString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextLabel,preferredTextLabelString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextType,preferredTextTypeString);
	}
	else if( param )
	{
		AAMPLOG_INFO("Setting Text Languages  %s", param);
		std::string inputTextLanguagesString;
		inputTextLanguagesString = std::string(param);
		
		if (inputTextLanguagesString != preferredTextLanguagesString)
		{
			isRetuneNeeded = true;
		}
		preferredTextLanguagesList.clear();
		preferredTextLanguagesList.push_back(inputTextLanguagesString);
		preferredTextLanguagesString = inputTextLanguagesString;
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING, eAAMPConfig_PreferredTextLanguage, preferredTextLanguagesString);
	}
	else
	{
		AAMPLOG_INFO("No valid Parameter Recieved");
		return;
	}

	PrivAAMPState state;
	GetState(state);
	if (state != eSTATE_IDLE && state != eSTATE_RELEASED && state != eSTATE_ERROR && isRetuneNeeded )
	{ // active playback session; apply immediately
		if (mpStreamAbstractionAAMP)
		{
			bool languagePresent = false;
			bool renditionPresent = false;
			bool accessibilityTypePresent = false;
			bool codecPresent = false;
			bool labelPresent = false;
			int trackIndex = GetTextTrack();
			bool languageAvailabilityInManifest = false;
			bool renditionAvailabilityInManifest = false;
			bool accessibilityAvailabilityInManifest = false;
			bool labelAvailabilityInManifest = false;
			bool trackNotEnabled = false;
			
			if (trackIndex >= 0)
			{
				std::vector<TextTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableTextTracks();
				char *currentPrefLanguage = const_cast<char*>(trackInfo[trackIndex].language.c_str());
				char *currentPrefRendition = const_cast<char*>(trackInfo[trackIndex].rendition.c_str());

				// Logic to check whether the given language is present in the available tracks,
				// if available, it should not match with current preferredLanguagesString, then call tune to reflect the language change.
				// if not available, then avoid calling tune.
				if(preferredTextLanguagesList.size() > 0)
				{
					std::string firstLanguage = preferredTextLanguagesList.at(0);
					auto language = std::find_if(trackInfo.begin(), trackInfo.end(),
								[firstLanguage, currentPrefLanguage](TextTrackInfo& temp)
								{ return ((temp.language == firstLanguage) && (temp.language != currentPrefLanguage)); });
					languagePresent = (language != end(trackInfo) || (preferredTextLanguagesList.size() > 1)); /* If multiple value of language is present then retune */
					auto languageAvailable = std::find_if(trackInfo.begin(), trackInfo.end(),
								[firstLanguage, currentPrefLanguage](TextTrackInfo& temp)
								{ return ((temp.language == firstLanguage) && (temp.language != currentPrefLanguage) && (temp.isAvailable)); });
					languageAvailabilityInManifest = (languageAvailable != end(trackInfo) && languageAvailable->isAvailable);
				}

				// Logic to check whether the given rendition is present in the available tracks,
				// if available, it should not match with current preferredTextRenditionString, then call tune to reflect the rendition change.
				// if not available, then avoid calling tune.
				if(!preferredTextRenditionString.empty())
				{
					std::string curRendition = preferredTextRenditionString;
					auto rendition = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curRendition, currentPrefRendition](TextTrackInfo& temp)
								{ return ((temp.rendition == curRendition) && (temp.rendition != currentPrefRendition)); });
					renditionPresent = (rendition != end(trackInfo));
					auto renditionAvailable = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curRendition, currentPrefRendition](TextTrackInfo& temp)
								{ return ((temp.rendition == curRendition) && (temp.rendition != currentPrefRendition) && (temp.isAvailable)); });
					renditionAvailabilityInManifest = ((renditionAvailable != end(trackInfo)) && renditionAvailable->isAvailable);
				}
			}
			else
			{
				trackNotEnabled = true;
			}

			if((mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE) || (mMediaFormat == eMEDIAFORMAT_OTA))
			{
				/**< Avoid retuning in case of HEMIIN and COMPOSITE IN*/
			}
			else if (languagePresent || renditionPresent || accessibilityPresent || trackNotEnabled) /**< call the tune only if there is a change in the language, rendition or accessibility.*/
			{
				discardEnteringLiveEvt = true;
				seek_pos_seconds = GetPositionSeconds();
				mOffsetFromTunetimeForSAPWorkaround = (double)(aamp_GetCurrentTimeMS() / 1000) - mLiveOffset;
				mLanguageChangeInProgress = true;
				AcquireStreamLock();
				TeardownStream(false);
				if(IsTSBSupported() &&
				 ((languagePresent && !languageAvailabilityInManifest) ||
				 (renditionPresent && !renditionAvailabilityInManifest) ||
				 (accessibilityTypePresent && !accessibilityAvailabilityInManifest) ||
				 (labelPresent && !labelAvailabilityInManifest)))
				{
					ReloadTSB();
				}

				TuneHelper(eTUNETYPE_SEEK);
				discardEnteringLiveEvt = false;
				ReleaseStreamLock();
			}
		}
	}
}

/**
 * @brief Enable download activity for individual mediaType
 *
 */
void PrivateInstanceAAMP::EnableMediaDownloads(MediaType type)
{
	mMediaDownloadsEnabled[type] = true;
}

/**
 * @brief Disable download activity for individual mediaType
 */
void PrivateInstanceAAMP::DisableMediaDownloads(MediaType type)
{
	mMediaDownloadsEnabled[type] = false;
}

/**
 * @brief Enable Download activity for all mediatypes
 */
void PrivateInstanceAAMP::EnableAllMediaDownloads()
{
	for (int i = 0; i <= eMEDIATYPE_DEFAULT; i++)
	{
		// Enable downloads for all mediaTypes
		EnableMediaDownloads((MediaType) i);
	}
}

/*
 *   @brief get the WideVine KID Workaround from url
 *
 */
#define WV_KID_WORKAROUND "SkyStoreDE="

/**
 * @brief get the SkyDE Store workaround
 */
bool PrivateInstanceAAMP::IsWideVineKIDWorkaround(std::string url)
{
	bool enable = false;
	int pos = url.find(WV_KID_WORKAROUND);
	if (pos != string::npos){
		pos = pos + strlen(WV_KID_WORKAROUND);
		AAMPLOG_INFO("URL found WideVine KID Workaround at %d key = %c",
            pos, url.at(pos));
		enable = (url.at(pos) == '1');
	}

	return enable;
}

//#define ENABLE_DUMP 1 //uncomment this to enable dumping of PSSH Data

/**
 * @brief Replace KeyID from PsshData
 */ 
unsigned char* PrivateInstanceAAMP::ReplaceKeyIDPsshData(const unsigned char *InputData, const size_t InputDataLength,  size_t & OutputDataLength)
{
	unsigned char *OutpuData = NULL;
	unsigned int WIDEVINE_PSSH_KEYID_OFFSET = 36u;
	unsigned int WIDEVINE_PSSH_DATA_SIZE = 60u;
	unsigned int CK_PSSH_KEYID_OFFSET = 32u;
	unsigned int COMMON_KEYID_SIZE = 16u;
	unsigned char WVSamplePSSH[] = {
		0x00, 0x00, 0x00, 0x3c, 
		0x70, 0x73, 0x73, 0x68, 
		0x00, 0x00, 0x00, 0x00, 
		0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce, 
		0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed, 
		0x00, 0x00, 0x00, 0x1c, 0x08, 0x01, 0x12, 0x10,  
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //dummy KeyId (16 byte)
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //dummy KeyId (16 byte)
		0x22, 0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x37
	};
	if (InputData){
		AAMPLOG_INFO("Converting system UUID of PSSH data size (%d)", InputDataLength);
#ifdef ENABLE_DUMP
		AAMPLOG_INFO("PSSH Data (%d) Before Modification : ", InputDataLength);
		DumpBlob(InputData, InputDataLength);
#endif

		/** Replace KeyID of WV PSSH Data with Key ID of CK PSSH Data **/
		int iWVpssh = WIDEVINE_PSSH_KEYID_OFFSET; 
		int CKPssh = CK_PSSH_KEYID_OFFSET;
		int size = 0;
		if (CK_PSSH_KEYID_OFFSET+COMMON_KEYID_SIZE <=  InputDataLength){
			for (; size < COMMON_KEYID_SIZE; ++size, ++iWVpssh, ++CKPssh  ){
				/** Transfer KeyID from CK PSSH data to WV PSSH Data **/
				WVSamplePSSH[iWVpssh] = InputData[CKPssh];
			}

			/** Allocate WV PSSH Data memory and transfer local data **/
			OutpuData = (unsigned char *)malloc(sizeof(WVSamplePSSH));
			if (OutpuData){
				memcpy(OutpuData, WVSamplePSSH, sizeof(WVSamplePSSH));
				OutputDataLength = sizeof(WVSamplePSSH);
#ifdef ENABLE_DUMP
				AAMPLOG_INFO("PSSH Data (%d) after Modification : ", OutputDataLength);
				DumpBlob(OutpuData, OutputDataLength);
#endif
				return OutpuData;

			}else{
				AAMPLOG_ERR("PSSH Data Memory allocation failed ");
			}
		}else{
			//Invalid PSSH data
			AAMPLOG_ERR("Invalid Clear Key PSSH data ");
		}
	}else{
		//Inalid argument - PSSH Data
		AAMPLOG_ERR("Invalid Argument of PSSH data ");
	}
	return NULL;
}

/**
 * @brief Check if segment starts with an ID3 section
 */
bool PrivateInstanceAAMP::hasId3Header(const uint8_t* data, uint32_t length)
{
	if (length >= 3)
	{
		/* Check file identifier ("ID3" = ID3v2) and major revision matches (>= ID3v2.2.x). */
		if (*data++ == 'I' && *data++ == 'D' && *data++ == '3' && *data++ >= 2)
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief Process the ID3 metadata from segment
 */
void PrivateInstanceAAMP::ProcessID3Metadata(char *segment, size_t size, MediaType type, uint64_t timeStampOffset)
{
	// Logic for ID3 metadata
	if(segment && mEventManager->IsEventListenerAvailable(AAMP_EVENT_ID3_METADATA))
	{
		IsoBmffBuffer buffer(mLogObj);
		buffer.setBuffer((uint8_t *)segment, size);
		buffer.parseBuffer();
		if(!buffer.isInitSegment())
		{
			uint8_t* message = nullptr;
			uint32_t messageLen = 0;
			uint8_t* schemeIDUri = nullptr;
			uint8_t* value = nullptr;
			uint64_t presTime = 0;
			uint32_t timeScale = 0;
			uint32_t eventDuration = 0;
			uint32_t id = 0;
			if(buffer.getEMSGData(message, messageLen, schemeIDUri, value, presTime, timeScale, eventDuration, id))
			{
				if(message && messageLen > 0 && hasId3Header(message, messageLen))
				{
					AAMPLOG_TRACE("PrivateInstanceAAMP: Found ID3 metadata[%d]", type);
					if(mMediaFormat == eMEDIAFORMAT_DASH)
					{
						ReportID3Metadata(type, message, messageLen, (char*)(schemeIDUri), (char*)(value), presTime, id, eventDuration, timeScale, GetMediaStreamContext(type)->timeStampOffset);
					}else
					{
						ReportID3Metadata(type, message, messageLen, (char*)(schemeIDUri), (char*)(value), presTime, id, eventDuration, timeScale);
					}
				}
			}
		}
	}
}

/**
 * @brief Report ID3 metadata events
 */
void PrivateInstanceAAMP::ReportID3Metadata(MediaType mediaType, const uint8_t* ptr, uint32_t len, const char* schemeIdURI, const char* id3Value, uint64_t presTime, uint32_t id3ID, uint32_t eventDur, uint32_t tScale, uint64_t tStampOffset)
{
	FlushLastId3Data(mediaType);
	Id3CallbackData* id3Metadata = new Id3CallbackData(this, static_cast<const uint8_t*>(ptr), len, static_cast<const char*>(schemeIdURI), static_cast<const char*>(id3Value), presTime, id3ID, eventDur, tScale, tStampOffset);
	lastId3Data[mediaType] = (uint8_t*)g_malloc(len);
	if (lastId3Data[mediaType])
	{
		lastId3DataLen[mediaType] = len;
		memcpy(lastId3Data[mediaType], ptr, len);
	}

	SendId3MetadataEvent(id3Metadata);
	SAFE_DELETE(id3Metadata);
}

/**
 * @brief Flush last saved ID3 metadata
 */ 
void PrivateInstanceAAMP::FlushLastId3Data(MediaType mediaType)
{
	if(lastId3Data[mediaType])
	{
		lastId3DataLen[mediaType] = 0;
		g_free(lastId3Data[mediaType]);
		lastId3Data[mediaType] = NULL;
	}
}

/**
 * @brief GetPauseOnFirstVideoFrameDisplay
 */
bool PrivateInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
    return mPauseOnFirstVideoFrameDisp;
}

/** 
 * @brief Sets  Low Latency Service Data
 */
void PrivateInstanceAAMP::SetLLDashServiceData(AampLLDashServiceData &stAampLLDashServiceData)
{
    this->mAampLLDashServiceData = stAampLLDashServiceData;
}

/**
 * @brief Gets Low Latency Service Data
 */
AampLLDashServiceData*  PrivateInstanceAAMP::GetLLDashServiceData(void)
{
    return &this->mAampLLDashServiceData;
}


/**
 * @brief Sets Low Video TimeScale
 */
void PrivateInstanceAAMP::SetVidTimeScale(uint32_t vidTimeScale)
{
    this->vidTimeScale = vidTimeScale;
}

/**
 * @brief Gets Video TimeScale
 */
uint32_t  PrivateInstanceAAMP::GetVidTimeScale(void)
{
    return vidTimeScale;
}

/**
 * @brief Sets Low Audio TimeScale
 */
void PrivateInstanceAAMP::SetAudTimeScale(uint32_t audTimeScale)
{
    this->audTimeScale = audTimeScale;
}

/**
 * @brief Gets Audio TimeScale
 */
uint32_t  PrivateInstanceAAMP::GetAudTimeScale(void)
{
    return audTimeScale;
}
/**
 * @brief Sets Speed Cache
 */
void PrivateInstanceAAMP::SetLLDashSpeedCache(struct SpeedCache &speedCache)
{
    this->speedCache = speedCache;
}

/**
 * @brief Gets Speed Cache
 */
struct SpeedCache* PrivateInstanceAAMP::GetLLDashSpeedCache()
{
    return &speedCache;
}

bool PrivateInstanceAAMP::GetLiveOffsetAppRequest()
{
    return mLiveOffsetAppRequest;
}

/**
 * @brief set LiveOffset Request flag Status
 */
void PrivateInstanceAAMP::SetLiveOffsetAppRequest(bool LiveOffsetAppRequest)
{
    this->mLiveOffsetAppRequest = LiveOffsetAppRequest;
}
/**
 *  @brief Get Low Latency Service Configuration Status
 */
bool PrivateInstanceAAMP::GetLowLatencyServiceConfigured()
{
    return bLowLatencyServiceConfigured;
}

/**
 *  @brief Set Low Latency Service Configuration Status
 */
void PrivateInstanceAAMP::SetLowLatencyServiceConfigured(bool bConfig)
{
    bLowLatencyServiceConfigured = bConfig;
}

/**
 *  @brief Get Utc Time
 */
time_t PrivateInstanceAAMP::GetUtcTime()
{
    return mTime;
}

/**
 *  @brief Set Utc Time
 */
void PrivateInstanceAAMP::SetUtcTime(time_t time)
{
    this->mTime = time;
}

/**
 *  @brief Get Current Latency
 */
long PrivateInstanceAAMP::GetCurrentLatency()
{
    return mCurrentLatency;
}

/**
 * @brief Set Current Latency
 */
void PrivateInstanceAAMP::SetCurrentLatency(long currentLatency)
{
    this->mCurrentLatency = currentLatency;
}

/**
 *     @brief Get Media Stream Context
 */
MediaStreamContext* PrivateInstanceAAMP::GetMediaStreamContext(MediaType type)
{
    if(mpStreamAbstractionAAMP &&
    (type == eMEDIATYPE_VIDEO ||
     type == eMEDIATYPE_AUDIO ||
     type == eMEDIATYPE_SUBTITLE ||
     type == eMEDIATYPE_AUX_AUDIO))
    {
        MediaStreamContext* context = (MediaStreamContext*)mpStreamAbstractionAAMP->GetMediaTrack((TrackType)type);
        return context;
    }
    return NULL;
}

/**
 *  @brief GetPeriodDurationTimeValue
 */
double PrivateInstanceAAMP::GetPeriodDurationTimeValue(void)
{
        return mNextPeriodDuration;
}

/**
 *  @brief GetPeriodStartTimeValue
 */
double PrivateInstanceAAMP::GetPeriodStartTimeValue(void)
{
        return mNextPeriodStartTime;
}

/**
 *  @brief GetPeriodScaledPtoStartTime
 */
double PrivateInstanceAAMP::GetPeriodScaledPtoStartTime(void)
{
       return mNextPeriodScaledPtoStartTime;
}

/**
 *  @brief Get playback stats for the session so far
 */
std::string PrivateInstanceAAMP::GetPlaybackStats()
{
	std::string strVideoStatsJson;
#ifdef SESSION_STATS
	long liveLatency = 0;
	//Update liveLatency only when playback is active and live
	if(mpStreamAbstractionAAMP && IsLive())
		liveLatency = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec() * 1000.0;

	if(mVideoEnd)
	{
		mVideoEnd->setPlaybackMode(mPlaybackMode);
		mVideoEnd->setLiveLatency(liveLatency);
		mVideoEnd->SetDisplayResolution(mDisplayWidth,mDisplayHeight);
		//Update VideoEnd Data
		if(mTimeAtTopProfile > 0)
		{
			// Losing milisecons of data in conversion from double to long
			mVideoEnd->SetTimeAtTopProfile(mTimeAtTopProfile);
			mVideoEnd->SetTimeToTopProfile(mTimeToTopProfile);
		}
		mVideoEnd->SetTotalDuration(mPlaybackDuration);
		char * videoStatsPtr = mVideoEnd->ToJsonString(nullptr, true);
		if(videoStatsPtr)
		{
			strVideoStatsJson = videoStatsPtr;
			free(videoStatsPtr);
		}
	}
	else
	{
		AAMPLOG_ERR("GetPlaybackStats failed, mVideoEnd is NULL");
	}
	
	if(!strVideoStatsJson.empty())
	{
		AAMPLOG_INFO("Playback stats json:%s", strVideoStatsJson.c_str());
	}
	else
	{
		AAMPLOG_ERR("Failed to retrieve playback stats (video stats returned as empty from aamp metrics)");
	}
#else
	AAMPLOG_WARN("SESSION_STATS not enabled");
#endif
	return strVideoStatsJson;
}

/**
* @brief LoadFogConfig - Load needed player Config to Fog
*/
long PrivateInstanceAAMP::LoadFogConfig()
{
	std::string jsonStr;
	AampJsonObject jsondata;
	double tmpVar = 0;
	long tmpLongVar = 0;
	int maxdownload = 0;

	// langCodePreference
	jsondata.add("langCodePreference", (int) GetLangCodePreference());

	// networkTimeout value in sec and convert into MS
	GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout,tmpVar);
	jsondata.add("downloadTimeoutMS", (long)CONVERT_SEC_TO_MS(tmpVar));
	
	tmpVar = 0;
	// manifestTimeout value in sec and convert into MS
        GETCONFIGVALUE_PRIV(eAAMPConfig_ManifestTimeout,tmpVar);
        jsondata.add("manifestTimeoutMS", (long)CONVERT_SEC_TO_MS(tmpVar));

	tmpLongVar = 0;
	//downloadStallTimeout in sec
	GETCONFIGVALUE_PRIV(eAAMPConfig_CurlStallTimeout,tmpLongVar);
	jsondata.add("downloadStallTimeout", tmpLongVar);

	tmpLongVar = 0;
	//downloadStartTimeout sec
	GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadStartTimeout,tmpLongVar);
	jsondata.add("downloadStartTimeout", tmpLongVar);

	tmpLongVar = 0;
	//downloadStartTimeout sec
	GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadLowBWTimeout,tmpLongVar);
	jsondata.add("downloadLowBWTimeout", tmpLongVar);

	//maxConcurrentDownloads
	GETCONFIGVALUE_PRIV(eAAMPConfig_FogMaxConcurrentDownloads, maxdownload);
	jsondata.add("maxConcurrentDownloads", (long)(maxdownload));

	//disableEC3
	jsondata.add("disableEC3", ISCONFIGSET_PRIV(eAAMPConfig_DisableEC3));

	//disableATMOS
	jsondata.add("disableATMOS", ISCONFIGSET_PRIV(eAAMPConfig_DisableATMOS));

	//disableAC4
	jsondata.add("disableAC4", ISCONFIGSET_PRIV(eAAMPConfig_DisableAC4));

	//persistLowNetworkBandwidth
	jsondata.add("persistLowNetworkBandwidth", ISCONFIGSET_PRIV(eAAMPConfig_PersistLowNetworkBandwidth));
	
	//disableAC3
	jsondata.add("disableAC3", ISCONFIGSET_PRIV(eAAMPConfig_DisableAC3));
	
	//persistHighNetworkBandwidth
	jsondata.add("persistHighNetworkBandwidth", ISCONFIGSET_PRIV(eAAMPConfig_PersistHighNetworkBandwidth));

	tmpLongVar = 0;
	GETCONFIGVALUE_PRIV(eAAMPConfig_MinBitrate,tmpLongVar);
	jsondata.add("minBitrate", tmpLongVar);

	tmpLongVar = 0;
	GETCONFIGVALUE_PRIV(eAAMPConfig_MaxBitrate,tmpLongVar);
	jsondata.add("maxBitrate", tmpLongVar);

	jsondata.add("enableABR", ISCONFIGSET_PRIV(eAAMPConfig_EnableABR));

	//info
	jsondata.add("info", ISCONFIGSET_PRIV(eAAMPConfig_InfoLogging));

	/*
	 * Audio and subtitle preference
	 * Disabled this for XRE supported TSB linear
	 */
	if (!ISCONFIGSET_PRIV(eAAMPConfig_XRESupportedTune))
	{
		AampJsonObject jsondataForPreference;
		AampJsonObject audioPreference;
		AampJsonObject subtitlePreference;
		bool aPrefAvail = false;
		bool tPrefAvail = false;
		if((preferredLanguagesList.size() > 0) || !preferredRenditionString.empty() || !preferredLabelsString.empty() || !preferredAudioAccessibilityNode.getSchemeId().empty())
		{
			aPrefAvail = true;
			if ((preferredLanguagesList.size() > 0) && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredAudioLanguage) > AAMP_DEFAULT_SETTING ))
			{
				audioPreference.add("languages", preferredLanguagesList);
			}
			if(!preferredRenditionString.empty() && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredAudioRendition) > AAMP_DEFAULT_SETTING ))
			{
				audioPreference.add("rendition", preferredRenditionString);
			}
			if(!preferredLabelsString.empty() && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredAudioLabel) > AAMP_DEFAULT_SETTING ))
			{
				audioPreference.add("label", preferredLabelsString);
			}
			if(!preferredAudioAccessibilityNode.getSchemeId().empty())
			{
				AampJsonObject accessiblity;
				std::string schemeId = preferredAudioAccessibilityNode.getSchemeId();
				accessiblity.add("schemeId", schemeId);
				std::string value;
				if(preferredAudioAccessibilityNode.getTypeName() == "int_value")
				{
					value = std::to_string(preferredAudioAccessibilityNode.getIntValue());
				}
				else
				{
					value = preferredAudioAccessibilityNode.getStrValue();
				}
				accessiblity.add("value", value);
				audioPreference.add("accessibility", accessiblity);
			}
		}
#if 0
        /** Time being disabled due to issues - LLAMA-7953, LLAMA-7760 **/

		if((preferredTextLanguagesList.size() > 0) || !preferredTextRenditionString.empty() || !preferredTextLabelString.empty() || !preferredTextAccessibilityNode.getSchemeId().empty())
		{
			tPrefAvail = true;
			if ((preferredTextLanguagesList.size() > 0) && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredTextLanguage) > AAMP_DEFAULT_SETTING ))
			{
				subtitlePreference.add("languages", preferredTextLanguagesList);
			}
			if(!preferredTextRenditionString.empty() && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredTextRendition) > AAMP_DEFAULT_SETTING ))
			{
				subtitlePreference.add("rendition", preferredTextRenditionString);
			}
			if(!preferredTextLabelString.empty() && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredTextLabel) > AAMP_DEFAULT_SETTING ))
			{
				subtitlePreference.add("label", preferredTextLabelString);
			}
			if(!preferredTextAccessibilityNode.getSchemeId().empty())
			{
				AampJsonObject accessiblity;
				std::string schemeId = preferredTextAccessibilityNode.getSchemeId();
				accessiblity.add("schemeId", schemeId);
				std::string value;
				if(preferredTextAccessibilityNode.getTypeName() == "int_value")
				{
					value = std::to_string(preferredTextAccessibilityNode.getIntValue());
				}
				else
				{
					value = preferredTextAccessibilityNode.getStrValue();
				}
				accessiblity.add("value", value);
				subtitlePreference.add("accessibility", accessiblity);
			}
		}
#endif
		bool trackAdded = false;
		if(aPrefAvail)
		{
			jsondataForPreference.add("audio", audioPreference);
			trackAdded = true;
		}
		if(tPrefAvail)
		{
			jsondataForPreference.add("text", subtitlePreference);
			trackAdded = true;
		}

		if(trackAdded)
		{
			jsondata.add("trackPreference", jsondataForPreference);
		}
	}
	

	jsonStr = jsondata.print_UnFormatted();
	AAMPLOG_TRACE("%s", jsonStr.c_str());
	std::string remoteUrl = "127.0.0.1:9080/playerconfig";
	long http_error = -1;
	ProcessCustomCurlRequest(remoteUrl, NULL, &http_error, eCURL_POST, jsonStr);
	return http_error;
}


/** 
 * @brief -To Load needed config from player to aampabr
 */
void PrivateInstanceAAMP::LoadAampAbrConfig()
{
	HybridABRManager::AampAbrConfig mhAampAbrConfig;
	// ABR config values
	GETCONFIGVALUE_PRIV(eAAMPConfig_ABRCacheLife,mhAampAbrConfig.abrCacheLife);
	GETCONFIGVALUE_PRIV(eAAMPConfig_ABRCacheLength,mhAampAbrConfig.abrCacheLength);
	GETCONFIGVALUE_PRIV(eAAMPConfig_ABRSkipDuration,mhAampAbrConfig.abrSkipDuration);
	GETCONFIGVALUE_PRIV(eAAMPConfig_ABRNWConsistency,mhAampAbrConfig.abrNwConsistency);
	GETCONFIGVALUE_PRIV(eAAMPConfig_ABRThresholdSize,mhAampAbrConfig.abrThresholdSize);
	GETCONFIGVALUE_PRIV(eAAMPConfig_MaxABRNWBufferRampUp,mhAampAbrConfig.abrMaxBuffer);
	GETCONFIGVALUE_PRIV(eAAMPConfig_MinABRNWBufferRampDown,mhAampAbrConfig.abrMinBuffer);
	
	// Logging level support on aampabr

	mhAampAbrConfig.infologging  = (ISCONFIGSET_PRIV(eAAMPConfig_InfoLogging)  ? 1 :0);
	mhAampAbrConfig.debuglogging = (ISCONFIGSET_PRIV(eAAMPConfig_DebugLogging) ? 1 :0);
	mhAampAbrConfig.tracelogging = (ISCONFIGSET_PRIV(eAAMPConfig_TraceLogging) ? 1:0);
	mhAampAbrConfig.warnlogging  = (ISCONFIGSET_PRIV(eAAMPConfig_WarnLogging) ? 1:0);

	mhAbrManager.ReadPlayerConfig(&mhAampAbrConfig);
}

/**
 * @brief Get License Custom Data
 */
std::string PrivateInstanceAAMP::GetLicenseCustomData()
{
    std::string customData;
    GETCONFIGVALUE_PRIV(eAAMPConfig_CustomLicenseData,customData);
    return customData;
}

/**
 * @brief check if sidecar data available
 */
bool PrivateInstanceAAMP::HasSidecarData()
{
	if (mData != NULL)
	{
		return true;
	}
	return false;
}
