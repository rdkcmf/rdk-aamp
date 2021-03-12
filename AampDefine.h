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

#define MAX_PTS_ERRORS_THRESHOLD 4
#define DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS (1000)    /**< Wait time in milliseconds before retry for 5xx errors */

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

#if 0

#define AAMP_DAI_CURL_COUNT 1           /**< Download Ad manifest */
#define AAMP_DAI_CURL_IDX (AAMP_TRACK_COUNT + AAMP_DRM_CURL_COUNT)                                /**< CURL Index for DAI */
#define MAX_CURL_INSTANCE_COUNT (AAMP_TRACK_COUNT + AAMP_DRM_CURL_COUNT + AAMP_DAI_CURL_COUNT)    /**< Maximum number of CURL instances */
#define AAMP_MAX_PIPE_DATA_SIZE 1024    /**< Max size of data send across pipe */
#define DEFAULT_MINIMUM_CACHE_VOD_SECONDS  0        /**< Default cache size of VOD playback */
#define DEFAULT_MINIMUM_CACHE_VIDEO_SECONDS  0        /**< Default cache size of VIDEO playback */
#define NOW_SYSTEM_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()     /**< Getting current system clock in milliseconds */
#define NOW_STEADY_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()     /**< Getting current steady clock in milliseconds */
#define SEGMENTS_DOWNLOADED_BEFORE_PLAY  10
#define MAX_FRAGMENT_RETRY_LIMIT  10
#define DRM_FAIL_RETRY_COUNT  10
#define SEGMENT_INJECT_FAIL_RETRY_COUNT  10
#define MAX_RETRY_INIT_CURL_FRAGMENT_COUNT 1


#define DEFAULT_CURL_TIMEOUT 5L         /**< Default timeout for Curl downloads */
#define DEFAULT_AAMP_ABR_THRESHOLD_SIZE (6000)		/**< aamp abr threshold size */
#define DEFAULT_PREBUFFER_COUNT (2)
#define DEFAULT_MINIMUM_INIT_CACHE_SECONDS  0        /**< Default initial cache size of playback */
#define MINIMUM_INIT_CACHE_NOT_OVERRIDDEN  -1        /**< Initial cache size not overridden by aamp.cfg */
#define BITRATE_ALLOWED_VARIATION_BAND 500000       /**< NW BW change beyond this will be ignored */

#define DEFAULT_CACHED_FRAGMENTS_PER_TRACK  3       /**< Default cached fragements per track */
#define DEFAULT_BUFFER_HEALTH_MONITOR_DELAY 10
#define DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL 5
#define DEFAULT_DISCONTINUITY_TIMEOUT 3000          /**< Default discontinuity timeout after cache is empty in MS */

#define DEFAULT_STALL_ERROR_CODE (7600)             /**< Default stall error code: 7600 */
#define DEFAULT_STALL_DETECTION_TIMEOUT (10000)     /**< Stall detection timeout: 10sec */
#define FRAGMENT_DOWNLOAD_WARNING_THRESHOLD 2000    /**< MAX Fragment download threshold time in Msec*/

#define DEFAULT_REPORT_PROGRESS_INTERVAL (1000)     /**< Progress event reporting interval: 1sec */

#define TRICKPLAY_NETWORK_PLAYBACK_FPS 4            /**< Frames rate for trickplay from CDN server */
#define TRICKPLAY_TSB_PLAYBACK_FPS 8                /**< Frames rate for trickplay from TSB */
#define MAX_SEG_DOWNLOAD_FAIL_COUNT 10              /**< Max segment download failures to identify a playback failure. */
#define MAX_AD_SEG_DOWNLOAD_FAIL_COUNT 2            /**< Max Ad segment download failures to identify as the ad playback failure. */
#define MAX_SEG_DRM_DECRYPT_FAIL_COUNT 10           /**< Max segment decryption failures to identify a playback failure. */
#define MAX_SEG_INJECT_FAIL_COUNT 10                /**< Max segment injection failure to identify a playback failure. */
#define DEF_LICENSE_REQ_RETRY_WAIT_TIME 500			/**< Wait time in milliseconds before retrying for DRM license */
#define MAX_DIFF_BETWEEN_PTS_POS_MS (3600*1000)

#define MAX_PTS_ERRORS_THRESHOLD 4

#define MANIFEST_TEMP_DATA_LENGTH 100				/**< Manifest temp data length */
#define AAMP_LOW_BUFFER_BEFORE_RAMPDOWN 10 // 10sec buffer before rampdown
#define AAMP_HIGH_BUFFER_BEFORE_RAMPUP  15 // 15sec buffer before rampup
#define AAMP_USERAGENT_SUFFIX		"AAMP/2.0.0"    /**< Version string of AAMP Player */
#define AAMP_USERAGENT_BASE_STRING	"Mozilla/5.0 (Linux; x86_64 GNU/Linux) AppleWebKit/601.1 (KHTML, like Gecko) Version/8.0 Safari/601.1 WPE"	/**< Base User agent string,it will be appneded with AAMP_USERAGENT_SUFFIX */
#define AAMP_USER_AGENT_MAX_CONFIG_LEN  512    /**< Max Chars allowed in aamp.cfg for user-agent */
#define MIN_DASH_DRM_SESSIONS 3
#define MAX_DASH_DRM_SESSIONS 30

// HLS CDVR/VOD playlist size for 1hr -> 225K , 2hr -> 450-470K , 3hr -> 670K . Most played CDVR/Vod < 2hr
#define MAX_PLAYLIST_CACHE_SIZE    (3*1024*1024) // Approx 3MB -> 2 video profiles + one audio profile + one iframe profile, 500-700K MainManifest
#define DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS (1000)    /**< Wait time in milliseconds before retry for 5xx errors */

#define DEFAULT_TIMEOUT_FOR_SOURCE_SETUP (1000) /**< Default timeout value in milliseconds */

#define AAMP_TRACK_COUNT 4              /**< internal use - audio+video+sub+aux track */
#define DEFAULT_CURL_INSTANCE_COUNT (AAMP_TRACK_COUNT + 1) // One for Manifest/Playlist + Number of tracks
#define AAMP_DRM_CURL_COUNT 4           /**< audio+video+sub+aux track DRMs */
#define AAMP_LIVE_OFFSET 15             /**< Live offset in seconds */
#define AAMP_CDVR_LIVE_OFFSET 30 	/**< Live offset in seconds for CDVR hot recording */
#define CURL_FRAGMENT_DL_TIMEOUT 10L    /**< Curl timeout for fragment download */
#define DEFAULT_PLAYLIST_DL_TIMEOUT 10L /**< Curl timeout for playlist download */
#define DEFAULT_CURL_TIMEOUT 5L         /**< Default timeout for Curl downloads */
#define DEFAULT_CURL_CONNECTTIMEOUT 3L  /**< Curl socket connection timeout */
#define EAS_CURL_TIMEOUT 3L             /**< Curl timeout for EAS manifest downloads */
#define EAS_CURL_CONNECTTIMEOUT 2L      /**< Curl timeout for EAS connection */
#define DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS (6*1000)   /**< Interval between playlist refreshes */

#define AAMP_SEEK_TO_LIVE_POSITION (-1)

// MSO-specific VSS Service Zone identifier in URL
#define VSS_MARKER			"?sz="
#define VSS_MARKER_LEN			4
#define VSS_MARKER_FOG			"%3Fsz%3D" // URI-encoded ?sz=
#define VSS_VIRTUAL_STREAM_ID_KEY_STR "content:xcal:virtualStreamId"
#define VSS_VIRTUAL_STREAM_ID_PREFIX "urn:merlin:linear:stream:"
#define VSS_SERVICE_ZONE_KEY_STR "device:xcal:serviceZone"

#define MAX_ERROR_DESCRIPTION_LENGTH 128
#define MAX_ANOMALY_BUFF_SIZE   256

// Player supported play/trick-play rates.
#define AAMP_RATE_TRICKPLAY_MAX		64
#define AAMP_NORMAL_PLAY_RATE		1
#define AAMP_RATE_PAUSE			0
#define AAMP_RATE_INVALID		INT_MAX

#define STRLEN_LITERAL(STRING) (sizeof(STRING)-1)
#define STARTS_WITH_IGNORE_CASE(STRING, PREFIX) (0 == strncasecmp(STRING, PREFIX, STRLEN_LITERAL(PREFIX)))

/*1 for debugging video track, 2 for audio track, 4 for subtitle track and 7 for all*/
/*#define AAMP_DEBUG_FETCH_INJECT 0x001 */

/**
 * @brief Max debug log buffer size
 */
#define MAX_DEBUG_LOG_BUFF_SIZE 1024

/**
 * @brief Max URL log size
 */
#define MAX_URL_LOG_SIZE 960	// Considering "aamp_tune" and [AAMP-PLAYER] pretext

#define CONVERT_SEC_TO_MS(_x_) (_x_ * 1000) /**< Convert value to sec to ms*/
#define DEFAULT_PRECACHE_WINDOW (10) 	// 10 mins for full precaching
#define DEFAULT_DOWNLOAD_RETRY_COUNT (1)		// max download failure retry attempt count

// These error codes are used internally to identify the cause of error from GetFile
#define PARTIAL_FILE_CONNECTIVITY_AAMP (130)
#define PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP (131)
#define OPERATION_TIMEOUT_CONNECTIVITY_AAMP (132)
#define PARTIAL_FILE_START_STALL_TIMEOUT_AAMP (133)
#define AAMP_MINIMUM_AUDIO_LEVEL (0) /**< minimum value for audio level supported */
#define AAMP_MAXIMUM_AUDIO_LEVEL (100) /**< maximum value for audio level supported */

#define STRBGPLAYER "BACKGROUND"
#define STRFGPLAYER "FOREGROUND"

#define AAMP_MAX_EVENT_PRIORITY (-70) /** Maximum allowed priority value for events */

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
 * @brief TriState enums 
 */
enum TriState
{
	eFalseState = 0,
	eTrueState = 1,
	eUndefinedState = -1
};


/**
 * @brief Media types for telemetry
 */
enum MediaTypeTelemetry
{
	eMEDIATYPE_TELEMETRY_AVS,		/**< Type audio, video or subtitle */
	eMEDIATYPE_TELEMETRY_DRM,		/**< Type DRM license */
	eMEDIATYPE_TELEMETRY_INIT,		/**< Type audio or video init fragment */
	eMEDIATYPE_TELEMETRY_MANIFEST,		/**< Type main or sub manifest file */
	eMEDIATYPE_TELEMETRY_UNKNOWN,		/**< Type unknown*/
};

/**
 * @brief Media output format
 */
enum StreamOutputFormat
{
	FORMAT_INVALID,         /**< Invalid format */
	FORMAT_MPEGTS,          /**< MPEG Transport Stream */
	FORMAT_ISO_BMFF,        /**< ISO Base Media File format */
	FORMAT_AUDIO_ES_AAC,    /**< AAC Audio Elementary Stream */
	FORMAT_AUDIO_ES_AC3,    /**< AC3 Audio Elementary Stream */
	FORMAT_AUDIO_ES_EC3,    /**< Dolby Digital Plus Elementary Stream */
	FORMAT_AUDIO_ES_ATMOS,  /**< ATMOS Audio stream */
	FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
	FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
	FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
	FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
	FORMAT_UNKNOWN          /**< Unknown Format */
};

/**
 * @brief Video zoom mode
 */
enum VideoZoomMode
{
	VIDEO_ZOOM_FULL,    /**< Video Zoom Enabled */
	VIDEO_ZOOM_NONE     /**< Video Zoom Disabled */
};

/**
 * @addtogroup AAMP_COMMON_TYPES
 * @{
 */

/**
 * @brief Enumeration for Curl Instances
 */
enum AampCurlInstance
{
	eCURLINSTANCE_VIDEO,
	eCURLINSTANCE_AUDIO,
	eCURLINSTANCE_SUBTITLE,
	eCURLINSTANCE_AUX_AUDIO,
	eCURLINSTANCE_MANIFEST_PLAYLIST,
	eCURLINSTANCE_DAI,
	eCURLINSTANCE_AES,
	eCURLINSTANCE_PLAYLISTPRECACHE,
	eCURLINSTANCE_MAX
};

/*
 * @brief Playback Error Type
 */
enum PlaybackErrorType
{
	eGST_ERROR_PTS,                 /**< PTS error from gstreamer */
	eGST_ERROR_UNDERFLOW,           /**< Underflow error from gstreamer */
	eGST_ERROR_VIDEO_BUFFERING,     /**< Video buffering error */
	eGST_ERROR_OUTPUT_PROTECTION_ERROR,     /**< Output Protection error */
	eDASH_ERROR_STARTTIME_RESET,    /**< Start time reset of DASH */
	eSTALL_AFTER_DISCONTINUITY,		/** Playback stall after notifying discontinuity */
	eGST_ERROR_GST_PIPELINE_INTERNAL	/** GstPipeline Internal Error */
};


/**
 * @brief Tune Typea
 */
enum TuneType
{
	eTUNETYPE_NEW_NORMAL,   /**< Play from live point for live streams, from start for VOD*/
	eTUNETYPE_NEW_SEEK,     /**< A new tune with valid seek position*/
	eTUNETYPE_SEEK,         /**< Seek to a position. Not a new channel, so resources can be reused*/
	eTUNETYPE_SEEKTOLIVE,   /**< Seek to live point. Not a new channel, so resources can be reused*/
	eTUNETYPE_RETUNE,       /**< Internal retune for error handling.*/
	eTUNETYPE_LAST          /**< Use the tune mode used in last tune*/
};

/**
 * @brief AAMP Function return values
 */
enum AAMPStatusType
{
	eAAMPSTATUS_OK,
	eAAMPSTATUS_GENERIC_ERROR,
	eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR,
	eAAMPSTATUS_PLAYLIST_VIDEO_DOWNLOAD_ERROR,
	eAAMPSTATUS_PLAYLIST_AUDIO_DOWNLOAD_ERROR,
	eAAMPSTATUS_MANIFEST_PARSE_ERROR,
	eAAMPSTATUS_MANIFEST_CONTENT_ERROR,
	eAAMPSTATUS_MANIFEST_INVALID_TYPE,
	eAAMPSTATUS_PLAYLIST_PLAYBACK,
	eAAMPSTATUS_SEEK_RANGE_ERROR,
	eAAMPSTATUS_TRACKS_SYNCHRONISATION_ERROR,
	eAAMPSTATUS_UNSUPPORTED_DRM_ERROR
};


/**
 * @brief Http Header Type
 */
enum HttpHeaderType
{
	eHTTPHEADERTYPE_COOKIE,     /**< Cookie Header */
	eHTTPHEADERTYPE_XREASON,    /**< X-Reason Header */
	eHTTPHEADERTYPE_FOG_REASON, /**< FOG-X-Reason Header */
	eHTTPHEADERTYPE_EFF_LOCATION, /**< Effective URL location returned */
	eHTTPHEADERTYPE_FOG_ERROR,  /**< FOG-X-Error Header */
	eHTTPHEADERTYPE_UNKNOWN=-1  /**< Unkown Header */
};


/**
 * @brief Http Header Type
 */
enum CurlAbortReason
{
	eCURL_ABORT_REASON_NONE = 0,
	eCURL_ABORT_REASON_STALL_TIMEDOUT,
	eCURL_ABORT_REASON_START_TIMEDOUT
};

/**
 * @brief Different reasons for bitrate change
 */
typedef enum
{
	eAAMP_BITRATE_CHANGE_BY_ABR = 0,
	eAAMP_BITRATE_CHANGE_BY_RAMPDOWN = 1,
	eAAMP_BITRATE_CHANGE_BY_TUNE = 2,
	eAAMP_BITRATE_CHANGE_BY_SEEK = 3,
	eAAMP_BITRATE_CHANGE_BY_TRICKPLAY = 4,
	eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL = 5,
	eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY = 6,
	eAAMP_BITRATE_CHANGE_BY_FOG_ABR = 7,
	eAAMP_BITRATE_CHANGE_MAX = 8
} BitrateChangeReason;

/**
 * @enum AudioType
 *
 * @brief Type of audio ES for MPD
 */
enum AudioType
{
	eAUDIO_UNSUPPORTED,
	eAUDIO_UNKNOWN,
	eAUDIO_AAC,
	eAUDIO_DDPLUS,
	eAUDIO_ATMOS
};

/**
 *
 * @enum Curl Request
 *
 */
enum CurlRequest
{
	eCURL_GET,
	eCURL_POST,
	eCURL_DELETE
};

/**
 * @brief Asset's content types
 */
enum ContentType
{
	ContentType_UNKNOWN,        /**< 0 - Unknown type */
	ContentType_CDVR,           /**< 1 - CDVR */
	ContentType_VOD,            /**< 2 - VOD */
	ContentType_LINEAR,         /**< 3 - Linear */
	ContentType_IVOD,           /**< 4 - IVOD */
	ContentType_EAS,            /**< 5 - EAS */
	ContentType_CAMERA,         /**< 6 - Camera */
	ContentType_DVR,            /**< 7 - DVR */
	ContentType_MDVR,           /**< 8 - MDVR */
	ContentType_IPDVR,          /**< 8 - IPDVR */
	ContentType_PPV,            /**< 10 - PPV */
	ContentType_OTT,            /**< 11 - OTT */
	ContentType_OTA,            /**< 12 - OTA*/
	ContentType_HDMIIN,         /**< 13 - HDMI Input */
	ContentType_COMPOSITEIN,    /**< 14 - COMPOSITE Input*/
	ContentType_MAX             /**< 15 - Type Count*/
};

/**
 * @brief Media format types
 */
typedef enum
{
	eMEDIAFORMAT_HLS,
	eMEDIAFORMAT_DASH,
	eMEDIAFORMAT_PROGRESSIVE,
	eMEDIAFORMAT_HLS_MP4,
	eMEDIAFORMAT_OTA,
	eMEDIAFORMAT_HDMI,
	eMEDIAFORMAT_COMPOSITE,
	eMEDIAFORMAT_UNKNOWN
} MediaFormat;

/**
 * @brief DRM system types
 * @note these are now deprecated in favor of DrmHelpers, don't expand this
 */
enum DRMSystems
{
	eDRM_NONE,              /**< No DRM */
	eDRM_WideVine,          /**< Widevine, used to set legacy API */
	eDRM_PlayReady,         /**< Playread, used to set legacy APIy */
	eDRM_CONSEC_agnostic,   /**< CONSEC Agnostic DRM, deprecated */
	eDRM_Adobe_Access,      /**< Adobe Access, fully deprecated */
	eDRM_Vanilla_AES,       /**< Vanilla AES, fully deprecated */
	eDRM_ClearKey,          /**< Clear key, used to set legacy API */
	eDRM_MAX_DRMSystems     /**< Drm system count */
};


/**
 * @brief Media types
 */
// Please maintain the order video, audio, subtitle and aux_audio in future
// Above order to be maintained across fragment, init and playlist media types
// These enums are used in a lot of calculation in AAMP code and breaking the order will bring a lot of issues
// This order is also followed in other enums like AampCurlInstance and TrackType
enum MediaType
{
	eMEDIATYPE_VIDEO,               /**< Type video */
	eMEDIATYPE_AUDIO,               /**< Type audio */
	eMEDIATYPE_SUBTITLE,            /**< Type subtitle */
	eMEDIATYPE_AUX_AUDIO,           /**< Type auxiliary audio */
	eMEDIATYPE_MANIFEST,            /**< Type manifest */
	eMEDIATYPE_LICENCE,             /**< Type license */
	eMEDIATYPE_IFRAME,              /**< Type iframe */
	eMEDIATYPE_INIT_VIDEO,          /**< Type video init fragment */
	eMEDIATYPE_INIT_AUDIO,          /**< Type audio init fragment */
	eMEDIATYPE_INIT_SUBTITLE,       /**< Type subtitle init fragment */
	eMEDIATYPE_INIT_AUX_AUDIO,      /**< Type auxiliary audio init fragment */
	eMEDIATYPE_PLAYLIST_VIDEO,      /**< Type video playlist */
	eMEDIATYPE_PLAYLIST_AUDIO,      /**< Type audio playlist */
	eMEDIATYPE_PLAYLIST_SUBTITLE,	/**< Type subtitle playlist */
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,	/**< Type auxiliary audio playlist */
	eMEDIATYPE_PLAYLIST_IFRAME,     /**< Type Iframe playlist */
	eMEDIATYPE_INIT_IFRAME,         /**< Type IFRAME init fragment */
	eMEDIATYPE_DSM_CC,              /**< Type digital storage media command and control (DSM-CC) */
	eMEDIATYPE_IMAGE,		/**< Type image for thumbnail playlist */
	eMEDIATYPE_DEFAULT              /**< Type unknown */
};

/**
 *  @brief Auth Token Failure codes
 */
enum AuthTokenErrors {
	eAUTHTOKEN_TOKEN_PARSE_ERROR = -1,
	eAUTHTOKEN_INVALID_STATUS_CODE = -2
};

/**
 *  @brief Language Code Preference types
 */
typedef enum
{
    ISO639_NO_LANGCODE_PREFERENCE,
    ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE,
    ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE,
    ISO639_PREFER_2_CHAR_LANGCODE
} LangCodePreference;

/**
 * @brief AAMP anomaly message types
 */
typedef enum
{
	ANOMALY_ERROR,
	ANOMALY_WARNING,
	ANOMALY_TRACE
} AAMPAnomalyMessageType;

#endif

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


