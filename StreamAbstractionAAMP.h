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

#ifndef STREAMABSTRACTIONAAMP_H
#define STREAMABSTRACTIONAAMP_H

#include "priv_aamp.h"
#include <map>
#include <iterator>
#include <glib.h>

#define MAX_CACHED_FRAGMENTS_PER_TRACK 3

typedef enum
{
	eTRACK_VIDEO,
	eTRACK_AUDIO
} TrackType;

struct CachedFragment
{
	GrowableBuffer fragment;
	double position;
	double duration;
	bool discontinuity;
	int profileIndex; /**< Updated internally*/
#ifdef AAMP_DEBUG_FETCH_INJECT
	char uri[MAX_URI_LENGTH];
#endif
};

typedef enum
{
	ePLAYLISTTYPE_UNDEFINED,
	ePLAYLISTTYPE_EVENT, // playlist may grow via appended lines, but otherwise won't change
	ePLAYLISTTYPE_VOD, // playlist will never change
} PlaylistType;

enum BufferHealthStatus
{
	BUFFER_STATUS_GREEN, /**< healthy state, where buffering is (close) to being maxed out*/
	BUFFER_STATUS_YELLOW, /**< danger  state, where buffering is (close) to being exhausted
						(presenting final fragment of video with no 'on deck' content queued up)*/
	BUFFER_STATUS_RED /**< failed state, where buffers have run dry, and player experiences underrun/stalled video*/
};

class MediaTrack
{
public:
	MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name);
	virtual ~MediaTrack();
	void StartInjectLoop();
	void StopInjectLoop();
	bool Enabled();
	bool InjectFragment();
	double GetTotalInjectedDuration() { return totalInjectedDuration; };
	void RunInjectLoop();
	void UpdateTSAfterFetch();
	bool WaitForFreeFragmentAvailable( int timeoutMs = -1);
	void AbortWaitForCachedFragment( bool immediate);
	virtual void ABRProfileChanged(void) = 0;
	int GetTotalFragmentsFetched(){ return totalFragmentsDownloaded; }
	CachedFragment* GetFetchBuffer(bool initialize);
	void SetCurrentBandWidth(int bandwidthBps);
	int GetCurrentBandWidth();
	void MonitorBufferHealth();
	void ScheduleBufferHealthMonitor();
	BufferHealthStatus GetBufferHealthStatus() { return bufferStatus; };
protected:
	void UpdateTSAfterInject();
	bool WaitForCachedFragmentAvailable();
	virtual class StreamAbstractionAAMP* GetContext() = 0;
	virtual void InjectFragmentInternal(CachedFragment* cachedFragment, bool &stopInjection, bool &fragmentDiscarded) = 0;
private:
	static const char* GetBufferHealthStatusString(BufferHealthStatus status);
public:
	bool eosReached; /**< set to true when a vod asset has been played to completion */
	bool enabled; /**< set to true if track is enabled */
	int numberOfFragmentsCached;/**< Number of fragments cached in this track*/
	const char* name; /**< Track name used for debugging*/
	double fragmentDurationSeconds; /**< duration in seconds for current fragment-of-interest */
	int segDLFailCount;
	int segDrmDecryptFailCount;
	TrackType type; /**< Media type of the track*/
protected:
	PrivateInstanceAAMP* aamp;
	CachedFragment cachedFragment[MAX_CACHED_FRAGMENTS_PER_TRACK]; /**< storage for currently-downloaded fragment */
	bool abort; /**< Abort all operations if flag is set*/
	pthread_mutex_t mutex; /**< protection of track variables accessed from multiple threads */
private:
	pthread_cond_t fragmentFetched; /**< Signaled after a fragment is fetched*/
	pthread_cond_t fragmentInjected; /**< Signaled after a fragment is injected*/
	pthread_t fragmentInjectorThreadID;
	int totalFragmentsDownloaded; /**< Total fragments downloaded since start by track*/
	bool fragmentInjectorThreadStarted;
	double totalInjectedDuration;
	double cacheDurationSeconds;
	bool notifiedCachingComplete;
	int fragmentIdxToInject; /**< Write position */
	int fragmentIdxToFetch; /**< Read position */
	int bandwidthBytesPerSecond; /** Bandwidth of last selected profile*/
	BufferHealthStatus bufferStatus; /**< Buffer status of the track*/
	BufferHealthStatus prevBufferStatus; /**< Buffer status of the track*/
	guint bufferHealthMonitorIdleTaskId; /**< ID of idle task for buffer monitoring*/
};

struct StreamInfo
{
	bool isIframeTrack;
	long bandwidthBitsPerSecond;
	struct
	{
		int width;
		int height;
	} resolution;
};

class StreamAbstractionAAMP
{
public:
	/**
	 *   @brief  StreamAbstractionAAMP constructor.
	 */
	StreamAbstractionAAMP(PrivateInstanceAAMP* aamp);

	/**
	 *   @brief  StreamAbstractionAAMP destructor.
	 */
	virtual ~StreamAbstractionAAMP();

	/**
	 *   @brief  Dump profiles for debugging.
	 *
	 *   @return void
	 */
	virtual void DumpProfiles(void) = 0;

	/**
	 *   @brief  Initialize a newly created object.
	 *
	 *   @param  tuneType to set type of object.
	 *   @return true on success
	 *   @return false on failure
	 */
	virtual bool Init(TuneType tuneType) = 0;

	/**
	 *   @brief  Set a position at which stop injection .
	 *   @param  endPosition - playback end position.
	 *   @return void
	 */
	virtual void SetEndPos(double endPosition){};

	/**
	 *   @brief  Start streaming.
	 *
	 *   @return void
	 */
	virtual void Start() = 0;

	/**
	*   @brief  Stop streaming.
	*
	*   @param  clearChannelData - clear channel /drm data on stop.
	*   @return void
	*/
	virtual void Stop(bool clearChannelData) = 0;


	/**
	 *   @brief  Is the stream live.
	 *
	 *   @return true if live stream
	 *   @return false if VOD
	 */
	virtual bool IsLive() = 0;

	/**
	 *   @brief Get output format of stream.
	 *
	 *   @param[out]  primaryOutputFormat - format of primary track
	 *   @param[out]  audioOutputFormat - format of audio track
	 *   @return void
	 */
	virtual void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat) = 0;

	/**
	 *   @brief Get current stream position.
	 *
	 *   @param[out]  primaryOutputFormat - format of primary track
	 *   @param[out]  audioOutputFormat - format of audio track
	 *   @return current position of stream.
	 */
	virtual double GetStreamPosition() = 0;

	/**
	 *   @brief  Get PTS of first sample.
	 *
	 *   @return PTS of first sample
	 */
	virtual double GetFirstPTS() = 0;

	/**
	 *   @brief Return MediaTrack of requested type
	 *
	 *   @param[in]  type - track type
	 *   @return MediaTrack pointer.
	 */
	virtual MediaTrack* GetMediaTrack(TrackType type) = 0;

	/**
	 *   @brief Waits track injection until caught up with video track.
	 *   Used internally by injection logic
	 *
	 *   @return void.
	 */
	void WaitForVideoTrackCatchup();

	/**
	 *   @brief Unblock track if caught up with video or downloads are stopped
	 *
	 *   @return void.
	 */
	void ReassessAndResumeAudioTrack();

	/**
	 *   @brief When TSB is involved, use this to set bandwidth to be reported.
	 *
	 *   @param[in]  tsbBandwidth - Bandwidth of the track.
	 *   @return void.
	 */
	void SetTsbBandwidth(long tsbBandwidth){ mTsbBandwidth = tsbBandwidth;}

	PrivateInstanceAAMP* aamp;

	/**
	 *   @brief Rampdown profile.
	 *
	 *   @return void.
	 */
	bool RampDownProfile(void);

	/**
	 *   @brief Check for ramdown profile.
	 *
	 *   @return true if rampdown needed in the case of fragment not available in higher profile.
	 */
	bool CheckForRampDownProfile(long http_error);

	/**
	 *   @brief Checks and update profile based on bandwidth.
	 *
	 *   @return void.
	 */
	void CheckForProfileChange(void);

	/**
	 *   @brief Get iframe track index.
	 *   This shall be called only after UpdateIframeTracks() is done
	 *
	 *   @return iframe track index.
	 */
	int GetIframeTrack();

	/**
	 *   @brief Update iframe tracks.
	 *   Subclasses shall invoke this after StreamInfo is populated .
	 *
	 *   @return void.
	 */
	void UpdateIframeTracks();

	/**
	 *   @brief Get the desired profile to start fetching.
	 *
	 *   @return profile index to be used for the track.
	 */
	int GetDesiredProfile(bool getMidProfile, long defaultBitrate);

	/**
	 *   @brief Notify bitrate updates to application.
	 *   Used internally by injection logic
	 *
	 *   @param[in]  profileIndex - profile index of last injected fragment.
	 *   @return void.
	 */
	void NotifyBitRateUpdate(int profileIndex);

	/**
	 *   @brief Fragment Buffering is required before playing.
	 *
	 *   @return true if buffering is required.
	 */
	bool IsFragmentBufferingRequired() { return false; }

	/**
	 *   @brief Whether we are playing at live point or not.
	 *
	 *   @return true if we are at live point.
	 */
	bool IsStreamerAtLivePoint() { return mIsAtLivePoint; }

	/**
	 *   @brief Informs streamer that playback was paused.
	 *
	 *   @param[in] paused - true, if playback was paused
	 *   @return void
	 */
	virtual void NotifyPlaybackPaused(bool paused);
#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	/**
	 *   @brief Check if playback is stalled in streamer.
	 *
	 *   @return true if playback stalled, false otherwise.
	 */
	bool CheckIfPlaybackStalled(double expectedInjectedDuration);
#else
	/**
	 *   @brief Check if player caches are running dry.
	 *
	 *   @return true if player caches are running dry, false otherwise.
	 */
	bool CheckIfPlayerRunningDry(void );
#endif
	/**
	 *   @brief MediaTracks shall call this to notify first fragment is injected.
	 *
	 *   @return void.
	 */
	void NotifyFirstFragmentInjected(void);

	/**
	 *   @brief Get elapsed time of playback.
	 *
	 *   @return elapsed time.
	 */
	double GetElapsedTime();

	bool trickplayMode;  /**< trick play flag to be updated by subclasses*/
	int currentProfileIndex;   /**< current profile index of the track*/
	int profileIdxForBandwidthNotification; /**< internal - profile index for bandwidth change notification*/
	bool hasDrm;  /**< denotes if the current asset is DRM protected*/

	bool mIsAtLivePoint; /**< flag that denotes if playback is at live point*/

#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	bool mIsPlaybackStalled; /**< flag that denotes if playback was stalled or not*/
#endif
	bool mIsFirstBuffer; /** <flag that denotes if the first buffer was processed or not*/

    /* START: Added As Part of DELIA-28247 and DELIA-28363 */
    int GetMaxBWProfile() { return mSortedBWProfileList.rbegin()->second; } /* Return the Top Profile Index*/
    int GetBWIndex(long BitRate);
    /* END: Added As Part of DELIA-28247  and  DELIA-28363 */

protected:

	/**
	 *   @brief Get number of profiles/ representations from subclass.
	 *
	 *   @return number of profiles.
	 */
	virtual int GetProfileCount() = 0;

	/**
	 *   @brief Get stream information of a profile from subclass.
	 *
	 *   @param[in]  idx - profile index.
	 *   @return stream information corresponding to index.
	 */
	virtual StreamInfo* GetStreamInfo(int idx) = 0;

private:
	int GetDesiredProfileBasedOnCache(void);
	void UpdateProfileBasedOnFragmentDownloaded(void);
	void UpdateProfileBasedOnFragmentCache(void);
	pthread_mutex_t mLock;
	pthread_cond_t mCond;

	// abr variables
	int mAbrProfileChgUpCntr;
	int mAbrProfileChgDnCntr;
	long mCurrentBandwidth;
	int mLastVideoFragCheckedforABR;
	long mTsbBandwidth;
	int mNwConsistencyBypass;
	int desiredIframeProfile;
	int lowestIframeProfile;
	std::map<long,int> mSortedBWProfileList;
	typedef std::map<long,int>::iterator SortedBWProfileListIter;
	typedef std::map<long,int>::reverse_iterator SortedBWProfileListRevIter;
	bool mIsPaused;
	long long mTotalPausedDurationMS;
	long long mStartTimeStamp;
	long long mLastPausedTimeStamp;
};

#endif // STREAMABSTRACTIONAAMP_H
