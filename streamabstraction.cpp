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
#include "AampUtils.h"
#include "isobmffbuffer.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <iterator>
#include <sys/time.h>
#include <cmath>

#define AAMP_BUFFER_MONITOR_GREEN_THRESHOLD 4 //2 fragments for MSO specific linear streams.
//#define AAMP_DEBUG_INJECT_CHUNK
//#define AAMP_DEBUG_FETCH_INJECT 1

using namespace std;

/**
 * @brief Thread funtion for Buffer Health Monitoring
 *
 * @return NULL
 */
static void* BufferHealthMonitor(void* user_data)
{
	MediaTrack *track = (MediaTrack *)user_data;
	if(aamp_pthread_setname(pthread_self(), "aampBuffHealth"))
	{
		logprintf("%s:%d: aamp_pthread_setname failed", __FUNCTION__, __LINE__);
	}
	track->MonitorBufferHealth();
	return NULL;
}

/**
 * @brief Get string corresponding to buffer status.
 *
 * @return string representation of buffer status
 */
const char* MediaTrack::GetBufferHealthStatusString(BufferHealthStatus status)
{
	const char* ret = NULL;
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

/**
 * @brief Get buffer Status of track
 */
BufferHealthStatus MediaTrack::GetBufferStatus()
{
    BufferHealthStatus bStatus = BUFFER_STATUS_GREEN;
    double bufferedTime = totalInjectedDuration - GetContext()->GetElapsedTime();
    if ( (numberOfFragmentsCached <= 0) && (bufferedTime <= AAMP_BUFFER_MONITOR_GREEN_THRESHOLD))
    {
        logprintf("%s:%d [%s] bufferedTime %f totalInjectedDuration %f elapsed time %f",__FUNCTION__, __LINE__,
                name, bufferedTime, totalInjectedDuration, GetContext()->GetElapsedTime());
        if (bufferedTime <= 0)
        {
            bStatus = BUFFER_STATUS_RED;
        }
        else
        {
            bStatus = BUFFER_STATUS_YELLOW;
        }
    }
    return bStatus;
}

/**
 * @brief Monitors buffer health of track
 */
void MediaTrack::MonitorBufferHealth()
{
	int  bufferHealthMonitorDelay,bufferHealthMonitorInterval;
	long discontinuityTimeoutValue;
	GETCONFIGVALUE(eAAMPConfig_BufferHealthMonitorDelay,bufferHealthMonitorDelay); 
	GETCONFIGVALUE(eAAMPConfig_BufferHealthMonitorInterval,bufferHealthMonitorInterval);
	GETCONFIGVALUE(eAAMPConfig_DiscontinuityTimeout,discontinuityTimeoutValue);
	assert(bufferHealthMonitorDelay >= bufferHealthMonitorInterval);
	unsigned int bufferMontiorScheduleTime = bufferHealthMonitorDelay - bufferHealthMonitorInterval;
	bool keepRunning = false;
	if(aamp->DownloadsAreEnabled() && !abort)
	{
		aamp->InterruptableMsSleep(bufferMontiorScheduleTime *1000);
		keepRunning = true;
	}
	int monitorInterval = bufferHealthMonitorInterval  * 1000;
	while(keepRunning && !abort)
	{
		aamp->InterruptableMsSleep(monitorInterval);
		pthread_mutex_lock(&mutex);
		if (aamp->DownloadsAreEnabled() && !abort)
		{
			bufferStatus = GetBufferStatus();
			if (bufferStatus != prevBufferStatus)
			{
				logprintf("aamp: track[%s] buffering %s->%s", name, GetBufferHealthStatusString(prevBufferStatus),
						GetBufferHealthStatusString(bufferStatus));
				prevBufferStatus = bufferStatus;
			}
			else
			{
				traceprintf("%s:%d track[%s] No Change [%s]", __FUNCTION__, __LINE__, name,
						GetBufferHealthStatusString(bufferStatus));
			}

			pthread_mutex_unlock(&mutex);

			if (aamp->IsLive() && !aamp->pipeline_paused && !aamp->IsFirstFrameReceived())
			{
				// Added logic to identify the playback stall which is observed even before the pipeline moving to PLAYING state (or) receiving the first frame.
				GetContext()->CheckForPlaybackStall(false, true);
			}

			// We use another lock inside CheckForMediaTrackInjectionStall for synchronization
			GetContext()->CheckForMediaTrackInjectionStall(type);

			pthread_mutex_lock(&mutex);
			if((!aamp->pipeline_paused) && aamp->IsDiscontinuityProcessPending() && discontinuityTimeoutValue)
			{
				aamp->CheckForDiscontinuityStall((MediaType)type);
			}

			// If underflow occurred and cached fragments are full
			if (aamp->GetBufUnderFlowStatus() && bufferStatus == BUFFER_STATUS_GREEN && type == eTRACK_VIDEO)
			{
				// There is a chance for deadlock here
				// We hit an underflow in a scenario where its not actually an underflow
				// If track injection to GStreamer is stopped because of this special case, we can't come out of
				// buffering even if we have enough data
				if (!aamp->TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
				{
					// This is a deadlock, buffering is active and enough-data received from GStreamer
					AAMPLOG_WARN("%s:%d Possible deadlock with buffering. Enough buffers cached, un-pause pipeline!", __FUNCTION__, __LINE__);
					aamp->StopBuffering(true);
				}

			}

		}
		else
		{
			keepRunning = false;
		}
		pthread_mutex_unlock(&mutex);
	}
}

/**
 * @brief Updates internal state after a fragment inject
 */
void MediaTrack::UpdateTSAfterInject()
{
	pthread_mutex_lock(&mutex);
	AAMPLOG_TRACE("%s:%d [%s] Free cachedFragment[%d] numberOfFragmentsCached %d", __FUNCTION__, __LINE__,
			name, fragmentIdxToInject, numberOfFragmentsCached);
	aamp_Free(&cachedFragment[fragmentIdxToInject].fragment);
	memset(&cachedFragment[fragmentIdxToInject], 0, sizeof(CachedFragment));
	fragmentIdxToInject++;
	if (fragmentIdxToInject == maxCachedFragmentsPerTrack)
	{
		fragmentIdxToInject = 0;
	}
	numberOfFragmentsCached--;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] updated fragmentIdxToInject = %d numberOfFragmentsCached %d", __FUNCTION__, __LINE__,
		        name, fragmentIdxToInject, numberOfFragmentsCached);
	}
#endif
	pthread_cond_signal(&fragmentInjected);
	pthread_mutex_unlock(&mutex);
}

/**
 * @brief Updates internal state after a fragment inject
 */
void MediaTrack::UpdateTSAfterChunkInject()
{
	pthread_mutex_lock(&mutex);
	//Free Chunk Cache Buffer
        aamp_Free(&cachedFragmentChunks[fragmentChunkIdxToInject].fragmentChunk);
        memset(&cachedFragmentChunks[fragmentChunkIdxToInject], 0, sizeof(CachedFragmentChunk));

	aamp_Free(&parsedBufferChunk);
	memset(&parsedBufferChunk, 0x00, sizeof(GrowableBuffer));

	//increment Inject Index
	fragmentChunkIdxToInject = (++fragmentChunkIdxToInject) % maxCachedFragmentChunksPerTrack;
	if(numberOfFragmentChunksCached > 0) numberOfFragmentChunksCached--;

	AAMPLOG_TRACE("%s:%d [%s] updated fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d", __FUNCTION__, __LINE__,
			name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);

	pthread_cond_signal(&fragmentChunkInjected);
	pthread_mutex_unlock(&mutex);
}


/**
 * @brief Receives cached fragment and injects to sink.
 *
 * @param[in] cachedFragmentChunk - contains fragment  Chunk to be processed and injected
 * @param[out] fragmentChunkDiscarded - true if fragment is discarded.
 */
void MediaTrack::InjectFragmentChunkInternal(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double fDuration)
{
	aamp->SendStreamTransfer(mediaType, buffer,
                     fpts, fdts, fDuration);

} // InjectFragmentChunkInternal

/**
 * @brief Updates internal state after a fragment fetch
 */
void MediaTrack::UpdateTSAfterFetch()
{
	bool notifyCacheCompleted = false;
	pthread_mutex_lock(&mutex);
	cachedFragment[fragmentIdxToFetch].profileIndex = GetContext()->profileIdxForBandwidthNotification;
	GetContext()->UpdateStreamInfoBitrateData(cachedFragment[fragmentIdxToFetch].profileIndex, cachedFragment[fragmentIdxToFetch].cacheFragStreamInfo);
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] before update fragmentIdxToFetch = %d numberOfFragmentsCached %d",
		        __FUNCTION__, __LINE__, name, fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	totalFetchedDuration += cachedFragment[fragmentIdxToFetch].duration;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		if (numberOfFragmentsCached == 0)
		{
			logprintf("## %s:%d [%s] Caching fragment for track when numberOfFragmentsCached is 0 ##", __FUNCTION__, __LINE__, name);
		}
	}
#endif
	numberOfFragmentsCached++;
	assert(numberOfFragmentsCached <= maxCachedFragmentsPerTrack);
	currentInitialCacheDurationSeconds += cachedFragment[fragmentIdxToFetch].duration;

	if( (eTRACK_VIDEO == type)
			&& aamp->IsFragmentCachingRequired()
			&& !cachingCompleted)
	{
		const int minInitialCacheSeconds = aamp->GetInitialBufferDuration();
		if(currentInitialCacheDurationSeconds >= minInitialCacheSeconds)
		{
			logprintf("## %s:%d [%s] Caching Complete cacheDuration %d minInitialCacheSeconds %d##",
					__FUNCTION__, __LINE__, name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
		else if (sinkBufferIsFull && numberOfFragmentsCached == maxCachedFragmentsPerTrack)
		{
			logprintf("## %s:%d [%s] Cache is Full cacheDuration %d minInitialCacheSeconds %d, aborting caching!##",
					__FUNCTION__, __LINE__, name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
		else
		{
			AAMPLOG_INFO("## %s:%d [%s] Caching Ongoing cacheDuration %d minInitialCacheSeconds %d##",
					__FUNCTION__, __LINE__, name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
		}
	}
	fragmentIdxToFetch++;
	if (fragmentIdxToFetch == maxCachedFragmentsPerTrack)
	{
		fragmentIdxToFetch = 0;
	}
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] updated fragmentIdxToFetch = %d numberOfFragmentsCached %d",
			__FUNCTION__, __LINE__, name, fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	totalFragmentsDownloaded++;
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);
	if(notifyCacheCompleted)
	{
		aamp->NotifyFragmentCachingComplete();
	}
}

/**
 * @brief Updates internal state after a fragment fetch
 */
void MediaTrack::UpdateTSAfterChunkFetch()
{
	pthread_mutex_lock(&mutex);

	numberOfFragmentChunksCached++;

	AAMPLOG_TRACE("%s:%d [%s] numberOfFragmentChunksCached++ [%d]",__FUNCTION__, __LINE__, name,numberOfFragmentChunksCached);

	//this should never HIT
	assert(numberOfFragmentChunksCached <= maxCachedFragmentChunksPerTrack);

	fragmentChunkIdxToFetch = (fragmentChunkIdxToFetch+1) % maxCachedFragmentChunksPerTrack;

	AAMPLOG_TRACE("%s:%d [%s] updated fragmentChunkIdxToFetch [%d] numberOfFragmentChunksCached [%d]",
			__FUNCTION__, __LINE__, name, fragmentChunkIdxToFetch, numberOfFragmentChunksCached);

	totalFragmentChunksDownloaded++;
	pthread_cond_signal(&fragmentChunkFetched);
	pthread_mutex_unlock(&mutex);

	AAMPLOG_TRACE("%s:%d [%s] pthread_cond_signal(fragmentChunkFetched)",__FUNCTION__, __LINE__, name);
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
	PrivAAMPState state;
	int preplaybuffercount=0;
	GETCONFIGVALUE(eAAMPConfig_PrePlayBufferCount,preplaybuffercount);

	if(abort)
	{
		ret = false;
	}
	else
	{
		// Still in preparation mode , not to inject any more fragments beyond capacity
		// Wait for 100ms
		pthread_mutex_lock(&aamp->mMutexPlaystart);
		aamp->GetState(state);
		if(state == eSTATE_PREPARED && totalFragmentsDownloaded > preplaybuffercount
				&& !aamp->IsFragmentCachingRequired() )
		{

		timeoutMs = 500;
		struct timespec tspec;
		tspec = aamp_GetTimespec(timeoutMs);

		pthreadReturnValue = pthread_cond_timedwait(&aamp->waitforplaystart, &aamp->mMutexPlaystart, &tspec);

		if (ETIMEDOUT == pthreadReturnValue)
		{
			ret = false;
		}
		else if (0 != pthreadReturnValue)
		{
			logprintf("%s:%d [%s] pthread_cond_timedwait returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
			ret = false;
		}
		}
		pthread_mutex_unlock(&aamp->mMutexPlaystart);	
	}
	
	pthread_mutex_lock(&mutex);
	if ( ret && (numberOfFragmentsCached == maxCachedFragmentsPerTrack) )
	{
		if (timeoutMs >= 0)
		{
			struct timespec tspec = aamp_GetTimespec(timeoutMs);

			pthreadReturnValue = pthread_cond_timedwait(&fragmentInjected, &mutex, &tspec);

			if (ETIMEDOUT == pthreadReturnValue)
			{
				ret = false;
			}
			else if (0 != pthreadReturnValue)
			{
				logprintf("%s:%d [%s] pthread_cond_timedwait returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
				ret = false;
			}
		}
		else
		{
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				logprintf("%s:%d [%s] waiting for fragmentInjected condition", __FUNCTION__, __LINE__, name);
			}
#endif
			pthreadReturnValue = pthread_cond_wait(&fragmentInjected, &mutex);

			if (0 != pthreadReturnValue)
			{
				logprintf("%s:%d [%s] pthread_cond_wait returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
				ret = false;
			}
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				logprintf("%s:%d [%s] wait complete for fragmentInjected", __FUNCTION__, __LINE__, name);
			}
#endif
		}
		if(abort)
		{
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				logprintf("%s:%d [%s] abort set, returning false", __FUNCTION__, __LINE__, name);
			}
#endif
			ret = false;
		}
	}
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] fragmentIdxToFetch = %d numberOfFragmentsCached %d",
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
	if ((numberOfFragmentsCached == 0) && !(abort || abortInject))
	{
#ifdef AAMP_DEBUG_FETCH_INJECT
		if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
		{
			logprintf("## %s:%d [%s] Waiting for CachedFragment to be available, eosReached=%d ##", __FUNCTION__, __LINE__, name, eosReached);
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
		logprintf("%s:%d [%s] fragmentIdxToInject = %d numberOfFragmentsCached %d",
			__FUNCTION__, __LINE__, name, fragmentIdxToInject, numberOfFragmentsCached);
	}
#endif
	ret = !(abort || abortInject || (numberOfFragmentsCached == 0));
	pthread_mutex_unlock(&mutex);
	return ret;
}

/**
 * @brief Wait until a cached fragment chunk is Injected.
 * @retval true if fragment chunk injected , false on abort.
 */
bool MediaTrack::WaitForCachedFragmentChunkInjected(int timeoutMs)
{
	bool ret = true;
	int pthreadReturnValue = 0;
	pthread_mutex_lock(&mutex);

	if ((numberOfFragmentChunksCached == maxCachedFragmentChunksPerTrack) && !(abort || abortInjectChunk))
	{
		if (timeoutMs >= 0)
		{
			struct timespec tspec = aamp_GetTimespec(timeoutMs);

			pthreadReturnValue = pthread_cond_timedwait(&fragmentChunkInjected, &mutex, &tspec);

			if (ETIMEDOUT == pthreadReturnValue)
			{
				ret = false;
			}
			else if (0 != pthreadReturnValue)
			{
				AAMPLOG_WARN("%s:%d [%s] pthread_cond_timedwait returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
				ret = false;
			}
		}
		else
		{
			AAMPLOG_TRACE("%s:%d [%s] waiting for fragmentChunkInjected condition", __FUNCTION__, __LINE__, name);
			pthreadReturnValue = pthread_cond_wait(&fragmentChunkInjected, &mutex);

			if (0 != pthreadReturnValue)
			{
				AAMPLOG_WARN("%s:%d [%s] pthread_cond_wait returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
				ret = false;
			}
			AAMPLOG_TRACE("%s:%d [%s] wait complete for fragmentChunkInjected", __FUNCTION__, __LINE__, name);
		}
	}
	if(abort || abortInjectChunk)
	{
		AAMPLOG_TRACE("%s:%d [%s] abort set, returning false", __FUNCTION__, __LINE__, name);
		ret = false;
	}

	AAMPLOG_TRACE("%s:%d [%s] fragmentChunkIdxToFetch = %d numberOfFragmentChunksCached %d",
			__FUNCTION__, __LINE__, name, fragmentChunkIdxToFetch, numberOfFragmentChunksCached);

    pthread_mutex_unlock(&mutex);
    return ret;
}

/**
 * @brief Wait until a cached fragment chunk is available.
 * @retval true if fragment available, false on abort.
 */
bool MediaTrack::WaitForCachedFragmentChunkAvailable()
{
	bool ret = true;
	pthread_mutex_lock(&mutex);

	AAMPLOG_TRACE("%s:%d [%s] Acquired MUTEX ==> fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d ret = %d abort = %d abortInjectChunk = %d cleanChunkCache = %d", __FUNCTION__, __LINE__, name, fragmentChunkIdxToInject, numberOfFragmentChunksCached, ret, abort, abortInjectChunk, cleanChunkCache);

	if(cleanChunkCache )
	{
		AAMPLOG_TRACE("%s:%d [%s]numberOfFragmentChunksCached %d ", __FUNCTION__, __LINE__, name, numberOfFragmentChunksCached);

		if((!cleanChunkCacheInitiated) && numberOfFragmentChunksCached == 0 )
		{
			logprintf("%s:%d [%s] Ignore Chunk Inject(After) - Cleanup In-progress!!!", __FUNCTION__, __LINE__, name);
			cleanChunkCacheInitiated = true;

			for (int i = 0; i < DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK; i++)
			{
				aamp_Free(&cachedFragmentChunks[i].fragmentChunk);
				memset(&cachedFragmentChunks[i], 0, sizeof(CachedFragmentChunk));
			}
			aamp_Free(&unparsedBufferChunk);
			memset(&unparsedBufferChunk, 0x00, sizeof(GrowableBuffer));
			aamp_Free(&parsedBufferChunk);
			memset(&parsedBufferChunk, 0x00, sizeof(GrowableBuffer));
		}

		if( numberOfFragmentChunksCached == 0)
		{
			pthread_cond_signal(&fragmentChunkClean);
			pthread_mutex_unlock(&mutex);
		}
		return ret;
	}

	if ((numberOfFragmentChunksCached == 0) && !(abort || abortInjectChunk || cleanChunkCache))
	{
		AAMPLOG_TRACE("## %s:%d [%s] Waiting for CachedFragment to be available, eosReached=%d ##", __FUNCTION__, __LINE__, name, eosReached);

		if (!eosReached)
		{
			int pthreadReturnValue = 0;
			pthreadReturnValue = pthread_cond_wait(&fragmentChunkFetched, &mutex);
			if (0 != pthreadReturnValue)
			{
				AAMPLOG_WARN("%s:%d [%s] pthread_cond_wait(fragmentChunkFetched) returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
			}
			AAMPLOG_TRACE("%s:%d [%s] wait complete for fragmentChunkFetched", __FUNCTION__, __LINE__, name);
		}
	}


	ret = !(abort || abortInjectChunk);

	AAMPLOG_TRACE("%s:%d [%s] fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d ret = %d abort = %d abortInjectChunk = %d",
			 __FUNCTION__, __LINE__, name, fragmentChunkIdxToInject, numberOfFragmentChunksCached, ret, abort, abortInjectChunk);

	pthread_mutex_unlock(&mutex);
	return ret;
}

/**
 * @brief Aborts wait for fragment.
 * @param[in] immediate Indicates immediate abort as in a seek/ stop
 */
void MediaTrack::AbortWaitForCachedAndFreeFragment(bool immediate)
{
	pthread_mutex_lock(&mutex);
	if (immediate)
	{
		abort = true;
#ifdef AAMP_DEBUG_FETCH_INJECT
		if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
		{
			logprintf("%s:%d [%s] signal fragmentInjected condition", __FUNCTION__, __LINE__, name);
		}
#endif
		pthread_cond_signal(&fragmentInjected);
		if(aamp->GetLLDashServiceData()->lowLatencyMode)
		{
			AAMPLOG_TRACE("%s:%d [%s] signal fragmentChunkInjected condition", __FUNCTION__, __LINE__, name);
			pthread_cond_signal(&fragmentChunkInjected);
		}
	}
	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		AAMPLOG_TRACE("%s:%d [%s] signal fragmentChunkFetched condition", __FUNCTION__, __LINE__, name);
		pthread_cond_signal(&fragmentChunkFetched);
	}
	pthread_cond_signal(&aamp->waitforplaystart);
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);

	GetContext()->AbortWaitForDiscontinuity();
}


/**
 * @brief Aborts wait for cached fragment.
 */
void MediaTrack::AbortWaitForCachedFragment()
{
	pthread_mutex_lock(&mutex);

	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		abortInjectChunk = true;
		AAMPLOG_TRACE("%s:%d [%s] signal fragmentChunkFetched condition", __FUNCTION__, __LINE__, name);
		pthread_cond_signal(&fragmentChunkFetched);
	}

	abortInject = true;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		logprintf("%s:%d [%s] signal fragmentFetched condition", __FUNCTION__, __LINE__, name);
	}
#endif
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);

	GetContext()->AbortWaitForDiscontinuity();
}

/**
 * @brief Process next cached fragment chunk
 */
bool MediaTrack::ProcessFragmentChunk()
{
	//Get Cache buffer
	CachedFragmentChunk* cachedFragmentChunk = &this->cachedFragmentChunks[fragmentChunkIdxToInject];
	if(cachedFragmentChunk != NULL && NULL == cachedFragmentChunk->fragmentChunk.ptr)
	{
		AAMPLOG_TRACE("%s:%d [%s] Ignore NULL Chunk - cachedFragmentChunk->fragmentChunk.len %d", __FUNCTION__, __LINE__, name, cachedFragmentChunk->fragmentChunk.len);
		return false;
	}

	size_t requiredLength = cachedFragmentChunk->fragmentChunk.len + unparsedBufferChunk.len;
	AAMPLOG_TRACE("%s:%d [%s] cachedFragmentChunk->fragmentChunk.len [%d] to unparsedBufferChunk.len [%d] Required Len [%d]", __FUNCTION__, __LINE__, name, cachedFragmentChunk->fragmentChunk.len, unparsedBufferChunk.len, requiredLength);

	//Append Cache buffer to unparsed buffer for processing
	aamp_AppendBytes(&unparsedBufferChunk, cachedFragmentChunk->fragmentChunk.ptr, cachedFragmentChunk->fragmentChunk.len);

#if 0  //enable to avoid small buffer processing
	if(cachedFragmentChunk->fragmentChunk.len < 500)
	{
		AAMPLOG_TRACE("%s:%d [%s] cachedFragmentChunk->fragmentChunk.len [%d] Ignoring", __FUNCTION__, __LINE__, name, cachedFragmentChunk->fragmentChunk.len);
		return true;
	}
#endif
	//Parse Chunk Data
	IsoBmffBuffer isobuf;                   /**< Fragment Chunk buffer box parser*/
	const Box *pBox = NULL;
	std::vector<Box*> *pBoxes;
	std::vector<Box*> boxes;
	size_t mdatCount = 0;
	size_t parsedBoxCount = 0;
	char *unParsedBuffer = NULL;
	size_t parsedBufferSize = 0, unParsedBufferSize = 0;
	
	unParsedBuffer = unparsedBufferChunk.ptr;
	unParsedBufferSize = parsedBufferSize = unparsedBufferChunk.len;

	isobuf.setBuffer(reinterpret_cast<uint8_t *>(unparsedBufferChunk.ptr), unparsedBufferChunk.len);

	AAMPLOG_TRACE("%s:%d [%s] Unparsed Buffer Size: %d", __FUNCTION__, __LINE__, name,unparsedBufferChunk.len);

	bool bParse = false;
	try
	{
		bParse = isobuf.parseBuffer();
	}
	catch( std::bad_alloc& ba)
	{
		AAMPLOG_ERR("%s %d: Bad allocation: %s", __FUNCTION__, __LINE__, ba.what() );
	}
	catch( std::exception &e)
	{
		AAMPLOG_ERR("%s %d: Unhandled exception: %s", __FUNCTION__, __LINE__, e.what() );
	}
	catch( ... )
	{
		AAMPLOG_ERR("%s %d:  Unknown exception", __FUNCTION__, __LINE__ );
	}
	if(!bParse)
	{
		AAMPLOG_INFO("%s:%d [%s] No Box available in cache chunk: fragmentChunkIdxToInject %d", __FUNCTION__, __LINE__, name, fragmentChunkIdxToInject);
		return true;
	}
	//Print box details
	//isobuf.printBoxes();

	isobuf.getMdatBoxCount(mdatCount);
	if(!mdatCount)
	{
		noMDATCount++;
		AAMPLOG_TRACE("%s:%d [%s] No MDAT Found. Exit noMDATCount=%d fragmentChunkIdxToInject=%d", __FUNCTION__, __LINE__, name,noMDATCount, fragmentChunkIdxToInject);
		isobuf.destroyBoxes();
		if(noMDATCount > MAX_MDAT_NOT_FOUND_COUNT)
		{
			//AAMPLOG_WARN("%s:%d [%s] scheduling retune since noMDATCount is [%d]", __FUNCTION__, __LINE__, name,noMDATCount);
			AAMPLOG_WARN("%s:%d [%s] scheduling retune REQUIRED since noMDATCount is [%d]", __FUNCTION__, __LINE__, name,noMDATCount);
			aamp->ScheduleRetune(eDASH_LOW_LATENCY_INPUT_PROTECTION_ERROR,eMEDIATYPE_VIDEO);
		}
		return true;
	}
	noMDATCount = 0;
	totalMdatCount += mdatCount;
	AAMPLOG_TRACE("%s:%d [%s] MDAT count found: %d, Total Found: %d", __FUNCTION__, __LINE__, name,  mdatCount, totalMdatCount );

	pBoxes = isobuf.getParsedBoxes();
	parsedBoxCount = pBoxes->size();

	pBox = isobuf.getChunkedfBox();
	if(pBox)
	{
		parsedBoxCount--;

		AAMPLOG_TRACE("%s:%d [%s] MDAT Chunk Found - Actual Parsed Box Count: %d", __FUNCTION__, __LINE__, name,parsedBoxCount);
		AAMPLOG_TRACE("%s:%d [%s] Chunk Offset[%u] Chunk Type[%s] Chunk Size[%u]\n", __FUNCTION__, __LINE__, name, pBox->getOffset(), pBox->getBoxType(), pBox->getSize());
	}
	if(mdatCount)
	{
		int lastMDatIndex = -1;
		//Get Last MDAT box
		for(int i=parsedBoxCount-1;i>=0;i--)
		{
			Box *box = pBoxes->at(i);
			if (IS_TYPE(box->getType(), Box::MDAT))
			{
				lastMDatIndex = i;

				AAMPLOG_TRACE("%s:%d [%s] Last MDAT Index : %d", __FUNCTION__, __LINE__, name,lastMDatIndex);

				//Calculate unparsed buffer based on last MDAT
				unParsedBuffer += (box->getOffset()+box->getSize()); //increment buffer pointer to chunk offset
				unParsedBufferSize -= (box->getOffset()+box->getSize()); //decerese by parsed buffer size

				parsedBufferSize -= unParsedBufferSize; //get parsed buf size

				AAMPLOG_TRACE("%s:%d [%s] parsedBufferSize : %d updated unParsedBufferSize: %d Total Buf Size processed: %d", __FUNCTION__, __LINE__, name,parsedBufferSize,unParsedBufferSize,parsedBufferSize+unParsedBufferSize);

				break;
			}
		}

		uint64_t fPts = 0;
		double fpts = 0.0;
		uint64_t fDuration = 0;
		double fduration = 0.0;
		uint64_t totalChunkDuration = 0.0;

		//logprintf("===========Base Media Decode Time Start================");
		//isobuf.PrintPTS();
		//logprintf("============Base Media Decode Time End===============");

		for(int i=0;i<lastMDatIndex;i++)
		{
			Box *box = pBoxes->at(i);
#ifdef AAMP_DEBUG_INJECT_CHUNK
			logprintf("%s:%d [%s] Type: %s", __FUNCTION__, __LINE__, name,box->getType());
#endif
			if (IS_TYPE(box->getType(), Box::MOOF))
			{
				isobuf.getSampleDuration(box, fDuration);
				totalChunkDuration += fDuration;
#ifdef AAMP_DEBUG_INJECT_CHUNK
				logprintf("%s:%d [%s] fDuration = %lld, totalChunkDuration = %lld", __FUNCTION__, __LINE__, name,fDuration, totalChunkDuration);
#endif
			}
		}
		//get PTS of buffer
		bool bParse = isobuf.getFirstPTS(fPts);
		if (bParse)
		{
			AAMPLOG_TRACE("%s:%d [%s] fPts %lld", __FUNCTION__, __LINE__,name, fPts);
		}

		uint32_t timeScale = 0;
		if(type == eTRACK_VIDEO)
		{
			timeScale = aamp->GetLLDashVidTimeScale();
		}
		else if(type == eTRACK_AUDIO)
		{
			timeScale = aamp->GetLLDashAudTimeScale();
		}

		if(!timeScale)
		{
			//FIX-ME-Read from MPD INSTEAD
			timeScale = GetContext()->GetCurrPeriodTimeScale();
			if(!timeScale)
			{
				timeScale = 10000000.0;
				AAMPLOG_WARN("%s:%d [%s] Empty timeScale!!! Using default timeScale=%d", __FUNCTION__, __LINE__, name, timeScale);
			}
		}

		fpts = fPts/(timeScale*1.0);
		fduration = totalChunkDuration/(timeScale*1.0);

		//Prepeare parsed buffer
		aamp_AppendBytes(&parsedBufferChunk, unparsedBufferChunk.ptr, parsedBufferSize);
#ifdef AAMP_DEBUG_INJECT_CHUNK
		IsoBmffBuffer isobufTest;
		//TEST CODE for PARSED DATA COMPELTENESS
		isobufTest.setBuffer(reinterpret_cast<uint8_t *>(parsedBufferChunk.ptr), parsedBufferChunk.len);
		isobufTest.parseBuffer();
		if(isobufTest.getChunkedfBox())
		{
			logprintf("%s:%d [%s] CHUNK Found in parsed buffer. Something is wrong", __FUNCTION__, __LINE__, name);
		}
		else
		{
			logprintf("%s:%d [%s] No CHUNK Found in parsed buffer. All Good to Send", __FUNCTION__, __LINE__, name);
		}
		//isobufTest.printBoxes();
		isobufTest.destroyBoxes();
#endif

		InjectFragmentChunkInternal((MediaType)type,&parsedBufferChunk , fpts, fpts, fduration);
		totalInjectedChunksDuration += fduration;
	}

	// Move unparsed data sections to beginning
	//Available size remains same
	//This buffer should be released on Track cleanup
	if(unParsedBufferSize)
	{
		AAMPLOG_TRACE("%s:%d [%s] unparsed[%p] unparsed_size[%u]", __FUNCTION__, __LINE__, name,unParsedBuffer,unParsedBufferSize);

		GrowableBuffer tempBuffer;
		memset(&tempBuffer,0x00,sizeof(GrowableBuffer));
		aamp_AppendBytes(&tempBuffer,unParsedBuffer,unParsedBufferSize);
		aamp_Free(&unparsedBufferChunk);
		memset(&unparsedBufferChunk,0x00,sizeof(GrowableBuffer));
		aamp_AppendBytes(&unparsedBufferChunk,tempBuffer.ptr,tempBuffer.len);
		aamp_Free(&tempBuffer);
		memset(&tempBuffer,0x00,sizeof(GrowableBuffer));
	}
	else
	{
		AAMPLOG_TRACE("%s:%d [%s] Set Unparsed Buffer chunk Empty...", __FUNCTION__, __LINE__, name);
		aamp_Free(&unparsedBufferChunk);
		memset(&unparsedBufferChunk, 0x00, sizeof(GrowableBuffer));
	}
	isobuf.destroyBoxes();
	return true;
}

/**
 * @brief Inject next cached fragment
 */
bool MediaTrack::InjectFragmentChunk()
{
    bool ret = true;

    if (WaitForCachedFragmentChunkAvailable())
    {
        bool bIgnore = true;
        bIgnore = ProcessFragmentChunk();
        if(bIgnore)
            UpdateTSAfterChunkInject();
    }
    else
    {
        logprintf("WaitForCachedFragmentChunkAvailable %s aborted", name);
        ret = false;
    }
    return ret;
}

/**
 * @brief Inject next cached fragment
 */
bool MediaTrack::InjectFragment()
{
	bool ret = true;
	if(!aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		aamp->BlockUntilGstreamerWantsData(NULL, 0, type);
	}

	if (WaitForCachedFragmentAvailable())
	{
		bool stopInjection = false;
		bool fragmentDiscarded = false;
		CachedFragment* cachedFragment = &this->cachedFragment[fragmentIdxToInject];
#ifdef TRACE
		logprintf("%s:%d [%s] - fragmentIdxToInject %d cachedFragment %p ptr %p", __FUNCTION__, __LINE__,
				name, fragmentIdxToInject, cachedFragment, cachedFragment->fragment.ptr);
#endif
		if (cachedFragment->fragment.ptr)
		{
			StreamAbstractionAAMP* context = GetContext();
#ifdef AAMP_DEBUG_INJECT
			if ((1 << type) & AAMP_DEBUG_INJECT)
			{
				if (cachedFragment->discontinuity)
				{
					logprintf("%s:%d [%s] Discontinuity present. uri %s", __FUNCTION__, __LINE__, name, cachedFragment->uri);
				}
			}
#endif
			if (type == eTRACK_SUBTITLE && cachedFragment->discontinuity)
			{
				logprintf("%s:%d [%s] notifying discontinuity to parser!", __FUNCTION__, __LINE__, name);
				if (mSubtitleParser)
				{
					mSubtitleParser->reset();
					stopInjection = true;
					discontinuityProcessed = true;
					ret = false;
				}
				cachedFragment->discontinuity = false;
			}
			else if ((cachedFragment->discontinuity || ptsError) && (AAMP_NORMAL_PLAY_RATE == context->aamp->rate))
			{
				bool isDiscoIgnoredForOtherTrack = aamp->IsDiscontinuityIgnoredForOtherTrack((MediaType)!type);
				logprintf("%s:%d - track %s - encountered aamp discontinuity @position - %f, isDiscoIgnoredForOtherTrack - %d", __FUNCTION__, __LINE__, name, cachedFragment->position, isDiscoIgnoredForOtherTrack);
				cachedFragment->discontinuity = false;
				ptsError = false;

				if (totalInjectedDuration == 0)
				{
					stopInjection = false;

					if (!isDiscoIgnoredForOtherTrack)
					{
						// set discontinuity ignored flag to check and avoid paired discontinuity processing of other track.
						aamp->SetTrackDiscontinuityIgnoredStatus((MediaType)type);
					}
					else
					{
						// reset the flag when both the paired discontinuities ignored; since no buffer pushed before.
						aamp->ResetTrackDiscontinuityIgnoredStatus();
					}

					logprintf("%s:%d - ignoring %s discontinuity since no buffer pushed before!", __FUNCTION__, __LINE__, name);
				}
				else if (isDiscoIgnoredForOtherTrack)
				{
					logprintf("%s:%d - discontinuity ignored for other AV track , no need to process %s track", __FUNCTION__, __LINE__, name);
					stopInjection = false;

					// reset the flag when both the paired discontinuities ignored.
					aamp->ResetTrackDiscontinuityIgnoredStatus();
				}
				else
				{
					stopInjection = context->ProcessDiscontinuity(type);
				}

				if (stopInjection)
				{
					ret = false;
					discontinuityProcessed = true;
					logprintf("%s:%d - track %s - stopping injection @position - %f", __FUNCTION__, __LINE__, name, cachedFragment->position);
				}
				else
				{
					logprintf("%s:%d - track %s - continuing injection", __FUNCTION__, __LINE__, name);
				}
			}
			else if (cachedFragment->discontinuity)
			{
				SignalTrickModeDiscontinuity();
			}

			if (!stopInjection)
			{
#ifdef AAMP_DEBUG_INJECT
				if ((1 << type) & AAMP_DEBUG_INJECT)
				{
					logprintf("%s:%d [%s] Inject uri %s", __FUNCTION__, __LINE__, name, cachedFragment->uri);
				}
#endif
				if (type == eTRACK_SUBTITLE)
				{
					if (mSubtitleParser)
					{
						mSubtitleParser->processData(cachedFragment->fragment.ptr, cachedFragment->fragment.len, cachedFragment->position, cachedFragment->duration);
					}
				}
				else
				{
#ifndef SUPRESS_DECODE
#ifndef FOG_HAMMER_TEST // support aamp stress-tests of fog without video decoding/presentation
					InjectFragmentInternal(cachedFragment, fragmentDiscarded);
#endif
#endif
				}
				if (eTRACK_VIDEO == type && GetContext()->GetProfileCount())
				{
					GetContext()->NotifyBitRateUpdate(cachedFragment->profileIndex, cachedFragment->cacheFragStreamInfo, cachedFragment->position);
				}
				AAMPLOG_TRACE("%s:%d [%p] - %s - injected cached uri at pos %f dur %f", __FUNCTION__, __LINE__, this, name, cachedFragment->position, cachedFragment->duration);
				if (!fragmentDiscarded)
				{
					totalInjectedDuration += cachedFragment->duration;
					mSegInjectFailCount = 0;
				}
				else
				{
					logprintf("%s:%d [%s] - Not updating totalInjectedDuration since fragment is Discarded", __FUNCTION__, __LINE__, name);
					mSegInjectFailCount++;
					int  SegInjectFailCount;
					GETCONFIGVALUE(eAAMPConfig_SegmentInjectThreshold,SegInjectFailCount); 
					if(SegInjectFailCount <= mSegInjectFailCount)
					{
						ret	= false;
						AAMPLOG_ERR("%s:%d [%s] Reached max inject failure count: %d, stopping playback",__FUNCTION__, __LINE__, name, SegInjectFailCount);
						aamp->SendErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR);
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
				StreamAbstractionAAMP* pContext = GetContext();
				if(pContext != NULL)
				{
					int rate = GetContext()->aamp->rate;
					aamp->EndOfStreamReached((MediaType)type);
					/*For muxed streams, provide EOS for audio track as well since
					 * no separate MediaTrack for audio is present*/
					MediaTrack* audio = GetContext()->GetMediaTrack(eTRACK_AUDIO);
					if (audio && !audio->enabled && rate == AAMP_NORMAL_PLAY_RATE)
					{
						aamp->EndOfStreamReached(eMEDIATYPE_AUDIO);
					}
				}
				else
				{
					AAMPLOG_WARN("%s:%d :  GetContext is null", __FUNCTION__, __LINE__);  //CID:81799 - Null Return
				}
			}
			else
			{
				logprintf("%s:%d - %s - NULL ptr to inject. fragmentIdxToInject %d", __FUNCTION__, __LINE__, name, fragmentIdxToInject);
			}
			ret = false;
		}
	}
	else
	{
		logprintf("WaitForCachedFragmentAvailable %s aborted", name);
		if (eosReached)
		{
			//Save the playback rate prior to sending EOS
			int rate = GetContext()->aamp->rate;
			aamp->EndOfStreamReached((MediaType)type);
			/*For muxed streams, provide EOS for audio track as well since
			 * no separate MediaTrack for audio is present*/
			MediaTrack* audio = GetContext()->GetMediaTrack(eTRACK_AUDIO);
			if (audio && !audio->enabled && rate == AAMP_NORMAL_PLAY_RATE)
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
	if(aamp_pthread_setname(pthread_self(), "aampInjector"))
	{
		logprintf("%s:%d: aamp_pthread_setname failed", __FUNCTION__, __LINE__);
	}
	track->RunInjectLoop();
	return NULL;
}

/**
 * @brief Fragment Chunk injector thread
 * @param arg Pointer to MediaTrack
 * @retval NULL
 */
static void *FragmentChunkInjector(void *arg)
{
	MediaTrack *track = (MediaTrack *)arg;
	bool bFlag = false;
	if(track->type == eTRACK_VIDEO)
	{
		bFlag = aamp_pthread_setname(pthread_self(), "VideoChunkInjector");
	}
	else if(track->type == eTRACK_AUDIO)
	{
		bFlag = aamp_pthread_setname(pthread_self(), "AudioChunkInjector");
	}
	if(!bFlag)
	{
		AAMPLOG_WARN("%s:%d: aamp_pthread_setname failed", __FUNCTION__, __LINE__);
	}
	track->RunInjectChunkLoop();
	return NULL;
}


/**
 * @brief Starts inject loop of track
 */
void MediaTrack::StartInjectLoop()
{
	abort = false;
	abortInject = false;
	discontinuityProcessed = false;
	assert(!fragmentInjectorThreadStarted);
	if (0 == pthread_create(&fragmentInjectorThreadID, NULL, &FragmentInjector, this))
	{
		fragmentInjectorThreadStarted = true;
	}
	else
	{
		logprintf("Failed to create FragmentInjector thread");
	}
}

/**
 * @brief Starts inject loop of track
 */
void MediaTrack::StartInjectChunkLoop()
{
	abort = false;
	abortInjectChunk = false;
	discontinuityProcessed = false;
	assert(!fragmentChunkInjectorThreadStarted);
	if (0 == pthread_create(&fragmentChunkInjectorThreadID, NULL, &FragmentChunkInjector, this))
	{
		fragmentChunkInjectorThreadStarted = true;
	}
	else
	{
        AAMPLOG_WARN("Failed to create FragmentChunkInjector thread");
	}
}

/**
 * @brief Injection loop - use internally by injection logic
 */
void MediaTrack::RunInjectLoop()
{
    AAMPLOG_WARN("fragment injector started. track %s", name);
    bool notifyFirstFragment = true;
    bool keepInjecting = true;
    if ((AAMP_NORMAL_PLAY_RATE == aamp->rate) && !bufferMonitorThreadStarted )

    {
        if (0 == pthread_create(&bufferMonitorThreadID, NULL, &BufferHealthMonitor, this))
        {
            bufferMonitorThreadStarted = true;
        }
        else
        {
            logprintf("Failed to create BufferHealthMonitor thread errno = %d, %s", errno, strerror(errno));
        }
    }
    totalInjectedDuration = 0;
    while (aamp->DownloadsAreEnabled() && keepInjecting)
    {
        if (!InjectFragment())
        {
            keepInjecting = false;
        }
        if (notifyFirstFragment && type != eTRACK_SUBTITLE)
        {
            notifyFirstFragment = false;
            GetContext()->NotifyFirstFragmentInjected();
        }
        // BCOM-2959  -- Disable audio video balancing for CDVR content ..
        // CDVR Content includes eac3 audio, the duration of audio doesnt match with video
        // and hence balancing fetch/inject not needed for CDVR
        if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback) && !aamp->IsCDVRContent())
        {
            StreamAbstractionAAMP* pContext = GetContext();
            if(pContext != NULL)
            {
                if(eTRACK_AUDIO == type)
                {
                    pContext->WaitForVideoTrackCatchup();
                }
                else if (eTRACK_VIDEO == type)
                {
                    pContext->ReassessAndResumeAudioTrack(false);
                }
                else if (eTRACK_SUBTITLE == type)
                {
                    pContext->WaitForAudioTrackCatchup();
                }
            }
            else
            {
                AAMPLOG_WARN("%s:%d :  GetContext  is null", __FUNCTION__, __LINE__);  //CID:85546 - Null Return
            }
        }
    }
    abortInject = true;
    AAMPLOG_WARN("fragment injector done. track %s", name);
}

/**
 * @brief Injection loop - use internally by injection logic
 */
void MediaTrack::RunInjectChunkLoop()
{
	AAMPLOG_WARN("fragment Chunk injector started. track %s", name);
	bool notifyFirstFragmentChunk = true;
	bool keepInjectingChunk = true;

	totalInjectedChunksDuration = 0;
	while (aamp->DownloadsAreEnabled() && keepInjectingChunk)
	{
		if (!InjectFragmentChunk())
		{
			keepInjectingChunk = false;
		}
	}
	abortInjectChunk = true;
	AAMPLOG_WARN("fragment chunk injector done. track %s", name);
}

/**
 * @brief Stop inject loop of track
 */
void MediaTrack::StopInjectLoop()
{
	if (fragmentInjectorThreadStarted)
	{
		int rc = pthread_join(fragmentInjectorThreadID, NULL);
		if (rc != 0)
		{
			logprintf("***pthread_join fragmentInjectorThread returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			logprintf("joined fragmentInjectorThread");
		}
#endif
	}
	fragmentInjectorThreadStarted = false;
}

/**
 * @brief Stop inject chunk loop of track
 */
void MediaTrack::StopInjectChunkLoop()
{
	if (fragmentChunkInjectorThreadStarted)
	{
		int rc = pthread_join(fragmentChunkInjectorThreadID, NULL);
		if (rc != 0)
		{
			logprintf("***pthread_join fragmentInjectorChunkThread returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			logprintf("joined fragmentInjectorChunkThread");
		}
#endif
	}
	fragmentChunkInjectorThreadStarted = false;
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
			logprintf("%s:%d fragment.ptr already set - possible memory leak", __FUNCTION__, __LINE__);
		}
		memset(&cachedFragment->fragment, 0x00, sizeof(GrowableBuffer));
	}
	return cachedFragment;
}

/**
 * @brief Get buffer to fetch chunk and cache next fragment chunk
 * @param[in] initialize true to initialize the fragment chunk
 * @retval Pointer to fragment chunk buffer.
 */
CachedFragmentChunk* MediaTrack::GetFetchChunkBuffer(bool initialize)
{
	if(fragmentChunkIdxToFetch <0 || fragmentChunkIdxToFetch >= maxCachedFragmentChunksPerTrack)
	{
		AAMPLOG_WARN("%s:%d [%s] OUT OF RANGE => fragmentChunkIdxToFetch: %d", __FUNCTION__, __LINE__,name,fragmentChunkIdxToFetch);
		return NULL;
	}

	CachedFragmentChunk* cachedFragmentChunk = NULL;
	cachedFragmentChunk = &this->cachedFragmentChunks[fragmentChunkIdxToFetch];

	AAMPLOG_TRACE("%s:%d [%s] fragmentChunkIdxToFetch: %d cachedFragmentChunk: %p", __FUNCTION__, __LINE__,name, fragmentChunkIdxToFetch, cachedFragmentChunk);

	if(initialize && cachedFragmentChunk)
	{
		if (cachedFragmentChunk->fragmentChunk.ptr)
		{
			AAMPLOG_WARN("%s:%d [%s] fragmentChunk.ptr[%p] already set - possible memory leak (len=[%d],avail=[%d])", __FUNCTION__, __LINE__,name, cachedFragmentChunk->fragmentChunk.ptr, cachedFragmentChunk->fragmentChunk.len,cachedFragmentChunk->fragmentChunk.avail);
		}
		memset(&cachedFragmentChunk->fragmentChunk, 0x00, sizeof(GrowableBuffer));
	}
	return cachedFragmentChunk;
}

/**
 * @brief Set current bandwidth of track
 * @param bandwidthBps bandwidth in bits per second
 */
void MediaTrack::SetCurrentBandWidth(int bandwidthBps)
{
	this->bandwidthBitsPerSecond = bandwidthBps;
}

/**
 * @brief Get profile index for TsbBandwidth
 * @param bandwidth - bandwidth to identify profile index from list
 * @retval profile index of the current bandwidth
 */
int MediaTrack::GetProfileIndexForBW(long mTsbBandwidth)
{
       return GetContext()->GetProfileIndexForBandwidth(mTsbBandwidth);
}

/**
 * @brief Get current bandwidth of track
 * @return bandwidth in bytes per second
 */
int MediaTrack::GetCurrentBandWidth()
{
	return this->bandwidthBitsPerSecond;
}

/**
 *   @brief Flushes all media fragments and resets all relevant counters
 * 			Only intended for use on subtitle streams
 *
 *   @return void
 */
void MediaTrack::FlushFragments()
{
	for (int i = 0; i < maxCachedFragmentsPerTrack; i++)
	{
		aamp_Free(&cachedFragment[i].fragment);
		memset(&cachedFragment[i], 0, sizeof(CachedFragment));
	}
	fragmentIdxToInject = 0;
	fragmentIdxToFetch = 0;
	numberOfFragmentsCached = 0;
	totalFetchedDuration = 0;
	totalFragmentsDownloaded = 0;
	totalInjectedDuration = 0;
}

/**
* @brief Cleans all cached fragment Chunks and unparsed buffer
*
* @return void
*/
void MediaTrack::CleanChunkCache()
{
	pthread_mutex_lock(&mutex);
	logprintf("%s:%d Clean Chunk Cache",__FUNCTION__, __LINE__);

	cleanChunkCache = true;
	pthread_cond_signal(&fragmentChunkFetched);

	int pthreadReturnValue = 0;
	pthreadReturnValue = pthread_cond_wait(&fragmentChunkClean, &mutex);
	if (0 != pthreadReturnValue)
	{
		AAMPLOG_WARN("%s:%d [%s] pthread_cond_wait(fragmentChunkClean) returned %s", __FUNCTION__, __LINE__, name, strerror(pthreadReturnValue));
	}
	logprintf("%s:%d [%s] wait complete for fragmentChunkClean", __FUNCTION__, __LINE__, name);

	cleanChunkCache = false;
	cleanChunkCacheInitiated = false;
	pthread_mutex_unlock(&mutex);
}

/**
 * @brief Flushes all cached fragment Chunks
 *
 * @return void
 */
void MediaTrack::FlushFragmentChunks()
{
	logprintf("%s:%d [%s]",__FUNCTION__, __LINE__, name);

	for (int i = 0; i < maxCachedFragmentChunksPerTrack; i++)
	{
		aamp_Free(&cachedFragmentChunks[i].fragmentChunk);
		memset(&cachedFragmentChunks[i], 0, sizeof(CachedFragmentChunk));
	}
	aamp_Free(&unparsedBufferChunk);
	memset(&unparsedBufferChunk, 0x00, sizeof(GrowableBuffer));
	aamp_Free(&parsedBufferChunk);
	memset(&parsedBufferChunk, 0x00, sizeof(GrowableBuffer));

	fragmentChunkIdxToInject = 0;
	fragmentChunkIdxToFetch = 0;
	numberOfFragmentChunksCached = 0;
	totalFragmentChunksDownloaded = 0;
	totalInjectedChunksDuration = 0;
}

/**
 * @brief MediaTrack Constructor
 * @param type Type of track
 * @param aamp Pointer to associated aamp instance
 * @param name Name of the track
 */
MediaTrack::MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name) :
		eosReached(false), enabled(false), numberOfFragmentsCached(0), numberOfFragmentChunksCached(0), fragmentIdxToInject(0), fragmentChunkIdxToInject(0),
fragmentIdxToFetch(0), fragmentChunkIdxToFetch(0), abort(false), fragmentInjectorThreadID(0), fragmentChunkInjectorThreadID(0),bufferMonitorThreadID(0), totalFragmentsDownloaded(0), totalFragmentChunksDownloaded(0),
		fragmentInjectorThreadStarted(false), fragmentChunkInjectorThreadStarted(false),bufferMonitorThreadStarted(false), totalInjectedDuration(0), totalInjectedChunksDuration(0), currentInitialCacheDurationSeconds(0),
		sinkBufferIsFull(false), cachingCompleted(false), fragmentDurationSeconds(0),  segDLFailCount(0),segDrmDecryptFailCount(0),mSegInjectFailCount(0),
		bufferStatus(BUFFER_STATUS_GREEN), prevBufferStatus(BUFFER_STATUS_GREEN),
		bandwidthBitsPerSecond(0), totalFetchedDuration(0),
		discontinuityProcessed(false), ptsError(false), cachedFragment(NULL), name(name), type(type), aamp(aamp),
		mutex(), fragmentFetched(), fragmentInjected(), abortInject(false),
		mSubtitleParser(), refreshSubtitles(false), maxCachedFragmentsPerTrack(0),
		totalMdatCount(0), cachedFragmentChunks{}, unparsedBufferChunk{}, parsedBufferChunk{}, fragmentChunkFetched(), fragmentChunkInjected(), abortInjectChunk(false), maxCachedFragmentChunksPerTrack(0),
		fragmentChunkClean(), cleanChunkCache(false),cleanChunkCacheInitiated(false), noMDATCount(0)
{
	GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached,maxCachedFragmentsPerTrack);
	cachedFragment = new CachedFragment[maxCachedFragmentsPerTrack];
	for(int X =0; X< maxCachedFragmentsPerTrack; ++X){
		memset(&cachedFragment[X], 0, sizeof(CachedFragment));
	}

	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		GETCONFIGVALUE(eAAMPConfig_MaxFragmentChunkCached,maxCachedFragmentChunksPerTrack);
		for(int X =0; X< DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK; ++X)
			memset(&cachedFragmentChunks[X], 0x00, sizeof(CachedFragmentChunk));

		pthread_cond_init(&fragmentChunkFetched, NULL);
		pthread_cond_init(&fragmentChunkInjected, NULL);
		pthread_cond_init(&fragmentChunkClean, NULL);
	}

	pthread_cond_init(&fragmentFetched, NULL);
	pthread_cond_init(&fragmentInjected, NULL);
	pthread_mutex_init(&mutex, NULL);
}

/**
 * @brief MediaTrack Destructor
 */
MediaTrack::~MediaTrack()
{
	if (bufferMonitorThreadStarted)
	{
		int rc = pthread_join(bufferMonitorThreadID, NULL);
		if (rc != 0)
		{
			logprintf("***pthread_join bufferMonitorThreadID returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			logprintf("joined bufferMonitorThreadID");
		}
#endif
	}

	if (fragmentInjectorThreadStarted)
	{
		// DELIA-45035: For debugging purpose
		logprintf("In MediaTrack destructor - fragmentInjectorThreads are still running, signalling cond variable");
	}
	if (fragmentChunkInjectorThreadStarted)
	{
		// DELIA-45035: For debugging purpose
		logprintf("In MediaTrack destructor - fragmentChunkInjectorThreads are still running, signalling cond variable");
	}

	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		FlushFragmentChunks();
		pthread_cond_destroy(&fragmentChunkFetched);
		pthread_cond_destroy(&fragmentChunkInjected);
		pthread_cond_destroy(&fragmentChunkClean);
	}
    
	for (int j = 0; j < maxCachedFragmentsPerTrack; j++)
	{
		aamp_Free(&cachedFragment[j].fragment);
		memset(&cachedFragment[j], 0x00, sizeof(CachedFragment));
	}

	if(cachedFragment)
	{
		delete [] cachedFragment;
		cachedFragment = NULL;
	}
	
	pthread_cond_destroy(&fragmentFetched);
	pthread_cond_destroy(&fragmentInjected);
	pthread_mutex_destroy(&mutex);
}

/**
 *   @brief Unblocks track if caught up with video or downloads are stopped
 *
 */
void StreamAbstractionAAMP::ReassessAndResumeAudioTrack(bool abort)
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	MediaTrack *aux = GetMediaTrack(eTRACK_AUX_AUDIO);
	if( audio && video )
	{
		pthread_mutex_lock(&mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();
		if(audioDuration < (videoDuration + (2 * video->fragmentDurationSeconds)) || !aamp->DownloadsAreEnabled() || video->IsDiscontinuityProcessed() || abort || video->IsAtEndOfTrack())
		{
			pthread_cond_signal(&mCond);
#ifdef AAMP_DEBUG_FETCH_INJECT
			logprintf("%s:%d signalling cond - audioDuration %f videoDuration %f",
				__FUNCTION__, __LINE__, audioDuration, videoDuration);
#endif
		}
		if (aux && aux->enabled)
		{
			double auxDuration = aux->GetTotalInjectedDuration();
			if (auxDuration < (videoDuration + (2 * video->fragmentDurationSeconds)) || !aamp->DownloadsAreEnabled() || video->IsDiscontinuityProcessed() || abort || video->IsAtEndOfTrack())
			{
				pthread_cond_signal(&mAuxCond);
#ifdef AAMP_DEBUG_FETCH_INJECT
				logprintf("%s:%d signalling cond - auxDuration %f videoDuration %f",
					__FUNCTION__, __LINE__, auxDuration, videoDuration);
#endif
			}
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
	if(video != NULL)
	{

		struct timespec ts;
		int ret = 0;

		pthread_mutex_lock(&mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();

		while ((audioDuration > (videoDuration + video->fragmentDurationSeconds)) && aamp->DownloadsAreEnabled() && !audio->IsDiscontinuityProcessed() && !video->IsInjectionAborted() && !(video->IsAtEndOfTrack()))
		{
	#ifdef AAMP_DEBUG_FETCH_INJECT
			logprintf("%s:%d waiting for cond - audioDuration %f videoDuration %f video->fragmentDurationSeconds %f",
				__FUNCTION__, __LINE__, audioDuration, videoDuration,video->fragmentDurationSeconds);
	#endif
			ts = aamp_GetTimespec(100);

			ret = pthread_cond_timedwait(&mCond, &mLock, &ts);

			if (ret == 0)
			{
				break;
			}
			if (ret != ETIMEDOUT)
			{
				logprintf("%s:%d error while calling pthread_cond_timedwait - %s", __FUNCTION__, __LINE__, strerror(ret));
			}
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  video  is null", __FUNCTION__, __LINE__);  //CID:85054 - Null Returns
	}
	pthread_mutex_unlock(&mLock);
}


/**
 * @brief StreamAbstractionAAMP Constructor
 * @param[in] aamp pointer to PrivateInstanceAAMP object associated with stream
 */
StreamAbstractionAAMP::StreamAbstractionAAMP(PrivateInstanceAAMP* aamp):
		trickplayMode(false), currentProfileIndex(0), mCurrentBandwidth(0),currentAudioProfileIndex(-1),currentTextTrackProfileIndex(-1),
		mTsbBandwidth(0),mNwConsistencyBypass(true), profileIdxForBandwidthNotification(0),
		hasDrm(false), mIsAtLivePoint(false), mESChangeStatus(false),
		mNetworkDownDetected(false), mTotalPausedDurationMS(0), mIsPaused(false),
		mStartTimeStamp(-1),mLastPausedTimeStamp(-1), aamp(aamp),
		mIsPlaybackStalled(false), mCheckForRampdown(false), mTuneType(), mLock(),
		mCond(), mLastVideoFragCheckedforABR(0), mLastVideoFragParsedTimeMS(0),
		mAbrManager(), mSubCond(), mAudioTracks(), mTextTracks(),mABRHighBufferCounter(0),mABRLowBufferCounter(0),mMaxBufferCountCheck(0),
		mStateLock(), mStateCond(), mTrackState(eDISCONTIUITY_FREE),
		mRampDownLimit(-1), mRampDownCount(0),mABRMaxBuffer(0), mABRCacheLength(0), mABRMinBuffer(0), mABRNwConsistency(0),
		mBitrateReason(eAAMP_BITRATE_CHANGE_BY_TUNE),
		mAudioTrackIndex(), mTextTrackIndex(),
		mAuxCond(), mFwdAudioToAux(false)
{
	mLastVideoFragParsedTimeMS = aamp_GetCurrentTimeMS();
	traceprintf("StreamAbstractionAAMP::%s", __FUNCTION__);
	pthread_mutex_init(&mLock, NULL);
	pthread_cond_init(&mCond, NULL);
	pthread_cond_init(&mSubCond, NULL);
	pthread_cond_init(&mAuxCond, NULL);

	pthread_mutex_init(&mStateLock, NULL);
	pthread_cond_init(&mStateCond, NULL);
	GETCONFIGVALUE(eAAMPConfig_ABRCacheLength,mMaxBufferCountCheck);
	mABRCacheLength = mMaxBufferCountCheck;
	GETCONFIGVALUE(eAAMPConfig_MaxABRNWBufferRampUp,mABRMaxBuffer);
	GETCONFIGVALUE(eAAMPConfig_MinABRNWBufferRampDown,mABRMinBuffer);
	GETCONFIGVALUE(eAAMPConfig_ABRNWConsistency,mABRNwConsistency); 
	// Set default init bitrate according to the config.
	mAbrManager.setDefaultInitBitrate(aamp->GetDefaultBitrate());
	long ibitrate = aamp->GetIframeBitrate();
	if (ibitrate > 0)
	{
		mAbrManager.setDefaultIframeBitrate(ibitrate);
	}
	GETCONFIGVALUE(eAAMPConfig_RampDownLimit,mRampDownLimit); 
	if (!aamp->IsNewTune())
	{
		mBitrateReason = (aamp->rate != AAMP_NORMAL_PLAY_RATE) ? eAAMP_BITRATE_CHANGE_BY_TRICKPLAY : eAAMP_BITRATE_CHANGE_BY_SEEK;
	}
}


/**
 * @brief StreamAbstractionAAMP Destructor
 */
StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
	traceprintf("StreamAbstractionAAMP::%s", __FUNCTION__);
	pthread_cond_destroy(&mCond);
	pthread_cond_destroy(&mSubCond);
	pthread_cond_destroy(&mAuxCond);
	pthread_mutex_destroy(&mLock);

	pthread_cond_destroy(&mStateCond);
	pthread_mutex_destroy(&mStateLock);
	AAMPLOG_INFO("Exit StreamAbstractionAAMP::%s", __FUNCTION__);
}

/**
 * @brief Get the last video fragment parsed time.
 *
 *	 @param None
 *	 @return Last video fragment parsed time.
 */
double StreamAbstractionAAMP::LastVideoFragParsedTimeMS(void)
{
	return mLastVideoFragParsedTimeMS;
}

/**
 *   @brief Get the desired profile to start fetching.
 *
 *   @param getMidProfile
 *   @retval profile index to be used for the track.
 */
int StreamAbstractionAAMP::GetDesiredProfile(bool getMidProfile)
{
	int desiredProfileIndex = 0;
	if(GetProfileCount())
	{
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
			StreamInfo* streamInfo = GetStreamInfo(profileIdxForBandwidthNotification);
			if(streamInfo != NULL)
			{
				video->SetCurrentBandWidth(streamInfo->bandwidthBitsPerSecond);
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  GetStreamInfo is null", __FUNCTION__, __LINE__);  //CID:81678 - Null Returns
			}
		}
		else
		{
			AAMPLOG_TRACE("%s:%d video track NULL", __FUNCTION__, __LINE__);
		}
		AAMPLOG_TRACE("%s:%d profileIdxForBandwidthNotification updated to %d ", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
	}
	return desiredProfileIndex;
}

/**
 *   @brief Notify bitrate updates to application.
 *   Used internally by injection logic
 *
 *   @param[in]  profileIndex - profile index of last injected fragment.
 */
void StreamAbstractionAAMP::NotifyBitRateUpdate(int profileIndex, const StreamInfo &cacheFragStreamInfo, double position)
{
	if (profileIndex != aamp->GetPersistedProfileIndex() && cacheFragStreamInfo.bandwidthBitsPerSecond != 0)
	{
		//logprintf("%s:%d stream Info bps(%ld) w(%d) h(%d) fr(%f)", __FUNCTION__, __LINE__, cacheFragStreamInfo.bandwidthBitsPerSecond, cacheFragStreamInfo.resolution.width, cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate);

		StreamInfo* streamInfo = GetStreamInfo(GetMaxBWProfile());
		if(streamInfo != NULL)
		{
			bool lGetBWIndex = false;
			/* START: Added As Part of DELIA-28363 and DELIA-28247 */
			if(aamp->IsTuneTypeNew && cacheFragStreamInfo.bandwidthBitsPerSecond == (streamInfo->bandwidthBitsPerSecond))
			{
				MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
				logprintf("NotifyBitRateUpdate: Max BitRate: %ld, timetotop: %f", cacheFragStreamInfo.bandwidthBitsPerSecond, video->GetTotalInjectedDuration());
				aamp->IsTuneTypeNew = false;
				lGetBWIndex = true;
			}
			/* END: Added As Part of DELIA-28363 and DELIA-28247 */

			// Send bitrate notification
			aamp->NotifyBitRateChangeEvent(cacheFragStreamInfo.bandwidthBitsPerSecond,
					cacheFragStreamInfo.reason, cacheFragStreamInfo.resolution.width,
					cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate, position, lGetBWIndex);
			// Store the profile , compare it before sending it . This avoids sending of event after trickplay if same bitrate
			aamp->SetPersistedProfileIndex(profileIndex);
		}
		else
		{
			AAMPLOG_WARN("%s:%d : StreamInfo  is null", __FUNCTION__, __LINE__);  //CID:82200 - Null Returns
		}
	}
}

bool StreamAbstractionAAMP::IsInitialCachingSupported()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	return (video && video->enabled);
}
/**
 *	 @brief Function to update stream info of current fetched fragment
 *
 *	 @param[in]  profileIndex - profile index of current fetched fragment
 *	 @param[out]  cacheFragStreamInfo - stream info of current fetched fragment
 */
void StreamAbstractionAAMP::UpdateStreamInfoBitrateData(int profileIndex, StreamInfo &cacheFragStreamInfo)
{
	StreamInfo* streamInfo = GetStreamInfo(profileIndex);

	if (streamInfo)
	{
		cacheFragStreamInfo.bandwidthBitsPerSecond = streamInfo->bandwidthBitsPerSecond;
		cacheFragStreamInfo.reason = mBitrateReason;
		cacheFragStreamInfo.resolution.height = streamInfo->resolution.height;
		cacheFragStreamInfo.resolution.framerate = streamInfo->resolution.framerate;
		cacheFragStreamInfo.resolution.width = streamInfo->resolution.width;
		//logprintf("%s:%d stream Info bps(%ld) w(%d) h(%d) fr(%f)", __FUNCTION__, __LINE__, cacheFragStreamInfo.bandwidthBitsPerSecond, cacheFragStreamInfo.resolution.width, cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate);
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
		desiredProfileIndex = GetMediaTrack(eTRACK_VIDEO)->GetProfileIndexForBW(mTsbBandwidth);
		mCurrentBandwidth = mTsbBandwidth;
		StreamInfo* streamInfo = GetStreamInfo(profileIdxForBandwidthNotification);
		if (profileIdxForBandwidthNotification != desiredProfileIndex)
		{
			if(streamInfo != NULL)
			{
				profileIdxForBandwidthNotification = desiredProfileIndex;
				GetMediaTrack(eTRACK_VIDEO)->SetCurrentBandWidth(streamInfo->bandwidthBitsPerSecond);
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_FOG_ABR;
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  GetStreamInfo is null", __FUNCTION__, __LINE__);  //CID:84179 - Null Returns
			}
		}
	}
}

/**
 * @brief Update ramp down profile reason on fragments download failure
 */
void StreamAbstractionAAMP::UpdateRampdownProfileReason(void)
{
	mBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;
}

/**
 * @brief GetDesiredProfileOnBuffer - Get the new profile corrected based on buffer availability
 */
void StreamAbstractionAAMP::GetDesiredProfileOnBuffer(int currProfileIndex, int &newProfileIndex)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);

	long currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
	long newBandwidth = GetStreamInfo(newProfileIndex)->bandwidthBitsPerSecond;
	double bufferValue = video->GetBufferedDuration();
	// Buffer levels 
	// Steadystate Buffer = 10sec - Good condition
	// Lower threshold before rampdown to happen - 5sec 
	// Higher threshold before attempting rampup - 15sec
	// So player to maintain steady state ABR if 10sec buffer is available and absorb all the shocks
	if(bufferValue > 0 )
	{
		if(newBandwidth > currentBandwidth)
		{
			// Rampup attempt . check if buffer availability is good before profile change
			// else retain current profile  
			if(bufferValue < mABRMaxBuffer)
				newProfileIndex = currProfileIndex;
		}
		else
		{
			// Rampdown attempt. check if buffer availability is good before profile change
			// Also if delta of current profile to new profile is 1 , then ignore the change
			// if bigger rampdown , then adjust to new profile 
			// else retain current profile
			double minBufferNeeded = video->fragmentDurationSeconds + aamp->mNetworkTimeoutMs/1000;
			if(bufferValue > minBufferNeeded && mAbrManager.getRampedDownProfileIndex(currProfileIndex) == newProfileIndex)
				newProfileIndex = currProfileIndex;
		}
	}
}

void StreamAbstractionAAMP::GetDesiredProfileOnSteadyState(int currProfileIndex, int &newProfileIndex, long nwBandwidth)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double bufferValue = video->GetBufferedDuration();

	if(bufferValue > 0 && currProfileIndex == newProfileIndex)
	{
		AAMPLOG_INFO("%s buffer:%f currProf:%d nwBW:%ld",__FUNCTION__,bufferValue,currProfileIndex,nwBandwidth);
		if(bufferValue > mABRMaxBuffer)
		{
			mABRHighBufferCounter++;
			mABRLowBufferCounter = 0 ;
			if(mABRHighBufferCounter > mMaxBufferCountCheck)
			{
				int nProfileIdx =  mAbrManager.getRampedUpProfileIndex(currProfileIndex);
				long newBandwidth = GetStreamInfo(nProfileIdx)->bandwidthBitsPerSecond;
				if(newBandwidth - nwBandwidth < 2000000)
					newProfileIndex = nProfileIdx;
				if(newProfileIndex  != currProfileIndex)
				{
					static int loop = 1;
					logprintf("%s Attempted rampup from steady state ->currProf:%d newProf:%d bufferValue:%f",__FUNCTION__,
					currProfileIndex,newProfileIndex,bufferValue);
					loop = (++loop >4)?1:loop;
					mMaxBufferCountCheck =  pow(mABRCacheLength,loop);
					mBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL;
				}
				// hand holding and rampup neednot be done every time. Give till abr cache to be full (ie abrCacheLength)
				// if rampup or rampdown happens due to throughput ,then its good . Else provide help to come out that state
				// counter is set back to 0 to prevent frequent rampup from multiple valley points
				mABRHighBufferCounter = 0;
			}
		}
		// steady state ,with no ABR cache available to determine actual bandwidth
		// this state can happen due to timeouts
		if(nwBandwidth == -1 && bufferValue < mABRMinBuffer && !video->IsInjectionAborted())
		{
			mABRLowBufferCounter++;
			mABRHighBufferCounter = 0;
			if(mABRLowBufferCounter > mABRCacheLength)
			{
				newProfileIndex =  mAbrManager.getRampedDownProfileIndex(currProfileIndex);
				if(newProfileIndex  != currProfileIndex)
				{
					logprintf("%s Attempted rampdown from steady state with low buffer ->currProf:%d newProf:%d bufferValue:%f ",__FUNCTION__,
					currProfileIndex,newProfileIndex,bufferValue);
					mBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY;
				}
				mABRLowBufferCounter = 0 ;
			}
		}
	}
	else
	{
		mABRLowBufferCounter = 0 ;
		mABRHighBufferCounter = 0;
	}
}


/**
 * @brief ConfigureTimeoutOnBuffer - Configure timeout of next download based on buffer
 */
void StreamAbstractionAAMP::ConfigureTimeoutOnBuffer()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);

	if(video && video->enabled)
	{
		// If buffer is high , set high timeout , not to fail the download 
		// If buffer is low , set timeout less than the buffer availability
		double vBufferDuration = video->GetBufferedDuration();
		if(vBufferDuration > 0)
		{
			long timeoutMs = (long)(vBufferDuration*1000); ;
			if(vBufferDuration < mABRMaxBuffer)
			{
				timeoutMs = aamp->mNetworkTimeoutMs;
			}
			else
			{	// enough buffer available 
				timeoutMs = std::min(timeoutMs/2,(long)(mABRMaxBuffer*1000));
				timeoutMs = std::max(timeoutMs , aamp->mNetworkTimeoutMs);
			}
			aamp->SetCurlTimeout(timeoutMs,eCURLINSTANCE_VIDEO);
			AAMPLOG_INFO("Setting Video timeout to :%ld %f",timeoutMs,vBufferDuration);
		}
	}
	if(audio && audio->enabled)
	{
		// If buffer is high , set high timeout , not to fail the download
		// If buffer is low , set timeout less than the buffer availability
		double aBufferDuration = audio->GetBufferedDuration();
		if(aBufferDuration > 0)
		{
			long timeoutMs = (long)(aBufferDuration*1000);
			if(aBufferDuration < mABRMaxBuffer)
			{
				timeoutMs = aamp->mNetworkTimeoutMs;
			}
			else
			{
				timeoutMs = std::min(timeoutMs/2,(long)(mABRMaxBuffer*1000));
				timeoutMs = std::max(timeoutMs , aamp->mNetworkTimeoutMs);
			}
			aamp->SetCurlTimeout(timeoutMs,eCURLINSTANCE_AUDIO);
			AAMPLOG_INFO("Setting Audio timeout to :%ld %f",timeoutMs,aBufferDuration);
		}
	}
}

/**
 * @brief Get desired profile based on cached duration
 * @retval index of desired profile based on cached duration
 */
int StreamAbstractionAAMP::GetDesiredProfileBasedOnCache(void)
{
	int desiredProfileIndex = currentProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if(video != NULL)
	{
		if (this->trickplayMode)
		{
			int tmpIframeProfile = GetIframeTrack();
			if(tmpIframeProfile != ABRManager::INVALID_PROFILE)
			{
				if (currentProfileIndex != tmpIframeProfile)
				{
					mBitrateReason = eAAMP_BITRATE_CHANGE_BY_ABR;
				}
				desiredProfileIndex = tmpIframeProfile;
			}
		}
		/*In live, fog takes care of ABR, and cache updating is not based only on bandwidth,
		 * but also depends on fragment availability in CDN*/
		else
		{
			long currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
			long networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
			int nwConsistencyCnt = (mNwConsistencyBypass)?1:mABRNwConsistency;
			// Ramp up/down (do ABR)
			desiredProfileIndex = mAbrManager.getProfileIndexByBitrateRampUpOrDown(currentProfileIndex,
					currentBandwidth, networkBandwidth, nwConsistencyCnt);

			AAMPLOG_INFO("%s currBW:%ld NwBW=%ld currProf:%d desiredProf:%d",__FUNCTION__,currentBandwidth,networkBandwidth,currentProfileIndex,desiredProfileIndex);
			if (currentProfileIndex != desiredProfileIndex)
			{
				// There is a chance that desiredProfileIndex is reset in below GetDesiredProfileOnBuffer call
				// Since bitrate notification will not be triggered in this case, its fine
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_ABR;
			}
			// For first time after tune, not to check for buffer availability, go for existing method .
			// during steady state run check the buffer for ramp up or ramp down
			if(!mNwConsistencyBypass && ISCONFIGSET(eAAMPConfig_ABRBufferCheckEnabled))
			{
				// Checking if frequent profile change happening
				if(currentProfileIndex != desiredProfileIndex)	
				{
					GetDesiredProfileOnBuffer(currentProfileIndex, desiredProfileIndex);
				}

				// Now check for Fixed BitRate for longer time(valley)
				GetDesiredProfileOnSteadyState(currentProfileIndex, desiredProfileIndex, networkBandwidth);

				// After ABR is done , next configure the timeouts for next downloads based on buffer
				ConfigureTimeoutOnBuffer();
			}
		}
		// only for first call, consistency check is ignored
		mNwConsistencyBypass = false;
	}
	else
	{
		AAMPLOG_WARN("%s:%d : video is null", __FUNCTION__, __LINE__);  //CID:84160 - Null Returns
	}
	return desiredProfileIndex;
}


/**
 * @brief Rampdown profile
 *
 * @param[in] http_error
 * @retval true on profile change
 */
bool StreamAbstractionAAMP::RampDownProfile(long http_error)
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
			if (ABRManager::INVALID_PROFILE != lowestIframeProfile)
			{
				desiredProfileIndex = lowestIframeProfile;
			}
			else
			{
				logprintf("%s:%d lowestIframeProfile Invalid - Stream does not has an iframe track!! ", __FUNCTION__, __LINE__);
			}
		}
	}
	else
	{
		desiredProfileIndex = mAbrManager.getRampedDownProfileIndex(currentProfileIndex);
	}
	if (desiredProfileIndex != currentProfileIndex)
	{
		AAMPAbrInfo stAbrInfo = {};

		stAbrInfo.abrCalledFor = AAMPAbrFragmentDownloadFailed;
		stAbrInfo.currentProfileIndex = currentProfileIndex;
		stAbrInfo.desiredProfileIndex = desiredProfileIndex;
		StreamInfo* streamInfodesired = GetStreamInfo(desiredProfileIndex);
		StreamInfo* streamInfocurrent = GetStreamInfo(currentProfileIndex);
		if((streamInfocurrent != NULL) || (streamInfodesired != NULL))
		{
			stAbrInfo.currentBandwidth = streamInfocurrent->bandwidthBitsPerSecond;
			stAbrInfo.desiredBandwidth = streamInfodesired->bandwidthBitsPerSecond;
			stAbrInfo.networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
			stAbrInfo.errorType = AAMPNetworkErrorHttp;
			stAbrInfo.errorCode = (int)http_error;

			AAMP_LOG_ABR_INFO(&stAbrInfo);

			aamp->UpdateVideoEndMetrics(stAbrInfo);

			if(ISCONFIGSET(eAAMPConfig_ABRBufferCheckEnabled))
			{
				// After Rampdown, configure the timeouts for next downloads based on buffer
				ConfigureTimeoutOnBuffer();
			}

			this->currentProfileIndex = desiredProfileIndex;
			profileIdxForBandwidthNotification = desiredProfileIndex;
			traceprintf("%s:%d profileIdxForBandwidthNotification updated to %d ", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
			ret = true;
			long newBW = GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond;
			video->SetCurrentBandWidth(newBW);
			aamp->ResetCurrentlyAvailableBandwidth(newBW,false,profileIdxForBandwidthNotification);
			mBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;

			// Send abr notification to XRE
			video->ABRProfileChanged();
			mABRLowBufferCounter = 0 ;
			mABRHighBufferCounter = 0;
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  GetStreamInfo is null", __FUNCTION__, __LINE__);  //CID:84132 - Null Returns
		}
	}

	return ret;
}

/**
 *	 @brief Check whether the current profile is lowest.
 *
 *	 @param currentProfileIndex - current profile index to be checked.
 *	 @return true if the given profile index is lowest.
 */
bool StreamAbstractionAAMP::IsLowestProfile(int currentProfileIndex)
{
	bool ret = false;

	if (trickplayMode)
	{
		if (currentProfileIndex == mAbrManager.getLowestIframeProfile())
		{
			ret = true;
		}
	}
	else
	{
		ret = mAbrManager.isProfileIndexBitrateLowest(currentProfileIndex);
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

	if (!aamp->CheckABREnabled())
	{
		return retValue;
	}

	if (!aamp->IsTSBSupported())
	{
		if (http_error == 404 || http_error == 502 || http_error == 500 || http_error == 503 || http_error == CURLE_PARTIAL_FILE)
		{
			if (RampDownProfile(http_error))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP::%s:%d > Condition Rampdown Success", __FUNCTION__, __LINE__);
				retValue = true;
			}
		}
		//For timeout, rampdown in single steps might not be enough
		else if (http_error == CURLE_OPERATION_TIMEDOUT)
		{
			// If lowest profile reached, then no need to check for ramp up/down for timeout cases, instead skip the failed fragment and jump to next fragment to download.
			if (!IsLowestProfile(currentProfileIndex))
			{
				if(UpdateProfileBasedOnFragmentCache())
				{
					retValue = true;
				}
				else if (RampDownProfile(http_error))
				{
					retValue = true;
				}
			}
		}
	}

	if ((true == retValue) && (mRampDownLimit > 0))
	{
		mRampDownCount++;
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
		if(video != NULL)
		{
			double totalFetchedDuration = video->GetTotalFetchedDuration();
			bool checkProfileChange = true;
			int abrSkipDuration;
			GETCONFIGVALUE(eAAMPConfig_ABRSkipDuration,abrSkipDuration) ;
			//Avoid doing ABR during initial buffering which will affect tune times adversely
			if ( totalFetchedDuration > 0 && totalFetchedDuration < abrSkipDuration)
			{
				//For initial fragment downloads, check available bw is less than default bw
				long availBW = aamp->GetCurrentlyAvailableBandwidth();
				long currBW = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;

				//If available BW is less than current selected one, we need ABR
				if (availBW > 0 && availBW < currBW)
				{
					logprintf("%s:%d Changing profile due to low available bandwidth(%ld) than default(%ld)!! ", __FUNCTION__, __LINE__, availBW, currBW);
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
		else
		{
			AAMPLOG_WARN("%s:%d :  Video is null", __FUNCTION__, __LINE__);  //CID:82070 - Null Returns
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


/**
 *   @brief Function called when playback is paused to update related flags.
 *
 *   @param[in] paused - true if playback paused, otherwise false.
 */
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
			logprintf("StreamAbstractionAAMP:%s() mLastPausedTimeStamp -1", __FUNCTION__);
		}
	}
}


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
	if (videoBufferIsEmpty || audioBufferIsEmpty) /* Changed the condition from '&&' to '||', becasue if video getting stalled it doesn't need to wait until audio become dry */
	{
		logprintf("StreamAbstractionAAMP:%s() Stall detected. Buffer status is RED!", __FUNCTION__);
		return true;
	}
	return false;
}


/**
 * @brief Update profile state based on cached fragments
 *
 * @return true if profile was changed, false otherwise
 */
bool StreamAbstractionAAMP::UpdateProfileBasedOnFragmentCache()
{
	bool retVal = false;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	int desiredProfileIndex = GetDesiredProfileBasedOnCache();
	if (desiredProfileIndex != currentProfileIndex)
	{
#if 0 /* Commented since the same is supported via AAMP_LOG_ABR_INFO */
		logprintf("**aamp changing profile: %d->%d [%ld->%ld]",
			currentProfileIndex, desiredProfileIndex,
			GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond,
			GetStreamInfo(desiredProfileIndex)->bandwidthBitsPerSecond);
#else
		AAMPAbrInfo stAbrInfo = {};

		stAbrInfo.abrCalledFor = AAMPAbrBandwidthUpdate;
		stAbrInfo.currentProfileIndex = currentProfileIndex;
		stAbrInfo.desiredProfileIndex = desiredProfileIndex;
		stAbrInfo.currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
		stAbrInfo.desiredBandwidth = GetStreamInfo(desiredProfileIndex)->bandwidthBitsPerSecond;
		stAbrInfo.networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
		stAbrInfo.errorType = AAMPNetworkErrorNone;

		AAMP_LOG_ABR_INFO(&stAbrInfo);
		aamp->UpdateVideoEndMetrics(stAbrInfo);
#endif /* 0 */

		this->currentProfileIndex = desiredProfileIndex;
		profileIdxForBandwidthNotification = desiredProfileIndex;
		traceprintf("%s:%d profileIdxForBandwidthNotification updated to %d ", __FUNCTION__, __LINE__, profileIdxForBandwidthNotification);
		video->ABRProfileChanged();
		long newBW = GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond;
		video->SetCurrentBandWidth(newBW);
		aamp->ResetCurrentlyAvailableBandwidth(newBW,false,profileIdxForBandwidthNotification);
		mABRLowBufferCounter = 0 ;
		mABRHighBufferCounter = 0;
		retVal = true;
	}
	return retVal;
}


/**
 *   @brief Check if playback has stalled and update related flags.
 *
 *   @param[in] fragmentParsed - true if next fragment was parsed, otherwise false
 */
void StreamAbstractionAAMP::CheckForPlaybackStall(bool fragmentParsed, bool isStalledBeforePlay)
{
	if (fragmentParsed)
	{
		mLastVideoFragParsedTimeMS = aamp_GetCurrentTimeMS();
		if (mIsPlaybackStalled)
		{
			mIsPlaybackStalled = false;
		}
	}
	else
	{
		/** Need to confirm if we are stalled here */
		double timeElapsedSinceLastFragment = (aamp_GetCurrentTimeMS() - mLastVideoFragParsedTimeMS);

		// We have not received a new fragment for a long time, check for cache empty required for dash
		MediaTrack* mediatrack = GetMediaTrack(eTRACK_VIDEO);
		if(mediatrack != NULL)
		{
			int stalltimeout;
			GETCONFIGVALUE(eAAMPConfig_StallTimeoutMS,stalltimeout);
			if (!mNetworkDownDetected && (timeElapsedSinceLastFragment > stalltimeout) && mediatrack->numberOfFragmentsCached == 0)
			{
				AAMPLOG_INFO("StreamAbstractionAAMP::%s() Didn't download a new fragment for a long time(%f) and cache empty!", __FUNCTION__, timeElapsedSinceLastFragment);
				mIsPlaybackStalled = true;
				if (CheckIfPlayerRunningDry())
				{
					logprintf("StreamAbstractionAAMP::%s() Stall detected!. Time elapsed since fragment parsed(%f), caches are all empty!", __FUNCTION__, timeElapsedSinceLastFragment);
					aamp->SendStalledErrorEvent(isStalledBeforePlay);
				}
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  GetMediaTrack  is null", __FUNCTION__, __LINE__);  //CID:85383 - Null Returns
		}
	}
}

/**
 *   @brief MediaTracks shall call this to notify first fragment is injected.
 */
void StreamAbstractionAAMP::NotifyFirstFragmentInjected()
{
	pthread_mutex_lock(&mLock);
	mIsPaused = false;
	mLastPausedTimeStamp = -1;
	mTotalPausedDurationMS = 0;
	mStartTimeStamp = aamp_GetCurrentTimeMS();
	pthread_mutex_unlock(&mLock);
}

/**
 *   @brief Get elapsed time of play-back.
 *
 *   @return elapsed time.
 */
double StreamAbstractionAAMP::GetElapsedTime()
{
	double elapsedTime;
	pthread_mutex_lock(&mLock);
	traceprintf("StreamAbstractionAAMP:%s() mStartTimeStamp %lld mTotalPausedDurationMS %lld mLastPausedTimeStamp %lld", __FUNCTION__, mStartTimeStamp, mTotalPausedDurationMS, mLastPausedTimeStamp);
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

/**
 *   @brief Get the bitrate of current video profile selected.
 *
 *   @return bitrate of current video profile.
 */
long StreamAbstractionAAMP::GetVideoBitrate(void)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	return ((video && video->enabled) ? (video->GetCurrentBandWidth()) : 0);
}


/**
 *   @brief Get the bitrate of current audio profile selected.
 *
 *   @return bitrate of current audio profile.
 */
long StreamAbstractionAAMP::GetAudioBitrate(void)
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	return ((audio && audio->enabled) ? (audio->GetCurrentBandWidth()) : 0);
}


/**
 *   @brief Check if current stream is muxed
 *
 *   @return true if current stream is muxed
 */
bool StreamAbstractionAAMP::IsMuxedStream()
{
	bool ret = false;

	if ((!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback)) && (AAMP_NORMAL_PLAY_RATE == aamp->rate))
	{
		MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
		MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
		if (!audio || !video || !audio->enabled || !video->enabled)
		{
			ret = true;
		}
	}
	return ret;
}


/**
 *   @brief Waits subtitle track injection until caught up with muxed/audio track.
 *   Used internally by injection logic
 */
void StreamAbstractionAAMP::WaitForAudioTrackCatchup()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if(subtitle != NULL)
	{
		//Check if its muxed a/v
		if (audio && !audio->enabled)
		{
			audio = GetMediaTrack(eTRACK_VIDEO);
		}

		struct timespec ts;
		int ret = 0;

		pthread_mutex_lock(&mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double subtitleDuration = subtitle->GetTotalInjectedDuration();
		//Allow subtitles to be ahead by 5 seconds compared to audio
		while ((subtitleDuration > (audioDuration + audio->fragmentDurationSeconds + 15.0)) && aamp->DownloadsAreEnabled() && !subtitle->IsDiscontinuityProcessed() && !audio->IsInjectionAborted())
		{
			traceprintf("Blocked on Inside mSubCond with sub:%f and audio:%f", subtitleDuration, audioDuration);
	#ifdef AAMP_DEBUG_FETCH_INJECT
			logprintf("%s:%d waiting for mSubCond - subtitleDuration %f audioDuration %f",
				__FUNCTION__, __LINE__, subtitleDuration, audioDuration);
	#endif
			ts = aamp_GetTimespec(100);

			ret = pthread_cond_timedwait(&mSubCond, &mLock, &ts);

			if (ret == 0)
			{
				break;
			}
			if (ret != ETIMEDOUT)
			{
				logprintf("%s:%d error while calling pthread_cond_timedwait - %s", __FUNCTION__, __LINE__, strerror(ret));
			}
			audioDuration = audio->GetTotalInjectedDuration();
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  subtitle    is null", __FUNCTION__, __LINE__);  //CID:85996 - Null Returns
	}
	pthread_mutex_unlock(&mLock);
}


/**
 *   @brief Unblocks subtitle track injection if downloads are stopped
 *
 */
void StreamAbstractionAAMP::AbortWaitForAudioTrackCatchup(bool force)
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled)
	{
		pthread_mutex_lock(&mLock);
		if (force || !aamp->DownloadsAreEnabled())
		{
			pthread_cond_signal(&mSubCond);
#ifdef AAMP_DEBUG_FETCH_INJECT
			logprintf("%s:%d signalling mSubCond", __FUNCTION__, __LINE__);
#endif
		}
		pthread_mutex_unlock(&mLock);
	}
}

void StreamAbstractionAAMP::MuteSubtitles(bool mute)
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		subtitle->mSubtitleParser->mute(mute);
	}
}

/**
 *   @brief Checks if streamer reached end of stream
 *
 *   @return true if end of stream reached, false otherwise
 */
bool StreamAbstractionAAMP::IsEOSReached()
{
	bool eos = true;
	for (int i = 0 ; i < AAMP_TRACK_COUNT; i++)
	{
		MediaTrack *track = GetMediaTrack((TrackType) i);
		if (track && track->enabled)
		{
			eos = eos && track->IsAtEndOfTrack();
			if (!eos)
			{
				AAMPLOG_WARN("%s:%d EOS not seen by track: %s, skip check for rest of the tracks", __FUNCTION__, __LINE__, track->name);
				aamp->ResetEOSSignalledFlag();
				break;
			}
		}
	}
	return eos;
}

/*
 *   @brief Function to returns last injected fragment position
 *
 *   @return double last injected fragment position in seconds
 */
double StreamAbstractionAAMP::GetLastInjectedFragmentPosition()
{
	// We get the position of video, we use video position for most of our position related things
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double pos = 0;
	if (video)
	{
		pos = video->GetTotalInjectedDuration();
	}
	AAMPLOG_INFO("%s:%d Last Injected fragment Position : %f", __FUNCTION__, __LINE__, pos);
	return pos;
}

/**
 * @brief To check for discontinuity in future fragments.
 *
 * @param[out] cachedDuration - cached fragment duration in seconds
 * @return bool - true if discontinuity present, false otherwise
 */
bool MediaTrack::CheckForFutureDiscontinuity(double &cachedDuration)
{
	bool ret = false;
	cachedDuration = 0;
	pthread_mutex_lock(&mutex);

	int start = fragmentIdxToInject;
	int count = numberOfFragmentsCached;
	while (count > 0)
	{
		if (!ret)
		{
			ret = ret || cachedFragment[start].discontinuity;
			if (ret)
			{
				AAMPLOG_WARN("%s:%d Found discontinuity for track %s at index: %d and position - %f", __FUNCTION__, __LINE__, name, start, cachedFragment[start].position);
			}
		}
		cachedDuration += cachedFragment[start].duration;
		if (++start == maxCachedFragmentsPerTrack)
		{
			start = 0;
		}
		count--;
	}
	AAMPLOG_WARN("%s:%d track %s numberOfFragmentsCached - %d, cachedDuration - %f", __FUNCTION__, __LINE__, name, numberOfFragmentsCached, cachedDuration);
	pthread_mutex_unlock(&mutex);

	return ret;
}

void MediaTrack::OnSinkBufferFull()
{
	//check if we should stop initial caching here
	if(sinkBufferIsFull)
	{
		return;
	}

	bool notifyCacheCompleted = false;

	pthread_mutex_lock(&mutex);
	sinkBufferIsFull = true;
	// check if cache buffer is full and caching was needed
	if( numberOfFragmentsCached == maxCachedFragmentsPerTrack
			&& (eTRACK_VIDEO == type)
			&& aamp->IsFragmentCachingRequired()
			&& !cachingCompleted)
	{
		logprintf("## %s:%d [%s] Cache is Full cacheDuration %d minInitialCacheSeconds %d, aborting caching!##",
							__FUNCTION__, __LINE__, name, currentInitialCacheDurationSeconds, aamp->GetInitialBufferDuration());
		notifyCacheCompleted = true;
		cachingCompleted = true;
	}
	pthread_mutex_unlock(&mutex);

	if(notifyCacheCompleted)
	{
		aamp->NotifyFragmentCachingComplete();
	}
}

/**
 *   @brief Function to process discontinuity.
 *
 *   @param[in] type - track type.
 */
bool StreamAbstractionAAMP::ProcessDiscontinuity(TrackType type)
{
	bool ret = true;
	MediaTrackDiscontinuityState state = eDISCONTIUITY_FREE;
	bool isMuxedAndAudioDiscoIgnored = false;

	pthread_mutex_lock(&mStateLock);
	if (type == eTRACK_VIDEO)
	{
		state = eDISCONTINUIY_IN_VIDEO;

		/*For muxed streams, give discontinuity for audio track as well*/
		MediaTrack* audio = GetMediaTrack(eTRACK_AUDIO);
		if (audio && !audio->enabled)
		{
			mTrackState = (MediaTrackDiscontinuityState) (mTrackState | eDISCONTINUIY_IN_BOTH);
			ret = aamp->Discontinuity(eMEDIATYPE_AUDIO, false);

			/* In muxed stream, if discontinuity-EOS processing for audio track failed, then set the "mProcessingDiscontinuity" flag of audio to true if video track discontinuity succeeded.
			 * In this case, no need to reset mTrackState by removing audio track, because need to process the video track discontinuity-EOS process since its a muxed stream.
			 */
			if (ret == false)
			{
				AAMPLOG_WARN("%s:%d muxed track audio discontinuity/EOS processing ignored!", __FUNCTION__, __LINE__);
				isMuxedAndAudioDiscoIgnored = true;
			}
		}
	}
	else if (type == eTRACK_AUDIO)
	{
		state = eDISCONTINUIY_IN_AUDIO;
	}
	// RDK-27796, bypass discontinuity check for auxiliary audio for now
	else if (type == eTRACK_AUX_AUDIO)
	{
		aamp->Discontinuity(eMEDIATYPE_AUX_AUDIO, false);
	}

	if (state != eDISCONTIUITY_FREE)
	{
		bool aborted = false;
		bool wait = false;
		mTrackState = (MediaTrackDiscontinuityState) (mTrackState | state);

		AAMPLOG_WARN("%s:%d mTrackState:%d!", __FUNCTION__, __LINE__, mTrackState);

		if (mTrackState == state)
		{
			wait = true;
			AAMPLOG_WARN("%s:%d track[%d] Going into wait for processing discontinuity in other track!", __FUNCTION__, __LINE__, type);
			pthread_cond_wait(&mStateCond, &mStateLock);

			MediaTrack *track = GetMediaTrack(type);
			if (track && track->IsInjectionAborted())
			{
				//AbortWaitForDiscontinuity called, don't push discontinuity
				//Just exit with ret = true to avoid InjectFragmentInternal
				aborted = true;
			}
			else if (type == eTRACK_AUDIO)
			{
				//AbortWaitForDiscontinuity() will be triggered by video first, check video injection aborted
				MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
				if (video && video->IsInjectionAborted())
				{
					aborted = true;
				}
			}

			//Check if mTrackState was reset from CheckForMediaTrackInjectionStall
			if ((!ISCONFIGSET(eAAMPConfig_RetuneForUnpairDiscontinuity) || type == eTRACK_AUDIO) && (!aborted && ((mTrackState & state) != state)))
			{
				//Ignore discontinuity
				ret = false;
				aborted = true;
			}
		}

		// We can't ensure that mTrackState == eDISCONTINUIY_IN_BOTH after wait, because
		// if Discontinuity() returns false, we need to reset the track bit from mTrackState
		if (mTrackState == eDISCONTINUIY_IN_BOTH || (wait && !aborted))
		{
			pthread_mutex_unlock(&mStateLock);

			ret = aamp->Discontinuity((MediaType) type, false);
			//Discontinuity ignored, so we need to remove state from mTrackState
			if (ret == false)
			{
				mTrackState = (MediaTrackDiscontinuityState) (mTrackState & ~state);
				AAMPLOG_WARN("%s:%d track:%d discontinuity processing ignored! reset mTrackState to: %d!", __FUNCTION__, __LINE__, type, mTrackState);
			}
			else if (isMuxedAndAudioDiscoIgnored && type == eTRACK_VIDEO)
			{
				// In muxed stream, set the audio track's mProcessingDiscontinuity flag to true to unblock the ProcessPendingDiscontinuity if video track discontinuity-EOS processing succeeded
				AAMPLOG_WARN("%s:%d set muxed track audio discontinuity flag to true since video discontinuity processing succeeded.", __FUNCTION__, __LINE__);
				aamp->Discontinuity((MediaType) eTRACK_AUDIO, true);
			}

			pthread_mutex_lock(&mStateLock);
			pthread_cond_signal(&mStateCond);
		}
	}
	pthread_mutex_unlock(&mStateLock);

	return ret;
}

/**
 * @brief Function to abort any wait for discontinuity by injector theads.
 */
void StreamAbstractionAAMP::AbortWaitForDiscontinuity()
{
	//Release injector thread blocked in ProcessDiscontinuity
	pthread_mutex_lock(&mStateLock);
	pthread_cond_signal(&mStateCond);
	pthread_mutex_unlock(&mStateLock);
}


/**
 *   @brief Function to check if any media tracks are stalled on discontinuity.
 *
 *   @param[in] type - track type.
 */
void StreamAbstractionAAMP::CheckForMediaTrackInjectionStall(TrackType type)
{
	MediaTrackDiscontinuityState state = eDISCONTIUITY_FREE;
	MediaTrack *track = GetMediaTrack(type);
	MediaTrack *otherTrack = NULL;
	bool bProcessFlag = false;

	if (type == eTRACK_AUDIO)
	{
		otherTrack = GetMediaTrack(eTRACK_VIDEO);
		state = eDISCONTINUIY_IN_AUDIO;
	}
	else if (type == eTRACK_VIDEO)
	{
		otherTrack = GetMediaTrack(eTRACK_AUDIO);
		state = eDISCONTINUIY_IN_VIDEO;
	}

	// If both tracks are available and enabled, then only check required
	if (track && track->enabled && otherTrack && otherTrack->enabled)
	{
		pthread_mutex_lock(&mStateLock);
		if (mTrackState == eDISCONTINUIY_IN_VIDEO || mTrackState == eDISCONTINUIY_IN_AUDIO)
		{
			bool isDiscontinuitySeen = mTrackState & state;
			if (isDiscontinuitySeen)
			{
				double cachedDuration = 0;
				bool isDiscontinuityPresent;
				double duration = track->GetTotalInjectedDuration();
				double otherTrackDuration = otherTrack->GetTotalInjectedDuration();
				double diff = otherTrackDuration - duration;
				AAMPLOG_WARN("%s:%d Discontinuity encountered in track:%d with injectedDuration:%f and other track injectedDuration:%f, fragmentDurationSeconds:%f, diff:%f",
								__FUNCTION__, __LINE__, type, duration, otherTrackDuration, track->fragmentDurationSeconds, diff);
				if (otherTrackDuration >= duration)
				{
					//Check for future discontinuity
					isDiscontinuityPresent = otherTrack->CheckForFutureDiscontinuity(cachedDuration);
					if (isDiscontinuityPresent)
					{
						//Scenario - video wait on discontinuity, and audio has a future discontinuity
						if (type == eTRACK_VIDEO)
						{
							AAMPLOG_WARN("%s:%d For discontinuity in track:%d, other track has injectedDuration:%f and future discontinuity, signal mCond var!",
									__FUNCTION__, __LINE__, type, otherTrackDuration);
							pthread_mutex_lock(&mLock);
							pthread_cond_signal(&mCond);
							pthread_mutex_unlock(&mLock);
						}
					}
					// If discontinuity is not seen in future fragments or if the unblocked track has finished more than 2 * fragmentDurationSeconds,
					// unblock this track
					else if (((diff + cachedDuration) > (2 * track->fragmentDurationSeconds)))
					{
						AAMPLOG_WARN("%s:%d Discontinuity in track:%d does not have a discontinuity in other track (diff: %f, injectedDuration: %f, cachedDuration: %f)",
								__FUNCTION__, __LINE__, type, diff, otherTrackDuration, cachedDuration);
						bProcessFlag = true;
					}
				}
				// Current track injected duration goes very huge value with the below cases
				// 1. When the EOS for earlier discontinuity missed to processing due to singular discontinuity or some edge case missing
				// 2. When there is no EOS processed message for the previous discontinuity seen from the pipeline.
				// In that case the diff value will go to negative and this CheckForMediaTrackInjectionStall() continuously called
				// until stall happens from outside or explicitely aamp_stop() to be called from XRE or Apps,
				// so need to control the stalling as soon as possible for the negative diff case from here.
				else if ((diff < 0) && (abs(diff) > (2 * track->fragmentDurationSeconds)))
				{
					AAMPLOG_WARN("%s:%d Discontinuity in track:%d does not have a discontinuity in other track (diff is negative: %f, injectedDuration: %f)",
							__FUNCTION__, __LINE__, type, diff, otherTrackDuration);
					isDiscontinuityPresent = otherTrack->CheckForFutureDiscontinuity(cachedDuration); // called just to get the value of cachedDuration of the track.
					bProcessFlag = true;
				}

				if (bProcessFlag)
				{
					if (ISCONFIGSET(eAAMPConfig_RetuneForUnpairDiscontinuity) && type != eTRACK_AUDIO)
					{
						if(aamp->GetBufUnderFlowStatus())
						{
							AAMPLOG_WARN("%s:%d Schedule retune since for discontinuity in track:%d other track doesn't have a discontinuity (diff: %f, injectedDuration: %f, cachedDuration: %f)",
									__FUNCTION__, __LINE__, type, diff, otherTrackDuration, cachedDuration);
							aamp->ScheduleRetune(eSTALL_AFTER_DISCONTINUITY, (MediaType) type);
						}
						else
						{
							//Check for PTS change for 1 second
							aamp->CheckForDiscontinuityStall((MediaType) type);
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d Ignoring discontinuity in track:%d since other track doesn't have a discontinuity (diff: %f, injectedDuration: %f, cachedDuration: %f)",
								__FUNCTION__, __LINE__, type, diff, otherTrackDuration, cachedDuration);
						mTrackState = (MediaTrackDiscontinuityState) (mTrackState & ~state);
						pthread_cond_signal(&mStateCond);
					}
				}
			}
		}
		pthread_mutex_unlock(&mStateLock);
	}
}
/**
 *   @brief Check for ramp down limit reached by player
 *   @return true if limit reached, false otherwise
 */
bool StreamAbstractionAAMP::CheckForRampDownLimitReached()
{
	bool ret = false;
	// Check rampdownlimit reached when the value is set,
	// limit will be -1 by default, function will return false to attempt rampdown.
	if ((mRampDownCount >= mRampDownLimit) && (mRampDownLimit >= 0))
	{
		ret = true;
		mRampDownCount = 0;
		AAMPLOG_WARN("%s:%d Rampdown limit reached, Limit is %d", __FUNCTION__, __LINE__, mRampDownLimit);
	}
	return ret;
}

/**
 *   @brief Get buffered video duration in seconds
 *
 *   @return duration of currently buffered video in seconds
 */
double StreamAbstractionAAMP::GetBufferedVideoDurationSec()
{
	// do not support trickplay track
	if(AAMP_NORMAL_PLAY_RATE != aamp->rate)
	{
		return -1.0;
	}

	return GetBufferedDuration();
}

/**
 *   @brief Get current audio track
 *
 *   @return int - index of current audio track
 */
int StreamAbstractionAAMP::GetAudioTrack()
{
	int index = -1;
	if (!mAudioTrackIndex.empty())
	{
		for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
		{
			if (it->index == mAudioTrackIndex)
			{
				index = std::distance(mAudioTracks.begin(), it);
			}
		}
	}
	return index;
}

/**
 *   @brief Get current text track
 *
 *   @return int - index of current text track
 */
int StreamAbstractionAAMP::GetTextTrack()
{
	int index = -1;
	if (!mTextTrackIndex.empty())
	{
		for (auto it = mTextTracks.begin(); it != mTextTracks.end(); it++)
		{
			if (it->index == mTextTrackIndex)
			{
				index = std::distance(mTextTracks.begin(), it);
			}
		}
	}
	return index;
}

/**
 *   @brief Refresh subtitle track
 *
 *   @return void
 */
void StreamAbstractionAAMP::RefreshSubtitles()
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		logprintf("Setting refreshSubtitles");
		subtitle->refreshSubtitles = true;
		subtitle->AbortWaitForCachedAndFreeFragment(true);
	}
}


/**
 *   @brief Waits aux track injection until caught up with video track.
 *   Used internally by injection logic
 */
void StreamAbstractionAAMP::WaitForVideoTrackCatchupForAux()
{
	MediaTrack *aux = GetMediaTrack(eTRACK_AUX_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if(video != NULL)
	{

		struct timespec ts;
		int ret = 0;

		pthread_mutex_lock(&mLock);
		double auxDuration = aux->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();

		while ((auxDuration > (videoDuration + video->fragmentDurationSeconds)) && aamp->DownloadsAreEnabled() && !aux->IsDiscontinuityProcessed() && !video->IsInjectionAborted() && !(video->IsAtEndOfTrack()))
		{
	#ifdef AAMP_DEBUG_FETCH_INJECT
			logprintf("%s:%d waiting for cond - auxDuration %f videoDuration %f video->fragmentDurationSeconds %f",
				__FUNCTION__, __LINE__, auxDuration, videoDuration, video->fragmentDurationSeconds);
	#endif
			ts = aamp_GetTimespec(100);

			ret = pthread_cond_timedwait(&mAuxCond, &mLock, &ts);

			if (ret == 0)
			{
				break;
			}
	#ifndef WIN32
			if (ret != ETIMEDOUT)
			{
				logprintf("%s:%d error while calling pthread_cond_timedwait - %s", __FUNCTION__, __LINE__, strerror(ret));
			}
	#endif
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  video  is null", __FUNCTION__, __LINE__);  //CID:85054 - Null Returns
	}
	pthread_mutex_unlock(&mLock);
}
