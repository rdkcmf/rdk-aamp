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
 * @file AampProfiler.h
 * @brief ProfileEventAAMP header file
 */

#ifndef __AAMP_PROFILER_H__
#define __AAMP_PROFILER_H__

#include <mutex>
#include <list>
#include <sstream>
#include <string>
#include "AampLogManager.h"

/**
 * @addtogroup AAMP_COMMON_TYPES
 * @{
 */

/**
 * @brief Bucket types of AAMP profiler
 */
typedef enum
{
	PROFILE_BUCKET_MANIFEST,            /**< Manifest download bucket*/

	PROFILE_BUCKET_PLAYLIST_VIDEO,      /**< Video playlist download bucket*/
	PROFILE_BUCKET_PLAYLIST_AUDIO,      /**< Audio playlist download bucket*/
	PROFILE_BUCKET_PLAYLIST_SUBTITLE,   /**< Subtitle playlist download bucket*/
	PROFILE_BUCKET_PLAYLIST_AUXILIARY,  /**< Auxiliary playlist download bucket*/

	PROFILE_BUCKET_INIT_VIDEO,          /**< Video init fragment download bucket*/
	PROFILE_BUCKET_INIT_AUDIO,          /**< Audio init fragment download bucket*/
	PROFILE_BUCKET_INIT_SUBTITLE,       /**< Subtitle fragment download bucket*/
	PROFILE_BUCKET_INIT_AUXILIARY,      /**< Auxiliary fragment download bucket*/

	PROFILE_BUCKET_FRAGMENT_VIDEO,      /**< Video fragment download bucket*/
	PROFILE_BUCKET_FRAGMENT_AUDIO,      /**< Audio fragment download bucket*/
	PROFILE_BUCKET_FRAGMENT_SUBTITLE,   /**< Subtitle fragment download bucket*/
	PROFILE_BUCKET_FRAGMENT_AUXILIARY,  /**< Auxiliary fragment download bucket*/

	PROFILE_BUCKET_DECRYPT_VIDEO,       /**< Video decryption bucket*/
	PROFILE_BUCKET_DECRYPT_AUDIO,       /**< Audio decryption bucket*/
	PROFILE_BUCKET_DECRYPT_SUBTITLE,    /**< Subtitle decryption bucket*/
	PROFILE_BUCKET_DECRYPT_AUXILIARY,   /**< Auxiliary decryption bucket*/

	PROFILE_BUCKET_LA_TOTAL,            /**< License acquisition total bucket*/
	PROFILE_BUCKET_LA_PREPROC,          /**< License acquisition pre-processing bucket*/
	PROFILE_BUCKET_LA_NETWORK,          /**< License acquisition network operation bucket*/
	PROFILE_BUCKET_LA_POSTPROC,         /**< License acquisition post-processing bucket*/

	PROFILE_BUCKET_FIRST_BUFFER,        /**< First buffer to gstreamer bucket*/
	PROFILE_BUCKET_FIRST_FRAME,         /**< First frame displayed bucket*/
	PROFILE_BUCKET_PLAYER_PRE_BUFFERED, /**< Prebuffer bucket ( BG to FG )*/
	PROFILE_BUCKET_TYPE_COUNT           /**< Bucket count*/
} ProfilerBucketType;

/**
 * @brief Bucket types of classic profiler
 */
typedef enum
{
	TuneTimeBaseTime,           /**< Tune time base*/
	TuneTimeBeginLoad,          /**< Player load time*/
	TuneTimePrepareToPlay,      /**< Manifest ready time*/
	TuneTimePlay,               /**< Profiles ready time*/
	TuneTimeDrmReady,           /**< DRM ready time*/
	TuneTimeStartStream,        /**< First buffer insert time*/
	TuneTimeStreaming,          /**< First frame display time*/
	TuneTimeBackToXre,          /**< Tune status back to XRE time*/
	TuneTimeMax                 /**< Max bucket type*/
} ClassicProfilerBucketType;

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
	ContentType_IPDVR,          /**< 9 - IPDVR */
	ContentType_PPV,            /**< 10 - PPV */
	ContentType_OTT,            /**< 11 - OTT */
	ContentType_OTA,            /**< 12 - OTA*/
	ContentType_HDMIIN,         /**< 13 - HDMI Input */
	ContentType_COMPOSITEIN,    /**< 14 - COMPOSITE Input*/
	ContentType_SLE,            /**< 15 - SLE - Single Live Event (kind of iVOD)*/
	ContentType_MAX             /**< 16 - Type Count*/
};

/**
 * @}
 */

/**
 * @brief Class for AAMP event Profiling
 */
class ProfileEventAAMP
{
private:
	// TODO: include settop type (to distinguish settop performance)
	// TODO: include flag to indicate whether FOG used (to isolate FOG overhead)

	/**
	 * @brief Class corresponding to tune time events.
	 */
	class TuneEvent
	{
	public:
		ProfilerBucketType id;      /**< Event identifier */
		unsigned int start;         /**< Event start time */
		unsigned int duration;      /**< Event duration */
		int result;                 /**< Event result */

		/**
		 * @brief TuneEvent Constructor
		 * @param[in] i - Event id
		 * @param[in] s - Event start time
		 * @param[in] d - Event duration
		 * @param[in] r - Event result
		 */
		TuneEvent(ProfilerBucketType i, unsigned int s, unsigned int d, int r):
			id(i), start(s), duration(d), result(r)
		{
		}
	};

	/**
	 * @brief Data structure corresponding to profiler bucket
	 */
	struct ProfilerBucket
	{
		unsigned int tStart;    /**< Relative start time of operation, based on tuneStartMonotonicBase */
		unsigned int tFinish;   /**< Relative end time of operation, based on tuneStartMonotonicBase */
		int errorCount;         /**< non-zero if errors/retries occured during this operation */
		bool complete;          /**< true if this step already accounted for, and further profiling should be ignored */
	} buckets[PROFILE_BUCKET_TYPE_COUNT];

	/**
	 * @brief Calculating effecting duration of overlapping buckets, id1 & id2
	 */
#define bucketsOverlap(id1,id2) \
		buckets[id1].complete && buckets[id2].complete && \
		(buckets[id1].tStart <= buckets[id2].tFinish) && (buckets[id2].tStart <= buckets[id1].tFinish)

	/**
	 * @brief Calculating total duration a bucket id
	 */
#define bucketDuration(id) \
		(buckets[id].complete?(buckets[id].tFinish - buckets[id].tStart):0)

	long long tuneStartMonotonicBase;       /**< Base time from Monotonic clock for interval calculation */

	long long tuneStartBaseUTCMS;           /**< common UTC base for start of tune */
	long long xreTimeBuckets[TuneTimeMax];  /**< Start time of each buckets for classic metrics conversion */
	long bandwidthBitsPerSecondVideo;       /**< Video bandwidth in bps */
	long bandwidthBitsPerSecondAudio;       /**< Audio bandwidth in bps */
	int drmErrorCode;                       /**< DRM error code */
	bool enabled;                           /**< Profiler started or not */
	std::list<TuneEvent> tuneEventList;     /**< List of events happened during tuning */
	std::mutex tuneEventListMtx;            /**< Mutex protecting tuneEventList */

	ProfilerBucketType mTuneFailBucketType;  /* ProfilerBucketType in case of error */
	int mTuneFailErrorCode;			/* tune Fail Error Code */
	AampLogManager *mLogObj;

	/**
	 * @brief Calculating effective time of two overlapping buckets.
	 *
	 * @param[in] id1 - Bucket type 1
	 * @param[in] id2 - Bucket type 2
	 * @return void
	 */
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
	ProfileEventAAMP();

	/**
	 * @brief ProfileEventAAMP Destructor
	 */
	~ProfileEventAAMP(){}

	ProfileEventAAMP(const ProfileEventAAMP&) = delete;
	ProfileEventAAMP& operator=(const ProfileEventAAMP&) = delete;

	/**
	 * @brief Setting video bandwidth in bps
	 *
	 * @param[in] bw - Bandwidth in bps
	 * @return void
	 */
	void SetBandwidthBitsPerSecondVideo(long bw)
	{
		bandwidthBitsPerSecondVideo = bw;
	}

	/**
	 * @brief Setting audio bandwidth in bps
	 *
	 * @param[in] bw - Bandwidth in bps
	 * @return void
	 */
	void SetBandwidthBitsPerSecondAudio(long bw)
	{
		bandwidthBitsPerSecondAudio = bw;
	}

	/**
	 * @brief Setting DRM error code
	 *
	 * @param[in] errCode - Error code
	 * @return void
	 */
	void SetDrmErrorCode(int errCode)
	{
		drmErrorCode = errCode;
	}


	/**
	 * @brief Get tune time events in JSON format
	 *
	 * @param[out] outSS - Output JSON string
	 * @param[in] streamType - Stream type
	 * @param[in] url - Tune URL
	 * @param[in] success - Tune success/failure
	 * @return void
	 */
	void getTuneEventsJSON(std::string &outSS, const std::string &streamType, const char *url, bool success);

	/**
	 * @brief Profiler method to perform tune begin related operations.
	 *
	 * @return void
	 */
	void TuneBegin(void);

	/**
	 * @brief Logging performance metrics after successful tune completion. Metrics starts with IP_AAMP_TUNETIME
	 *
	 * <h4>Format of IP_AAMP_TUNETIME:</h4>
	 * version,	// version for this protocol, initially zero<br>
	 * build,		// incremented when there are significant player changes/optimizations<br>
	 * tunestartUtcMs,	// when tune logically started from AAMP perspective<br>
	 * <br>
	 * ManifestDownloadStartTime,  // offset in milliseconds from tunestart when main manifest begins download<br>
	 * ManifestDownloadTotalTime,  // time (ms) taken for main manifest download, relative to ManifestDownloadStartTime<br>
	 * ManifestDownloadFailCount,  // if >0 ManifestDownloadTotalTime spans multiple download attempts<br>
	 * <br>
	 * PlaylistDownloadStartTime,  // offset in milliseconds from tunestart when playlist subManifest begins download<br>
	 * PlaylistDownloadTotalTime,  // time (ms) taken for playlist subManifest download, relative to PlaylistDownloadStartTime<br>
	 * PlaylistDownloadFailCount,  // if >0 otherwise PlaylistDownloadTotalTime spans multiple download attempts<br>
	 * <br>
	 * InitFragmentDownloadStartTime, // offset in milliseconds from tunestart when init fragment begins download<br>
	 * InitFragmentDownloadTotalTime, // time (ms) taken for fragment download, relative to InitFragmentDownloadStartTime<br>
	 * InitFragmentDownloadFailCount, // if >0 InitFragmentDownloadTotalTime spans multiple download attempts<br>
	 * <br>
	 * Fragment1DownloadStartTime, // offset in milliseconds from tunestart when fragment begins download<br>
	 * Fragment1DownloadTotalTime, // time (ms) taken for fragment download, relative to Fragment1DownloadStartTime<br>
	 * Fragment1DownloadFailCount, // if >0 Fragment1DownloadTotalTime spans multiple download attempts<br>
	 * Fragment1Bandwidth,	    	// intrinsic bitrate of downloaded fragment<br>
	 * <br>
	 * drmLicenseRequestStart,	    // offset in milliseconds from tunestart<br>
	 * drmLicenseRequestTotalTime, // time (ms) for license acquisition relative to drmLicenseRequestStart<br>
	 * drmFailErrorCode,           // nonzero if drm license acquisition failed during tuning<br>
	 * <br>
	 * LAPreProcDuration,	    	// License acquisition pre-processing duration in ms<br>
	 * LANetworkDuration, 			// License acquisition network duration in ms<br>
	 * LAPostProcDuration,         // License acquisition post-processing duration in ms<br>
	 * <br>
	 * VideoDecryptDuration,		// Video fragment decrypt duration in ms<br>
	 * AudioDecryptDuration,		// Audio fragment decrypt duration in ms<br>
	 * <br>
	 * gstStart,	// offset in ms from tunestart when pipeline creation/setup begins<br>
	 * gstFirstFrame,  // offset in ms from tunestart when first frame of video is decoded/presented<br>
	 * <br>
	 * contentType, 	//Playback Mode. Values: CDVR, VOD, LINEAR, IVOD, EAS, CAMERA, DVR, MDVR, IPDVR, PPV<br>
	 * streamType, 	//Stream Type. Values: 10-HLS/Clear, 11-HLS/Consec, 12-HLS/Access, 13-HLS/Vanilla AES, 20-DASH/Clear, 21-DASH/WV, 22-DASH/PR<br>
	 * firstTune		//First tune after reboot/crash<br>
 	 * Prebuffered		//If the Player was in preBuffer(BG) mode)<br>
	 * PreBufferedTime		//Player spend Time in BG<br>
	 * @param[in] success - Tune status
	 * @param[in] contentType - Content Type. Eg: LINEAR, VOD, etc
	 * @param[in] streamType - Stream Type. Eg: HLS, DASH, etc
	 * @param[in] firstTune - Is it a first tune after reboot/crash.
	 * @param[in] durationSeconds - Asset duration in seconds
	 * @param[in] interfaceWifi - Active connection is Wifi or Ethernet.
	 * @return void
	 */
	void TuneEnd(bool success, ContentType contentType, int streamType, bool firstTune, std::string appName, std::string playerActiveMode, int playerId, bool playerPreBuffered, unsigned int durationSeconds, bool interfaceWifi);

	/**
	 * @brief Method converting the AAMP style tune performance data to IP_EX_TUNETIME style data
	 *
	 * @param[in] success - Tune status
	 * @param[in] tuneRetries - Number of tune attempts
	 * @param[in] playerLoadTime - Time at which the first tune request reached the AAMP player
	 * @param[in] streamType - Type of stream. eg: HLS, DASH, etc
	 * @param[in] isLive  - Live channel or not
	 * @param[in] durationinSec - Asset duration in seconds
	 * @param[out] TuneTimeInfoStr - Formatted output string
	 * @return void
	 */
	void GetClassicTuneTimeInfo(bool success, int tuneRetries, int firstTuneType, long long playerLoadTime, int streamType, bool isLive,unsigned int durationinSec, char *TuneTimeInfoStr);

	/**
	 * @brief Marking the beginning of a bucket
	 *
	 * @param[in] type - Bucket type
	 * @return void
	 */
	void ProfileBegin(ProfilerBucketType type);

	/**
	 * @brief Marking error while executing a bucket
	 *
	 * @param[in] type - Bucket type
	 * @param[in] result - Error code
	 * @return void
	 */
	void ProfileError(ProfilerBucketType type, int result = -1);

	/**
	 * @brief Marking the end of a bucket
	 *
	 * @param[in] type - Bucket type
	 * @return void
	 */
	void ProfileEnd(ProfilerBucketType type);

	/**
	 * @brief Method to mark the end of a bucket, for which beginning is not marked
	 *
	 * @param[in] type - Bucket type
	 * @return void
	 */
	void ProfilePerformed(ProfilerBucketType type);

	/**
	 * @brief Method to set Failure code and Bucket Type used for microevents
	 *
	 * @param[in] type - tune Fail Code
	 * @param[in] type - Bucket type
	 * @return void
	 */
	void SetTuneFailCode(int tuneFailCode, ProfilerBucketType failBucketType);
	
	void SetLogger(AampLogManager *logObj) { mLogObj = logObj;}
};

#endif /* __AAMP_PROFILER_H__ */

