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
#include "AampConfig.h"
#include "_base64.h"
#include "base16.h"
#include "AampJsonObject.h" // For JSON parsing


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

template void AampConfig::SetConfigValue<long>(ConfigPriority owner, AAMPConfigSettings cfg , const long &value);
template void AampConfig::SetConfigValue<double>(ConfigPriority owner, AAMPConfigSettings cfg , const double &value);
template void AampConfig::SetConfigValue<int>(ConfigPriority owner, AAMPConfigSettings cfg , const int &value);
template void AampConfig::SetConfigValue<bool>(ConfigPriority owner, AAMPConfigSettings cfg , const bool &value);

#ifdef WIN32
std::string AampConfig::cWindowsCfg= AAMP_CFG_PATH;
std::string AampConfig::cWindowsJsonCfg = AAMP_JSON_PATH;
#endif

/**
 * @brief AAMP Config Owners enum-string mapping table
 */

static AampOwnerLookupEntry OwnerLookUpTable[] =
{
	{"def",AAMP_DEFAULT_SETTING},
	{"oper",AAMP_OPERATOR_SETTING},
	{"stream",AAMP_STREAM_SETTING},
	{"app",AAMP_APPLICATION_SETTING},
	{"tune",AAMP_APPLICATION_SETTING},
	{"cfg",AAMP_DEV_CFG_SETTING},
	{"unknown",AAMP_MAX_SETTING}
};

/**
 * @brief AAMP Config Command lookup table
 */

///// No order is required to add to this table ,
///// Better to add always at the end than inserting in the middle to avoid git merge conflicts
///// Format -> configuration name, configuration enum (defined in AampConfig.h) , minValue(-1 if none) , maxValue(-1 if none)
static AampConfigLookupEntry ConfigLookUpTable[] =
{
	{"mapMPD",eAAMPConfig_MapMPD,-1,-1},
	{"mapM3U8",eAAMPConfig_MapM3U8,-1,-1},
	{"fragmp4LicensePrefetch",eAAMPConfig_Fragmp4PrefetchLicense,-1,-1},
	{"enableVideoEndEvent",eAAMPConfig_EnableVideoEndEvent,-1,-1},
	{"fog",eAAMPConfig_Fog,-1,-1},
	{"harvestCountLimit",eAAMPConfig_HarvestCountLimit,-1,-1},
	{"harvestConfig",eAAMPConfig_HarvestConfig,-1,-1},
	{"harvestPath",eAAMPConfig_HarvestPath,-1,-1},
	{"forceEC3",eAAMPConfig_ForceEC3,-1,-1},										// Complete
	{"disableEC3",eAAMPConfig_DisableEC3,-1,-1},									// Complete
	{"disableATMOS",eAAMPConfig_DisableATMOS,-1,-1},								// Complete
	{"stereoOnly",eAAMPConfig_StereoOnly,-1,-1},									// Complete
	{"cdvrLiveOffset",eAAMPConfig_CDVRLiveOffset,{.dMinValue = 0},{.dMaxValue=50}},
	{"liveOffset",eAAMPConfig_LiveOffset,{.dMinValue = 0},{.dMaxValue=50}},
	{"disablePlaylistIndexEvent",eAAMPConfig_DisablePlaylistIndexEvent,-1,-1},		// Complete
	{"enableSubscribedTags",eAAMPConfig_EnableSubscribedTags,-1,-1},				// Complete
	{"networkTimeout",eAAMPConfig_NetworkTimeout,{.dMinValue = -1},{.dMaxValue=-1}},
	{"manifestTimeout",eAAMPConfig_ManifestTimeout,{.dMinValue = -1},{.dMaxValue=-1}},
	{"playlistTimeout",eAAMPConfig_PlaylistTimeout,{.dMinValue = -1},{.dMaxValue=-1}},
	{"dashIgnoreBaseUrlIfSlash",eAAMPConfig_DASHIgnoreBaseURLIfSlash,-1,-1},	// Complete
	{"licenseAnonymousRequest",eAAMPConfig_AnonymousLicenseRequest,-1,-1},		// Complete
	{"useLinearSimulator",eAAMPConfig_EnableLinearSimulator,-1,-1},
	{"info",eAAMPConfig_InfoLogging,-1,-1},
	{"failover",eAAMPConfig_FailoverLogging,-1,-1},
	{"curlHeader",eAAMPConfig_CurlHeader,-1,-1},
	{"curlLicense",eAAMPConfig_CurlLicenseLogging,-1,-1},
	{"logMetadata",eAAMPConfig_MetadataLogging,-1,-1},
	{"customHeader",eAAMPConfig_CustomHeader,-1,-1},
	{"uriParameter",eAAMPConfig_URIParameter,-1,-1},
	{"gst",eAAMPConfig_GSTLogging,-1,-1},
	{"progress",eAAMPConfig_ProgressLogging,-1,-1},
	{"debug",eAAMPConfig_DebugLogging,-1,-1},
	{"trace",eAAMPConfig_TraceLogging,-1,-1},
	{"curl",eAAMPConfig_CurlLogging,-1,-1},
	{"stream",eAAMPConfig_StreamLogging,-1,-1},
	{"defaultBitrate",eAAMPConfig_DefaultBitrate,-1,-1},
	{"defaultBitrate4K",eAAMPConfig_DefaultBitrate4K,-1,-1},
	{"abr",eAAMPConfig_EnableABR,-1,-1},
	{"abrCacheLife",eAAMPConfig_ABRCacheLife,-1,-1},
	{"abrCacheLength",eAAMPConfig_ABRCacheLength,-1,-1},
	{"useNewABR",eAAMPConfig_ABRBufferCheckEnabled,-1,-1},
	{"useNewAdBreaker",eAAMPConfig_NewDiscontinuity,-1,-1},
	{"reportVideoPTS",eAAMPConfig_ReportVideoPTS,-1,-1},
	{"decoderUnavailableStrict",eAAMPConfig_DecoderUnavailableStrict,-1,-1},
	{"descriptiveAudioTrack",eAAMPConfig_DescriptiveAudioTrack,-1,-1},
	{"langCodePreference",eAAMPConfig_LanguageCodePreference,{.iMinValue=0},{.iMaxValue=3}},
	{"appSrcForProgressivePlayback",eAAMPConfig_UseAppSrcForProgressivePlayback,-1,-1},
	{"abrCacheOutlier",eAAMPConfig_ABRCacheOutlier,-1,-1},
	{"abrSkipDuration",eAAMPConfig_ABRSkipDuration,-1,-1},
	{"abrNwConsistency",eAAMPConfig_ABRNWConsistency,-1,-1},
	{"minABRBufferRampdown",eAAMPConfig_MinABRNWBufferRampDown,-1,-1},
	{"preservePipeline",eAAMPConfig_PreservePipeline,-1,-1},
	{"demuxHlsAudioTrack",eAAMPConfig_DemuxAudioHLSTrack,-1,-1},
	{"demuxHlsVideoTrack",eAAMPConfig_DemuxVideoHLSTrack,-1,-1},
	{"demuxHlsVideoTrackTrickMode",eAAMPConfig_DemuxHLSVideoTsTrackTM,-1,-1},
	{"demuxAudioBeforeVideo",eAAMPConfig_DemuxAudioBeforeVideo,-1,-1},
	{"throttle",eAAMPConfig_Throttle,-1,-1},
	{"bufferHealthMonitorDelay",eAAMPConfig_BufferHealthMonitorDelay,-1,-1},
	{"bufferHealthMonitorInterval",eAAMPConfig_BufferHealthMonitorInterval,-1,-1},
	{"preferredDrm",eAAMPConfig_PreferredDRM,{.iMinValue=0},{.iMaxValue=eDRM_MAX_DRMSystems}},
	{"playreadyOutputProtection",eAAMPConfig_EnablePROutputProtection,-1,-1},
	{"liveTuneEvent",eAAMPConfig_LiveTuneEvent,{.iMinValue=eTUNED_EVENT_ON_PLAYLIST_INDEXED},{.iMaxValue=eTUNED_EVENT_ON_GST_PLAYING}},
	{"vodTuneEvent",eAAMPConfig_VODTuneEvent,{.iMinValue=eTUNED_EVENT_ON_PLAYLIST_INDEXED},{.iMaxValue=eTUNED_EVENT_ON_GST_PLAYING}},
	{"parallelPlaylistDownload",eAAMPConfig_PlaylistParallelFetch,-1,-1},
	{"dashParallelFragDownload",eAAMPConfig_DashParallelFragDownload,-1,-1},
	{"parallelPlaylistRefresh",eAAMPConfig_PlaylistParallelRefresh ,-1,-1},
	{"bulkTimedMetadata",eAAMPConfig_BulkTimedMetaReport,-1,-1},
	{"useRetuneForUnpairedDiscontinuity",eAAMPConfig_RetuneForUnpairDiscontinuity,-1,-1},
	{"useRetuneForGstInternalError",eAAMPConfig_RetuneForGSTError,-1,-1},
	{"useWesterosSink",eAAMPConfig_UseWesterosSink,-1,-1},
	{"setLicenseCaching",eAAMPConfig_SetLicenseCaching,-1,-1},
	{"propagateUriParameters",eAAMPConfig_PropogateURIParam,-1,-1},
	{"preFetchIframePlaylist",eAAMPConfig_PrefetchIFramePlaylistDL,-1,-1},
	{"hlsAVTrackSyncUsingPDT",eAAMPConfig_HLSAVTrackSyncUsingStartTime,-1,-1},
	{"mpdDiscontinuityHandling",eAAMPConfig_MPDDiscontinuityHandling,-1,-1},
	{"mpdDiscontinuityHandlingCdvr",eAAMPConfig_MPDDiscontinuityHandlingCdvr,-1,-1},
	{"vodTrickPlayFps",eAAMPConfig_VODTrickPlayFPS,-1,-1},
	{"linearTrickPlayFps",eAAMPConfig_LinearTrickPlayFPS,-1,-1},
	{"progressReportingInterval",eAAMPConfig_ReportProgressInterval,-1,-1},
	{"forceHttp",eAAMPConfig_ForceHttp,-1,-1},
	{"internalRetune",eAAMPConfig_InternalReTune,-1,-1},
	{"gstBufferAndPlay",eAAMPConfig_GStreamerBufferingBeforePlay,-1,-1},
	{"retuneOnBufferingTimeout",eAAMPConfig_ReTuneOnBufferingTimeout,-1,-1},
	{"iframeDefaultBitrate",eAAMPConfig_IFrameDefaultBitrate,-1,-1},
	{"iframeDefaultBitrate4K",eAAMPConfig_IFrameDefaultBitrate4K,-1,-1},
	{"audioOnlyPlayback",eAAMPConfig_AudioOnlyPlayback,-1,-1},
	{"licenseRetryWaitTime",eAAMPConfig_LicenseRetryWaitTime,-1,-1},
	{"downloadBuffer",eAAMPConfig_MaxFragmentCached,-1,-1},
	{"ptsErrorThreshold",eAAMPConfig_PTSErrorThreshold,-1,-1},
	{"enableVideoRectangle",eAAMPConfig_EnableRectPropertyCfg,-1,-1},
	{"maxPlaylistCacheSize",eAAMPConfig_MaxPlaylistCacheSize,{.iMinValue=0},{.iMaxValue=15360}},              // Range for PlaylistCache size - upto 15 MB max
	{"dashMaxDrmSessions",eAAMPConfig_MaxDASHDRMSessions,{.iMinValue=1},{.iMaxValue=MAX_DASH_DRM_SESSIONS}},
	{"userAgent",eAAMPConfig_UserAgent,-1,-1},
	{"waitTimeBeforeRetryHttp5xx",eAAMPConfig_Http5XXRetryWaitInterval,-1,-1},
	{"preplayBuffercount",eAAMPConfig_PrePlayBufferCount,-1,-1},
	{"sslVerifyPeer",eAAMPConfig_SslVerifyPeer,-1,-1},
	{"downloadStallTimeout",eAAMPConfig_CurlStallTimeout,-1,-1},
	{"downloadStartTimeout",eAAMPConfig_CurlDownloadStartTimeout,-1,-1},
	{"discontinuityTimeout",eAAMPConfig_DiscontinuityTimeout,-1,-1},
	{"client-dai",eAAMPConfig_EnableClientDai,-1,-1},                               // not changing this name , this is already in use for RFC
	{"cdnAdsOnly",eAAMPConfig_PlayAdFromCDN,-1,-1},
	{"thresholdSizeABR",eAAMPConfig_ABRThresholdSize,-1,-1},
	{"preferredSubtitleLanguage",eAAMPConfig_SubTitleLanguage,-1,-1},
	{"reportBufferEvent",eAAMPConfig_ReportBufferEvent,-1,-1},
	{"enableTuneProfiling",eAAMPConfig_EnableMicroEvents,-1,-1},
	{"gstPositionQueryEnable",eAAMPConfig_EnableGstPositionQuery,-1,-1},
	{"useMatchingBaseUrl",eAAMPConfig_MatchBaseUrl,-1,-1},
	{"removeAVEDRMPersistent",eAAMPConfig_RemovePersistent,-1,-1},
	{"useAverageBandwidth",eAAMPConfig_AvgBWForABR,-1,-1},
	{"preCachePlaylistTime",eAAMPConfig_PreCachePlaylistTime,-1,-1},
	{"fragmentRetryLimit",eAAMPConfig_RampDownLimit,-1,-1},
	{"segmentInjectFailThreshold",eAAMPConfig_SegmentInjectThreshold,-1,-1},
	{"drmDecryptFailThreshold",eAAMPConfig_DRMDecryptThreshold,-1,-1},
	{"minBitrate",eAAMPConfig_MinBitrate,-1,-1},
	{"maxBitrate",eAAMPConfig_MaxBitrate,-1,-1},
	{"initFragmentRetryCount",eAAMPConfig_InitFragmentRetryCount,-1,-1},
	{"nativeCCRendering",eAAMPConfig_NativeCCRendering,-1,-1},
	{"subtecSubtitle",eAAMPConfig_Subtec_subtitle,-1,-1},
	{"webVttNative",eAAMPConfig_WebVTTNative,-1,-1},
	{"ceaFormat",eAAMPConfig_CEAPreferred,-1,1},
	{"asyncTune",eAAMPConfig_AsyncTune,-1,-1},
	{"initRampdownLimit",eAAMPConfig_InitRampDownLimit,-1,-1},
	{"enableSeekableRange",eAAMPConfig_EnableSeekRange,-1,-1},
	{"maxTimeoutForSourceSetup",eAAMPConfig_SourceSetupTimeout,-1,-1},
	{"seekMidFragment",eAAMPConfig_MidFragmentSeek,-1,-1},
	{"wifiCurlHeader",eAAMPConfig_WifiCurlHeader,-1,-1},
	{"persistBitrateOverSeek",eAAMPConfig_PersistentBitRateOverSeek,-1,-1},
	{"log",eAAMPConfig_LogLevel,-1,-1},
	{"maxABRBufferRampup",eAAMPConfig_MaxABRNWBufferRampUp,-1,-1},
	{"networkProxy",eAAMPConfig_NetworkProxy,-1,-1},
	{"licenseProxy",eAAMPConfig_LicenseProxy,-1,-1},
	{"sessionToken",eAAMPConfig_SessionToken,-1,-1},
	{"enableAccessAttributes",eAAMPConfig_EnableAccessAttributes,-1,-1},
	{"ckLicenseServerUrl",eAAMPConfig_CKLicenseServerUrl,-1,-1},
	{"licenseServerUrl",eAAMPConfig_LicenseServerUrl,-1,-1},
	{"prLicenseServerUrl",eAAMPConfig_PRLicenseServerUrl,-1,-1},
	{"wvLicenseServerUrl",eAAMPConfig_WVLicenseServerUrl,-1,-1},
	{"stallErrorCode",eAAMPConfig_StallErrorCode,-1,-1},
	{"stallTimeout",eAAMPConfig_StallTimeoutMS,-1,-1},
	{"initialBuffer",eAAMPConfig_InitialBuffer,-1,-1},
	{"downloadDelay",eAAMPConfig_DownloadDelay,-1,-1},
	{"livePauseBehavior",eAAMPConfig_LivePauseBehavior,{.iMinValue=ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE},{.iMaxValue=ePAUSED_BEHAVIOR_MAX}},
	{"disableUnderflow",eAAMPConfig_DisableUnderflow,-1,-1},
	{"limitResolution",eAAMPConfig_LimitResolution,-1,-1},
	{"useAbsoluteTimeline",eAAMPConfig_UseAbsoluteTimeline,-1,-1},
	{"id3",eAAMPConfig_ID3Logging,-1,-1},
	{"SkyStoreDE",eAAMPConfig_WideVineKIDWorkaround,-1,-1},
	{"repairIframes",eAAMPConfig_RepairIframes,-1,-1}
};

/////////////////// Public Functions /////////////////////////////////////
/**
 * @brief AampConfig Constructor function . Default values defined
 *
 * @return None
 */
AampConfig::AampConfig():mAampLookupTable(),mChannelOverrideMap(),logging(),mAampDevCmdTable()
{
	for(int i=0; i<sizeof(ConfigLookUpTable) / sizeof(AampConfigLookupEntry); ++i)
	{
		mAampLookupTable[ConfigLookUpTable[i].cmdString] = ConfigLookUpTable[i];
	}
}


void AampConfig::Initialize()
{
	// Player Default Configuration
	///////////////// Following for Boolean setting ////////////////////////////
	bAampCfgValue[eAAMPConfig_EnableABR].value				=	true;
	bAampCfgValue[eAAMPConfig_Fog].value					=	true;
	bAampCfgValue[eAAMPConfig_PrefetchIFramePlaylistDL].value		=	false;
	bAampCfgValue[eAAMPConfig_PreservePipeline].value			=	false;
	bAampCfgValue[eAAMPConfig_DemuxAudioHLSTrack].value			=	true;
	bAampCfgValue[eAAMPConfig_DemuxVideoHLSTrack].value			=	true;
	bAampCfgValue[eAAMPConfig_Throttle].value					=	false;
	bAampCfgValue[eAAMPConfig_DemuxAudioBeforeVideo].value			=	false;
	bAampCfgValue[eAAMPConfig_DemuxHLSVideoTsTrackTM].value			=	true;
	bAampCfgValue[eAAMPConfig_ForceEC3].value				=	false;
	bAampCfgValue[eAAMPConfig_StereoOnly].value				=	false;
	bAampCfgValue[eAAMPConfig_DisableEC3].value				=	false;
	bAampCfgValue[eAAMPConfig_DisableATMOS].value				=	false;
	bAampCfgValue[eAAMPConfig_DisablePlaylistIndexEvent].value		=	true;
	bAampCfgValue[eAAMPConfig_EnableSubscribedTags].value			=	true;
	bAampCfgValue[eAAMPConfig_DASHIgnoreBaseURLIfSlash].value		=	false;
	bAampCfgValue[eAAMPConfig_AnonymousLicenseRequest].value		=	false;
	bAampCfgValue[eAAMPConfig_HLSAVTrackSyncUsingStartTime].value		=	false;
	bAampCfgValue[eAAMPConfig_MPDDiscontinuityHandling].value		=	true;
	bAampCfgValue[eAAMPConfig_MPDDiscontinuityHandlingCdvr].value		=	true;
	bAampCfgValue[eAAMPConfig_ForceHttp].value				=	false;
	bAampCfgValue[eAAMPConfig_InternalReTune].value				=	true;
	bAampCfgValue[eAAMPConfig_AudioOnlyPlayback].value			=	false;
#ifdef REALTEKCE
	// XIONE-3397 temporary workaround for SkyDE - use of queued_frames for default 'prebuffering' is not working (this can be removed once REALTEK-286 addressed)
	bAampCfgValue[eAAMPConfig_GStreamerBufferingBeforePlay].value		=	false;
#else	
	bAampCfgValue[eAAMPConfig_GStreamerBufferingBeforePlay].value		=	true;
#endif
	bAampCfgValue[eAAMPConfig_EnableMicroEvents].value			=	false;
	bAampCfgValue[eAAMPConfig_EnablePROutputProtection].value		=	false;
	bAampCfgValue[eAAMPConfig_ReTuneOnBufferingTimeout].value		=	true;
	bAampCfgValue[eAAMPConfig_SslVerifyPeer].value				=	false;
	bAampCfgValue[eAAMPConfig_EnableClientDai].value			=	false;
	bAampCfgValue[eAAMPConfig_PlayAdFromCDN].value				=	false;
	bAampCfgValue[eAAMPConfig_EnableVideoEndEvent].value			=	true;
	bAampCfgValue[eAAMPConfig_EnableRectPropertyCfg].value			=	true;
	bAampCfgValue[eAAMPConfig_ReportVideoPTS].value				=	false;
	bAampCfgValue[eAAMPConfig_DecoderUnavailableStrict].value		=	false;
	bAampCfgValue[eAAMPConfig_UseAppSrcForProgressivePlayback].value	=	false;
	bAampCfgValue[eAAMPConfig_DescriptiveAudioTrack].value			=	false;
	bAampCfgValue[eAAMPConfig_ReportBufferEvent].value			=	true;
	bAampCfgValue[eAAMPConfig_InfoLogging].value				=	false;
	bAampCfgValue[eAAMPConfig_DebugLogging].value				=	false;
	bAampCfgValue[eAAMPConfig_TraceLogging].value				=	false;
	bAampCfgValue[eAAMPConfig_FailoverLogging].value			=	false;
	bAampCfgValue[eAAMPConfig_GSTLogging].value					=	false;
	bAampCfgValue[eAAMPConfig_ProgressLogging].value			=	false;
	bAampCfgValue[eAAMPConfig_CurlLogging].value				=	false;
	bAampCfgValue[eAAMPConfig_CurlLicenseLogging].value			=	false;
	bAampCfgValue[eAAMPConfig_MetadataLogging].value			=	false;
	bAampCfgValue[eAAMPConfig_StreamLogging].value				=	false;
	bAampCfgValue[eAAMPConfig_ID3Logging].value    				= 	false;
	bAampCfgValue[eAAMPConfig_CurlHeader].value					=	false;
	//bAampCfgValue[eAAMPConfig_XREEventReporting].value			=	true;
#ifdef INTELCE
	bAampCfgValue[eAAMPConfig_EnableGstPositionQuery].value			=	false;
#else
       bAampCfgValue[eAAMPConfig_EnableGstPositionQuery].value			=	true;
#endif
	bAampCfgValue[eAAMPConfig_MidFragmentSeek].value                        =       false;
	bAampCfgValue[eAAMPConfig_PropogateURIParam].value			=	true;
#if defined(REALTEKCE) || defined(AMLOGIC)	// Temporary till westerossink disable is rollbacked
	bAampCfgValue[eAAMPConfig_UseWesterosSink].value			=	true;
#else
	bAampCfgValue[eAAMPConfig_UseWesterosSink].value			=	false;
#endif
	bAampCfgValue[eAAMPConfig_RetuneForGSTError].value			=	true;
	bAampCfgValue[eAAMPConfig_MatchBaseUrl].value				=	false;
#ifdef IARM_MGR
	bAampCfgValue[eAAMPConfig_WifiCurlHeader].value                         =       true;
#else
	bAampCfgValue[eAAMPConfig_WifiCurlHeader].value                         =       false;
#endif

	bAampCfgValue[eAAMPConfig_EnableLinearSimulator].value			=	false;
	bAampCfgValue[eAAMPConfig_RetuneForUnpairDiscontinuity].value		=	true;
	bAampCfgValue[eAAMPConfig_EnableSeekRange].value			=	false;
	bAampCfgValue[eAAMPConfig_DashParallelFragDownload].value		=	true;
	bAampCfgValue[eAAMPConfig_PersistentBitRateOverSeek].value		=	false;
	bAampCfgValue[eAAMPConfig_SetLicenseCaching].value			=	true;
	bAampCfgValue[eAAMPConfig_Fragmp4PrefetchLicense].value			=	true;
	bAampCfgValue[eAAMPConfig_ABRBufferCheckEnabled].value			=	true;
	bAampCfgValue[eAAMPConfig_NewDiscontinuity].value			=	false;
	bAampCfgValue[eAAMPConfig_PlaylistParallelFetch].value			=	false;
	bAampCfgValue[eAAMPConfig_PlaylistParallelRefresh].value		=	true;
	bAampCfgValue[eAAMPConfig_BulkTimedMetaReport].value			=	false;
	bAampCfgValue[eAAMPConfig_RemovePersistent].value			=	false;
	bAampCfgValue[eAAMPConfig_AvgBWForABR].value				=	false;
	bAampCfgValue[eAAMPConfig_NativeCCRendering].value			=	false;
	bAampCfgValue[eAAMPConfig_Subtec_subtitle].value			=	true;
	bAampCfgValue[eAAMPConfig_WebVTTNative].value				=	true;
 	bAampCfgValue[eAAMPConfig_AsyncTune].value				=	false;
	bAampCfgValue[eAAMPConfig_EnableAccessAttributes].value			=	true;
	bAampCfgValue[eAAMPConfig_DisableUnderflow].value                       =       false;
	bAampCfgValue[eAAMPConfig_LimitResolution].value                        =       false;
	bAampCfgValue[eAAMPConfig_UseAbsoluteTimeline].value                  	=       false;
	bAampCfgValue[eAAMPConfig_WideVineKIDWorkaround].value                  	=       false;
	bAampCfgValue[eAAMPConfig_RepairIframes].value                  	=       false;

	///////////////// Following for Integer Data type configs ////////////////////////////
	iAampCfgValue[eAAMPConfig_HarvestCountLimit-eAAMPConfig_IntStartValue].value		=	0;
	iAampCfgValue[eAAMPConfig_ABRCacheLife-eAAMPConfig_IntStartValue].value			=	DEFAULT_ABR_CACHE_LIFE;

	iAampCfgValue[eAAMPConfig_ABRCacheLength-eAAMPConfig_IntStartValue].value		=	DEFAULT_ABR_CACHE_LENGTH;
	iAampCfgValue[eAAMPConfig_ABRCacheOutlier-eAAMPConfig_IntStartValue].value		=	DEFAULT_ABR_OUTLIER;

	iAampCfgValue[eAAMPConfig_ABRSkipDuration-eAAMPConfig_IntStartValue].value		=	DEFAULT_ABR_SKIP_DURATION;
	iAampCfgValue[eAAMPConfig_ABRNWConsistency-eAAMPConfig_IntStartValue].value		=	DEFAULT_ABR_NW_CONSISTENCY_CNT;
	iAampCfgValue[eAAMPConfig_BufferHealthMonitorDelay-eAAMPConfig_IntStartValue].value     =       DEFAULT_BUFFER_HEALTH_MONITOR_DELAY;
	iAampCfgValue[eAAMPConfig_BufferHealthMonitorInterval-eAAMPConfig_IntStartValue].value  =       DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL;
	iAampCfgValue[eAAMPConfig_ReportProgressInterval-eAAMPConfig_IntStartValue].value	=	DEFAULT_REPORT_PROGRESS_INTERVAL;
	iAampCfgValue[eAAMPConfig_LicenseRetryWaitTime-eAAMPConfig_IntStartValue].value		=	DEFAULT_LICENSE_REQ_RETRY_WAIT_TIME;
	iAampCfgValue[eAAMPConfig_HarvestConfig-eAAMPConfig_IntStartValue].value		=	0;
	iAampCfgValue[eAAMPConfig_PreferredDRM-eAAMPConfig_IntStartValue].value			=	eDRM_PlayReady;
	iAampCfgValue[eAAMPConfig_CEAPreferred-eAAMPConfig_IntStartValue].value			=	-1;
	iAampCfgValue[eAAMPConfig_LiveTuneEvent-eAAMPConfig_IntStartValue].value		=	eTUNED_EVENT_ON_GST_PLAYING;
	iAampCfgValue[eAAMPConfig_VODTuneEvent-eAAMPConfig_IntStartValue].value			=	eTUNED_EVENT_ON_GST_PLAYING;
	iAampCfgValue[eAAMPConfig_MaxPlaylistCacheSize-eAAMPConfig_IntStartValue].value		=	MAX_PLAYLIST_CACHE_SIZE;
	iAampCfgValue[eAAMPConfig_MaxDASHDRMSessions-eAAMPConfig_IntStartValue].value		=	MIN_DASH_DRM_SESSIONS;
	iAampCfgValue[eAAMPConfig_InitRampDownLimit-eAAMPConfig_IntStartValue].value            =       0;
	iAampCfgValue[eAAMPConfig_MaxFragmentCached-eAAMPConfig_IntStartValue].value            =       DEFAULT_CACHED_FRAGMENTS_PER_TRACK;

	iAampCfgValue[eAAMPConfig_VODTrickPlayFPS-eAAMPConfig_IntStartValue].value		=	TRICKPLAY_VOD_PLAYBACK_FPS;
	iAampCfgValue[eAAMPConfig_LinearTrickPlayFPS-eAAMPConfig_IntStartValue].value		=	TRICKPLAY_LINEAR_PLAYBACK_FPS;
	iAampCfgValue[eAAMPConfig_RampDownLimit-eAAMPConfig_IntStartValue].value		=	-1;
	iAampCfgValue[eAAMPConfig_InitFragmentRetryCount-eAAMPConfig_IntStartValue].value	=	DEFAULT_DOWNLOAD_RETRY_COUNT;
	iAampCfgValue[eAAMPConfig_StallErrorCode-eAAMPConfig_IntStartValue].value		=	DEFAULT_STALL_ERROR_CODE;
	iAampCfgValue[eAAMPConfig_StallTimeoutMS-eAAMPConfig_IntStartValue].value		=	DEFAULT_STALL_DETECTION_TIMEOUT;
	iAampCfgValue[eAAMPConfig_PreCachePlaylistTime-eAAMPConfig_IntStartValue].value 	=	0;
	iAampCfgValue[eAAMPConfig_LanguageCodePreference-eAAMPConfig_IntStartValue].value	=	0;
	iAampCfgValue[eAAMPConfig_InitialBuffer-eAAMPConfig_IntStartValue].value               	=       DEFAULT_MINIMUM_INIT_CACHE_SECONDS;
	iAampCfgValue[eAAMPConfig_SourceSetupTimeout-eAAMPConfig_IntStartValue].value           =       DEFAULT_TIMEOUT_FOR_SOURCE_SETUP;
	iAampCfgValue[eAAMPConfig_DRMDecryptThreshold-eAAMPConfig_IntStartValue].value		=	MAX_SEG_DRM_DECRYPT_FAIL_COUNT;
	iAampCfgValue[eAAMPConfig_SegmentInjectThreshold-eAAMPConfig_IntStartValue].value	=	MAX_SEG_INJECT_FAIL_COUNT;
	iAampCfgValue[eAAMPConfig_ABRThresholdSize-eAAMPConfig_IntStartValue].value		=	DEFAULT_AAMP_ABR_THRESHOLD_SIZE;
	iAampCfgValue[eAAMPConfig_PrePlayBufferCount-eAAMPConfig_IntStartValue].value           =       DEFAULT_PREBUFFER_COUNT;
	iAampCfgValue[eAAMPConfig_MinABRNWBufferRampDown-eAAMPConfig_IntStartValue].value       =       AAMP_LOW_BUFFER_BEFORE_RAMPDOWN;
	iAampCfgValue[eAAMPConfig_MaxABRNWBufferRampUp-eAAMPConfig_IntStartValue].value         =       AAMP_HIGH_BUFFER_BEFORE_RAMPUP;
	iAampCfgValue[eAAMPConfig_DownloadDelay-eAAMPConfig_IntStartValue].value         	=       0;
	iAampCfgValue[eAAMPConfig_LivePauseBehavior-eAAMPConfig_IntStartValue].value            =       ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE;

	///////////////// Following for long data types /////////////////////////////
	lAampCfgValue[eAAMPConfig_DiscontinuityTimeout-eAAMPConfig_LongStartValue].value	=	DEFAULT_DISCONTINUITY_TIMEOUT;
	lAampCfgValue[eAAMPConfig_MaxBitrate-eAAMPConfig_LongStartValue].value			= 	LONG_MAX;
	lAampCfgValue[eAAMPConfig_CurlStallTimeout-eAAMPConfig_LongStartValue].value		=	0;
	lAampCfgValue[eAAMPConfig_CurlDownloadStartTimeout-eAAMPConfig_LongStartValue].value	=	0;
        lAampCfgValue[eAAMPConfig_MinBitrate-eAAMPConfig_LongStartValue].value			=       0;
        iAampCfgValue[eAAMPConfig_PTSErrorThreshold-eAAMPConfig_IntStartValue].value            =       MAX_PTS_ERRORS_THRESHOLD;
        iAampCfgValue[eAAMPConfig_Http5XXRetryWaitInterval-eAAMPConfig_IntStartValue].value     =       DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS;
	lAampCfgValue[eAAMPConfig_DefaultBitrate-eAAMPConfig_LongStartValue].value		=       DEFAULT_INIT_BITRATE;
	lAampCfgValue[eAAMPConfig_DefaultBitrate4K-eAAMPConfig_LongStartValue].value		=       DEFAULT_INIT_BITRATE_4K;
	lAampCfgValue[eAAMPConfig_IFrameDefaultBitrate-eAAMPConfig_LongStartValue].value	=       0;
	lAampCfgValue[eAAMPConfig_IFrameDefaultBitrate4K-eAAMPConfig_LongStartValue].value	=       0;

	///////////////// Following for double data types /////////////////////////////
	dAampCfgValue[eAAMPConfig_NetworkTimeout-eAAMPConfig_DoubleStartValue].value      	=       CURL_FRAGMENT_DL_TIMEOUT;
	dAampCfgValue[eAAMPConfig_ManifestTimeout-eAAMPConfig_DoubleStartValue].value     	=       CURL_FRAGMENT_DL_TIMEOUT;
	dAampCfgValue[eAAMPConfig_PlaylistTimeout-eAAMPConfig_DoubleStartValue].value     	=       0;
	dAampCfgValue[eAAMPConfig_LiveOffset-eAAMPConfig_DoubleStartValue].value		=	AAMP_LIVE_OFFSET;
	dAampCfgValue[eAAMPConfig_CDVRLiveOffset-eAAMPConfig_DoubleStartValue].value		=	AAMP_CDVR_LIVE_OFFSET;

	///////////////// Following for String type config ////////////////////////////
	sAampCfgValue[eAAMPConfig_MapMPD-eAAMPConfig_StringStartValue].value			=	"";
	sAampCfgValue[eAAMPConfig_MapM3U8-eAAMPConfig_StringStartValue].value			=	"";
	sAampCfgValue[eAAMPConfig_HarvestPath-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_LicenseServerUrl-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_CKLicenseServerUrl-eAAMPConfig_StringStartValue].value	=	"";
	sAampCfgValue[eAAMPConfig_PRLicenseServerUrl-eAAMPConfig_StringStartValue].value	=	"";
	//sAampCfgValue[eAAMPConfig_RedirectUrl-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_WVLicenseServerUrl-eAAMPConfig_StringStartValue].value	=	"";
	sAampCfgValue[eAAMPConfig_UserAgent-eAAMPConfig_StringStartValue].value			=	"";
	sAampCfgValue[eAAMPConfig_SubTitleLanguage-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_CustomHeader-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_URIParameter-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_NetworkProxy-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_LicenseProxy-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_SessionToken-eAAMPConfig_StringStartValue].value		=	"";
	sAampCfgValue[eAAMPConfig_LogLevel-eAAMPConfig_StringStartValue].value                  =       "";

}

#if 0
LangCodePreference AampConfig::GetLanguageCodePreference()
{
	return (LangCodePreference)iAampCfgValue[eAAMPConfig_LanguageCodePreference-eAAMPConfig_IntStartValue].value;
}
#endif
std::string AampConfig::GetUserAgentString()
{
	return std::string(sAampCfgValue[eAAMPConfig_UserAgent-eAAMPConfig_StringStartValue].value + " " + AAMP_USERAGENT_SUFFIX);
}

void AampConfig::SetCfgDrive(char drivename)
{
#ifdef WIN32
	cWindowsCfg.replace(0,1,1,drivename);
	cWindowsJsonCfg.replace(0,1,1,drivename);
#endif
}

/**
 * @brief IsConfigSet - Gets the boolean configuration value
 *
 * @param[in] cfg - Configuration enum
 * @return true / false
 */
bool AampConfig::IsConfigSet(AAMPConfigSettings cfg)
{
	bool ret= false;
	if(cfg < eAAMPConfig_BoolMaxValue)
	{
		ret = bAampCfgValue[cfg].value;
	}
	return ret;
}

/**
 * @brief GetConfigValue - Gets configuration for integer data type
 *
 * @param[in] cfg - configuration enum
 * @param[out] value - configuration value
 * @return true - if valid return
 */
bool AampConfig::GetConfigValue(AAMPConfigSettings cfg , int &value)
{
	bool ret= false;
	if(cfg > eAAMPConfig_IntStartValue && cfg < eAAMPConfig_IntMaxValue)
	{
		value = iAampCfgValue[cfg-eAAMPConfig_IntStartValue].value;
		ret = true;
	}
	return ret;
}

/**
 * @brief GetConfigValue - Gets configuration for long data type
 *
 * @param[in] cfg - configuration enum
 * @param[out] value - configuration value
 * @return true - if valid return
 */
bool AampConfig::GetConfigValue(AAMPConfigSettings cfg, long &value)
{
	bool ret= false;
	if(cfg > eAAMPConfig_LongStartValue && cfg < eAAMPConfig_LongMaxValue)
	{
		value = lAampCfgValue[cfg-eAAMPConfig_LongStartValue].value;
		ret=true;
	}
	return ret;
}

/**
 * @brief GetConfigValue - Gets configuration for double data type
 *
 * @param[in] cfg - configuration enum
 * @param[out] value - configuration value
 * @return true - if valid return
 */
bool AampConfig::GetConfigValue(AAMPConfigSettings cfg, double &value)
{
	bool ret= false;
	if(cfg > eAAMPConfig_DoubleStartValue && cfg < eAAMPConfig_DoubleMaxValue)
	{
		value = dAampCfgValue[cfg-eAAMPConfig_DoubleStartValue].value;
		ret=true;
	}
	return ret;
}



/**
 * @brief GetConfigValue - Gets configuration for string data type
 *
 * @param[in] cfg - configuration enum
 * @param[out] value - configuration value
 * @return true - if valid return
 */
bool AampConfig::GetConfigValue(AAMPConfigSettings cfg, std::string &value)
{
	bool ret=false;
	if(cfg > eAAMPConfig_StringStartValue && cfg < eAAMPConfig_StringMaxValue)
	{
		value = sAampCfgValue[cfg-eAAMPConfig_StringStartValue].value;
		ret = true;
	}
	return ret;
}
/**
 * @brief GetChannelOverride - Gets channel override url for channel Name
 *
 * @param[in] chName - channel name to search
 * @param[out] chOverride - URI
 * @return true - if valid return
 */
const char * AampConfig::GetChannelOverride(const std::string manifestUrl)
{
	if(mChannelOverrideMap.size() && manifestUrl.size())
	{
		for (ChannelMapIter it = mChannelOverrideMap.begin(); it != mChannelOverrideMap.end(); ++it)
		{
			ConfigChannelInfo &pChannelInfo = *it;
			if (manifestUrl.find(pChannelInfo.name) != std::string::npos)
			{
				return pChannelInfo.uri.c_str();
			}
		}
	}
	return NULL;
}

/**
 * @brief ToggleConfigValue - Toggle Boolean configuration
 *
 * @param[in] owner  - ownership of new set call
 * @param[in] cfg	- Configuration enum to set
 * @return None
 */
void AampConfig::ToggleConfigValue(ConfigPriority owner, AAMPConfigSettings cfg )
{
	if(cfg < eAAMPConfig_BoolMaxValue)
	{

		bool value = !bAampCfgValue[cfg].value;
		SetValue<ConfigBool , bool> (bAampCfgValue[cfg], owner,value,GetConfigName(cfg));
	}
	else
	{
		logprintf("%s:%d Index beyond range : %d Max:%d \n",__FUNCTION__,__LINE__,cfg,eAAMPConfig_BoolMaxValue);
	}
}


/**
 * @brief SetConfigValue - Set function to set bool/int/long data type configuration
 *
 * @param[in] owner  - ownership of new set call
 * @param[in] cfg	- Configuration enum to set
 * @param[in] value   - value to set
 * @return None
 */
template<typename T>
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettings cfg ,const T &value)
{
	if(cfg < eAAMPConfig_BoolMaxValue)
	{
		SetValue<ConfigBool , bool> (bAampCfgValue[cfg], owner,(bool)value,GetConfigName(cfg));
	}
	else if(cfg > eAAMPConfig_IntStartValue && cfg < eAAMPConfig_IntMaxValue)
	{
		int valueInt = (int)value;
		if(ValidateRange(GetConfigName(cfg),valueInt))
		{
			SetValue<ConfigInt , int>(iAampCfgValue[cfg-eAAMPConfig_IntStartValue], owner,(int)value,GetConfigName(cfg));
		}
	}
	else if(cfg > eAAMPConfig_LongStartValue && cfg < eAAMPConfig_LongMaxValue)
	{
		long valueLong = (long)value;
		if(ValidateRange(GetConfigName(cfg),valueLong))
		{
			SetValue<ConfigLong , long>(lAampCfgValue[cfg-eAAMPConfig_LongStartValue], owner,(long)value,GetConfigName(cfg));
		}
	}
	else if(cfg > eAAMPConfig_DoubleStartValue && cfg < eAAMPConfig_DoubleMaxValue)
	{
		double valueDouble = (double)value;
		if(ValidateRange(GetConfigName(cfg),valueDouble))
		{
			SetValue<ConfigDouble , double>(dAampCfgValue[cfg-eAAMPConfig_DoubleStartValue], owner,(double)value,GetConfigName(cfg));
		}
	}
	else
	{
		logprintf("%s:%d Cfg Index Not in range : %d \n",__FUNCTION__,__LINE__,cfg);
	}
}

/**
 * @brief SetConfigValue - Set function to set string data type configuration
 *
 * @param[in] owner  - ownership of new set call
 * @param[in] cfg	- Configuration enum to set
 * @param[in] value   - string value to set
 * @return None
 */
template <>
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettings cfg , const std::string &value)
{
	if(cfg > eAAMPConfig_StringStartValue && cfg < eAAMPConfig_StringMaxValue)
	{
		SetValue<ConfigString , std::string>(sAampCfgValue[cfg-eAAMPConfig_StringStartValue], owner,value,GetConfigName(cfg));
	}
	else
	{
		logprintf("%s:%d Index Not in range : %d [%d-%d] \n",__FUNCTION__,__LINE__,cfg,eAAMPConfig_StringStartValue,eAAMPConfig_StringMaxValue);
	}
}

/**
 * @brief ProcessConfigJson - Function to parse and process json configuration string
 *
 * @param[in] cfg - json string
 * @param[in] owner   - Owner who is setting the value
 * @return bool - true on success
 */
bool AampConfig::ProcessConfigJson(const char *jsonbuffer, ConfigPriority owner )
{
	bool retval = false;

	if(jsonbuffer)
	{
		cJSON *cfgdata = cJSON_Parse(jsonbuffer);
		if(cfgdata != NULL)
		{
			std::string keyname;
			for (auto it = mAampLookupTable.begin(); it != mAampLookupTable.end(); ++it)
			{

				AampConfigLookupEntry cfg = it->second;
				AAMPConfigSettings cfgEnum = cfg.cfgEntryValue;
				keyname =  it->first;
				cJSON *searchObj = cJSON_GetObjectItem(cfgdata,keyname.c_str());
				if(searchObj)
				{
					// Found that keyname in json string
					if(cfgEnum < eAAMPConfig_BoolMaxValue )
					{
						if(cJSON_IsTrue(searchObj))
						{
							SetConfigValue<bool>(owner,cfgEnum,true);
						}
						else
						{
							SetConfigValue<bool>(owner,cfgEnum,false);
						}
					}
					else if(cfgEnum > eAAMPConfig_IntStartValue && cfgEnum < eAAMPConfig_IntMaxValue && cJSON_IsNumber(searchObj))
					{
						// For those parameters in Integer Settings
						int conv = (int)searchObj->valueint;
						if(ValidateRange(keyname,conv))
						{
							SetConfigValue<int>(owner,cfgEnum,conv);
						}
						else
						{
							logprintf("%s:%d Set failed .Input beyond the configured range",__FUNCTION__,__LINE__);
						}
					}
					else if(cfgEnum > eAAMPConfig_LongStartValue && cfgEnum < eAAMPConfig_LongMaxValue && cJSON_IsNumber(searchObj))
					{
						// For those parameters in long Settings
						long conv = (long)searchObj->valueint;
						if(ValidateRange(keyname,conv))
						{
							SetConfigValue<long>(owner,cfgEnum,conv);
						}
						else
						{
							logprintf("%s:%d Set failed .Input beyond the configured range",__FUNCTION__,__LINE__);
						}
					}
					else if(cfgEnum > eAAMPConfig_DoubleStartValue && cfgEnum < eAAMPConfig_DoubleMaxValue && cJSON_IsNumber(searchObj))
					{
						// For those parameters in double settings
						double conv= (double)searchObj->valuedouble;
						if(ValidateRange(keyname,conv))
						{
							SetConfigValue<double>(owner,cfgEnum,conv);
						}
						else
						{
							logprintf("%s:%d Set failed .Input beyond the configured range",__FUNCTION__,__LINE__);
						}
					}
					else if(cfgEnum > eAAMPConfig_StringStartValue && cfgEnum < eAAMPConfig_StringMaxValue && cJSON_IsString(searchObj))
					{
						// For those parameters in string Settings
						SetConfigValue<std::string>(owner,cfgEnum,std::string(searchObj->valuestring));
					}
				}
			}
			// checked all the config string in json
			// next check is channel override array is present
			cJSON *chMap = cJSON_GetObjectItem(cfgdata,"chmap");
			if(chMap)
			{
				if(cJSON_IsArray(chMap))
				{
					for (int i = 0 ; i < cJSON_GetArraySize(chMap) ; i++)
					{
						cJSON * subitem = cJSON_GetArrayItem(chMap, i);
						char *name      = (char *)cJSON_GetObjectItem(subitem, "name")->valuestring;
						char *url       = (char *)cJSON_GetObjectItem(subitem, "url")->valuestring;
						if(name && url )
						{
							ConfigChannelInfo channelInfo;
							channelInfo.uri = url;
							channelInfo.name = name;
							mChannelOverrideMap.push_back(channelInfo);
						}
					}
				}
				else
				{
					logprintf("%s %d JSON Channel Override format is wrong", __FUNCTION__,__LINE__);
				}
			}
#if 0

			cJSON *drmConfig = cJSON_GetObjectItem(cfgdata,"drmConfig");
			if(drmConfig)
			{
				cJSON *subitem = drmConfig->child;
				DRMSystems drmType;
				while( subitem )
				{
					if(strcasecmp("com.microsoft.playready",subitem->string)==0)
					{
						SetConfigValue<std::string>(owner,eAAMPConfig_PRLicenseServerUrl,(std::string)subitem->valuestring);
						drmType = eDRM_PlayReady;
					}
					if(strcasecmp("com.widevine.alpha",subitem->string)==0)
					{
						SetConfigValue<std::string>(owner,eAAMPConfig_WVLicenseServerUrl,(std::string)subitem->valuestring);
						drmType = eDRM_WideVine;
					}
					if(strcasecmp("org.w3.clearkey",subitem->string)==0)
					{
						SetConfigValue<std::string>(owner,eAAMPConfig_CKLicenseServerUrl,(std::string)subitem->valuestring);
						drmType = eDRM_ClearKey;
					}
					if(strcasecmp("preferredKeysystem",subitem->string)==0)
					{
						SetConfigValue<int>(owner,eAAMPConfig_PreferredDRM,(int)drmType);
					}
					subitem = subitem->next;
				}
			}
#endif
			retval = true;
			cJSON_Delete(cfgdata);
		}
	}
	return retval;
}

/**
 * @brief GetAampConfigJSONStr - Function to Complete Config as JSON str
 *
 * @param[in] str  - input string where config json will be stored
 * @return true
 */
bool AampConfig::GetAampConfigJSONStr(std::string &str)
{
	AampJsonObject jsondata;

	// All Bool values
	for(int i=0;i<eAAMPConfig_BoolMaxValue;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettings)i),bAampCfgValue[i].value);
	}

	// All integer values
	for(int i=eAAMPConfig_IntStartValue+1;i<eAAMPConfig_IntMaxValue;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettings)i),iAampCfgValue[i-eAAMPConfig_IntStartValue].value);
	}

	// All long values
	for(int i=eAAMPConfig_LongStartValue+1;i<eAAMPConfig_LongMaxValue;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettings)i),lAampCfgValue[i-eAAMPConfig_LongStartValue].value);
	}

	// All double values
	for(int i=eAAMPConfig_DoubleStartValue+1;i<eAAMPConfig_DoubleMaxValue;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettings)i),dAampCfgValue[i-eAAMPConfig_DoubleStartValue].value);
	}

	// All String values
	for(int i=eAAMPConfig_StringStartValue+1;i<eAAMPConfig_StringMaxValue;i++)
	{
		jsondata.add(GetConfigName((AAMPConfigSettings)i),sAampCfgValue[i-eAAMPConfig_StringStartValue].value);
	}

	str = jsondata.print_UnFormatted();
	return true;
}

/**
 * @brief ProcessConfigText - Function to parse and process configuration text
 *
 * @param[in] cfg - config text ( new line separated)
 * @param[in] owner   - Owner who is setting the value
 * @return None
 */
bool AampConfig::GetDeveloperConfigData(std::string &key,std::string &value)
{
	bool retval = false;
	DevCmdsIter iter = mAampDevCmdTable.find(key);
	if(iter != mAampDevCmdTable.end())
	{
		value = iter->second;
		retval = true;
	}
	return retval;
}



/**
 * @brief ProcessConfigText - Function to parse and process configuration text
 *
 * @param[in] cfg - config text ( new line separated)
 * @param[in] owner   - Owner who is setting the value
 * @return None
 */
bool AampConfig::ProcessConfigText(std::string &cfg, ConfigPriority owner )
{
	bool retval=true;
	do{
	if (!cfg.empty() && cfg.at(0) != '#')
	{ // ignore comments

		if(cfg[0] == '#')
			break;
		if(cfg[0] == '*')
		{
			// Add to channel map
			std::size_t pos = cfg.find_first_of(' ');
			if (pos != std::string::npos)
			{
				//Populate channel map from aamp.cfg
				// new wildcard matching for overrides - allows *HBO to remap any url including "HBO"
				ConfigChannelInfo channelInfo;
				std::stringstream iss(cfg.substr(1));
				std::string token;
				while (getline(iss, token, ' '))
				{
					if (token.compare(0,4,"http") == 0)
						channelInfo.uri = token;
					else if ((token.compare(0,5,"live:") == 0) || (token.compare(0,3,"mr:") == 0) || (token.compare(0,5,"tune:") == 0))
					{
						logprintf("%s %d Overriden OTA Url!!", __FUNCTION__,__LINE__);
						channelInfo.uri = token;
					}
					else
						channelInfo.name = token;
				}
				mChannelOverrideMap.push_back(channelInfo);
			}

		}
		else
		{

			//Removing unnecessary spaces and newlines
			cfg.erase(std::remove_if(cfg.begin(), cfg.end(), ::isspace),
									 cfg.end());
			// Process commands
			bool toggle = false;
			std::string key,value;
			std::size_t delimiterPos = cfg.find("=");
			if(delimiterPos != std::string::npos)
			{
				key = cfg.substr(0, delimiterPos);
				value = cfg.substr(delimiterPos + 1);
				//logprintf("%s:INFO Key:%s Value:%s\n",__FUNCTION__,key.c_str(),value.c_str());
			}
			else
			{
				key = cfg.substr(0);
				toggle = true;
			}

			LookUpIter iter = mAampLookupTable.find(key);
			if(iter != mAampLookupTable.end())
			{
				AampConfigLookupEntry cfg = iter->second;
				AAMPConfigSettings cfgEnum = cfg.cfgEntryValue;
				//logprintf("Config First:%s Value:%s Second : %d\n",iter->first.c_str(),value.c_str(),cfgEnum);
				// For those parameters in Boolean Settings
				if(cfgEnum < eAAMPConfig_BoolMaxValue )
				{
					bool param=false;
					int conv = 0;
					if(!toggle)
					{
						if(isdigit(value[0]))
						{ // for backward compatability 0/1
							if(ReadNumericHelper(value,conv))
								SetConfigValue<bool>(owner,cfgEnum,(bool)(conv != 0));
						}
						else
						{
							// look for true or false
							if(strcasecmp(value.c_str(),"true")==0)
								SetConfigValue<bool>(owner,cfgEnum,(bool)true);
							else if(strcasecmp(value.c_str(),"false")==0)
								SetConfigValue<bool>(owner,cfgEnum,(bool)false);
							else
								logprintf("%s Wrong input provided for Cfg:%s Value:%s",__FUNCTION__,iter->first.c_str(),value.c_str());
						}
					}
					else
					{
						// for toggle
						ToggleConfigValue(owner,cfgEnum);
					}
				}
				else if(cfgEnum > eAAMPConfig_IntStartValue && cfgEnum < eAAMPConfig_IntMaxValue)
				{
					// For those parameters in Integer Settings
					int conv = 0;
					if(isdigit(value[0]) && ReadNumericHelper(value,conv))
					{
						if(ValidateRange(key,conv))
						{
							SetConfigValue<int>(owner,cfgEnum,(int)conv);
						}
						else
						{
							logprintf("%s:%d Set failed .Input beyond the configured range",__FUNCTION__,__LINE__);
						}
					}
				}
				else if(cfgEnum > eAAMPConfig_LongStartValue && cfgEnum < eAAMPConfig_LongMaxValue)
				{
					// For those parameters in long Settings
					long conv = 0;
					if(isdigit(value[0]) && ReadNumericHelper(value,conv))
					{
						if(ValidateRange(key,conv))
						{
							SetConfigValue<long>(owner,cfgEnum,(long)conv);
						}
						else
						{
							logprintf("%s:%d Set failed .Input beyond the configured range",__FUNCTION__,__LINE__);
						}
					}
				}
				else if(cfgEnum > eAAMPConfig_DoubleStartValue && cfgEnum < eAAMPConfig_DoubleMaxValue)
				{
					// For those parameters in double settings
					double conv=0.0;
					if(isdigit(value[0]) && ReadNumericHelper(value,conv))
					{
						if(ValidateRange(key,conv))
						{
							SetConfigValue<double>(owner,cfgEnum,(double)conv);
						}
						else
						{
							logprintf("%s:%d Set failed .Input beyond the configured range",__FUNCTION__,__LINE__);
						}
					}
				}
				else if(cfgEnum > eAAMPConfig_StringStartValue && cfgEnum < eAAMPConfig_StringMaxValue)
				{
					// For those parameters in string Settings
					if(value.size())
						SetConfigValue<std::string>(owner,cfgEnum,value);
				}

			}
			else
			{
				mAampDevCmdTable[key]=value;
				logprintf("%s:%d Unknown command(%s) added to DeveloperTable",__FUNCTION__,__LINE__,key.c_str());
			}
		}
	}
	}while(0);
	return retval;
}

/**
 * @brief ReadAampCfgJsonFile - Function to parse and process configuration file in json format
 *
 * @return None
 */
bool AampConfig::ReadAampCfgJsonFile()
{
	bool retVal=false;
#if defined(__APPLE__)
	std::string cfgPath(getenv("HOME"));
#else
	std::string cfgPath = "";
#endif

#ifdef WIN32
		cfgPath.assign(cWindowsJsonCfg);
#elif defined(__APPLE__)
		cfgPath += "/aampcfg.json";
#else

#ifdef AAMP_CPC // Comcast builds
		// AAMP_ENABLE_OPT_OVERRIDE is only added for PROD builds.
		const char *env_aamp_enable_opt = getenv("AAMP_ENABLE_OPT_OVERRIDE");
#else
		const char *env_aamp_enable_opt = "true";
#endif

		if(env_aamp_enable_opt)
		{
			cfgPath = AAMP_JSON_PATH;
		}
#endif

		if (!cfgPath.empty())
		{
			std::ifstream f(cfgPath, std::ifstream::in | std::ifstream::binary);
			if (f.good())
			{
				logprintf("%s:INFO opened aampcfg.json\n",__FUNCTION__);
				std::filebuf* pbuf = f.rdbuf();
				std::size_t size = pbuf->pubseekoff (0,f.end,f.in);
				pbuf->pubseekpos (0,f.in);
				char* jsonbuffer=new char[size];
				pbuf->sgetn (jsonbuffer,size);
				f.close();
				ProcessConfigJson( jsonbuffer, AAMP_DEV_CFG_SETTING);
				delete[] jsonbuffer;
				DoCustomSetting();
				retVal = true;
			}
		}
	return retVal;
}


/**
 * @brief ReadAampCfgTxtFile - Function to parse and process configuration file in text format
 *
 * @return None
 */
bool AampConfig::ReadAampCfgTxtFile()
{
	bool retVal = false;
#if defined(__APPLE__)
	std::string cfgPath(getenv("HOME"));
#else
	std::string cfgPath = "";
#endif

#ifdef WIN32
	cfgPath.assign(cWindowsCfg);
#elif defined(__APPLE__)
	cfgPath += "/aamp.cfg";
#else

#ifdef AAMP_CPC // Comcast builds
	// AAMP_ENABLE_OPT_OVERRIDE is only added for PROD builds.
	const char *env_aamp_enable_opt = getenv("AAMP_ENABLE_OPT_OVERRIDE");
#else
	const char *env_aamp_enable_opt = "true";
#endif

	if(env_aamp_enable_opt)
	{
		cfgPath = AAMP_CFG_PATH;
	}
#endif

	if (!cfgPath.empty())
	{
		std::ifstream f(cfgPath, std::ifstream::in | std::ifstream::binary);
		if (f.good())
		{
			logprintf("%s:INFO opened aamp.cfg\n",__FUNCTION__);
			std::string buf;
			while (f.good())
			{
				std::getline(f, buf);
				ProcessConfigText(buf, AAMP_DEV_CFG_SETTING);
			}
			f.close();
			DoCustomSetting();
			retVal = true;
		}
	}
	return retVal;
}

/**
 * @brief ReadOperatorConfiguration - Reads Operator configuration from RFC and env variables
 *
 * @return None
 */

void AampConfig::ReadOperatorConfiguration()
{
#ifdef IARM_MGR
	size_t iConfigLen = 0;
	char *	cloudConf = GetTR181AAMPConfig("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.b64Config", iConfigLen);
	if(cloudConf && (iConfigLen > 0))
	{
		bool bCharCompliant = true;
		for (int i = 0; i < iConfigLen; i++)
		{
			if (!( cloudConf[i] == 0xD || cloudConf[i] == 0xA) && // ignore LF and CR chars
				((cloudConf[i] < 0x20) || (cloudConf[i] > 0x7E)))
			{
				bCharCompliant = false;
				logprintf("%s ERROR Non Compliant char[0x%X] found, Ignoring whole config  \n",__FUNCTION__,cloudConf[i]);
				break;
			}
		}

		if (bCharCompliant)
		{
			std::string strCfg(cloudConf,iConfigLen);
			if(!ProcessConfigJson(strCfg.c_str(),AAMP_OPERATOR_SETTING))
			{
				// Input received is not json format, parse as text
				std::istringstream iSteam(strCfg);
				std::string line;
				while (std::getline(iSteam, line))
				{
					if (line.length() > 0)
					{
						logprintf("%s INFO aamp-cmd:[%s]\n",__FUNCTION__, line.c_str());
						ProcessConfigText(line,AAMP_OPERATOR_SETTING);
					}
				}
			}
		}
		free(cloudConf); // allocated by base64_Decode in GetTR181AAMPConfig
	}
#endif

	///////////// Read environment variables set specific to Operator ///////////////////
	const char *env_aamp_force_aac = getenv("AAMP_FORCE_AAC");
	if(env_aamp_force_aac)
	{
		logprintf("AAMP_FORCE_AAC present: Changing preference to AAC over ATMOS & DD+");
		SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_DisableEC3,true);
		SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_DisableATMOS,true);
		SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_ForceEC3,false);
		SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_StereoOnly,true);
	}

	const char *env_aamp_min_init_cache = getenv("AAMP_MIN_INIT_CACHE");
	if(env_aamp_min_init_cache)
	{
		int minInitCache = 0;
		if(sscanf(env_aamp_min_init_cache,"%d",&minInitCache) && minInitCache >= 0)
		{
			logprintf("AAMP_MIN_INIT_CACHE present: Changing min initial cache to %d seconds",minInitCache);
			SetConfigValue<int>(AAMP_OPERATOR_SETTING,eAAMPConfig_InitialBuffer,minInitCache);
		}
	}


	const char *env_enable_micro_events = getenv("TUNE_MICRO_EVENTS");
	if(env_enable_micro_events)
	{
		logprintf("TUNE_MICRO_EVENTS present: Enabling TUNE_MICRO_EVENTS.");
		SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_EnableMicroEvents,true);
	}

	const char *env_enable_cdai = getenv("CLIENT_SIDE_DAI");
	if(env_enable_cdai)
	{
		logprintf("CLIENT_SIDE_DAI present: Enabling CLIENT_SIDE_DAI.");
		SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_EnableClientDai,true);
	}

	const char *env_enable_westoros_sink = getenv("AAMP_ENABLE_WESTEROS_SINK");
	if(env_enable_westoros_sink)
	{

		int iValue = atoi(env_enable_westoros_sink);
		bool bValue = (strcasecmp(env_enable_westoros_sink,"true") == 0);

		logprintf("AAMP_ENABLE_WESTEROS_SINK present, Value = %d", (bValue ? bValue : (iValue ? iValue : 0)));

		if(iValue || bValue)
		{
			logprintf("AAMP_ENABLE_WESTEROS_SINK present: Enabling westeros-sink.");
			SetConfigValue<bool>(AAMP_OPERATOR_SETTING,eAAMPConfig_UseWesterosSink,true);
		}

	}

}


/**
 * @brief ConfigureLogSettings - This function configures log settings for LogManager instance
 *
 * @return None
 */
void AampConfig::ConfigureLogSettings()
{
	std::string logString;
	logString = sAampCfgValue[eAAMPConfig_LogLevel-eAAMPConfig_StringStartValue].value;

	if(bAampCfgValue[eAAMPConfig_TraceLogging].value || logString.compare("trace") == 0)
	{
		// backward compatability
		logging.setLogLevel(eLOGLEVEL_TRACE);
		logging.trace = true;
	}
	else if(bAampCfgValue[eAAMPConfig_DebugLogging].value || logString.compare("debug") == 0)
	{
		// backward compatability . Trace ande debug does same job.
		logging.info = false;
		logging.setLogLevel(eLOGLEVEL_TRACE);
		logging.debug = true;
	}
	else if((bAampCfgValue[eAAMPConfig_InfoLogging].value || logString.compare("info") == 0))
	{
		// backward compatability
		logging.setLogLevel(eLOGLEVEL_INFO);
		logging.info = true;
	}
	else if(logString.compare("warn") == 0)
	{
		logging.setLogLevel(eLOGLEVEL_WARN);
	}
	else if(logString.compare("error") == 0)
	{
		logging.setLogLevel(eLOGLEVEL_ERROR);
	}


	// This is pending to handle the ownership rights , whether App can set following config
	logging.failover			=	bAampCfgValue[eAAMPConfig_FailoverLogging].value;
	logging.gst				=	bAampCfgValue[eAAMPConfig_GSTLogging].value;
	logging.progress			=	bAampCfgValue[eAAMPConfig_ProgressLogging].value;
	logging.curl				=	bAampCfgValue[eAAMPConfig_CurlLogging].value;
	logging.stream				=	bAampCfgValue[eAAMPConfig_StreamLogging].value;
	logging.curlHeader			= 	bAampCfgValue[eAAMPConfig_CurlHeader].value;
	logging.curlLicense			=	bAampCfgValue[eAAMPConfig_CurlLicenseLogging].value;
	logging.logMetadata			=	bAampCfgValue[eAAMPConfig_MetadataLogging].value;
	logging.id3    				= 	bAampCfgValue[eAAMPConfig_ID3Logging].value;

}

/**
 * @brief ShowOperatorSetConfiguration - List all operator configured settings
 *
 * @return None
 */
void AampConfig::ShowOperatorSetConfiguration()
{
	logprintf("////////////////// AAMP Config (Operator Set) //////////");
	ShowConfiguration(AAMP_OPERATOR_SETTING);
	logprintf("//////////////////////////////////////////////////");
}
/**
 * @brief ShowAppSetConfiguration - List all Application configured settings
 *
 * @return None
 */
void AampConfig::ShowAppSetConfiguration()
{
	logprintf("////////////////// AAMP Config (Application Set) //////////");
	ShowConfiguration(AAMP_APPLICATION_SETTING);
	logprintf("//////////////////////////////////////////////////");
}
/**
 * @brief ShowStreamSetConfiguration - List all stream configured settings
 *
 * @return None
 */
void AampConfig::ShowStreamSetConfiguration()
{
	logprintf("////////////////// AAMP Config (Stream Set) //////////");
	ShowConfiguration(AAMP_STREAM_SETTING);
	logprintf("//////////////////////////////////////////////////");

}
/**
 * @brief ShowDefaultAampConfiguration - List all AAMP Default settings
 *
 * @return None
 */
void AampConfig::ShowDefaultAampConfiguration()
{
	logprintf("////////////////// AAMP Default Configuration  //////////");
	ShowConfiguration(AAMP_DEFAULT_SETTING);
	logprintf("//////////////////////////////////////////////////");
}
/**
 * @brief ShowDevCfgConfiguration - List all developer configured settings
 *
 * @return None
 */
void AampConfig::ShowDevCfgConfiguration()
{
	logprintf("////////////////// AAMP Cfg Override Configuration  //////////");
	ShowConfiguration(AAMP_DEV_CFG_SETTING);
	logprintf("//////////////////////////////////////////////////");
}
/**
 * @brief ShowAAMPConfiguration - Show all settings for every owner
 *
 * @return None
 */
void AampConfig::ShowAAMPConfiguration()
{
	logprintf("////////////////// AAMP Configuration  //////////");
	ShowConfiguration(AAMP_MAX_SETTING);
	logprintf("//////////////////////////////////////////////////");
}

//////////////// Special Functions which involve conversion of configuration ///////////
/////////Only add new functions if required only , else use default Get call and covert
/////////at usage
#if 0
/**
 * @brief GetPreferredDRM - Get Preferred DRM type
 *
 * @return DRMSystems - type of drm
 */
DRMSystems AampConfig::GetPreferredDRM()
{
	DRMSystems prefSys;
	std::string keySystem = sAampCfgValue[eAAMPConfig_PreferredDRM-eAAMPConfig_StringStartValue].value;
	std::transform(keySystem.begin(), keySystem.end(), keySystem.begin(), ::tolower);
	if(keySystem.find("widevine") != std::string::npos)
		prefSys = eDRM_WideVine;
	else if(keySystem.find("clearkey") != std::string::npos)
		prefSys = eDRM_ClearKey;
	else
		prefSys = eDRM_PlayReady;       // default is playready
	return prefSys;
}
#endif

///////////////////////////////// Private Functions ///////////////////////////////////////////

/**
 * @brief DoCustomSetting - Function to do override , to avoid complexity with multiple configs
 *
 * @return None
 */
void AampConfig::DoCustomSetting()
{
	if(IsConfigSet(eAAMPConfig_StereoOnly))
	{
		// If Stereo Only flag is set , it will override all other sub setting with audio
		SetConfigValue<bool>(AAMP_DEV_CFG_SETTING,eAAMPConfig_DisableEC3,true);
		SetConfigValue<bool>(AAMP_DEV_CFG_SETTING,eAAMPConfig_DisableATMOS,true);
		SetConfigValue<bool>(AAMP_DEV_CFG_SETTING,eAAMPConfig_ForceEC3,false);
	}
	else if(IsConfigSet(eAAMPConfig_DisableEC3))
	{
		// if EC3 is disabled , no need to enable forceEC3
		SetConfigValue<bool>(AAMP_DEV_CFG_SETTING,eAAMPConfig_ForceEC3,false);
	}
	ConfigureLogSettings();
}


/**
 * @brief SetValue - Function to store the configuration and ownership based on priority set
 *
 * @param[in] setting - Config variable to set
 * @param[in] newowner - New owner value
 * @param[in] value - Value to set
 * @return None
 */
template<class J,class K>
void AampConfig::SetValue(J &setting, ConfigPriority newowner, const K &value, std::string cfgName)
{
	if(setting.owner <= newowner )
	{
		// n number of times same configuration can be overwritten.
		// to store to last value ,owner has to be different
		if(setting.lastowner != newowner)
		{
			setting.lastvalue = setting.value;
			setting.lastowner = setting.owner;
		}
		setting.value = value;
		setting.owner = newowner;
		logprintf("%s: %s New Owner[%d]",__FUNCTION__,cfgName.c_str(),newowner);
	}
	else
	{
		logprintf("%s: %s Owner[%d] not allowed to Set ,current Owner[%d]",__FUNCTION__,cfgName.c_str(),newowner,setting.owner);
	}
}

/**
 * @brief GetConfigName - Function to get configuration name for enum from lookup table
 *
 * @param[in] cfg  - configuration enum
 * @return string - configuration name
 */
std::string AampConfig::GetConfigName(AAMPConfigSettings cfg )
{
	std::string keyname;
	for (auto it = mAampLookupTable.begin(); it != mAampLookupTable.end(); ++it)
	{
		AampConfigLookupEntry cfgitem = it->second;
		AAMPConfigSettings cfgEnum = cfgitem.cfgEntryValue;
		if (cfgEnum == cfg)
		{
			keyname =  it->first;
			break;
		}
	}
	return keyname;
}

template<typename T>
bool AampConfig::ValidateRange(std::string key, T& value)
{
	bool retval = true;

	LookUpIter iter = mAampLookupTable.find(key);
	if(iter != mAampLookupTable.end())
	{
		// check the type and find the min and max values
		AampConfigLookupEntry item = iter->second;
		if (std::is_same<T, int>::value)
		{
			if(item.Min.iMinValue != -1 && value < item.Min.iMinValue)
				retval = false;
			if(item.Max.iMaxValue != -1 && value > item.Max.iMaxValue)
				retval = false;
		}
		else if (std::is_same<T, long>::value)
		{
			if(item.Min.lMinValue != -1 && value < item.Min.lMinValue)
				retval = false;
			if(item.Max.lMaxValue != -1 && value > item.Max.lMaxValue)
				retval = false;
		}
		else if (std::is_same<T, double>::value)
		{
			if(item.Min.dMinValue != -1 && value < item.Min.dMinValue)
				retval = false;
			if(item.Max.dMaxValue != -1 && value > item.Max.dMaxValue)
				retval = false;
		}
	}
	else
	{
		retval = false;
	}
	return retval;
}

/**
 * @brief RestoreConfiguration - Function is restore last configuration value from current ownership
 *
 * @param[in] owner - Owner value for reverting
 * @return None
 */
void AampConfig::RestoreConfiguration(ConfigPriority owner)
{
	// All Bool values
	for(int i=0;i<eAAMPConfig_BoolMaxValue;i++)
	{
		if(bAampCfgValue[i].owner == owner && bAampCfgValue[i].owner != bAampCfgValue[i].lastowner)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s]->[%-5s][%s]->[%s]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[bAampCfgValue[i].owner].ownerName,
				OwnerLookUpTable[bAampCfgValue[i].lastowner].ownerName,bAampCfgValue[i].value?"true":"false",bAampCfgValue[i].lastvalue?"true":"false");
			bAampCfgValue[i].owner = bAampCfgValue[i].lastowner;
			bAampCfgValue[i].value = bAampCfgValue[i].lastvalue;

		}
	}

	// All integer values
	for(int i=eAAMPConfig_IntStartValue+1;i<eAAMPConfig_IntMaxValue;i++)
	{
		// for int array
		if(iAampCfgValue[i-eAAMPConfig_IntStartValue].owner == owner && iAampCfgValue[i-eAAMPConfig_IntStartValue].owner != iAampCfgValue[i-eAAMPConfig_IntStartValue].lastowner)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s]->[%-5s][%d]->[%d]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[iAampCfgValue[i-eAAMPConfig_IntStartValue].owner].ownerName,
				OwnerLookUpTable[iAampCfgValue[i-eAAMPConfig_IntStartValue].lastowner].ownerName,iAampCfgValue[i-eAAMPConfig_IntStartValue].value,iAampCfgValue[i-eAAMPConfig_IntStartValue].lastvalue);
			iAampCfgValue[i-eAAMPConfig_IntStartValue].owner = iAampCfgValue[i-eAAMPConfig_IntStartValue].lastowner;
			iAampCfgValue[i-eAAMPConfig_IntStartValue].value = iAampCfgValue[i-eAAMPConfig_IntStartValue].lastvalue;

		}
	}

	// All long values
	for(int i=eAAMPConfig_LongStartValue+1;i<eAAMPConfig_LongMaxValue;i++)
	{
		// for long array
		if(lAampCfgValue[i-eAAMPConfig_LongStartValue].owner == owner && lAampCfgValue[i-eAAMPConfig_LongStartValue].owner  != lAampCfgValue[i-eAAMPConfig_LongStartValue].lastowner)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s]->[%-5s][%ld]->[%ld]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[lAampCfgValue[i-eAAMPConfig_LongStartValue].owner].ownerName,
				OwnerLookUpTable[lAampCfgValue[i-eAAMPConfig_LongStartValue].lastowner].ownerName,lAampCfgValue[i-eAAMPConfig_LongStartValue].value,lAampCfgValue[i-eAAMPConfig_LongStartValue].lastvalue);
			lAampCfgValue[i-eAAMPConfig_LongStartValue].owner = lAampCfgValue[i-eAAMPConfig_LongStartValue].lastowner;
			lAampCfgValue[i-eAAMPConfig_LongStartValue].value = lAampCfgValue[i-eAAMPConfig_LongStartValue].lastvalue;
		}
	}

	// All double values
	for(int i=eAAMPConfig_DoubleStartValue+1;i<eAAMPConfig_DoubleMaxValue;i++)
	{
		// for double array
		if(dAampCfgValue[i-eAAMPConfig_DoubleStartValue].owner == owner && dAampCfgValue[i-eAAMPConfig_DoubleStartValue].owner != dAampCfgValue[i-eAAMPConfig_DoubleStartValue].lastowner)
                {
					logprintf("Cfg [%-3d][%-20s][%-5s]->[%-5s][%f]->[%f]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[dAampCfgValue[i-eAAMPConfig_DoubleStartValue].owner].ownerName,
						OwnerLookUpTable[dAampCfgValue[i-eAAMPConfig_DoubleStartValue].lastowner].ownerName,dAampCfgValue[i-eAAMPConfig_DoubleStartValue].value,dAampCfgValue[i-eAAMPConfig_DoubleStartValue].lastvalue);
					dAampCfgValue[i-eAAMPConfig_DoubleStartValue].owner = dAampCfgValue[i-eAAMPConfig_DoubleStartValue].lastowner;
					dAampCfgValue[i-eAAMPConfig_DoubleStartValue].value = dAampCfgValue[i-eAAMPConfig_DoubleStartValue].lastvalue;
                }
        }


	// All String values
	for(int i=eAAMPConfig_StringStartValue+1;i<eAAMPConfig_StringMaxValue;i++)
	{
		// for string array
		if(sAampCfgValue[i-eAAMPConfig_StringStartValue].owner == owner && sAampCfgValue[i-eAAMPConfig_StringStartValue].owner != sAampCfgValue[i-eAAMPConfig_StringStartValue].lastowner)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s]->[%-5s][%s]->[%s]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[sAampCfgValue[i-eAAMPConfig_StringStartValue].owner].ownerName,
				OwnerLookUpTable[sAampCfgValue[i-eAAMPConfig_StringStartValue].lastowner].ownerName,sAampCfgValue[i-eAAMPConfig_StringStartValue].value.c_str(),sAampCfgValue[i-eAAMPConfig_StringStartValue].lastvalue.c_str());
			sAampCfgValue[i-eAAMPConfig_StringStartValue].owner = sAampCfgValue[i-eAAMPConfig_StringStartValue].lastowner;
			sAampCfgValue[i-eAAMPConfig_StringStartValue].value = sAampCfgValue[i-eAAMPConfig_StringStartValue].lastvalue;
		}
	}

}

/**
 * @brief ShowConfiguration - Function to list configration values based on the owner
 *
 * @param[in] owner - Owner value for listing
 * @return None
 */
void AampConfig::ShowConfiguration(ConfigPriority owner)
{
	// All Bool values
	for(int i=0;i<eAAMPConfig_BoolMaxValue;i++)
	{
		if(bAampCfgValue[i].owner == owner || owner == AAMP_MAX_SETTING)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s][%s]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[bAampCfgValue[i].owner].ownerName,bAampCfgValue[i].value?"true":"false");
		}
	}

	// All integer values
	for(int i=eAAMPConfig_IntStartValue+1;i<eAAMPConfig_IntMaxValue;i++)
	{
		// for int array
		if(iAampCfgValue[i-eAAMPConfig_IntStartValue].owner == owner || owner == AAMP_MAX_SETTING)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s][%d]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[iAampCfgValue[i-eAAMPConfig_IntStartValue].owner].ownerName,iAampCfgValue[i-eAAMPConfig_IntStartValue].value);
		}
	}

	// All long values
	for(int i=eAAMPConfig_LongStartValue+1;i<eAAMPConfig_LongMaxValue;i++)
	{
		// for long array
		if(lAampCfgValue[i-eAAMPConfig_LongStartValue].owner == owner || owner == AAMP_MAX_SETTING)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s][%ld]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[lAampCfgValue[i-eAAMPConfig_LongStartValue].owner].ownerName,lAampCfgValue[i-eAAMPConfig_LongStartValue].value);
		}
	}

	// All double values
	for(int i=eAAMPConfig_DoubleStartValue+1;i<eAAMPConfig_DoubleMaxValue;i++)
	{
		// for double array
		if(dAampCfgValue[i-eAAMPConfig_DoubleStartValue].owner == owner || owner == AAMP_MAX_SETTING)
                {
                        logprintf("Cfg [%-3d][%-20s][%-5s][%f]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[dAampCfgValue[i-eAAMPConfig_DoubleStartValue].owner].ownerName,dAampCfgValue[i-eAAMPConfig_DoubleStartValue].value);
                }
        }



	// All String values
	for(int i=eAAMPConfig_StringStartValue+1;i<eAAMPConfig_StringMaxValue;i++)
	{
		// for string array
		if(sAampCfgValue[i-eAAMPConfig_StringStartValue].owner == owner || owner == AAMP_MAX_SETTING)
		{
			logprintf("Cfg [%-3d][%-20s][%-5s][%s]",i,GetConfigName((AAMPConfigSettings)i).c_str(),OwnerLookUpTable[sAampCfgValue[i-eAAMPConfig_StringStartValue].owner].ownerName,sAampCfgValue[i-eAAMPConfig_StringStartValue].value.c_str());
		}
	}

	if(mChannelOverrideMap.size() && (owner == AAMP_DEV_CFG_SETTING || owner == AAMP_MAX_SETTING))
	{
		ChannelMapIter iter;
		for (iter = mChannelOverrideMap.begin(); iter != mChannelOverrideMap.end(); ++iter)
		{
			logprintf("Cfg Channel[%s]-> [%s]",iter->name.c_str(),iter->uri.c_str());
		}
	}

}

/**
 * @brief ReadNumericHelper - Parse helper function
 *
 * @param[in] valStr - string input to convert
 * @param[out] value - coverted output
 * @return true on success
 */
template<typename T>
bool AampConfig::ReadNumericHelper(std::string valStr, T& value)
{
	bool ret=false;
	if (valStr.size())
	{
		ret = true;
		if (std::is_same<T, int>::value)
			value = std::stoi(valStr);
		else if (std::is_same<T, long>::value)
			value = std::stol(valStr);
		else if (std::is_same<T, float>::value)
			value = std::stof(valStr);
		else if (std::is_same<T, double>::value)
			value = std::stod(valStr);
		else
		{
			logprintf("%s:ERROR Invalid Input \n",__FUNCTION__);
			ret = false;
		}

	}
	return ret;
}

#ifdef IARM_MGR
/**
 * @brief GetTR181AAMPConfig
 *
 * @param[in] paramName - Parameter Name to parse
 * @param[in] configLen
 * @return config value
 */
char * AampConfig::GetTR181AAMPConfig(const char * paramName, size_t & iConfigLen)
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

				logprintf("GetTR181AAMPConfig: Got:%s En-Len:%d Dec-len:%d",strforLog.c_str(),param.paramLen,iConfigLen);
			}
			else
			{
				logprintf("GetTR181AAMPConfig: Not a string param type=%d or Invalid len:%d ",param.paramtype, param.paramLen);
			}
		}
	}
	else
	{
		logprintf("GetTR181AAMPConfig: Failed to retrieve value result=%d",result);
	}
	return strConfig;
}
#endif

#ifdef UNIT_TEST_ENABLED
int main()
{
	AampConfig var1,var2;
	var1.ReadAampCfgTxtFile();

	var1.ShowAAMPConfiguration();
	for(int i=0;i<eAAMPConfig_BoolMaxValue;i++)
	{
		var1.SetConfigValue(AAMP_DEV_CFG_SETTING,(AAMPConfigSettings)i,false);
	}
	var1.ShowDevCfgConfiguration();
	for(int i=0;i<eAAMPConfig_BoolMaxValue;i++)
	{
		var1.SetConfigValue(AAMP_DEV_CFG_SETTING,(AAMPConfigSettings)i,true);
	}
	var1.ShowOperatorSetConfiguration();
	for(int i=0;i<eAAMPConfig_BoolMaxValue;i++)
	{
		var1.SetConfigValue(AAMP_OPERATOR_SETTING,(AAMPConfigSettings)i,false);
	}
	var1.ShowAppSetConfiguration();

	/*
	var1.SetConfigValue(AAMP_OPERATOR_SETTING, eAAMPConfig_EnableABR, false);
	var1.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_EnableABR, true);
	var1.SetConfigValue(AAMP_OPERATOR_SETTING, eAAMPConfig_EnableABR, false);
	printf("Var 1 value : %d\n",var1.IsConfigSet(eAAMPConfig_EnableABR));
	printf("Var 2 value : %d\n",var2.IsConfigSet(eAAMPConfig_EnableABR));
	var2 = var1;
	printf("Var 2 value : %d\n",var2.IsConfigSet(eAAMPConfig_EnableABR));
	*/
	var1.SetConfigValue<std::string>(AAMP_DEV_CFG_SETTING,eAAMPConfig_LicenseServerUrl,(std::string)"Testing");
	printf("Before Client DAI : %d \n",var1.IsConfigSet(eAAMPConfig_EnableClientDai));
	var1.ToggleConfigValue(AAMP_DEV_CFG_SETTING,eAAMPConfig_EnableClientDai);
	printf("After Client DAI : %d \n",var1.IsConfigSet(eAAMPConfig_EnableClientDai));
	std::string test1;
	var1.GetConfigValue(eAAMPConfig_LicenseServerUrl,test1);
	printf("Var1 LicenseServer Url:%s\n",test1.c_str());

	var2.ReadAampCfgTxtFile();
	var2.SetConfigValue<std::string>(AAMP_OPERATOR_SETTING,eAAMPConfig_LicenseServerUrl,"Testing URL");
	var2.SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,true);
	var2.ShowAAMPConfiguration();
	std::string ovrride;
	if(var2.GetChannelOverride("HBOCM",ovrride))
	{
		printf("Ch override for HBO: %s \n",ovrride.c_str());
	}
	return 0;
}
#endif



//TODO
// Time conversion of networktimeout , manifesttimeout ,
// Handling info/debug/trace logging
// channel mapping
// playlist Cache size calc
// MaxDRM session min n max checl
// langcodepref - convert to enum





