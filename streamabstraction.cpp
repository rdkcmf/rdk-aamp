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
 * distributed under the License is distributed on an
 "AS IS" BASIS,
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
#include "AampCacheHandler.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <iterator>
#include <sys/time.h>
#include <cmath>

#define AAMP_BUFFER_MONITOR_GREEN_THRESHOLD 4 //2 fragments for MSO specific linear streams.
//#define AAMP_DEBUG_INJECT_CHUNK
//#define AAMP_DEBUG_FETCH_INJECT 1

// checks if current state is going to use IFRAME ( Fragment/Playlist )
#define IS_FOR_IFRAME(rate, type) ((type == eTRACK_VIDEO) && (rate != AAMP_NORMAL_PLAY_RATE))

#define MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS (6000)
#define MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS (500) // 500mSec

using namespace std;

/**
 * @brief Thread funtion for Buffer Health Monitoring
 */
static void* BufferHealthMonitor(void* user_data)
{
	MediaTrack *track = (MediaTrack *)user_data;
	if(aamp_pthread_setname(pthread_self(), "aampBuffHealth"))
	{
		AAMPLOG_WARN("aamp_pthread_setname failed");
	}
	track->MonitorBufferHealth();
	return NULL;
}

/**
 * @brief Start playlist downloader loop
 */
void MediaTrack::StartPlaylistDownloaderThread()
{
	AAMPLOG_TRACE("Starting playlist downloader for %s", name);
	if(!playlistDownloaderThreadStarted)
	{
		// Start a new thread for this track
		if(NULL == playlistDownloaderThread)
		{
			// Set thread abort flag to false and start the thread.
			abortPlaylistDownloader = false;
			playlistDownloaderThread = new std::thread(&MediaTrack::PlaylistDownloader, this);
			playlistDownloaderThreadStarted = true;
		}
		else
		{
			AAMPLOG_ERR("Failed to start thread, already initialized for %s", name);
		}
	}
	else
	{
		AAMPLOG_INFO("Thread already running for %s", name);
	}
}

/**
 * @brief Stop playlist downloader loop
 */
void MediaTrack::StopPlaylistDownloaderThread()
{
	if ((playlistDownloaderThreadStarted) && (playlistDownloaderThread) && (playlistDownloaderThread->joinable()))
	{
		abortPlaylistDownloader = true;
		AbortWaitForPlaylistDownload();
		AbortFragmentDownloaderWait();
		playlistDownloaderThread->join();
		delete playlistDownloaderThread;
		playlistDownloaderThread = NULL;
		playlistDownloaderThreadStarted = false;
		AAMPLOG_WARN("[%s] Aborted", name);
	}
}

/**
 * @brief Get string corresponding to buffer status.
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
        AAMPLOG_WARN("[%s] bufferedTime %f totalInjectedDuration %f elapsed time %f",
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
				AAMPLOG_WARN("aamp: track[%s] buffering %s->%s", name, GetBufferHealthStatusString(prevBufferStatus),
						GetBufferHealthStatusString(bufferStatus));
				prevBufferStatus = bufferStatus;
			}
			else
			{
				AAMPLOG_TRACE(" track[%s] No Change [%s]",  name,
						GetBufferHealthStatusString(bufferStatus));
			}

			pthread_mutex_unlock(&mutex);

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
					AAMPLOG_WARN("Possible deadlock with buffering. Enough buffers cached, un-pause pipeline!");
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
 * @brief Update segment cache and inject buffer to gstreamer
 */
void MediaTrack::UpdateTSAfterInject()
{
	pthread_mutex_lock(&mutex);
	AAMPLOG_TRACE("[%s] Free cachedFragment[%d] numberOfFragmentsCached %d",
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
		AAMPLOG_WARN("[%s] updated fragmentIdxToInject = %d numberOfFragmentsCached %d",
		        name, fragmentIdxToInject, numberOfFragmentsCached);
	}
#endif
	pthread_cond_signal(&fragmentInjected);
	pthread_mutex_unlock(&mutex);
}

/**
 * @brief Update segment cache and inject buffer to gstreamer
 */
void MediaTrack::UpdateTSAfterChunkInject()
{
	pthread_mutex_lock(&mutex);
	//Free Chunk Cache Buffer
	prevDownloadStartTime = cachedFragmentChunks[fragmentChunkIdxToInject].downloadStartTime;
	aamp_Free(&cachedFragmentChunks[fragmentChunkIdxToInject].fragmentChunk);
    memset(&cachedFragmentChunks[fragmentChunkIdxToInject], 0, sizeof(CachedFragmentChunk));

	aamp_Free(&parsedBufferChunk);
	memset(&parsedBufferChunk, 0x00, sizeof(GrowableBuffer));

	//increment Inject Index
	fragmentChunkIdxToInject = (++fragmentChunkIdxToInject) % maxCachedFragmentChunksPerTrack;
	if(numberOfFragmentChunksCached > 0) numberOfFragmentChunksCached--;

	AAMPLOG_TRACE("[%s] updated fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d",
			name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);

	pthread_cond_signal(&fragmentChunkInjected);
	pthread_mutex_unlock(&mutex);
}

/**
 * @brief  To be implemented by derived classes to receive cached fragment Chunk
 *         Receives cached fragment and injects to sink.
 */
void MediaTrack::InjectFragmentChunkInternal(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double fDuration)
{
	aamp->SendStreamTransfer(mediaType, buffer,
                     fpts, fdts, fDuration);

} // InjectFragmentChunkInternal

/**
 *  @brief Updates internal state after a fragment fetch
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
		AAMPLOG_WARN("[%s] before update fragmentIdxToFetch = %d numberOfFragmentsCached %d",
		        fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	totalFetchedDuration += cachedFragment[fragmentIdxToFetch].duration;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		if (numberOfFragmentsCached == 0)
		{
			AAMPLOG_WARN("## [%s] Caching fragment for track when numberOfFragmentsCached is 0 ##", name);
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
			AAMPLOG_WARN("##[%s] Caching Complete cacheDuration %d minInitialCacheSeconds %d##",
					name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
		else if (sinkBufferIsFull && numberOfFragmentsCached == maxCachedFragmentsPerTrack)
		{
			AAMPLOG_WARN("## [%s] Cache is Full cacheDuration %d minInitialCacheSeconds %d, aborting caching!##",
					name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
		else
		{
			AAMPLOG_INFO("## [%s] Caching Ongoing cacheDuration %d minInitialCacheSeconds %d##",
					name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
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
		AAMPLOG_WARN("[%s] updated fragmentIdxToFetch = %d numberOfFragmentsCached %d",
			name, fragmentIdxToFetch, numberOfFragmentsCached);
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
 *  @brief Updates internal state after a fragment fetch
 */
void MediaTrack::UpdateTSAfterChunkFetch()
{
	pthread_mutex_lock(&mutex);

	numberOfFragmentChunksCached++;

	AAMPLOG_TRACE("[%s] numberOfFragmentChunksCached++ [%d]", name,numberOfFragmentChunksCached);

	//this should never HIT
	assert(numberOfFragmentChunksCached <= maxCachedFragmentChunksPerTrack);

	fragmentChunkIdxToFetch = (fragmentChunkIdxToFetch+1) % maxCachedFragmentChunksPerTrack;

	AAMPLOG_TRACE("[%s] updated fragmentChunkIdxToFetch [%d] numberOfFragmentChunksCached [%d]",
			name, fragmentChunkIdxToFetch, numberOfFragmentChunksCached);

	totalFragmentChunksDownloaded++;
	pthread_cond_signal(&fragmentChunkFetched);
	pthread_mutex_unlock(&mutex);

	AAMPLOG_TRACE("[%s] pthread_cond_signal(fragmentChunkFetched)", name);
}

/**
 * @brief Wait until a free fragment is available.
 * @note To be called before fragment fetch by subclasses
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
			AAMPLOG_WARN("[%s] pthread_cond_timedwait returned %s", name, strerror(pthreadReturnValue));
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
				AAMPLOG_WARN("[%s] pthread_cond_timedwait returned %s", name, strerror(pthreadReturnValue));
				ret = false;
			}
		}
		else
		{
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				AAMPLOG_WARN("[%s] waiting for fragmentInjected condition", name);
			}
#endif
			pthreadReturnValue = pthread_cond_wait(&fragmentInjected, &mutex);

			if (0 != pthreadReturnValue)
			{
				AAMPLOG_WARN("[%s] pthread_cond_wait returned %s",  name, strerror(pthreadReturnValue));
				ret = false;
			}
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				AAMPLOG_WARN("[%s] wait complete for fragmentInjected", name);
			}
#endif
		}
		if(abort)
		{
#ifdef AAMP_DEBUG_FETCH_INJECT
			if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
			{
				AAMPLOG_WARN("[%s] abort set, returning false", name);
			}
#endif
			ret = false;
		}
	}
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		AAMPLOG_WARN("[%s] fragmentIdxToFetch = %d numberOfFragmentsCached %d",
			name, fragmentIdxToFetch, numberOfFragmentsCached);
	}
#endif
	pthread_mutex_unlock(&mutex);
	return ret;
}

/**
 *  @brief Wait till cached fragment available
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
			AAMPLOG_WARN("## [%s] Waiting for CachedFragment to be available, eosReached=%d ##", name, eosReached);
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
		AAMPLOG_WARN("[%s] fragmentIdxToInject = %d numberOfFragmentsCached %d",
			name, fragmentIdxToInject, numberOfFragmentsCached);
	}
#endif
	ret = !(abort || abortInject || (numberOfFragmentsCached == 0));
	pthread_mutex_unlock(&mutex);
	return ret;
}

/**
 *  @brief Wait until a cached fragment chunk is Injected.
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
				AAMPLOG_WARN("[%s] pthread_cond_timedwait returned %s", name, strerror(pthreadReturnValue));
				ret = false;
			}
		}
		else
		{
			AAMPLOG_TRACE("[%s] waiting for fragmentChunkInjected condition", name);
			pthreadReturnValue = pthread_cond_wait(&fragmentChunkInjected, &mutex);

			if (0 != pthreadReturnValue)
			{
				AAMPLOG_WARN("[%s] pthread_cond_wait returned %s", name, strerror(pthreadReturnValue));
				ret = false;
			}
			AAMPLOG_TRACE("[%s] wait complete for fragmentChunkInjected", name);
		}
	}
	if(abort || abortInjectChunk)
	{
		AAMPLOG_TRACE("[%s] abort set, returning false", name);
		ret = false;
	}

	AAMPLOG_TRACE("[%s] fragmentChunkIdxToFetch = %d numberOfFragmentChunksCached %d",
			name, fragmentChunkIdxToFetch, numberOfFragmentChunksCached);

    pthread_mutex_unlock(&mutex);
    return ret;
}

/**
 *  @brief Wait till cached fragment chunk available
 */
bool MediaTrack::WaitForCachedFragmentChunkAvailable()
{
	bool ret = true;
	pthread_mutex_lock(&mutex);

	AAMPLOG_TRACE("[%s] Acquired MUTEX ==> fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d ret = %d abort = %d abortInjectChunk = %d ", name, fragmentChunkIdxToInject, numberOfFragmentChunksCached, ret, abort, abortInjectChunk);

	if ((numberOfFragmentChunksCached == 0) && !(abort || abortInjectChunk ))
	{
		AAMPLOG_TRACE("## [%s] Waiting for CachedFragment to be available, eosReached=%d ##", name, eosReached);

		if (!eosReached)
		{
			int pthreadReturnValue = 0;
			pthreadReturnValue = pthread_cond_wait(&fragmentChunkFetched, &mutex);
			if (0 != pthreadReturnValue)
			{
				AAMPLOG_WARN("[%s] pthread_cond_wait(fragmentChunkFetched) returned %s", name, strerror(pthreadReturnValue));
			}
			AAMPLOG_TRACE("[%s] wait complete for fragmentChunkFetched", name);
		}
	}


	ret = !(abort || abortInjectChunk);

	AAMPLOG_TRACE("[%s] fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d ret = %d abort = %d abortInjectChunk = %d",
			 name, fragmentChunkIdxToInject, numberOfFragmentChunksCached, ret, abort, abortInjectChunk);

	pthread_mutex_unlock(&mutex);
	return ret;
}

/**
 *  @brief Abort the waiting for cached fragments and free fragment slot
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
			AAMPLOG_WARN("[%s] signal fragmentInjected condition", name);
		}
#endif
		pthread_cond_signal(&fragmentInjected);
		if(aamp->GetLLDashServiceData()->lowLatencyMode)
		{
			AAMPLOG_TRACE("[%s] signal fragmentChunkInjected condition", name);
			pthread_cond_signal(&fragmentChunkInjected);
		}
	}
	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		AAMPLOG_TRACE("[%s] signal fragmentChunkFetched condition", name);
		pthread_cond_signal(&fragmentChunkFetched);
	}
	pthread_cond_signal(&aamp->waitforplaystart);
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);

	GetContext()->AbortWaitForDiscontinuity();
}


/**
 * @brief Abort the waiting for cached fragments immediately
 */
void MediaTrack::AbortWaitForCachedFragment()
{
	pthread_mutex_lock(&mutex);

	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		abortInjectChunk = true;
		AAMPLOG_TRACE("[%s] signal fragmentChunkFetched condition", name);
		pthread_cond_signal(&fragmentChunkFetched);
	}

	abortInject = true;
#ifdef AAMP_DEBUG_FETCH_INJECT
	if ((1 << type) & AAMP_DEBUG_FETCH_INJECT)
	{
		AAMPLOG_WARN("[%s] signal fragmentFetched condition", name);
	}
#endif
	pthread_cond_signal(&fragmentFetched);
	pthread_mutex_unlock(&mutex);

	GetContext()->AbortWaitForDiscontinuity();
}

/**
 *  @brief Process next cached fragment chunk
 */
bool MediaTrack::ProcessFragmentChunk()
{
	//Get Cache buffer
	CachedFragmentChunk* cachedFragmentChunk = &this->cachedFragmentChunks[fragmentChunkIdxToInject];
	if(cachedFragmentChunk != NULL && NULL == cachedFragmentChunk->fragmentChunk.ptr)
	{
		AAMPLOG_TRACE("[%s] Ignore NULL Chunk - cachedFragmentChunk->fragmentChunk.len %d", name, cachedFragmentChunk->fragmentChunk.len);
		return false;
	}
	if((cachedFragmentChunk->downloadStartTime != prevDownloadStartTime) && (unparsedBufferChunk.ptr != NULL))
	{
		AAMPLOG_WARN("[%s] clean up curl chunk buffer, since  prevDownloadStartTime[%lld] != currentdownloadtime[%lld]", name,prevDownloadStartTime,cachedFragmentChunk->downloadStartTime);
		aamp_Free(&unparsedBufferChunk);
		memset(&unparsedBufferChunk,0x00,sizeof(GrowableBuffer));

	}
	size_t requiredLength = cachedFragmentChunk->fragmentChunk.len + unparsedBufferChunk.len;
	AAMPLOG_TRACE("[%s] cachedFragmentChunk->fragmentChunk.len [%d] to unparsedBufferChunk.len [%d] Required Len [%d]", name, cachedFragmentChunk->fragmentChunk.len, unparsedBufferChunk.len, requiredLength);

	//Append Cache buffer to unparsed buffer for processing
	aamp_AppendBytes(&unparsedBufferChunk, cachedFragmentChunk->fragmentChunk.ptr, cachedFragmentChunk->fragmentChunk.len);

#if 0  //enable to avoid small buffer processing
	if(cachedFragmentChunk->fragmentChunk.len < 500)
	{
		AAMPLOG_TRACE("[%s] cachedFragmentChunk->fragmentChunk.len [%d] Ignoring", name, cachedFragmentChunk->fragmentChunk.len);
		return true;
	}
#endif
	//Parse Chunk Data
	IsoBmffBuffer isobuf(mLogObj);                   /**< Fragment Chunk buffer box parser*/
	const Box *pBox = NULL;
	std::vector<Box*> *pBoxes;
	size_t mdatCount = 0;
	size_t parsedBoxCount = 0;
	char *unParsedBuffer = NULL;
	size_t parsedBufferSize = 0, unParsedBufferSize = 0;
	
	unParsedBuffer = unparsedBufferChunk.ptr;
	unParsedBufferSize = parsedBufferSize = unparsedBufferChunk.len;

	isobuf.setBuffer(reinterpret_cast<uint8_t *>(unparsedBufferChunk.ptr), unparsedBufferChunk.len);

	AAMPLOG_TRACE("[%s] Unparsed Buffer Size: %d", name,unparsedBufferChunk.len);

	bool bParse = false;
	try
	{
		bParse = isobuf.parseBuffer();
	}
	catch( std::bad_alloc& ba)
	{
		AAMPLOG_ERR("Bad allocation: %s", ba.what() );
	}
	catch( std::exception &e)
	{
		AAMPLOG_ERR("Unhandled exception: %s", e.what() );
	}
	catch( ... )
	{
		AAMPLOG_ERR("Unknown exception");
	}
	if(!bParse)
	{
		AAMPLOG_INFO("[%s] No Box available in cache chunk: fragmentChunkIdxToInject %d", name, fragmentChunkIdxToInject);
		return true;
	}
	//Print box details
	//isobuf.printBoxes();

	isobuf.getMdatBoxCount(mdatCount);
	if(!mdatCount)
	{
		 if( noMDATCount > MAX_MDAT_NOT_FOUND_COUNT )
		 {
			 AAMPLOG_INFO("[%s] noMDATCount=%d ChunkIndex=%d totchunklen=%d", name,noMDATCount, fragmentChunkIdxToInject,unParsedBufferSize);
			 noMDATCount=0;
		 }
		 noMDATCount++;
		 return true;
	}
	noMDATCount = 0;
	totalMdatCount += mdatCount;
	AAMPLOG_TRACE("[%s] MDAT count found: %d, Total Found: %d", name,  mdatCount, totalMdatCount );

	pBoxes = isobuf.getParsedBoxes();
	parsedBoxCount = pBoxes->size();

	pBox = isobuf.getChunkedfBox();
	if(pBox)
	{
		
		parsedBoxCount--;

		AAMPLOG_TRACE("[%s] MDAT Chunk Found - Actual Parsed Box Count: %d", name,parsedBoxCount);
		AAMPLOG_TRACE("[%s] Chunk Offset[%u] Chunk Type[%s] Chunk Size[%u]\n", name, pBox->getOffset(), pBox->getBoxType(), pBox->getSize());
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

				AAMPLOG_TRACE("[%s] Last MDAT Index : %d", name,lastMDatIndex);

				//Calculate unparsed buffer based on last MDAT
				unParsedBuffer += (box->getOffset()+box->getSize()); //increment buffer pointer to chunk offset
				unParsedBufferSize -= (box->getOffset()+box->getSize()); //decerese by parsed buffer size

				parsedBufferSize -= unParsedBufferSize; //get parsed buf size

				AAMPLOG_TRACE("[%s] parsedBufferSize : %d updated unParsedBufferSize: %d Total Buf Size processed: %d", name,parsedBufferSize,unParsedBufferSize,parsedBufferSize+unParsedBufferSize);

				break;
			}
		}

		uint64_t fPts = 0;
		double fpts = 0.0;
		uint64_t fDuration = 0;
		double fduration = 0.0;
		uint64_t totalChunkDuration = 0.0;

		//AAMPLOG_WARN("===========Base Media Decode Time Start================");
		//isobuf.PrintPTS();
		//AAMPLOG_WARN("============Base Media Decode Time End===============");

		for(int i=0;i<lastMDatIndex;i++)
		{
			Box *box = pBoxes->at(i);
#ifdef AAMP_DEBUG_INJECT_CHUNK
			AAMPLOG_WARN("[%s] Type: %s", name,box->getType());
#endif
			if (IS_TYPE(box->getType(), Box::MOOF))
			{
				isobuf.getSampleDuration(box, fDuration);
				totalChunkDuration += fDuration;
#ifdef AAMP_DEBUG_INJECT_CHUNK
				AAMPLOG_WARN("[%s] fDuration = %lld, totalChunkDuration = %lld", name,fDuration, totalChunkDuration);
#endif
			}
		}
		//get PTS of buffer
		bool bParse = isobuf.getFirstPTS(fPts);
		if (bParse)
		{
			AAMPLOG_TRACE("[%s] fPts %lld",name, fPts);
		}

		uint32_t timeScale = 0;
		if(type == eTRACK_VIDEO)
		{
			timeScale = aamp->GetVidTimeScale();
		}
		else if(type == eTRACK_AUDIO)
		{
			timeScale = aamp->GetAudTimeScale();
		}

		if(!timeScale)
		{
			//FIX-ME-Read from MPD INSTEAD
			timeScale = GetContext()->GetCurrPeriodTimeScale();
			if(!timeScale)
			{
				timeScale = 10000000.0;
				AAMPLOG_WARN("[%s] Empty timeScale!!! Using default timeScale=%d", name, timeScale);
			}
		}

		fpts = fPts/(timeScale*1.0);
		fduration = totalChunkDuration/(timeScale*1.0);

		//Prepeare parsed buffer
		aamp_AppendBytes(&parsedBufferChunk, unparsedBufferChunk.ptr, parsedBufferSize);
#ifdef AAMP_DEBUG_INJECT_CHUNK
		IsoBmffBuffer isobufTest(mLogObj);
		//TEST CODE for PARSED DATA COMPELTENESS
		isobufTest.setBuffer(reinterpret_cast<uint8_t *>(parsedBufferChunk.ptr), parsedBufferChunk.len);
		isobufTest.parseBuffer();
		if(isobufTest.getChunkedfBox())
		{
			AAMPLOG_WARN("[%s] CHUNK Found in parsed buffer. Something is wrong", name);
		}
		else
		{
			AAMPLOG_WARN("[%s] No CHUNK Found in parsed buffer. All Good to Send", name);
		}
		//isobufTest.printBoxes();
		isobufTest.destroyBoxes();
#endif
		AAMPLOG_INFO("Injecting chunk for %s br=%d,chunksize=%ld fpts=%f fduration=%f",name,bandwidthBitsPerSecond,parsedBufferChunk.len,fpts,fduration);
		InjectFragmentChunkInternal((MediaType)type,&parsedBufferChunk , fpts, fpts, fduration);
		totalInjectedChunksDuration += fduration;
	}

	// Move unparsed data sections to beginning
	//Available size remains same
	//This buffer should be released on Track cleanup
	if(unParsedBufferSize)
	{
		AAMPLOG_TRACE("[%s] unparsed[%p] unparsed_size[%u]", name,unParsedBuffer,unParsedBufferSize);

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
		AAMPLOG_TRACE("[%s] Set Unparsed Buffer chunk Empty...", name);
		aamp_Free(&unparsedBufferChunk);
		memset(&unparsedBufferChunk, 0x00, sizeof(GrowableBuffer));
	}
	
	aamp_Free(&parsedBufferChunk);
	memset(&parsedBufferChunk, 0x00, sizeof(GrowableBuffer));
	return true;
}

/**
 *  @brief Inject fragment Chunk into the gstreamer
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
        AAMPLOG_WARN("WaitForCachedFragmentChunkAvailable %s aborted", name);
        ret = false;
    }
    return ret;
}

/**
 *  @brief Inject fragment into the gstreamer
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
		AAMPLOG_WARN("[%s] - fragmentIdxToInject %d cachedFragment %p ptr %p",
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
					AAMPLOG_WARN("[%s] Discontinuity present. uri %s", name, cachedFragment->uri);
				}
			}
#endif
			if (type == eTRACK_SUBTITLE && cachedFragment->discontinuity)
			{
				AAMPLOG_WARN("[%s] notifying discontinuity to parser!", name);
				if (mSubtitleParser) mSubtitleParser->reset();
				stopInjection = true;
				discontinuityProcessed = true;
				ret = false;
				cachedFragment->discontinuity = false;
			}
			else if ((cachedFragment->discontinuity || ptsError) && (AAMP_NORMAL_PLAY_RATE == context->aamp->rate))
			{
				bool isDiscoIgnoredForOtherTrack = aamp->IsDiscontinuityIgnoredForOtherTrack((MediaType)!type);
				AAMPLOG_WARN("track %s - encountered aamp discontinuity @position - %f, isDiscoIgnoredForOtherTrack - %d", name, cachedFragment->position, isDiscoIgnoredForOtherTrack);
				cachedFragment->discontinuity = false;
				ptsError = false;

				/* GetESChangeStatus() check is specifically added to fix an audio loss issue (DELIA-55078) due to no reconfigure pipeline when there was an audio codec change for a very short period with no fragments.
				 * The totalInjectedDuration will be 0 for the very short duration periods if the single fragment is not injected or failed (due to fragment download failures).
				 * In that case, if there is an audio codec change is detected for this period, it could cause audio loss since ignoring the discontinuity to be processed since totalInjectedDuration is 0.
				 */
				if (totalInjectedDuration == 0 && !aamp->mpStreamAbstractionAAMP->GetESChangeStatus())
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
						aamp->UnblockWaitForDiscontinuityProcessToComplete();
					}

					AAMPLOG_WARN("ignoring %s discontinuity since no buffer pushed before!", name);
				}
				else if (isDiscoIgnoredForOtherTrack && !aamp->mpStreamAbstractionAAMP->GetESChangeStatus())
				{
					AAMPLOG_WARN("discontinuity ignored for other AV track , no need to process %s track", name);
					stopInjection = false;

					// reset the flag when both the paired discontinuities ignored.
					aamp->ResetTrackDiscontinuityIgnoredStatus();
					aamp->UnblockWaitForDiscontinuityProcessToComplete();
				}
				else
				{
					stopInjection = context->ProcessDiscontinuity(type);
				}

				if (stopInjection)
				{
					ret = false;
					discontinuityProcessed = true;
					AAMPLOG_WARN("track %s - stopping injection @position - %f", name, cachedFragment->position);
				}
				else
				{
					AAMPLOG_WARN("track %s - continuing injection", name);
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
					AAMPLOG_WARN("[%s] Inject uri %s", name, cachedFragment->uri);
				}
#endif
				if (mSubtitleParser && type == eTRACK_SUBTITLE)
				{
					mSubtitleParser->processData(cachedFragment->fragment.ptr, cachedFragment->fragment.len, cachedFragment->position, cachedFragment->duration);
				}

				if (type != eTRACK_SUBTITLE || ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
				{
					InjectFragmentInternal(cachedFragment, fragmentDiscarded);
				}
				if (eTRACK_VIDEO == type && GetContext()->GetProfileCount())
				{
					GetContext()->NotifyBitRateUpdate(cachedFragment->profileIndex, cachedFragment->cacheFragStreamInfo, cachedFragment->position);
				}
				AAMPLOG_TRACE("[%p] - %s - injected cached uri at pos %f dur %f", this, name, cachedFragment->position, cachedFragment->duration);
				if (!fragmentDiscarded)
				{
					totalInjectedDuration += cachedFragment->duration;
					mSegInjectFailCount = 0;
				}
				else
				{
					AAMPLOG_WARN("[%s] - Not updating totalInjectedDuration since fragment is Discarded", name);
					mSegInjectFailCount++;
					int  SegInjectFailCount;
					GETCONFIGVALUE(eAAMPConfig_SegmentInjectThreshold,SegInjectFailCount); 
					if(SegInjectFailCount <= mSegInjectFailCount)
					{
						ret	= false;
						AAMPLOG_ERR("[%s] Reached max inject failure count: %d, stopping playback", name, SegInjectFailCount);
						aamp->SendErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR);
					}
					
				}
				UpdateTSAfterInject();
			}
		}
		else
		{
			//EOS should not be triggerd when subtitle sets its "eosReached" in any circumstances
			if (eosReached && (eTRACK_SUBTITLE != type))
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
					AAMPLOG_WARN("GetContext is null");  //CID:81799 - Null Return
				}
			}
			else
			{
				AAMPLOG_WARN("%s - NULL ptr to inject. fragmentIdxToInject %d", name, fragmentIdxToInject);
			}
			ret = false;
		}
	}
	else
	{
		AAMPLOG_WARN("WaitForCachedFragmentAvailable %s aborted", name);
		//EOS should not be triggerd when subtitle sets its "eosReached" in any circumstances
		if (eosReached && (eTRACK_SUBTITLE != type))
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
		AAMPLOG_WARN("aamp_pthread_setname failed");
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
		AAMPLOG_WARN("aamp_pthread_setname failed");
	}
	track->RunInjectChunkLoop();
	return NULL;
}


/**
 *  @brief Start fragment injector loop
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
		AAMPLOG_WARN("Failed to create FragmentInjector thread");
	}
}

/**
 *  @brief Start fragment Chunk injector loop
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
 *  @brief Injection loop - use internally by injection logic
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
            AAMPLOG_WARN("Failed to create BufferHealthMonitor thread errno = %d, %s", errno, strerror(errno));
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
        if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback) && !aamp->IsCDVRContent() && (!aamp->mAudioOnlyPb && !aamp->mVideoOnlyPb))
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
				else if (eTRACK_AUX_AUDIO == type)
				{
					pContext->WaitForVideoTrackCatchupForAux();
				}
            }
            else
            {
                AAMPLOG_WARN("GetContext  is null");  //CID:85546 - Null Return
            }
        }
    }
    abortInject = true;
    AAMPLOG_WARN("fragment injector done. track %s", name);
}

/**
 * @brief Run fragment injector loop.
 *        Injection loop - use internally by injection logic
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
 *  @brief Stop fragment injector loop
 */
void MediaTrack::StopInjectLoop()
{
	if (fragmentInjectorThreadStarted)
	{
		int rc = pthread_join(fragmentInjectorThreadID, NULL);
		if (rc != 0)
		{
			AAMPLOG_WARN("***pthread_join fragmentInjectorThread returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			AAMPLOG_WARN("joined fragmentInjectorThread");
		}
#endif
	}
	fragmentInjectorThreadStarted = false;
}

/**
 * @brief Stop fragment chunk injector loop of track
 */
void MediaTrack::StopInjectChunkLoop()
{
	if (fragmentChunkInjectorThreadStarted)
	{
		int rc = pthread_join(fragmentChunkInjectorThreadID, NULL);
		if (rc != 0)
		{
			AAMPLOG_WARN("***pthread_join fragmentInjectorChunkThread returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			AAMPLOG_WARN("joined fragmentInjectorChunkThread");
		}
#endif
	}
	fragmentChunkInjectorThreadStarted = false;
}

/**
 *  @brief Check if a track is enabled
 */
bool MediaTrack::Enabled()
{
	return enabled;
}


/**
 *  @brief Get buffer to store the downloaded fragment content to cache next fragment
 */
CachedFragment* MediaTrack::GetFetchBuffer(bool initialize)
{
	/*Make sure fragmentDurationSeconds updated before invoking this*/
	CachedFragment* cachedFragment = &this->cachedFragment[fragmentIdxToFetch];
	if(initialize)
	{
		if (cachedFragment->fragment.ptr)
		{
			AAMPLOG_WARN("fragment.ptr already set - possible memory leak");
		}
		memset(&cachedFragment->fragment, 0x00, sizeof(GrowableBuffer));
	}
	return cachedFragment;
}

/**
 *  @brief Get buffer to fetch and cache next fragment chunk
 */
CachedFragmentChunk* MediaTrack::GetFetchChunkBuffer(bool initialize)
{
	if(fragmentChunkIdxToFetch <0 || fragmentChunkIdxToFetch >= maxCachedFragmentChunksPerTrack)
	{
		AAMPLOG_WARN("[%s] OUT OF RANGE => fragmentChunkIdxToFetch: %d",name,fragmentChunkIdxToFetch);
		return NULL;
	}

	CachedFragmentChunk* cachedFragmentChunk = NULL;
	cachedFragmentChunk = &this->cachedFragmentChunks[fragmentChunkIdxToFetch];

	AAMPLOG_TRACE("[%s] fragmentChunkIdxToFetch: %d cachedFragmentChunk: %p",name, fragmentChunkIdxToFetch, cachedFragmentChunk);

	if(initialize && cachedFragmentChunk)
	{
		if (cachedFragmentChunk->fragmentChunk.ptr)
		{
			AAMPLOG_WARN("[%s] fragmentChunk.ptr[%p] already set - possible memory leak (len=[%d],avail=[%d])",name, cachedFragmentChunk->fragmentChunk.ptr, cachedFragmentChunk->fragmentChunk.len,cachedFragmentChunk->fragmentChunk.avail);
		}
		memset(&cachedFragmentChunk->fragmentChunk, 0x00, sizeof(GrowableBuffer));
	}
	return cachedFragmentChunk;
}

/**
 *  @brief Set current bandwidth of track
 */
void MediaTrack::SetCurrentBandWidth(int bandwidthBps)
{
	this->bandwidthBitsPerSecond = bandwidthBps;
}

/**
 *  @brief Get profile index for TsbBandwidth
 */
int MediaTrack::GetProfileIndexForBW(long mTsbBandwidth)
{
       return GetContext()->GetProfileIndexForBandwidth(mTsbBandwidth);
}

/**
 *  @brief Get current bandwidth in bps
 */
int MediaTrack::GetCurrentBandWidth()
{
	return this->bandwidthBitsPerSecond;
}

/**
 *  @brief Flushes all cached fragments
 *         Flushes all media fragments and resets all relevant counters
 *                 Only intended for use on subtitle streams
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
 *  @brief Flushes all cached fragment Chunks
 */
void MediaTrack::FlushFragmentChunks()
{
	AAMPLOG_WARN("[%s]", name);

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
	pthread_mutex_lock(&mutex);
	numberOfFragmentChunksCached = 0;
	totalFragmentChunksDownloaded = 0;
	totalInjectedChunksDuration = 0;
	pthread_mutex_unlock(&mutex);
}


/**
 *  @brief MediaTrack Constructor
 */
MediaTrack::MediaTrack(AampLogManager *logObj, TrackType type, PrivateInstanceAAMP* aamp, const char* name) :
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
		noMDATCount(0), mLogObj(logObj)
		,abortPlaylistDownloader(true), playlistDownloaderThreadStarted(false), plDownloadWait()
		,plDwnldMutex(), playlistDownloaderThread(NULL), fragmentCollectorWaitingForPlaylistUpdate(false)
		, frDwnldMutex(), frDownloadWait(),prevDownloadStartTime(-1)
{
	GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached,maxCachedFragmentsPerTrack);
	cachedFragment = new CachedFragment[maxCachedFragmentsPerTrack];
	for(int X =0; X< maxCachedFragmentsPerTrack; ++X){
		memset(&cachedFragment[X], 0, sizeof(CachedFragment));
	}

	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		GETCONFIGVALUE(eAAMPConfig_MaxFragmentChunkCached,maxCachedFragmentChunksPerTrack);
		for(int X =0; X< maxCachedFragmentChunksPerTrack; ++X)
			memset(&cachedFragmentChunks[X], 0x00, sizeof(CachedFragmentChunk));

		pthread_cond_init(&fragmentChunkFetched, NULL);
		pthread_cond_init(&fragmentChunkInjected, NULL);
	}

	pthread_cond_init(&fragmentFetched, NULL);
	pthread_cond_init(&fragmentInjected, NULL);
	pthread_mutex_init(&mutex, NULL);
}


/**
 *  @brief MediaTrack Destructor
 */
MediaTrack::~MediaTrack()
{
	if (bufferMonitorThreadStarted)
	{
		int rc = pthread_join(bufferMonitorThreadID, NULL);
		if (rc != 0)
		{
			AAMPLOG_WARN("***pthread_join bufferMonitorThreadID returned %d(%s)", rc, strerror(rc));
		}
#ifdef TRACE
		else
		{
			AAMPLOG_WARN("joined bufferMonitorThreadID");
		}
#endif
	}

	if (fragmentInjectorThreadStarted)
	{
		// DELIA-45035: For debugging purpose
		AAMPLOG_WARN("In MediaTrack destructor - fragmentInjectorThreads are still running, signalling cond variable");
	}
	if (fragmentChunkInjectorThreadStarted)
	{
		// DELIA-45035: For debugging purpose
		AAMPLOG_WARN("In MediaTrack destructor - fragmentChunkInjectorThreads are still running, signalling cond variable");
	}

	if(aamp->GetLLDashServiceData()->lowLatencyMode)
	{
		AAMPLOG_INFO("LL-Mode flushing chunks");
		FlushFragmentChunks();
		pthread_cond_destroy(&fragmentChunkFetched);
		pthread_cond_destroy(&fragmentChunkInjected);
	}
    
	for (int j = 0; j < maxCachedFragmentsPerTrack; j++)
	{
		aamp_Free(&cachedFragment[j].fragment);
		memset(&cachedFragment[j], 0x00, sizeof(CachedFragment));
	}

	SAFE_DELETE_ARRAY(cachedFragment);
	
	pthread_cond_destroy(&fragmentFetched);
	pthread_cond_destroy(&fragmentInjected);
	pthread_mutex_destroy(&mutex);
}

/**
 *  @brief Unblock track if caught up with video or downloads are stopped
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
			AAMPLOG_WARN("signalling cond - audioDuration %f videoDuration %f",
				audioDuration, videoDuration);
#endif
		}
		if (aux && aux->enabled)
		{
			double auxDuration = aux->GetTotalInjectedDuration();
			if (auxDuration < (videoDuration + (2 * video->fragmentDurationSeconds)) || !aamp->DownloadsAreEnabled() || video->IsDiscontinuityProcessed() || abort || video->IsAtEndOfTrack())
			{
				pthread_cond_signal(&mAuxCond);
#ifdef AAMP_DEBUG_FETCH_INJECT
				AAMPLOG_WARN("signalling cond - auxDuration %f videoDuration %f",
					auxDuration, videoDuration);
#endif
			}
		}
		pthread_mutex_unlock(&mLock);
	}
}


/**
 * @brief Blocks aux track injection until caught up with video track.
 *        Used internally by injection logic
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
			AAMPLOG_WARN("waiting for cond - audioDuration %f videoDuration %f video->fragmentDurationSeconds %f",
				audioDuration, videoDuration,video->fragmentDurationSeconds);
	#endif
			ts = aamp_GetTimespec(100);

			ret = pthread_cond_timedwait(&mCond, &mLock, &ts);

			if (ret == 0)
			{
				break;
			}
			if (ret != ETIMEDOUT)
			{
				AAMPLOG_WARN("error while calling pthread_cond_timedwait - %s", strerror(ret));
			}
		}
	}
	else
	{
		AAMPLOG_WARN("video  is null");  //CID:85054 - Null Returns
	}
	pthread_mutex_unlock(&mLock);
}


/**
 * @brief StreamAbstractionAAMP constructor.
 */
StreamAbstractionAAMP::StreamAbstractionAAMP(AampLogManager *logObj, PrivateInstanceAAMP* aamp):
		trickplayMode(false), currentProfileIndex(0), mCurrentBandwidth(0),currentAudioProfileIndex(-1),currentTextTrackProfileIndex(-1),
		mTsbBandwidth(0),mNwConsistencyBypass(true), profileIdxForBandwidthNotification(0),
		hasDrm(false), mIsAtLivePoint(false), mESChangeStatus(false),mAudiostateChangeCount(0),
		mNetworkDownDetected(false), mTotalPausedDurationMS(0), mIsPaused(false), mProgramStartTime(-1),
		mStartTimeStamp(-1),mLastPausedTimeStamp(-1), aamp(aamp),
		mIsPlaybackStalled(false), mCheckForRampdown(false), mTuneType(), mLock(),
		mCond(), mLastVideoFragCheckedforABR(0), mLastVideoFragParsedTimeMS(0),
		mSubCond(), mAudioTracks(), mTextTracks(),mABRHighBufferCounter(0),mABRLowBufferCounter(0),mMaxBufferCountCheck(0),
		mStateLock(), mStateCond(), mTrackState(eDISCONTIUITY_FREE),
		mRampDownLimit(-1), mRampDownCount(0),mABRMaxBuffer(0), mABRCacheLength(0), mABRMinBuffer(0), mABRNwConsistency(0),
		mBitrateReason(eAAMP_BITRATE_CHANGE_BY_TUNE),
		mAudioTrackIndex(), mTextTrackIndex(),
		mAuxCond(), mFwdAudioToAux(false), mLogObj(logObj)
		, mAudioTracksAll()
		, mTextTracksAll(),
		mTsbMaxBitrateProfileIndex(-1)
{
	mLastVideoFragParsedTimeMS = aamp_GetCurrentTimeMS();
	AAMPLOG_TRACE("StreamAbstractionAAMP");
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
	aamp->mhAbrManager.setDefaultInitBitrate(aamp->GetDefaultBitrate());


	long ibitrate = aamp->GetIframeBitrate();
	if (ibitrate > 0)
	{
		aamp->mhAbrManager.setDefaultIframeBitrate(ibitrate);
	}
	GETCONFIGVALUE(eAAMPConfig_RampDownLimit,mRampDownLimit); 
	if (!aamp->IsNewTune())
	{
		mBitrateReason = (aamp->rate != AAMP_NORMAL_PLAY_RATE) ? eAAMP_BITRATE_CHANGE_BY_TRICKPLAY : eAAMP_BITRATE_CHANGE_BY_SEEK;
	}
}


/**
 *  @brief StreamAbstractionAAMP destructor.
 */
StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
	AAMPLOG_TRACE("StreamAbstractionAAMP");
	pthread_cond_destroy(&mCond);
	pthread_cond_destroy(&mSubCond);
	pthread_cond_destroy(&mAuxCond);
	pthread_mutex_destroy(&mLock);

	pthread_cond_destroy(&mStateCond);
	pthread_mutex_destroy(&mStateLock);
	AAMPLOG_INFO("Exit StreamAbstractionAAMP");
}

/**
 *  @brief Get the last video fragment parsed time.
 */
double StreamAbstractionAAMP::LastVideoFragParsedTimeMS(void)
{
	return mLastVideoFragParsedTimeMS;
}

/**
 *  @brief Get the desired profile to start fetching.
 */
int StreamAbstractionAAMP::GetDesiredProfile(bool getMidProfile)
{
	int desiredProfileIndex = 0;
	if(GetProfileCount())
	{
		if (this->trickplayMode && ABRManager::INVALID_PROFILE != aamp->mhAbrManager.getLowestIframeProfile())
		{
			desiredProfileIndex = GetIframeTrack();
		}
		else
		{
			desiredProfileIndex = aamp->mhAbrManager.getInitialProfileIndex(getMidProfile);
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
				AAMPLOG_WARN("GetStreamInfo is null");  //CID:81678 - Null Returns
			}
		}
		else
		{
			AAMPLOG_TRACE("video track NULL");
		}
		AAMPLOG_TRACE("profileIdxForBandwidthNotification updated to %d ", profileIdxForBandwidthNotification);
	}
	return desiredProfileIndex;
}

/**
 *   @brief Get profile index of highest bandwidth
 *
 *   @return Profile highest BW profile index
 */
int StreamAbstractionAAMP::GetMaxBWProfile()
{
	int ret = 0;
	if(aamp->IsTSBSupported() && mTsbMaxBitrateProfileIndex >= 0)
	{
		ret = mTsbMaxBitrateProfileIndex;
	}
	else
	{
		ret =  aamp->mhAbrManager.getMaxBandwidthProfile();
	}
	return ret;
}

/**
 *   @brief Notify bitrate updates to application.
 *          Used internally by injection logic
 */
void StreamAbstractionAAMP::NotifyBitRateUpdate(int profileIndex, const StreamInfo &cacheFragStreamInfo, double position)
{
	if (profileIndex != aamp->GetPersistedProfileIndex() && cacheFragStreamInfo.bandwidthBitsPerSecond != 0)
	{
		//AAMPLOG_WARN("stream Info bps(%ld) w(%d) h(%d) fr(%f)", cacheFragStreamInfo.bandwidthBitsPerSecond, cacheFragStreamInfo.resolution.width, cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate);

		StreamInfo* streamInfo = GetStreamInfo(GetMaxBWProfile());
		if(streamInfo != NULL)
		{
			bool lGetBWIndex = false;
			/* START: Added As Part of DELIA-28363 and DELIA-28247 */
			if(aamp->IsTuneTypeNew && (cacheFragStreamInfo.bandwidthBitsPerSecond == streamInfo->bandwidthBitsPerSecond))
			{
				MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
				AAMPLOG_WARN("NotifyBitRateUpdate: Max BitRate: %ld, timetotop: %f", cacheFragStreamInfo.bandwidthBitsPerSecond, video->GetTotalInjectedDuration());
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
			AAMPLOG_WARN("StreamInfo  is null");  //CID:82200 - Null Returns
		}
	}
}

/**
 *  @brief Check if Initial Fragment Caching is supported
 */
bool StreamAbstractionAAMP::IsInitialCachingSupported()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	return (video && video->enabled);
}

/**
 *  @brief Function to update stream info of current fetched fragment
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
		//AAMPLOG_WARN("stream Info bps(%ld) w(%d) h(%d) fr(%f)", cacheFragStreamInfo.bandwidthBitsPerSecond, cacheFragStreamInfo.resolution.width, cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate);
	}
}


/**
 *  @brief Update profile state based on bandwidth of fragments downloaded.
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
				AAMPLOG_WARN("GetStreamInfo is null");  //CID:84179 - Null Returns
			}
		}
	}
}

/**
 *  @brief Update rampdown profile on network failure
 */
void StreamAbstractionAAMP::UpdateRampdownProfileReason(void)
{
	mBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;
}

/**
 *  @brief Get Desired Profile based on Buffer availability
 */
void StreamAbstractionAAMP::GetDesiredProfileOnBuffer(int currProfileIndex, int &newProfileIndex)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);

	double bufferValue = video->GetBufferedDuration();
	double minBufferNeeded = video->fragmentDurationSeconds + aamp->mNetworkTimeoutMs/1000;
	aamp->mhAbrManager.GetDesiredProfileOnBuffer(currProfileIndex,newProfileIndex,bufferValue,minBufferNeeded);
}

/**
 *  @brief Get Desired Profile on steady state
 */
void StreamAbstractionAAMP::GetDesiredProfileOnSteadyState(int currProfileIndex, int &newProfileIndex, long nwBandwidth)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double bufferValue = video->GetBufferedDuration();
	if(bufferValue > 0 && currProfileIndex == newProfileIndex)
	{
		AAMPLOG_INFO("buffer:%f currProf:%d nwBW:%ld",bufferValue,currProfileIndex,nwBandwidth);
		if(bufferValue > mABRMaxBuffer)
		{
			mABRHighBufferCounter++;
			mABRLowBufferCounter = 0 ;
			if(mABRHighBufferCounter > mMaxBufferCountCheck)
			{
				int nProfileIdx =  aamp->mhAbrManager.getRampedUpProfileIndex(currProfileIndex);
				long newBandwidth = GetStreamInfo(nProfileIdx)->bandwidthBitsPerSecond;
				HybridABRManager::BitrateChangeReason mhBitrateReason;
				mhBitrateReason = (HybridABRManager::BitrateChangeReason) mBitrateReason;
				aamp->mhAbrManager.CheckRampupFromSteadyState(currProfileIndex,newProfileIndex,nwBandwidth,bufferValue,newBandwidth,mhBitrateReason,mMaxBufferCountCheck);
				mBitrateReason = (BitrateChangeReason) mhBitrateReason;
				mABRHighBufferCounter = 0;
			}
		}
		// steady state ,with no ABR cache available to determine actual bandwidth
		// this state can happen due to timeouts
		if(nwBandwidth == -1 && bufferValue < mABRMinBuffer && !video->IsInjectionAborted())
		{
			mABRLowBufferCounter++;
			mABRHighBufferCounter = 0;
			
				HybridABRManager::BitrateChangeReason mhBitrateReason;
				mhBitrateReason = (HybridABRManager::BitrateChangeReason) mBitrateReason;
				aamp->mhAbrManager.CheckRampdownFromSteadyState(currProfileIndex,newProfileIndex,mhBitrateReason,mABRLowBufferCounter);
				mBitrateReason = (BitrateChangeReason) mhBitrateReason;
				mABRLowBufferCounter = (mABRLowBufferCounter > mABRCacheLength)? 0 : mABRLowBufferCounter ;
		}
	}
	else
	{
		mABRLowBufferCounter = 0 ;
		mABRHighBufferCounter = 0;
	}
}

/**
 *  @brief Configure download timeouts based on buffer
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
 *  @brief Get desired profile based on cached duration
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
			desiredProfileIndex = aamp->mhAbrManager.getProfileIndexByBitrateRampUpOrDown(currentProfileIndex,
					currentBandwidth, networkBandwidth, nwConsistencyCnt);

			AAMPLOG_INFO("currBW:%ld NwBW=%ld currProf:%d desiredProf:%d",currentBandwidth,networkBandwidth,currentProfileIndex,desiredProfileIndex);
			if (currentProfileIndex != desiredProfileIndex)
			{
				// There is a chance that desiredProfileIndex is reset in below GetDesiredProfileOnBuffer call
				// Since bitrate notification will not be triggered in this case, its fine
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_ABR;
			}
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
		AAMPLOG_WARN("video is null");  //CID:84160 - Null Returns
	}
	return desiredProfileIndex;
}


/**
 *  @brief Rampdown profile
 */
bool StreamAbstractionAAMP::RampDownProfile(long http_error)
{
	bool ret = false;
	int desiredProfileIndex = currentProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if (this->trickplayMode)
	{
		//We use only second last and lowest profiles for iframes
		int lowestIframeProfile = aamp->mhAbrManager.getLowestIframeProfile();
		if (desiredProfileIndex != lowestIframeProfile)
		{
			if (ABRManager::INVALID_PROFILE != lowestIframeProfile)
			{
				desiredProfileIndex = lowestIframeProfile;
			}
			else
			{
				AAMPLOG_WARN("lowestIframeProfile Invalid - Stream does not has an iframe track!! ");
			}
		}
	}
	else
	{
		desiredProfileIndex = aamp->mhAbrManager.getRampedDownProfileIndex(currentProfileIndex);
	}
	if (desiredProfileIndex != currentProfileIndex)
	{
		AAMPAbrInfo stAbrInfo = {};

		stAbrInfo.abrCalledFor = AAMPAbrFragmentDownloadFailed;
		stAbrInfo.currentProfileIndex = currentProfileIndex;
		stAbrInfo.desiredProfileIndex = desiredProfileIndex;
		StreamInfo* streamInfodesired = GetStreamInfo(desiredProfileIndex);
		StreamInfo* streamInfocurrent = GetStreamInfo(currentProfileIndex);
		if((streamInfocurrent != NULL) && (streamInfodesired != NULL))   //CID:160715 - Forward null
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
			AAMPLOG_TRACE(" profileIdxForBandwidthNotification updated to %d ",  profileIdxForBandwidthNotification);
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
			AAMPLOG_WARN("GetStreamInfo is null");  //CID:84132 - Null Returns
		}
	}

	return ret;
}

/**
 *  @brief Check whether the current profile is lowest.
 */
bool StreamAbstractionAAMP::IsLowestProfile(int currentProfileIndex)
{
	bool ret = false;

	if (trickplayMode)
	{
		if (currentProfileIndex == aamp->mhAbrManager.getLowestIframeProfile())
		{
			ret = true;
		}
	}
	else
	{
		ret = aamp->mhAbrManager.isProfileIndexBitrateLowest(currentProfileIndex);
	}

	return ret;
}

/**
 *  @brief Convert custom curl errors to original
 */
long StreamAbstractionAAMP::getOriginalCurlError(long http_error)
{
	long ret = http_error;

	if (http_error >= PARTIAL_FILE_CONNECTIVITY_AAMP && http_error <= PARTIAL_FILE_START_STALL_TIMEOUT_AAMP)
	{
		if (http_error == OPERATION_TIMEOUT_CONNECTIVITY_AAMP)
		{
			ret = CURLE_OPERATION_TIMEDOUT;
		}
		else
		{
			ret = CURLE_PARTIAL_FILE;
		}
	}

	// return original error code
	return ret;
}


/**
 *  @brief Check for ramdown profile.
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
		http_error = getOriginalCurlError(http_error);

		if (http_error == 404 || http_error == 502 || http_error == 500 || http_error == 503 || http_error == CURLE_PARTIAL_FILE)
		{
			if (RampDownProfile(http_error))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP: Condition Rampdown Success");
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
 *  @brief Checks and update profile based on bandwidth.
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
			long availBW = aamp->GetCurrentlyAvailableBandwidth();
			bool checkProfileChange = aamp->mhAbrManager.CheckProfileChange(totalFetchedDuration,currentProfileIndex,availBW);
		
			if (checkProfileChange)
			{
				UpdateProfileBasedOnFragmentCache();
			}
		}
		else
		{
			AAMPLOG_WARN("Video is null");  //CID:82070 - Null Returns
		}
	}
}

/**
 *  @brief Get iframe track index.
 *         This shall be called only after UpdateIframeTracks() is done
 */
int StreamAbstractionAAMP::GetIframeTrack()
{
	return aamp->mhAbrManager.getDesiredIframeProfile();
}

/**
 *   @brief Update iframe tracks.
 *          Subclasses shall invoke this after StreamInfo is populated .
 */
void StreamAbstractionAAMP::UpdateIframeTracks()
{
	aamp->mhAbrManager.updateProfile();
}


/**
 *  @brief Function called when playback is paused to update related flags.
 */
void StreamAbstractionAAMP::NotifyPlaybackPaused(bool paused)
{
	pthread_mutex_lock(&mLock);
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
			AAMPLOG_WARN("StreamAbstractionAAMP: mLastPausedTimeStamp -1");
		}
	}
	pthread_mutex_unlock(&mLock);   //CID:136243 - Missing_lock
}


/**
 *  @brief Check if player caches are running dry.
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
		AAMPLOG_WARN("StreamAbstractionAAMP: Stall detected. Buffer status is RED!");
		return true;
	}
	return false;
}

/**
 *  @brief Update profile based on fragment cache.
 */
bool StreamAbstractionAAMP::UpdateProfileBasedOnFragmentCache()
{
	bool retVal = false;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	int desiredProfileIndex = GetDesiredProfileBasedOnCache();
	if (desiredProfileIndex != currentProfileIndex)
	{
#if 0 /* Commented since the same is supported via AAMP_LOG_ABR_INFO */
		AAMPLOG_WARN("**aamp changing profile: %d->%d [%ld->%ld]",
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
		AAMPLOG_TRACE(" profileIdxForBandwidthNotification updated to %d ",  profileIdxForBandwidthNotification);
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
 *  @brief Check if playback has stalled and update related flags.
 */
void StreamAbstractionAAMP::CheckForPlaybackStall(bool fragmentParsed)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		return;
	}
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
				AAMPLOG_INFO("StreamAbstractionAAMP: Didn't download a new fragment for a long time(%f) and cache empty!", timeElapsedSinceLastFragment);
				mIsPlaybackStalled = true;
				if (CheckIfPlayerRunningDry())
				{
					AAMPLOG_WARN("StreamAbstractionAAMP: Stall detected!. Time elapsed since fragment parsed(%f), caches are all empty!", timeElapsedSinceLastFragment);
					aamp->SendStalledErrorEvent();
				}
			}
		}
		else
		{
			AAMPLOG_WARN("GetMediaTrack  is null");  //CID:85383 - Null Returns
		}
	}
}


/**
 *  @brief MediaTracks shall call this to notify first fragment is injected.
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
 *  @brief Get elapsed time of play-back.
 */
double StreamAbstractionAAMP::GetElapsedTime()
{
	double elapsedTime;
	pthread_mutex_lock(&mLock);
	AAMPLOG_TRACE("StreamAbstractionAAMP:mStartTimeStamp %lld mTotalPausedDurationMS %lld mLastPausedTimeStamp %lld", mStartTimeStamp, mTotalPausedDurationMS, mLastPausedTimeStamp);
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
 *  @brief Get the bitrate of current video profile selected.
 */
long StreamAbstractionAAMP::GetVideoBitrate(void)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	return ((video && video->enabled) ? (video->GetCurrentBandWidth()) : 0);
}

/**
 *  @brief Get the bitrate of current audio profile selected.
 */
long StreamAbstractionAAMP::GetAudioBitrate(void)
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	return ((audio && audio->enabled) ? (audio->GetCurrentBandWidth()) : 0);
}


/**
 *  @brief Check if current stream is muxed
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
 *   @brief Set AudioTrack info from Muxed stream
 *
 *   @param[in] vector AudioTrack info
 *   @return void
 */
void StreamAbstractionAAMP::SetAudioTrackInfoFromMuxedStream(std::vector<AudioTrackInfo>& vector)
{
	for(auto iter = mAudioTracks.begin(); iter != mAudioTracks.end();)
	{
		if(iter->characteristics == "muxed-audio")
		{
			/*
			 *  Remove old entries. Fresh elementery streams found in new PAT/PMT for TS stream
			 */
			iter = mAudioTracks.erase(iter);
		}
		else
		{
			iter++;
		}
	}
	for(auto iter : vector)
	{
		char* currentId = const_cast<char*>(iter.rendition.c_str());
		char* currentLanguage = const_cast<char*>(iter.language.c_str());
		auto language = std::find_if(mAudioTracks.begin(), mAudioTracks.end(),
                                                                [currentId, currentLanguage](AudioTrackInfo& temp)
                                                                { return ((temp.language == currentLanguage) && (temp.rendition == currentId)); });
		if(language != mAudioTracks.end())
		{
			/*
			 * Store proper codec, characteristics and index
			 */
			language->characteristics = iter.characteristics;
			language->codec = iter.codec;
			language->index = iter.index;
		}
		else
		{
			mAudioTracks.push_back(iter);
		}
	}
	if(vector.size() > 0)
	{
		/*
		 *  Notify the audio track change
		 */
		aamp->NotifyAudioTracksChanged();
	}
}


/**
 *   @brief Waits subtitle track injection until caught up with muxed/audio track.
 *          Used internally by injection logic
 */
void StreamAbstractionAAMP::WaitForAudioTrackCatchup()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if(subtitle == NULL)
	{
		AAMPLOG_WARN("subtitle is null");
		return;
	}
	//Check if its muxed a/v
	if (audio && !audio->enabled)
	{
		audio = GetMediaTrack(eTRACK_VIDEO);
	}

	struct timespec ts;
	int ret = 0;

	if(audio == NULL)
	{
		AAMPLOG_WARN("audio  is null");
                return;
	}
	pthread_mutex_lock(&mLock);
	double audioDuration = audio->GetTotalInjectedDuration();
	double subtitleDuration = subtitle->GetTotalInjectedDuration();
	//Allow subtitles to be ahead by 5 seconds compared to audio
	while ((subtitleDuration > (audioDuration + audio->fragmentDurationSeconds + 15.0)) && aamp->DownloadsAreEnabled() && !subtitle->IsDiscontinuityProcessed() && !audio->IsInjectionAborted())
	{
		AAMPLOG_TRACE("Blocked on Inside mSubCond with sub:%f and audio:%f", subtitleDuration, audioDuration);
	#ifdef AAMP_DEBUG_FETCH_INJECT
		AAMPLOG_WARN("waiting for mSubCond - subtitleDuration %f audioDuration %f",
			subtitleDuration, audioDuration);
	#endif
		ts = aamp_GetTimespec(100);

		ret = pthread_cond_timedwait(&mSubCond, &mLock, &ts);

		if (ret == 0)
		{
			break;
		}
		if (ret != ETIMEDOUT)
		{
			AAMPLOG_WARN("error while calling pthread_cond_timedwait - %s", strerror(ret));
		}
		audioDuration = audio->GetTotalInjectedDuration();
	}
	pthread_mutex_unlock(&mLock);
}

/**
 *  @brief Unblock subtitle track injector if downloads are stopped
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
			AAMPLOG_WARN("signalling mSubCond");
#endif
		}
		pthread_mutex_unlock(&mLock);
	}
}

/**
 *  @brief Send a MUTE/UNMUTE packet to the subtitle renderer
 */
void StreamAbstractionAAMP::MuteSubtitles(bool mute)
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		subtitle->mSubtitleParser->mute(mute);
	}
}

/**
 *  @brief Checks if streamer reached end of stream
 */
bool StreamAbstractionAAMP::IsEOSReached()
{
	bool eos = true;
	for (int i = 0 ; i < AAMP_TRACK_COUNT; i++)
	{
	    // For determining EOS we will Ignore the subtitle track
	    if ((TrackType)i == eTRACK_SUBTITLE)
	        continue;

		MediaTrack *track = GetMediaTrack((TrackType) i);
		if (track && track->enabled)
		{
			eos = eos && track->IsAtEndOfTrack();
			if (!eos)
			{
				AAMPLOG_WARN("EOS not seen by track: %s, skip check for rest of the tracks", track->name);
				aamp->ResetEOSSignalledFlag();
				break;
			}
		}
	}
	return eos;
}

/**
 *  @brief Function to returns last injected fragment position
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
	AAMPLOG_INFO("Last Injected fragment Position : %f", pos);
	return pos;
}

/**
 *  @brief To check for discontinuity in future fragments.
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
				AAMPLOG_WARN("Found discontinuity for track %s at index: %d and position - %f", name, start, cachedFragment[start].position);
			}
		}
		cachedDuration += cachedFragment[start].duration;
		if (++start == maxCachedFragmentsPerTrack)
		{
			start = 0;
		}
		count--;
	}
	AAMPLOG_WARN("track %s numberOfFragmentsCached - %d, cachedDuration - %f", name, numberOfFragmentsCached, cachedDuration);
	pthread_mutex_unlock(&mutex);

	return ret;
}

/**
 *  @brief Called if sink buffer is full
 */
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
		AAMPLOG_WARN("## [%s] Cache is Full cacheDuration %d minInitialCacheSeconds %d, aborting caching!##",
							name, currentInitialCacheDurationSeconds, aamp->GetInitialBufferDuration());
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
 *  @brief Function to process discontinuity.
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
				AAMPLOG_WARN("muxed track audio discontinuity/EOS processing ignored!");
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

		AAMPLOG_WARN("mTrackState:%d!", mTrackState);

		if (mTrackState == state)
		{
			wait = true;
			AAMPLOG_WARN("track[%d] Going into wait for processing discontinuity in other track!", type);
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
				AAMPLOG_WARN("track:%d discontinuity processing ignored! reset mTrackState to: %d!", type, mTrackState);
			}
			else if (isMuxedAndAudioDiscoIgnored && type == eTRACK_VIDEO)
			{
				// In muxed stream, set the audio track's mProcessingDiscontinuity flag to true to unblock the ProcessPendingDiscontinuity if video track discontinuity-EOS processing succeeded
				AAMPLOG_WARN("set muxed track audio discontinuity flag to true since video discontinuity processing succeeded.");
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
 *  @brief Function to abort any wait for discontinuity by injector theads.
 */
void StreamAbstractionAAMP::AbortWaitForDiscontinuity()
{
	//Release injector thread blocked in ProcessDiscontinuity
	pthread_mutex_lock(&mStateLock);
	pthread_cond_signal(&mStateCond);
	pthread_mutex_unlock(&mStateLock);
}

/**
 *  @brief Function to check if any media tracks are stalled on discontinuity.
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
				AAMPLOG_WARN("Discontinuity encountered in track:%d with injectedDuration:%f and other track injectedDuration:%f, fragmentDurationSeconds:%f, diff:%f",
								type, duration, otherTrackDuration, track->fragmentDurationSeconds, diff);
				if (otherTrackDuration >= duration)
				{
					//Check for future discontinuity
					isDiscontinuityPresent = otherTrack->CheckForFutureDiscontinuity(cachedDuration);
					if (isDiscontinuityPresent)
					{
						//Scenario - video wait on discontinuity, and audio has a future discontinuity
						if (type == eTRACK_VIDEO)
						{
							AAMPLOG_WARN("For discontinuity in track:%d, other track has injectedDuration:%f and future discontinuity, signal mCond var!",
									type, otherTrackDuration);
							pthread_mutex_lock(&mLock);
							pthread_cond_signal(&mCond);
							pthread_mutex_unlock(&mLock);
						}
					}
					// If discontinuity is not seen in future fragments or if the unblocked track has finished more than 2 * fragmentDurationSeconds,
					// unblock this track
					else if (((diff + cachedDuration) > (2 * track->fragmentDurationSeconds)))
					{
						AAMPLOG_WARN("Discontinuity in track:%d does not have a discontinuity in other track (diff: %f, injectedDuration: %f, cachedDuration: %f)",
								type, diff, otherTrackDuration, cachedDuration);
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
					AAMPLOG_WARN("Discontinuity in track:%d does not have a discontinuity in other track (diff is negative: %f, injectedDuration: %f)",
							type, diff, otherTrackDuration);
					isDiscontinuityPresent = otherTrack->CheckForFutureDiscontinuity(cachedDuration); // called just to get the value of cachedDuration of the track.
					bProcessFlag = true;
				}

				if (bProcessFlag)
				{
					if (ISCONFIGSET(eAAMPConfig_RetuneForUnpairDiscontinuity) && type != eTRACK_AUDIO)
					{
						if(aamp->GetBufUnderFlowStatus())
						{
							AAMPLOG_WARN("Schedule retune since for discontinuity in track:%d other track doesn't have a discontinuity (diff: %f, injectedDuration: %f, cachedDuration: %f)",
									type, diff, otherTrackDuration, cachedDuration);
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
						AAMPLOG_WARN("Ignoring discontinuity in track:%d since other track doesn't have a discontinuity (diff: %f, injectedDuration: %f, cachedDuration: %f)",
								type, diff, otherTrackDuration, cachedDuration);
						// This is a logic to handle special case identified with Sky CDVR SCTE embedded streams.
						// During the period transition, the audio track detected the discontinuity, but the Player didn’t detect discontinuity for the video track within the expected time frame.
						// So the Audio track is exiting from the discontinuity process due to singular discontinuity condition,
						// but after that, the video track encountered discontinuity and ended in a deadlock due to no more discontinuity in audio to match with it.
						if(aamp->IsDashAsset())
						{
							AAMPLOG_WARN("Ignoring discontinuity in DASH period for track:%d",type);
							aamp->SetTrackDiscontinuityIgnoredStatus((MediaType)type);
						}
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
 *  @brief Check for ramp down limit reached by player
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
		AAMPLOG_WARN("Rampdown limit reached, Limit is %d", mRampDownLimit);
	}
	return ret;
}

/**
 *  @brief Get buffered video duration in seconds
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
 *  @brief Get current audio track information
 */
bool StreamAbstractionAAMP::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
	int index = -1;
	bool bFound = false;
	if (!mAudioTrackIndex.empty())
	{
		for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
		{
			if (it->index == mAudioTrackIndex)
			{
				audioTrack = *it;
				bFound = true;
			}
		}
	}
	return bFound;
}


/**
 *   @brief Get current text track
 */
bool StreamAbstractionAAMP::GetCurrentTextTrack(TextTrackInfo &textTrack)
{
	int index = -1;
	bool bFound = false;
	if (!mTextTrackIndex.empty())
	{
		for (auto it = mTextTracks.begin(); it != mTextTracks.end(); it++)
		{
			if (it->index == mTextTrackIndex)
			{
				textTrack = *it;
				bFound = true;
			}
		}
	}
	return bFound;
}

/**
 *   @brief Get current audio track
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
 *  @brief Get current text track
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
 *  @brief Refresh subtitle track
 */
void StreamAbstractionAAMP::RefreshSubtitles()
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_WARN("Setting refreshSubtitles");
		subtitle->refreshSubtitles = true;
		subtitle->AbortWaitForCachedAndFreeFragment(true);
	}
}


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
			AAMPLOG_WARN("waiting for cond - auxDuration %f videoDuration %f video->fragmentDurationSeconds %f",
				auxDuration, videoDuration, video->fragmentDurationSeconds);
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
				AAMPLOG_WARN("error while calling pthread_cond_timedwait - %s", strerror(ret));
			}
	#endif
		}
	}
	else
	{
		AAMPLOG_WARN("video  is null");  //CID:85054 - Null Returns
	}
	pthread_mutex_unlock(&mLock);
}

/**
 * @brief Returns playlist type of track
 */
MediaType MediaTrack::GetPlaylistMediaTypeFromTrack(TrackType type, bool isIframe)
{
		MediaType playlistType = eMEDIATYPE_MANIFEST;
		// For DASH, return playlist type as manifest
		if(eMEDIAFORMAT_DASH != aamp->mMediaFormat)
		{
			if(isIframe)
			{
				playlistType = eMEDIATYPE_PLAYLIST_IFRAME;
			}
			else if (type == eTRACK_AUDIO )
			{
				playlistType = eMEDIATYPE_PLAYLIST_AUDIO;
			}
			else if (type == eTRACK_SUBTITLE)
			{
				playlistType = eMEDIATYPE_PLAYLIST_SUBTITLE;
			}
			else if (type == eTRACK_AUX_AUDIO)
			{
				playlistType = eMEDIATYPE_PLAYLIST_AUX_AUDIO;
			}
			else if (type == eTRACK_VIDEO)
			{
				playlistType = eMEDIATYPE_PLAYLIST_VIDEO;
			}
		}
		return playlistType;
}


/**
 * @brief Notify playlist downloader threads of tracks
 */
void StreamAbstractionAAMP::DisablePlaylistDownloads()
{
	for (int i = 0 ; i < AAMP_TRACK_COUNT; i++)
	{
		MediaTrack *track = GetMediaTrack((TrackType) i);
		if (track && track->enabled)
		{
			track->AbortWaitForPlaylistDownload();
			track->AbortFragmentDownloaderWait();
		}
	}
}

/**
 * @fn GetPreferredLiveOffsetFromConfig
 * @brief check if current stream have 4K content
 * @retval true on success
 */
bool StreamAbstractionAAMP::GetPreferredLiveOffsetFromConfig()
{
	bool stream4K = false;
	do
	{
		int height = 0;
		long bandwidth = 0;

		/** Update Live Offset with default or configured liveOffset*/
		aamp->UpdateLiveOffset();

		/**< 1. Is it CDVR or iVOD? not required live Offset correction*/
		if(!aamp->IsLiveAdjustRequired())
		{
			/** 4K live offset not applicable for CDVR/IVOD */
			stream4K = false;
			break;
		}

		/**< 2. Check whether it is 4K stream or not*/
		stream4K =  Is4KStream(height, bandwidth);
		if (!stream4K)
		{
			/**Not a 4K */
			break;
		}

		/**< 3. 4K disabled by user? **/
		if (ISCONFIGSET(eAAMPConfig_Disable4K) )
		{
			AAMPLOG_WARN("4K playback disabled by User!!");
			break;
		}

		/**< 4. maxbitrate should be less than 4K bitrate? */
		long maxBitrate = aamp->GetMaximumBitrate();
		if (bandwidth > maxBitrate)
		{
			AAMPLOG_WARN("Maxbitrate (%ld) set by user is less than 4K bitrate (%ld);", maxBitrate, bandwidth);
			stream4K = false;
			break;
		}

		/**< 5. If display resolution check enabled and resolution available then it should be grater than 4K profile */
		if (ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mDisplayHeight == 0)
		{
			AAMPLOG_WARN("Ignoring display resolution check due to invalid display height 0");
		}
		else
		{
			if (ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mDisplayHeight < height  )
			{
				AAMPLOG_WARN("Display resolution (%d) doesn't support the 4K resolution (%d)", aamp->mDisplayHeight, height);
				stream4K = false;
				break;
			}
		}

		/** 4K stream and 4K support is found ; Use 4K live offset if provided*/
		if (GETCONFIGOWNER(eAAMPConfig_LiveOffset4K) > AAMP_DEFAULT_SETTING)
		{
			/**Update live Offset with 4K stream live offset configured*/
			GETCONFIGVALUE(eAAMPConfig_LiveOffset4K, aamp->mLiveOffset);
			AAMPLOG_INFO("Updated live offset for 4K stream %lf", aamp->mLiveOffset);
			stream4K = true;
		}

	}while(0);
	return stream4K;
}

/**
 * @brief Abort wait for playlist download
 */
void MediaTrack::AbortWaitForPlaylistDownload()
{
	if(playlistDownloaderThreadStarted)
	{
		plDownloadWait.notify_one();
	}
	else
	{
		AAMPLOG_ERR("[%s] Playlist downloader thread not started", name);
	}
}

/**
 * @brief Wait until timeout is reached or interrupted
 */
void MediaTrack::EnterTimedWaitForPlaylistRefresh(int timeInMs)
{
	if(timeInMs > 0 && aamp->DownloadsAreEnabled())
	{
		std::unique_lock<std::mutex> lock(plDwnldMutex);
		if(plDownloadWait.wait_for(lock, std::chrono::milliseconds(timeInMs)) == std::cv_status::timeout)
		{
			AAMPLOG_TRACE("[%s] timeout exceeded %d", name, timeInMs); // make it trace
		}
		else
		{
			AAMPLOG_TRACE("[%s] Signalled conditional wait", name); // TRACE
		}
	}
}

/**
 * @brief Abort fragment downloader wait
 */
void MediaTrack::AbortFragmentDownloaderWait()
{
	if(fragmentCollectorWaitingForPlaylistUpdate)
	{
		frDownloadWait.notify_one();
	}
}

/**
 * @brief Wait for playlist download and update
 */
void MediaTrack::WaitForManifestUpdate()
{
	if(aamp->DownloadsAreEnabled() && fragmentCollectorWaitingForPlaylistUpdate)
	{
		std::unique_lock<std::mutex> lock(plDwnldMutex);
		AAMPLOG_INFO("Waiting for manifest update", name);
		frDownloadWait.wait(lock);
	}
	fragmentCollectorWaitingForPlaylistUpdate = false;
	AAMPLOG_INFO("Exit");
}

/**
 * @brief Playlist downloader
 */
void MediaTrack::PlaylistDownloader()
{
	MediaType mediaType = GetPlaylistMediaTypeFromTrack(type, IS_FOR_IFRAME(aamp->rate,type));
	std::string trackName = aamp->MediaTypeString(mediaType);
	int updateDuration = 0, liveRefreshTimeOutInMs = 0 ;
	updateDuration = GetDefaultDurationBetweenPlaylistUpdates();
	long long lastPlaylistDownloadTimeMS = 0;
	bool quickPlaylistDownload = false;
	bool firstTimeDownload = true;
	long minUpdateDuration = 0, maxSegDuration = 0,availTimeOffMs=0;
	
	// abortPlaylistDownloader is by default true, sets as "false" when thread initializes
	// This supports Single download mode for VOD and looped mode for Live (always runs in thread)
	if(abortPlaylistDownloader)
	{
		// Playlist downloader called one time, For VOD content profile changes
		AAMPLOG_INFO("Downloading playlist : %s", name);
	}
	else
	{
		// Playlist downlader called in loop mode
		AAMPLOG_WARN("[%s] : Enter, track '%s'", trackName.c_str(), name);
		AAMPLOG_INFO("[%s] Playlist download timeout : %d", trackName.c_str(), updateDuration);
	}

	if( aamp->GetLLDashServiceData()->lowLatencyMode )
	{
		minUpdateDuration = GetMinUpdateDuration();
		maxSegDuration = (long)(aamp->GetLLDashServiceData()->fragmentDuration*1000);
		availTimeOffMs = (long)((aamp->GetLLDashServiceData()->availabilityTimeOffset)*1000);

		AAMPLOG_INFO("LL-DASH [%s] maxSegDuration=i[%d]d[%f] minUpdateDuration=[%d]d[%f],availTimeOff=%d]d[%f]",
		name,(int)maxSegDuration,(double)maxSegDuration,(int)minUpdateDuration,(double)minUpdateDuration,(int)availTimeOffMs,(double)availTimeOffMs);
	}

	/* DOWNLODER LOOP */
	do
	{
		/* TIMEOUT WAIT LOGIC
		 *
		 * Skipping this for VOD contents.
		 * Hits : When player attempts ABR, Player rampdown for retry logic
		 * 			Subtitle language change is requested
		 * quickPlaylistDownload is enabled under above cases for live refresh.
		 *
		 */
		if(aamp->DownloadsAreEnabled() && aamp->IsLive() && !quickPlaylistDownload)
		{
			lastPlaylistDownloadTimeMS = GetLastPlaylistDownloadTime();
			liveRefreshTimeOutInMs = updateDuration - (int)(aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
			if(liveRefreshTimeOutInMs <= 0 && aamp->IsLive() && aamp->rate > 0)
			{
				AAMPLOG_TRACE("[%s] Refreshing playlist as it exceeded download timeout : %d", trackName.c_str(), updateDuration);
			}
			else
			{
				// For DASH first time download, always take maximum time to download enough fragments from different tracks
				// Else calculate wait time based on buffer
				if (firstTimeDownload && (eMEDIAFORMAT_DASH == aamp->mMediaFormat))
				{
					if(aamp->GetLLDashServiceData()->lowLatencyMode)
					{
						
						
						
						
						if( minUpdateDuration > 0 &&  minUpdateDuration > availTimeOffMs )
						{
							liveRefreshTimeOutInMs = (int)(minUpdateDuration-availTimeOffMs);
						}
						else if(maxSegDuration > 0 && maxSegDuration > availTimeOffMs)
						{
							liveRefreshTimeOutInMs = (int)maxSegDuration - availTimeOffMs;
						}
						else
						{
							liveRefreshTimeOutInMs = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
						}
					}
					else
					{
						liveRefreshTimeOutInMs = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
					}
				}
				else
				{
					liveRefreshTimeOutInMs = WaitTimeBasedOnBufferAvailable();
				}
				AAMPLOG_INFO("Refreshing playlist at %d ", liveRefreshTimeOutInMs);
				EnterTimedWaitForPlaylistRefresh(liveRefreshTimeOutInMs);
			}
			firstTimeDownload = false;
		}

		/* PLAYLIST DOWNLOAD LOGIC
		 *
		 * Proceed if downloads are enabled.
		 *
		 */
		if(aamp->DownloadsAreEnabled())
		{
			GrowableBuffer manifest;
			AAMPStatusType status = AAMPStatusType::eAAMPSTATUS_OK;
			// reset quickPlaylistDownload for live playlist
			quickPlaylistDownload = false;
			std::string manifestUrl = GetPlaylistUrl();
			// take the original url before it gets changed in GetFile
			std::string effectiveUrl = GetEffectivePlaylistUrl();
			bool gotManifest = false;
			long http_error = 0;
			double downloadTime;
			memset(&manifest, 0, sizeof(manifest));

			/*
			 *
			 * FOR HLS, This should be called here
			 * FOR DASH, after getting MPD doc
			 *
			 */
			if(eMEDIAFORMAT_DASH != aamp->mMediaFormat)
			{
				long long lastPlaylistDownloadTime = aamp_GetCurrentTimeMS();
				SetLastPlaylistDownloadTime(lastPlaylistDownloadTime);
			}

			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(manifestUrl, &manifest, effectiveUrl))
			{
				gotManifest = true;
				AAMPLOG_INFO("manifest[%s] retrieved from cache", trackName.c_str());
			}
			else
			{
				//aamp->profiler.ProfileBegin(PROFILE_BUCKET_MANIFEST);

				AampCurlInstance curlInstance = aamp->GetPlaylistCurlInstance(mediaType, false);
				// Enable downloads of mediaType if disabled
				if(!aamp->mMediaDownloadsEnabled[mediaType])
				{
					AAMPLOG_INFO("[%s] Re-enabling media download", trackName.c_str());
					aamp->EnableMediaDownloads(mediaType);
				}
				gotManifest = aamp->GetFile(manifestUrl, &manifest, effectiveUrl, &http_error, &downloadTime, NULL, curlInstance, true, mediaType);

				//update videoend info
				aamp->UpdateVideoEndMetrics(mediaType,0,http_error,effectiveUrl,downloadTime);
			}

			if(gotManifest)
			{
				if(eMEDIAFORMAT_DASH == aamp->mMediaFormat)
				{
					aamp->mManifestUrl = effectiveUrl;
				}
				else
				{
					// HLS or HLS_MP4
					// Set effective URL, else fragments will be mapped from old url
					SetEffectivePlaylistUrl(effectiveUrl);
				}
			}


			// Index playlist and update track informations.
			ProcessPlaylist(manifest, http_error);

			if(fragmentCollectorWaitingForPlaylistUpdate && gotManifest)
			{
				// (gotManifest => false) If manifest download failed due to ABR request from HLS, don't abort wait.
				// DASH waits for manifest update only at EOS from all tracks, proceed only with fresh manifest.
				// Signal fragment collector to abort it's wait for playlist process
				AbortFragmentDownloaderWait();
			}

			// Check whether downloads are still enabled after processing playlist
			if (aamp->DownloadsAreEnabled())
			{
				if (!aamp->mMediaDownloadsEnabled[mediaType])
				{
					AAMPLOG_ERR("[%s] Aborted playlist download by callback, retrying..", trackName.c_str());
					// Download playlist without any wait
					quickPlaylistDownload = true;
				}
			}
			else // if downloads disabled
			{
				AAMPLOG_ERR("[%s] : Downloads are disabled, exitting", trackName.c_str());
				abortPlaylistDownloader = true;
			}
		}
		else
		{
			AAMPLOG_ERR("[%s] : Downloads are disabled, exitting", trackName.c_str());
			abortPlaylistDownloader = true;
		}
	} while (!abortPlaylistDownloader && aamp->IsLive());
	// abortPlaylistDownloader is true by default, made for VOD playlist.
	// Loop runs for Live manifests, closes at dynamic => static transition

	AAMPLOG_WARN("[%s] : Exit", trackName.c_str());
}
/**
 * @brief Wait time for playlist refresh based on buffer available
 *
 * @return minDelayBetweenPlaylistUpdates - wait time for playlist refresh
 */
int MediaTrack::WaitTimeBasedOnBufferAvailable()
{
	long long lastPlaylistDownloadTimeMS = GetLastPlaylistDownloadTime();
	int minDelayBetweenPlaylistUpdates = 0;
	if (lastPlaylistDownloadTimeMS)
	{
		int timeSinceLastPlaylistDownload = (int)(aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
		long long currentPlayPosition = aamp->GetPositionMilliseconds();
		long long endPositionAvailable = (aamp->culledSeconds + aamp->durationSeconds)*1000;
		bool lowLatencyMode = aamp->GetLLDashServiceData()->lowLatencyMode;
		// playTarget value will vary if TSB is full and trickplay is attempted. Cant use for buffer calculation
		// So using the endposition in playlist - Current playing position to get the buffer availability
		long bufferAvailable = (endPositionAvailable - currentPlayPosition);
		//Get Minimum update duration in milliseconds
		long minUpdateDuration = GetMinUpdateDuration();
		minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;

		// when target duration is high value(>Max delay)  but buffer is available just above the max update inteval,then go with max delay between playlist refresh.
		if(bufferAvailable < (2* MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS))
		{
			if ((minUpdateDuration > 0) && (bufferAvailable  > minUpdateDuration))
			{
				//1.If buffer Available is > 2*minUpdateDuration , may be 1.0 times also can be set ???
				//2.If buffer is between 2*target & mMinUpdateDurationMs
				float mFactor=0.0f;
				if (lowLatencyMode)
				{
					mFactor = (bufferAvailable  > (minUpdateDuration * 2)) ? (float)(minUpdateDuration/1000) : 0.5;
				}
				else
				{
					mFactor = (bufferAvailable  > (minUpdateDuration * 2)) ? 1.5 : 0.5;
				}
				minDelayBetweenPlaylistUpdates = (int)(mFactor * minUpdateDuration);
			}
			// if buffer < targetDuration && buffer < MaxDelayInterval
			else
			{
				// if bufferAvailable is less than targetDuration ,its in RED alert . Close to freeze
				// need to refresh soon ..
				minDelayBetweenPlaylistUpdates = (bufferAvailable) ? (int)(bufferAvailable / 3) : MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS; //500ms
				// limit the logs when buffer is low
				{
					static int bufferlowCnt;
					if((bufferlowCnt++ & 5) == 0)
					{
						AAMPLOG_WARN("Buffer is running low(%ld).Refreshing playlist(%d).PlayPosition(%lld) End(%lld)",
								bufferAvailable,minDelayBetweenPlaylistUpdates,currentPlayPosition,endPositionAvailable);
					}
				}
			}
		}

		// First cap max limit ..
		// remove already consumed time from last update
		// if time interval goes negative, limit to min value

		// restrict to Max delay interval
		if (minDelayBetweenPlaylistUpdates > MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
		{
			minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
		}

		// adjust with last refreshed time interval
		minDelayBetweenPlaylistUpdates -= timeSinceLastPlaylistDownload;

		if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
		{
			if (lowLatencyMode)
			{
				long availTimeOffMs = (long)((aamp->GetLLDashServiceData()->availabilityTimeOffset)*1000);
				long maxSegDuration = (long)(aamp->GetLLDashServiceData()->fragmentDuration*1000);
				if(minUpdateDuration > 0 && minUpdateDuration < maxSegDuration)
				{
					minDelayBetweenPlaylistUpdates = (int)minUpdateDuration;		
				}
				else if(minUpdateDuration > 0 && minUpdateDuration > availTimeOffMs)
				{
					minDelayBetweenPlaylistUpdates = (int)minUpdateDuration-availTimeOffMs;
				}
				else if (maxSegDuration > 0 && maxSegDuration > availTimeOffMs)
				{
						minDelayBetweenPlaylistUpdates = (int)maxSegDuration-availTimeOffMs;
				}
				else
				{
					// minimum of 500 mSec needed to avoid too frequent download.
					minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
				}
				if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
				{
						// minimum of 500 mSec needed to avoid too frequent download.
					minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
				}
			}
			else
			{
				// minimum of 500 mSec needed to avoid too frequent download.
				minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			}
		}

		AAMPLOG_INFO("aamp playlist end refresh bufferMs(%ld) delay(%d) delta(%d) End(%lld) PlayPosition(%lld)",
			bufferAvailable,minDelayBetweenPlaylistUpdates,timeSinceLastPlaylistDownload,endPositionAvailable,currentPlayPosition);

		// sleep before next manifest update
		// aamp->InterruptableMsSleep();
	}
	return minDelayBetweenPlaylistUpdates;
}
