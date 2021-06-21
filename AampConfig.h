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
	eAAMPConfig_EnableABR = 0,								/**< Enable/Disable adaptive bitrate logic*/
	eAAMPConfig_Fog, 										/**< Enable / Disable FOG*/
	eAAMPConfig_PrefetchIFramePlaylistDL,					/**< Enabled prefetching of I-Frame playlist*/
	eAAMPConfig_PreservePipeline,							/**< Flush instead of teardown*/
	eAAMPConfig_DemuxAudioHLSTrack ,						/**< Demux Audio track from HLS transport stream*/
	eAAMPConfig_DemuxVideoHLSTrack ,						/**< Demux Video track from HLS transport stream*/
	eAAMPConfig_Throttle,									/**< Regulate output data flow*/
	eAAMPConfig_DemuxAudioBeforeVideo,						/**< Demux video track from HLS transport stream track mode*/
	eAAMPConfig_DemuxHLSVideoTsTrackTM, 					/**< Send demuxed audio before video*/
	eAAMPConfig_ForceEC3,									/**< Forcefully enable DDPlus*/
	eAAMPConfig_DisableEC3, 								/**< Disable DDPlus*/
	eAAMPConfig_DisableATMOS,								/**< Disable Dolby ATMOS*/
	eAAMPConfig_StereoOnly,									/**< Enable Stereo Only playback, disables EC3/ATMOS. Overrides ForceEC3 */
	eAAMPConfig_DescriptiveTrackName,							/**< Enable Descriptive track name*/
	eAAMPConfig_DisablePlaylistIndexEvent,					/**< Disable playlist index event*/
	eAAMPConfig_EnableSubscribedTags,						/**< Enabled subscribed tags*/
	eAAMPConfig_DASHIgnoreBaseURLIfSlash,					/**< Ignore the constructed URI of DASH, if it is / */
	eAAMPConfig_AnonymousLicenseRequest,					/**< Acquire license without token*/
	eAAMPConfig_HLSAVTrackSyncUsingStartTime,				/**< HLS A/V track to be synced with start time*/
	eAAMPConfig_MPDDiscontinuityHandling,					/**< Enable MPD discontinuity handling*/
	eAAMPConfig_MPDDiscontinuityHandlingCdvr,				/**< Enable MPD discontinuity handling for CDVR*/
	eAAMPConfig_ForceHttp,									/**< Force HTTP*/
	eAAMPConfig_InternalReTune, 							/**< Internal re-tune on underflows/ pts errors*/
	eAAMPConfig_AudioOnlyPlayback,							/**< AAMP Audio Only Playback*/
	eAAMPConfig_GStreamerBufferingBeforePlay,				/**< Enable pre buffering logic which ensures minimum buffering is done before pipeline play*/
	eAAMPConfig_EnableMicroEvents,							/**< Enabling the tunetime micro events*/
	eAAMPConfig_EnablePROutputProtection,					/**< Playready output protection config */
	eAAMPConfig_ReTuneOnBufferingTimeout,					/**< Re-tune on buffering timeout */
	eAAMPConfig_SslVerifyPeer,						/**< Enable curl ssl certificate verification. */
	eAAMPConfig_EnableClientDai,							/**< Enabling the client side DAI*/
	eAAMPConfig_PlayAdFromCDN,								/**< Play Ad from CDN. Not from FOG.*/
	eAAMPConfig_EnableVideoEndEvent,						/**< Enable or disable videovend events */
	eAAMPConfig_EnableRectPropertyCfg,						/**< To allow or deny rectangle property set for sink element*/
	eAAMPConfig_ReportVideoPTS, 							/**< Enables Video PTS reporting */
	eAAMPConfig_DecoderUnavailableStrict,					/**< Reports decoder unavailable GST Warning as aamp error*/
	eAAMPConfig_UseAppSrcForProgressivePlayback,			/**< Enables appsrc for playing progressive AV type */
	eAAMPConfig_DescriptiveAudioTrack,						/**< advertise audio tracks using <langcode>-<role> instead of just <langcode> */
	eAAMPConfig_ReportBufferEvent,							/**< Enables Buffer event reporting */
	eAAMPConfig_InfoLogging,								/**< Enables Info logging */
	eAAMPConfig_DebugLogging,								/**< Enables Debug logging */
	eAAMPConfig_TraceLogging,								/**< Enables Trace logging */
	eAAMPConfig_FailoverLogging,							/**< Enables failover logging */
	eAAMPConfig_GSTLogging,									/**< Enables Gstreamer logging */
	eAAMPConfig_ProgressLogging,							/**< Enables Progress logging */
	eAAMPConfig_CurlLogging,								/**< Enables Curl logging */
	eAAMPConfig_CurlLicenseLogging, 						/** <Enable verbose curl logging for license request (non-secclient) */
	eAAMPConfig_MetadataLogging,							/**<Enable timed metadata logging */
	eAAMPConfig_CurlHeader, 								/**<enable curl header response logging on curl errors*/
	eAAMPConfig_StreamLogging,								/**< Enables HLS Playlist content logging */
	eAAMPConfig_ID3Logging,        							/**< Enables ID3 logging */
	//eAAMPConfig_XREEventReporting,							/**< Enable/Disable Event reporting to XRE */
	eAAMPConfig_EnableGstPositionQuery, 					/**<GStreamer position query will be used for progress report events, Enabled by default for non-Intel platforms*/
	eAAMPConfig_MidFragmentSeek,                                                    /**<Enable/Disable the Mid-Fragment seek functionality in aamp.*/
	eAAMPConfig_PropogateURIParam,							/**<Feature where top-level manifest URI parameters included when downloading fragments*/
	eAAMPConfig_UseWesterosSink, 							/**<Enable/Disable player to use westeros sink based video decoding */
	eAAMPConfig_EnableLinearSimulator,						/**<Enable linear simulator for testing purpose, simulate VOD asset as a "virtual linear" stream.*/
	eAAMPConfig_RetuneForUnpairDiscontinuity,				/**<disable unpaired discontinuity retun functionality*/
	eAAMPConfig_RetuneForGSTError,							/**<disable retune mitigation for gst pipeline internal data stream error*/
	eAAMPConfig_MatchBaseUrl,								/**<Enable host of main url will be matched with host of base url*/
	eAAMPConfig_WifiCurlHeader,
	eAAMPConfig_EnableSeekRange,							/**<Enable seekable range reporting via progress events */
	eAAMPConfig_DashParallelFragDownload,					/**<Enable dash fragment parallel download*/
	eAAMPConfig_PersistentBitRateOverSeek,					/**< ABR profile persistence during Seek/Trickplay/Audio switching*/
	eAAMPConfig_SetLicenseCaching,							/**< License caching*/
	eAAMPConfig_Fragmp4PrefetchLicense,					/*** Enable fragment mp4 license prefetching**/
	eAAMPConfig_ABRBufferCheckEnabled,						/**< Flag to enable/disable buffer based ABR handling*/
	eAAMPConfig_NewDiscontinuity,							/**< Flag to enable/disable new discontinuity handling with PDT*/
	eAAMPConfig_PlaylistParallelFetch,						/**< Enabled parallel fetching of audio & video playlists*/
	eAAMPConfig_PlaylistParallelRefresh,					/**< Enabled parallel fetching for refresh of audio & video playlists*/
	eAAMPConfig_BulkTimedMetaReport, 						/**< Enabled Bulk event reporting for TimedMetadata*/
	eAAMPConfig_RemovePersistent,							/**< Flag to enable/disable code in ave drm to avoid crash when majorerror 3321, 3328 occurs*/
	eAAMPConfig_AvgBWForABR,								/** Enables usage of AverageBandwidth if available for ABR */
	eAAMPConfig_NativeCCRendering,							/*** If native CC rendering to be supported */
	eAAMPConfig_Subtec_subtitle,							/**< Enable subtec-based subtitles */
	eAAMPConfig_WebVTTNative,								/**< Enable subtec-based subtitles */
	eAAMPConfig_AsyncTune,								 	/*** To enable Asynchronous tune */
	eAAMPConfig_DisableUnderflow,                                                   /**< Enable/Disable Underflow processing*/
	eAAMPConfig_LimitResolution,                                                    /** Flag to indicate if display resolution based profile selection to be done */
	eAAMPConfig_UseAbsoluteTimeline,					/**Enable Report Progress report position based on Availability Start Time **/
	eAAMPConfig_EnableAccessAttributes,							/*** Usage of Access Attributes in VSS */
	eAAMPConfig_WideVineKIDWorkaround,                          /**< SkyDE Store workaround to pick WV DRM Key Id from different location */
	eAAMPConfig_SyncAudioFragments,							/** Flag to enable Audio Video Fragment Sync */
	eAAMPConfig_RepairIframes,								/*** Enable fragment repair (Stripping and box size correction) for iframe tracks */
	eAAMPConfig_EnableLowLatencyDash,                           			/** Enables Low Latency Dash */
	eAAMPConfig_DisableLowLatencyMonitor,                   				/** Enables Low Latency Monitor Thread */
	eAAMPConfig_DisableLowLatencyABR,							/** Enables Low Latency ABR handling */
	eAAMPConfig_DisableLowLatencyCorrection,                    			/** Enables Low Latency Correction handling */
	eAAMPConfig_EnableLowLatencyOffsetMin,                                /** Enables Low Latency Offset Min handling */
	eAAMPConfig_SEITimeCode,							/** Enables SEI Time Code handling */
	eAAMPConfig_InterruptHandling,							/** Enables Config for network interrupt handling*/
	eAAMPConfig_Disable4K,							/** Enalbe/Disable 4K stream support*/
	eAAMPConfig_EnableSharedSSLSession,                                             /** Enable/Disable config for shared ssl session reuse */
	eAAMPConfig_BoolMaxValue,
	/////////////////////////////////
	eAAMPConfig_IntStartValue,
	eAAMPConfig_HarvestCountLimit,									/**< Number of files to be harvested */
	eAAMPConfig_HarvestConfig,									/**< Indicate type of file to be  harvest */
	eAAMPConfig_ABRCacheLife,									/**< Adaptive bitrate cache life in seconds*/
	eAAMPConfig_ABRCacheLength,									/**< Adaptive bitrate cache length*/
	eAAMPConfig_TimeShiftBufferLength,								/**< TSB length*/
	eAAMPConfig_ABRCacheOutlier,								/**< Adaptive bitrate outlier, if values goes beyond this*/
	eAAMPConfig_ABRSkipDuration,								/**< Initial duration for ABR skip*/
	eAAMPConfig_ABRNWConsistency,								/**< Adaptive bitrate network consistency*/
	eAAMPConfig_ABRThresholdSize,								/**< AAMP ABR threshold size*/
	eAAMPConfig_MaxFragmentCached,								/**< fragment cache length*/
	eAAMPConfig_BufferHealthMonitorDelay,						/**< Buffer health monitor start delay after tune/ seek*/
	eAAMPConfig_BufferHealthMonitorInterval,					/**< Buffer health monitor interval*/
	eAAMPConfig_PreferredDRM,									/**< Preferred DRM*/
	eAAMPConfig_TuneEventConfig,									/**< When to send TUNED event*/
	eAAMPConfig_VODTrickPlayFPS,								/**< Trickplay frames per second for VOD*/
	eAAMPConfig_LinearTrickPlayFPS,								/**< Trickplay frames per second for Linear*/
	eAAMPConfig_ReportProgressInterval,							/**< Interval of progress reporting*/
	eAAMPConfig_LicenseRetryWaitTime,							/**< License retry wait interval*/
	eAAMPConfig_PTSErrorThreshold,								/**< Max number of back-to-back PTS errors within designated time*/
	eAAMPConfig_MaxPlaylistCacheSize,							/**< Max Playlist Cache Size  */
	eAAMPConfig_MaxDASHDRMSessions,								/**< Max drm sessions that can be cached by AampDRMSessionManager*/
	eAAMPConfig_Http5XXRetryWaitInterval,						/**< Wait time in milliseconds before retry for 5xx errors*/
	eAAMPConfig_LanguageCodePreference,							/**< prefered format for normalizing language code */
	eAAMPConfig_RampDownLimit, 							/**<Set fragment rampdown/retry limit for video fragment failure*/
	eAAMPConfig_InitRampDownLimit,									/**<Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time*/
	eAAMPConfig_DRMDecryptThreshold,							/**<Retry count on drm decryption failure*/
	eAAMPConfig_SegmentInjectThreshold, 						/**<Retry count for segment injection discard/failue*/
	eAAMPConfig_InitFragmentRetryCount, 						/**<Retry attempts for init frag curl timeout failures*/
	eAAMPConfig_MinABRNWBufferRampDown, 						/**< Mininum ABR Buffer for Rampdown*/
	eAAMPConfig_MaxABRNWBufferRampUp,							/**< Maximum ABR Buffer for Rampup*/
	eAAMPConfig_PrePlayBufferCount, 							/** Count of segments to be downloaded until play state */
	eAAMPConfig_PreCachePlaylistTime,							/** Max time to complete PreCaching .In Minutes  */
	eAAMPConfig_CEAPreferred,									/*** To force 608/708 track selection in CC manager */
	eAAMPConfig_StallErrorCode,
	eAAMPConfig_StallTimeoutMS,
	eAAMPConfig_InitialBuffer,
	eAAMPConfig_PlaybackBuffer,
	eAAMPConfig_SourceSetupTimeout, 							/**<Timeout value wait for GStreamer appsource setup to complete*/
	eAAMPConfig_DownloadDelay,
	eAAMPConfig_LivePauseBehavior,                                                          /**< player paused state behavior */
	eAAMPConfig_LatencyMonitorDelay,                            				/**< Latency Monitor Delay */
	eAAMPConfig_LatencyMonitorInterval,                         				/**< Latency Monitor Interval */
	eAAMPConfig_MaxFragmentChunkCached,                         				/**< fragment chunk cache length*/
	eAAMPConfig_ABRChunkThresholdSize,                         				/**< AAMP ABR Chunk threshold size*/
	eAAMPConfig_IntMaxValue,
	///////////////////////////////////
	eAAMPConfig_LongStartValue,
	eAAMPConfig_DefaultBitrate,									/**< Default bitrate*/
	eAAMPConfig_DefaultBitrate4K,								/**< Default 4K bitrate*/
	eAAMPConfig_IFrameDefaultBitrate,							/**< Default bitrate for iframe track selection for non-4K assets*/
	eAAMPConfig_IFrameDefaultBitrate4K,							/**< Default bitrate for iframe track selection for 4K assets*/
	eAAMPConfig_CurlStallTimeout,								/**< Timeout value for detection curl download stall in seconds*/
	eAAMPConfig_CurlDownloadStartTimeout,						/**< Timeout value for curl download to start after connect in seconds*/
	eAAMPConfig_DiscontinuityTimeout,							/**< Timeout value to auto process pending discontinuity after detecting cache is empty*/
	eAAMPConfig_MinBitrate, 									/**<minimum bitrate filter for playback profiles */
	eAAMPConfig_MaxBitrate, 									/**<maximum bitrate filter for playback profiles*/

	eAAMPConfig_LongMaxValue,
	////////////////////////////////////
	eAAMPConfig_DoubleStartValue,
	eAAMPConfig_NetworkTimeout,									/**< Fragment download timeout in sec*/
	eAAMPConfig_ManifestTimeout,								/**< Manifest download timeout in sec*/
	eAAMPConfig_PlaylistTimeout,								/**<playlist download time out in sec*/
	eAAMPConfig_PlaybackOffset,								/**<playback offset value in seconds*/
	eAAMPConfig_LiveOffset, 									/**< Current LIVE offset*/
	eAAMPConfig_CDVRLiveOffset, 								/**< CDVR LIVE offset*/
	eAAMPConfig_DoubleMaxValue,
	////////////////////////////////////
	eAAMPConfig_StringStartValue,
	eAAMPConfig_MapMPD, 										/**< host name in url for which hls to mpd mapping done'*/
	eAAMPConfig_MapM3U8,										/**< host name in url for which mpd to hls mapping done'*/
	eAAMPConfig_HarvestPath,									/**< Path to store Harvested files */
	eAAMPConfig_LicenseServerUrl,								/**< License server URL ( if no individual configuration */
	eAAMPConfig_CKLicenseServerUrl,								/**< ClearKey License server URL*/
	eAAMPConfig_PRLicenseServerUrl,								/**< PlayReady License server URL*/
	eAAMPConfig_WVLicenseServerUrl,								/**< Widevine License server URL*/
	eAAMPConfig_UserAgent,										/**< Curl user-agent string */
	eAAMPConfig_SubTitleLanguage,								/**< User preferred subtitle language*/
	//eAAMPConfig_RedirectUrl,									/**<redirects requests to tune to url1 to url2*/
	eAAMPConfig_CustomHeader,								  	/**<custom header string data to be appended to curl request*/
	eAAMPConfig_URIParameter,									/**<uri parameter data to be appended on download-url during curl request*/
	eAAMPConfig_NetworkProxy,									/**<Network Proxy */
	eAAMPConfig_LicenseProxy,									/**<License Proxy */
	eAAMPConfig_AuthToken,									/**<Session Token  */
	eAAMPConfig_LogLevel,										/**< New Configuration to overide info/debug/trace */
	eAAMPConfig_CustomHeaderLicense,                            					/**<custom header string data to be appended to curl License request*/
	eAAMPConfig_PreferredAudioRendition,								/**< PreferredAudioRendition*/
	eAAMPConfig_PreferredAudioCodec,								/**< PreferredAudioCodec*/
	eAAMPConfig_PreferredAudioLanguage,								/**< PreferredAudioLanguage*/
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
 * @brief AAMP Config ownership enum string mapping table
 */

struct AampOwnerLookupEntry
{
	const char* ownerName;
	ConfigPriority ownerValue;
};


/**
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
 * @brief AAMP Config Class defn
 */
class AampConfig
{
public:
	// Had to define as public as globalConfig loggin var is used multiple files
	// TODO when player level logging is done, need to remove this
	AampLogManager logging;                 /**< Aamp log manager class*/
public:
	AampConfig();
	~AampConfig(){};
	void Initialize();
	void ShowOperatorSetConfiguration();
	void ShowAppSetConfiguration();
	void ShowStreamSetConfiguration();
	void ShowDefaultAampConfiguration();
	void ShowDevCfgConfiguration();
	void ShowAAMPConfiguration();
	bool ReadAampCfgTxtFile();
	bool ReadAampCfgJsonFile();
	void ReadOperatorConfiguration();
	void ParseAampCfgTxtString(std::string &cfg);
	void ParseAampCfgJsonString(std::string &cfg);
	void ToggleConfigValue(ConfigPriority owner, AAMPConfigSettings cfg );
	template<typename T>
	void SetConfigValue(ConfigPriority owner, AAMPConfigSettings cfg , const T &value);
	bool IsConfigSet(AAMPConfigSettings cfg);
	bool GetConfigValue(AAMPConfigSettings cfg, std::string &value);
	bool GetConfigValue(AAMPConfigSettings cfg, long &value);
	bool GetConfigValue(AAMPConfigSettings cfg, double &value);
	bool GetConfigValue(AAMPConfigSettings cfg , int &value);
	ConfigPriority GetConfigOwner(AAMPConfigSettings cfg);
	const char * GetChannelOverride(const std::string chName);
	const char * GetChannelLicenseOverride(const std::string chName);
	bool ProcessConfigJson(const char *, ConfigPriority owner );
	bool ProcessConfigText(std::string &cfg, ConfigPriority owner );
	void RestoreConfiguration(ConfigPriority owner);
	void ConfigureLogSettings();
	bool GetAampConfigJSONStr(std::string &str);
	bool GetDeveloperConfigData(std::string &key,std::string &value);
	void DoCustomSetting(ConfigPriority owner);
	void CustomArrayRead( cJSON *customArray,ConfigPriority owner );
	bool CustomSearch( std::string url, int playerId , std::string appname);

	////////// Special Functions /////////////////////////
	std::string GetUserAgentString();
	//long GetManifestTimeoutMs();
	//long GetNetworkTimeoutMs();
	//LangCodePreference GetLanguageCodePreference();
	//DRMSystems GetPreferredDRM();
private:
	template<class J,class K>
	void SetValue(J &setting, ConfigPriority newowner, const K &value,std::string cfgName);
	void trim(std::string& src);
	char * GetTR181AAMPConfig(const char * paramName, size_t & iConfigLen);
	template<typename T>
	bool ReadNumericHelper(std::string valStr, T& value);
	void ShowConfiguration(ConfigPriority owner);
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
	ConfigBool	bAampCfgValue[eAAMPConfig_BoolMaxValue];								// Stores bool configuration
	ConfigInt	iAampCfgValue[eAAMPConfig_IntMaxValue-eAAMPConfig_IntStartValue];		// Stores int configuration
	ConfigLong	lAampCfgValue[eAAMPConfig_LongMaxValue-eAAMPConfig_LongStartValue];		// Stores long configuration
	ConfigDouble 	dAampCfgValue[eAAMPConfig_DoubleMaxValue-eAAMPConfig_DoubleStartValue];		// Stores double configuration
	ConfigString	sAampCfgValue[eAAMPConfig_StringMaxValue-eAAMPConfig_StringStartValue];	// Stores string configuration
	typedef std::list<ConfigChannelInfo> ChannelMap ;
	typedef std::list<ConfigChannelInfo>::iterator ChannelMapIter ;
	ChannelMap mChannelOverrideMap;
};

extern AampConfig  *gpGlobalConfig;
#endif



