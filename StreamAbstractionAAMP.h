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

#include "AampMemoryUtils.h"
#include "priv_aamp.h"
#include <map>
#include <iterator>
#include <vector>

#include <ABRManager.h>
#include <glib.h>
#include "subtitleParser.h"

/**
 * @brief Media Track Types
 */
typedef enum
{
	eTRACK_VIDEO,     /**< Video track */
	eTRACK_AUDIO,     /**< Audio track */
	eTRACK_SUBTITLE,  /**< Subtitle track */
	eTRACK_AUX_AUDIO  /**< Auxiliary audio track */
} TrackType;

/**
 * @brief Structure holding the resolution of stream
 */
struct StreamResolution
{
	int width;      /**< Width in pixels*/
	int height;     /**< Height in pixels*/
	double framerate; /**< Frame Rate */
};

/**
 * @brief Structure holding the information of a stream.
 */
struct StreamInfo
{
	bool enabled;					/**< indicates if the streamInfo profile is enabled */
	bool isIframeTrack;             /**< indicates if the stream is iframe stream*/
	long bandwidthBitsPerSecond;    /**< Bandwidth of the stream bps*/
	StreamResolution resolution;    /**< Resolution of the stream*/
	BitrateChangeReason reason;	/**< Reason for bitrate change*/
};

/**
*	\struct	TileInfo
* 	\brief	TileInfo structure for Thumbnail data
*/
struct TileInfo
{
	int numRows; /**< Number of Rows from Tile Inf */
	int numCols; /**< Number of Cols from Tile Inf */
	double posterDuration; /**< Duration of each Tile in Spritesheet */

	double tileSetDuration; /**<Duration of whole Tile set */
	double startTime;
	const char *url;
};

/**
 * @brief Structure of cached fragment data
 *        Holds information about a cached fragment
 */
struct CachedFragment
{
	GrowableBuffer fragment;    /**< Buffer to keep fragment content */
	double position;            /**< Position in the playlist */
	double duration;            /**< Fragment duration */
	bool initFragment;	        /**< Is init frgment */
	bool discontinuity;         /**< PTS discontinuity status */
	int profileIndex;           /**< Profile index; Updated internally */
#ifdef AAMP_DEBUG_INJECT
	std::string uri;   /**< Fragment url */
#endif
	StreamInfo cacheFragStreamInfo; /**< Bitrate info of the fragment */
	MediaType   type;           /**< MediaType info of the fragment */
};

/**
 * @brief Structure of cached fragment data
 *        Holds information about a cached fragment
 */
struct CachedFragmentChunk
{
	GrowableBuffer fragmentChunk;    /**< Buffer to keep fragment content */
	MediaType   type; /**< MediaType info of the fragment */
};

/**
 * @brief Playlist Types
 */
typedef enum
{
	ePLAYLISTTYPE_UNDEFINED,    /**< Playlist type undefined */
	ePLAYLISTTYPE_EVENT,        /**< Playlist may grow via appended lines, but otherwise won't change */
	ePLAYLISTTYPE_VOD,          /**< Playlist will never change */
} PlaylistType;

/**
 * @brief Buffer health status
 */
enum BufferHealthStatus
{
	BUFFER_STATUS_GREEN,  /**< Healthy state, where buffering is close to being maxed out */
	BUFFER_STATUS_YELLOW, /**< Danger  state, where buffering is close to being exhausted */
	BUFFER_STATUS_RED     /**< Failed state, where buffers have run dry, and player experiences underrun/stalled video */
};

typedef enum
{
	eDISCONTIUITY_FREE = 0,
	eDISCONTINUIY_IN_VIDEO = 1,
	eDISCONTINUIY_IN_AUDIO = 2,
	eDISCONTINUIY_IN_BOTH = 3
} MediaTrackDiscontinuityState;

/**
 * @brief Base Class for Media Track
 */
class MediaTrack
{
public:

	/**
	 * @brief MediaTrack Constructor
	 *
	 * @param[in] type - Media track type
	 * @param[in] aamp - Pointer to PrivateInstanceAAMP
	 * @param[in] name - Media track name
	 */
	MediaTrack(AampLogManager *logObj, TrackType type, PrivateInstanceAAMP* aamp, const char* name);

	/**
	 * @brief MediaTrack Destructor
	 */
	virtual ~MediaTrack();

	/**
	* @brief MediaTrack Copy Constructor
	*/
	MediaTrack(const MediaTrack&) = delete;

	/**
	* @brief MediaTrack assignment operator overloading
	*/
	MediaTrack& operator=(const MediaTrack&) = delete;

	/**
	 * @brief Start fragment injector loop
	 *
	 * @return void
	 */
	void StartInjectLoop();

	/**
	* @brief Start fragment Chunk injector loop
	*
	* @return void
	*/
	void StartInjectChunkLoop();

	/**
	 * @brief Stop fragment injector loop
	 *
	 * @return void
	 */
	void StopInjectLoop();

	/**
	* @brief Stop fragment chunk injector loop
	*
	* @return void
	*/
	void StopInjectChunkLoop();

	/**
	 * @brief Status of media track
	 *
	 * @return Enabled/Disabled
	 */
	bool Enabled();

	/**
	 * @brief Inject fragment into the gstreamer
	 *
	 * @return Success/Failure
	 */
	bool InjectFragment();

	/**
	* @brief Process next cached fragment chunk
	*/

	bool ProcessFragmentChunk();

	/**
	* @brief Inject fragment Chunk into the gstreamer
	*
	* @return Success/Failure
	*/
	bool InjectFragmentChunk();

	/**
	 * @brief Get total fragment injected duration
	 *
	 * @return Total duration in seconds
	 */
	double GetTotalInjectedDuration() { return totalInjectedDuration; };

	/**
	* @brief Get total fragment chunk injected duration
	*
	* @return Total duration in seconds
	*/
	double GetTotalInjectedChunkDuration() { return totalInjectedChunksDuration; };

	/**
	 * @brief Run fragment injector loop.
	 *
	 * @return void
	 */
	void RunInjectLoop();

	/**
	* @brief Run fragment injector loop.
	*
	* @return void
	*/
	void RunInjectChunkLoop();

	/**
	 * @brief Update cache after fragment fetch
	 *
	 * @return void
	 */
	void UpdateTSAfterFetch();

	/**
	* @brief Update cache after fragment chunk fetch
	*
	* @return void
	*/
	void UpdateTSAfterChunkFetch();

	/**
	 * @brief Wait till fragments available
	 *
	 * @param[in] timeoutMs - Timeout in milliseconds. Default - infinite
	 * @return Fragment available or not.
	 */
	bool WaitForFreeFragmentAvailable( int timeoutMs = -1);

	/**
	 * @brief Abort the waiting for cached fragments and free fragment slot
	 *
	 * @param[in] immediate - Forced or lazy abort
	 * @return void
	 */
	void AbortWaitForCachedAndFreeFragment(bool immediate);

	/**
	 * @brief Notifies profile changes to subclasses
	 *
	 * @return void
	 */
	virtual void ABRProfileChanged(void) = 0;
	virtual double GetBufferedDuration (void) = 0;

	/**
	 * @brief Get number of fragments dpownloaded
	 *
	 * @return Number of downloaded fragments
	 */
	int GetTotalFragmentsFetched(){ return totalFragmentsDownloaded; }

	/**
	 * @brief Get buffer to store the downloaded fragment content
	 *
	 * @param[in] initialize - Buffer to to initialized or not
	 * @return Fragment cache buffer
	 */
	CachedFragment* GetFetchBuffer(bool initialize);

	/**
	* @brief Get buffer to fetch and cache next fragment chunk
	* @param[in] initialize true to initialize the fragment chunk
	* @retval Pointer to fragment chunk buffer.
	*/
	CachedFragmentChunk* GetFetchChunkBuffer(bool initialize);

	/**
	 * @brief Set current bandwidth
	 *
	 * @param[in] bandwidthBps - Bandwidth in bps
	 * @return void
	 */
	void SetCurrentBandWidth(int bandwidthBps);

       /**
       * @brief Get profile index for TsbBandwidth
       * @param bandwidth - bandwidth to identify profile index from list
       * @retval profile index of the current bandwidth
       */
       int GetProfileIndexForBW(long mTsbBandwidth);

	/**
	 * @brief Get current bandwidth in bps
	 *
	 * @return Bandwidth in bps
	 */
	int GetCurrentBandWidth();

	/**
	 * @brief Get total duration of fetched fragments
	 *
	 * @return Total duration in seconds
	 */
	double GetTotalFetchedDuration() { return totalFetchedDuration; };

    /**
     * @brief Get total duration of fetched fragments
     *
     * @return Total duration in seconds
     */
    double GetTotalInjectedChunksDuration() { return totalInjectedChunksDuration; };


	/**
	 * @brief Check if discontinuity is being processed
	 *
	 * @return true if discontinuity is being processed
	 */
	bool IsDiscontinuityProcessed() { return discontinuityProcessed; }

	bool isFragmentInjectorThreadStarted( ) {  return fragmentInjectorThreadStarted;}
	void MonitorBufferHealth();
	void ScheduleBufferHealthMonitor();

	/**
	* @brief Get buffer Status of track
	*
	* @return BufferHealthStatus
	*/
	BufferHealthStatus GetBufferStatus();

	/**
	 * @brief Get buffer health status
	 *
	 * @return current buffer health status
	 */
	BufferHealthStatus GetBufferHealthStatus() { return bufferStatus; };

	/**
	 * @brief Abort the waiting for cached fragments immediately
	 *
	 * @return void
	 */
	void AbortWaitForCachedFragment();

	/**
	 * @brief Check whether track data injection is aborted
	 *
	 * @return true if injection is aborted, false otherwise
	 */
	bool IsInjectionAborted() { return (abort || abortInject); }

	/**
	 * @brief Returns if the end of track reached.
	 */
	virtual bool IsAtEndOfTrack() { return eosReached;}

	/**
	 * @brief To check for discontinuity in future fragments.
	 *
	 * @param[out] cacheDuration - cached fragment duration in seconds
	 * @return bool - true if discontinuity present, false otherwise
	 */
	bool CheckForFutureDiscontinuity(double &cacheDuration);

	/**
	* @brief Called if sink buffer is full
	*
	* @return void
	*/
	void OnSinkBufferFull();

	/**
	 * @brief Flushes all cached fragments
	 *
	 * @return void
	 */
	void FlushFragments();

	/**
	* @brief Cleans all cached fragment Chunks and unparsed buffer
	*
	* @return void
	*/
	void CleanChunkCache();
	/**
	* @brief Flushes all cached fragment Chunks
	*
	* @return void
	*/
	void FlushFragmentChunks();

protected:

	/**
	 * @brief Update segment cache and inject buffer to gstreamer
	 *
	 * @return void
	 */
	void UpdateTSAfterInject();

	/**
	* @brief Update segment cache and inject buffer to gstreamer
	*
	* @return void
	*/
	void UpdateTSAfterChunkInject();

	/**
	 * @brief Wait till cached fragment available
	 *
	 * @return TRUE if fragment available, FALSE if aborted/fragment not available.
	 */
	bool WaitForCachedFragmentAvailable();

	/**
	* @brief Wait until a cached fragment chunk is Injected.
	* @retval true if fragment chunk injected , false on abort.
	*/
	bool WaitForCachedFragmentChunkInjected(int timeoutMs = -1);

	/**
	* @brief Wait till cached fragment chunk available
	*
	* @return TRUE if fragment chunk available, FALSE if aborted/fragment chunk not available.
	*/
	bool WaitForCachedFragmentChunkAvailable();

	/**
	 * @brief Get the context of media track. To be implemented by subclasses
	 *
	 * @return Pointer to StreamAbstractionAAMP object
	 */
	virtual class StreamAbstractionAAMP* GetContext() = 0;

	/**
	 * @brief To be implemented by derived classes to receive cached fragment.
	 *
	 * @param[in] cachedFragment - contains fragment to be processed and injected
	 * @param[out] fragmentDiscarded - true if fragment is discarded.
	 * @return void
	 */
	virtual void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded) = 0;

	/**
	* @brief To be implemented by derived classes to receive cached fragment Chunk
	*
	* @param[in] cachedFragmentChunk - contains fragment to be processed and injected
	* @param[out] fragmentChunkDiscarded - true if fragment is discarded.
	* @return void
	*/
	void InjectFragmentChunkInternal(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double fDuration);


	static int GetDeferTimeMs(long maxTimeSeconds);


	/**
	 * @brief To be implemented by derived classes if discontinuity on trick-play is to be notified.
	 *
	 */
	virtual void SignalTrickModeDiscontinuity(){};

private:
	static const char* GetBufferHealthStatusString(BufferHealthStatus status);

public:
	bool eosReached;                    /**< set to true when a vod asset has been played to completion */
	bool enabled;                       /**< set to true if track is enabled */
	int numberOfFragmentsCached;        /**< Number of fragments cached in this track*/
	int numberOfFragmentChunksCached;   /**< Number of fragments cached in this track*/
	const char* name;                   /**< Track name used for debugging*/
	double fragmentDurationSeconds;     /**< duration in seconds for current fragment-of-interest */
	int segDLFailCount;                 /**< Segment download fail count*/
	int segDrmDecryptFailCount;         /**< Segment decryption failure count*/
	int mSegInjectFailCount;            /**< Segment Inject/Decode fail count */
	TrackType type;                     /**< Media type of the track*/
	std::unique_ptr<SubtitleParser> mSubtitleParser;    /**< Parser for subtitle data*/
	bool refreshSubtitles;              /**< Switch subtitle track in the FetchLoop */
	int maxCachedFragmentsPerTrack;
	int maxCachedFragmentChunksPerTrack;
	pthread_cond_t fragmentChunkFetched;/**< Signaled after a fragment Chunk is fetched*/
	pthread_cond_t fragmentChunkClean;  /**< Signaled after a fragment Chunks are cleaned*/
	uint32_t totalMdatCount;            /**< Total MDAT Chunk Found*/
	int noMDATCount;                    /**< MDAT Chunk Not Found count continuously while chunk buffer processoing*/

protected:
	AampLogManager *mLogObj;
	PrivateInstanceAAMP* aamp;          /**< Pointer to the PrivateInstanceAAMP*/
	CachedFragment *cachedFragment;     /**< storage for currently-downloaded fragment */
	CachedFragmentChunk cachedFragmentChunks[DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK];
	GrowableBuffer unparsedBufferChunk;    /**< Buffer to keep fragment content */
	GrowableBuffer parsedBufferChunk;    /**< Buffer to keep fragment content */
	bool abort;                         /**< Abort all operations if flag is set*/
	pthread_mutex_t mutex;              /**< protection of track variables accessed from multiple threads */
	bool ptsError;                      /**< flag to indicate if last injected fragment has ptsError */
	bool abortInject;                   /**< Abort inject operations if flag is set*/
	bool abortInjectChunk;                   /**< Abort inject operations if flag is set*/
	bool cleanChunkCache;
	bool cleanChunkCacheInitiated;
private:
	pthread_cond_t fragmentFetched;     /**< Signaled after a fragment is fetched*/
	pthread_cond_t fragmentInjected;    /**< Signaled after a fragment is injected*/
	pthread_t fragmentInjectorThreadID; /**< Fragment injector thread id*/
	pthread_cond_t fragmentChunkInjected;/**< Signaled after a fragment is injected*/
	pthread_t fragmentChunkInjectorThreadID; /**< Fragment injector thread id*/
	pthread_t bufferMonitorThreadID;    /**< Buffer Monitor thread id */
	int totalFragmentsDownloaded;       /**< Total fragments downloaded since start by track*/
	int totalFragmentChunksDownloaded;       /**< Total fragments downloaded since start by track*/
	bool fragmentInjectorThreadStarted; /**< Fragment injector's thread started or not*/
	bool fragmentChunkInjectorThreadStarted; /**< Fragment Chunk injector's thread started or not*/
	bool bufferMonitorThreadStarted;    /**< Buffer Monitor thread started or not */
	double totalInjectedDuration;       /**< Total fragment injected duration*/
	double totalInjectedChunksDuration;  /**< Total fragment injected chunk duration*/
	int currentInitialCacheDurationSeconds;    /**< Current cached fragments duration before playing*/
	bool sinkBufferIsFull;                /**< True if sink buffer is full and do not want new fragments*/
	bool cachingCompleted;              /**< Fragment caching completed or not*/
	int fragmentIdxToInject;            /**< Write position */
	int fragmentChunkIdxToInject;       /**< Write position */
	int fragmentIdxToFetch;             /**< Read position */
	int fragmentChunkIdxToFetch;        /**< Read position */
	int bandwidthBitsPerSecond;        /**< Bandwidth of last selected profile*/
	double totalFetchedDuration;        /**< Total fragment fetched duration*/
	bool discontinuityProcessed;

	BufferHealthStatus bufferStatus;     /**< Buffer status of the track*/
	BufferHealthStatus prevBufferStatus; /**< Previous buffer status of the track*/
};

/**
 * @brief StreamAbstraction class of AAMP
 */
class StreamAbstractionAAMP
{
public:
	/**
	 * @brief StreamAbstractionAAMP constructor.
	 */
	StreamAbstractionAAMP(AampLogManager *logObj, PrivateInstanceAAMP* aamp);

	/**
	 * @brief StreamAbstractionAAMP destructor.
	 */
	virtual ~StreamAbstractionAAMP();

	/**
	* @brief StreamAbstractionAAMP Copy Constructor
	*/
	StreamAbstractionAAMP(const StreamAbstractionAAMP&) = delete;

	/**
	* @brief StreamAbstractionAAMP assignment operator overloading
	*/
	StreamAbstractionAAMP& operator=(const StreamAbstractionAAMP&) = delete;

	/**
	 * @brief  Dump profiles for debugging.
	 *         To be implemented by sub classes
	 *
	 * @return void
	 */
	virtual void DumpProfiles(void) = 0;

	/**
	 *   @brief  Initialize a newly created object.
	 *           To be implemented by sub classes
	 *
	 *   @param[in]  tuneType - to set type of playback.
	 *   @return true on success, false failure
	 */
	virtual AAMPStatusType Init(TuneType tuneType) = 0;

	/**
	 *   @brief  Start streaming.
	 *
 	 *   @return void
	 */
	virtual void Start() = 0;

	/**
	*   @brief  Stops streaming.
	*
	*   @param[in]  clearChannelData - clear channel /drm data on stop.
	*   @return void
	*/
	virtual void Stop(bool clearChannelData) = 0;

	/**
	 *   @brief Get output format of stream.
	 *
	 *   @param[out]  primaryOutputFormat - format of primary track
	 *   @param[out]  audioOutputFormat - format of audio track
	 *   @param[out]  auxAudioOutputFormat - format of aux audio track
	 *   @return void
	 */
	virtual void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat) = 0;

	/**
	 *   @brief Get current stream position.
	 *
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
	 *   @brief  Get Start time PTS of first sample.
	 *
	 *   @retval start time of first sample
	 */
	virtual double GetStartTimeOfFirstPTS() = 0;

	/**
	 *   @brief Return MediaTrack of requested type
	 *
	 *   @param[in]  type - track type
	 *   @return MediaTrack pointer.
	 */
	virtual MediaTrack* GetMediaTrack(TrackType type) = 0;

	/**
	 *   @brief Waits audio track injection until caught up with video track.
	 *          Used internally by injection logic
	 *
	 *   @param None
	 *   @return void
	 */
	void WaitForVideoTrackCatchup();

	/**
	 *   @brief Unblock track if caught up with video or downloads are stopped
	 *
	 *   @return void
	 */
	void ReassessAndResumeAudioTrack(bool abort);

	/**
	 *   @brief When TSB is involved, use this to set bandwidth to be reported.
	 *
	 *   @param[in]  tsbBandwidth - Bandwidth of the track.
	 *   @return void
	 */
	void SetTsbBandwidth(long tsbBandwidth){ mTsbBandwidth = tsbBandwidth;}

	/**
	 *   @brief When TSB is involved, use this to get bandwidth to be reported.
	 *
	 *   @return Bandwidth of the track.
	 */
	long GetTsbBandwidth() { return mTsbBandwidth ;}

	/**
	 *   @brief Set elementary stream type change status for reconfigure the pipeline.
	 *
	 *   @param[in]  None
	 *   @return void
	 */
	void SetESChangeStatus(void){ mESChangeStatus = true;}

	/**
	 *   @brief Reset elementary stream type change status once the pipeline reconfigured.
	 *
	 *   @param[in]  None
	 *   @return void
	 */
	void ResetESChangeStatus(void){ mESChangeStatus = false;}

	/**
	 *   @brief Get elementary stream type change status for reconfigure the pipeline..
	 *
	 *   @param[in]  None
	 *   @retval mESChangeStatus flag value ( true or false )
	 */
	bool GetESChangeStatus(void){ return mESChangeStatus;}

	PrivateInstanceAAMP* aamp;  /**< Pointer to PrivateInstanceAAMP object associated with stream*/

	AampLogManager *mLogObj;
	/**
	 * @brief Rampdown profile
	 *
	 * @param[in] http_error
	 * @return True, if ramp down successful. Else false
	 */
	bool RampDownProfile(long http_error);
	/**
	 *   @brief Get Desired Profile based on Buffer availability
	 *
	 *   @param [in] currProfileIndex
	 *   @param [in] newProfileIndex
	 *   @return None.
	 */
	void GetDesiredProfileOnBuffer(int currProfileIndex, int &newProfileIndex);
	/**
	 *   @brief Get Desired Profile on steady state 
	 *
	 *   @param [in] currProfileIndex
	 *   @param [in] newProfileIndex
	 *   @param [in] nwBandwidth         
	 *   @return None.
	 */
	void GetDesiredProfileOnSteadyState(int currProfileIndex, int &newProfileIndex, long nwBandwidth);
	/**
	 *   @brief Configure download timeouts based on buffer
	 *
	 *   @return None.
	 */        
	void ConfigureTimeoutOnBuffer();
	/**
	 *   @brief Function to get the buffer duration of stream
	 *
	 *   @return buffer value 
	 */                
	virtual double GetBufferedDuration (void) = 0;

    /**
	 *   @brief Check whether the current profile is lowest.
	 *
	 *   @param currentProfileIndex - current profile index to be checked.
	 *   @return true if the given profile index is lowest.
	 */
	bool IsLowestProfile(int currentProfileIndex);

    /**
	 *   @brief Check for ramdown profile.
	 *
	 *   @param http_error
	 *   @return true if rampdown needed in the case of fragment not available in higher profile.
	 */
	bool CheckForRampDownProfile(long http_error);

	/**
	 *   @brief Checks and update profile based on bandwidth.
	 *
	 *   @param None
	 *   @return void
	 */
	void CheckForProfileChange(void);

	/**
	 *   @brief Get iframe track index.
	 *   This shall be called only after UpdateIframeTracks() is done
	 *
	 *   @param None
	 *   @return iframe track index.
	 */
	int GetIframeTrack();

	/**
	 *   @brief Update iframe tracks.
	 *   Subclasses shall invoke this after StreamInfo is populated .
	 *
	 *   @param None
	 *   @return void
	 */
	void UpdateIframeTracks();

	/**
	 *   @brief Get the last video fragment parsed time.
	 *
	 *   @param None
	 *   @return Last video fragment parsed time.
	 */
	double LastVideoFragParsedTimeMS(void);

	/**
	 *   @brief Get the desired profile to start fetching.
	 *
	 *   @param getMidProfile
	 *   @return profile index to be used for the track.
	 */
	int GetDesiredProfile(bool getMidProfile);

	/**
	 *   @brief Update rampdown profile on network failure
	 *
	 *   @param None
	 *   @return void
	 */
	void UpdateRampdownProfileReason(void);

	/**
	 *   @brief Notify bitrate updates to application.
	 *   Used internally by injection logic
	 *
	 *   @param[in]  profileIndex - profile index of last injected fragment.
	 *   @param[in]  cacheFragStreamInfo - stream info for the last injected fragment.
	 *   @return void
	 */
	void NotifyBitRateUpdate(int profileIndex, const StreamInfo &cacheFragStreamInfo, double position);
	
	/**
	 *   @brief Check if Initial Fragment Caching is supported
	 *
	 *   @return true if is supported
	 */
	virtual bool IsInitialCachingSupported();

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

	/**
	 *   @brief Check if player caches are running dry.
	 *
	 *   @return true if player caches are dry, false otherwise.
	 */
	bool CheckIfPlayerRunningDry(void);

	/**
	 *   @brief Check if playback has stalled and update related flags.
	 *
	 *   @param[in] fragmentParsed - true if next fragment was parsed, otherwise false
	 *   @param[in] isStalledBeforePlay - true if the playback stalled due to lack of new fragment before the pipeline changed to playing state.
	 */
	void CheckForPlaybackStall(bool fragmentParsed, bool isStalledBeforePlay = false);

	void NotifyFirstFragmentInjected(void);

	double GetElapsedTime();

	virtual double GetFirstPeriodStartTime() { return 0; }
	virtual double GetFirstPeriodDynamicStartTime() { return 0; }
	virtual uint32_t GetCurrPeriodTimeScale()  { return 0; }
	/**
	 *   @brief Check for ramp down limit reached by player
	 *   @return true if limit reached, false otherwise
	 */
	bool CheckForRampDownLimitReached();

	bool trickplayMode;                     /**< trick play flag to be updated by subclasses*/
	int currentProfileIndex;                /**< current Video profile index of the track*/
	int currentAudioProfileIndex;           /**< current Audio profile index of the track*/
	int currentTextTrackProfileIndex;        /**< current SubTitle profile index of the track*/
	int profileIdxForBandwidthNotification; /**< internal - profile index for bandwidth change notification*/
	bool hasDrm;                            /**< denotes if the current asset is DRM protected*/

	bool mIsAtLivePoint;                    /**< flag that denotes if playback is at live point*/

	bool mIsPlaybackStalled;                /**< flag that denotes if playback was stalled or not*/
	bool mNetworkDownDetected;              /**< Network down status indicator */
	bool mCheckForRampdown;			/**< flag to indicate if rampdown is attempted or not */
	TuneType mTuneType;                     /**< Tune type of current playback, initialize by derived classes on Init()*/
	int mRampDownCount;			/**< Total number of rampdowns */


	/**
	 *   @brief Get profile index of highest bandwidth
	 *
	 *   @return Profile index
	 */
	int GetMaxBWProfile() { return mAbrManager.getMaxBandwidthProfile(); } /* Return the Top Profile Index*/

	/**
	 *   @brief Get profile index of given bandwidth.
	 *
	 *   @param[in]  bandwidth - Bandwidth
	 *   @return Profile index
	 */
	virtual int GetBWIndex(long bandwidth) = 0;

	/**
	 *    @brief Get the ABRManager reference.
	 *
	 *    @return The ABRManager reference.
	 */
	ABRManager& GetABRManager() {
		return mAbrManager;
	}

	/**
	 *   @brief Get number of profiles/ representations from subclass.
	 *
	 *   @return number of profiles.
	 */
	virtual int GetProfileCount() {
		return mAbrManager.getProfileCount();
	}

       /**
       * @brief Get profile index for TsbBandwidth
       * @param bandwidth - bandwidth to identify profile index from list
       * @retval profile index of the current bandwidth
       */
       virtual int GetProfileIndexForBandwidth(long mTsbBandwidth) {
	       return mAbrManager.getBestMatchedProfileIndexByBandWidth(mTsbBandwidth);
       }

	long GetCurProfIdxBW(){
		return mAbrManager.getBandwidthOfProfile(this->currentProfileIndex);
	}


	/**
	 *   @brief Gets Max bitrate supported
	 *
	 *   @return max bandwidth
	 */
	virtual long GetMaxBitrate(){
		return mAbrManager.getBandwidthOfProfile(mAbrManager.getMaxBandwidthProfile());
	}


	/**
	 *   @brief Get the bitrate of current video profile selected.
	 *
	 *   @return bitrate of current video profile.
	 */
	long GetVideoBitrate(void);

	/**
	 *   @brief Get the bitrate of current audio profile selected.
	 *
	 *   @return bitrate of current audio profile.
	 */
	long GetAudioBitrate(void);

	/**
	 *   @brief Set a preferred bitrate for video.
	 *
	 *   @param[in] preferred bitrate.
	 */
	void SetVideoBitrate(long bitrate);


	/**
	 *   @brief Get available video bitrates.
	 *
	 *   @return available video bitrates.
	 */
	virtual std::vector<long> GetVideoBitrates(void) = 0;

	/**
	 *   @brief Get available audio bitrates.
	 *
	 *   @return available audio bitrates.
	 */
	virtual std::vector<long> GetAudioBitrates(void) = 0;

	/**
	 *   @brief Check if playback stalled in fragment collector side.
	 *
	 *   @return true if stalled, false otherwise.
	 */
	bool IsStreamerStalled(void) { return mIsPlaybackStalled; }

	/**
	 *   @brief Stop injection of fragments.
	 */
	virtual void StopInjection(void) = 0;

	/**
	 *   @brief Start injection of fragments.
	 */
	virtual void StartInjection(void) = 0;

	/**
	 *   @brief Check if current stream is muxed
	 *
	 *   @return true if current stream is muxed
	 */
	bool IsMuxedStream();

	/**
	 *   @brief Receives first video PTS for the current playback
	 *
	 *   @param[in] pts - pts value
	 *   @param[in] timeScale - time scale value
	 */
	virtual void NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale) { };

	/**
	 * @brief Kicks off subtitle display - sent at start of video presentation
	 * 
	 */
	virtual void StartSubtitleParser() { };

	/**
	 *   @brief Pause/unpause subtitles
	 *
	 *   @param pause - enable or disable pause
	 *   @return void
	 */
	virtual void PauseSubtitleParser(bool pause) { };

	/**
	 *   @brief Waits subtitle track injection until caught up with audio track.
	 *          Used internally by injection logic
	 *
	 *   @param None
	 *   @return void
	 */
	void WaitForAudioTrackCatchup(void);

	/**
	 *   @brief Unblock subtitle track injector if downloads are stopped
	 *
	 *   @return void
	 */
	void AbortWaitForAudioTrackCatchup(bool force);

	/**
	 *   @brief Set Client Side DAI object instance
	 *
	 *   @param[in] cdaiObj - Pointer to Client Side DAI object.
	 */
	virtual void SetCDAIObject(CDAIObject *cdaiObj) {};

	/**
	 *   @brief Checks if streamer reached end of stream
	 *
	 *   @return true if end of stream reached, false otherwise
	 */
	virtual bool IsEOSReached();

	/**
	 *   @brief Get available audio tracks.
	 *
	 *   @return std::vector<AudioTrackInfo> list of audio tracks
	 */
	virtual std::vector<AudioTrackInfo> &GetAvailableAudioTracks() { return mAudioTracks; };

	/**
	 *   @brief Get available text tracks.
	 *
	 *   @return std::vector<TextTrackInfo> list of text tracks
	 */
	virtual std::vector<TextTrackInfo> &GetAvailableTextTracks() { return mTextTracks; };

	/**
	*   @brief Update seek position when player is initialized
	*
	*   @param[in] seekposition time.
	*/
	virtual void SeekPosUpdate(double secondsRelativeToTuneTime) = 0;

	/*
	 *   @brief Function to returns last injected fragment position
	 *
	 *   @return double last injected fragment position in seconds
	 */
	double GetLastInjectedFragmentPosition();

	/**
	 *   @brief Function to process discontinuity.
	 *
	 *   @param[in] type - track type.
	 */
	bool ProcessDiscontinuity(TrackType type);

	/**
	 *   @brief Function to abort any wait for discontinuity by injector theads.
	 */
	void AbortWaitForDiscontinuity();

	/**
	 *   @brief Function to check if any media tracks are stalled on discontinuity.
	 *
	 *   @param[in] type - track type.
	 */
	void CheckForMediaTrackInjectionStall(TrackType type);

	/**
	 *   @brief Get buffered video duration in seconds
	 *
	 *   @return duration of currently buffered video in seconds
	 */
	double GetBufferedVideoDurationSec();

	/**
	 *   @brief Function to update stream info of current fetched fragment
	 *
	 *   @param[in]  profileIndex - profile index of current fetched fragment
	 *   @param[out]  cacheFragStreamInfo - stream info of current fetched fragment
	 */
	void UpdateStreamInfoBitrateData(int profileIndex, StreamInfo &cacheFragStreamInfo);

	/**
	 *   @brief Get current audio track
	 *
	 *   @return int - index of current audio track
	 */
	virtual int GetAudioTrack();

	/**
	 *   @brief Get current text track
	 *
	 *   @return int - index of current text track
	 */
	int GetTextTrack();

	/**
	 *   @brief Refresh subtitle track
	 *
	 *   @return void
	 */
	void RefreshSubtitles();

	/**
	 * @brief setVideoRectangle sets the position coordinates (x,y) & size (w,h) for OTA streams only
	 *
	 * @param[in] x,y - position coordinates of video rectangle
	 * @param[in] wxh - width & height of video rectangle
	 */
	virtual void SetVideoRectangle(int x, int y, int w, int h) {}

        /**
         *   @brief Get available thumbnail bitrates.
         *
         *   @return available thumbnail bitrates.
         */
        virtual std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) = 0;

        /**
         *   @brief Set thumbnail bitrate.
         *
         *   @return none.
         */
	virtual bool SetThumbnailTrack(int) = 0;

        /**
         *   @brief Get thumbnail data for duration value.
         *
         *   @return thumbnail data.
         */
	virtual std::vector<ThumbnailData> GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) = 0;
	
        /**
          * @brief SetAudioTrack set the audio track using index value. [currently for OTA]
          *
          * @param[in]
          * @param[in]
          */
        virtual void SetAudioTrack (int index) {}

        /**
          * @brief SetAudioTrackByLanguage set the audio language. [currently for OTA]
          *
          * @param[in] lang Language to be set
          * @param[in]
          */
        virtual void SetAudioTrackByLanguage(const char* lang) {}

        /**
          * @brief SetPreferredAudioLanguages set the preferred audio languages and rendition. [currently for OTA]
          *
          * @param[in]
          * @param[in]
          */
        virtual void SetPreferredAudioLanguages() {}

	/**
          * @brief Send a MUTE/UNMUTE packet to the subtitle renderer
          *
          * @param[in] mute mute/unmute
	  * @param[in] mute mute/unmute
          */
	void MuteSubtitles(bool mute);

		/**
	 * @brief Blocks aux track injection until caught up with video track.
	 *        Used internally by injection logic
	 *
	 * @param None
	 * @return void
	 */
	void WaitForVideoTrackCatchupForAux();

	/**
         *       @brief Set Content Restrictions
         *       @param[in] restrictions - restrictions to be applied
         *
         *       @return void
         */
	virtual void ApplyContentRestrictions(std::vector<std::string> restrictions){};

	/**
         *       @brief Disable Content Restrictions - unlock
         *       @param[in] secondsRelativeToCurrentTime -time till which the channel need to be kept unlocked
         *
         *       @return void
         */
	
        /**
	 *       @brief Disable Content Restrictions - unlock
	 *       @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 *       @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 *       @param[in] eventChange - disable restriction handling till next program event boundary
	 *
	 *       @return void
	 */
	virtual void DisableContentRestrictions(long grace, long time, bool eventChange){};

	/**
         *       @brief Enable Content Restrictions - lock
         *       @return void
         */
	virtual void EnableContentRestrictions(){};

	/**
	 * @brief Get audio forward to aux pipeline status
	 *
	 * @return bool true if audio buffers are to be forwarded
	 */
	bool GetAudioFwdToAuxStatus() { return mFwdAudioToAux; }

	/**
	 * @brief Set audio forward to aux pipeline status
	 *
	 * @param[in] status - enabled/disabled
	 * @return void
	 */
	void SetAudioFwdToAuxStatus(bool status) { mFwdAudioToAux = status; }

protected:
	/**
	 *   @brief Get stream information of a profile from subclass.
	 *
	 *   @param[in]  idx - profile index.
	 *   @return stream information corresponding to index.
	 */
	virtual StreamInfo* GetStreamInfo(int idx) = 0;

private:

	/**
	 * @brief Get desired profile based on cache
	 *
	 * @return Profile index
	 */
	int GetDesiredProfileBasedOnCache(void);

	/**
	 * @brief Update profile based on fragments downloaded.
	 *
	 * @return void
	 */
	void UpdateProfileBasedOnFragmentDownloaded(void);

	/**
	 * @brief Update profile based on fragment cache.
	 *
	 * @return bool
	 */
	bool UpdateProfileBasedOnFragmentCache(void);

	pthread_mutex_t mLock;              /**< lock for A/V track catchup logic*/
	pthread_cond_t mCond;               /**< condition for A/V track catchup logic*/
	pthread_cond_t mSubCond;            /**< condition for Audio/Subtitle track catchup logic*/
	pthread_cond_t mAuxCond;            /**< condition for Aux and video track catchup logic*/

	// abr variables
	long mCurrentBandwidth;             /**< stores current bandwidth*/
	int mLastVideoFragCheckedforABR;    /**< Last video fragment for which ABR is checked*/
	long mTsbBandwidth;                 /**< stores bandwidth when TSB is involved*/
	long mNwConsistencyBypass;          /**< Network consistency bypass**/
	int mABRHighBufferCounter;	    /**< ABR High buffer counter */
	int mABRLowBufferCounter;	    /**< ABR Low Buffer counter */
	int mMaxBufferCountCheck;
	int mABRMaxBuffer;			/**< ABR ramp up buffer*/
	int mABRCacheLength;			/**< ABR cache length*/
	int mABRMinBuffer;			/**< ABR ramp down buffer*/
	int mABRNwConsistency;			/**< ABR Network consistency*/
	bool mESChangeStatus;               /**< flag value which is used to call pipeline configuration if the audio type changed in mid stream */
	double mLastVideoFragParsedTimeMS;  /**< timestamp when last video fragment was parsed */

	bool mIsPaused;                     /**< paused state or not */
	long long mTotalPausedDurationMS;   /**< Total duration for which stream is paused */
	long long mStartTimeStamp;          /**< stores timestamp at which injection starts */
	long long mLastPausedTimeStamp;     /**< stores timestamp of last pause operation */
	pthread_mutex_t mStateLock;         /**< lock for A/V track discontinuity injection*/
	pthread_cond_t mStateCond;          /**< condition for A/V track discontinuity injection*/
	int mRampDownLimit;		/**< stores ramp down limit value */
	BitrateChangeReason mBitrateReason; /**< holds the reason for last bitrate change */
protected:
	ABRManager mAbrManager;             /**< Pointer to abr manager*/
	std::vector<AudioTrackInfo> mAudioTracks; /**< Available audio tracks */
	std::vector<TextTrackInfo> mTextTracks; /**< Available text tracks */
	MediaTrackDiscontinuityState mTrackState; /**< stores the discontinuity status of tracks*/
	std::string mAudioTrackIndex; /**< Current audio track index in track list */
	std::string mTextTrackIndex; /**< Current text track index in track list */
	bool mFwdAudioToAux;  /**< If audio buffers are to be forwarded to auxiliary pipeline, happens if both are playing same language */
};

#endif // STREAMABSTRACTIONAAMP_H
