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
 * @file GlobalConfigAAMP.h
 * @brief Advanced Adaptive Media Player (AAMP) GlobalConfigAAMP header
 */

#ifndef GLOBALCONFIGAAMP_H
#define GLOBALCONFIGAAMP_H

#include <string>
#include <map>

#include "AampDrmSystems.h"
#include "AampLogManager.h"

#define DEFAULT_AAMP_ABR_THRESHOLD_SIZE (6000)		/**< aamp abr threshold size */
#define DEFAULT_PREBUFFER_COUNT (2)

#define DEFAULT_INIT_BITRATE     2500000            /**< Initial bitrate: 2.5 mb - for non-4k playback */
#define DEFAULT_INIT_BITRATE_4K 13000000            /**< Initial bitrate for 4K playback: 13mb ie, 3/4 profile */
#define DEFAULT_MINIMUM_INIT_CACHE_SECONDS  0        /**< Default initial cache size of playback */
#define MINIMUM_INIT_CACHE_NOT_OVERRIDDEN  -1        /**< Initial cache size not overridden by aamp.cfg */
#define BITRATE_ALLOWED_VARIATION_BAND 500000       /**< NW BW change beyond this will be ignored */
#define DEFAULT_ABR_CACHE_LIFE 5000                 /**< Default ABR cache life */
#define DEFAULT_ABR_CACHE_LENGTH 3                  /**< Default ABR cache length */
#define DEFAULT_ABR_OUTLIER 5000000                 /**< ABR outlier: 5 MB */
#define DEFAULT_ABR_SKIP_DURATION 6                 /**< Initial skip duration of ABR - 6 sec */
#define DEFAULT_ABR_NW_CONSISTENCY_CNT 2            /**< ABR network consistency count */

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
#define MIN_DASH_DRM_SESSIONS 2
#define MAX_DASH_DRM_SESSIONS 30

// HLS CDVR/VOD playlist size for 1hr -> 225K , 2hr -> 450-470K , 3hr -> 670K . Most played CDVR/Vod < 2hr
#define MAX_PLAYLIST_CACHE_SIZE    (3*1024*1024) // Approx 3MB -> 2 video profiles + one audio profile + one iframe profile, 500-700K MainManifest
#define DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS (1000)    /**< Wait time in milliseconds before retry for 5xx errors */

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
 * @brief Class for AAMP's global configuration
 */
class GlobalConfigAAMP
{
public:
	long defaultBitrate;        /**< Default bitrate*/
	long defaultBitrate4K;      /**< Default 4K bitrate*/
	bool bEnableABR;            /**< Enable/Disable adaptive bitrate logic*/
	bool noFog;                 /**< Disable FOG*/
	char *mapMPD;               /**< Mapping of HLS to MPD url for matching string */
	bool fogSupportsDash;       /**< Enable FOG support for DASH*/
#ifdef AAMP_HARVEST_SUPPORT_ENABLED
	int harvest;                /**< Save decrypted fragments for debugging*/
	char* harvestpath;
#endif

	AampLogManager logging;             	/**< Aamp log manager class*/
	int gPreservePipeline;                  /**< Flush instead of teardown*/
	int gAampDemuxHLSAudioTsTrack;          /**< Demux Audio track from HLS transport stream*/
	int gAampDemuxHLSVideoTsTrack;          /**< Demux Video track from HLS transport stream*/
	int gAampMergeAudioTrack;               /**< Merge audio track and queued till video processed*/
	int gThrottle;                          /**< Regulate output data flow*/
	TunedEventConfig tunedEventConfigLive;  /**< When to send TUNED event for LIVE*/
	TunedEventConfig tunedEventConfigVOD;   /**< When to send TUNED event for VOD*/
	int demuxHLSVideoTsTrackTM;             /**< Demux video track from HLS transport stream track mode*/
	int demuxedAudioBeforeVideo;            /**< Send demuxed audio before video*/
	TriState playlistsParallelFetch;        /**< Enabled parallel fetching of audio & video playlists*/
	TriState parallelPlaylistRefresh ;	/**< Enabled parallel fetching for refresh of audio & video playlists*/
	TriState enableBulkTimedMetaReport;	/**< Enabled Bulk event reporting for TimedMetadata*/
	TriState useRetuneForUnpairedDiscontinuity; /**< Used for unpaired discontinuity retune logic*/
	TriState mAsyncTuneConfig;		/**< Enalbe Async tune from application */
	TriState mWesterosSinkConfig;		/**< Enalbe Westeros sink from application */
	TriState mEnableRectPropertyCfg;        /**< Allow or deny rectangle property set for sink element*/
	TriState mUseAverageBWForABR;           /** Enables usage of AverageBandwidth if available for ABR */
	int  mPreCacheTimeWindow;		/** Max time to complete PreCaching .In Minutes  */
	bool prefetchIframePlaylist;            /**< Enabled prefetching of I-Frame playlist*/
	int forceEC3;                           /**< Forcefully enable DDPlus*/
	int disableEC3;                         /**< Disable DDPlus*/
	int disableATMOS;                       /**< Disable Dolby ATMOS*/
	int liveOffset;                         /**< Current LIVE offset*/
	int cdvrliveOffset;                     /**< CDVR LIVE offset*/
	int disablePlaylistIndexEvent;          /**< Disable playlist index event*/
	int enableSubscribedTags;               /**< Enabled subscribed tags*/
	bool dashIgnoreBaseURLIfSlash;          /**< Ignore the constructed URI of DASH, if it is / */
	long networkTimeoutMs;                 	/**< Fragment download timeout in ms*/
	long manifestTimeoutMs;                 /**< Manifest download timeout in ms*/
	long playlistTimeoutMs;                 /**< Playlist download timeout in ms*/
	bool licenseAnonymousRequest;           /**< Acquire license without token*/
	bool useLinearSimulator;				/**< Simulate linear stream from VOD asset*/
	int minInitialCacheSeconds;             /**< Minimum cached duration before playing in seconds*/
	int abrCacheLife;                       /**< Adaptive bitrate cache life in seconds*/
	int abrCacheLength;                     /**< Adaptive bitrate cache length*/
	int maxCachedFragmentsPerTrack;         /**< fragment cache length*/
	int abrOutlierDiffBytes;                /**< Adaptive bitrate outlier, if values goes beyond this*/
	int abrNwConsistency;                   /**< Adaptive bitrate network consistency*/
	int minABRBufferForRampDown;		/**< Mininum ABR Buffer for Rampdown*/
	int maxABRBufferForRampUp;		/**< Maximum ABR Buffer for Rampup*/
	TriState abrBufferCheckEnabled;         /**< Flag to enable/disable buffer based ABR handling*/
	TriState useNewDiscontinuity;         /**< Flag to enable/disable buffer based ABR handling*/
	int bufferHealthMonitorDelay;           /**< Buffer health monitor start delay after tune/ seek*/
	int bufferHealthMonitorInterval;        /**< Buffer health monitor interval*/
	bool hlsAVTrackSyncUsingStartTime;      /**< HLS A/V track to be synced with start time*/
	char* licenseServerURL;                 /**< License server URL*/
	bool licenseServerLocalOverride;        /**< Enable license server local overriding*/
	int vodTrickplayFPS;                    /**< Trickplay frames per second for VOD*/
	bool vodTrickplayFPSLocalOverride;      /**< Enabled VOD Trickplay FPS local overriding*/
	int linearTrickplayFPS;                 /**< Trickplay frames per second for LIVE*/
	bool linearTrickplayFPSLocalOverride;   /**< Enabled LIVE Trickplay FPS local overriding*/
	int stallErrorCode;                     /**< Stall error code*/
	int stallTimeoutInMS;                   /**< Stall timeout in milliseconds*/
	char* httpProxy;                  /**< HTTP proxy address*/
	int reportProgressInterval;             /**< Interval of progress reporting*/
	DRMSystems preferredDrm;                /**< Preferred DRM*/
	bool  isUsingLocalConfigForPreferredDRM;          /**< Preferred DRM configured as part of aamp.cfg */
	bool mpdDiscontinuityHandling;          /**< Enable MPD discontinuity handling*/
	bool mpdDiscontinuityHandlingCdvr;      /**< Enable MPD discontinuity handling for CDVR*/
	bool bForceHttp;                        /**< Force HTTP*/
	int abrSkipDuration;                    /**< Initial duration for ABR skip*/
	bool internalReTune;                    /**< Internal re-tune on underflows/ pts errors*/
	int ptsErrorThreshold;                       /**< Max number of back-to-back PTS errors within designated time*/
	bool bAudioOnlyPlayback;                /**< AAMP Audio Only Playback*/
	bool gstreamerBufferingBeforePlay;      /**< Enable pre buffering logic which ensures minimum buffering is done before pipeline play*/
	int licenseRetryWaitTime;
	long iframeBitrate;                     /**< Default bitrate for iframe track selection for non-4K assets*/
	long iframeBitrate4K;                   /**< Default bitrate for iframe track selection for 4K assets*/
	char *ckLicenseServerURL;				/**< ClearKey License server URL*/
	bool enableMicroEvents;                 /**< Enabling the tunetime micro events*/
	long curlStallTimeout;                  /**< Timeout value for detection curl download stall in seconds*/
	long curlDownloadStartTimeout;          /**< Timeout value for curl download to start after connect in seconds*/
	bool enablePROutputProtection;          /**< Playready output protection config */
	char *pUserAgentString;			/**< Curl user-agent string */
	bool reTuneOnBufferingTimeout;          /**< Re-tune on buffering timeout */
	int gMaxPlaylistCacheSize;              /**< Max Playlist Cache Size  */
	int waitTimeBeforeRetryHttp5xxMS;		/**< Wait time in milliseconds before retry for 5xx errors*/
	bool disableSslVerifyPeer;		/**< Disable curl ssl certificate verification. */
	std::string mSubtitleLanguage;          /**< User preferred subtitle language*/
	bool enableClientDai;                   /**< Enabling the client side DAI*/
	bool playAdFromCDN;                     /**< Play Ad from CDN. Not from FOG.*/
	bool mEnableVideoEndEvent;              /**< Enable or disable videovend events */
	int dash_MaxDRMSessions;				/** < Max drm sessions that can be cached by AampDRMSessionManager*/
	long discontinuityTimeout;              /**< Timeout value to auto process pending discontinuity after detecting cache is empty*/
	bool bReportVideoPTS;                    /**< Enables Video PTS reporting */
	bool decoderUnavailableStrict;           /**< Reports decoder unavailable GST Warning as aamp error*/
	bool useAppSrcForProgressivePlayback;    /**< Enables appsrc for playing progressive AV type */
	int aampAbrThresholdSize;		/**< AAMP ABR threshold size*/
	int langCodePreference; /**<prefered format for normalizing language code */
        bool bDescriptiveAudioTrack;            /**< advertise audio tracks using <langcode>-<role> instead of just <langcode> */
	bool reportBufferEvent;			/** Enables Buffer event reporting */
	bool fragmp4LicensePrefetch;   /*** Enable fragment mp4 license prefetching**/
	bool bPositionQueryEnabled;		/** Enables GStreamer position query for progress reporting */
	int aampRemovePersistent;               /**< Flag to enable/disable code in ave drm to avoid crash when majorerror 3321, 3328 occurs*/
	int preplaybuffercount;         /** Count of segments to be downloaded until play state */
	#define GetLangCodePreference() ((LangCodePreference)gpGlobalConfig->langCodePreference)
	int rampdownLimit;		/*** Fragment rampdown/retry limit */
	int mInitRampdownLimit; /** Maximum number of limits for ramdown and try download if playlist download failed at start time before reporting failure **/
	long minBitrate;		/*** Minimum bandwidth of playback profile */
	long maxBitrate;		/*** Maximum bandwidth of playback profile */
	int segInjectFailCount;		/*** Inject failure retry threshold */
	int drmDecryptFailCount;	/*** DRM decryption failure retry threshold */
	char *uriParameter;	/*** uri parameter data to be appended on download-url during curl request */
	std::vector<std::string> customHeaderStr; /*** custom header data to be appended to curl request */
	int initFragmentRetryCount; /**< max attempts for int frag curl timeout failures */
	TriState useMatchingBaseUrl;
	bool bEnableSubtec; 		/**< Enable subtec-based subtitles */
	std::map<std::string, std::string> unknownValues;       /***  Anything we don't know about **/
	bool nativeCCRendering;  /*** If native CC rendering to be supported */
	TriState preferredCEA708; /*** To force 608/708 track selection in CC manager */
public:

	/**
	 * @brief Find the value of key that start with <key>
	 * @param key the value to look for
	 * @return value of key
	 */
	std::string getUnknownValue(const std::string& key);

	/**
	 * @brief Find the value of key that start with <key>
	 * @param key the value to look for
	 * @param defaultValue value to be returned in case key not found
	 * @return value of key
	 */
	const std::string& getUnknownValue(const std::string& key, const std::string& defaultValue);

	/**
	 * @brief Find the value of key that start with <key>
	 * @param key the value to look for
	 * @param defaultValue value to be returned in case key not found
	 * @return value of key
	 */
	int getUnknownValue(const std::string& key, int defaultValue);

	/**
	 * @brief Find the value of key that start with <key>
	 * @param key the value to look for
	 * @param defaultValue value to be returned in case key not found
	 * @return value of key
	 */
	bool getUnknownValue(const std::string& key, bool defaultValue);

	/**
	 * @brief Find the value of key that start with <key>
	 * @param key the value to look for
	 * @param defaultValue value to be returned in case key not found
	 * @return value of key
	 */
	double getUnknownValue(const std::string& key, double defaultValue);

	/**
	 * Finds all the unknown keys that start with <key>
	 * @param key the value to look for
	 * @param values [out] all the found keys that start with <key>
	 * @return the count of values found
	 */
	int getMatchingUnknownKeys(const std::string& key, std::vector<std::string>& values);

	/**
	 * @brief GlobalConfigAAMP Constructor
	 */
	GlobalConfigAAMP();

	/**
	 * @brief GlobalConfigAAMP Destructor
	 */
	~GlobalConfigAAMP();

	GlobalConfigAAMP(const GlobalConfigAAMP&) = delete;

	GlobalConfigAAMP& operator=(const GlobalConfigAAMP&) = delete;

	/**
	 * @brief Sets user agent string
	 *
	 * @return none
	 */
	void setBaseUserAgentString(const char * newUserAgent);
};

extern GlobalConfigAAMP *gpGlobalConfig;

#endif /* GLOBALCONFIGAAMP_H */

