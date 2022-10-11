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
 * @file AampConfig.h
 * @brief Configurations for AAMP
 */
 
#ifndef __AAMP_CONFIG_H__
#define __AAMP_CONFIG_H__

#include <iostream>
#include <map>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <math.h>
#include <algorithm>
#include <ctype.h>
#include <sstream>
#include <curl/curl.h>
#include "AampDefine.h"
#include "AampLogManager.h"
#include <cjson/cJSON.h>
#include "AampDrmSystems.h"
#ifdef IARM_MGR
#include "host.hpp"
#include "manager.hpp"
#include "libIBus.h"
#include "libIBusDaemon.h"

#include <hostIf_tr69ReqHandler.h>
#include <sstream>
#endif


//////////////// CAUTION !!!! STOP !!! Read this before you proceed !!!!!!! /////////////
/// 1. This Class handles Configuration Parameters of AAMP Player , only Config related functionality to be added
/// 2. Simple Steps to add a new configuration
///		a) Identify new configuration takes what value ( bool / int / long / string )
/// 	b) Add the new configuration string in README.txt with appropriate comment
///		c) Add a enum value for new config in AAMPConfigSettings. It should be inserted
///			at right place based on data type
/// 	d) Add the config string added in README and its equivalent enum value at the
///			end of ConfigLookUpTable
/// 	e) Go to AampConfig constructor and assign default value . Again the array to
///			store is based on the datatype of config
/// 	f) Thats it !! You added a new configuration . Use Set and Get function to
///			store and read value using enum config
/// 	g) IF any conversion required only (from config to usage, ex: sec to millisec ),
///			add specific Get function for each config
///			Not recommened . Better to have the conversion ( enum to string , sec to millisec etc ) where its consumed .
///////////////////////////////// Happy Configuration ////////////////////////////////////


#define ISCONFIGSET(x) (aamp->mConfig->IsConfigSet(x))
#define ISCONFIGSET_PRIV(x) (mConfig->IsConfigSet(x))
#define SETCONFIGVALUE(owner,key,value) (aamp->mConfig->SetConfigValue(owner, key ,value))
#define SETCONFIGVALUE_PRIV(owner,key,value) (mConfig->SetConfigValue(owner, key ,value))
#define GETCONFIGVALUE(key,value) (aamp->mConfig->GetConfigValue( key ,value))
#define GETCONFIGVALUE_PRIV(key,value) (mConfig->GetConfigValue( key ,value))
#define GETCONFIGOWNER(key) (aamp->mConfig->GetConfigOwner(key))
#define GETCONFIGOWNER_PRIV(key,value) (mConfig->GetConfigOwner(key))
/**
 * @brief AAMP Config Settings
 */
typedef enum
{
	eAAMPConfig_EnableABR = 0,						/**< Enable/Disable adaptive bitrate logic*/
	eAAMPConfig_Fog, 							/**< Enable / Disable FOG*/
	eAAMPConfig_PrefetchIFramePlaylistDL,					/**< Enabled prefetching of I-Frame playlist*/
	eAAMPConfig_PreservePipeline,						/**< Flush instead of teardown*/
	eAAMPConfig_DemuxAudioHLSTrack ,					/**< Demux Audio track from HLS transport stream*/
	eAAMPConfig_DemuxVideoHLSTrack ,					/**< Demux Video track from HLS transport stream*/
	eAAMPConfig_Throttle,							/**< Regulate output data flow*/
	eAAMPConfig_DemuxAudioBeforeVideo,					/**< Demux video track from HLS transport stream track mode*/
	eAAMPConfig_DemuxHLSVideoTsTrackTM, 					/**< Send demuxed audio before video*/
	eAAMPConfig_ForceEC3,							/**< Forcefully enable DDPlus*/
	eAAMPConfig_DisableEC3, 						/**< Disable DDPlus*/
	eAAMPConfig_DisableATMOS,						/**< Disable Dolby ATMOS*/
	eAAMPConfig_DisableAC4,							/**< Disable AC4 Audio */
	eAAMPConfig_StereoOnly,							/**< Enable Stereo Only playback, disables EC3/ATMOS. Overrides ForceEC3 */
	eAAMPConfig_DescriptiveTrackName,					/**< Enable Descriptive track name*/
	eAAMPConfig_DisableAC3,							/**< Disable AC3 Audio */
	eAAMPConfig_DisablePlaylistIndexEvent,					/**< Disable playlist index event*/
	eAAMPConfig_EnableSubscribedTags,					/**< Enabled subscribed tags*/
	eAAMPConfig_DASHIgnoreBaseURLIfSlash,					/**< Ignore the constructed URI of DASH, if it is / */
	eAAMPConfig_AnonymousLicenseRequest,					/**< Acquire license without token*/
	eAAMPConfig_HLSAVTrackSyncUsingStartTime,				/**< HLS A/V track to be synced with start time*/
	eAAMPConfig_MPDDiscontinuityHandling,					/**< Enable MPD discontinuity handling*/
	eAAMPConfig_MPDDiscontinuityHandlingCdvr,				/**< Enable MPD discontinuity handling for CDVR*/
	eAAMPConfig_ForceHttp,							/**< Force HTTP*/
	eAAMPConfig_InternalReTune, 						/**< Internal re-tune on underflows/ pts errors*/
	eAAMPConfig_AudioOnlyPlayback,						/**< AAMP Audio Only Playback*/
	eAAMPConfig_GStreamerBufferingBeforePlay,				/**< Enable pre buffering logic which ensures minimum buffering is done before pipeline play*/
	eAAMPConfig_EnablePROutputProtection,					/**< Playready output protection config */
	eAAMPConfig_ReTuneOnBufferingTimeout,					/**< Re-tune on buffering timeout */
	eAAMPConfig_SslVerifyPeer,						/**< Enable curl ssl certificate verification. */
	eAAMPConfig_EnableClientDai,						/**< Enabling the client side DAI*/
	eAAMPConfig_PlayAdFromCDN,						/**< Play Ad from CDN. Not from FOG.*/
	eAAMPConfig_EnableVideoEndEvent,					/**< Enable or disable videovend events */
	eAAMPConfig_EnableRectPropertyCfg,					/**< To allow or deny rectangle property set for sink element*/
	eAAMPConfig_ReportVideoPTS, 						/**< Enables Video PTS reporting */
	eAAMPConfig_DecoderUnavailableStrict,					/**< Reports decoder unavailable GST Warning as aamp error*/
	eAAMPConfig_UseAppSrcForProgressivePlayback,				/**< Enables appsrc for playing progressive AV type */
	eAAMPConfig_DescriptiveAudioTrack,					/**< advertise audio tracks using <langcode>-<role> instead of just <langcode> */
	eAAMPConfig_ReportBufferEvent,						/**< Enables Buffer event reporting */
	eAAMPConfig_InfoLogging,						/**< Enables Info logging */
	eAAMPConfig_DebugLogging,						/**< Enables Debug logging */
	eAAMPConfig_TraceLogging,						/**< Enables Trace logging */
	eAAMPConfig_FailoverLogging,						/**< Enables failover logging */
	eAAMPConfig_GSTLogging,							/**< Enables Gstreamer logging */
	eAAMPConfig_ProgressLogging,						/**< Enables Progress logging */
	eAAMPConfig_CurlLogging,						/**< Enables Curl logging */
	eAAMPConfig_CurlLicenseLogging, 					/**< Enable verbose curl logging for license request (non-secclient) */
	eAAMPConfig_MetadataLogging,						/**< Enable timed metadata logging */
	eAAMPConfig_CurlHeader, 						/**< enable curl header response logging on curl errors*/
	eAAMPConfig_StreamLogging,						/**< Enables HLS Playlist content logging */
	eAAMPConfig_ID3Logging,        						/**< Enables ID3 logging */
	//eAAMPConfig_XREEventReporting,					/**< Enable/Disable Event reporting to XRE */
	eAAMPConfig_EnableGstPositionQuery, 					/**< GStreamer position query will be used for progress report events, Enabled by default for non-Intel platforms*/
	eAAMPConfig_MidFragmentSeek,                                            /**< Enable/Disable the Mid-Fragment seek functionality in aamp.*/
	eAAMPConfig_PropogateURIParam,						/**< Feature where top-level manifest URI parameters included when downloading fragments*/
	eAAMPConfig_UseWesterosSink, 						/**< Enable/Disable player to use westeros sink based video decoding */
	eAAMPConfig_EnableLinearSimulator,					/**< Enable linear simulator for testing purpose, simulate VOD asset as a "virtual linear" stream.*/
	eAAMPConfig_RetuneForUnpairDiscontinuity,				/**< disable unpaired discontinuity retun functionality*/
	eAAMPConfig_RetuneForGSTError,						/**< disable retune mitigation for gst pipeline internal data stream error*/
	eAAMPConfig_MatchBaseUrl,						/**< Enable host of main url will be matched with host of base url*/
	eAAMPConfig_WifiCurlHeader,
	eAAMPConfig_EnableSeekRange,						/**< Enable seekable range reporting via progress events */
	eAAMPConfig_DashParallelFragDownload,					/**< Enable dash fragment parallel download*/
	eAAMPConfig_PersistentBitRateOverSeek,					/**< ABR profile persistence during Seek/Trickplay/Audio switching*/
	eAAMPConfig_SetLicenseCaching,						/**< License caching*/
	eAAMPConfig_Fragmp4PrefetchLicense,					/*** Enable fragment mp4 license prefetching**/
	eAAMPConfig_ABRBufferCheckEnabled,					/**< Flag to enable/disable buffer based ABR handling*/
	eAAMPConfig_NewDiscontinuity,						/**< Flag to enable/disable new discontinuity handling with PDT*/
	eAAMPConfig_PlaylistParallelFetch,					/**< Enabled parallel fetching of audio & video playlists*/
	eAAMPConfig_PlaylistParallelRefresh,					/**< Enabled parallel fetching for refresh of audio & video playlists*/
 	eAAMPConfig_BulkTimedMetaReport, 					/**< Enabled Bulk event reporting for TimedMetadata*/
	eAAMPConfig_RemovePersistent,						/**< Flag to enable/disable code in ave drm to avoid crash when majorerror 3321, 3328 occurs*/
	eAAMPConfig_AvgBWForABR,						/**< Enables usage of AverageBandwidth if available for ABR */
	eAAMPConfig_NativeCCRendering,						/**< If native CC rendering to be supported */
	eAAMPConfig_Subtec_subtitle,						/**< Enable subtec-based subtitles */
	eAAMPConfig_WebVTTNative,						/**< Enable subtec-based subtitles */
	eAAMPConfig_AsyncTune,						 	/**< To enable Asynchronous tune */
	eAAMPConfig_DisableUnderflow,                                           /**< Enable/Disable Underflow processing*/
	eAAMPConfig_LimitResolution,                                            /**< Flag to indicate if display resolution based profile selection to be done */
	eAAMPConfig_UseAbsoluteTimeline,					/**< Enable Report Progress report position based on Availability Start Time **/
	eAAMPConfig_EnableAccessAttributes,					/**< Usage of Access Attributes in VSS */
	eAAMPConfig_WideVineKIDWorkaround,                         		/**< SkyDE Store workaround to pick WV DRM Key Id from different location */
	eAAMPConfig_RepairIframes,						/**< Enable fragment repair (Stripping and box size correction) for iframe tracks */
	eAAMPConfig_SEITimeCode,						/**< Enables SEI Time Code handling */
	eAAMPConfig_Disable4K,							/**< Enalbe/Disable 4K stream support*/
	eAAMPConfig_EnableSharedSSLSession,                                     /**< Enable/Disable config for shared ssl session reuse */
	eAAMPConfig_InterruptHandling,						/**< Enables Config for network interrupt handling*/
	eAAMPConfig_EnableLowLatencyDash,                           		/**< Enables Low Latency Dash */
	eAAMPConfig_DisableLowLatencyMonitor,                   		/**< Enables Low Latency Monitor Thread */
	eAAMPConfig_DisableLowLatencyABR,					/**< Enables Low Latency ABR handling */
	eAAMPConfig_DisableLowLatencyCorrection,                    		/**< Enables Low Latency Correction handling */
	eAAMPConfig_EnableLowLatencyOffsetMin,                                  /**< Enables Low Latency Offset Min handling */
	eAAMPConfig_SyncAudioFragments,						/**< Flag to enable Audio Video Fragment Sync */
	eAAMPConfig_EnableIgnoreEosSmallFragment,                               /**< Enable/Disable Small fragment ignore based on minimum duration Threshold at period End*/
	eAAMPConfig_UseSecManager,                                              /**< Enable/Disable secmanager instead of secclient for license acquisition */
	eAAMPConfig_EnablePTO,								/**< Enable/Disable PTO Handling */
	eAAMPConfig_EnableAampConfigToFog,                                      /**< Enable/Disable player config to Fog on every tune*/
 	eAAMPConfig_XRESupportedTune,						/**< Enable/Disable XRE supported tune*/
	eAAMPConfig_GstSubtecEnabled,								/**< Force Gstreamer subtec */
	eAAMPConfig_AllowPageHeaders,						/**< Allow page http headers*/
	eAAMPConfig_SuppressDecode,						/**< To Suppress Decode of segments for playback . Test only Downloader */
	eAAMPConfig_PersistHighNetworkBandwidth,				/** Flag to enable Persist High Network Bandwidth across Tunes */
	eAAMPConfig_PersistLowNetworkBandwidth,					/** Flag to enable Persist Low Network Bandwidth across Tunes */
	eAAMPConfig_ChangeTrackWithoutRetune,					/**< Flag to enable audio track change without disturbing video pipeline */
	eAAMPConfig_EnableCurlStore,						/**< Enable/Disable CurlStore to save/reuse curl fds */
	eAAMPConfig_RuntimeDRMConfig,                                           /**< Enable/Disable Dynamic DRM config feature */
	eAAMPConfig_EnablePublishingMuxedAudio,				/**< Enable/Disable publishing the audio track info from muxed contents */
	eAAMPConfig_EnableCMCD,							/**< Enable/Disable CMCD config feature */
	eAAMPConfig_BoolMaxValue,
	/////////////////////////////////
	eAAMPConfig_IntStartValue,
	eAAMPConfig_HarvestCountLimit,						/**< Number of files to be harvested */
	eAAMPConfig_HarvestConfig,						/**< Indicate type of file to be  harvest */
	eAAMPConfig_ABRCacheLife,						/**< Adaptive bitrate cache life in seconds*/
	eAAMPConfig_ABRCacheLength,						/**< Adaptive bitrate cache length*/
	eAAMPConfig_TimeShiftBufferLength,					/**< TSB length*/
	eAAMPConfig_ABRCacheOutlier,						/**< Adaptive bitrate outlier, if values goes beyond this*/
	eAAMPConfig_ABRSkipDuration,						/**< Initial duration for ABR skip*/
	eAAMPConfig_ABRNWConsistency,						/**< Adaptive bitrate network consistency*/
	eAAMPConfig_ABRThresholdSize,						/**< AAMP ABR threshold size*/
	eAAMPConfig_MaxFragmentCached,						/**< fragment cache length*/
	eAAMPConfig_BufferHealthMonitorDelay,					/**< Buffer health monitor start delay after tune/ seek*/
	eAAMPConfig_BufferHealthMonitorInterval,				/**< Buffer health monitor interval*/
	eAAMPConfig_PreferredDRM,						/**< Preferred DRM*/
	eAAMPConfig_TuneEventConfig,						/**< When to send TUNED event*/
	eAAMPConfig_VODTrickPlayFPS,						/**< Trickplay frames per second for VOD*/
	eAAMPConfig_LinearTrickPlayFPS,						/**< Trickplay frames per second for Linear*/
	eAAMPConfig_LicenseRetryWaitTime,					/**< License retry wait interval*/
	eAAMPConfig_PTSErrorThreshold,						/**< Max number of back-to-back PTS errors within designated time*/
	eAAMPConfig_MaxPlaylistCacheSize,					/**< Max Playlist Cache Size  */
	eAAMPConfig_MaxDASHDRMSessions,						/**< Max drm sessions that can be cached by AampDRMSessionManager*/
	eAAMPConfig_Http5XXRetryWaitInterval,					/**< Wait time in milliseconds before retry for 5xx errors*/
	eAAMPConfig_LanguageCodePreference,					/**< prefered format for normalizing language code */
	eAAMPConfig_RampDownLimit, 						/**< Set fragment rampdown/retry limit for video fragment failure*/
	eAAMPConfig_InitRampDownLimit,						/**< Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time*/
	eAAMPConfig_DRMDecryptThreshold,					/**< Retry count on drm decryption failure*/
	eAAMPConfig_SegmentInjectThreshold, 					/**< Retry count for segment injection discard/failue*/
	eAAMPConfig_InitFragmentRetryCount, 					/**< Retry attempts for init frag curl timeout failures*/
	eAAMPConfig_MinABRNWBufferRampDown, 					/**< Mininum ABR Buffer for Rampdown*/
	eAAMPConfig_MaxABRNWBufferRampUp,					/**< Maximum ABR Buffer for Rampup*/
	eAAMPConfig_PrePlayBufferCount, 					/**< Count of segments to be downloaded until play state */
	eAAMPConfig_PreCachePlaylistTime,					/**< Max time to complete PreCaching .In Minutes  */
	eAAMPConfig_CEAPreferred,						/**< To force 608/708 track selection in CC manager */
	eAAMPConfig_StallErrorCode,
	eAAMPConfig_StallTimeoutMS,
	eAAMPConfig_InitialBuffer,
	eAAMPConfig_PlaybackBuffer,
	eAAMPConfig_SourceSetupTimeout, 					/**<Timeout value wait for GStreamer appsource setup to complete*/
	eAAMPConfig_DownloadDelay,
	eAAMPConfig_LivePauseBehavior,                                          /**< player paused state behavior */
	eAAMPConfig_GstVideoBufBytes,                                           /**< Gstreamer Max Video buffering bytes*/
	eAAMPConfig_GstAudioBufBytes,                                           /**< Gstreamer Max Audio buffering bytes*/
	eAAMPConfig_LatencyMonitorDelay,               				/**< Latency Monitor Delay */
	eAAMPConfig_LatencyMonitorInterval,           				/**< Latency Monitor Interval */
	eAAMPConfig_MaxFragmentChunkCached,           				/**< fragment chunk cache length*/
	eAAMPConfig_ABRChunkThresholdSize,                			/**< AAMP ABR Chunk threshold size*/
	eAAMPConfig_FragmentDownloadFailThreshold, 				/**< Retry attempts for non-init fragment curl timeout failures*/
	eAAMPConfig_MaxInitFragCachePerTrack,					/**< Max no of Init fragment cache per track */
	eAAMPConfig_FogMaxConcurrentDownloads,                                  /**< Concurrent download posted to fog from player*/
	eAAMPConfig_ContentProtectionDataUpdateTimeout,				/**< Default Timeout For ContentProtectionData Update */
	eAAMPConfig_MaxCurlSockStore,								/**< Max no of curl socket to be stored */
	eAAMPConfig_IntMaxValue,
	///////////////////////////////////
	eAAMPConfig_LongStartValue,
	eAAMPConfig_DefaultBitrate,						/**< Default bitrate*/
	eAAMPConfig_DefaultBitrate4K,						/**< Default 4K bitrate*/
	eAAMPConfig_IFrameDefaultBitrate,					/**< Default bitrate for iframe track selection for non-4K assets*/
	eAAMPConfig_IFrameDefaultBitrate4K,					/**< Default bitrate for iframe track selection for 4K assets*/
	eAAMPConfig_CurlStallTimeout,						/**< Timeout value for detection curl download stall in seconds*/
	eAAMPConfig_CurlDownloadStartTimeout,					/**< Timeout value for curl download to start after connect in seconds*/
	eAAMPConfig_CurlDownloadLowBWTimeout,					/**< Timeout value for curl download expiry if player cann't catchup the selected bitrate buffer*/
	eAAMPConfig_DiscontinuityTimeout,					/**< Timeout value to auto process pending discontinuity after detecting cache is empty*/
	eAAMPConfig_MinBitrate, 						/**< minimum bitrate filter for playback profiles */
	eAAMPConfig_MaxBitrate, 						/**< maximum bitrate filter for playback profiles*/
	eAAMPConfig_TLSVersion,

	eAAMPConfig_LongMaxValue,
	////////////////////////////////////
	eAAMPConfig_DoubleStartValue,
	eAAMPConfig_NetworkTimeout,						/**< Fragment download timeout in sec*/
	eAAMPConfig_ManifestTimeout,						/**< Manifest download timeout in sec*/
	eAAMPConfig_PlaylistTimeout,						/**< playlist download time out in sec*/
	eAAMPConfig_ReportProgressInterval,					/**< Interval of progress reporting*/
	eAAMPConfig_PlaybackOffset,						/**< playback offset value in seconds*/
	eAAMPConfig_LiveOffset, 						/**< Current LIVE offset*/
	eAAMPConfig_CDVRLiveOffset, 						/**< CDVR LIVE offset*/
	eAAMPConfig_DoubleMaxValue,
	////////////////////////////////////
	eAAMPConfig_StringStartValue,
	eAAMPConfig_MapMPD, 							/**< host name in url for which hls to mpd mapping done'*/
	eAAMPConfig_MapM3U8,							/**< host name in url for which mpd to hls mapping done'*/
	eAAMPConfig_HarvestPath,						/**< Path to store Harvested files */
	eAAMPConfig_LicenseServerUrl,						/**< License server URL ( if no individual configuration */
	eAAMPConfig_CKLicenseServerUrl,						/**< ClearKey License server URL*/
	eAAMPConfig_PRLicenseServerUrl,						/**< PlayReady License server URL*/
	eAAMPConfig_WVLicenseServerUrl,						/**< Widevine License server URL*/
	eAAMPConfig_UserAgent,							/**< Curl user-agent string */
	eAAMPConfig_SubTitleLanguage,						/**< User preferred subtitle language*/
	//eAAMPConfig_RedirectUrl,						/**< redirects requests to tune to url1 to url2 */
	eAAMPConfig_CustomHeader,						/**< custom header string data to be appended to curl request*/
	eAAMPConfig_URIParameter,						/**< uri parameter data to be appended on download-url during curl request*/
	eAAMPConfig_NetworkProxy,						/**< Network Proxy */
	eAAMPConfig_LicenseProxy,						/**< License Proxy */
	eAAMPConfig_AuthToken,							/**< Session Token  */
	eAAMPConfig_LogLevel,							/**< New Configuration to overide info/debug/trace */
	eAAMPConfig_CustomHeaderLicense,                       			/**< custom header string data to be appended to curl License request*/
	eAAMPConfig_PreferredAudioRendition,					/**< New Configuration to save preferred Audio rendition/role descriptor field; support only single string value*/
	eAAMPConfig_PreferredAudioCodec,					/**< New Configuration to save preferred Audio codecs values; support comma separated multiple string values*/
	eAAMPConfig_PreferredAudioLanguage,					/**< New Configuration to save preferred Audio languages; support comma separated multiple string values*/
	eAAMPConfig_PreferredAudioLabel,					/**< New Configuration to save preferred Audio label field; Label is a textual description of the content. Support only single string value*/ 
	eAAMPConfig_PreferredTextRendition,					/**< New Configuration to save preferred Text rendition/role descriptor field; support only single string value*/
	eAAMPConfig_PreferredTextLanguage,					/**<  New Configuration to save preferred Text languages; support comma separated multiple string values*/
	eAAMPConfig_PreferredTextLabel,						/**< New Configuration to save preferred Text label field; Label is a textual description of the content. Support only single string value*/
	eAAMPConfig_CustomLicenseData,                          		/**< Custom Data for License Request */
	eAAMPConfig_StringMaxValue,
	eAAMPConfig_MaxValue
}AAMPConfigSettings;

/**
 * @struct ConfigChannelInfo
 * @brief Holds information of a channel
 */
struct ConfigChannelInfo
{
	ConfigChannelInfo() : name(), uri(), licenseUri()
	{
	}
	std::string name;
	std::string uri;
	std::string licenseUri;
};

/**
 * @struct customJson
 * @brief Holds information of a custom JSON array
 */

struct customJson
{
        customJson() : config(), configValue()
        { }
        std::string config;
        std::string configValue;
};


/**
 * @struct AampConfigLookupEntry
 * @brief AAMP Config lookup table structure
 */
struct AampConfigLookupEntry
{
	const char* cmdString;
	AAMPConfigSettings cfgEntryValue;
	union
	{
		int iMinValue;
		long lMinValue;
		double dMinValue;
	}Min;
	union
	{
		int iMaxValue;
		long lMaxValue;
		double dMaxValue;
	}Max;

};

/**
 * @struct AampOwnerLookupEntry
 * @brief AAMP Config ownership enum string mapping table
 */
struct AampOwnerLookupEntry
{
	const char* ownerName;
	ConfigPriority ownerValue;
};


/**
 * @struct ConfigBool
 * @brief AAMP Config Boolean data type
 */
typedef struct ConfigBool
{
	ConfigPriority owner;
	bool value;
	ConfigPriority lastowner;
	bool lastvalue;
	ConfigBool():owner(AAMP_DEFAULT_SETTING),value(false),lastowner(AAMP_DEFAULT_SETTING),lastvalue(false){}
}ConfigBool;

/**
 * @brief AAMP Config Int data type
 */
typedef struct ConfigInt
{
	ConfigPriority owner;
	int value;
	ConfigPriority lastowner;
	int lastvalue;
	ConfigInt():owner(AAMP_DEFAULT_SETTING),value(0),lastowner(AAMP_DEFAULT_SETTING),lastvalue(0){}
}ConfigInt;

/**
 * @brief AAMP Config Long data type
 */
typedef struct ConfigLong
{
	ConfigPriority owner;
	long value;
	ConfigPriority lastowner;
	long lastvalue;
	ConfigLong():owner(AAMP_DEFAULT_SETTING),value(0),lastowner(AAMP_DEFAULT_SETTING),lastvalue(0){}
}ConfigLong;

/**
 * @brief AAMP Config double data type
 */
typedef struct ConfigDouble
{
    ConfigPriority owner;
    double value;
    ConfigPriority lastowner;
    double lastvalue;
    ConfigDouble():owner(AAMP_DEFAULT_SETTING),value(0),lastowner(AAMP_DEFAULT_SETTING),lastvalue(0){}
}ConfigDouble;
/**
 * @brief AAMP Config String data type
 */
typedef struct ConfigString
{
	ConfigPriority owner;
	std::string value;
	ConfigPriority lastowner;
	std::string lastvalue;
	ConfigString():owner(AAMP_DEFAULT_SETTING),value(""),lastowner(AAMP_DEFAULT_SETTING),lastvalue(""){}
}ConfigString;


/**
 * @class AampConfig
 * @brief AAMP Config Class defn
 */
class AampConfig
{
public:
	// Had to define as public as globalConfig loggin var is used multiple files
	// TODO when player level logging is done, need to remove this
	AampLogManager logging;                 /**< Aamp log manager class*/
	AampLogManager *mLogObj;
public:
	/**
    	 * @fn AampConfig
    	 *
    	 * @return None
    	 */
	AampConfig();
	/**
         * @brief AampConfig Distructor function
         *
         * @return None
         */
	~AampConfig(){};
	/**
         * @brief Copy constructor disabled
         *
         */
	AampConfig(const AampConfig&) = delete;
	/**
     	 * @fn operator= 
     	 *
     	 * @return New Config instance with copied values
     	 */
	AampConfig& operator=(const AampConfig&);
	void Initialize();
	/**
	 * @fn ReadDeviceCapability
	 * @return Void
	 */
	void ReadDeviceCapability();
	/**
     	 * @fn ShowOperatorSetConfiguration
     	 * @return Void
     	 */
	void ShowOperatorSetConfiguration();
	/**
     	 * @fn ShowAppSetConfiguration
     	 * @return void
     	 */
	void ShowAppSetConfiguration();
	/**
     	 * @fn ShowStreamSetConfiguration
	 *
     	 * @return Void
     	 */
	void ShowStreamSetConfiguration();
	/**
     	 * @fn ShowDefaultAampConfiguration 
     	 *
     	 * @return Void
     	 */
	void ShowDefaultAampConfiguration();	
	/**
     	 *@fn ShowDevCfgConfiguration
	 *
     	 * @return Void
     	 */
	void ShowDevCfgConfiguration();
	/**
     	 * @fn ShowAAMPConfiguration
     	 *
     	 * @return Void
     	 */
	void ShowAAMPConfiguration();
	/**
     	 * @fn ReadAampCfgTxtFile
	 *
     	 * @return Void
     	 */
	bool ReadAampCfgTxtFile();
	/**
     	 * @fn ReadAampCfgJsonFile
    	 */
	bool ReadAampCfgJsonFile();
	/**
     	 * @fn ReadOperatorConfiguration
     	 * @return void
     	 */
	void ReadOperatorConfiguration();
	/**
         * @brief ParseAampCfgTxtString - It parses the aamp configuration 
         *
         * @return Void
         */
	void ParseAampCfgTxtString(std::string &cfg);
	/**
         * @brief ParseAampCfgJsonString - It parses the aamp configuration from json format
         *
         * @return Void
         */
	void ParseAampCfgJsonString(std::string &cfg);	
	/**
     	 * @fn ToggleConfigValue
     	 * @param[in] owner  - ownership of new set call
     	 * @param[in] cfg	- Configuration enum to set
     	 */
	void ToggleConfigValue(ConfigPriority owner, AAMPConfigSettings cfg );
	/**
     	 * @fn SetConfigValue
     	 * @param[in] owner  - ownership of new set call
     	 * @param[in] cfg	- Configuration enum to set
     	 * @param[in] value   - value to set
     	 */
	template<typename T>
	void SetConfigValue(ConfigPriority owner, AAMPConfigSettings cfg , const T &value);	
	/**
     	 * @fn IsConfigSet
     	 *
     	 * @param[in] cfg - Configuration enum
     	 * @return true / false 
     	 */
	bool IsConfigSet(AAMPConfigSettings cfg);
	/**
     	 * @fn GetConfigValue
     	 * @param[in] cfg - configuration enum
     	 * @param[out] value - configuration value
    	 */
	bool GetConfigValue(AAMPConfigSettings cfg, std::string &value);
	/**
    	 * @fn GetConfigValue
     	 * @param[in] cfg - configuration enum
     	 * @param[out] value - configuration value
     	 */
	bool GetConfigValue(AAMPConfigSettings cfg, long &value);
	/**
    	 * @fn GetConfigValue
     	 * @param[in] cfg - configuration enum
     	 * @param[out] value - configuration value
     	 */
	bool GetConfigValue(AAMPConfigSettings cfg, double &value);	
	/**
     	 * @fn GetConfigValue
     	 * @param[in] cfg - configuration enum
     	 * @param[out] value - configuration value
     	 */
	bool GetConfigValue(AAMPConfigSettings cfg , int &value);
	/**
	 * @fn GetConfigOwner
     	 * @param[in] cfg - configuration enum
     	 */
	ConfigPriority GetConfigOwner(AAMPConfigSettings cfg);
 	/**
     	 * @fn GetChannelOverride
     	 * @param[in] chName - channel name to search
     	 */
	const char * GetChannelOverride(const std::string chName);    
 	/**
     	 * @fn GetChannelLicenseOverride
     	 * @param[in] chName - channel Name to override
     	 */
 	const char * GetChannelLicenseOverride(const std::string chName);
	/**
     	 * @fn ProcessConfigJson
     	 * @param[in] cfg - json string
     	 * @param[in] owner   - Owner who is setting the value
     	 */
	bool ProcessConfigJson(const char *cfg, ConfigPriority owner );	
	/**
         * @fn ProcessConfigJson
         * @param[in] cfg - json format
         * @param[in] owner   - Owner who is setting the value
         */
        bool ProcessConfigJson(const cJSON *cfgdata, ConfigPriority owner );
	/**
     	 * @fn ProcessConfigText
     	 * @param[in] cfg - config text ( new line separated)
     	 * @param[in] owner   - Owner who is setting the value
     	 */
	bool ProcessConfigText(std::string &cfg, ConfigPriority owner );
	/**
     	 * @fn RestoreConfiguration
     	 * @param[in] owner - Owner value for reverting
     	 * @return None
     	 */
	void RestoreConfiguration(ConfigPriority owner, AampLogManager *mLogObj);	
	/**
     	 * @fn ConfigureLogSettings
     	 * @return None
     	 */
	void ConfigureLogSettings();	
	/**
     	 * @fn GetAampConfigJSONStr
     	 * @param[in] str  - input string where config json will be stored
     	 */
	bool GetAampConfigJSONStr(std::string &str);
	/**
     	 * @fn GetDeveloperConfigData
     	 * @param[in] key - key string to parse
     	 * @param[in] value - value read from input string 
     	 */
	bool GetDeveloperConfigData(std::string &key,std::string &value);
	/**
     	 * @fn DoCustomSetting 
     	 *
	 * @param[in] owner - ConfigPriority owner
     	 * @return None
     	 */
	void DoCustomSetting(ConfigPriority owner);
	/**
	 * @fn CustomArrayRead
     	 * @param[in] customArray - input string where custom config json will be stored
     	 * @param[in] owner - ownership of configs will be stored
     	 */
	void CustomArrayRead( cJSON *customArray,ConfigPriority owner );
	/**
     	 * @fn CustomSearch
     	 * @param[in] url  - input string where url name will be stored
     	 * @param[in] playerId  - input int variable where playerId will be stored
     	 * @param[in] appname  - input string where appname will be stored
     	 */
	bool CustomSearch( std::string url, int playerId , std::string appname);
	AampLogManager *GetLoggerInstance() { return &logging;}
	////////// Special Functions /////////////////////////
	std::string GetUserAgentString();
	//long GetManifestTimeoutMs();
	//long GetNetworkTimeoutMs();
	//LangCodePreference GetLanguageCodePreference();
	//DRMSystems GetPreferredDRM();
private:

	/**
     	 * @fn SetValue
     	 *
     	 * @param[in] setting - Config variable to set
     	 * @param[in] newowner - New owner value
     	 * @param[in] value - Value to set
       	 * @return void
    	 */
	template<class J,class K>
	void SetValue(J &setting, ConfigPriority newowner, const K &value,std::string cfgName);
	void trim(std::string& src);
	
	/**
    	 * @fn GetTR181AAMPConfig
     	 * @param[in] paramName - Parameter Name to parse
     	 * @param[in] iConfigLen - Length of the configuration
     	 */
	char * GetTR181AAMPConfig(const char * paramName, size_t & iConfigLen);
	
	/**
     	 * @fn ReadNumericHelper
     	 * @param[in] valStr - string input to convert
     	 * @param[out] value - coverted output
     	 * @return true on success
     	 */
	template<typename T>
	bool ReadNumericHelper(std::string valStr, T& value);
	/**
     	 * @fn ShowConfiguration
     	 * @param[in] owner - Owner value for listing
     	 * @return None
    	 */
	void ShowConfiguration(ConfigPriority owner);	
	/**
     	 * @fn GetConfigName
     	 * @param[in] cfg  - configuration enum
     	 * @return string - configuration name
     	 */
	std::string GetConfigName(AAMPConfigSettings cfg );
	template<typename T>
	bool ValidateRange(std::string key,T& value);
private:
	typedef std::map<std::string, AampConfigLookupEntry> LookUp;
	typedef std::map<std::string, AampConfigLookupEntry>::iterator LookUpIter;
	LookUp mAampLookupTable;
	typedef std::map<std::string, std::string> DevCmds;
	typedef std::map<std::string, std::string>::iterator DevCmdsIter;
	DevCmds mAampDevCmdTable;
	std::vector<struct customJson>vCustom;
	std::vector<struct customJson>::iterator vCustomIt;
	bool customFound;
	ConfigBool	bAampCfgValue[eAAMPConfig_BoolMaxValue];					/**< Stores bool configuration */
	ConfigInt	iAampCfgValue[eAAMPConfig_IntMaxValue-eAAMPConfig_IntStartValue];		/**< Stores int configuration */
	ConfigLong	lAampCfgValue[eAAMPConfig_LongMaxValue-eAAMPConfig_LongStartValue];		/**< Stores long configuration */
	ConfigDouble 	dAampCfgValue[eAAMPConfig_DoubleMaxValue-eAAMPConfig_DoubleStartValue];		/**< Stores double configuration */
	ConfigString	sAampCfgValue[eAAMPConfig_StringMaxValue-eAAMPConfig_StringStartValue];		/**< Stores string configuration */
	typedef std::list<ConfigChannelInfo> ChannelMap ;
	typedef std::list<ConfigChannelInfo>::iterator ChannelMapIter ;
	ChannelMap mChannelOverrideMap;
};

/**
 * @brief Global configuration */
extern AampConfig  *gpGlobalConfig;
#endif



