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
#include <limits.h>
#ifdef WIN32
#define AAMP_CFG_PATH "c:/tmp/aamp.cfg"
#define AAMP_JSON_PATH "c:/tmp/aamp.json"
#else

#ifdef UNIT_TEST_ENABLED
#define AAMP_CFG_PATH "aamp.cfg"
#define AAMP_JSON_PATH "aampcfg.json"
#else
#define AAMP_CFG_PATH "/opt/aamp.cfg"
#define AAMP_JSON_PATH "/opt/aampcfg.json"
#endif
#endif


//Stringification of Macro :  use two levels of macros
#define MACRO_TO_STRING(s) X_STR(s)
#define X_STR(s) #s

#define MAX_PTS_ERRORS_THRESHOLD 4
#define DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS (1000)    /**< Wait time in milliseconds before retry for 5xx errors */
#define MAX_PLAYLIST_CACHE_SIZE    (3*1024) // Approx 3MB -> 2 video profiles + one audio profile + one iframe profile, 500-700K MainManifest

#define DEFAULT_ABR_CACHE_LIFE 5000                 /**< Default ABR cache life  in milli secs*/
#define DEFAULT_ABR_OUTLIER 5000000                 /**< ABR outlier: 5 MB */
#define DEFAULT_ABR_SKIP_DURATION 6                 /**< Initial skip duration of ABR - 6 sec */
#define DEFAULT_ABR_NW_CONSISTENCY_CNT 2            /**< ABR network consistency count */
#define DEFAULT_BUFFER_HEALTH_MONITOR_DELAY 10
#define DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL 5
#define DEFAULT_ABR_CACHE_LENGTH 3                  /**< Default ABR cache length */
#define DEFAULT_REPORT_PROGRESS_INTERVAL 1000     /**< Progress event reporting interval: 1msec */
#define DEFAULT_LICENSE_REQ_RETRY_WAIT_TIME 500			/**< Wait time in milliseconds before retrying for DRM license */
#define DEFAULT_INIT_BITRATE     2500000            /**< Initial bitrate: 2.5 mb - for non-4k playback */
#define DEFAULT_INIT_BITRATE_4K 13000000            /**< Initial bitrate for 4K playback: 13mb ie, 3/4 profile */
#define AAMP_LIVE_OFFSET 15             /**< Live offset in seconds */
#define AAMP_CDVR_LIVE_OFFSET 30        /**< Live offset in seconds for CDVR hot recording */
#define MIN_DASH_DRM_SESSIONS 3
#define DEFAULT_CACHED_FRAGMENTS_PER_TRACK  4       /**< Default cached fragements per track */
#define TRICKPLAY_VOD_PLAYBACK_FPS 4            /**< Frames rate for trickplay from CDN server */
#define TRICKPLAY_LINEAR_PLAYBACK_FPS 8                /**< Frames rate for trickplay from TSB */
#define DEFAULT_DOWNLOAD_RETRY_COUNT (1)		// max download failure retry attempt count
#define DEFAULT_DISCONTINUITY_TIMEOUT 3000          /**< Default discontinuity timeout after cache is empty in MS */
#define CURL_FRAGMENT_DL_TIMEOUT 10L    /**< Curl timeout for fragment download */
#define DEFAULT_STALL_ERROR_CODE (7600)             /**< Default stall error code: 7600 */
#define DEFAULT_STALL_DETECTION_TIMEOUT (10000)     /**< Stall detection timeout: 10000 millisec */
#define DEFAULT_MINIMUM_INIT_CACHE_SECONDS  0        /**< Default initial cache size of playback */
#define DEFAULT_TIMEOUT_FOR_SOURCE_SETUP (1000) /**< Default timeout value in milliseconds */
#define MAX_SEG_DRM_DECRYPT_FAIL_COUNT 10           /**< Max segment decryption failures to identify a playback failure. */
#define MAX_SEG_INJECT_FAIL_COUNT 10                /**< Max segment injection failure to identify a playback failure. */
#define AAMP_USERAGENT_BASE_STRING	"Mozilla/5.0 (Linux; x86_64 GNU/Linux) AppleWebKit/601.1 (KHTML, like Gecko) Version/8.0 Safari/601.1 WPE"	/**< Base User agent string,it will be appneded with AAMP_USERAGENT_SUFFIX */
#define AAMP_USERAGENT_SUFFIX		"AAMP/2.0.0"    /**< Version string of AAMP Player */
#define DEFAULT_AAMP_ABR_THRESHOLD_SIZE (6000)		/**< aamp abr threshold size */
#define DEFAULT_PREBUFFER_COUNT (2)
#define AAMP_LOW_BUFFER_BEFORE_RAMPDOWN 10 // 10sec buffer before rampdown
#define AAMP_HIGH_BUFFER_BEFORE_RAMPUP  15 // 15sec buffer before rampup
#define MAX_DASH_DRM_SESSIONS 30
#define MAX_AD_SEG_DOWNLOAD_FAIL_COUNT 2            /**< Max Ad segment download failures to identify as the ad playback failure. */
#define FRAGMENT_DOWNLOAD_WARNING_THRESHOLD 2000    /**< MAX Fragment download threshold time in Msec*/
#define BITRATE_ALLOWED_VARIATION_BAND 500000       /**< NW BW change beyond this will be ignored */
#define MAX_DIFF_BETWEEN_PTS_POS_MS (3600*1000)
#define MAX_SEG_DOWNLOAD_FAIL_COUNT 10              /**< Max segment download failures to identify a playback failure. */
#define MAX_DOWNLOAD_DELAY_LIMIT_MS 30000
#define MAX_ERROR_DESCRIPTION_LENGTH 128
#define MAX_ANOMALY_BUFF_SIZE   256

// Player supported play/trick-play rates.
#define AAMP_RATE_TRICKPLAY_MAX		64
#define AAMP_NORMAL_PLAY_RATE		1
#define AAMP_RATE_PAUSE			0
#define AAMP_RATE_INVALID		INT_MAX

#define STRLEN_LITERAL(STRING) (sizeof(STRING)-1)
#define STARTS_WITH_IGNORE_CASE(STRING, PREFIX) (0 == strncasecmp(STRING, PREFIX, STRLEN_LITERAL(PREFIX)))

/**
 * @brief Enumeration for TUNED Event Configuration
 */
enum TunedEventConfig
{
        eTUNED_EVENT_ON_PLAYLIST_INDEXED,           /**< Send TUNED event after playlist indexed*/
        eTUNED_EVENT_ON_FIRST_FRAGMENT_DECRYPTED,    /**< Send TUNED event after first fragment decryption*/
        eTUNED_EVENT_ON_GST_PLAYING,                /**< Send TUNED event on gstreamer's playing event*/
        eTUNED_EVENT_MAX
};

/**
 * @brief Enumeration for Paused state behavior
 */
enum PausedBehavior
{
	ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE,            /**< automatically begin playback from eldest portion of live window*/
	ePAUSED_BEHAVIOR_LIVE_IMMEDIATE,                        /**< automatically jump to live*/
	ePAUSED_BEHAVIOR_AUTOPLAY_DEFER,                        /**< video remains paused indefinitely till play() call, resume playback from new start portion of live window*/
	ePAUSED_BEHAVIOR_LIVE_DEFER,                            /**<  video remains paused indefinitely till play() call, resume playback from live position*/
	ePAUSED_BEHAVIOR_MAX
};

/**
 * @brief AAMP Config Ownership values
 */

typedef enum
{
	AAMP_DEFAULT_SETTING            = 0,            // Lowest priority
	AAMP_OPERATOR_SETTING           = 1,
	AAMP_STREAM_SETTING             = 2,
	AAMP_APPLICATION_SETTING        = 3,
	AAMP_TUNE_SETTING        		= 4,
	AAMP_DEV_CFG_SETTING            = 5,            // Highest priority
	AAMP_MAX_SETTING
}ConfigPriority;

#endif


