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
 * @file AampProfiler.cpp
 * @brief ProfileEventAAMP class impl
 */

#include "AampProfiler.h"
#include "AampConstants.h"
#include "AampUtils.h"
#include "AampConfig.h"

#include <algorithm>

#define MAX std::max

/**
 * @brief ProfileEventAAMP Constructor
 */
ProfileEventAAMP::ProfileEventAAMP():
	tuneStartMonotonicBase(0), tuneStartBaseUTCMS(0), bandwidthBitsPerSecondVideo(0),
        bandwidthBitsPerSecondAudio(0), drmErrorCode(0), enabled(false), xreTimeBuckets(), tuneEventList(),
	tuneEventListMtx(), mTuneFailBucketType(PROFILE_BUCKET_MANIFEST), mTuneFailErrorCode(0),mLogObj(NULL)
{

}

/**
 *  @brief Get tune time events in JSON format
 */
void ProfileEventAAMP::getTuneEventsJSON(std::string &outStr, const std::string &streamType, const char *url, bool success)
{
	bool siblingEvent = false;
	unsigned int tEndTime = NOW_STEADY_TS_MS;
	unsigned int td = tEndTime - tuneStartMonotonicBase;
	size_t end = 0;

	std::string temlUrl = url;
	end = temlUrl.find("?");

	if (end != std::string::npos)
	{
		temlUrl = temlUrl.substr(0, end);
	}

	char outPtr[512];
	memset(outPtr, '\0', 512);

	snprintf(outPtr, 512, "{\"s\":%lld,\"td\":%d,\"st\":\"%s\",\"u\":\"%s\",\"tf\":{\"i\":%d,\"er\":%d},\"r\":%d,\"v\":[",tuneStartBaseUTCMS, td, streamType.c_str(), temlUrl.c_str(), mTuneFailBucketType, mTuneFailErrorCode, (success ? 1 : 0));

	outStr.append(outPtr);

	std::lock_guard<std::mutex> lock(tuneEventListMtx);
	for(auto &te:tuneEventList)
	{
		if(siblingEvent)
		{
			outStr.append(",");
		}
		char eventPtr[256];
		memset(eventPtr, '\0', 256);
		snprintf(eventPtr, 256, "{\"i\":%d,\"b\":%d,\"d\":%d,\"o\":%d}", te.id, te.start, te.duration, te.result);
		outStr.append(eventPtr);

		siblingEvent = true;
	}
	outStr.append("]}");

	tuneEventList.clear();
	mTuneFailErrorCode = 0;
	mTuneFailBucketType = PROFILE_BUCKET_MANIFEST;
}

/**
 *  @brief Profiler method to perform tune begin related operations.
 */
void ProfileEventAAMP::TuneBegin(void)
{ // start tune
	memset(buckets, 0, sizeof(buckets));
	tuneStartBaseUTCMS = NOW_SYSTEM_TS_MS;
	tuneStartMonotonicBase = NOW_STEADY_TS_MS;
	bandwidthBitsPerSecondVideo = 0;
	bandwidthBitsPerSecondAudio = 0;
	drmErrorCode = 0;
	enabled = true;
	mTuneFailBucketType = PROFILE_BUCKET_MANIFEST;
	mTuneFailErrorCode = 0;
	tuneEventList.clear();
}

/**
 * @brief Logging performance metrics after successful tune completion. Metrics starts with IP_AAMP_TUNETIME
 *
 * <h4>Format of IP_AAMP_TUNETIME:</h4>
 * version,                       // version for this protocol, initially zero<br>
 * build,                         // incremented when there are significant player changes/optimizations<br>
 * tunestartUtcMs,                // when tune logically started from AAMP perspective<br>
 * <br>
 * ManifestDownloadStartTime,     // offset in milliseconds from tunestart when main manifest begins download<br>
 * ManifestDownloadTotalTime,     // time (ms) taken for main manifest download, relative to ManifestDownloadStartTime<br>
 * ManifestDownloadFailCount,     // if >0 ManifestDownloadTotalTime spans multiple download attempts<br>
 * <br>
 * PlaylistDownloadStartTime,     // offset in milliseconds from tunestart when playlist subManifest begins download<br>
 * PlaylistDownloadTotalTime,     // time (ms) taken for playlist subManifest download, relative to PlaylistDownloadStartTime<br>
 * PlaylistDownloadFailCount,     // if >0 otherwise PlaylistDownloadTotalTime spans multiple download attempts<br>
 * <br>
 * InitFragmentDownloadStartTime, // offset in milliseconds from tunestart when init fragment begins download<br>
 * InitFragmentDownloadTotalTime, // time (ms) taken for fragment download, relative to InitFragmentDownloadStartTime<br>
 * InitFragmentDownloadFailCount, // if >0 InitFragmentDownloadTotalTime spans multiple download attempts<br>
 * <br>
 * Fragment1DownloadStartTime,    // offset in milliseconds from tunestart when fragment begins download<br>
 * Fragment1DownloadTotalTime,    // time (ms) taken for fragment download, relative to Fragment1DownloadStartTime<br>
 * Fragment1DownloadFailCount,    // if >0 Fragment1DownloadTotalTime spans multiple download attempts<br>
 * Fragment1Bandwidth,            // intrinsic bitrate of downloaded fragment<br>
 * <br>
 * drmLicenseRequestStart,        // offset in milliseconds from tunestart<br>
 * drmLicenseRequestTotalTime,    // time (ms) for license acquisition relative to drmLicenseRequestStart<br>
 * drmFailErrorCode,              // nonzero if drm license acquisition failed during tuning<br>
 * <br>
 * LAPreProcDuration,             // License acquisition pre-processing duration in ms<br>
 * LANetworkDuration,             // License acquisition network duration in ms<br>
 * LAPostProcDuration,            // License acquisition post-processing duration in ms<br>
 * <br>
 * VideoDecryptDuration,          // Video fragment decrypt duration in ms<br>
 * AudioDecryptDuration,          // Audio fragment decrypt duration in ms<br>
 * <br>
 * gstStart,                      // offset in ms from tunestart when pipeline creation/setup begins<br>
 * gstFirstFrame,                 // offset in ms from tunestart when first frame of video is decoded/presented<br>
 * contentType,                   //Playback Mode. Values: CDVR, VOD, LINEAR, IVOD, EAS, CAMERA, DVR, MDVR, IPDVR, PPV<br>
 * streamType,                    //Stream Type. Values: 10-HLS/Clear, 11-HLS/Consec, 12-HLS/Access, 13-HLS/Vanilla AES, 20-DASH/Clear, 21-DASH/WV, 22-DASH/PR<br>
 * firstTune                      //First tune after reboot/crash<br>
 * Prebuffered                    //If the Player was in preBuffer(BG) mode)<br>
 * PreBufferedTime                //Player spend Time in BG<br> 
 * success                        //Tune status
 * contentType                    //Content Type. Eg: LINEAR, VOD, etc
 * streamType                     //Stream Type. Eg: HLS, DASH, etc
 * firstTune                      //Is it a first tune after reboot/crash.
 * <br>
 */
void ProfileEventAAMP::TuneEnd(TuneEndMetrics &mTuneEndMetrics,std::string appName, std::string playerActiveMode, int playerId, bool playerPreBuffered, unsigned int durationSeconds, bool interfaceWifi,std::string failureReason)
{
	if(!enabled )
	{
		return;
	}
	enabled = false;
	unsigned int licenseAcqNWTime = bucketDuration(PROFILE_BUCKET_LA_NETWORK);
	if(licenseAcqNWTime == 0) licenseAcqNWTime = bucketDuration(PROFILE_BUCKET_LA_TOTAL); //A HACK for HLS

	char tuneTimeStrPrefix[64];
	memset(tuneTimeStrPrefix, '\0', sizeof(tuneTimeStrPrefix));
	int mTotalTime;
 	int mTimedMetadataStartTime = static_cast<int> (mTuneEndMetrics.mTimedMetadataStartTime - tuneStartMonotonicBase);
	if (mTuneEndMetrics.success > 0)
	{
		mTotalTime = playerPreBuffered ? buckets[PROFILE_BUCKET_FIRST_FRAME].tStart - buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : buckets[PROFILE_BUCKET_FIRST_FRAME].tStart;
	}
	else
	{
		mTotalTime = static_cast<int> (mTuneEndMetrics.mTotalTime - tuneStartMonotonicBase);
	}
	if (!appName.empty())
	{
		snprintf(tuneTimeStrPrefix, sizeof(tuneTimeStrPrefix), "%s PLAYER[%d] APP: %s IP_AAMP_TUNETIME", playerActiveMode.c_str(),playerId,appName.c_str());
	}
	else
	{
		snprintf(tuneTimeStrPrefix, sizeof(tuneTimeStrPrefix), "%s PLAYER[%d] IP_AAMP_TUNETIME", playerActiveMode.c_str(),playerId);
	}

	AAMPLOG_WARN("%s:%d,%s,%lld," // prefix, version, build, tuneStartBaseUTCMS
		"%d,%d,%d,"		// main manifest (start,total,err)
		"%d,%d,%d,"		// video playlist (start,total,err)
		"%d,%d,%d,"		// audio playlist (start,total,err)

		"%d,%d,%d,"		// video init-segment (start,total,err)
		"%d,%d,%d,"		// audio init-segment (start,total,err)

		"%d,%d,%d,%ld,"	// video fragment (start,total,err, bitrate)
		"%d,%d,%d,%ld,"	// audio fragment (start,total,err, bitrate)

		"%d,%d,%d,"		// licenseAcqStart, licenseAcqTotal, drmFailErrorCode
		"%d,%d,%d,"		// LAPreProcDuration, LANetworkDuration, LAPostProcDuration

		"%d,%d,"		// VideoDecryptDuration, AudioDecryptDuration
		"%d,%d,"		// gstPlayStartTime, gstFirstFrameTime
		"%d,%d,%d,"		// contentType, streamType, firstTune
		"%d,%d,"		// If Player was in prebufferd mode, time spent in prebufferd(BG) mode
		"%d,%d,"		// Asset duration in seconds, Connection is wifi or not - wifi(1) ethernet(0)
		"%d,%d,%s,%s,"		// TuneAttempts ,Tunestatus -success(1) failure (0) ,Failure Reason, AppName
		"%d,%d,%d,%d,%d",       // TimedMetadata (count,start,total) ,TSBEnabled or not - enabled(1) not enabled(0)
					//  TotalTime -for failure and interrupt tune -it is time at which failure /interrupt reported	
		// TODO: settop type, flags, isFOGEnabled, isDDPlus, isDemuxed, assetDurationMs

		tuneTimeStrPrefix,
		AAMP_TUNETIME_VERSION, // version for this protocol, initially zero
		AAMP_VERSION, // build - incremented when there are significant player changes/optimizations
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

		(playerPreBuffered && mTuneEndMetrics.success > 0) ? buckets[PROFILE_BUCKET_FIRST_BUFFER].tStart - buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : buckets[PROFILE_BUCKET_FIRST_BUFFER].tStart, // gstPlaying: offset in ms from tunestart when pipeline first fed data
		(playerPreBuffered && mTuneEndMetrics.success > 0) ? buckets[PROFILE_BUCKET_FIRST_FRAME].tStart - buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : buckets[PROFILE_BUCKET_FIRST_FRAME].tStart,  // gstFirstFrame: offset in ms from tunestart when first frame of video is decoded/presented
		mTuneEndMetrics.contentType,mTuneEndMetrics.streamType,mTuneEndMetrics.mFirstTune,
		playerPreBuffered,playerPreBuffered ? buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : 0,
		durationSeconds,interfaceWifi,
		mTuneEndMetrics.mTuneAttempts, mTuneEndMetrics.success,failureReason.c_str(),appName.c_str(),
		mTuneEndMetrics.mTimedMetadata,mTimedMetadataStartTime < 0 ? 0 : mTimedMetadataStartTime , mTuneEndMetrics.mTimedMetadataDuration,mTuneEndMetrics.mTSBEnabled,mTotalTime
		);	
}

/**
 *  @brief Method converting the AAMP style tune performance data to IP_EX_TUNETIME style data
 */
void ProfileEventAAMP::GetClassicTuneTimeInfo(bool success, int tuneRetries, int firstTuneType, long long playerLoadTime, int streamType, bool isLive,unsigned int durationinSec, char *TuneTimeInfoStr)
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
	unsigned int fragmentBucketTime                 =       (unsigned int)(fragmentReadyTime - xreTimeBuckets[TuneTimePlay]) ;
	unsigned int decoderStreamingBucketTime         =       xreTimeBuckets[TuneTimeStreaming] - xreTimeBuckets[TuneTimeStartStream];
	/*Note: 'Drm Ready' to 'decrypt start' gap is not covered in any of the buckets.*/

	unsigned int manifestTotal      =       bucketDuration(PROFILE_BUCKET_MANIFEST);
	unsigned int profilesTotal      =       effectiveBucketTime(PROFILE_BUCKET_PLAYLIST_VIDEO, PROFILE_BUCKET_PLAYLIST_AUDIO);
	unsigned int initFragmentTotal  =       effectiveBucketTime(PROFILE_BUCKET_INIT_VIDEO, PROFILE_BUCKET_INIT_AUDIO);
	unsigned int fragmentTotal      =       effectiveBucketTime(PROFILE_BUCKET_FRAGMENT_VIDEO, PROFILE_BUCKET_FRAGMENT_AUDIO);
	// DrmReadyBucketTime is licenseTotal, time taken for complete license acquisition
	// licenseNWTime is the time taken for network request.
	unsigned int licenseTotal       =       bucketDuration(PROFILE_BUCKET_LA_TOTAL);
	unsigned int licenseNWTime      =       bucketDuration(PROFILE_BUCKET_LA_NETWORK);
	if(licenseNWTime == 0)
	{
		licenseNWTime = licenseTotal;  //A HACK for HLS
	}

	// Total Network Time
	unsigned int networkTime = manifestTotal + profilesTotal + initFragmentTotal + fragmentTotal + licenseNWTime;

	snprintf(TuneTimeInfoStr,AAMP_MAX_PIPE_DATA_SIZE,"%d,%lld,%d,%d," //totalNetworkTime, playerLoadTime , failRetryBucketTime, prepareToPlayBucketTime,
			"%d,%d,%d,"                                             //playBucketTime ,licenseTotal , decoderStreamingBucketTime
			"%d,%d,%d,%d,"                                          // manifestTotal,profilesTotal,fragmentTotal,effectiveFragmentDLTime
			"%d,%d,%d,%d,"                                          // licenseNWTime,success,durationinMilliSec,isLive
			"%lld,%lld,%lld,"                                       // TuneTimeBeginLoad,TuneTimePrepareToPlay,TuneTimePlay,
			"%lld,%lld,%lld,"                                       //TuneTimeDrmReady,TuneTimeStartStream,TuneTimeStreaming
			"%d,%d,%d,%lld",                                             //streamType, tuneRetries, TuneType, TuneCompleteTime(UTC MSec)
			networkTime,playerLoadTime, failRetryBucketTime, prepareToPlayBucketTime,playBucketTime,licenseTotal,decoderStreamingBucketTime,
			manifestTotal,profilesTotal,(initFragmentTotal + fragmentTotal),fragmentBucketTime, licenseNWTime,success,durationinSec*1000,isLive,
			xreTimeBuckets[TuneTimeBeginLoad],xreTimeBuckets[TuneTimePrepareToPlay],xreTimeBuckets[TuneTimePlay] ,xreTimeBuckets[TuneTimeDrmReady],
			xreTimeBuckets[TuneTimeStartStream],xreTimeBuckets[TuneTimeStreaming],streamType,tuneRetries,firstTuneType,(long long)NOW_SYSTEM_TS_MS
	);
#ifndef CREATE_PIPE_SESSION_TO_XRE
	AAMPLOG_WARN("AAMP=>XRE: %s", TuneTimeInfoStr);
#endif
}

/**
 * @brief Marking the beginning of a bucket
 */
void ProfileEventAAMP::ProfileBegin(ProfilerBucketType type)
{
	struct ProfilerBucket *bucket = &buckets[type];
	if (!bucket->complete && (0==bucket->tStart))	//No other Begin should record before the End
	{
		bucket->tStart 		= NOW_STEADY_TS_MS - tuneStartMonotonicBase;
		bucket->tFinish 	= bucket->tStart;
		bucket ->profileStarted = true;
	}
}

/**
 *  @brief Marking error while executing a bucket
 */
void ProfileEventAAMP::ProfileError(ProfilerBucketType type, int result)
{
	struct ProfilerBucket *bucket = &buckets[type];
	if (!bucket->complete && bucket->profileStarted)
	{
		SetTuneFailCode(result, type);
		bucket->errorCount++;

	}
}

/**
 *  @brief Marking the end of a bucket
 */
void ProfileEventAAMP::ProfileEnd(ProfilerBucketType type)
{
	struct ProfilerBucket *bucket = &buckets[type];
	if (!bucket->complete && bucket->profileStarted)
	{
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

		logprintf("aamp %7d (+%6d): %s",
		bucket->tStart,
		bucket->tFinish - bucket->tStart,
		bucketName[type]);
		*/
		bucket->complete = true;
	}
}

/**
 *  @brief Method to mark the end of a bucket, for which beginning is not marked
 */
void ProfileEventAAMP::ProfilePerformed(ProfilerBucketType type)
{
	ProfileBegin(type);
	buckets[type].complete = true;
}

/**
 *  @brief Method to set Failure code and Bucket Type used for microevents
 */
void ProfileEventAAMP::SetTuneFailCode(int tuneFailCode, ProfilerBucketType failBucketType)
{
	if(!mTuneFailErrorCode)
	{
		AAMPLOG_INFO("Tune Fail: ProfilerBucketType: %d, tuneFailCode: %d", failBucketType, tuneFailCode);
		mTuneFailErrorCode = tuneFailCode;
		mTuneFailBucketType = failBucketType;
	}
}

