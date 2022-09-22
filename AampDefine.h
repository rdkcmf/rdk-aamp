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

#ifndef __AAMP_DEFINE_H__
#define __AAMP_DEFINE_H__

/**
 * @file AampDefine.h
 * @brief Macros for Aamp
 */

#include <limits.h>

#ifdef UNIT_TEST_ENABLED
#define AAMP_CFG_PATH "aamp.cfg"
#define AAMP_JSON_PATH "aampcfg.json"
#else
#define AAMP_CFG_PATH "/opt/aamp.cfg"
#define AAMP_JSON_PATH "/opt/aampcfg.json"
#endif


#define AAMP_VERSION "4.11"
#define AAMP_TUNETIME_VERSION 5

//Stringification of Macro : use two levels of macros
#define MACRO_TO_STRING(s) X_STR(s)
#define X_STR(s) #s

#if defined(REALTEKCE)
#define GST_VIDEOBUFFER_SIZE_BYTES_BASE 5242880        		/**< XIONE-6722 more generous buffering - RealTek UHD specific */
#else
#define GST_VIDEOBUFFER_SIZE_BYTES_BASE 4194304
#endif
#define GST_AUDIOBUFFER_SIZE_BYTES_BASE 512000
#if defined(CONTENT_4K_SUPPORTED)
#define GST_VIDEOBUFFER_SIZE_BYTES (GST_VIDEOBUFFER_SIZE_BYTES_BASE*3)
#define GST_AUDIOBUFFER_SIZE_BYTES (GST_AUDIOBUFFER_SIZE_BYTES_BASE*3)
#else
#define GST_VIDEOBUFFER_SIZE_BYTES (GST_VIDEOBUFFER_SIZE_BYTES_BASE)
#define GST_AUDIOBUFFER_SIZE_BYTES (GST_AUDIOBUFFER_SIZE_BYTES_BASE)
#endif

#define DEFAULT_ENCODED_CONTENT_BUFFER_SIZE (512*1024)		/**< 512KB buffer is allocated for a content encoded curl download to minimize buffer reallocation*/
#define MAX_PTS_ERRORS_THRESHOLD 4
#define DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS (1000)  	/**< Wait time in milliseconds before retry for 5xx errors */
#define MAX_PLAYLIST_CACHE_SIZE    (3*1024) 			/**< Approx 3MB -> 2 video profiles + one audio profile + one iframe profile, 500-700K MainManifest */

#define DEFAULT_ABR_CACHE_LIFE 5000                 		/**< Default ABR cache life  in milli secs*/
#define DEFAULT_ABR_OUTLIER 5000000                 		/**< ABR outlier: 5 MB */
#define DEFAULT_ABR_SKIP_DURATION 6          		        /**< Initial skip duration of ABR - 6 sec */
#define DEFAULT_ABR_NW_CONSISTENCY_CNT 2            		/**< ABR network consistency count */
#define DEFAULT_BUFFER_HEALTH_MONITOR_DELAY 10
#define DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL 5
#define DEFAULT_ABR_CACHE_LENGTH 3                  		/**< Default ABR cache length */
#define DEFAULT_REPORT_PROGRESS_INTERVAL 1     			/**< Progress event reporting interval: 1sec */
#define DEFAULT_LICENSE_REQ_RETRY_WAIT_TIME 500			/**< Wait time in milliseconds before retrying for DRM license */
#define MIN_LICENSE_KEY_ACQUIRE_WAIT_TIME 500			/**<minimum wait time in milliseconds for DRM license to ACQUIRE */
#define DEFAULT_LICENSE_KEY_ACQUIRE_WAIT_TIME 5000		/**< Wait time in milliseconds for DRM license to ACQUIRE  */
#define MAX_LICENSE_ACQ_WAIT_TIME 12000  			/**< 12 secs Increase from 10 to 12 sec(DELIA-33528) */
#define DEFAULT_INIT_BITRATE     2500000            		/**< Initial bitrate: 2.5 mb - for non-4k playback */
#define DEFAULT_BITRATE_OFFSET_FOR_DOWNLOAD 500000		/**< Offset in bandwidth window for checking buffer download expiry */
#define DEFAULT_INIT_BITRATE_4K 13000000            		/**< Initial bitrate for 4K playback: 13mb ie, 3/4 profile */
#define AAMP_LIVE_OFFSET 15             			/**< Live offset in seconds */
#define AAMP_DEFAULT_PLAYBACK_OFFSET -99999            		/**< default 'unknown' offset value */
#define AAMP_CDVR_LIVE_OFFSET 30        			/**< Live offset in seconds for CDVR hot recording */
#define MIN_DASH_DRM_SESSIONS 3
#ifdef XIONE_UK
#define DEFAULT_CACHED_FRAGMENTS_PER_TRACK  3      	 	/**< Default cached fragements per track - decreased only for XIONE UK per XIONE-6823 */
#else
#define DEFAULT_CACHED_FRAGMENTS_PER_TRACK  4       		/**< Default cached fragements per track */
#endif
#define TRICKPLAY_VOD_PLAYBACK_FPS 4            		/**< Frames rate for trickplay from CDN server */
#define TRICKPLAY_LINEAR_PLAYBACK_FPS 8                		/**< Frames rate for trickplay from TSB */
#define DEFAULT_DOWNLOAD_RETRY_COUNT (1)			/**< max download failure retry attempt count */
#define DEFAULT_DISCONTINUITY_TIMEOUT 3000          		/**< Default discontinuity timeout after cache is empty in MS */
#define CURL_FRAGMENT_DL_TIMEOUT 10L    			/**< Curl timeout for fragment download */
#define DEFAULT_STALL_ERROR_CODE (7600)             		/**< Default stall error code: 7600 */
#define DEFAULT_STALL_DETECTION_TIMEOUT (10000)     		/**< Stall detection timeout: 10000 millisec */
#define DEFAULT_MINIMUM_INIT_CACHE_SECONDS  0        		/**< Default initial cache size of playback */
#define DEFAULT_MAXIMUM_PLAYBACK_BUFFER_SECONDS 30   		/**< Default maximum playback buffer size */
#define DEFAULT_TIMEOUT_FOR_SOURCE_SETUP (1000) 		/**< Default timeout value in milliseconds */
#define MAX_SEG_DRM_DECRYPT_FAIL_COUNT 10           		/**< Max segment decryption failures to identify a playback failure. */
#define MAX_SEG_INJECT_FAIL_COUNT 10                		/**< Max segment injection failure to identify a playback failure. */
#define AAMP_USERAGENT_BASE_STRING	"Mozilla/5.0 (Linux; x86_64 GNU/Linux) AppleWebKit/601.1 (KHTML, like Gecko) Version/8.0 Safari/601.1 WPE"	/**< Base User agent string,it will be appneded with AAMP_USERAGENT_SUFFIX */
#define AAMP_USERAGENT_SUFFIX		"AAMP/"+AAMP_VERSION    /**< Version string of AAMP Player */
#define DEFAULT_AAMP_ABR_THRESHOLD_SIZE (6000)			/**< aamp abr threshold size */
#define DEFAULT_PREBUFFER_COUNT (2)
#define AAMP_LOW_BUFFER_BEFORE_RAMPDOWN 10 			/**< 10sec buffer before rampdown */
#define AAMP_HIGH_BUFFER_BEFORE_RAMPUP  15 			/**< 15sec buffer before rampup */
#define MAX_DASH_DRM_SESSIONS 30
#define MAX_AD_SEG_DOWNLOAD_FAIL_COUNT 2            		/**< Max Ad segment download failures to identify as the ad playback failure. */
#define FRAGMENT_DOWNLOAD_WARNING_THRESHOLD 2000    		/**< MAX Fragment download threshold time in Msec*/
#define BITRATE_ALLOWED_VARIATION_BAND 500000       		/**< NW BW change beyond this will be ignored */
#define MAX_DIFF_BETWEEN_PTS_POS_MS (3600*1000)
#define MAX_SEG_DOWNLOAD_FAIL_COUNT 10              		/**< Max segment download failures to identify a playback failure. */
#define MAX_DOWNLOAD_DELAY_LIMIT_MS 30000
#define MAX_ERROR_DESCRIPTION_LENGTH 128
#define MAX_ANOMALY_BUFF_SIZE   256
#define MAX_WAIT_TIMEOUT_MS	200				/**< Max Timeout furation for wait until cache is available to inject next*/
#define MAX_INIT_FRAGMENT_CACHE_PER_TRACK  5       		/**< Max No Of cached Init fragements per track */
#define MIN_SEG_DURTION_THREASHOLD	(0.25)			/**< Min Segment Duration threshold for pushing to pipeline at period End*/
#define MAX_CURL_SOCK_STORE		10			/**< Maximum no of host to be maintained in curl store*/

// Player supported play/trick-play rates.
#define AAMP_RATE_TRICKPLAY_MAX		64
#define AAMP_NORMAL_PLAY_RATE		1
#define AAMP_SLOWMOTION_RATE        0.5
#define AAMP_RATE_PAUSE			0
#define AAMP_RATE_INVALID		INT_MAX

// Defines used for PauseAt functionality
#define AAMP_PAUSE_POSITION_POLL_PERIOD_MS		(250)
#define AAMP_PAUSE_POSITION_INVALID_POSITION	(-1)

#define STRLEN_LITERAL(STRING) (sizeof(STRING)-1)
#define STARTS_WITH_IGNORE_CASE(STRING, PREFIX) (0 == strncasecmp(STRING, PREFIX, STRLEN_LITERAL(PREFIX)))

#define MAX_GST_VIDEO_BUFFER_BYTES			(GST_VIDEOBUFFER_SIZE_BYTES)
#define MAX_GST_AUDIO_BUFFER_BYTES			(GST_AUDIOBUFFER_SIZE_BYTES)

#define DEFAULT_LATENCY_MONITOR_DELAY			9					/**< Latency Monitor Delay */
#define DEFAULT_LATENCY_MONITOR_INTERVAL		6					/**< Latency monitor Interval */
#define DEFAULT_MIN_LOW_LATENCY			3					/**< min Default Latency */
#define DEFAULT_MAX_LOW_LATENCY			9					/**< max Default Latency */
#define DEFAULT_TARGET_LOW_LATENCY			6					/**< Target Default Latency */
#define DEFAULT_MIN_RATE_CORRECTION_SPEED		0.90f					/**< min Rate correction speed */
#define DEFAULT_MAX_RATE_CORRECTION_SPEED		1.10f					/**< max Rate correction speed */
#define AAMP_NORMAL_LL_PLAY_RATE 				1.01f					/**< LL Normal play rate adjusted to 1.01 */
#define DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK	20					/**< Default cached fragement chunks per track */
#define DEFAULT_ABR_CHUNK_CACHE_LENGTH			10					/**< Default ABR chunk cache length */
#define DEFAULT_AAMP_ABR_CHUNK_THRESHOLD_SIZE		(DEFAULT_AAMP_ABR_THRESHOLD_SIZE)	/**< aamp abr Chunk threshold size */
#define DEFAULT_ABR_CHUNK_SPEEDCNT			10					/**< Chunk Speed Count Store Size */
#define DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE		100					/**< Duration(ms) to check Chunk Speed */
#define DEFAULT_ABR_BYTES_TRANSFERRED_FOR_ESTIMATE	(512 * 1024)				/**< 512K */
#define MAX_MDAT_NOT_FOUND_COUNT			500					/**< Max MDAT not found count*/
#define DEFAULT_CONTENT_PROTECTION_DATA_UPDATE_TIMEOUT	5000					/**< Default Timeout for Content Protection Data Update on Dynamic Key Rotation */

// Player configuration for Fog download
#define FOG_MAX_CONCURRENT_DOWNLOADS			1					/**< Max concurrent downloads in Fog*/

#define AAMP_MAX_EVENT_PRIORITY (-70) 	/**< Maximum allowed priority value for events */
#define AAMP_TASK_ID_INVALID 0

//License acquistion related configuration
#define MAX_LICENSE_REQUEST_ATTEMPTS 2
//Secmanager error class codes
#define SECMANGER_DRM_FAILURE 200
#define SECMANGER_WM_FAILURE 300 	/**< If secmanager couldn't initialize watermark service */

//Secmanager error reason codes
#define SECMANGER_DRM_GEN_FAILURE 1	/**< General or internal failure */
#define SECMANGER_SERVICE_TIMEOUT 3
#define SECMANGER_SERVICE_CON_FAILURE 4
#define SECMANGER_SERVICE_BUSY 5
#define SECMANGER_ACCTOKEN_EXPIRED 8
#define SECMANGER_ENTITLEMENT_FAILURE 102

//delay for the first speed set event
#define SECMANGER_SPEED_SET_DELAY 500

//DELIA-53727 change this into #define to extract the raw YCrCb colors from each frame of video
#undef RENDER_FRAMES_IN_APP_CONTEXT

/**
 * @brief Enumeration for TUNED Event Configuration
 */
enum TunedEventConfig
{
        eTUNED_EVENT_ON_PLAYLIST_INDEXED,           /**< Send TUNED event after playlist indexed*/
        eTUNED_EVENT_ON_FIRST_FRAGMENT_DECRYPTED,   /**< Send TUNED event after first fragment decryption*/
        eTUNED_EVENT_ON_GST_PLAYING,                /**< Send TUNED event on gstreamer's playing event*/
        eTUNED_EVENT_MAX
};

/**
 * @brief Enumeration for Paused state behavior
 */
enum PausedBehavior
{
	ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE,            /**< automatically begin playback from eldest portion of live window*/
	ePAUSED_BEHAVIOR_LIVE_IMMEDIATE,                /**< automatically jump to live*/
	ePAUSED_BEHAVIOR_AUTOPLAY_DEFER,                /**< video remains paused indefinitely till play() call, resume playback from new start portion of live window*/
	ePAUSED_BEHAVIOR_LIVE_DEFER,                    /**< video remains paused indefinitely till play() call, resume playback from live position*/
	ePAUSED_BEHAVIOR_MAX
};

/**
 * @brief AAMP Config Ownership values
 */

typedef enum
{
	AAMP_DEFAULT_SETTING            = 0,            /**< Lowest priority */
	AAMP_OPERATOR_SETTING           = 1,
	AAMP_STREAM_SETTING             = 2,
	AAMP_APPLICATION_SETTING        = 3,
	AAMP_TUNE_SETTING        	= 4,
	AAMP_DEV_CFG_SETTING            = 5,
	AAMP_CUSTOM_DEV_CFG_SETTING     = 6,		/**< Highest priority */
	AAMP_MAX_SETTING
}ConfigPriority;

/**
 * @brief Latency status
 */
enum LatencyStatus
{
    LATENCY_STATUS_UNKNOWN=-1,     /**< The latency is Unknown */
    LATENCY_STATUS_MIN,            /**< The latency is within range but less than mimium latency */
    LATENCY_STATUS_THRESHOLD_MIN,  /**< The latency is within range but less than target latency but greater than minimum latency */
    LATENCY_STATUS_THRESHOLD,      /**< The latency is equal to given latency from mpd */
    LATENCY_STATUS_THRESHOLD_MAX,  /**< The latency is more that target latency but less than maximum latency */
    LATENCY_STATUS_MAX             /**< The latency is more than maximum latency */
};


typedef enum {
	SECMANAGER_CLASS_RESULT_SUCCESS = 0,
	SECMANAGER_CLASS_RESULT_API_FAIL = 100,
	SECMANAGER_CLASS_RESULT_DRM_FAIL = 200,
	SECMANAGER_CLASS_RESULT_WATERMARK_FAIL = 300,
	SECMANAGER_CLASS_RESULT_SECCLIENT_FAIL = 400,
	SECMANAGER_CLASS_RESULT_UNDEFINED = 9999
} SecManagerResultClassStatusCode;

typedef enum {
	SECMANAGER_SUCCESS = 0,
	SECMANAGER_SUCCESS_WATERMARK_SESSION_ENGAGED = 100,
	SECMANAGER_SUCCESS_WATERMARK_NOT_REQUIRED = 101
} SecManagerResultSuccessCode;

typedef enum {
	SECMANAGER_REASON_API_INVALID_SESSION_CONFIG = 1,
	SECMANAGER_REASON_API_INVALID_ASPECT_DIMENSION = 2,
	SECMANAGER_REASON_API_INVALID_KEY_SYSTEM_PARAM = 3,
	SECMANAGER_REASON_API_INVALID_DRM_LICENSE_PARAM = 4,
	SECMANAGER_REASON_API_INVALID_CONTENT_METADATA = 5,
	SECMANAGER_REASON_API_INVALID_MEDIA_USAGE = 6,
	SECMANAGER_REASON_API_INVALID_ACCESS_TOKEN = 7,
	SECMANAGER_REASON_API_INVALID_ACCESS_ATTRIBUTE = 8,
	SECMANAGER_REASON_API_INVALID_SESSION_ID = 9,
	SECMANAGER_REASON_API_INVALID_APPLICATION_ID = 10,
	SECMANAGER_REASON_API_INVALID_EVENT_ID = 11,
	SECMANAGER_REASON_API_INVALID_CLIENT_ID = 12,
	SECMANAGER_REASON_API_INVALID_PERCEPTION_ID = 13,
	SECMANAGER_REASON_API_INVALID_WATERMARK_PARAMETER = 14,
	SECMANAGER_REASON_API_INVALID_CONTENT_PARAMETER = 15,
	SECMANAGER_REASON_API_UNDEFINED_ERROR = 9999
} SecManagerResultApiCode;

typedef enum {
	SECMANAGER_REASON_DRM_GENERAL_FAILURE = 1,
	SECMANAGER_REASON_DRM_NO_PLAYBACK_SESSION = 2,
	SECMANAGER_REASON_DRM_LICENSE_TIMEOUT = 3,
	SECMANAGER_REASON_DRM_LICENSE_NETWORK_FAIL = 4,
	SECMANAGER_REASON_DRM_LICENSE_BUSY = 5,
	SECMANAGER_REASON_DRM_ACCESS_TOKEN_ERROR = 6,
	SECMANAGER_REASON_DRM_ACCESS_TOKEN_IP_DIFF = 7,
	SECMANAGER_REASON_DRM_ACCESS_TOKEN_EXPIRED = 8,
	SECMANAGER_REASON_DRM_DEVICE_TOKEN_EXPIRED = 9,
	SECMANAGER_REASON_DRM_MAC_TOKEN_MISSING = 10,
	SECMANAGER_REASON_DRM_MAC_TOKEN_NO_PROV = 11,
	SECMANAGER_REASON_DRM_MEMORY_ALLOCATION_ERROR = 12,
	SECMANAGER_REASON_DRM_SECAPI_USAGE_FAILURE = 13,
	SECMANAGER_REASON_DRM_PERMISSION_DENIED = 100,
	SECMANAGER_REASON_DRM_RULE_ERROR = 101,
	SECMANAGER_REASON_DRM_ENTITLEMENT_ERROR = 102,
	SECMANAGER_REASON_DRM_AUTHENTICATION_FAIL = 103
} SecManagerResultDRMCode;
#endif


