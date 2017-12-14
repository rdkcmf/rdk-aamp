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
* @file mediatrack.cpp
* Definition of common class functions used by fragment collectors.
*/

#include "StreamAbstractionAAMP.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <iterator>
#include <sys/time.h>
#define AAMP_DEFAULT_BANDWIDTH_BYTES_PREALLOC (512*1024/8)
#define AAMP_STALL_CHECK_TOLERANCE 2
#define AAMP_BUFFER_MONITOR_GREEN_THRESHOLD 6 //3 fragments for Comcast linear streams.

using namespace std;
struct IframeTrackInfo
{
    long bandwidth;
    int idx;
};

static gboolean BufferHealthMonitor(gpointer user_data)
{
	MediaTrack* mediaTrack = (MediaTrack*) user_data;
	mediaTrack->MonitorBufferHealth();
	return G_SOURCE_CONTINUE;
}

static gboolean BufferHealthMonitorSchedule(gpointer user_data)
{
	MediaTrack* mediaTrack = (MediaTrack*) user_data;
	mediaTrack->ScheduleBufferHealthMonitor();
	return G_SOURCE_REMOVE;
}

const char* MediaTrack::GetBufferHealthStatusString(BufferHealthStatus status)
{
	const char* ret;
	switch (status)
	{
		default:
		case BUFFER_STATUS_GREEN:
			ret = "GREEN";
			break;
		case BUFFER_STATUS_YELLOW:
			ret = "YELLOW";
			break;
		case BUFFER_STATUS_RED:
			ret = "RED";
			break;
	}
	return ret;
}

void MediaTrack::ScheduleBufferHealthMonitor()
{
	pthread_mutex_lock(&mutex);
	if (!abort)
	{
		bufferHealthMonitorIdleTaskId = g_timeout_add_seconds(gpGlobalConfig->bufferHealthMonitorInterval, BufferHealthMonitor, this);
	}
	pthread_mutex_unlock(&mutex);
}

void MediaTrack::MonitorBufferHealth()
{
	pthread_mutex_lock(&mutex);
#ifdef BUFFER_MONITOR_USE_SINK_CACHE_QUERY
	if (BUFFER_STATUS_YELLOW == bufferStatus)
	{
		if (aamp->IsSinkCacheEmpty((MediaType) type) )
		{
			bufferStatus = BUFFER_STATUS_RED;
		}
	}
#else
	double bufferedTime = cacheDurationSeconds - GetContext()->GetElapsedTime();
	traceprintf("%s:%d [%s] bufferedTime %f current time-stamp %lld\n", __FUNCTION__, __LINE__, name, bufferedTime, aamp_GetCurrentTimeMS());
	if (bufferedTime <= 0)
	{
		bufferStatus = BUFFER_STATUS_RED;
	}
	else if (bufferedTime > AAMP_BUFFER_MONITOR_GREEN_THRESHOLD)
	{
		bufferStatus = BUFFER_STATUS_GREEN;
	}
	else
	{
		bufferStatus = BUFFER_STATUS_YELLOW;
	}
#endif
	if (bufferStatus != prevBufferStatus)
	{
		logprintf("aamp: track[%s] buffering %s->%s\n", name, GetBufferHealthStatusString(prevBufferStatus),
		        GetBufferHealthStatusString(bufferStatus));
		prevBufferStatus = bufferStatus;
	}
	else
	{
		traceprintf("%s:%d track[%s] No Change [%s]\n", __FUNCTION__, __LINE__, name,
		        GetBufferHealthStatusString(bufferStatus));
	}
	pthread_mutex_unlock(&mutex);
}

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

#ifdef BUFFER_MONITOR_USE_SINK_CACHE_QUERY
	if (0 == numberOfFragmentsCached)
	{
		bufferStatus = BUFFER_STATUS_YELLOW;
	}
#endif

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

void MediaTrack::UpdateTSAfterFetch()
{
	bool notifyCacheCompleted = false;
	pthread_mutex_lock(&mutex);
	cachedFragment[fragmentIdxToFetch].profileIndex = GetContext()->profileIdxForBandwidthNotification;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] before update fragmentIdxToFetch = %d numberOfFragmentsCached %d uri %s\n",
		        __FUNCTION__, __LINE__, name, fragmentIdxToFetch, numberOfFragmentsCached,
		        cachedFragment[fragmentIdxToFetch].uri);
	}
#endif
	cacheDurationSeconds += cachedFragment[fragmentIdxToFetch].duration;
	if((eTRACK_VIDEO == type) && aamp->IsFragmentBufferingRequired())
	{
		if(!notifiedCachingComplete)
		{
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

#ifdef BUFFER_MONITOR_USE_SINK_CACHE_QUERY
	if (MAX_CACHED_FRAGMENTS_PER_TRACK == numberOfFragmentsCached)
	{
		bufferStatus = BUFFER_STATUS_GREEN;
	}
#endif

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


bool MediaTrack::InjectFragment()
{
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
#ifndef SUPRESS_DECODE
#ifndef FOG_HAMMER_TEST // support aamp stress-tests of fog without video decoding/presentation
			InjectFragmentInternal(cachedFragment, stopInjection, fragmentDiscarded);
			if (GetContext()->mIsFirstBuffer && !fragmentDiscarded)
			{
				GetContext()->mIsFirstBuffer = false;
				aamp->NotifyFirstBufferProcessed();
			}
			if (stopInjection)
			{
				return false;
			}
#endif
#endif
			if(eTRACK_VIDEO == type)
			{
				GetContext()->NotifyBitRateUpdate(cachedFragment->profileIndex);
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
			return false;
		}
		AAMPLOG_TRACE("%s:%d [%p] - %s - injected cached uri at pos %f dur %f\n", __FUNCTION__, __LINE__, this, name, cachedFragment->position, cachedFragment->duration);
		if (!fragmentDiscarded)
		{
			totalInjectedDuration += cachedFragment->duration;
		}
		else
		{
			logprintf("%s:%d [%s] - Not updating totalInjectedDuration since fragment is Discarded\n", __FUNCTION__, __LINE__, name);
		}
		UpdateTSAfterInject();
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
		return false;
	}
	return true;
} // InjectFragment


static void *FragmentInjector(void *arg)
{
	MediaTrack *track = (MediaTrack *)arg;
	if(aamp_pthread_setname(pthread_self(), "aampInjector"))
	{
		logprintf("%s:%d: pthread_setname_np failed\n", __FUNCTION__, __LINE__);
	}
	track->RunInjectLoop();
	return NULL;
}


void MediaTrack::StartInjectLoop()
{
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

void MediaTrack::RunInjectLoop()
{
	const bool isAudioTrack = (eTRACK_AUDIO == type);
	bool notifyFirstFragment = true;
	if (1.0 == aamp->rate)
	{
		assert(gpGlobalConfig->bufferHealthMonitorDelay >= gpGlobalConfig->bufferHealthMonitorInterval);
		guint bufferMontiorSceduleTime = gpGlobalConfig->bufferHealthMonitorDelay - gpGlobalConfig->bufferHealthMonitorInterval;
		bufferHealthMonitorIdleTaskId = g_timeout_add_seconds(bufferMontiorSceduleTime, BufferHealthMonitorSchedule, this);
	}
	while (aamp->DownloadsAreEnabled())
	{
		if (!InjectFragment())
		{
			break;
		}
		if (notifyFirstFragment)
		{
			notifyFirstFragment = false;
			GetContext()->NotifyFirstFragmentInjected();
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
	if(bufferHealthMonitorIdleTaskId)
	{
		g_source_remove(bufferHealthMonitorIdleTaskId);
		bufferHealthMonitorIdleTaskId = 0;
	}
	AAMPLOG_WARN("fragment injector done. track %s\n", name);
}

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

bool MediaTrack::Enabled()
{
	return enabled;
}

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
		int preAllocSize = bandwidthBytesPerSecond * fragmentDurationSeconds * 1.5;
		traceprintf ("%s:%d [%s] bandwidthBytesPerSecond %d fragmentDurationSeconds %f prealloc size %d\n",
				__FUNCTION__, __LINE__, name, bandwidthBytesPerSecond, fragmentDurationSeconds, preAllocSize);
		aamp_Malloc(&cachedFragment->fragment, preAllocSize);
	}
	return cachedFragment;
}

void MediaTrack::SetCurrentBandWidth(int bandwidthBps)
{
	this->bandwidthBytesPerSecond = bandwidthBps/8;
}

int MediaTrack::GetCurrentBandWidth()
{
	return this->bandwidthBytesPerSecond;
}

MediaTrack::MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name) :
		eosReached(false), enabled(false), numberOfFragmentsCached(0), fragmentIdxToInject(0),
		fragmentIdxToFetch(0), abort(false), fragmentInjectorThreadID(0), totalFragmentsDownloaded(0),
		fragmentInjectorThreadStarted(false), totalInjectedDuration(0), cacheDurationSeconds(0),
		notifiedCachingComplete(false), fragmentDurationSeconds(0), segDLFailCount(0),segDrmDecryptFailCount(0),
		bufferStatus(BUFFER_STATUS_GREEN), prevBufferStatus(BUFFER_STATUS_GREEN), bufferHealthMonitorIdleTaskId(0),
		bandwidthBytesPerSecond(AAMP_DEFAULT_BANDWIDTH_BYTES_PREALLOC)
{
	this->type = type;
	this->aamp = aamp;
	this->name = name;
	memset(&cachedFragment[0], 0, sizeof(cachedFragment));
	pthread_cond_init(&fragmentFetched, NULL);
	pthread_cond_init(&fragmentInjected, NULL);
	pthread_mutex_init(&mutex, NULL);
}

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

void StreamAbstractionAAMP::ReassessAndResumeAudioTrack()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if( audio && video )
	{
		pthread_mutex_lock(&mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();
		if( audioDuration < (videoDuration + (2*video->fragmentDurationSeconds )) || !aamp->DownloadsAreEnabled())
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

void StreamAbstractionAAMP::WaitForVideoTrackCatchup()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	pthread_mutex_lock(&mLock);
	double audioDuration = audio->GetTotalInjectedDuration();
	double videoDuration = video->GetTotalInjectedDuration();
	if ((audioDuration > (videoDuration +  video->fragmentDurationSeconds)) && aamp->DownloadsAreEnabled())
	{
#ifdef AAMP_DEBUG_FETCH_INJECT
		logprintf("\n%s:%d waiting for cond - audioDuration %f videoDuration %f\n",
			__FUNCTION__, __LINE__, audioDuration, videoDuration);
#endif
		pthread_cond_wait(&mCond, &mLock);
	}
	pthread_mutex_unlock(&mLock);
}

StreamAbstractionAAMP::StreamAbstractionAAMP(PrivateInstanceAAMP* aamp):
		mAbrProfileChgUpCntr(0), mAbrProfileChgDnCntr(0), trickplayMode(false), currentProfileIndex(0), mCurrentBandwidth(0),
		mLastVideoFragCheckedforABR(0), mTsbBandwidth(0),mNwConsistencyBypass(true),
		desiredIframeProfile(0), lowestIframeProfile(-1), profileIdxForBandwidthNotification(0),
		hasDrm(false), mIsAtLivePoint(false), mTotalPausedDurationMS(0), mIsPaused(false),
		mStartTimeStamp(-1),mLastPausedTimeStamp(-1), mIsFirstBuffer(true)
{
#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	mIsPlaybackStalled = false;
#endif
	traceprintf("StreamAbstractionAAMP::%s\n", __FUNCTION__);
	pthread_mutex_init(&mLock, NULL);
	pthread_cond_init(&mCond, NULL);
	this->aamp = aamp;
}

StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
	traceprintf("StreamAbstractionAAMP::%s\n", __FUNCTION__);
	pthread_cond_destroy(&mCond);
	pthread_mutex_destroy(&mLock);
	AAMPLOG_INFO("Exit StreamAbstractionAAMP::%s\n", __FUNCTION__);
}


int StreamAbstractionAAMP::GetDesiredProfile(bool getMidProfile, long defaultBitrate)
{
	int desiredProfileIndex;
	// This function will be called only once during session creation to get default profile
	// check if any profiles are added (incase), remove it before adding fresh
	// populate the container with sorted order of BW vs its index
	if(mSortedBWProfileList.size())
	{
		mSortedBWProfileList.erase(mSortedBWProfileList.begin(),mSortedBWProfileList.end());
	}
	int profileCount = GetProfileCount();
	for(int cnt=0;cnt<profileCount;cnt++)
	{
		// store all the bandwidth data and its index to map which will sort byitself
		if (!GetStreamInfo(cnt)->isIframeTrack)
		{
			mSortedBWProfileList[GetStreamInfo(cnt)->bandwidthBitsPerSecond] = cnt;
		}
	}
	if (this->trickplayMode && -1 != lowestIframeProfile)
	{
		desiredProfileIndex = GetIframeTrack();
	}
	else if(getMidProfile && profileCount > 1)
	{
		// get the mid profile from the sorted list
		SortedBWProfileListIter iter = mSortedBWProfileList.begin();
		std::advance(iter,(int)(mSortedBWProfileList.size()/2));
		desiredProfileIndex = iter->second;
	}
	else
	{
		SortedBWProfileListIter iter;
		desiredProfileIndex = mSortedBWProfileList.begin()->second;
		for(iter = mSortedBWProfileList.begin(); iter != mSortedBWProfileList.end();iter++)
		{
			if (iter->first >= defaultBitrate)
			{
				logprintf("StreamAbstractionAAMP::%s desiredProfileIndex %d bitrate %ld defaultBitrate %ld\n", __FUNCTION__, desiredProfileIndex, GetStreamInfo(desiredProfileIndex)->bandwidthBitsPerSecond, defaultBitrate);
				break;
			}
			desiredProfileIndex = iter->second;
		}
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

void StreamAbstractionAAMP::NotifyBitRateUpdate(int profileIndex)
{
	if (profileIndex != aamp->GetPersistedProfileIndex())
	{
		StreamInfo* streamInfo = GetStreamInfo(profileIndex);

        bool lGetBWIndex = false;
        /* START: Added As Part of DELIA-28363 and DELIA-28247 */
        if(aamp->IsTuneTypeNew &&
                streamInfo->bandwidthBitsPerSecond == (GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond)){
            MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
            logprintf("NotifyBitRateUpdate: Max BitRate: %ld,  timetotop: %f", streamInfo->bandwidthBitsPerSecond, video->GetTotalInjectedDuration());
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


void StreamAbstractionAAMP::UpdateProfileBasedOnFragmentDownloaded(void)
{
	// This function checks for bandwidth change based on the fragment url from FOG
	int desiredProfileIndex = 0;
	if (mCurrentBandwidth != mTsbBandwidth)
	{
		// a) Check if network bandwidth changed from starting bw
		// b) Check if netwwork bandwidth is different from persisted bandwidth( needed for first time reporting)
		// find the profile for the newbandwidth
		for (int i = 0; i < GetProfileCount(); i++)
		{
			StreamInfo* streamInfo = GetStreamInfo(i);
			if (!streamInfo->isIframeTrack)
			{
				if (streamInfo->bandwidthBitsPerSecond == mTsbBandwidth)
				{
					// Good case ,most manifest url will have same bandwidth in fragment file with configured profile bandwidth
					desiredProfileIndex = i;
				}
				else if (streamInfo->bandwidthBitsPerSecond < mTsbBandwidth)
				{
					// fragment file name bandwidth doesnt match the profile bandwidth, will be always less
					desiredProfileIndex = (i + 1);
				}
			}
		}
		mCurrentBandwidth = mTsbBandwidth;
		profileIdxForBandwidthNotification = desiredProfileIndex;
		traceprintf("%s:%d profileIdxForBandwidthNotification updated to %d \n", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
		GetMediaTrack(eTRACK_VIDEO)->SetCurrentBandWidth(GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond);
	}
}

int StreamAbstractionAAMP::GetDesiredProfileBasedOnCache(void)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	int desiredProfileIndex = currentProfileIndex;
	int nwConsistencyCnt = (mNwConsistencyBypass)?1:gpGlobalConfig->abrNwConsistency;
	// this shouldnt happen , but checking it for safety , ensure list is populated
        if(!mSortedBWProfileList.size())
        {
        	int profileCount = GetProfileCount();
        	for(int cnt=0;cnt<profileCount;cnt++)
        	{
                	// store all the bandwidth data and its index to map which will sort byitself
                	if (!GetStreamInfo(cnt)->isIframeTrack)
                        	mSortedBWProfileList[GetStreamInfo(cnt)->bandwidthBitsPerSecond] = cnt;
        	}
	}

	if (this->trickplayMode)
	{
		int tmpIframeProfile = GetIframeTrack();
		if(tmpIframeProfile != -1)
			desiredProfileIndex = tmpIframeProfile;
	}
	/*In live, fog takes care of ABR, and cache updating is not based only on bandwidth,
	 * but also depends on fragment availability in CDN*/
	else
	{
		long currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
		long networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
		if(networkBandwidth == -1)
		{
			logprintf("No network bandwidth info available , not changing profile[%d]\n",currentProfileIndex);
			mAbrProfileChgUpCntr=0;mAbrProfileChgDnCntr=0;
		}
		else {
		// regular scenario where cache is not empty
		if(networkBandwidth > currentBandwidth)
		{
			// if networkBandwidth > is more than current bandwidth
			SortedBWProfileListIter iter;
			SortedBWProfileListIter currIter=mSortedBWProfileList.find(currentBandwidth);
			SortedBWProfileListIter storedIter=mSortedBWProfileList.end();
			for(iter = currIter; iter != mSortedBWProfileList.end();iter++)
				{					
					// This is sort List 
					if(networkBandwidth >= iter->first)
						{
							desiredProfileIndex = iter->second;
							storedIter = iter;
						}
						else
							break;
				}

			// No need to jump one profile for one network bw increase
			if(storedIter != mSortedBWProfileList.end() && (currIter->first < storedIter->first) &&  std::distance(currIter,storedIter) == 1 )	
			{
				mAbrProfileChgUpCntr++;
				// if same profile holds good for next 3*2 fragments
				if(mAbrProfileChgUpCntr < nwConsistencyCnt)
					desiredProfileIndex = currentProfileIndex;
				else
					mAbrProfileChgUpCntr=0;
			}
			else
			{
				mAbrProfileChgUpCntr=0;
			}
			mAbrProfileChgDnCntr=0;
		}
		else
		{
			// if networkBandwidth < than current bandwidth
			SortedBWProfileListRevIter reviter;
                        SortedBWProfileListIter currIter=mSortedBWProfileList.find(currentBandwidth);
                        SortedBWProfileListIter storedIter=mSortedBWProfileList.end();
                        for(reviter=mSortedBWProfileList.rbegin(); reviter != mSortedBWProfileList.rend();reviter++)
                                {
                                        // This is sorted List
                                        if(networkBandwidth >= reviter->first)
                                                {
                                                        desiredProfileIndex = reviter->second;
							// convert from reviter to fwditer
                                                        storedIter = reviter.base();
							storedIter--;
							break;
                                                }
                                }
	
			// No need to jump one profile for small  network change
			if(storedIter != mSortedBWProfileList.end() && (currIter->first > storedIter->first) && std::distance(storedIter,currIter) == 1 )
			{
				mAbrProfileChgDnCntr++;
				// if same profile holds good for next 3*2 fragments
				if(mAbrProfileChgDnCntr < nwConsistencyCnt)
					desiredProfileIndex = currentProfileIndex;
				else
					mAbrProfileChgDnCntr=0;
                        }
			else
			{
				mAbrProfileChgDnCntr=0;
			}
			mAbrProfileChgUpCntr=0;
		}
		}

		if(currentProfileIndex != desiredProfileIndex)
			logprintf("aamp::GetDesiredProfileBasedOnCache---> currbw[%ld] nwbw[%ld] currProf[%d] desiredProf[%d] vidCache[%d]\n",currentBandwidth,networkBandwidth,currentProfileIndex,desiredProfileIndex,video->numberOfFragmentsCached);
	}
	// only for first call, consistency check is ignored
	mNwConsistencyBypass = false;
	return desiredProfileIndex;
}

bool StreamAbstractionAAMP::RampDownProfile()
{
	bool ret = false;
	int desiredProfileIndex = currentProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	mLastVideoFragCheckedforABR = video->GetTotalFragmentsFetched();
	if (this->trickplayMode)
	{
		//We use only second last and lowest profiles for iframes
		if (desiredProfileIndex != lowestIframeProfile)
		{
			desiredProfileIndex = lowestIframeProfile;
		}
	}
	else
	{
		 long currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
                 SortedBWProfileListIter Iter=mSortedBWProfileList.find(currentBandwidth);
		 if(Iter != mSortedBWProfileList.begin() && Iter!= mSortedBWProfileList.end())
		 {
			// get the prev profile . This is sorted list , so no worry of getting wrong profile 
			std::advance(Iter, -1);
                        desiredProfileIndex = Iter->second;
		 }
	}
	if (desiredProfileIndex != currentProfileIndex)
	{
		this->currentProfileIndex = desiredProfileIndex;
		ret = true;

		// Send abr notification to XRE
		video->ABRProfileChanged();
	}

	return ret;
}

bool StreamAbstractionAAMP::CheckForRampDownProfile(long http_error)
{
	bool retValue = false;

	if (!aamp->IsTSBSupported())
	{
		retValue = true;
		if (http_error == 404 || http_error == 500 || http_error == 503)
		{
			if (RampDownProfile())
			{
				AAMPLOG_INFO("StreamAbstractionAAMP::%s:%d > Condition Rampdown Success\n", __FUNCTION__, __LINE__);
			}
		}
		//For timeout, rampdown in single steps might not be enough
		else if (http_error == CURLE_OPERATION_TIMEDOUT)
		{
			mLastVideoFragCheckedforABR = GetMediaTrack(eTRACK_VIDEO)->GetTotalFragmentsFetched();
			UpdateProfileBasedOnFragmentCache();
		}
	}

	return retValue;
}

void StreamAbstractionAAMP::CheckForProfileChange(void)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	// profile change check to be done for every 3rd video fragment or if current fragment is of duration > 5sec
	// This function is called multiple times for same fragment.Ignore duplicate processing!!!
	if((mLastVideoFragCheckedforABR != video->GetTotalFragmentsFetched()) && 
		((video->GetTotalFragmentsFetched()%gpGlobalConfig->abrCheckInterval == 0)
		||(video->fragmentDurationSeconds >= FRAG_DURATION_SEC_FOR_ABR_CHECK)))
	{
		// TODO: this is a HACK , need to remove later . This function is gettin called twice for same video fragment .
		mLastVideoFragCheckedforABR	=	video->GetTotalFragmentsFetched();
		// FOG based
		if(aamp->IsTSBSupported())
		{
			// This is for FOG based download , where bandwidth is calculated based on downloaded fragment file name
			// No profile change will be done or manifest download triggered based on profilechange
			UpdateProfileBasedOnFragmentDownloaded();
		}
		else
		{
			UpdateProfileBasedOnFragmentCache();
		}
	}
}

int StreamAbstractionAAMP::GetIframeTrack()
{
	return desiredIframeProfile;
}

void StreamAbstractionAAMP::UpdateIframeTracks()
{
	// Find the iframe profiles
	int profileCount = GetProfileCount();
	int iframeTrackIdx = -1;
	struct IframeTrackInfo *iframeTrackInfo = new struct IframeTrackInfo[profileCount];
	bool is4K = false;

	for (int i = 0; i < GetProfileCount(); i++)
	{
		if (GetStreamInfo(i)->isIframeTrack)
		{
			iframeTrackIdx++;
			iframeTrackInfo[iframeTrackIdx].bandwidth = GetStreamInfo(i)->bandwidthBitsPerSecond;
			iframeTrackInfo[iframeTrackIdx].idx = i;
		}
	}

	if(iframeTrackIdx >= 0)
	{
		//sort the iframe track array
		 for (int i = 0; i < iframeTrackIdx; i++)
		 {
			 for (int j = 0; j < iframeTrackIdx - i; j++)
			 {
					if (iframeTrackInfo[j].bandwidth > iframeTrackInfo[j+1].bandwidth)
					{
						struct IframeTrackInfo temp = iframeTrackInfo[j];
						iframeTrackInfo[j] = iframeTrackInfo[j+1];
						iframeTrackInfo[j+1] = temp;
					}
			 }
		 }

		int highestProfileIdx = iframeTrackInfo[iframeTrackIdx].idx;
		if(GetStreamInfo(highestProfileIdx)->resolution.height > 1080
			|| GetStreamInfo(highestProfileIdx)->resolution.width > 1920)
		{
			is4K = true;
		}

		if(is4K)
		{
			// get the default profile of 4k video , apply same bandwidth of video to iframe also
			int desiredProfileIndexNonIframe 	=  GetProfileCount()/ 2; 
			int desiredProfileNonIframeBW 		= GetStreamInfo(desiredProfileIndexNonIframe)->bandwidthBitsPerSecond ;
			desiredIframeProfile	= lowestIframeProfile	= 0;
			for (int cnt = 0; cnt <= iframeTrackIdx; cnt++)
				{
					// if bandwidth matches , apply to both desired and lower ( for all speed of trick)
					if(iframeTrackInfo[cnt].bandwidth == desiredProfileNonIframeBW)
						{
							desiredIframeProfile = lowestIframeProfile = cnt;
							break;
						}
				}
			// if matching bandwidth not found with video , then pick the middle profile for iframe
			if((!desiredIframeProfile) && (iframeTrackIdx >= 1))
				{
					desiredIframeProfile = (iframeTrackIdx/2) + (iframeTrackIdx%2);
					lowestIframeProfile = desiredIframeProfile;
				}
		}
		else
		{
			//Keeping old logic for non 4K streams
			for (int i = 0; i < GetProfileCount(); i++)
			{
				if (GetStreamInfo(i)->isIframeTrack)
				{
					if(lowestIframeProfile == -1)
					{
						// first pick the lowest profile available
						lowestIframeProfile = i;
						desiredIframeProfile = i;
						continue;
					}
					// if more profiles available , stored second best to desired profile
					desiredIframeProfile = i;
					break; // select first-advertised
				}
			}
		}
	}
	delete[] iframeTrackInfo;
}

void StreamAbstractionAAMP::NotifyPlaybackPaused(bool paused)
{
	mIsPaused = paused;
	if (paused)
	{
		mIsAtLivePoint = false;
		mLastPausedTimeStamp = aamp_GetCurrentTimeMS();
	}
	else
	{
		if(-1 != mLastPausedTimeStamp)
		{
			mTotalPausedDurationMS += (aamp_GetCurrentTimeMS() - mLastPausedTimeStamp);
			mLastPausedTimeStamp = -1;
		}
		else
		{
			logprintf("StreamAbstractionAAMP:%s() mLastPausedTimeStamp -1\n", __FUNCTION__);
		}
	}
}

#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
bool StreamAbstractionAAMP::CheckIfPlaybackStalled(double expectedInjectedDuration)
{
	// positionMS is currentPosition that will be sent in MediaProgress event
	// make sure that value never exceeds total injected duration

	MediaTrack *videoTrack = GetMediaTrack(eTRACK_VIDEO);

	if (videoTrack && (expectedInjectedDuration > (videoTrack->GetTotalInjectedDuration() + AAMP_STALL_CHECK_TOLERANCE)))
	{
		traceprintf("StreamAbstractionAAMP:%s() expectedInjectedDuration %f injectedDuration %f\n", __FUNCTION__, expectedInjectedDuration, videoTrack->GetTotalInjectedDuration());
		// Also check if internal cache and gstreamer cache are also empty
		if (videoTrack->GetBufferHealthStatus() == BUFFER_STATUS_RED && mIsPlaybackStalled)
		{
			logprintf("StreamAbstractionAAMP:%s() Stall detected. Buffer status is RED! expectedInjectedDuration %f injectedDuration %f\n", __FUNCTION__, expectedInjectedDuration, videoTrack->GetTotalInjectedDuration());
			return true;
		}
	}

	return false;
}
#else
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

void StreamAbstractionAAMP::NotifyFirstFragmentInjected()
{
	pthread_mutex_lock(&mLock);
	if (-1 == mStartTimeStamp)
	{
		mStartTimeStamp = aamp_GetCurrentTimeMS();
	}
	pthread_mutex_unlock(&mLock);
}

double StreamAbstractionAAMP::GetElapsedTime()
{
	double elapsedTime = -1;
	pthread_mutex_lock(&mLock);
	traceprintf("StreamAbstractionAAMP:%s() mStartTimeStamp %lld mTotalPausedDurationMS %lld mLastPausedTimeStamp %lld\n", __FUNCTION__, mStartTimeStamp, mTotalPausedDurationMS, mLastPausedTimeStamp);
	if (!mIsPaused)
	{
		elapsedTime = (double)(aamp_GetCurrentTimeMS() - mStartTimeStamp - mTotalPausedDurationMS) / 1000;
	}
	else
	{
		elapsedTime = (double)(mLastPausedTimeStamp - mStartTimeStamp - mTotalPausedDurationMS) / 1000;
	}
	pthread_mutex_unlock(&mLock);
	return elapsedTime;
}


/* START: Added As Part of DELIA-28363 and DELIA-28247 */
int StreamAbstractionAAMP::GetBWIndex(long BitRate)
{
    int TopBWIndex = 0;

    SortedBWProfileListRevIter MapBWRevIter;
    for(MapBWRevIter = mSortedBWProfileList.rbegin(); MapBWRevIter != mSortedBWProfileList.rend(); ++MapBWRevIter){
        if(MapBWRevIter->first == BitRate){
            break;
        }
        --TopBWIndex;
    }

    return TopBWIndex;
}
/* END: Added As Part of DELIA-28363 and DELIA-28247 */

void StreamAbstractionAAMP::UpdateProfileBasedOnFragmentCache()
{
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
	}
}
