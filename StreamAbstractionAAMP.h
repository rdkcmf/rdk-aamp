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
 * @file StreamAbstractionAAMP.h
 * @brief Base classes of HLS/MPD collectors. Implements common caching/injection logic.
 */
 
#ifndef STREAMABSTRACTIONAAMP_H
#define STREAMABSTRACTIONAAMP_H

#include "priv_aamp.h"
#include <map>
#include <iterator>

#include <ABRManager.h>

#define MAX_CACHED_FRAGMENTS_PER_TRACK 3

/**
 * @enum TrackType
 * @brief Type of media track
 */
typedef enum
{
	eTRACK_VIDEO,
	eTRACK_AUDIO
} TrackType;

/**
 * @struct CachedFragment
 * @brief Holds information about a cached fragment
 */
struct CachedFragment
{
	GrowableBuffer fragment;
	double position;
	double duration;
	bool discontinuity;
	int profileIndex; /**< Updated internally*/
#ifdef AAMP_DEBUG_INJECT
	char uri[MAX_URI_LENGTH];
#endif
};

/**
 * @enum PlaylistType
 * @brief Type of playlist
 */
typedef enum
{
	ePLAYLISTTYPE_UNDEFINED,
	ePLAYLISTTYPE_EVENT, // playlist may grow via appended lines, but otherwise won't change
	ePLAYLISTTYPE_VOD, // playlist will never change
} PlaylistType;

/**
 * @class MediaTrack
 * @brief Base class of a media track
 */
class MediaTrack
{
public:
	MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name);
	virtual ~MediaTrack();
	void StartInjectLoop();
	void StopInjectLoop();
	bool Enabled();
	bool InjectFragment();

	/**
	 * @brief Get total injected duration of the track
	 * @retval total injected duration of the track
	 */
	double GetTotalInjectedDuration() { return totalInjectedDuration; };
	void RunInjectLoop();
	void UpdateTSAfterFetch();
	bool WaitForFreeFragmentAvailable( int timeoutMs = -1);
	void AbortWaitForCachedFragment( bool immediate);

	/**
	 * @brief Notifies profile changes to subclasses
	 */
	virtual void ABRProfileChanged(void) = 0;

	/**
	 * @brief Get number of fragments fetched
	 * @retval total number of fragments fetched by the track
	 */
	int GetTotalFragmentsFetched(){ return totalFragmentsDownloaded; }

	CachedFragment* GetFetchBuffer(bool initialize);
	void SetCurrentBandWidth(int bandwidthBps);
	int GetCurrentBandWidth();
	double GetTotalFetchedDuration() { return totalFetchedDuration; };
protected:

	void UpdateTSAfterInject();
	bool WaitForCachedFragmentAvailable();


	/**
	 * @brief Get the context of media track. To be implemented by subclasses
	 * @retval Context of track.
	 */
	virtual class StreamAbstractionAAMP* GetContext() = 0;

	/**
	 * @brief To be implemented by derived classes to receive cached fragment.
	 *
	 * @param[in] cachedFragment - contains fragment to be processed and injected
	 * @param[out] fragmentDiscarded - true if fragment is discarded.
	 */
	virtual void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded) = 0;
public:
	bool eosReached; /**< set to true when a vod asset has been played to completion */
	bool enabled; /**< set to true if track is enabled */
	int numberOfFragmentsCached;/**< Number of fragments cached in this track*/
	const char* name; /**< Track name used for debugging*/
	double fragmentDurationSeconds; /**< duration in seconds for current fragment-of-interest */
	int segDLFailCount; /**< Number of segment download failures */
	int segDrmDecryptFailCount; /**< Number of segment decrypt failures */
	int mSegInjectFailCount;	/**< Segment Inject/Decode fail count */
	TrackType type; /**< Media type of the track*/
protected:
	PrivateInstanceAAMP* aamp; /**< private aamp instance associated with this track*/
	CachedFragment cachedFragment[MAX_CACHED_FRAGMENTS_PER_TRACK]; /**< storage for currently-downloaded fragment */
	bool abort; /**< Abort all operations if flag is set*/
	pthread_mutex_t mutex; /**< protection of track variables accessed from multiple threads */
private:
	pthread_cond_t fragmentFetched; /**< Signaled after a fragment is fetched*/
	pthread_cond_t fragmentInjected; /**< Signaled after a fragment is injected*/
	pthread_t fragmentInjectorThreadID; /**< fragment Injector Thread ID */
	int totalFragmentsDownloaded; /**< Total fragments downloaded since start by track*/
	bool fragmentInjectorThreadStarted; /**< indicates if fragmentInjectorThread is started*/
	double totalInjectedDuration; /**< total injected duration*/
	int cacheDurationSeconds; /**< duration of cache in seconds*/
	bool notifiedCachingComplete; /**< indicates if cache complete is notified*/
	int fragmentIdxToInject; /**< Write position */
	int fragmentIdxToFetch; /**< Read position */
	int bandwidthBytesPerSecond; /** Bandwidth of last selected profile*/
	double totalFetchedDuration; /** Total duration fetched by track*/
	size_t fetchBufferPreAllocLen; /** Buffer length to pre-allocate for next fetch buffer*/
};


/**
 * @struct StreamResolution
 * @brief Holds resolution of stream
 */
struct StreamResolution
{
	int width; /**< Width in pixels*/
	int height; /**< Height in pixels*/
};

/**
 * @struct StreamInfo
 * @brief Holds information of a stream.
 */
struct StreamInfo
{
	bool isIframeTrack; /**< indicates if the stream is iframe stream*/
	long bandwidthBitsPerSecond; /**< Bandwidth of the stream bps*/
	StreamResolution resolution; /**< Resolution of the stream*/
};

/**
 * @class StreamAbstractionAAMP
 * @brief Base class of top level HLS/MPD collectors.
 */
class StreamAbstractionAAMP
{
public:
	StreamAbstractionAAMP(PrivateInstanceAAMP* aamp);
	virtual ~StreamAbstractionAAMP();

	/**
	 * @brief  Dump profiles for debugging.
	 * @note To be implemented by sub classes
	 */
	virtual void DumpProfiles(void) = 0;

	/**
	 *   @brief  Initialize a newly created object.
	 *   @note   To be implemented by sub classes
	 *   @param  tuneType to set type of object.
	 *   @retval true on success
	 *   @retval false on failure
	 */
	virtual bool Init(TuneType tuneType) = 0;

	/**
	 *   @brief  Set a position at which stop injection .
	 *   @param  endPosition - playback end position.
	 */
	virtual void SetEndPos(double endPosition){};

	/**
	 *   @brief  Starts streaming.
	 */
	virtual void Start() = 0;

	/**
	*   @brief  Stops streaming.
	*
	*   @param  clearChannelData - clear channel /drm data on stop.
	*/
	virtual void Stop(bool clearChannelData) = 0;


	/**
	 *   @brief  Check if the stream live.
	 *
	 *   @retval true if live stream
	 *   @retval false if VOD
	 */
	virtual bool IsLive() = 0;

	/**
	 *   @brief Get output format of stream.
	 *
	 *   @param[out]  primaryOutputFormat - format of primary track
	 *   @param[out]  audioOutputFormat - format of audio track
	 *   @retval void
	 */
	virtual void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat) = 0;

	/**
	 *   @brief Get current stream position.
	 *
	 *   @retval current position of stream.
	 */
	virtual double GetStreamPosition() = 0;

	/**
	 *   @brief  Get PTS of first sample.
	 *
	 *   @retval PTS of first sample
	 */
	virtual double GetFirstPTS() = 0;

	/**
	 *   @brief Return MediaTrack of requested type
	 *
	 *   @param[in]  type - track type
	 *   @retval MediaTrack pointer.
	 */
	virtual MediaTrack* GetMediaTrack(TrackType type) = 0;

	void WaitForVideoTrackCatchup();

	void ReassessAndResumeAudioTrack();

	/**
	 *   @brief When TSB is involved, use this to set bandwidth to be reported.
	 *
	 *   @param[in]  tsbBandwidth - Bandwidth of the track.
	 */
	void SetTsbBandwidth(long tsbBandwidth){ mTsbBandwidth = tsbBandwidth;}

	PrivateInstanceAAMP* aamp; /// pointer to PrivateInstanceAAMP object associated with stream

	bool RampDownProfile(void);

	bool CheckForRampDownProfile(long http_error);

	void CheckForProfileChange(void);

	int GetIframeTrack();

	void UpdateIframeTracks();

	int GetDesiredProfile(bool getMidProfile);

	void NotifyBitRateUpdate(int profileIndex);

	/**
	 *   @brief Fragment Buffering is required before playing.
	 *
	 *   @retval true if buffering is required.
	 */
	bool IsFragmentBufferingRequired() { return false; }

	/**
	 *   @brief Whether we are playing at live point or not.
	 *
	 *   @retval true if we are at live point.
	 */
	bool IsStreamerAtLivePoint() { return mIsAtLivePoint; }

	/**
	 *   @brief Informs streamer that playback was paused.
	 *
	 *   @param[in] paused - true, if playback was paused
	 *   @retval void
	 */
	virtual void NotifyPlaybackPaused(bool paused);

#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	bool CheckIfPlaybackStalled(double expectedInjectedDuration);
#else

	bool CheckIfPlayerRunningDry(void );
#endif

	bool trickplayMode;  /**< trick play flag to be updated by subclasses*/
	int currentProfileIndex;   /**< current profile index of the track*/
	int profileIdxForBandwidthNotification; /**< internal - profile index for bandwidth change notification*/
	bool hasDrm;  /**< denotes if the current asset is DRM protected*/

	bool mIsAtLivePoint; /**< flag that denotes if playback is at live point*/

#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	bool mIsPlaybackStalled; /**< flag that denotes if playback was stalled or not*/
#endif
	bool mIsFirstBuffer; /** <flag that denotes if the first buffer was processed or not*/


	/**
	 * @brief Get index of profile that has maximum bandwidth
	 * @retval Index of maximum bandwidth profile
	 */
	int GetMaxBWProfile() { return mAbrManager.getMaxBandwidthProfile(); } /* Return the Top Profile Index*/

	/**
	 * @brief Get index of profile corresponds to bandwidth
	 * @param[in] bandwidth Bandwidth to lookup profile
	 * @retval profile index
	 */
	virtual int GetBWIndex(long bandwidth) = 0;

	/**
	 *    @brief Get the ABRManager reference.
	 *
	 *    @retval The ABRManager reference.
	 */
	ABRManager& GetABRManager() {
		return mAbrManager;
	}

	/**
	 *   @brief Get number of profiles/ representations from subclass.
	 *
	 *   @return number of profiles.
	 */
	int GetProfileCount() {
		return mAbrManager.getProfileCount();
	}

protected:
	/**
	 *   @brief Get stream information of a profile from subclass.
	 *
	 *   @param[in]  idx - profile index.
	 *   @retval stream information corresponding to index.
	 */
	virtual StreamInfo* GetStreamInfo(int idx) = 0;

private:
	int GetDesiredProfileBasedOnCache(void);
	void UpdateProfileBasedOnFragmentDownloaded(void);
	void UpdateProfileBasedOnFragmentCache(void);

	pthread_mutex_t mLock; /**< lock for A/V track catchup logic*/
	pthread_cond_t mCond; /**< condition for A/V track catchup logic*/

	// abr variables
	long mCurrentBandwidth; /**< stores current bandwidth*/
	int mLastVideoFragCheckedforABR; /**< Last video fragment for which ABR is checked*/
	long mTsbBandwidth; /**< stores bandwidth when TSB is involved*/
	long mNwConsistencyBypass; /**< Network consistency bypass*/
protected:
	ABRManager mAbrManager; /**< Pointer to abr manager*/
};

#endif // STREAMABSTRACTIONAAMP_H
