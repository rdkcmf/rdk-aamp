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
 * @file streamabstraction.cpp
 * @brief Definition of common class functions used by fragment collectors.
 */

#include "StreamAbstractionAAMP.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <iterator>

#define AAMP_DEFAULT_BANDWIDTH_BYTES_PREALLOC (256*1024/8)
#define AAMP_STALL_CHECK_TOLERANCE 2

using namespace std;


/**
 * @brief Updates internal state after a fragment inject
 */
void MediaTrack::UpdateTSAfterInject()
{
	pthread_mutex_lock(&mutex);
	aamp_Free(&cachedFragment[fragmentIdxToInject].fragment.ptr);
	memset(&cachedFragment[fragmentIdxToInject], 0, sizeof(CachedFragment));
	fragmentIdxToInject++;
	if (fragmentIdxToInject == MAX_CACHED_FRAGMENTS_PER_TRACK)
	{
		fragmentIdxToInject = 0;
	}
	numberOfFragmentsCached--;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] updated fragmentIdxToInject = %d numberOfFragmentsCached %d\n", __FUNCTION__, __LINE__,
		        name, fragmentIdxToInject, numberOfFragmentsCached);
	}
#endif
	pthread_cond_signal(&fragmentInjected);
	pthread_mutex_unlock(&mutex);
}


/**
 * @brief Updates internal state after a fragment fetch
 */
void MediaTrack::UpdateTSAfterFetch()
{
	bool notifyCacheCompleted = false;
	pthread_mutex_lock(&mutex);
	cachedFragment[fragmentIdxToFetch].profileIndex = GetContext()->profileIdxForBandwidthNotification;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] before update fragmentIdxToFetch = %d numberOfFragmentsCached %d\n",
		        __FUNCTION__, __LINE__, name, fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	totalFetchedDuration += cachedFragment[fragmentIdxToFetch].duration;
	size_t fragmentLen = cachedFragment[fragmentIdxToFetch].fragment.len;
	if (fetchBufferPreAllocLen < fragmentLen)
	{
		logprintf("%s:%d [%s] Update fetchBufferPreAllocLen[%u]->[%u]\n", __FUNCTION__, __LINE__,
		        name, fetchBufferPreAllocLen, fragmentLen);
		fetchBufferPreAllocLen = fragmentLen;
	}
	else
	{
		traceprintf("%s:%d [%s] fetchBufferPreAllocLen[%u] fragment.len[%u] diff %u\n", __FUNCTION__, __LINE__,
		        name, fetchBufferPreAllocLen, fragmentLen, (fetchBufferPreAllocLen - fragmentLen));
	}

	if((eTRACK_VIDEO == type) && aamp->IsFragmentBufferingRequired())
	{
		if(!notifiedCachingComplete)
		{
			cacheDurationSeconds += cachedFragment[fragmentIdxToFetch].duration;
			if(cacheDurationSeconds >= gpGlobalConfig->minVODCacheSeconds)
			{
				logprintf("## %s:%d [%s] Caching Complete cacheDuration %d minVODCacheSeconds %d##\n", __FUNCTION__, __LINE__, name, cacheDurationSeconds, gpGlobalConfig->minVODCacheSeconds);
				notifyCacheCompleted = true;
			}
			else
			{
				logprintf("## %s:%d [%s] Caching Ongoing cacheDuration %d minVODCacheSeconds %d##\n", __FUNCTION__, __LINE__, name, cacheDurationSeconds, gpGlobalConfig->minVODCacheSeconds);
			}
		}
	}
	fragmentIdxToFetch++;
	if (fragmentIdxToFetch == MAX_CACHED_FRAGMENTS_PER_TRACK)
	{
		fragmentIdxToFetch = 0;
	}
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		if (numberOfFragmentsCached == 0)
		{
			logprintf("## %s:%d [%s] Caching fragment for track when numberOfFragmentsCached is 0 ##\n", __FUNCTION__, __LINE__, name);
		}
	}
#endif
	numberOfFragmentsCached++;
	assert(numberOfFragmentsCached <= MAX_CACHED_FRAGMENTS_PER_TRACK);
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] updated fragmentIdxToFetch = %d numberOfFragmentsCached %d\n",
			__FUNCTION__, __LINE__, name, fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	totalFragmentsDownloaded++;
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);
	if(notifyCacheCompleted)
	{
		aamp->NotifyFragmentCachingComplete();
		notifiedCachingComplete = true;
	}
}


/**
 * @brief Wait until a free fragment is available.
 * @note To be called before fragment fetch by subclasses
 * @param timeoutMs - timeout in milliseconds. -1 for infinite wait
 * @retval true if fragment available, false on abort.
 */
bool MediaTrack::WaitForFreeFragmentAvailable( int timeoutMs)
{
	bool ret = true;
	int pthreadReturnValue = 0;

	pthread_mutex_lock(&mutex);
	if(abort)
	{
		ret = false;
	}
	else if (numberOfFragmentsCached == MAX_CACHED_FRAGMENTS_PER_TRACK)
	{
		if (timeoutMs >= 0)
		{
			struct timespec tspec;
			struct timeval tv;
			gettimeofday(&tv, NULL);
			tspec.tv_sec = time(NULL) + timeoutMs / 1000;
			tspec.tv_nsec = (long)(tv.tv_usec * 1000 + 1000 * 1000 * (timeoutMs % 1000));
			tspec.tv_sec += tspec.tv_nsec / (1000 * 1000 * 1000);
			tspec.tv_nsec %= (1000 * 1000 * 1000);

			pthreadReturnValue = pthread_cond_timedwait(&fragmentInjected, &mutex, &tspec);

			if (ETIMEDOUT == pthreadReturnValue)
			{
				ret = false;
			}
			else if (0 != pthreadReturnValue)
			{
				logprintf("%s:%d [%s] pthread_cond_timedwait returned %s\n", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
				ret = false;
			}
		}
		else
		{
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				logprintf("%s:%d [%s] waiting for fragmentInjected condition\n", __FUNCTION__, __LINE__, name);
			}
#endif
			pthreadReturnValue = pthread_cond_wait(&fragmentInjected, &mutex);

			if (0 != pthreadReturnValue)
			{
				logprintf("%s:%d [%s] pthread_cond_wait returned %s\n", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
				ret = false;
			}
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				logprintf("%s:%d [%s] wait complete for fragmentInjected\n", __FUNCTION__, __LINE__, name);
			}
#endif
		}
		if(abort)
		{
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				logprintf("%s:%d [%s] abort set, returning false\n", __FUNCTION__, __LINE__, name);
			}
#endif
			ret = false;
		}
	}
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] fragmentIdxToFetch = %d numberOfFragmentsCached %d\n",
			__FUNCTION__, __LINE__, name, fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	pthread_mutex_unlock(&mutex);
	return ret;
}

/**
 * @brief Wait until a cached fragment is available.
 * @retval true if fragment available, false on abort.
 */
bool MediaTrack::WaitForCachedFragmentAvailable()
{
	bool ret;
	pthread_mutex_lock(&mutex);
	if ((numberOfFragmentsCached == 0) && (!abort))
	{
#ifdef AAMP_DEBUG_FETCH_INJECT
		if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
		{
			logprintf("## %s:%d [%s] Waiting for CachedFragment to be available, eosReached=%d ##\n", __FUNCTION__, __LINE__, name, eosReached);
		}
#endif
		if (!eosReached)
		{
			pthread_cond_wait(&fragmentFetched, &mutex);
		}
	}
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] fragmentIdxToInject = %d numberOfFragmentsCached %d\n",
			__FUNCTION__, __LINE__, name, fragmentIdxToInject, numberOfFragmentsCached);
	}
#endif
	ret = !(abort || (numberOfFragmentsCached == 0));
	pthread_mutex_unlock(&mutex);
	return ret;
}


/**
 * @brief Aborts wait for fragment.
 * @param[in] immediate Indicates immediate abort as in a seek/ stop
 */
void MediaTrack::AbortWaitForCachedFragment( bool immediate)
{
	pthread_mutex_lock(&mutex);
	if (immediate)
	{
		abort = true;
#ifdef AAMP_DEBUG_FETCH_INJECT
		if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
		{
			logprintf("%s:%d [%s] signal fragmentInjected condition\n", __FUNCTION__, __LINE__, name);
		}
#endif
		pthread_cond_signal(&fragmentInjected);
	}
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);
}



/**
 * @brief Inject next cached fragment
 */
bool MediaTrack::InjectFragment()
{
	bool ret = true;
	aamp->BlockUntilGstreamerWantsData(NULL, 0, type);

	if (WaitForCachedFragmentAvailable())
	{
		bool stopInjection = false;
		bool fragmentDiscarded = false;
		CachedFragment* cachedFragment = &this->cachedFragment[fragmentIdxToInject];
#ifdef TRACE
		logprintf("%s:%d [%s] - fragmentIdxToInject %d cachedFragment %p ptr %p\n", __FUNCTION__, __LINE__,
				name, fragmentIdxToInject, cachedFragment, cachedFragment->fragment.ptr);
#endif
		if (cachedFragment->fragment.ptr)
		{
			StreamAbstractionAAMP*  context = GetContext();
			if (cachedFragment->discontinuity &&  (1.0 == context->aamp->rate))
			{
				logprintf("%s:%d - track %s- notifying aamp discontinuity\n", __FUNCTION__, __LINE__, name);
				cachedFragment->discontinuity = false;
				stopInjection = aamp->Discontinuity((MediaType) type);
				/*For muxed streams, give discontinuity for audio track as well*/
				if (!context->GetMediaTrack(eTRACK_AUDIO)->enabled)
				{
					aamp->Discontinuity(eMEDIATYPE_AUDIO);
				}
				if (stopInjection)
				{
					ret = false;
					discontinuityProcessed = true;
					logprintf("%s:%d - stopping injection\n", __FUNCTION__, __LINE__);
				}
				else
				{
					logprintf("%s:%d - continuing injection\n", __FUNCTION__, __LINE__);
				}
			}
			if (!stopInjection)
			{
#ifdef AAMP_DEBUG_INJECT
				if ((1 << type) & AAMP_DEBUG_INJECT)
				{
					logprintf("%s:%d [%s] Inject uri %s\n", __FUNCTION__, __LINE__, name, cachedFragment->uri);
				}
#endif
#ifndef SUPRESS_DECODE
#ifndef FOG_HAMMER_TEST // support aamp stress-tests of fog without video decoding/presentation
				InjectFragmentInternal(cachedFragment, fragmentDiscarded);
#endif
#endif
				if (GetContext()->mIsFirstBuffer && !fragmentDiscarded)
				{
					GetContext()->mIsFirstBuffer = false;
					aamp->NotifyFirstBufferProcessed();
				}
				if (eTRACK_VIDEO == type)
				{
					GetContext()->NotifyBitRateUpdate(cachedFragment->profileIndex);
				}
				AAMPLOG_TRACE("%s:%d [%p] - %s - injected cached uri at pos %f dur %f\n", __FUNCTION__, __LINE__, this, name, cachedFragment->position, cachedFragment->duration);
				if (!fragmentDiscarded)
				{
					totalInjectedDuration += cachedFragment->duration;
					mSegInjectFailCount = 0;
				}
				else
				{
					logprintf("%s:%d [%s] - Not updating totalInjectedDuration since fragment is Discarded\n", __FUNCTION__, __LINE__, name);
					mSegInjectFailCount++;
					if(MAX_SEG_INJECT_FAIL_COUNT <= mSegInjectFailCount)
					{
						ret	= false;
						logprintf("%s:%d [%s] Reached max inject failure count , stopping playback\n",__FUNCTION__, __LINE__, name);
						aamp->SendErrorEvent(AAMP_TUNE_UNSUPPORTED_STREAM_TYPE);
					}
					
				}
				UpdateTSAfterInject();
			}
		}
		else
		{
			if (eosReached)
			{
				//Save the playback rate prior to sending EOS
				double rate = GetContext()->aamp->rate;
				aamp->EndOfStreamReached((MediaType)type);
				/*For muxed streams, provide EOS for audio track as well since
				 * no separate MediaTrack for audio is present*/
				MediaTrack* audio = GetContext()->GetMediaTrack(eTRACK_AUDIO);
				if (audio && !audio->enabled && rate == 1.0)
				{
					aamp->EndOfStreamReached(eMEDIATYPE_AUDIO);
				}
			}
			else
			{
				logprintf("%s:%d - %s - NULL ptr to inject. fragmentIdxToInject %d\n", __FUNCTION__, __LINE__, name, fragmentIdxToInject);
			}
			ret = false;
		}
	}
	else
	{
		logprintf("WaitForCachedFragmentAvailable %s aborted\n", name);
		if (eosReached)
		{
			//Save the playback rate prior to sending EOS
			double rate = GetContext()->aamp->rate;
			aamp->EndOfStreamReached((MediaType)type);
			/*For muxed streams, provide EOS for audio track as well since
			 * no separate MediaTrack for audio is present*/
			MediaTrack* audio = GetContext()->GetMediaTrack(eTRACK_AUDIO);
			if (audio && !audio->enabled && rate == 1.0)
			{
				aamp->EndOfStreamReached(eMEDIATYPE_AUDIO);
			}
		}
		ret = false;
	}
	return ret;
} // InjectFragment



/**
 * @brief Fragment injector thread
 * @param arg Pointer to MediaTrack
 * @retval NULL
 */
static void *FragmentInjector(void *arg)
{
	MediaTrack *track = (MediaTrack *)arg;
	if(pthread_setname_np(pthread_self(), "aampInjector"))
	{
		logprintf("%s:%d: pthread_setname_np failed\n", __FUNCTION__, __LINE__);
	}
	track->RunInjectLoop();
	return NULL;
}



/**
 * @brief Starts inject loop of track
 */
void MediaTrack::StartInjectLoop()
{
	abort = false;
	discontinuityProcessed = false;
	assert(!fragmentInjectorThreadStarted);
	if (0 == pthread_create(&fragmentInjectorThreadID, NULL, &FragmentInjector, this))
	{
		fragmentInjectorThreadStarted = true;
	}
	else
	{
		logprintf("Failed to create FragmentInjector thread\n");
	}
}


/**
 * @brief Injection loop - use internally by injection logic
 */
void MediaTrack::RunInjectLoop()
{
	const bool isAudioTrack = (eTRACK_AUDIO == type);
	bool keepInjecting = true;
	while (aamp->DownloadsAreEnabled() && keepInjecting)
	{
		if (!InjectFragment())
		{
			keepInjecting = false;
		}
		if(isAudioTrack)
		{
			GetContext()->WaitForVideoTrackCatchup();
		}
		else
		{
			GetContext()->ReassessAndResumeAudioTrack();
		}
	}
	AAMPLOG_WARN("fragment injector done. track %s\n", name);
}


/**
 * @brief Stop inject loop of track
 */
void MediaTrack::StopInjectLoop()
{
	if (fragmentInjectorThreadStarted)
	{
		void *value_ptr = NULL;
		int rc = pthread_join(fragmentInjectorThreadID, &value_ptr);
		if (rc != 0)
		{
			logprintf("***pthread_join fragmentInjectorThread returned %d(%s)\n", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			logprintf("joined fragmentInjectorThread\n");
		}
#endif
	}
	fragmentInjectorThreadStarted = false;
}


/**
 * @brief Check if a track is enabled
 * @retval true if enabled, false if disabled
 */
bool MediaTrack::Enabled()
{
	return enabled;
}


/**
 * @brief Get buffer to fetch and cache next fragment.
 * @param[in] initialize true to initialize the fragment.
 * @retval Pointer to fragment buffer.
 */
CachedFragment* MediaTrack::GetFetchBuffer(bool initialize)
{
	/*Make sure fragmentDurationSeconds updated before invoking this*/
	CachedFragment* cachedFragment = &this->cachedFragment[fragmentIdxToFetch];
	if(initialize)
	{
		if (cachedFragment->fragment.ptr)
		{
			logprintf("%s:%d fragment.ptr already set - possible memory leak\n", __FUNCTION__, __LINE__);
		}
		memset(&cachedFragment->fragment, 0x00, sizeof(GrowableBuffer));
		double duration = (0 == fragmentDurationSeconds)?(2.0):fragmentDurationSeconds;
		size_t estimatedFragmentSizeFromBW = bandwidthBytesPerSecond * duration * 1.5;
		if (estimatedFragmentSizeFromBW > fetchBufferPreAllocLen)
		{
			logprintf("%s:%d [%s] Update fetchBufferPreAllocLen[%u]->[%u]\n", __FUNCTION__, __LINE__,
			        name, fetchBufferPreAllocLen, estimatedFragmentSizeFromBW);
			fetchBufferPreAllocLen = estimatedFragmentSizeFromBW;
		}
		traceprintf ("%s:%d [%s] bandwidthBytesPerSecond %d fragmentDurationSeconds %f fetchBufferPreAllocLen %d, estimatedFragmentSizeFromBW %d\n",
				__FUNCTION__, __LINE__, name, bandwidthBytesPerSecond, fragmentDurationSeconds, fetchBufferPreAllocLen, estimatedFragmentSizeFromBW);
		aamp_Malloc(&cachedFragment->fragment, fetchBufferPreAllocLen);
	}
	return cachedFragment;
}


/**
 * @brief Set current bandwidth of track
 * @param bandwidthBps bandwidth in bits per second
 */
void MediaTrack::SetCurrentBandWidth(int bandwidthBps)
{
	this->bandwidthBytesPerSecond = bandwidthBps/8;
}

/**
 * @brief Get current bandwidth of track
 * @return bandwidth in bytes per second
 */
int MediaTrack::GetCurrentBandWidth()
{
	return this->bandwidthBytesPerSecond;
}


/**
 * @brief MediaTrack Constructor
 * @param type Type of track
 * @param aamp Pointer to associated aamp instance
 * @param name Name of the track
 */
MediaTrack::MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name) :
		eosReached(false), enabled(false), numberOfFragmentsCached(0), fragmentIdxToInject(0),
		fragmentIdxToFetch(0), abort(false), fragmentInjectorThreadID(0), totalFragmentsDownloaded(0),
		fragmentInjectorThreadStarted(false), totalInjectedDuration(0), cacheDurationSeconds(0),
		notifiedCachingComplete(false), fragmentDurationSeconds(0), segDLFailCount(0),segDrmDecryptFailCount(0),mSegInjectFailCount(0),
		bandwidthBytesPerSecond(AAMP_DEFAULT_BANDWIDTH_BYTES_PREALLOC), totalFetchedDuration(0), fetchBufferPreAllocLen(0), discontinuityProcessed(false)
{
	this->type = type;
	this->aamp = aamp;
	this->name = name;
	memset(&cachedFragment[0], 0, sizeof(cachedFragment));
	pthread_cond_init(&fragmentFetched, NULL);
	pthread_cond_init(&fragmentInjected, NULL);
	pthread_mutex_init(&mutex, NULL);
}


/**
 * @brief MediaTrack Destructor
 */
MediaTrack::~MediaTrack()
{
	for (int j=0; j< MAX_CACHED_FRAGMENTS_PER_TRACK; j++)
	{
		aamp_Free(&cachedFragment[j].fragment.ptr);
	}
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&fragmentFetched);
	pthread_cond_destroy(&fragmentInjected);
}

/**
 *   @brief Unblocks track if caught up with video or downloads are stopped
 *
 */
void StreamAbstractionAAMP::ReassessAndResumeAudioTrack()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if( audio && video )
	{
		pthread_mutex_lock(&mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();
		if(audioDuration < (videoDuration + (2 * video->fragmentDurationSeconds)) || !aamp->DownloadsAreEnabled() || video->IsDiscontinuityProcessed())
		{
			pthread_cond_signal(&mCond);
#ifdef AAMP_DEBUG_FETCH_INJECT
			logprintf("\n%s:%d signalling cond - audioDuration %f videoDuration %f\n",
				__FUNCTION__, __LINE__, audioDuration, videoDuration);
#endif
		}
		pthread_mutex_unlock(&mLock);
	}
}


/**
 *   @brief Waits track injection until caught up with video track.
 *   Used internally by injection logic
 */
void StreamAbstractionAAMP::WaitForVideoTrackCatchup()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);

	pthread_mutex_lock(&mLock);
	double audioDuration = audio->GetTotalInjectedDuration();
	double videoDuration = video->GetTotalInjectedDuration();
	if ((audioDuration > (videoDuration +  video->fragmentDurationSeconds)) && aamp->DownloadsAreEnabled() && !audio->IsDiscontinuityProcessed())
	{
#ifdef AAMP_DEBUG_FETCH_INJECT
		logprintf("\n%s:%d waiting for cond - audioDuration %f videoDuration %f\n",
			__FUNCTION__, __LINE__, audioDuration, videoDuration);
#endif
		pthread_cond_wait(&mCond, &mLock);
	}
	pthread_mutex_unlock(&mLock);
}


/**
 * @brief StreamAbstractionAAMP Constructor
 * @param[in] aamp pointer to PrivateInstanceAAMP object associated with stream
 */
StreamAbstractionAAMP::StreamAbstractionAAMP(PrivateInstanceAAMP* aamp):
		trickplayMode(false), currentProfileIndex(0), mCurrentBandwidth(0),
		mTsbBandwidth(0),mNwConsistencyBypass(true), profileIdxForBandwidthNotification(0),
		hasDrm(false), mIsAtLivePoint(false), mIsFirstBuffer(true), mESChangeStatus(false)
{
#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	mIsPlaybackStalled = false;
#endif
	traceprintf("StreamAbstractionAAMP::%s\n", __FUNCTION__);
	pthread_mutex_init(&mLock, NULL);
	pthread_cond_init(&mCond, NULL);

	// Set default init bitrate according to the config.
	mAbrManager.setDefaultInitBitrate(gpGlobalConfig->defaultBitrate);

	this->aamp = aamp;
}


/**
 * @brief StreamAbstractionAAMP Destructor
 */
StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
	traceprintf("StreamAbstractionAAMP::%s\n", __FUNCTION__);
	pthread_cond_destroy(&mCond);
	pthread_mutex_destroy(&mLock);
	AAMPLOG_INFO("Exit StreamAbstractionAAMP::%s\n", __FUNCTION__);
}


/**
 *   @brief Get the desired profile to start fetching.
 *
 *   @param getMidProfile
 *   @retval profile index to be used for the track.
 */
int StreamAbstractionAAMP::GetDesiredProfile(bool getMidProfile)
{
	int desiredProfileIndex;
	if (this->trickplayMode && ABRManager::INVALID_PROFILE != mAbrManager.getLowestIframeProfile())
	{
		desiredProfileIndex = GetIframeTrack();
	}
	else
	{
		desiredProfileIndex = mAbrManager.getInitialProfileIndex(getMidProfile);
	}
	profileIdxForBandwidthNotification = desiredProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if(video)
	{
		video->SetCurrentBandWidth(GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond);
	}
	else
	{
		AAMPLOG_TRACE("%s:%d video track NULL\n", __FUNCTION__, __LINE__);
	}
	AAMPLOG_TRACE("%s:%d profileIdxForBandwidthNotification updated to %d \n", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);

	return desiredProfileIndex;
}

/**
 *   @brief Notify bitrate updates to application.
 *   Used internally by injection logic
 *
 *   @param[in]  profileIndex - profile index of last injected fragment.
 */
void StreamAbstractionAAMP::NotifyBitRateUpdate(int profileIndex)
{
	if (profileIndex != aamp->GetPersistedProfileIndex())
	{
		StreamInfo* streamInfo = GetStreamInfo(profileIndex);

		bool lGetBWIndex = false;
		/* START: Added As Part of DELIA-28363 and DELIA-28247 */
		if(aamp->IsTuneTypeNew &&
			streamInfo->bandwidthBitsPerSecond == (GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond))
		{
			MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
			logprintf("NotifyBitRateUpdate: Max BitRate: %ld, timetotop: %f\n",
				streamInfo->bandwidthBitsPerSecond, video->GetTotalInjectedDuration());
			aamp->IsTuneTypeNew = false;
			lGetBWIndex = true;
		}
		/* END: Added As Part of DELIA-28363 and DELIA-28247 */

		// Send bitrate notification
		aamp->NotifyBitRateChangeEvent(streamInfo->bandwidthBitsPerSecond,
		        "BitrateChanged - Network Adaptation", streamInfo->resolution.width,
		        streamInfo->resolution.height, lGetBWIndex);
		// Store the profile , compare it before sending it . This avoids sending of event after trickplay if same bitrate
		aamp->SetPersistedProfileIndex(profileIndex);
	}
}



/**
 * @brief Update profile state based on bandwidth of fragments downloaded
 */
void StreamAbstractionAAMP::UpdateProfileBasedOnFragmentDownloaded(void)
{
	// This function checks for bandwidth change based on the fragment url from FOG
	int desiredProfileIndex = 0;
	if (mCurrentBandwidth != mTsbBandwidth)
	{
		// a) Check if network bandwidth changed from starting bw
		// b) Check if netwwork bandwidth is different from persisted bandwidth( needed for first time reporting)
		// find the profile for the newbandwidth
		desiredProfileIndex = mAbrManager.getBestMatchedProfileIndexByBandWidth(mTsbBandwidth);
		mCurrentBandwidth = mTsbBandwidth;
		profileIdxForBandwidthNotification = desiredProfileIndex;
		traceprintf("%s:%d profileIdxForBandwidthNotification updated to %d \n", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
		GetMediaTrack(eTRACK_VIDEO)->SetCurrentBandWidth(GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond);
	}
}


/**
 * @brief Get desired profile based on cached duration
 * @retval index of desired profile based on cached duration
 */
int StreamAbstractionAAMP::GetDesiredProfileBasedOnCache(void)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	int desiredProfileIndex = currentProfileIndex;

	if (this->trickplayMode)
	{
		int tmpIframeProfile = GetIframeTrack();
		if(tmpIframeProfile != ABRManager::INVALID_PROFILE)
			desiredProfileIndex = tmpIframeProfile;
	}
	/*In live, fog takes care of ABR, and cache updating is not based only on bandwidth,
	 * but also depends on fragment availability in CDN*/
	else
	{
		long currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
		long networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
		int nwConsistencyCnt = (mNwConsistencyBypass)?1:gpGlobalConfig->abrNwConsistency;
		// Ramp up/down (do ABR)
		desiredProfileIndex = mAbrManager.getProfileIndexByBitrateRampUpOrDown(currentProfileIndex,
				currentBandwidth, networkBandwidth, nwConsistencyCnt);
		if(currentProfileIndex != desiredProfileIndex)
		{
			logprintf("aamp::GetDesiredProfileBasedOnCache---> currbw[%ld] nwbw[%ld] currProf[%d] desiredProf[%d] vidCache[%d]\n",currentBandwidth,networkBandwidth,currentProfileIndex,desiredProfileIndex,video->numberOfFragmentsCached);
		}
	}
	// only for first call, consistency check is ignored
	mNwConsistencyBypass = false;

	return desiredProfileIndex;
}


/**
 * @brief Rampdown profile
 * @retval true on profile change
 */
bool StreamAbstractionAAMP::RampDownProfile()
{
	bool ret = false;
	int desiredProfileIndex = currentProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if (this->trickplayMode)
	{
		//We use only second last and lowest profiles for iframes
		int lowestIframeProfile = mAbrManager.getLowestIframeProfile();
		if (desiredProfileIndex != lowestIframeProfile)
		{
			desiredProfileIndex = lowestIframeProfile;
		}
	}
	else
	{
		desiredProfileIndex = mAbrManager.getRampedDownProfileIndex(currentProfileIndex);
	}
	if (desiredProfileIndex != currentProfileIndex)
	{
		this->currentProfileIndex = desiredProfileIndex;
		profileIdxForBandwidthNotification = desiredProfileIndex;
		traceprintf("%s:%d profileIdxForBandwidthNotification updated to %d \n", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
		ret = true;
		video->SetCurrentBandWidth(GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond);

		// Send abr notification to XRE
		video->ABRProfileChanged();
	}

	return ret;
}

/**
 *   @brief Check for ramdown profile.
 *
 *   @param http_error
 *   @retval true if rampdown needed in the case of fragment not available in higher profile.
 */
bool StreamAbstractionAAMP::CheckForRampDownProfile(long http_error)
{
	bool retValue = false;

	if (!aamp->IsTSBSupported())
	{
		if (http_error == 404 || http_error == 500 || http_error == 503)
		{
			if (RampDownProfile())
			{
				AAMPLOG_INFO("StreamAbstractionAAMP::%s:%d > Condition Rampdown Success\n", __FUNCTION__, __LINE__);
				retValue = true;
			}
		}
		//For timeout, rampdown in single steps might not be enough
		else if (http_error == CURLE_OPERATION_TIMEDOUT)
		{
			if(UpdateProfileBasedOnFragmentCache())
				retValue = true;
		}
	}

	return retValue;
}


/**
 *   @brief Checks and update profile based on bandwidth.
 */
void StreamAbstractionAAMP::CheckForProfileChange(void)
{
	// FOG based
	if(aamp->IsTSBSupported())
	{
		// This is for FOG based download , where bandwidth is calculated based on downloaded fragment file name
		// No profile change will be done or manifest download triggered based on profilechange
		UpdateProfileBasedOnFragmentDownloaded();
	}
	else
	{
		MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
		bool checkProfileChange = true;
		//Avoid doing ABR during initial buffering which will affect tune times adversely
		if (video->GetTotalFetchedDuration() > 0 && video->GetTotalFetchedDuration() < gpGlobalConfig->abrSkipDuration)
		{
			//For initial fragment downloads, check available bw is less than default bw
			long availBW = aamp->GetCurrentlyAvailableBandwidth();
			long currBW = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;

			//If available BW is less than current selected one, we need ABR
			if (availBW > 0 && availBW < currBW)
			{
				logprintf("%s:%d Changing profile due to low available bandwidth(%ld) than default(%ld)!! \n", __FUNCTION__, __LINE__, availBW, currBW);
			}
			else
			{
				checkProfileChange = false;
			}
		}

		if (checkProfileChange)
		{
			UpdateProfileBasedOnFragmentCache();
		}
	}
}


/**
 *   @brief Get iframe track index.
 *   This shall be called only after UpdateIframeTracks() is done
 *
 *   @retval iframe track index.
 */
int StreamAbstractionAAMP::GetIframeTrack()
{
	return mAbrManager.getDesiredIframeProfile();
}


/**
 *   @brief Update iframe tracks.
 *   Subclasses shall invoke this after StreamInfo is populated .
 */
void StreamAbstractionAAMP::UpdateIframeTracks()
{
	mAbrManager.updateProfile();
}

void StreamAbstractionAAMP::NotifyPlaybackPaused(bool paused)
{
	if (paused)
	{
		mIsAtLivePoint = false;
	}
}

#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED

/**
 *   @brief Check if playback is stalled in streamer.
 *
 *   @param expectedInjectedDuration
 *   @retval true if playback stalled, false otherwise.
 */
bool StreamAbstractionAAMP::CheckIfPlaybackStalled(double expectedInjectedDuration)
{
	// positionMS is currentPosition that will be sent in MediaProgress event
	// make sure that value never exceeds total injected duration

	MediaTrack *videoTrack = GetMediaTrack(eTRACK_VIDEO);

	if (videoTrack && (expectedInjectedDuration > (videoTrack->GetTotalInjectedDuration() + AAMP_STALL_CHECK_TOLERANCE)))
	{
		traceprintf("StreamAbstractionAAMP:%s() expectedInjectedDuration %f injectedDuration %f\n", __FUNCTION__, expectedInjectedDuration, videoTrack->GetTotalInjectedDuration());
		// Also check if internal cache and gstreamer cache are also empty
		if (videoTrack->numberOfFragmentsCached == 0 && aamp->IsSinkCacheEmpty(eMEDIATYPE_VIDEO) && mIsPlaybackStalled)
		{
			logprintf("StreamAbstractionAAMP:%s() Stall detected. Buffer status is RED! expectedInjectedDuration %f injectedDuration %f\n", __FUNCTION__, expectedInjectedDuration, videoTrack->GetTotalInjectedDuration());
			return true;
		}
	}

	return false;
}
#else

/**
 *   @brief Check if player caches are running dry.
 *
 *   @return true if player caches are running dry, false otherwise.
 */
bool StreamAbstractionAAMP::CheckIfPlayerRunningDry()
{
	MediaTrack *videoTrack = GetMediaTrack(eTRACK_VIDEO);
	MediaTrack *audioTrack = GetMediaTrack(eTRACK_AUDIO);

	if (!audioTrack || !videoTrack)
	{
		return false;
	}
	bool videoBufferIsEmpty = videoTrack->numberOfFragmentsCached == 0 && aamp->IsSinkCacheEmpty(eMEDIATYPE_VIDEO);
	bool audioBufferIsEmpty = (audioTrack->Enabled() ? (audioTrack->numberOfFragmentsCached == 0) : true) && aamp->IsSinkCacheEmpty(eMEDIATYPE_AUDIO);
	if (videoBufferIsEmpty && audioBufferIsEmpty)
	{
		logprintf("StreamAbstractionAAMP:%s() Stall detected. Buffer status is RED!\n", __FUNCTION__);
		return true;
	}
	return false;
}
#endif


/**
 * @brief Update profile state based on cached fragments
 */
bool StreamAbstractionAAMP::UpdateProfileBasedOnFragmentCache()
{
	bool retVal = false;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	int desiredProfileIndex = GetDesiredProfileBasedOnCache();
	if (desiredProfileIndex != currentProfileIndex)
	{
		logprintf("\n\n**aamp changing profile: %d->%d [%ld->%ld]\n\n",
			currentProfileIndex, desiredProfileIndex,
			GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond,
			GetStreamInfo(desiredProfileIndex)->bandwidthBitsPerSecond);
		this->currentProfileIndex = desiredProfileIndex;
		profileIdxForBandwidthNotification = desiredProfileIndex;
		traceprintf("%s:%d profileIdxForBandwidthNotification updated to %d \n", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
		video->ABRProfileChanged();
		video->SetCurrentBandWidth(GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond);
		retVal = true;
	}
	return retVal;
}
