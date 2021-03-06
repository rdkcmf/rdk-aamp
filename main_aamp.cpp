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
 * @file main_aamp.cpp
 * @brief Advanced Adaptive Media Player (AAMP)
 */

#include "main_aamp.h"
#include "GlobalConfigAAMP.h"
#include "AampCacheHandler.h"
#include "AampUtils.h"
#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif
#include "helper/AampDrmHelper.h"
#include "StreamAbstractionAAMP.h"
#include "aampgstplayer.h"

#include <dlfcn.h>

#ifdef WIN32
#include "conio.h"
#else
#include <termios.h>
#include <errno.h>
#include <regex>
#endif //WIN32

AampConfig *gpGlobalAampConfig=NULL;

#define ERROR_STATE_CHECK_VOID() \
	PrivAAMPState state; \
	aamp->GetState(state); \
	if( state == eSTATE_ERROR){ \
		logprintf("%s() operation is not allowed when player in eSTATE_ERROR state !", __FUNCTION__ );\
		return; \
	}

#define ERROR_STATE_CHECK_VAL(val) \
	PrivAAMPState state; \
	aamp->GetState(state); \
	if( state == eSTATE_ERROR){ \
		logprintf("%s() operation is not allowed when player in eSTATE_ERROR state !", __FUNCTION__ );\
		return val; \
	}

#define ERROR_OR_IDLE_STATE_CHECK_VOID() \
	PrivAAMPState state; \
	aamp->GetState(state); \
	if( state == eSTATE_ERROR || state == eSTATE_IDLE){ \
		logprintf("%s() operation is not allowed when player in %s state !", __FUNCTION__ ,\
		(state == eSTATE_ERROR) ? "eSTATE_ERROR" : "eSTATE_IDLE" );\
		return; \
	}

#define NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID() \
	PrivAAMPState state; \
	aamp->GetState(state); \
	if( state != eSTATE_IDLE && state != eSTATE_RELEASED){ \
		logprintf("%s() operation is not allowed when player not in eSTATE_IDLE or eSTATE_RELEASED state !", __FUNCTION__ );\
		return; \
	}

#define ERROR_OR_IDLE_STATE_CHECK_VAL(val) \
	PrivAAMPState state; \
	aamp->GetState(state); \
	if( state == eSTATE_ERROR || state == eSTATE_IDLE){ \
		logprintf("%s() operation is not allowed in %s state !", __FUNCTION__ ,\
		(state == eSTATE_ERROR) ? "eSTATE_ERROR" : "eSTATE_IDLE" );\
		return val; \
	}

/**
 *   @brief Constructor.
 *
 *   @param[in]  streamSink - custom stream sink, NULL for default.
 */
PlayerInstanceAAMP::PlayerInstanceAAMP(StreamSink* streamSink
	, std::function< void(uint8_t *, int, int, int) > exportFrames
	) : aamp(NULL), mInternalStreamSink(NULL), mJSBinding_DL(),mAsyncRunning(false),mConfig()
{
#ifdef SUPPORT_JS_EVENTS
#ifdef AAMP_WPEWEBKIT_JSBINDINGS //aamp_LoadJS defined in libaampjsbindings.so
	const char* szJSLib = "libaampjsbindings.so";
#else
	const char* szJSLib = "libaamp.so";
#endif
	mJSBinding_DL = dlopen(szJSLib, RTLD_GLOBAL | RTLD_LAZY);
	logprintf("[AAMP_JS] dlopen(\"%s\")=%p", szJSLib, mJSBinding_DL);
#endif

	// Create very first instance of Aamp Config to read the cfg & Operator file .This is needed for very first
	// tune only . After that every tune will use the same config parameters
	if(gpGlobalAampConfig == NULL)
	{
		logprintf("[AAMP_JS][%p]Creating global Config Instance",this);
		gpGlobalAampConfig =  new AampConfig();
		gpGlobalAampConfig->ReadAampCfgTxtFile();
		gpGlobalAampConfig->ReadOperatorConfiguration();
		gpGlobalAampConfig->ReadAampCfgJsonFile();
		gpGlobalAampConfig->ShowAAMPConfiguration();
	}

	// Copy the default configuration to session configuration .App can modify the configuration set
	mConfig = *gpGlobalAampConfig;

	aamp = new PrivateInstanceAAMP(&mConfig);
	if (NULL == streamSink)
	{
		mInternalStreamSink = new AAMPGstPlayer(aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
		, exportFrames
#endif
		);
		streamSink = mInternalStreamSink;
	}
	aamp->SetStreamSink(streamSink);
	if (gpGlobalConfig->mAsyncTuneConfig == eTrueState)
	{
		EnableAsyncOperation();
	}
}

/**
 * @brief PlayerInstanceAAMP Destructor
 */
PlayerInstanceAAMP::~PlayerInstanceAAMP()
{
	if (aamp)
	{
		if (mAsyncRunning)
		{
			// Before stop, clear up any remaining commands in queue
			RemoveAllTasks();
			aamp->DisableDownloads();
			AcquireExLock();
		}
		PrivAAMPState state;
		aamp->GetState(state);
		if (state != eSTATE_IDLE && state != eSTATE_RELEASED)
		{
			//Avoid stop call since already stopped
			aamp->Stop();
		}
		if (mAsyncRunning)
		{
			//Release lock
			ReleaseExLock();
		}
		delete aamp;
		aamp = NULL;
	}
	if (mInternalStreamSink)
	{
		delete mInternalStreamSink;
	}

	// Stop the async thread, queue will be cleared during Stop
	if (mAsyncRunning)
	{
		mAsyncRunning = false;
		StopScheduler();
	}

	bool isLastPlayerInstance = !PrivateInstanceAAMP::IsActiveInstancePresent();

#ifdef AAMP_CC_ENABLED
	if (isLastPlayerInstance)
	{
		AampCCManager::DestroyInstance();
	}
#endif
#ifdef SUPPORT_JS_EVENTS 
	if (mJSBinding_DL && isLastPlayerInstance)
	{
		logprintf("[AAMP_JS] dlclose(%p)", mJSBinding_DL);
		dlclose(mJSBinding_DL);
	}
#endif
	if (isLastPlayerInstance && gpGlobalConfig)
	{
		logprintf("[%s] Release GlobalConfig(%p)", __FUNCTION__,gpGlobalConfig);
		delete gpGlobalConfig;
		gpGlobalConfig = NULL;
	}
	
	if(gpGlobalAampConfig)
	{
		logprintf("[%s] Release GlobalConfig(%p)", __FUNCTION__,gpGlobalAampConfig);
		delete gpGlobalAampConfig;
	}
}


/**
 * @brief Stop playback and release resources.
 *
 * @param[in] sendStateChangeEvent - true if state change events need to be sent for Stop operation, default value true
 */
void PlayerInstanceAAMP::Stop(bool sendStateChangeEvent)
{
	if (aamp)
	{
		if (mAsyncRunning)
		{
			// Before stop, clear up any remaining commands in queue
			RemoveAllTasks();
			aamp->DisableDownloads();
			AcquireExLock();
		}
		PrivAAMPState state;
		aamp->GetState(state);

		//state will be eSTATE_IDLE or eSTATE_RELEASED, right after an init or post-processing of a Stop call
		if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
		{
			if (mAsyncRunning)
			{
				ReleaseExLock();
				aamp->EnableDownloads();
			}
			return;
		}

		StopInternal(sendStateChangeEvent);

		if (mAsyncRunning)
		{
			//Release lock
			ReleaseExLock();
			// EnableDownloads() not called, because it will be called in aamp->Stop()
		}
	}
}

/**
 *   @brief Tune to a URL.
 *   This extra Tune function is included for backwards compatibility
 *   @param[in]  url - HTTP/HTTPS url to be played.
 *   @param[in]  contentType - Content type of the asset
 *   @param[in]  audioDecoderStreamSync - Enable or disable audio decoder stream sync,
 *                set to 'false' if audio fragments come with additional padding at the end (BCOM-4203)
 *   @return void
 */
void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync)
{
	Tune(mainManifestUrl, /*autoPlay*/ true, contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync);
}

/**
 * @brief Tune to a URL.
 *
 * @param  mainManifestUrl - HTTP/HTTPS url to be played.
 * @param[in] autoPlay - Start playback immediately or not
 * @param  contentType - content Type.
 */
void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync)
{
	PrivAAMPState state;
	aamp->GetState(state);

	if ((state != eSTATE_IDLE) && (state != eSTATE_RELEASED))
	{
		//Calling tune without closing previous tune
		StopInternal(false);
	}
	aamp->getAampCacheHandler()->StartPlaylistCache();
	aamp->Tune(mainManifestUrl, autoPlay, contentType, bFirstAttempt, bFinalAttempt,traceUUID,audioDecoderStreamSync);

}



/**
 * @brief Soft-realease player.
 *
 */
void PlayerInstanceAAMP::detach()
{
	if (mAsyncRunning)
	{
		//Acquire lock
		AcquireExLock();
	}
	aamp->detach();
	if (mAsyncRunning)
	{
		//Release lock
		ReleaseExLock();
	}
}

/**
 *   @brief Register event handler.
 *
 *   @param  eventListener - pointer to implementation of EventListener to receive events.
 */
void PlayerInstanceAAMP::RegisterEvents(EventListener* eventListener)
{
	aamp->RegisterEvents(eventListener);
}

/**
 * @brief Set retry limit on Segment injection failure.
 *
 */
void PlayerInstanceAAMP::SetSegmentInjectFailCount(int value)
{
	if(gpGlobalConfig->segInjectFailCount > 0)
	{
		aamp->mSegInjectFailCount = gpGlobalConfig->segInjectFailCount;
		AAMPLOG_INFO("%s:%d Setting limit from configuration file: %d", __FUNCTION__, __LINE__, aamp->mSegInjectFailCount);
	}
	else
	{
		if ((value > 0) && (value <= MAX_SEG_INJECT_FAIL_COUNT))
		{
			aamp->mSegInjectFailCount = value;
			AAMPLOG_INFO("%s:%d Setting Segment Inject fail count : %d", __FUNCTION__, __LINE__, aamp->mSegInjectFailCount);
		}
		else
		{
			AAMPLOG_WARN("%s:%d Invalid value %d, will continue with %d", __FUNCTION__,__LINE__, value, MAX_SEG_INJECT_FAIL_COUNT);
		}
	}
}

/**
 * @brief Set retry limit on Segment drm decryption failure.
 *
 */
void PlayerInstanceAAMP::SetSegmentDecryptFailCount(int value)
{
	if (gpGlobalConfig->drmDecryptFailCount > 0)
	{
		aamp->mDrmDecryptFailCount = gpGlobalConfig->drmDecryptFailCount;
		AAMPLOG_INFO("%s:%d Setting limit from configuration file: %d", __FUNCTION__, __LINE__, aamp->mDrmDecryptFailCount);
	}
	else
	{
		if ((value > 0) && (value <= MAX_SEG_DRM_DECRYPT_FAIL_COUNT))
		{
			aamp->mDrmDecryptFailCount = value;
			AAMPLOG_INFO("%s:%d Setting Segment DRM decrypt fail count : %d", __FUNCTION__, __LINE__, aamp->mDrmDecryptFailCount);
		}
		else
		{
			AAMPLOG_WARN("%s:%d Invalid value %d, will continue with %d", __FUNCTION__,__LINE__, value, MAX_SEG_DRM_DECRYPT_FAIL_COUNT);
		}
	}
}

/**
 * @brief Set initial buffer duration in seconds
 *
 */
void PlayerInstanceAAMP::SetInitialBufferDuration(int durationSec)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();
	if(gpGlobalConfig->minInitialCacheSeconds == MINIMUM_INIT_CACHE_NOT_OVERRIDDEN)
	{
		aamp->SetInitialBufferDuration(durationSec);
	}
}

/**
 * @brief Set Maximum Cache Size for playlist store 
 *
 */
void PlayerInstanceAAMP::SetMaxPlaylistCacheSize(int cacheSize)
{
        if(gpGlobalConfig->gMaxPlaylistCacheSize == 0)
        {
                aamp->SetMaxPlaylistCacheSize(cacheSize);
        }
}

/**
 * @brief Set profile ramp down limit.
 *
 */
void PlayerInstanceAAMP::SetRampDownLimit(int limit)
{
	aamp->SetRampDownLimit(limit);
}

/**
 * @brief Set Language Format
 * @param[in] preferredFormat - one of \ref LangCodePreference
 * @param[in] useRole - if enabled, the language in format <lang>-<role>
 *                      if <role> attribute available in stream
 *
 * @return void
 */
void PlayerInstanceAAMP::SetLanguageFormat(LangCodePreference preferredFormat, bool useRole)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();
	gpGlobalConfig->langCodePreference = preferredFormat;
	gpGlobalConfig->bDescriptiveAudioTrack = useRole;
}

/**
 * @brief Set minimum bitrate value.
 * @param  url - stream url with vss service zone info as query string
 */
void PlayerInstanceAAMP::SetMinimumBitrate(long bitrate)
{
	if (gpGlobalConfig->minBitrate > 0)
	{
		aamp->SetMinimumBitrate(gpGlobalConfig->minBitrate);
		AAMPLOG_INFO("%s:%d Setting minBitrate from configuration file: %ld", __FUNCTION__, __LINE__, gpGlobalConfig->minBitrate);
	}
	else
	{
		if (bitrate > 0)
		{
			AAMPLOG_INFO("%s:%d Setting minimum bitrate: %ld", __FUNCTION__, __LINE__, bitrate);
			aamp->SetMinimumBitrate(bitrate);
		}
		else
		{
			AAMPLOG_WARN("%s:%d Invalid bitrate value %ld", __FUNCTION__,__LINE__, bitrate);
		}
	}
}

/**
 * @brief Set maximum bitrate value.
 *
 */
void PlayerInstanceAAMP::SetMaximumBitrate(long bitrate)
{
	if (gpGlobalConfig->maxBitrate > 0)
	{
		aamp->SetMinimumBitrate(gpGlobalConfig->maxBitrate);
		AAMPLOG_INFO("%s:%d Setting maxBitrate from configuration file: %ld", __FUNCTION__, __LINE__, gpGlobalConfig->maxBitrate);
	}
	else
	{
		if (bitrate > 0)
		{
			AAMPLOG_INFO("%s:%d Setting maximum bitrate : %ld", __FUNCTION__,__LINE__, bitrate);
			aamp->SetMaximumBitrate(bitrate);
		}
		else
		{
			AAMPLOG_WARN("%s:%d Invalid bitrate value %d", __FUNCTION__,__LINE__, bitrate);
		}
	}
}

/**
 *   @brief Check given rate is valid.
 *
 *   @param[in] rate - Rate of playback.
 *   @retval return true if the given rate is valid.
 */
bool PlayerInstanceAAMP::IsValidRate(int rate)
{
	bool retValue = false;
	if (abs(rate) <= AAMP_RATE_TRICKPLAY_MAX)
	{
		retValue = true;
	}
	return retValue;
}

/**
 *   @brief Set playback rate.
 *
 *   @param  rate - Rate of playback.
 *   @param  overshootcorrection - overshoot correction in milliseconds.
 */
void PlayerInstanceAAMP::SetRate(int rate,int overshootcorrection)
{
	AAMPLOG_INFO("%s:%d PLAYER[%d] rate=%d.", __FUNCTION__, __LINE__, aamp->mPlayerId, rate);

	ERROR_STATE_CHECK_VOID();

	if (!IsValidRate(rate))
	{
		AAMPLOG_WARN("%s:%d SetRate ignored!! Invalid rate (%d)", __FUNCTION__, __LINE__, rate);
		return;
	}

	if (aamp->mpStreamAbstractionAAMP)
	{
		if (!aamp->mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0)
		{
			AAMPLOG_WARN("%s:%d Ignoring trickplay. No iframe tracks in stream", __FUNCTION__, __LINE__);
			aamp->NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
			return;
		}
		if(!(aamp->mbPlayEnabled) && aamp->pipeline_paused && (AAMP_NORMAL_PLAY_RATE == rate))
		{
			AAMPLOG_WARN("%s:%d PLAYER[%d] Player %s=>%s.", __FUNCTION__, __LINE__, aamp->mPlayerId, STRBGPLAYER, STRFGPLAYER );
			aamp->mbPlayEnabled = true;
			aamp->LogPlayerPreBuffered();
			aamp->mStreamSink->Configure(aamp->mVideoFormat, aamp->mAudioFormat, aamp->mAuxFormat, aamp->mpStreamAbstractionAAMP->GetESChangeStatus(), aamp->mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
			aamp->mpStreamAbstractionAAMP->StartInjection();
			aamp->mStreamSink->Stream();
			aamp->pipeline_paused = false;
			return;
		}
		bool retValue = true;
		if (rate > 0 && aamp->IsLive() && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint() && aamp->rate >= AAMP_NORMAL_PLAY_RATE)
		{
			AAMPLOG_WARN("%s(): Already at logical live point, hence skipping operation", __FUNCTION__);
			aamp->NotifyOnEnteringLive();
			return;
		}

		// DELIA-39691 If input rate is same as current playback rate, skip duplicate operation
		// Additional check for pipeline_paused is because of 0(PAUSED) -> 1(PLAYING), where aamp->rate == 1.0 in PAUSED state
		if ((!aamp->pipeline_paused && rate == aamp->rate) || (rate == 0 && aamp->pipeline_paused))
		{
			AAMPLOG_WARN("%s(): Already running at playback rate(%d) pipeline_paused(%d), hence skipping set rate for (%d)", __FUNCTION__, aamp->rate, aamp->pipeline_paused, rate);
			return;
		}

		//DELIA-30274  -- Get the trick play to a closer position
		//Logic adapted
		// XRE gives fixed overshoot position , not suited for aamp . So ignoring overshoot correction value
			// instead use last reported posn vs the time player get play command
		// a. During trickplay , last XRE reported position is stored in aamp->mReportProgressPosn
					/// and last reported time is stored in aamp->mReportProgressTime
		// b. Calculate the time delta	from last reported time
		// c. Using this diff , calculate the best/nearest match position (works out 70-80%)
		// d. If time delta is < 100ms ,still last video fragment rendering is not removed ,but position updated very recently
			// So switch last displayed position - NewPosn -= Posn - ((aamp->rate/4)*1000)
		// e. If time delta is > 950ms , possibility of next frame to come by the time play event is processed.
			//So go to next fragment which might get displayed
		// f. If none of above ,maintain the last displayed position .
		//
		// h. TODO (again trial n error) - for 3x/4x , within 1sec there might multiple frame displayed . Can use timedelta to calculate some more near,to be tried

		int  timeDeltaFromProgReport = (aamp_GetCurrentTimeMS() - aamp->mReportProgressTime);

		//Skip this logic for either going to paused to coming out of paused scenarios with HLS
		//What we would like to avoid here is the update of seek_pos_seconds because gstreamer position will report proper position
		//Check for 1.0 -> 0.0 and 0.0 -> 1.0 usecase and avoid below logic
		if (!((aamp->rate == AAMP_NORMAL_PLAY_RATE && rate == 0) || (aamp->pipeline_paused && rate == AAMP_NORMAL_PLAY_RATE)))
		{
			double newSeekPosInSec = -1;
			// when switching from trick to play mode only
			if(aamp->rate && rate == AAMP_NORMAL_PLAY_RATE && !aamp->pipeline_paused)
			{
				if(timeDeltaFromProgReport > 950) // diff > 950 mSec
				{
					// increment by 1x trickplay frame , next possible displayed frame
					newSeekPosInSec = (aamp->mReportProgressPosn+(aamp->rate*1000))/1000;
				}
				else if(timeDeltaFromProgReport > 100) // diff > 100 mSec
				{
					// Get the last shown frame itself
					newSeekPosInSec = aamp->mReportProgressPosn/1000;
				}
				else
				{
					// Go little back to last shown frame
					newSeekPosInSec = (aamp->mReportProgressPosn-(aamp->rate*1000))/1000;
				}

				if (newSeekPosInSec >= 0)
				{
					aamp->seek_pos_seconds = newSeekPosInSec;
				}
				else
				{
					AAMPLOG_WARN("%s:%d new seek_pos_seconds calculated is invalid(%f), discarding it!", __FUNCTION__, __LINE__, newSeekPosInSec);
				}
			}
			else
			{
				// Coming out of pause mode(aamp->rate=0) or when going into pause mode (rate=0)
				// Show the last position
				aamp->seek_pos_seconds = aamp->GetPositionMilliseconds()/1000;
			}

			aamp->trickStartUTCMS = -1;
		}
		else
		{
			// DELIA-39530 - For 1.0->0.0 and 0.0->1.0 if bPositionQueryEnabled is enabled, GStreamer position query will give proper value
			// Fallback case added for when bPositionQueryEnabled is disabled, since we will be using elapsedTime to calculate position and
			// trickStartUTCMS has to be reset
			if (!gpGlobalConfig->bPositionQueryEnabled)
			{
				aamp->seek_pos_seconds = aamp->GetPositionMilliseconds()/1000;
				aamp->trickStartUTCMS = -1;
			}
		}

		logprintf("aamp_SetRate(%d)overshoot(%d) ProgressReportDelta:(%d) ", rate,overshootcorrection,timeDeltaFromProgReport);
		logprintf("aamp_SetRate Adj position: %f", aamp->seek_pos_seconds); // current position relative to tune time
		logprintf("aamp_SetRate rate(%d)->(%d)", aamp->rate,rate);
		logprintf("aamp_SetRate cur pipeline: %s", aamp->pipeline_paused ? "paused" : "playing");

		if (rate == aamp->rate)
		{ // no change in desired play rate
			if (aamp->pipeline_paused && rate != 0)
			{ // but need to unpause pipeline
				AAMPLOG_INFO("Resuming Playback at Position '%lld'.", aamp->GetPositionMilliseconds());
				// check if unpausing in the middle of fragments caching
				if(!aamp->SetStateBufferingIfRequired())
				{
					aamp->mpStreamAbstractionAAMP->NotifyPlaybackPaused(false);
					retValue = aamp->mStreamSink->Pause(false, false);
					aamp->NotifyFirstBufferProcessed(); //required since buffers are already cached in paused state
				}
				aamp->pipeline_paused = false;
				aamp->ResumeDownloads();
			}
		}
		else if (rate == 0)
		{
			if (!aamp->pipeline_paused)
			{
				AAMPLOG_INFO("Pausing Playback at Position '%lld'.", aamp->GetPositionMilliseconds());
				aamp->mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
				aamp->StopDownloads();
				retValue = aamp->mStreamSink->Pause(true, false);
				aamp->pipeline_paused = true;
			}
		}
		else
		{
			aamp->rate = rate;
			aamp->pipeline_paused = false;
			aamp->ResumeDownloads();
			aamp->TuneHelper(eTUNETYPE_SEEK); // this unpauses pipeline as side effect
		}

		if(retValue)
		{
			// Do not update state if fragments caching is ongoing and pipeline not paused,
			// target state will be updated once caching completed
			aamp->NotifySpeedChanged(aamp->pipeline_paused ? 0 : aamp->rate,
					(!aamp->IsFragmentCachingRequired() || aamp->pipeline_paused));
		}
	}
	else
	{
		aamp->mSetOnTuneRateRequested = rate;
		AAMPLOG_WARN("%s:%d aamp_SetRate rate[%d] - mpStreamAbstractionAAMP[%p] state[%d]", __FUNCTION__, __LINE__, aamp->rate, aamp->mpStreamAbstractionAAMP, state);
	}
}

/**
 *   @brief Seek to a time.
 *
 *   @param  ptr - Aamp  instance,
 *           Return true on success
 */
static gboolean SeekAfterPrepared(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP*) ptr;
	bool sentSpeedChangedEv = false;
	bool isSeekToLive = false;
	TuneType tuneType = eTUNETYPE_SEEK;
	PrivAAMPState state;
        aamp->GetState(state);
        if( state == eSTATE_ERROR){
                logprintf("%s() operation is not allowed when player in eSTATE_ERROR state !", __FUNCTION__ );\
                return false;
        }

	if (AAMP_SEEK_TO_LIVE_POSITION == aamp->seek_pos_seconds )
	{
		isSeekToLive = true;
		tuneType = eTUNETYPE_SEEKTOLIVE;
	}

	logprintf("aamp_Seek(%f) and seekToLive(%d)", aamp->seek_pos_seconds, isSeekToLive);

	if (isSeekToLive && !aamp->IsLive())
	{
		logprintf("%s:%d - Not live, skipping seekToLive",__FUNCTION__,__LINE__);
		return false;
	}

	if (aamp->IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint())
	{
		double currPositionSecs = aamp->GetPositionMilliseconds() / 1000.00;
		if (isSeekToLive || aamp->seek_pos_seconds >= currPositionSecs)
		{
			logprintf("%s():Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", __FUNCTION__, aamp->seek_pos_seconds, currPositionSecs, isSeekToLive);
			aamp->NotifyOnEnteringLive();
			return false;
		}
	}

	if (aamp->pipeline_paused)
	{
		// resume downloads and clear paused flag. state change will be done
		// on streamSink configuration.
		logprintf("%s(): paused state, so resume downloads", __FUNCTION__);
		aamp->pipeline_paused = false;
		aamp->ResumeDownloads();
		sentSpeedChangedEv = true;
	}

	if (tuneType == eTUNETYPE_SEEK)
	{
		logprintf("%s(): tune type is SEEK", __FUNCTION__);
	}
	if (aamp->rate != AAMP_NORMAL_PLAY_RATE)
	{
		aamp->rate = AAMP_NORMAL_PLAY_RATE;
		sentSpeedChangedEv = true;
	}
	if (aamp->mpStreamAbstractionAAMP)
	{ // for seek while streaming
		aamp->SetState(eSTATE_SEEKING);
		aamp->TuneHelper(tuneType);
		if (sentSpeedChangedEv)
		{
			aamp->NotifySpeedChanged(aamp->rate, false);
		}
	}
	return true;
}

/**
 *   @brief Seek to a time.
 *
 *   @param  secondsRelativeToTuneTime - Seek position for VOD,
 *           relative position from first tune command.
 *   @param  keepPaused - set true if want to keep paused state after seek
 */
void PlayerInstanceAAMP::Seek(double secondsRelativeToTuneTime, bool keepPaused)
{
	bool sentSpeedChangedEv = false;
	bool isSeekToLive = false;
	TuneType tuneType = eTUNETYPE_SEEK;

	ERROR_STATE_CHECK_VOID();

	if ((aamp->mMediaFormat == eMEDIAFORMAT_HLS || aamp->mMediaFormat == eMEDIAFORMAT_HLS_MP4) && (eSTATE_INITIALIZING == state)  && aamp->mpStreamAbstractionAAMP)
	{
		logprintf("Seeking to %lf at the middle of tune, no fragments downloaded yet.", secondsRelativeToTuneTime);
		aamp->mpStreamAbstractionAAMP->SeekPosUpdate(secondsRelativeToTuneTime);
	}
	else if (eSTATE_INITIALIZED == state || eSTATE_PREPARING == state)
	{
		logprintf("Seek will be called after preparing the content.");
		aamp->seek_pos_seconds = secondsRelativeToTuneTime ;
		g_idle_add(SeekAfterPrepared, (gpointer)aamp);
	}
	else
	{
		if (secondsRelativeToTuneTime == AAMP_SEEK_TO_LIVE_POSITION)
		{
			isSeekToLive = true;
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}

		logprintf("aamp_Seek(%f) and seekToLive(%d)", secondsRelativeToTuneTime, isSeekToLive);

		if (isSeekToLive && !aamp->IsLive())
		{
			logprintf("%s:%d - Not live, skipping seekToLive",__FUNCTION__,__LINE__);
			return;
		}

		if (aamp->IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint())
		{
			double currPositionSecs = aamp->GetPositionMilliseconds() / 1000.00;
			if (isSeekToLive || secondsRelativeToTuneTime >= currPositionSecs)
			{
				logprintf("%s():Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", __FUNCTION__, secondsRelativeToTuneTime, currPositionSecs, isSeekToLive);
				aamp->NotifyOnEnteringLive();
				return;
			}
		}

		bool seekWhilePause = false;
		// For autoplay false, pipeline_paused will be true, which denotes a non-playing state
		// as the GST pipeline is not yet created, avoid setting pipeline_paused to false here
		// which might mess up future SetRate call for BG->FG
		if (aamp->mbPlayEnabled && aamp->pipeline_paused)
		{
			// resume downloads and clear paused flag. state change will be done
			// on streamSink configuration.
			logprintf("%s(): paused state, so resume downloads", __FUNCTION__);
			aamp->pipeline_paused = false;
			aamp->ResumeDownloads();
			sentSpeedChangedEv = true;

			if(keepPaused && aamp->mMediaFormat != eMEDIAFORMAT_PROGRESSIVE)
			{
				// Enable seek while paused if not Progressive stream
				seekWhilePause = true;
			}
		}

		if (tuneType == eTUNETYPE_SEEK)
		{
			aamp->seek_pos_seconds = secondsRelativeToTuneTime;
		}
		if (aamp->rate != AAMP_NORMAL_PLAY_RATE)
		{
			aamp->rate = AAMP_NORMAL_PLAY_RATE;
			sentSpeedChangedEv = true;
		}
		if (aamp->mpStreamAbstractionAAMP)
		{ // for seek while streaming
			aamp->SetState(eSTATE_SEEKING);
			aamp->TuneHelper(tuneType, seekWhilePause);
			if (sentSpeedChangedEv && (!seekWhilePause) )
			{
				aamp->NotifySpeedChanged(aamp->rate, false);
			}
		}
	}
}

/**
 *   @brief Seek to live point.
 *
 *   @param[in]  keepPaused - set true if want to keep paused state after seek
 */
void PlayerInstanceAAMP::SeekToLive(bool keepPaused)
{
	Seek(AAMP_SEEK_TO_LIVE_POSITION, keepPaused);
}

/**
 *   @brief Seek to a time and playback with a new rate.
 *
 *   @param  rate - Rate of playback.
 *   @param  secondsRelativeToTuneTime - Seek position for VOD,
 *           relative position from first tune command.
 */
void PlayerInstanceAAMP::SetRateAndSeek(int rate, double secondsRelativeToTuneTime)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	logprintf("aamp_SetRateAndSeek(%d)(%f)", rate, secondsRelativeToTuneTime);
	aamp->TeardownStream(false);
	aamp->seek_pos_seconds = secondsRelativeToTuneTime;
	aamp->rate = rate;
	aamp->TuneHelper(eTUNETYPE_SEEK);
}

/**
 *   @brief Set video rectangle.
 *
 *   @param  x - horizontal start position.
 *   @param  y - vertical start position.
 *   @param  w - width.
 *   @param  h - height.
 */
void PlayerInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetVideoRectangle(x, y, w, h);
}

/**
 *   @brief Set video zoom.
 *
 *   @param  zoom - zoom mode.
 */
void PlayerInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
	ERROR_STATE_CHECK_VOID();
	aamp->zoom_mode = zoom;
	if (aamp->mpStreamAbstractionAAMP ){
		aamp->SetVideoZoom(zoom);
	}else{
		AAMPLOG_WARN("%s:%d Player is in state (%s) , value has been cached",
		__FUNCTION__, __LINE__, "eSTATE_IDLE");
	}
}

/**
 *   @brief Enable/ Disable Video.
 *
 *   @param  muted - true to disable video, false to enable video.
 */
void PlayerInstanceAAMP::SetVideoMute(bool muted)
{
	ERROR_STATE_CHECK_VOID();
	aamp->video_muted = muted;
	if (aamp->mpStreamAbstractionAAMP)
	{
		aamp->SetVideoMute(muted);
	}
	else
	{
		AAMPLOG_WARN("%s:%d Player is in state eSTATE_IDLE, value has been cached", __FUNCTION__, __LINE__);
	}
}

/**
 *   @brief Set Audio Volume.
 *
 *   @param  volume - Minimum 0, maximum 100.
 */
void PlayerInstanceAAMP::SetAudioVolume(int volume)
{
	ERROR_STATE_CHECK_VOID();
	if (volume < AAMP_MINIMUM_AUDIO_LEVEL || volume > AAMP_MAXIMUM_AUDIO_LEVEL)
	{
		AAMPLOG_WARN("%s:%d Audio level (%d) is outside the range supported.. discarding it..",
		__FUNCTION__, __LINE__, volume);
	}else{
		aamp->audio_volume = volume;
		if (aamp->mpStreamAbstractionAAMP)
		{
			aamp->SetAudioVolume(volume);
		}
		else
		{
			AAMPLOG_WARN("%s:%d Player is in state eSTATE_IDLE, value has been cached", __FUNCTION__, __LINE__);
		}
	}
}

/**
 *   @brief Set Audio language.
 *
 *   @param  language - Language of audio track.
 */
void PlayerInstanceAAMP::SetLanguage(const char* language)
{
	ERROR_STATE_CHECK_VOID();

	logprintf("aamp_SetLanguage(%s)->(%s)",aamp->language, language);
	if(strncmp(language, aamp->language, MAX_LANGUAGE_TAG_LENGTH) == 0)
	{
	    aamp->noExplicitUserLanguageSelection = false;
	    aamp->languageSetByUser = true;
	    return;
	}
	// There is no active playback session, save the language for later
	if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
	{
		aamp->languageSetByUser = true;
		aamp->UpdateAudioLanguageSelection(language);
	}
	// check if language is supported in manifest languagelist
	else if((aamp->IsAudioLanguageSupported(language)) || (!aamp->mMaxLanguageCount))
	{
		aamp->languageSetByUser = true;
		aamp->UpdateAudioLanguageSelection(language);
		if (aamp->mpStreamAbstractionAAMP)
		{
			logprintf("aamp_SetLanguage(%s) retuning", language);
			if(aamp->mMediaFormat == eMEDIAFORMAT_OTA)
			{
				aamp->mpStreamAbstractionAAMP->SetAudioTrackByLanguage(language);
			}
			else
			{
				aamp->discardEnteringLiveEvt = true;

				aamp->seek_pos_seconds = aamp->GetPositionMilliseconds()/1000.0;
				aamp->TeardownStream(false);
				// Before calling TuneHelper, ensure player is not in Error state
				ERROR_STATE_CHECK_VOID();
				aamp->TuneHelper(eTUNETYPE_SEEK);

				aamp->discardEnteringLiveEvt = false;
			}
		}
	}
	else
	{
		logprintf("aamp_SetLanguage(%s) not supported in manifest", language);
	}
}

/**
 *   @brief Set array of subscribed tags.
 *
 *   @param  subscribedTags - Array of subscribed tags.
 */
void PlayerInstanceAAMP::SetSubscribedTags(std::vector<std::string> subscribedTags)
{
	ERROR_STATE_CHECK_VOID();

	aamp->subscribedTags = subscribedTags;

	for (int i=0; i < aamp->subscribedTags.size(); i++) {
	        logprintf("    subscribedTags[%d] = '%s'", i, subscribedTags.at(i).data());
	}
}

#ifdef SUPPORT_JS_EVENTS 
/**
 *   @brief Load AAMP JS object in the specified JS context.
 *
 *   @param  context - JS context.
 */
void PlayerInstanceAAMP::LoadJS(void* context)
{
	logprintf("[AAMP_JS] %s(%p)", __FUNCTION__, context);
	if (mJSBinding_DL) {
		void(*loadJS)(void*, void*);
		const char* szLoadJS = "aamp_LoadJS";
		loadJS = (void(*)(void*, void*))dlsym(mJSBinding_DL, szLoadJS);
		if (loadJS) {
			logprintf("[AAMP_JS] %s() dlsym(%p, \"%s\")=%p", __FUNCTION__, mJSBinding_DL, szLoadJS, loadJS);
			loadJS(context, this);
		}
	}
}

/**
 *   @brief Unoad AAMP JS object in the specified JS context.
 *
 *   @param  context - JS context.
 */
void PlayerInstanceAAMP::UnloadJS(void* context)
{
	logprintf("[AAMP_JS] %s(%p)", __FUNCTION__, context);
	if (mJSBinding_DL) {
		void(*unloadJS)(void*);
		const char* szUnloadJS = "aamp_UnloadJS";
		unloadJS = (void(*)(void*))dlsym(mJSBinding_DL, szUnloadJS);
		if (unloadJS) {
			logprintf("[AAMP_JS] %s() dlsym(%p, \"%s\")=%p", __FUNCTION__, mJSBinding_DL, szUnloadJS, unloadJS);
			unloadJS(context);
		}
	}
}
#endif

/**
 *   @brief Support multiple listeners for multiple event type
 *
 *   @param  eventType - type of event.
 *   @param  eventListener - listener for the eventType.
 */
void PlayerInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	aamp->AddEventListener(eventType, eventListener);
}

/**
 *   @brief Remove event listener for eventType.
 *
 *   @param  eventType - type of event.
 *   @param  eventListener - listener to be removed for the eventType.
 */
void PlayerInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	aamp->RemoveEventListener(eventType, eventListener);
}

/**
 *   @brief To check playlist type.
 *
 *   @return bool - True if live content, false otherwise
 */
bool PlayerInstanceAAMP::IsLive()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(false);
	return aamp->IsLive();
}

/**
 *   @brief Get current audio language.
 *
 *   @return current audio language
 */
const char* PlayerInstanceAAMP::GetCurrentAudioLanguage(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL("");
	return aamp->language;
}

/**
 *   @brief Get current drm
 *
 *   @return current drm
 */
const char* PlayerInstanceAAMP::GetCurrentDRM(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL("");
	std::shared_ptr<AampDrmHelper> helper = aamp->GetCurrentDRM();
	if (helper) 
	{
		return helper->friendlyName().c_str();
	}
	return "NONE";
}

/**
 *   @brief Add/Remove a custom HTTP header and value.
 *
 *   @param  headerName - Name of custom HTTP header
 *   @param  headerValue - Value to be passed along with HTTP header.
 *   @param  isLicenseHeader - true if header is for license request
 */
void PlayerInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
	ERROR_STATE_CHECK_VOID();
	aamp->AddCustomHTTPHeader(headerName, headerValue, isLicenseHeader);
}

/**
 *   @brief Set License Server URL.
 *
 *   @param  url - URL of the server to be used for license requests
 *   @param  type - DRM Type(PR/WV) for which the server URL should be used, global by default
 */
void PlayerInstanceAAMP::SetLicenseServerURL(const char *url, DRMSystems type)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetLicenseServerURL(url, type);
}

/**
 *   @brief Indicates if session token has to be used with license request or not.
 *
 *   @param  isAnonymous - True if session token should be blank and false otherwise.
 */
void PlayerInstanceAAMP::SetAnonymousRequest(bool isAnonymous)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AnonymousLicenseRequest,isAnonymous);
}

/**
 *   @brief Indicates average BW to be used for ABR Profiling.
 *
 *   @param  useAvgBW - Flag for true / false
 */
void PlayerInstanceAAMP::SetAvgBWForABR(bool useAvgBW)
{
	aamp->SetAvgBWForABR(useAvgBW);
}

/**
*   @brief SetPreCacheTimeWindow Function to Set PreCache Time
*
*   @param  Time in minutes - Max PreCache Time 
*/
void PlayerInstanceAAMP::SetPreCacheTimeWindow(int nTimeWindow)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetPreCacheTimeWindow(nTimeWindow);
}

/**
 *   @brief Set VOD Trickplay FPS.
 *
 *   @param  vodTrickplayFPS - FPS to be used for VOD Trickplay
 */
void PlayerInstanceAAMP::SetVODTrickplayFPS(int vodTrickplayFPS)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetVODTrickplayFPS(vodTrickplayFPS);
}

/**
 *   @brief Set Linear Trickplay FPS.
 *
 *   @param  linearTrickplayFPS - FPS to be used for Linear Trickplay
 */
void PlayerInstanceAAMP::SetLinearTrickplayFPS(int linearTrickplayFPS)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetLinearTrickplayFPS(linearTrickplayFPS);
}

/**
 *   @brief Set Live Offset.
 *
 *   @param  liveoffset- Live Offset
 */
void PlayerInstanceAAMP::SetLiveOffset(int liveoffset)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetLiveOffset(liveoffset);
}

/**
 *   @brief To set the error code to be used for playback stalled error.
 *
 *   @param  errorCode - error code for playback stall errors.
 */
void PlayerInstanceAAMP::SetStallErrorCode(int errorCode)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetStallErrorCode(errorCode);
}

/**
 *   @brief To set the timeout value to be used for playback stall detection.
 *
 *   @param  timeoutMS - timeout in milliseconds for playback stall detection.
 */
void PlayerInstanceAAMP::SetStallTimeout(int timeoutMS)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetStallTimeout(timeoutMS);
}

/**
 *   @brief Set report interval duration
 *
 *   @param  reportIntervalMS - report interval duration in MS
 */
void PlayerInstanceAAMP::SetReportInterval(int reportIntervalMS)
{
	ERROR_STATE_CHECK_VOID();
	if(reportIntervalMS > 0)
	{
		aamp->SetReportInterval(reportIntervalMS);
	}
}

/**
 *   @brief To set the max retry attempts for init frag curl timeout failures
 *
 *   @param  count - max attempt for timeout retry count
 */
void PlayerInstanceAAMP::SetInitFragTimeoutRetryCount(int count)
{
	if(count >= 0)
	{
		aamp->SetInitFragTimeoutRetryCount(count);
	}
}

/**
 *   @brief To get the current playback position.
 *
 *   @ret current playback position in seconds
 */
double PlayerInstanceAAMP::GetPlaybackPosition()
{
	ERROR_STATE_CHECK_VAL(0.00);
	return (aamp->GetPositionMilliseconds() / 1000.00);
}

/**
*   @brief To get the current asset's duration.
*
*   @ret duration in seconds
*/
double PlayerInstanceAAMP::GetPlaybackDuration()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0.00);
	return (aamp->GetDurationMs() / 1000.00);
}

/**
 *   @brief To get the current AAMP state.
 *
 *   @ret current AAMP state
 */
PrivAAMPState PlayerInstanceAAMP::GetState(void)
{
	PrivAAMPState currentState;
	aamp->GetState(currentState);
	return currentState;
}

/**
 *   @brief To get the bitrate of current video profile.
 *
 *   @ret bitrate of video profile
 */
long PlayerInstanceAAMP::GetVideoBitrate(void)
{
	long bitrate = 0;
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrate = aamp->mpStreamAbstractionAAMP->GetVideoBitrate();
	}
	return bitrate;
}

/**
 *   @brief To set a preferred bitrate for video profile.
 *
 *   @param[in] preferred bitrate for video profile
 */
void PlayerInstanceAAMP::SetVideoBitrate(long bitrate)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->SetVideoBitrate(bitrate);
}

/**
 *   @brief To get the bitrate of current audio profile.
 *
 *   @ret bitrate of audio profile
 */
long PlayerInstanceAAMP::GetAudioBitrate(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	return aamp->mpStreamAbstractionAAMP->GetAudioBitrate();
}

/**
 *   @brief To set a preferred bitrate for audio profile.
 *
 *   @param[in] preferred bitrate for audio profile
 */
void PlayerInstanceAAMP::SetAudioBitrate(long bitrate)
{
	//no-op for now
}

/**
 *   @brief To get the current audio volume.
 *
 *   @ret audio volume
 */
int PlayerInstanceAAMP::GetAudioVolume(void)
{
	ERROR_STATE_CHECK_VAL(0);
	if (eSTATE_IDLE == state) 
	{
		AAMPLOG_WARN("%s:%d GetAudioVolume is returning cached value since player is at %s",
		__FUNCTION__, __LINE__,"eSTATE_IDLE");
	}
	return aamp->audio_volume;
}

/**
 *   @brief To get the current playback rate.
 *
 *   @ret current playback rate
 */
int PlayerInstanceAAMP::GetPlaybackRate(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	return (aamp->pipeline_paused ? 0 : aamp->rate);
}

/**
 *   @brief To get the available video bitrates.
 *
 *   @ret available video bitrates
 */
std::vector<long> PlayerInstanceAAMP::GetVideoBitrates(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::vector<long>());
	return aamp->mpStreamAbstractionAAMP->GetVideoBitrates();
}

/**
 *   @brief To get the available audio bitrates.
 *
 *   @ret available audio bitrates
 */
std::vector<long> PlayerInstanceAAMP::GetAudioBitrates(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::vector<long>());
	return aamp->mpStreamAbstractionAAMP->GetAudioBitrates();
}

/**
 *   @brief To set the initial bitrate value.
 *
 *   @param[in] initial bitrate to be selected
 */
void PlayerInstanceAAMP::SetInitialBitrate(long bitrate)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetInitialBitrate(bitrate);
	
}

/**
 *   @brief To set the initial bitrate value for 4K assets.
 *
 *   @param[in] initial bitrate to be selected for 4K assets
 */
void PlayerInstanceAAMP::SetInitialBitrate4K(long bitrate4K)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetInitialBitrate4K(bitrate4K);
}

/**
 *   @brief To set the network download timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetNetworkTimeout(double timeout)
{
        ERROR_STATE_CHECK_VOID();
        aamp->SetNetworkTimeout(timeout);
}

/**
 *   @brief To set the manifest download timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetManifestTimeout(double timeout)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetManifestTimeout(timeout);
}

/**
 *   @brief To set the playlist download timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetPlaylistTimeout(double timeout)
{
        ERROR_STATE_CHECK_VOID();
        aamp->SetPlaylistTimeout(timeout);
}

/**
 *   @brief To set the download buffer size value
 *
 *   @param[in] preferred download buffer size
 */
void PlayerInstanceAAMP::SetDownloadBufferSize(int bufferSize)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetDownloadBufferSize(bufferSize);
}

/**
 *   @brief Set preferred DRM.
 *
 *   @param[in] drmType - preferred DRM type
 */
void PlayerInstanceAAMP::SetPreferredDRM(DRMSystems drmType)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetPreferredDRM(drmType);
}

/**
 *   @brief Set Stereo Only Playback.
 */
void PlayerInstanceAAMP::SetStereoOnlyPlayback(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetStereoOnlyPlayback(bValue);
}

/**
 *   @brief Set BulkTimedMetadata Reporting flag
 */
void PlayerInstanceAAMP::SetBulkTimedMetaReport(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetBulkTimedMetaReport(bValue);
}

/**
 *   @brief Set unpaired discontinuity retune flag
 */
void PlayerInstanceAAMP::SetRetuneForUnpairedDiscontinuity(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetRetuneForUnpairedDiscontinuity(bValue);
}

/**
 *   @brief Set retune configuration for gstpipeline internal data stream error.
 */
void PlayerInstanceAAMP::SetRetuneForGSTInternalError(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetRetuneForGSTInternalError(bValue);
}

/**
 *   @brief Setting the alternate contents' (Ads/blackouts) URL.
 *
 *   @param[in] Adbreak's unique identifier.
 *   @param[in] Individual Ad's id
 *   @param[in] Ad URL
 */
void PlayerInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->SetAlternateContents(adBreakId, adId, url);
}

/**
 *   @brief To set the network proxy
 *
 *   @param[in] network proxy to use
 */
void PlayerInstanceAAMP::SetNetworkProxy(const char * proxy)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetNetworkProxy(proxy);
}

/**
 *   @brief To set the proxy for license request
 *
 *   @param[in] proxy to use for license request
 */
void PlayerInstanceAAMP::SetLicenseReqProxy(const char * licenseProxy)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetLicenseReqProxy(licenseProxy);
}

/**
 *   @brief To set the curl stall timeout value
 *
 *   @param[in] curl stall timeout
 */
void PlayerInstanceAAMP::SetDownloadStallTimeout(long stallTimeout)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetDownloadStallTimeout(stallTimeout);
}

/**
 *   @brief To set the curl download start timeout value
 *
 *   @param[in] curl download start timeout
 */
void PlayerInstanceAAMP::SetDownloadStartTimeout(long startTimeout)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetDownloadStartTimeout(startTimeout);
}

/**
 *   @brief Set preferred subtitle language.
 *
 *   @param[in]  language - Language of text track.
 *   @return void
 */
void PlayerInstanceAAMP::SetPreferredSubtitleLanguage(const char* language)
{
	ERROR_STATE_CHECK_VOID();
        AAMPLOG_WARN("PlayerInstanceAAMP::%s():%d (%s)->(%s)", __FUNCTION__, __LINE__, aamp->mSubLanguage, language);

	if (strncmp(language, aamp->mSubLanguage, MAX_LANGUAGE_TAG_LENGTH) == 0)
		return;

	
	if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
	{
		AAMPLOG_WARN("PlayerInstanceAAMP::%s():%d \"%s\" language set prior to tune start", __FUNCTION__, __LINE__, language);
	}
	else
	{
		AAMPLOG_WARN("PlayerInstanceAAMP::%s():%d \"%s\" language set - will take effect on next tune", __FUNCTION__, __LINE__, language);
	}
	aamp->UpdateSubtitleLanguageSelection(language);
}

/**
 *   @brief Set parallel playlist download config value.
 *   @param[in] bValue - true if a/v playlist to be downloaded in parallel
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetParallelPlaylistDL(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetParallelPlaylistDL(bValue);
}

/**
 *   @brief Set parallel playlist download config value for linear.
 *   @param[in] bValue - true if a/v playlist to be downloaded in parallel during refresh
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetParallelPlaylistRefresh(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetParallelPlaylistRefresh(bValue);
}

/**
 *   @brief Get Async Tune configuration
 *
 *   @return bool - true if config set
 */
bool PlayerInstanceAAMP::GetAsyncTuneConfig()
{
	return aamp->GetAsyncTuneConfig();
}

/**
 *   @brief Set Westeros sink Configuration
 *   @param[in] bValue - true if westeros sink enabled
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetWesterosSinkConfig(bool bValue)
{
	aamp->SetWesterosSinkConfig(bValue);
}

/**
 *   @brief Set license caching
 *   @param[in] bValue - true/false to enable/disable license caching
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetLicenseCaching(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetLicenseCaching(bValue);
}

/**
 *   @brief Set Matching BaseUrl Config Configuration
 *
 *   @param[in] bValue - true if Matching BaseUrl enabled
 *   @return void
 */
void PlayerInstanceAAMP::SetMatchingBaseUrlConfig(bool bValue)
{
	aamp->SetMatchingBaseUrlConfig(bValue);
}

/**
 *   @brief Configure New ABR Enable/Disable
 *   @param[in] bValue - true if new ABR enabled
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetNewABRConfig(bool bValue)
{
	aamp->SetNewABRConfig(bValue);
}

/**
 *   @brief Configure URI  parameters
 *   @param[in] bValue -true to enable
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetPropagateUriParameters(bool bValue)
{
        aamp->SetPropagateUriParameters(bValue);
}

/**
 *   @brief Configure URI  parameters
 *   @param[in] bValue -true to enable
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetSslVerifyPeerConfig(bool bValue)
{
        aamp->SetSslVerifyPeerConfig(bValue);
}

/**
 *   @brief Set optional preferred language list
 *   @param[in] languageList - string with comma-delimited language list in ISO-639
 *             from most to least preferred. Set NULL to clear current list.
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetPreferredLanguages(const char *languageList)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();

	aamp->preferredLanguagesString.clear();
	aamp->preferredLanguagesList.clear();

	if(languageList != NULL)
	{
		aamp->preferredLanguagesString = std::string(languageList);
		std::istringstream ss(aamp->preferredLanguagesString);
		std::string lng;
		while(std::getline(ss, lng, ','))
		{
			aamp->preferredLanguagesList.push_back(lng);
			AAMPLOG_INFO("%s:%d: Parsed preferred lang: %s", __FUNCTION__, __LINE__,
					lng.c_str());
		}

		aamp->preferredLanguagesString = std::string(languageList);

		// If user has not yet called SetLanguage(), force to use
		// preferred languages over default language
		if(!aamp->languageSetByUser)
			aamp->noExplicitUserLanguageSelection = true;
	}

	AAMPLOG_INFO("%s:%d: Number of preferred languages: %d", __FUNCTION__, __LINE__,
			aamp->preferredLanguagesList.size());
}

/**
 *	 @brief Get Preferred DRM.
 *
 *	 @return Preferred DRM type
 */
DRMSystems PlayerInstanceAAMP::GetPreferredDRM()
{
	return aamp->GetPreferredDRM();
}

/**
 *   @brief Get current preferred language list
 *
 *   @return  const char* - current comma-delimited language list or NULL if not set
 *
 */
const char* PlayerInstanceAAMP::GetPreferredLanguages()
{
	if(!aamp->preferredLanguagesString.empty())
	{
		return aamp->preferredLanguagesString.c_str();
	}

	return NULL;
}

/**
 *   @brief Configure New AdBreaker Enable/Disable
 *   @param[in] bValue - true if new AdBreaker enabled
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetNewAdBreakerConfig(bool bValue)
{
	aamp->SetNewAdBreakerConfig(bValue);
}

/**
 *   @brief Get available audio tracks.
 *
 *   @return std::string JSON formatted list of audio tracks
 */
std::string PlayerInstanceAAMP::GetAvailableAudioTracks()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetAvailableAudioTracks();
}

/**
 *   @brief Get available text tracks.
 *
 *   @return std::string JSON formatted list of text tracks
 */
std::string PlayerInstanceAAMP::GetAvailableTextTracks()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetAvailableTextTracks();
}

/*
 *   @brief Get the video window co-ordinates
 *
 *   @return current video co-ordinates in x,y,w,h format
 */
std::string PlayerInstanceAAMP::GetVideoRectangle()
{
	ERROR_STATE_CHECK_VAL(std::string());

	return aamp->GetVideoRectangle();
}

/*
 *   @brief Set the application name which has created PlayerInstanceAAMP, for logging purposes
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetAppName(std::string name)
{
	aamp->SetAppName(name);
}

/**
 *   @brief Enable/disable the native CC rendering feature
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetNativeCCRendering(bool enable)
{
#ifdef AAMP_CC_ENABLED
	gpGlobalConfig->nativeCCRendering = enable;
#endif
}

/**
 *   @brief To set the vod-tune-event according to the player.
 *
 *   @param[in] preferred tune event type
 */
void PlayerInstanceAAMP::SetTuneEventConfig(int tuneEventType)
{
	aamp->SetTuneEventConfig(static_cast<TunedEventConfig> (tuneEventType));
}

/**
 *   @brief Set video rectangle property
 *
 *   @param[in] video rectangle property
 */
void PlayerInstanceAAMP::EnableVideoRectangle(bool rectProperty)
{
	aamp->EnableVideoRectangle(rectProperty);
}

/**
 *   @brief Set audio track
 *
 *   @param[in] trackId index of audio track in available track list
 *   @return void
 */
void PlayerInstanceAAMP::SetAudioTrack(int trackId)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();

	aamp->SetAudioTrack(trackId);
}

/**
 *   @brief Get current audio track index
 *
 *   @return int - index of current audio track in available track list
 */
int PlayerInstanceAAMP::GetAudioTrack()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(-1);

	return aamp->GetAudioTrack();
}

/**
 *   @brief Set text track
 *
 *   @param[in] trackId index of text track in available track list
 *   @return void
 */
void PlayerInstanceAAMP::SetTextTrack(int trackId)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();

	aamp->SetTextTrack(trackId);
}

/**
 *   @brief Get current text track index
 *
 *   @return int - index of current text track in available track list
 */
int PlayerInstanceAAMP::GetTextTrack()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(-1);

	return aamp->GetTextTrack();
}

/**
 *   @brief Set CC visibility on/off
 *
 *   @param[in] enabled true for CC on, false otherwise
 *   @return void
 */
void PlayerInstanceAAMP::SetCCStatus(bool enabled)
{
	ERROR_STATE_CHECK_VOID();

	aamp->SetCCStatus(enabled);
}

/**
 *   @brief Set style options for text track rendering
 *
 *   @param[in] options - JSON formatted style options
 *   @return void
 */
void PlayerInstanceAAMP::SetTextStyle(const std::string &options)
{
	ERROR_STATE_CHECK_VOID();

	aamp->SetTextStyle(options);
}

/**
 *   @brief Get style options for text track rendering
 *
 *   @return std::string - JSON formatted style options
 */
std::string PlayerInstanceAAMP::GetTextStyle()
{
	ERROR_STATE_CHECK_VAL(std::string());

	return aamp->GetTextStyle();
}

/**
 * @brief Set profile ramp down limit.
 *
 */
void PlayerInstanceAAMP::SetInitRampdownLimit(int limit)
{
	aamp->SetInitRampdownLimit(limit);
}

/**
 *   @brief Set the CEA format for force setting
 *
 *   @param[in] format - 0 for 608, 1 for 708
 *   @return void
 */
void PlayerInstanceAAMP::SetCEAFormat(int format)
{
#ifdef AAMP_CC_ENABLED
	if (format == eCLOSEDCAPTION_FORMAT_608)
	{
		gpGlobalConfig->preferredCEA708 = eFalseState;
	}
	else if (format == eCLOSEDCAPTION_FORMAT_708)
	{
		gpGlobalConfig->preferredCEA708 = eTrueState;
	}
#endif
}

/**
*   @brief To get the bitrate of thumbnail profile.
*
*   @ret bitrate of thumbnail tracks profile
*/
std::string PlayerInstanceAAMP::GetAvailableThumbnailTracks(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());
	return aamp->GetThumbnailTracks();
}

/**
 *   @brief To set a preferred bitrate for thumbnail profile.
 *
 *   @param[in] preferred bitrate for thumbnail profile
 */
bool PlayerInstanceAAMP::SetThumbnailTrack(int thumbIndex)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(false);
	if(thumbIndex < 0 || !aamp->mpStreamAbstractionAAMP)
	{
		return false;
	}
	return aamp->mpStreamAbstractionAAMP->SetThumbnailTrack(thumbIndex);
}

/**
 *   @brief To get preferred thumbnails for the duration.
 *
 *   @param[in] duration  for thumbnails
 */
std::string PlayerInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());
	return aamp->GetThumbnails(tStart, tEnd);
}

/**
 *   @brief Set the session Token for player
 *
 *   @param[in] string - sessionToken
 *   @return void
 */
void PlayerInstanceAAMP::SetSessionToken(std::string sessionToken)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetSessionToken(sessionToken);
	return;
}

/**
 *   @brief Set auxiliary language
 *
 *   @param[in] language - auxiliary language
 *   @return void
 */
void PlayerInstanceAAMP::SetAuxiliaryLanguage(const std::string &language)
{
	ERROR_STATE_CHECK_VOID();
#ifdef AAMP_AUXILIARY_AUDIO_ENABLED
	//Can set the property only for BT enabled device

	std::string currentLanguage = aamp->GetAuxiliaryAudioLanguage();
	AAMPLOG_WARN("aamp_SetAuxiliaryLanguage(%s)->(%s)", currentLanguage.c_str(), language.c_str());

	if(language != currentLanguage)
	{
		// There is no active playback session, save the language for later
		if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
		{
			aamp->languageSetByUser = true;
			aamp->SetAuxiliaryLanguage(language);
		}
		// check if language is supported in manifest languagelist
		else if((aamp->IsAudioLanguageSupported(language.c_str())) || (!aamp->mMaxLanguageCount))
		{
			aamp->SetAuxiliaryLanguage(language);
			if (aamp->mpStreamAbstractionAAMP)
			{
				AAMPLOG_WARN("aamp_SetAuxiliaryLanguage(%s) retuning", language.c_str());

				aamp->discardEnteringLiveEvt = true;

				aamp->seek_pos_seconds = aamp->GetPositionMilliseconds()/1000.0;
				aamp->TeardownStream(false);
				aamp->TuneHelper(eTUNETYPE_SEEK);

				aamp->discardEnteringLiveEvt = false;
			}
		}
	}
#else
	AAMPLOG_ERR("%s:%d Auxiliary audio language is not supported in this platform, ignoring the input!", __FUNCTION__, __LINE__);
#endif
}

/**
 *   @brief Enable seekable range values in progress event
 *
 *   @param[in] enabled - true if enabled
 */
void PlayerInstanceAAMP::EnableSeekableRange(bool enabled)
{
	ERROR_STATE_CHECK_VOID();
	aamp->EnableSeekableRange(enabled);
}

/**
 *   @brief Enable video PTS reporting in progress event
 *
 *   @param[in] enabled - true if enabled
 */
void PlayerInstanceAAMP::SetReportVideoPTS(bool enabled)
{
	ERROR_STATE_CHECK_VOID();
	aamp->SetReportVideoPTS(enabled);
}

/**
*   @brief Disable Content Restrictions - unlock
*   @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
*   @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
*   @param[in] eventChange - disable restriction handling till next program event boundary
*
*   @return void
*/
void PlayerInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->DisableContentRestrictions(grace, time, eventChange);
}

/**
*   @brief Enable Content Restrictions - lock
*   @return void
*/
void PlayerInstanceAAMP::EnableContentRestrictions()
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->EnableContentRestrictions();
}

/**
 *   @brief Enable async operation and initialize resources
 *
 *   @return void
 */
void PlayerInstanceAAMP::EnableAsyncOperation()
{
	// Check if global configuration is set to false
	// Additional check added here, since this API can be called from jsbindings/native app
	if (gpGlobalConfig->mAsyncTuneConfig != eFalseState && !mAsyncRunning)
	{
		AAMPLOG_WARN("%s:%d Enable async tune operation!!", __FUNCTION__, __LINE__);
		mAsyncRunning = true;
		StartScheduler();
		aamp->SetAsyncTuneConfig(true);
		aamp->SetScheduler(this);
	}
}

/**
 *   @brief Enable/disable configuration to persist ABR profile over SAP/seek
 *
 *   @param[in] value - To enable/disable configuration
 *   @return void
 */
void PlayerInstanceAAMP::PersistBitRateOverSeek(bool value)
{
	ERROR_STATE_CHECK_VOID();
	aamp->PersistBitRateOverSeek(value);
}

/**
 *   @brief Stop playback and release resources.
 *
 *   @param[in]  sendStateChangeEvent - true if state change events need to be sent for Stop operation
 *   @return void
 */
void PlayerInstanceAAMP::StopInternal(bool sendStateChangeEvent)
{
	PrivAAMPState state;
	aamp->GetState(state);

	if(gpGlobalConfig->enableMicroEvents && (eSTATE_ERROR == state) && !(aamp->IsTuneCompleted()))
	{
		/*Sending metrics on tune Error; excluding mid-stream failure cases & aborted tunes*/
		aamp->sendTuneMetrics(false);
	}

	logprintf("PLAYER[%d] aamp_stop PlayerState=%d", aamp->mPlayerId, state);

	if (sendStateChangeEvent)
	{
		aamp->SetState(eSTATE_IDLE);
	}

	AAMPLOG_WARN("%s PLAYER[%d] Stopping Playback at Position '%lld'.\n",(aamp->mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), aamp->mPlayerId, aamp->GetPositionMilliseconds());
	aamp->Stop();
}
/**
 * @}
 */
