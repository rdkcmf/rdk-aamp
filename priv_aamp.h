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
 * @file priv_aamp.h
 * @brief private functions and types used internally by AAMP
 */

#ifndef PRIVAAMP_H
#define PRIVAAMP_H

#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include "main_aamp.h"
#include <curl/curl.h>
#include <string.h> // for memset
#include <glib.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <map>

extern void logprintf(const char *format, ...);

#ifdef __APPLE__
#define aamp_pthread_setname(tid,name) pthread_setname_np(name)
#else
#define aamp_pthread_setname(tid,name) pthread_setname_np(tid,name)
#endif

#ifdef TRACE
#define traceprintf logprintf
#else
#define traceprintf (void)
#endif

#define MAX_URI_LENGTH (2048) //Increasing size to include longer urls
#define AAMP_TRACK_COUNT 2 // internal use - audio+video track
#define AAMP_DRM_CURL_COUNT 2 // audio+video track DRMs
#define MAX_CURL_INSTANCE_COUNT (AAMP_TRACK_COUNT + AAMP_DRM_CURL_COUNT)
#define AAMP_MAX_PIPE_DATA_SIZE 1024
#define AAMP_LIVE_OFFSET 15
#define CURL_FRAGMENT_DL_TIMEOUT 5L
#define MANIFEST_CURL_TIMEOUT 10L
#define DEFAULT_CURL_TIMEOUT 5L
#define DEFAULT_CURL_CONNECTTIMEOUT 3L
#define EAS_CURL_TIMEOUT 3L
#define EAS_CURL_CONNECTTIMEOUT 2L
#define DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS (6*1000)
#define TRICKPLAY_NETWORK_PLAYBACK_FPS 4
#define TRICKPLAY_TSB_PLAYBACK_FPS 8
#define DEFAULT_INIT_BITRATE     2500000 // 2.5 mb - for non-4k playback
#define DEFAULT_INIT_BITRATE_4K 13000000 // 13mb - to select 3/4 profile while statring 4k playback
#define DEFAULT_MINIMUM_CACHE_VOD_SECONDS  0
#define BITRATE_ALLOWED_VARIATION_BAND 500000 // nw bw change of this much will be ignored
#define AAMP_ABR_THRESHOLD_SIZE 50000 // 50K 
#define DEFAULT_ABR_CACHE_LIFE 5000  // 5 sec
#define DEFAULT_ABR_CACHE_LENGTH 3
#define DEFAULT_ABR_OUTLIER 5000000 // 5 MB
#define DEFAULT_ABR_SKIP_DURATION 6 // 6 sec
#define DEFAULT_ABR_NW_CONSISTENCY_CNT 2
#define MAX_SEG_DOWNLOAD_FAIL_COUNT 10
#define MAX_SEG_DRM_DECRYPT_FAIL_COUNT 10
#define MAX_SEG_INJECT_FAIL_COUNT 10

#define DEFAULT_STALL_ERROR_CODE (7600)
#define DEFAULT_STALL_DETECTION_TIMEOUT (10000)

#define DEFAULT_REPORT_PROGRESS_INTERVAL (1000)
#define NOW_SYSTEM_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
#define NOW_STEADY_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

/*1 for debugging video track, 2 for audio track and 3 for both*/
/*#define AAMP_DEBUG_FETCH_INJECT 0x01*/


/**
 * @struct GrowableBuffer
 * @brief growable buffer abstraction
 */
struct GrowableBuffer
{
	char *ptr;
	size_t len;
	size_t avail;
};

/**
 * @enum TunedEventConfig
 * @brief Point at which tuned event is generated
 */
enum TunedEventConfig
{
	eTUNED_EVENT_ON_GST_PLAYING,
	eTUNED_EVENT_ON_PLAYLIST_INDEXED,
	eTUNED_EVENT_ON_FIRST_FRAGMENT_DECRYPTED
};

/**
 * @enum PlaybackErrorType
 * @brief Errors during playback
 */
enum PlaybackErrorType
{
	eGST_ERROR_PTS,
	eGST_ERROR_UNDERFLOW,
	eDASH_ERROR_STARTTIME_RESET
};

/**
 * @enum DRMSystems
 * @brief DRM types supported by AAMP
 */
enum DRMSystems
{
	eDRM_NONE,
	eDRM_WideVine,
	eDRM_PlayReady,
	eDRM_CONSEC_agnostic,
	eDRM_Adobe_Access,	//HLS
	eDRM_Vanilla_AES,	//HLS
	eDRM_MAX_DRMSystems
};

/**
 * @enum TuneType
 * @brief Type of Tune operation
 */
enum TuneType
{
	eTUNETYPE_NEW_NORMAL, /** Play from live point for live streams, from start for VOD*/
	eTUNETYPE_NEW_SEEK, /** A new tune with valid seek position*/
	eTUNETYPE_SEEK, /** Seek to a position. Not a new channel, so resources can be reused*/
	eTUNETYPE_SEEKTOLIVE, /** Seek to live point. Not a new channel, so resources can be reused*/
	eTUNETYPE_RETUNE, /** Internal retune for error handling.*/
	eTUNETYPE_LAST /** Use the tune mode used in last tune*/
};

/**
 * @enum ContentType
 * @brief Type of content
 */
enum ContentType
{
    ContentType_UNKNOWN,    //0
    ContentType_CDVR,       //1
    ContentType_VOD,        //2
    ContentType_LINEAR,     //3
    ContentType_IVOD,       //4
    ContentType_EAS,        //5
    ContentType_CAMERA,     //6
    ContentType_DVR,        //7
    ContentType_MDVR,       //8
    ContentType_IPDVR,      //9
    ContentType_PPV,        //10
    ContentType_MAX
};

/**
 * @enum AAMP_LogLevel
 * @brief Logging level
 */
enum AAMP_LogLevel
{
	eLOGLEVEL_TRACE,
	eLOGLEVEL_INFO,
	eLOGLEVEL_WARN,
	eLOGLEVEL_ERROR
};		 


/**
 * @enum HttpHeaderType
 * @brief HTTP header types
 */
enum HttpHeaderType
{
	eHTTPHEADERTYPE_COOKIE, /** Cookie Header */
	eHTTPHEADERTYPE_XREASON, /** X-Reason Header */
	eHTTPHEADERTYPE_UNKNOWN=-1
};

/**
 * @class GlobalConfigAAMP
 * @brief Manages AAMP global configuration
 */
class GlobalConfigAAMP
{
public:
	long defaultBitrate;
	long defaultBitrate4K;
	bool bEnableABR;
	bool SAP;
	bool bEnableCC;
	bool noFog;
	int mapMPD; //0=Disable, 1=m3u8 to mpd mapping, 2=COAM mapping, 3='*-nat-*.comcast.net/' to 'ctv-nat-slivel4lb-vip.cmc.co.ndcwest.comcast.net/' mapping
	bool fogSupportsDash;
#ifdef AAMP_HARVEST_SUPPORT_ENABLED
	int harvest;
#endif

	/**
	* @struct logging
	* @brief flags of which levels are enabled
	*/
	struct
	{
		bool info;
		bool trace;
		bool gst;
		bool curl;
		bool progress;
		bool debug;
	} logging;
	int gPreservePipeline;
	int gAampDemuxHLSAudioTsTrack;
	int gAampDemuxHLSVideoTsTrack;
	int gAampMergeAudioTrack;
	int gThrottle;
	TunedEventConfig tunedEventConfigLive;
	TunedEventConfig tunedEventConfigVOD;
	int demuxHLSVideoTsTrackTM;
	int demuxedAudioBeforeVideo;
	bool playlistsParallelFetch;
	bool prefetchIframePlaylist;
	int forceEC3;
	int disableEC3;
	int disableATMOS;
	int liveOffset;
	int adPositionSec;
	const char* adURL;
	int disablePlaylistIndexEvent;
	int enableSubscribedTags;
	bool dashIgnoreBaseURLIfSlash;
	long fragmentDLTimeout;
	bool licenseAnonymousRequest;
	int minVODCacheSeconds;
	int abrCacheLife;
	int abrCacheLength;
	int abrOutlierDiffBytes;
	int abrNwConsistency;
	bool hlsAVTrackSyncUsingStartTime;
	char* licenseServerURL;
	bool licenseServerLocalOverride;
	int vodTrickplayFPS;
	bool vodTrickplayFPSLocalOverride;
	int linearTrickplayFPS;
	bool linearTrickplayFPSLocalOverride;
	int stallErrorCode;
	int stallTimeoutInMS;
	const char* httpProxy;
	int reportProgressInterval;
	int preferredDrm;
	bool mpdDiscontinuityHandling;
	bool mpdDiscontinuityHandlingCdvr;
	bool bForceHttp;
	int abrSkipDuration;
public:

    /**
     * @brief GlobalConfigAAMP Constructor
     */
	GlobalConfigAAMP() :defaultBitrate(DEFAULT_INIT_BITRATE), defaultBitrate4K(DEFAULT_INIT_BITRATE_4K), bEnableABR(true), SAP(false), noFog(false), mapMPD(0), fogSupportsDash(true),abrCacheLife(DEFAULT_ABR_CACHE_LIFE),abrCacheLength(DEFAULT_ABR_CACHE_LENGTH),
#ifdef AAMP_CC_ENABLED
		bEnableCC(true),
#endif
#ifdef AAMP_HARVEST_SUPPORT_ENABLED
		harvest(0),
#endif
		gPreservePipeline(0), gAampDemuxHLSAudioTsTrack(1), gAampMergeAudioTrack(1), forceEC3(0),
		gAampDemuxHLSVideoTsTrack(1), demuxHLSVideoTsTrackTM(1), gThrottle(0), demuxedAudioBeforeVideo(0),
		playlistsParallelFetch(false), prefetchIframePlaylist(false),
		disableEC3(0), disableATMOS(0),abrOutlierDiffBytes(DEFAULT_ABR_OUTLIER),abrSkipDuration(DEFAULT_ABR_SKIP_DURATION),
		liveOffset(AAMP_LIVE_OFFSET), adPositionSec(0), adURL(0),abrNwConsistency(DEFAULT_ABR_NW_CONSISTENCY_CNT),
		disablePlaylistIndexEvent(1), enableSubscribedTags(1), dashIgnoreBaseURLIfSlash(false),fragmentDLTimeout(CURL_FRAGMENT_DL_TIMEOUT),
		licenseAnonymousRequest(false), minVODCacheSeconds(DEFAULT_MINIMUM_CACHE_VOD_SECONDS),aampLoglevel(eLOGLEVEL_WARN),
		preferredDrm(1), hlsAVTrackSyncUsingStartTime(false), licenseServerURL(NULL), licenseServerLocalOverride(false),
		vodTrickplayFPS(TRICKPLAY_NETWORK_PLAYBACK_FPS),vodTrickplayFPSLocalOverride(false),
		linearTrickplayFPS(TRICKPLAY_TSB_PLAYBACK_FPS),linearTrickplayFPSLocalOverride(false),
		stallErrorCode(DEFAULT_STALL_ERROR_CODE), stallTimeoutInMS(DEFAULT_STALL_DETECTION_TIMEOUT), httpProxy(0),
		reportProgressInterval(DEFAULT_REPORT_PROGRESS_INTERVAL), mpdDiscontinuityHandling(true), mpdDiscontinuityHandlingCdvr(true),bForceHttp(false)
	{
		memset(&logging, 0, sizeof(logging) );
		tunedEventConfigLive = eTUNED_EVENT_ON_PLAYLIST_INDEXED;
		tunedEventConfigVOD = eTUNED_EVENT_ON_PLAYLIST_INDEXED; //Changed since VOD-Cache feature 
									//is deprecated
	}

	/**
	 * @brief GlobalConfigAAMP Destructor
	 */
	~GlobalConfigAAMP(){}

	/**
	 * @brief Get SAP configuration
	 * @retval true if SAP is enabled
	 */
	bool aamp_GetSAP(void)
	{
		return SAP;
	}

	/**
	 * @brief Set SAP configuration
	 * @param on true to enable SAP, false to disable SAP
	 */
	void aamp_SetSAP(bool on)
	{
		SAP = on;
	}

	/**
	 * @brief Check if CC is enabled
	 * @retval true if enabled, false if disabled
	 */
	bool aamp_GetCCStatus(void)
	{
		return bEnableCC;
	}

	/**
	 * @brief Set CC status
	 * @param on true to enable, false to disable
	 */
	void aamp_SetCCStatus(bool on)
	{
		bEnableCC = on;
	}


	/**
	 * @brief Check if given log level is enabled
	 * @param chkLevel log level to be checked
	 * @retval true if enabled, false if disabled
	 */
	bool isLogLevelAllowed(AAMP_LogLevel chkLevel)
	{
		return (chkLevel>=aampLoglevel);
	}

	/**
	 * @brief Set aamp log level
	 * @param newLevel New level to set
	 */
	void setLogLevel(AAMP_LogLevel newLevel)
	{
		if(!logging.info && !logging.debug)
			aampLoglevel = newLevel;
	}
private:
	AAMP_LogLevel aampLoglevel;
};

extern GlobalConfigAAMP *gpGlobalConfig;

#define AAMPLOG(LEVEL,FORMAT, ...) \
	do { if (gpGlobalConfig->isLogLevelAllowed(LEVEL)) { \
        	logprintf(FORMAT, ##__VA_ARGS__); \
    } } while (0)

#define AAMPLOG_TRACE(FORMAT, ...) AAMPLOG(eLOGLEVEL_TRACE, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_INFO(FORMAT, ...) AAMPLOG(eLOGLEVEL_INFO,  FORMAT, ##__VA_ARGS__)
#define AAMPLOG_WARN(FORMAT, ...) AAMPLOG(eLOGLEVEL_WARN, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_ERR(FORMAT, ...) AAMPLOG(eLOGLEVEL_ERROR,  FORMAT, ##__VA_ARGS__)

// context-free utility functions

void aamp_ResolveURL(char *dst, const char *base, const char *uri);
long long aamp_GetCurrentTimeMS(void);	//TODO: Use NOW_STEADY_TS_MS/NOW_SYSTEM_TS_MS instead
void aamp_Error(const char *msg);
void aamp_Free(char **pptr);
void aamp_AppendBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len);
void aamp_AppendNulTerminator(struct GrowableBuffer *buffer);
void aamp_Malloc(struct GrowableBuffer *buffer, size_t len);
const char * GetDrmSystemID(DRMSystems drmSystem);
const char * GetDrmSystemName(DRMSystems drmSystem);

#ifdef AAMP_CC_ENABLED

bool aamp_IsCCEnabled(void);
int aamp_CCStart(void *handle);
void aamp_CCStop(void);
int aamp_CCShow(void);
int aamp_CCHide(void);

#endif //AAMP_CC_ENABLED

/**
 * @enum ProfilerBucketType
 * @brief Bucket types used for profiling
 */
typedef enum
{
	PROFILE_BUCKET_MANIFEST,

	PROFILE_BUCKET_PLAYLIST_VIDEO,
	PROFILE_BUCKET_PLAYLIST_AUDIO,

	PROFILE_BUCKET_INIT_VIDEO,
	PROFILE_BUCKET_INIT_AUDIO,

	PROFILE_BUCKET_FRAGMENT_VIDEO,
	PROFILE_BUCKET_FRAGMENT_AUDIO,

	PROFILE_BUCKET_DECRYPT_VIDEO,
	PROFILE_BUCKET_DECRYPT_AUDIO,

	PROFILE_BUCKET_LA_TOTAL,
	PROFILE_BUCKET_LA_PREPROC,
	PROFILE_BUCKET_LA_NETWORK,
	PROFILE_BUCKET_LA_POSTPROC,

	PROFILE_BUCKET_FIRST_BUFFER,
	PROFILE_BUCKET_FIRST_FRAME,
	PROFILE_BUCKET_TYPE_COUNT,
} ProfilerBucketType;


/**
 * @enum ClassicProfilerBucketType
 * @brief Classic xre profiler bucket types
 */
typedef enum
 {
        TuneTimeBaseTime,
        TuneTimeBeginLoad,
        TuneTimePrepareToPlay,
        TuneTimePlay,
        TuneTimeDrmReady,
        TuneTimeStartStream,
        TuneTimeStreaming,
        TuneTimeBackToXre,
        TuneTimeMax
 }ClassicProfilerBucketType;

/**
 * @class ProfileEventAAMP
 * @brief Manages profile events
 */
class ProfileEventAAMP
{
private:
	// TODO: include settop type (to distinguish settop performance)
	// TODO: include flag to indicate whether FOG used (to isolate FOG overhead)

	// TODO: output tune profiling info in a single line, consumable by telemetry system

	// TODO: include retry/failures as part of tune logging
	// TODO: include teardown overhead as part of tune logging

	/**
	 * @struct ProfilerBucket
	 * @brief Profiler bucket of an event
	 */
	struct ProfilerBucket
	{
		unsigned int tStart; // Relative start time of operation, based on tuneStartMonotonicBase
		//int actualStartRelativeTimeMs; // most recent request/retry; normally equal to logicalStartRelativeTimeMs
		unsigned int tFinish; // Relative end time of operation, based on tuneStartMonotonicBase
		int errorCount; // non-zero if errors/retries occured during this operation
		bool complete; // true if this step already accounted for, and further profiling should be ignored
	} buckets[PROFILE_BUCKET_TYPE_COUNT];

#define bucketsOverlap(id1,id2) \
		buckets[id1].complete && buckets[id2].complete && \
		(buckets[id1].tStart <= buckets[id2].tFinish) && (buckets[id2].tStart <= buckets[id1].tFinish)

#define bucketDuration(id) \
		(buckets[id].complete?(buckets[id].tFinish - buckets[id].tStart):0)

	long long tuneStartMonotonicBase; //base time from Monotonic clock for interval calculation

	// common UTC base for start of tune
	long long tuneStartBaseUTCMS;
	long long xreTimeBuckets[TuneTimeMax];
	long bandwidthBitsPerSecondVideo;
	long bandwidthBitsPerSecondAudio;
	int drmErrorCode;
	bool enabled;

	inline unsigned int effectiveBucketTime(ProfilerBucketType id1, ProfilerBucketType id2)
	{
#if 0
		if(bucketsOverlap(id1, id2))
			return MAX(buckets[id1].tFinish, buckets[id2].tFinish) - fmin(buckets[id1].tStart, buckets[id2].tStart);
#endif
		return bucketDuration(id1) + bucketDuration(id2);
	}
public:

	/**
	 * @brief ProfileEventAAMP Constructor
	 */
	ProfileEventAAMP()
	{
	}

	/**
	 * @brief ProfileEventAAMP Destructor
	 */
	~ProfileEventAAMP(){}

	/**
	 * @brief Set video bps profile data
	 * @param bw bandwidth in bps
	 */
	void SetBandwidthBitsPerSecondVideo(long bw)
	{
		bandwidthBitsPerSecondVideo = bw;
	}

	/**
	 * @brief Set audio bps profile data
	 * @param bw bandwidth in bps
	 */
	void SetBandwidthBitsPerSecondAudio(long bw)
	{
		bandwidthBitsPerSecondAudio = bw;
	}

	/**
	 * @brief Set drm error code
	 * @param errCode drm error code
	 */
	void SetDrmErrorCode(int errCode)
	{
		drmErrorCode = errCode;
	}


	/**
	 * @brief Notify begin of a tune operation
	 */
	void TuneBegin(void)
	{ // start tune
		memset(buckets, 0, sizeof(buckets));
		tuneStartBaseUTCMS = NOW_SYSTEM_TS_MS;
		tuneStartMonotonicBase = NOW_STEADY_TS_MS;
		bandwidthBitsPerSecondVideo = 0;
		bandwidthBitsPerSecondAudio = 0;
		drmErrorCode = 0;
		enabled = true;
	}

/*
IP_AAMP_TUNETIME:
version,	// version for this protocol, initially zero
build,		// incremented when there are significant player changes/optimizations

// context:
settopType,	// enum encoding settop type (xi3v2, xg1v3, etc.)
contentType,	// cdvr/vod/linear/eas/web
streamType,	// m3u8/mpd
flags,
// 0x1 isFOGEnabled,
// 0x2 isDDPlus
// 0x4 isDemuxed

assetDurationMs, // length

tunestartUtcMs,	// when tune logically started from AAMP perspective

ManifestDownloadStartTime,  // offset in milliseconds from tunestart when main manifest begins download
ManifestDownloadTotalTime,  // time (ms) taken for main manifest download, relative to ManifestDownloadStartTime
ManifestDownloadFailCount,  // if >0 ManifestDownloadTotalTime spans multiple download attempts
ManifestDownloadErrorCode,  // first (http) error detected while retrieving manifest

PlaylistDownloadStartTime,  // offset in milliseconds from tunestart when playlist subManifest begins download
PlaylistDownloadTotalTime,  // time (ms) taken for playlist subManifest download, relative to PlaylistDownloadStartTime
PlaylistDownloadFailCount,  // if >0 otherwise PlaylistDownloadTotalTime spans multiple download attempts
PlaylistDownloadErrorCode,  // first (http) error detected while retrieving manifest

Fragment1DownloadStartTime, // offset in milliseconds from tunestart when fragment begins download
Fragment1DownloadTotalTime, // time (ms) taken for fragment download, relative to Fragment1DownloadStartTime
Fragment1DownloadFailCount, // if >0 Fragment1DownloadTotalTime spans multiple download attempts
Fragment1DownloadErrorCode, // first (http) error detected while retrieving fragment
Fragment1Bandwidth,	    // intrinsic bitrate of downloaded fragment
Fragment1EncryptType,       // 0 for clear, 1 for adobe access, 2 for playready, 3 for widevine
// future: may add other flags/values for EncryptType to indicate DRM caching (if known)
Fragment1DecryptTime,	    // time in milliseconds associated with fragment decryption

drmLicenseRequestStart,	    // offset in milliseconds from tunestart
drmLicenseRequestTotalTime, // time (ms) for license acquisition relative to drmLicenseRequestStart
drmFailErrorCode,           // nonzero if drm license acquisition failed during tuning

gstStart,	// offset in ms from tunestart when pipeline creation/setup begins
gstReady,	// offset in ms from tunestart when pipeline ready for input
gstPlaying,	// offset in ms from tunestart when pipeline first fed data
gstFirstFrame,  // offset in ms from tunestart when first frame of video is decoded/presented
*/

	/**
	 * @brief Notify end of a tune operation
	 *
	 * @param success true if tune is success
	 * @param contentType Type of content
	 * @param streamType Type of stream
	 * @param firstTune true if first tune
	 */
	void TuneEnd(bool success, ContentType contentType, int streamType, bool firstTune)
	{
		if(!enabled )
		{
			return;
		}
		enabled = false;
		unsigned int licenseAcqNWTime = bucketDuration(PROFILE_BUCKET_LA_NETWORK);
		if(licenseAcqNWTime == 0) licenseAcqNWTime = bucketDuration(PROFILE_BUCKET_LA_TOTAL); //A HACK for HLS

		logprintf("IP_AAMP_TUNETIME:%d,%d,%lld," // version, build, tuneStartBaseUTCMS
			"%d,%d,%d," 	// main manifest (start,total,err)
			"%d,%d,%d," 	// video playlist (start,total,err)
			"%d,%d,%d," 	// audio playlist (start,total,err)

			"%d,%d,%d," 	// video init-segment (start,total,err)
			"%d,%d,%d," 	// audio init-segment (start,total,err)

			"%d,%d,%d,%ld," 	// video fragment (start,total,err, bitrate)
			"%d,%d,%d,%ld," 	// audio fragment (start,total,err, bitrate)

			"%d,%d,%d," 	// licenseAcqStart, licenseAcqTotal, drmFailErrorCode
			"%d,%d,%d," 	// LAPreProcDuration, LANetworkDuration, LAPostProcDuration

			"%d,%d," 		// VideoDecryptDuration, AudioDecryptDuration
			"%d,%d," 		// gstPlayStartTime, gstFirstFrameTime
			"%d,%d,%d\n", 		// contentType, streamType, firstTune
			// TODO: settop type, flags, isFOGEnabled, isDDPlus, isDemuxed, assetDurationMs

			4, // version for this protocol, initially zero
			0, // build - incremented when there are significant player changes/optimizations
			tuneStartBaseUTCMS, // when tune logically started from AAMP perspective

			buckets[PROFILE_BUCKET_MANIFEST].tStart, bucketDuration(PROFILE_BUCKET_MANIFEST), buckets[PROFILE_BUCKET_MANIFEST].errorCount,
			buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].tStart, bucketDuration(PROFILE_BUCKET_PLAYLIST_VIDEO), buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].errorCount,
			buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].tStart, bucketDuration(PROFILE_BUCKET_PLAYLIST_AUDIO), buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].errorCount,

			buckets[PROFILE_BUCKET_INIT_VIDEO].tStart, bucketDuration(PROFILE_BUCKET_INIT_VIDEO), buckets[PROFILE_BUCKET_INIT_VIDEO].errorCount,
			buckets[PROFILE_BUCKET_INIT_AUDIO].tStart, bucketDuration(PROFILE_BUCKET_INIT_AUDIO), buckets[PROFILE_BUCKET_INIT_AUDIO].errorCount,

			buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].tStart, bucketDuration(PROFILE_BUCKET_FRAGMENT_VIDEO), buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].errorCount,bandwidthBitsPerSecondVideo,
			buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].tStart, bucketDuration(PROFILE_BUCKET_FRAGMENT_AUDIO), buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].errorCount,bandwidthBitsPerSecondAudio,

			buckets[PROFILE_BUCKET_LA_TOTAL].tStart, bucketDuration(PROFILE_BUCKET_LA_TOTAL), drmErrorCode,
			bucketDuration(PROFILE_BUCKET_LA_PREPROC), licenseAcqNWTime, bucketDuration(PROFILE_BUCKET_LA_POSTPROC),
			bucketDuration(PROFILE_BUCKET_DECRYPT_VIDEO),bucketDuration(PROFILE_BUCKET_DECRYPT_AUDIO),

			buckets[PROFILE_BUCKET_FIRST_BUFFER].tStart, // gstPlaying: offset in ms from tunestart when pipeline first fed data
			buckets[PROFILE_BUCKET_FIRST_FRAME].tStart,  // gstFirstFrame: offset in ms from tunestart when first frame of video is decoded/presented
			contentType, streamType, firstTune
			);
		fflush(stdout);
	}

	/**
	 * @brief Get classic style tune profile information
	 * @param success true if tune is success
	 * @param tuneRetries Number of retries
	 * @param playerLoadTime Load time
	 * @param streamType Stream type
	 * @param isLive true if live
	 * @param durationinSec Duration in seconds
	 * @param[out] TuneTimeInfoStr Output string
	 */
	void GetClassicTuneTimeInfo(bool success, int tuneRetries, long long playerLoadTime, int streamType, bool isLive,unsigned int durationinSec, char *TuneTimeInfoStr)
	{
                       // Prepare String for Classic TuneTime data
                       // Note: Certain buckets won't be available; will take the tFinish of the previous bucket as the start & finish those buckets.
                       xreTimeBuckets[TuneTimeBeginLoad]               =       tuneStartMonotonicBase ;
                       xreTimeBuckets[TuneTimePrepareToPlay]           =       tuneStartMonotonicBase + buckets[PROFILE_BUCKET_MANIFEST].tFinish;
                       xreTimeBuckets[TuneTimePlay]                    =       tuneStartMonotonicBase + MAX(buckets[PROFILE_BUCKET_MANIFEST].tFinish, MAX(buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].tFinish, buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].tFinish));
                       xreTimeBuckets[TuneTimeDrmReady]                =       MAX(xreTimeBuckets[TuneTimePlay], (tuneStartMonotonicBase +  buckets[PROFILE_BUCKET_LA_TOTAL].tFinish));
                       long long fragmentReadyTime                     =       tuneStartMonotonicBase + MAX(buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].tFinish, buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].tFinish);
                       xreTimeBuckets[TuneTimeStartStream]             =       MAX(xreTimeBuckets[TuneTimeDrmReady],fragmentReadyTime);
                       xreTimeBuckets[TuneTimeStreaming]               =       tuneStartMonotonicBase + buckets[PROFILE_BUCKET_FIRST_FRAME].tStart;

                       unsigned int failRetryBucketTime                =       tuneStartMonotonicBase - playerLoadTime;
                       unsigned int prepareToPlayBucketTime            =       (unsigned int)(xreTimeBuckets[TuneTimePrepareToPlay] - xreTimeBuckets[TuneTimeBeginLoad]);
                       unsigned int playBucketTime                     =       (unsigned int)(xreTimeBuckets[TuneTimePlay]- xreTimeBuckets[TuneTimePrepareToPlay]);
                       unsigned int drmReadyBucketTime                 =       (unsigned int)(xreTimeBuckets[TuneTimeDrmReady] - xreTimeBuckets[TuneTimePlay]) ;
                       unsigned int fragmentBucketTime                 =       (unsigned int)(fragmentReadyTime - xreTimeBuckets[TuneTimePlay]) ;
                       unsigned int decoderStreamingBucketTime         =       xreTimeBuckets[TuneTimeStreaming] - xreTimeBuckets[TuneTimeStartStream];
                       /*Note: 'Drm Ready' to 'decrypt start' gap is not covered in any of the buckets.*/

                       unsigned int manifestTotal      =       bucketDuration(PROFILE_BUCKET_MANIFEST);
                       unsigned int profilesTotal      =       effectiveBucketTime(PROFILE_BUCKET_PLAYLIST_VIDEO, PROFILE_BUCKET_PLAYLIST_AUDIO);
                       unsigned int initFragmentTotal  =       effectiveBucketTime(PROFILE_BUCKET_INIT_VIDEO, PROFILE_BUCKET_INIT_AUDIO);
                       unsigned int fragmentTotal      =       effectiveBucketTime(PROFILE_BUCKET_FRAGMENT_VIDEO, PROFILE_BUCKET_FRAGMENT_AUDIO);
                       unsigned int licenseTotal       =       bucketDuration(PROFILE_BUCKET_LA_TOTAL);
                       unsigned int licenseNWTime      =       bucketDuration(PROFILE_BUCKET_LA_NETWORK);
                       if(licenseNWTime == 0) licenseNWTime = licenseTotal;  //A HACK for HLS

                       // Total Network Time
                       unsigned int networkTime = manifestTotal + profilesTotal + initFragmentTotal + fragmentTotal + licenseNWTime;

                       snprintf(TuneTimeInfoStr,AAMP_MAX_PIPE_DATA_SIZE,"%d,%lld,%d,%d," //totalNetworkTime, playerLoadTime , failRetryBucketTime, prepareToPlayBucketTime,
                               "%d,%d,%d,"                                             //playBucketTime ,drmReadyBucketTime , decoderStreamingBucketTime
                               "%d,%d,%d,%d,"                                          // manifestTotal,profilesTotal,fragmentTotal,effectiveFragmentDLTime
                               "%d,%d,%d,%d,"                                          // licenseTotal,success,durationinMilliSec,isLive
                               "%lld,%lld,%lld,"                                       // TuneTimeBeginLoad,TuneTimePrepareToPlay,TuneTimePlay,
                               "%lld,%lld,%lld,"                                       //TuneTimeDrmReady,TuneTimeStartStream,TuneTimeStreaming
                               "%d,%d",                                             //streamType, tuneRetries
                               networkTime,playerLoadTime, failRetryBucketTime, prepareToPlayBucketTime,playBucketTime,drmReadyBucketTime,decoderStreamingBucketTime,
                               manifestTotal,profilesTotal,(initFragmentTotal + fragmentTotal),fragmentBucketTime, licenseTotal,success,durationinSec*1000,isLive,
                               xreTimeBuckets[TuneTimeBeginLoad],xreTimeBuckets[TuneTimePrepareToPlay],xreTimeBuckets[TuneTimePlay] ,xreTimeBuckets[TuneTimeDrmReady],
                               xreTimeBuckets[TuneTimeStartStream],xreTimeBuckets[TuneTimeStreaming],streamType,tuneRetries
                               );
#ifdef STANDALONE_AAMP
                       logprintf("AAMP=>XRE: %s\n",TuneTimeInfoStr);
#endif

	}


	/**
	 * @brief Mark beginning of a profile bucket type
	 * @param type bucket type
	 */
	void ProfileBegin(ProfilerBucketType type )
	{
		struct ProfilerBucket *bucket = &buckets[type];
		if (!bucket->complete && (0==bucket->tStart))	//No other Begin should record before the End
		{
			bucket->tStart = NOW_STEADY_TS_MS - tuneStartMonotonicBase;
			bucket->tFinish = bucket->tStart;
		}
	}


	/**
	 * @brief Mark error of a profile bucket type
	 * @param type bucket type
	 */
	void ProfileError(ProfilerBucketType type)
	{
		struct ProfilerBucket *bucket = &buckets[type];
		if (!bucket->complete)
		{
			bucket->errorCount++;
		}
	}


	/**
	 * @brief Mark end of a profile bucket type
	 * @param type bucket type
	 */
	void ProfileEnd( ProfilerBucketType type )
	{
		struct ProfilerBucket *bucket = &buckets[type];
		if (!bucket->complete)
		{
			bucket->complete = true;
			bucket->tFinish = NOW_STEADY_TS_MS - tuneStartMonotonicBase;
			/*
			static const char *bucketName[PROFILE_BUCKET_TYPE_COUNT] =
			{
			"manifest",
			"playlist",
			"fragment",
			"key",
			"decrypt"
			"first-frame"
			};

			logprintf("aamp %7d (+%6d): %s\n",
			bucket->tStart,
			bucket->tFinish - bucket->tStart,
			bucketName[type]);
			*/
		}
	}

	/**
	 * @brief Mark a profile information of a type to performed
	 * @param type bucket type
	 */
	void ProfilePerformed(ProfilerBucketType type)
	{
		ProfileBegin(type);
		buckets[type].complete = true;
	}
};

/**
 * @class TimedMetadata
 * @brief Holds information on timed meta data events
 */
class TimedMetadata
{
public:

	/**
	 * @brief TimedMetadata Constructor
	 */
	TimedMetadata() : _timeMS(0), _name(""), _content("") {}

	/**
	 * @brief TimedMetadata Constructor
	 * @param timeMS Time in millisecond
	 * @param name Name of metadata
	 * @param content Content of metadata
	 */
	TimedMetadata(double timeMS, std::string name, std::string content) : _timeMS(timeMS), _name(name), _content(content) {}

public:
	double      _timeMS;
	std::string _name;
	std::string _content;
};


/**
 * @brief IdleTask function pointer type
 * @param arg Argument to be passed on idle callback
 */
typedef int(*IdleTask)(void* arg);

/**
 * @struct  httpRespHeaderData
 * @brief To store Set Cookie: headers and X-Reason headers in HTTP Response
 */
struct httpRespHeaderData {
	int type;
	std::string data;
};


/**
 * @struct ListenerData
 * @brief Linked list node to store event listeners
 */
struct ListenerData {
	AAMPEventListener* eventListener;
	ListenerData* pNext;
};

/**
 * @class PrivateInstanceAAMP
 * @brief AAMP common private class
 */
class PrivateInstanceAAMP
{

public:
	/**
	 * @brief Get profiler bucket type
	 * @param mediaType Media type
	 * @param isInitializationSegment true for init segment
	 * @retval profiler bucket type
	 */
	ProfilerBucketType GetProfilerBucketForMedia(MediaType mediaType, bool isInitializationSegment)
	{
		switch (mediaType)
		{
		case eMEDIATYPE_VIDEO:
			return isInitializationSegment ? PROFILE_BUCKET_INIT_VIDEO : PROFILE_BUCKET_FRAGMENT_VIDEO;
		case eMEDIATYPE_AUDIO:
		default:
			return isInitializationSegment ? PROFILE_BUCKET_INIT_AUDIO : PROFILE_BUCKET_FRAGMENT_AUDIO;
		}
	}

	void Tune(const char *url, const char *contentType, bool bFirstAttempt = true, bool bFinalAttempt = false);
	void TuneHelper(TuneType tuneType);
	void TeardownStream(bool newTune);
	void SendMessageOverPipe(const char *str,int nToWrite);
	void SetupPipeSession();
	void ClosePipeSession();

	std::vector< std::pair<long long,long> > mAbrBitrateData;

	pthread_mutex_t mLock;// = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutexattr_t mMutexAttr;

	class StreamAbstractionAAMP *mpStreamAbstractionAAMP; // HLS or MPD collector
	StreamOutputFormat mFormat;
	StreamOutputFormat mAudioFormat;
	pthread_cond_t mDownloadsDisabled;
	bool mDownloadsEnabled;
	StreamSink* mStreamSink;

	ProfileEventAAMP profiler;
	bool licenceFromManifest;

	CURL *curl[MAX_CURL_INSTANCE_COUNT];

	// To store Set Cookie: headers and X-Reason headers in HTTP Response
	httpRespHeaderData httpRespHeaders[MAX_CURL_INSTANCE_COUNT];
	//std::string cookieHeaders[MAX_CURL_INSTANCE_COUNT]; //To store Set-Cookie: headers in HTTP response
	char manifestUrl[MAX_URI_LENGTH];

	bool mbDownloadsBlocked;
	bool streamerIsActive;
	bool mTSBEnabled;
	bool mIscDVR;
	pthread_t fragmentCollectorThreadID;
	double seek_pos_seconds; // indicates the playback position at which most recent playback activity began
	float rate; // most recent (non-zero) play rate for non-paused content
	bool pipeline_paused; // true if pipeline is paused
	char language[MAX_LANGUAGE_TAG_LENGTH];  // current language set
	char mLanguageList[MAX_LANGUAGE_COUNT][MAX_LANGUAGE_TAG_LENGTH]; // list of languages in stream
	int  mMaxLanguageCount;
	VideoZoomMode zoom_mode;
	bool video_muted;
	int audio_volume;
	std::vector<std::string> subscribedTags;
	std::vector<TimedMetadata> timedMetadata;

    /* START: Added As Part of DELIA-28363 and DELIA-28247 */
    bool IsTuneTypeNew;  /* Flag for the eTUNETYPE_NEW_NORMAL */
    /* END: Added As Part of DELIA-28363 and DELIA-28247 */

	long long trickStartUTCMS;
	long long playStartUTCMS;
	double durationSeconds;
	double culledSeconds;
	float maxRefreshPlaylistIntervalSecs;
	long long initialTuneTimeMs;
	AAMPEventListener* mEventListener;
	double mReportProgressPosn;
	long long mReportProgressTime;
	bool discardEnteringLiveEvt;
	bool mPlayingAd;
	double mAdPosition;
	char mAdUrl[MAX_URI_LENGTH];
	bool mIsRetuneInProgress;
	bool mEnableCache;
	pthread_cond_t mCondDiscontinuity;
	gint mDiscontinuityTuneOperationId;

	void CurlInit(int startIdx, unsigned int instanceCount);
	void SetCurlTimeout(long timeout, unsigned int instance = 0);
	void StoreLanguageList(int maxLangCount , char langlist[][MAX_LANGUAGE_TAG_LENGTH]);
	bool IsAudioLanguageSupported (const char *checkLanguage);
	void CurlTerm(int startIdx, unsigned int instanceCount);
	bool GetFile(const char *remoteUrl, struct GrowableBuffer *buffer, char effectiveUrl[MAX_URI_LENGTH], long *http_error = NULL, const char *range = NULL,unsigned int curlInstance = 0, bool resetBuffer = true,MediaType fileType = eMEDIATYPE_MANIFEST);
	char *LoadFragment( ProfilerBucketType bucketType, const char *fragmentUrl, size_t *len, unsigned int curlInstance = 0, const char *range = NULL,MediaType fileType = eMEDIATYPE_MANIFEST);
	bool LoadFragment( ProfilerBucketType bucketType, const char *fragmentUrl, struct GrowableBuffer *buffer, unsigned int curlInstance = 0, const char *range = NULL, MediaType fileType = eMEDIATYPE_MANIFEST, long * http_code = NULL);
	void PushFragment(MediaType mediaType, char *ptr, size_t len, double fragmentTime, double fragmentDuration);
	void PushFragment(MediaType mediaType, GrowableBuffer* buffer, double fragmentTime, double fragmentDuration);
	void EndOfStreamReached(MediaType mediaType);
	void EndTimeReached(MediaType mediaType);
	void InsertAd(const char *url, double positionSeconds);
	void AddEventListener(AAMPEventType eventType, AAMPEventListener* eventListener);
	void RemoveEventListener(AAMPEventType eventType, AAMPEventListener* eventListener);
	void SendEventAsync(const AAMPEvent &e);
	void SendErrorEvent(AAMPTuneFailure tuneFailure,const char *description = NULL);
	void SendDownloadErrorEvent(AAMPTuneFailure tuneFailure,long error_code);
	void SendEventSync(const AAMPEvent &e);
	void NotifySpeedChanged(float rate);
	void NotifyBitRateChangeEvent(int bitrate , const char *description ,int width ,int height, bool GetBWIndex = false);
	void NotifyEOSReached();
	void NotifyOnEnteringLive();

	/**
	 * @brief Get persisted profile index
	 * @retval index of persisted profile
	 */
	int  GetPersistedProfileIndex() {return mPersistedProfileIndex;}

	/**
	 * @brief Set persisted profile index
	 * @param profile index of persisted profile
	 */
	void SetPersistedProfileIndex(int profile){mPersistedProfileIndex = profile;}

	/**
	 * @brief Set persisted bandwidth
	 * @param bandwidth bandwidth to persist
	 */
	void SetPersistedBandwidth(long bandwidth) {mAvailableBandwidth = bandwidth;}

	/**
	 * @brief Get persisted bandwidth
	 * @retval persisted bandwidth
	 */
	long GetPersistedBandwidth(){return mAvailableBandwidth;}

	void UpdateDuration(double seconds);
	void UpdateCullingState(double culledSeconds);
	void UpdateRefreshPlaylistInterval(float maxIntervalSecs);
	void ReportProgress(void);
	long long GetDurationMs(void);
	long long GetPositionMs(void);
	void SendStream(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration);
	void SendStream(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double fDuration);
	void SetStreamSink(StreamSink* streamSink);
	bool IsLive(void);
	void Stop(void);

	/**
	 * @brief Check if TSB is enabled
	 * @retval true if TSB is enabled
	 */
	bool IsTSBSupported() { return mTSBEnabled;}

	/**
	 * @brief Check if stream is cDvr
	 * @retval true if stream is cDvr
	 */
	bool IsInProgressCDVR() {return (mContentType==ContentType_CDVR);}
	void ReportTimedMetadata(double timeMS, const char* szName, const char* szContent, int nb);
	void InterruptableMsSleep(int timeInMs);

#ifdef AAMP_HARVEST_SUPPORT_ENABLED

	bool HarvestFragments(bool modifyCount = true);
#endif

	bool DownloadsAreEnabled(void);
	void StopDownloads();
	void ResumeDownloads();
	void StopTrackDownloads(MediaType);
	void ResumeTrackDownloads(MediaType);
	void BlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track);
	void LogTuneComplete(void);
	void LogFirstFrame(void);
	void LogDrmInitComplete(void);
	void LogDrmDecryptBegin( ProfilerBucketType bucketType );
	void LogDrmDecryptEnd( ProfilerBucketType bucketType );

	/**
	 * @brief Get manifest url
	 * @retval Manifest Url
	 */
	char *GetManifestUrl(void)
	{
		char * url;
		if (mPlayingAd)
		{
			url = mAdUrl;
		}
		else
		{
			url = manifestUrl;
		}
		return url;
	}

	/**
	 * @brief Set manifest url of stream
	 * @param url URL to be set
	 */
	void SetManifestUrl(const char *url)
	{
		strncpy(manifestUrl, url, MAX_URI_LENGTH);
		manifestUrl[MAX_URI_LENGTH-1]='\0';
	}

	void NotifyFirstFrameReceived(void);
	void SyncBegin(void);
	void SyncEnd(void);
	double GetSeekBase(void);
	void ResetCurrentlyAvailableBandwidth(long bitsPerSecond,bool trickPlay,int profile=0);
	long GetCurrentlyAvailableBandwidth(void);
	void DisableDownloads(void);
	void EnableDownloads(void);


	/**
	 *   @brief Register event handler.
	 *
	 *   @param  eventListener - pointer to implementation of AAMPEventListener to receive events.
	 */
	void RegisterEvents(AAMPEventListener* eventListener)
	{
		mEventListener = eventListener;
	}

	void ScheduleRetune(PlaybackErrorType errorType, MediaType trackType);

	PrivateInstanceAAMP();
	~PrivateInstanceAAMP();

	void SetVideoRectangle(int x, int y, int w, int h);
	bool Discontinuity(MediaType);
	void SetVideoZoom(VideoZoomMode zoom);
	void SetVideoMute(bool muted);
	void SetAudioVolume(int volume);
	void SetState(PrivAAMPState state);
	void GetState(PrivAAMPState &state);
	static void AddIdleTask(IdleTask task, void* arg);
	bool IsSinkCacheEmpty(MediaType mediaType);
	void NotifyFragmentCachingComplete();
	bool SendTunedEvent();
	bool IsFragmentBufferingRequired();
	void GetPlayerVideoSize(int &w, int &h);
	void SetCallbackAsPending(gint id);
	void SetCallbackAsDispatched(gint id);
	void AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue);
	void SetLicenseServerURL(char* url);
	void SetAnonymousRequest(bool isAnonymous);
	void SetVODTrickplayFPS(int vodTrickplayFPS);
	void SetLinearTrickplayFPS(int linearTrickplayFPS);
	void InsertToPlaylistCache(const std::string url, const GrowableBuffer* buffer, const char* effectiveUrl);
	bool RetrieveFromPlaylistCache(const std::string url, GrowableBuffer* buffer, char effectiveUrl[]);
	void ClearPlaylistCache();
	void SetStallErrorCode(int errorCode);
	void SetStallTimeout(int timeoutMS);
	void SetReportInterval(int reportIntervalMS);
	void SendStalledErrorEvent();
	bool IsDiscontinuityProcessPending();
	void ProcessPendingDiscontinuity();
	void NotifyFirstBufferProcessed();
	void UpdateAudioLanguageSelection(const char *lang);
	int getStreamType();

	void setCurrentDrm(DRMSystems drm){mCurrentDrm = drm;}

	/**
	 * @brief Check if current  playback is from local TSB
	 * @retval true if local playback
	 */
	bool IsLocalPlayback() { return mIsLocalPlayback; }
	void GetMoneyTraceString(std::string &);
	void NotifyFirstFragmentDecrypted();
private:

	static void LazilyLoadConfigIfNeeded(void);
	void ScheduleEvent(struct AsyncEventDescriptor* e);
	void SetContentType(const char *url, const char *contentType);
	ListenerData* mEventListeners[AAMP_MAX_NUM_EVENTS];
	TuneType lastTuneType;
	int m_fd;
	bool mTuneCompleted;
	bool mFirstTune;			//To identify the first tune after load.
	int  mTuneAttempts;			//To distinguish between new tune & retries with redundant over urls.
	long long mPlayerLoadTime;
	PrivAAMPState mState;
	long long lastUnderFlowTimeMs[AAMP_TRACK_COUNT];
	bool mbTrackDownloadsBlocked[AAMP_TRACK_COUNT];
	bool mIsDash;
	DRMSystems mCurrentDrm;
	int  mPersistedProfileIndex;
	long mAvailableBandwidth;
	bool mProcessingDiscontinuity;
	bool mDiscontinuityTuneOperationInProgress;
	bool mProcessingAdInsertion;
	ContentType mContentType;
	bool mTunedEventPending;
	bool mSeekOperationInProgress;
	std::unordered_map<std::string, std::pair<GrowableBuffer*, char*>> mPlaylistCache;
	std::map<gint, bool> mPendingAsyncEvents;
	std::unordered_map<std::string, std::vector<std::string>> mCustomHeaders;
	bool mIsFirstRequestToFOG;
	bool mIsLocalPlayback; /** indicates if the playback is from FOG(TSB/IP-DVR) */
};

#endif // PRIVAAMP_H
