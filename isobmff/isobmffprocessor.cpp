/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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
* @file isobmffprocessor.cpp
* @brief Source file for ISO Base Media File Format Segment Processor
*/

#include "isobmffprocessor.h"
#include <pthread.h>
#include <assert.h>

static const char *IsoBmffProcessorTypeName[] =
{
    "video", "audio"
};

/**
 *  @brief IsoBmffProcessor constructor
 */
IsoBmffProcessor::IsoBmffProcessor(class PrivateInstanceAAMP *aamp, AampLogManager *logObj, IsoBmffProcessorType trackType, IsoBmffProcessor* peerBmffProcessor)
	: p_aamp(aamp), type(trackType), peerProcessor(peerBmffProcessor), basePTS(0),
	processPTSComplete(false), timeScale(0), initSegment(),
	playRate(1.0f), abortAll(false), m_mutex(), m_cond(),
	initSegmentProcessComplete(false),
	mLogObj(logObj)	
{
	AAMPLOG_WARN("IsoBmffProcessor:: Created IsoBmffProcessor(%p) for type:%d and peerProcessor(%p)", this, type, peerBmffProcessor);
	if (peerProcessor)
	{
		peerProcessor->setPeerProcessor(this);
	}
	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_cond, NULL);

	// Sometimes AAMP pushes an encrypted init segment first to force decryptor plugin selection
	initSegment.reserve(2);
}

/**
 *  @brief IsoBmffProcessor destructor
 */
IsoBmffProcessor::~IsoBmffProcessor()
{
	clearInitSegment();
	pthread_mutex_destroy(&m_mutex);
	pthread_cond_destroy(&m_cond);
}

/**
 *  @brief Process and send ISOBMFF fragment
 */
bool IsoBmffProcessor::sendSegment(char *segment, size_t& size, double position, double duration, bool discontinuous, bool &ptsError)
{
	ptsError = false;
	bool ret = true;

	AAMPLOG_TRACE("IsoBmffProcessor:: [%s] sending segment at pos:%f dur:%f", IsoBmffProcessorTypeName[type], position, duration);

	// Logic for Audio Track
	if (type == eBMFFPROCESSOR_TYPE_AUDIO)
	{
		if (!processPTSComplete)
		{
			IsoBmffBuffer buffer(mLogObj);
			buffer.setBuffer((uint8_t *)segment, size);
			buffer.parseBuffer();

			if (buffer.isInitSegment())
			{
				cacheInitSegment(segment, size);
				ret = false;
			}
			else
			{
				// Wait for video to parse PTS
				pthread_mutex_lock(&m_mutex);
				if (!processPTSComplete)
				{
					AAMPLOG_INFO("IsoBmffProcessor:: [%s] Going into wait for PTS processing to complete",  IsoBmffProcessorTypeName[type]);
					pthread_cond_wait(&m_cond, &m_mutex);
				}
				if (abortAll)
				{
					ret = false;
				}
				pthread_mutex_unlock(&m_mutex);
			}
		}
		if (ret && !initSegmentProcessComplete)
		{
			if (processPTSComplete)
			{
				double pos = ((double)basePTS / (double)timeScale);
				if (!initSegment.empty())
				{
					pushInitSegment(pos);
				}
				else
				{
					// We have no cached init fragment, maybe audio download was delayed very much
					// Push this fragment with calculated PTS
					p_aamp->SendStreamCopy((MediaType)type, segment, size, pos, pos, duration);
					ret = false;
				}
				initSegmentProcessComplete = true;
			}
		}
	}


	// Logic for Video Track
	// For trickplay, restamping is done in qtdemux. We can avoid
	// pts parsing logic
	if (ret && !processPTSComplete && playRate == AAMP_NORMAL_PLAY_RATE)
	{
		// We need to parse PTS from first buffer
		IsoBmffBuffer buffer(mLogObj);
		buffer.setBuffer((uint8_t *)segment, size);
		buffer.parseBuffer();

		if (buffer.isInitSegment())
		{
			uint32_t tScale = 0;
			if (buffer.getTimeScale(tScale))
			{
				timeScale = tScale;
				AAMPLOG_INFO("IsoBmffProcessor:: [%s] TimeScale (%u) set", IsoBmffProcessorTypeName[type], timeScale);
			}

			cacheInitSegment(segment, size);
			ret = false;
		}
		else
		{
			// Init segment was parsed and stored previously. Find the base PTS now
			uint64_t fPts = 0;
			if (buffer.getFirstPTS(fPts))
			{
				basePTS = fPts;
				processPTSComplete = true;
				AAMPLOG_WARN("IsoBmffProcessor:: [%s] Base PTS (%lld) set", IsoBmffProcessorTypeName[type], basePTS);
			}
			else
			{
				AAMPLOG_ERR("IsoBmffProcessor:: [%s] Failed to process pts from buffer at pos:%f and dur:%f", IsoBmffProcessorTypeName[type], position, duration);
			}

			pthread_mutex_lock(&m_mutex);
			if (abortAll)
			{
				ret = false;
			}
			pthread_mutex_unlock(&m_mutex);

			if (ret && processPTSComplete)
			{
				if (timeScale == 0)
				{
					if (initSegment.empty())
					{
						AAMPLOG_WARN("IsoBmffProcessor::  [%s] Init segment missing during PTS processing!",  IsoBmffProcessorTypeName[type]);
						p_aamp->SendErrorEvent(AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING);
						ret = false;
					}
					else
					{
						AAMPLOG_WARN("IsoBmffProcessor:: [%s] MDHD/MVHD boxes are missing in init segment!",  IsoBmffProcessorTypeName[type]);
						uint32_t tScale = 0;
						if (buffer.getTimeScale(tScale))
						{
							timeScale = tScale;
							AAMPLOG_INFO("IsoBmffProcessor:: [%s] TimeScale (%u) set",  IsoBmffProcessorTypeName[type], timeScale);
						}
						if (timeScale == 0)
						{
							AAMPLOG_ERR("IsoBmffProcessor:: [%s] TimeScale value missing in init segment and mp4 fragment, setting to a default of 1!",  IsoBmffProcessorTypeName[type]);
							timeScale = 1; // to avoid div-by-zero errors later. MDHD and MVHD are mandatory boxes, but lets relax for now
						}

					}
				}

				if (ret)
				{
					double pos = ((double)basePTS / (double)timeScale);
					// If AAMP override hack is enabled for this platform, then we need to pass the basePTS value to
					// PrivateInstanceAAMP since PTS will be restamped in qtdemux. This ensures proper pts value is sent in progress event.
#ifdef ENABLE_AAMP_QTDEMUX_OVERRIDE
					p_aamp->NotifyFirstVideoPTS(basePTS, timeScale);
					// Here, basePTS might not be based on a 90KHz clock, whereas gst videosink might be.
					// So PTS value sent via progress event might not be accurate.
					p_aamp->NotifyVideoBasePTS(basePTS, timeScale);
#endif
					if (type == eBMFFPROCESSOR_TYPE_VIDEO)
					{
						// Send flushing seek to gstreamer pipeline.
						// For new tune, this will not work, so send pts as fragment position
						p_aamp->FlushStreamSink(pos, playRate);
					}

					if (peerProcessor)
					{
						peerProcessor->setBasePTS(basePTS, timeScale);
					}

					pushInitSegment(pos);
					initSegmentProcessComplete = true;
				}
			}
		}
	}

	if (ret)
	{
		p_aamp->ProcessID3Metadata(segment, size, (MediaType)type);
		p_aamp->SendStreamCopy((MediaType)type, segment, size, position, position, duration);
	}
	return true;
}

/**
 *  @brief Abort all operations
 */
void IsoBmffProcessor::abort()
{
	pthread_mutex_lock(&m_mutex);
	abortAll = true;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock(&m_mutex);
}

/**
 *  @brief Reset all variables
 */
void IsoBmffProcessor::reset()
{
	clearInitSegment();

	pthread_mutex_lock(&m_mutex);
	basePTS = 0;
	timeScale = 0;
	processPTSComplete = false;
	abortAll = false;
	initSegmentProcessComplete = false;
	pthread_mutex_unlock(&m_mutex);
}

/**
 *  @brief Set playback rate
 */
void IsoBmffProcessor::setRate(double rate, PlayMode mode)
{
	playRate = rate;
}

/**
 *  @brief Set base PTS and TimeScale value
 */
void IsoBmffProcessor::setBasePTS(uint64_t pts, uint32_t tScale)
{
	AAMPLOG_WARN("[%s] Base PTS (%lld) and TimeScale (%u) set",  IsoBmffProcessorTypeName[type], pts, tScale);
	pthread_mutex_lock(&m_mutex);
	basePTS = pts;
	timeScale = tScale;
	processPTSComplete = true;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock(&m_mutex);
}

/**
 *  @brief Cache init fragment internally
 */
void IsoBmffProcessor::cacheInitSegment(char *segment, size_t size)
{
	// Save init segment for later. Init segment will be pushed once basePTS is calculated
	AAMPLOG_INFO("IsoBmffProcessor::[%s] Caching init fragment", IsoBmffProcessorTypeName[type]);
	GrowableBuffer *buffer = new GrowableBuffer();
	memset(buffer, 0x00, sizeof(GrowableBuffer));
	aamp_AppendBytes(buffer, segment, size);
	initSegment.push_back(buffer);
}


/**
 *  @brief Push init fragment cached earlier
 */
void IsoBmffProcessor::pushInitSegment(double position)
{
	// Push init segment now, duration = 0
	AAMPLOG_WARN("IsoBmffProcessor:: [%s] Push init fragment", IsoBmffProcessorTypeName[type]);
	if (initSegment.size() > 0)
	{
		for (auto it = initSegment.begin(); it != initSegment.end();)
		{
			GrowableBuffer *buf = *it;
			p_aamp->SendStreamTransfer((MediaType)type, buf, position, position, 0);
			aamp_Free(buf);
			SAFE_DELETE(buf);
			it = initSegment.erase(it);
		}
	}
}

/**
 *  @brief Clear init fragment cached earlier
 */
void IsoBmffProcessor::clearInitSegment()
{
	if (initSegment.size() > 0)
	{
		for (auto it = initSegment.begin(); it != initSegment.end();)
		{
			GrowableBuffer *buf = *it;
			aamp_Free(buf);
			SAFE_DELETE(buf);
			it = initSegment.erase(it);
		}
	}
}

