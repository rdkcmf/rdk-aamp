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
 * @file GlobalConfigAAMP.cpp
 * @brief Advanced Adaptive Media Player (AAMP) GlobalConfigAAMP impl
 */

#include "GlobalConfigAAMP.h"
#include <algorithm>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief GlobalConfigAAMP Constructor
 */
GlobalConfigAAMP::GlobalConfigAAMP():
	defaultBitrate(DEFAULT_INIT_BITRATE), defaultBitrate4K(DEFAULT_INIT_BITRATE_4K), bEnableABR(true), noFog(false), mapMPD(NULL),mapM3U8(NULL),
	abrCacheLife(DEFAULT_ABR_CACHE_LIFE), abrCacheLength(DEFAULT_ABR_CACHE_LENGTH),
	maxCachedFragmentsPerTrack(DEFAULT_CACHED_FRAGMENTS_PER_TRACK),
	harvestCountLimit(0), harvestPath(0), harvestConfig(0xFFFFFFFF)/*Enable all type by default */,
	gPreservePipeline(0), gAampDemuxHLSAudioTsTrack(1), gAampMergeAudioTrack(1), forceEC3(eUndefinedState),
	gAampDemuxHLSVideoTsTrack(1), demuxHLSVideoTsTrackTM(1), gThrottle(0), demuxedAudioBeforeVideo(0),
	playlistsParallelFetch(eUndefinedState), prefetchIframePlaylist(false), dashParallelFragDownload(eUndefinedState),
	disableEC3(eUndefinedState), disableATMOS(eUndefinedState), abrOutlierDiffBytes(DEFAULT_ABR_OUTLIER), abrSkipDuration(DEFAULT_ABR_SKIP_DURATION),
	liveOffset(-1),cdvrliveOffset(-1), abrNwConsistency(DEFAULT_ABR_NW_CONSISTENCY_CNT),
	disablePlaylistIndexEvent(1), enableSubscribedTags(1), dashIgnoreBaseURLIfSlash(false),networkTimeoutMs(-1),
	licenseAnonymousRequest(false), minInitialCacheSeconds(MINIMUM_INIT_CACHE_NOT_OVERRIDDEN), useLinearSimulator(false),
	bufferHealthMonitorDelay(DEFAULT_BUFFER_HEALTH_MONITOR_DELAY), bufferHealthMonitorInterval(DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL),
	preferredDrm(eDRM_PlayReady), hlsAVTrackSyncUsingStartTime(false), licenseServerURL(NULL), licenseServerLocalOverride(false),
	vodTrickplayFPS(TRICKPLAY_NETWORK_PLAYBACK_FPS),vodTrickplayFPSLocalOverride(false), linearTrickplayFPS(TRICKPLAY_TSB_PLAYBACK_FPS),
	linearTrickplayFPSLocalOverride(false), stallErrorCode(DEFAULT_STALL_ERROR_CODE), stallTimeoutInMS(DEFAULT_STALL_DETECTION_TIMEOUT),
	httpProxy(0), reportProgressInterval(0), mpdDiscontinuityHandling(true), mpdDiscontinuityHandlingCdvr(true), bForceHttp(false),
	internalReTune(true), bAudioOnlyPlayback(false),licenseRetryWaitTime(DEF_LICENSE_REQ_RETRY_WAIT_TIME),
	iframeBitrate(0), iframeBitrate4K(0),ptsErrorThreshold(MAX_PTS_ERRORS_THRESHOLD), ckLicenseServerURL(NULL),
	curlStallTimeout(0), curlDownloadStartTimeout(0), enableMicroEvents(false), enablePROutputProtection(false),
	reTuneOnBufferingTimeout(true), gMaxPlaylistCacheSize(0), waitTimeBeforeRetryHttp5xxMS(DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS),
	dash_MaxDRMSessions(MIN_DASH_DRM_SESSIONS), tunedEventConfigLive(eTUNED_EVENT_MAX), tunedEventConfigVOD(eTUNED_EVENT_MAX),
	isUsingLocalConfigForPreferredDRM(false), pUserAgentString(NULL), logging(), sslVerifyPeer(eUndefinedState),
	mSubtitleLanguage(), enableClientDai(false), playAdFromCDN(false), mEnableVideoEndEvent(true),
	discontinuityTimeout(DEFAULT_DISCONTINUITY_TIMEOUT), bReportVideoPTS(eUndefinedState), mEnableRectPropertyCfg(eUndefinedState),
	decoderUnavailableStrict(false), aampAbrThresholdSize(DEFAULT_AAMP_ABR_THRESHOLD_SIZE),
	langCodePreference(0), bDescriptiveAudioTrack(false), useAppSrcForProgressivePlayback(false), reportBufferEvent(true),
	manifestTimeoutMs(-1), fragmp4LicensePrefetch(true), enableBulkTimedMetaReport(eUndefinedState), playlistTimeoutMs(-1),
	mAsyncTuneConfig(eUndefinedState), mWesterosSinkConfig(eUndefinedState), mPropagateUriParameters(eTrueState), aampRemovePersistent(0), preplaybuffercount(DEFAULT_PREBUFFER_COUNT),
	mUseAverageBWForABR(eUndefinedState), mPreCacheTimeWindow(0), parallelPlaylistRefresh(eUndefinedState),
	abrBufferCheckEnabled(eUndefinedState), useNewDiscontinuity(eUndefinedState),
#ifdef INTELCE
	bPositionQueryEnabled(false),
#else
	bPositionQueryEnabled(true),
#endif
	useRetuneForUnpairedDiscontinuity(eUndefinedState), uriParameter(NULL), customHeaderStr{""}, useRetuneForGSTInternalError(eUndefinedState),
	minABRBufferForRampDown(AAMP_LOW_BUFFER_BEFORE_RAMPDOWN), maxABRBufferForRampUp(AAMP_HIGH_BUFFER_BEFORE_RAMPUP),
	rampdownLimit(-1), minBitrate(0), maxBitrate(0), segInjectFailCount(0), drmDecryptFailCount(0),
	initFragmentRetryCount(-1), unknownValues(), useMatchingBaseUrl(eUndefinedState), bEnableSubtec(true), bWebVttNative(false),
	nativeCCRendering(false), preferredCEA708(eUndefinedState)
	,mInitRampdownLimit(-1),
#ifdef IARM_MGR
	wifiCurlHeaderEnabled(true)
#else
	wifiCurlHeaderEnabled(false)
#endif
	, mTimeoutForSourceSetup(DEFAULT_TIMEOUT_FOR_SOURCE_SETUP)
	, midFragmentSeekEnabled(false)
	, mEnableSeekableRange(eUndefinedState)
	, mPersistBitRateOverSeek(eUndefinedState)
	, licenseCaching(eUndefinedState)
#ifdef REALTEKCE
	, bDisableUnderflow(true)
	, gstreamerBufferingBeforePlay(false)
#else
	, bDisableUnderflow(false)
	, gstreamerBufferingBeforePlay(true)
#endif
{
	//XRE sends onStreamPlaying while receiving onTuned event.
	//onVideoInfo depends on the metrics received from pipe.
        // considering round trip delay to remove overlay
        // onStreamPlaying is sent optimistically in advance
	setBaseUserAgentString(AAMP_USERAGENT_BASE_STRING);
	//mSubtitleLanguage = std::string("en");
}


/**
 * @brief GlobalConfigAAMP Destructor
 */
GlobalConfigAAMP::~GlobalConfigAAMP()
{
	if(licenseServerURL)
	{
		free(licenseServerURL);
		licenseServerURL = NULL;
	}

	if(httpProxy)
	{
		free(httpProxy);
		httpProxy = NULL;
	}

	if(ckLicenseServerURL)
	{
		free(ckLicenseServerURL);
		ckLicenseServerURL = NULL;
	}

	if(uriParameter)
	{
		free(uriParameter);
		uriParameter = NULL;
	}

	if(mapMPD)
	{
		free(mapMPD);
		mapMPD = NULL;
	}

	if(mapM3U8)
	{
		free(mapM3U8);
		mapM3U8 = NULL;
	}
}

/**
 * @brief Find the value of key that start with <key>
 * @param key the value to look for
 * @return value of key
 */
std::string GlobalConfigAAMP::getUnknownValue(const std::string& key)
{
	static const std::string empty;
	return std::string {getUnknownValue(key, empty)};
}

/**
 * @brief Find the value of key that start with <key>
 * @param key the value to look for
 * @param defaultValue value to be returned in case key not found
 * @return value of key or default
 */
const std::string& GlobalConfigAAMP::getUnknownValue(const std::string& key, const std::string& defaultValue)
{
	auto iter = unknownValues.find(key);
	if (iter != unknownValues.end())
	{
		return iter->second;
	}
	else
	{
		return defaultValue;
	}
}

/**
 * @brief Find the value of key that start with <key>
 * @param key the value to look for
 * @param defaultValue value to be returned in case key not found
 * @return value of key or default
 */
int GlobalConfigAAMP::getUnknownValue(const std::string& key, int defaultValue)
{
	auto iter = unknownValues.find(key);
	if (iter != unknownValues.end())
	{
		return atoi(iter->second.c_str());
	}
	else
	{
		return defaultValue;
	}
}

/**
 * @brief Find the value of key that start with <key>
 * @param key the value to look for
 * @param defaultValue value to be returned in case key not found
 * @return value of key or default
 */
bool GlobalConfigAAMP::getUnknownValue(const std::string& key, bool defaultValue)
{
	auto iter = unknownValues.find(key);
	if (iter != unknownValues.end())
	{
		bool ret = (iter->second.compare("true") == 0 || iter->second.compare("1") == 0 || iter->second.compare("on") == 0);
		return ret;
	}
	else
	{
		return defaultValue;
	}
}

/**
 * @brief Find the value of key that start with <key>
 * @param key the value to look for
 * @param defaultValue value to be returned in case key not found
 * @return value of key or default
 */
double GlobalConfigAAMP::getUnknownValue(const std::string& key, double defaultValue)
{
	auto iter = unknownValues.find(key);
	if (iter != unknownValues.end())
	{
		return atof(iter->second.c_str());
	}
	else
	{
		return defaultValue;
	}
}

/**
 * Finds all the unknown keys that start with <key>
 * @param key the value to look for
 * @param values [out] all the found keys that start with <key>
 * @return the count of values found
 */
int GlobalConfigAAMP::getMatchingUnknownKeys(const std::string& key, std::vector<std::string>& values)
{
	values.clear();
	std::for_each(unknownValues.begin(), unknownValues.end(), [key, &values](std::pair<std::string, std::string> p) {
		if (p.first.find(key) == 0) {
			values.push_back(p.first);
		}
	});
	return values.size();
}

/**
 * @brief Sets user agent string
 * @return none
 */
void GlobalConfigAAMP::setBaseUserAgentString(const char * newUserAgent)
{
	int iLen = strlen(newUserAgent) + strlen(AAMP_USERAGENT_SUFFIX) + 2;
	if(pUserAgentString)
	{
		free(pUserAgentString);
	}
	pUserAgentString =(char*) malloc(iLen);
	snprintf(pUserAgentString,iLen, "%s %s",newUserAgent,AAMP_USERAGENT_SUFFIX);  //CID:85162 - DC>STRING_BUFFER
}
