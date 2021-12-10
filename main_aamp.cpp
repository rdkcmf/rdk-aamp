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
#ifdef IARM_MGR
#include "host.hpp"
#include "manager.hpp"
#include "libIBus.h"
#include "libIBusDaemon.h"
#include "irMgr.h"
#endif

#include "main_aamp.h"
#include "AampConfig.h"
#include "AampCacheHandler.h"
#include "AampUtils.h"
#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif
#include "helper/AampDrmHelper.h"
#include "StreamAbstractionAAMP.h"
#include "aampgstplayer.h"

#include <dlfcn.h>
#include <termios.h>
#include <errno.h>
#include <regex>

AampConfig *gpGlobalConfig=NULL;
AampLogManager *mLogObj=NULL;
#ifdef USE_SECMANAGER
#include "AampSecManager.h"
#endif

#define ERROR_STATE_CHECK_VOID() \
	PrivAAMPState state = GetState(); \
	if( state == eSTATE_ERROR){ \
		AAMPLOG_WARN("operation is not allowed when player in eSTATE_ERROR state !");\
		return; \
	}

#define ERROR_STATE_CHECK_VAL(val) \
	PrivAAMPState state = GetState(); \
	if( state == eSTATE_ERROR){ \
		AAMPLOG_WARN("operation is not allowed when player in eSTATE_ERROR state !");\
		return val; \
	}

#define ERROR_OR_IDLE_STATE_CHECK_VOID() \
	PrivAAMPState state = GetState(); \
	if( state == eSTATE_ERROR || state == eSTATE_IDLE){ \
		AAMPLOG_WARN("operation is not allowed when player in %s state !",\
		(state == eSTATE_ERROR) ? "eSTATE_ERROR" : "eSTATE_IDLE" );\
		return; \
	}

#define NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID() \
	PrivAAMPState state = GetState(); \
	if( state != eSTATE_IDLE && state != eSTATE_RELEASED){ \
		AAMPLOG_WARN("operation is not allowed when player not in eSTATE_IDLE or eSTATE_RELEASED state !");\
		return; \
	}

#define ERROR_OR_IDLE_STATE_CHECK_VAL(val) \
	PrivAAMPState state = GetState(); \
	if( state == eSTATE_ERROR || state == eSTATE_IDLE){ \
		AAMPLOG_WARN("operation is not allowed in %s state !",\
		(state == eSTATE_ERROR) ? "eSTATE_ERROR" : "eSTATE_IDLE" );\
		return val; \
	}

static bool iarmInitialized = false;
std::mutex PlayerInstanceAAMP::mPrvAampMtx;
/**
 *   @brief Constructor.
 *
 *   @param[in]  streamSink - custom stream sink, NULL for default.
 */
PlayerInstanceAAMP::PlayerInstanceAAMP(StreamSink* streamSink
	, std::function< void(uint8_t *, int, int, int) > exportFrames
	) : aamp(NULL), sp_aamp(nullptr), mInternalStreamSink(NULL), mJSBinding_DL(),mAsyncRunning(false),mConfig()
{

#ifdef IARM_MGR
if(!iarmInitialized)
{
        char processName[20] = {0};
        IARM_Result_t result;
        sprintf (processName, "AAMP-PLAYER-%u", getpid());
        if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Init((const char*) &processName))) {
                AAMPLOG_WARN("IARM Interface Inited in AAMP");
        }
        else {
            AAMPLOG_WARN("IARM Interface Inited Externally : %d", result);
        }

        if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Connect())) {
                AAMPLOG_WARN("IARM Interface Connected  in AAMP");
        }
        else {
            AAMPLOG_WARN ("IARM Interface Connected Externally :%d", result);
        }
	iarmInitialized = true;
        try
        {
        device::Manager::Initialize();
        AAMPLOG_INFO("PlayerInstanceAAMP: Successfully initialized device::Manager");
        }
        catch (...)
        {
        AAMPLOG_ERR("PlayerInstanceAAMP: device::Manager initialization failed");
        }
}
#endif

#ifdef SUPPORT_JS_EVENTS
#ifdef AAMP_WPEWEBKIT_JSBINDINGS //aamp_LoadJS defined in libaampjsbindings.so
	const char* szJSLib = "libaampjsbindings.so";
#else
	const char* szJSLib = "libaamp.so";
#endif
	mJSBinding_DL = dlopen(szJSLib, RTLD_GLOBAL | RTLD_LAZY);
	AAMPLOG_WARN("[AAMP_JS] dlopen(\"%s\")=%p", szJSLib, mJSBinding_DL);
#endif

	// Create very first instance of Aamp Config to read the cfg & Operator file .This is needed for very first
	// tune only . After that every tune will use the same config parameters
	if(gpGlobalConfig == NULL)
	{		
#ifdef AAMP_BUILD_INFO
		std::string tmpstr = MACRO_TO_STRING(AAMP_BUILD_INFO);
		AAMPLOG_WARN(" AAMP_BUILD_INFO: %s",tmpstr.c_str());
#endif
		gpGlobalConfig =  new AampConfig();
		// Init the default values
		gpGlobalConfig->Initialize();
		AAMPLOG_WARN("[AAMP_JS][%p]Creating GlobalConfig Instance[%p]",this,gpGlobalConfig);
		if(!gpGlobalConfig->ReadAampCfgTxtFile())
		{
			gpGlobalConfig->ReadAampCfgJsonFile();
		}
		gpGlobalConfig->ReadOperatorConfiguration();		
		gpGlobalConfig->ShowDevCfgConfiguration();
		gpGlobalConfig->ShowOperatorSetConfiguration();
		::mLogObj = gpGlobalConfig->GetLoggerInstance();
	}

	// Copy the default configuration to session configuration .
	// App can modify the configuration set
	mConfig = *gpGlobalConfig;

	sp_aamp = std::make_shared<PrivateInstanceAAMP>(&mConfig);
	aamp = sp_aamp.get();
	mLogObj = mConfig.GetLoggerInstance();
	mConfig.logging.setPlayerId(aamp->mPlayerId);
	if (NULL == streamSink)
	{
		mInternalStreamSink = new AAMPGstPlayer(mConfig.GetLoggerInstance(), aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
                , exportFrames
#endif
		);

		streamSink = mInternalStreamSink;
	}
	aamp->SetStreamSink(streamSink);
	AsyncStartStop();
}

/**
 * @brief PlayerInstanceAAMP Destructor
 */
PlayerInstanceAAMP::~PlayerInstanceAAMP()
{
	if (aamp)
	{
		PrivAAMPState state;
		aamp->GetState(state);
		if (state != eSTATE_IDLE && state != eSTATE_RELEASED)
		{
			if (mAsyncRunning)
			{
				// Before stop, clear up any remaining commands in queue
				RemoveAllTasks();
				aamp->DisableDownloads();
				AcquireExLock();
			}

			//Avoid stop call since already stopped
			aamp->Stop();

			if (mAsyncRunning)
			{
				//Release lock
				ReleaseExLock();
				EnableScheduleTask();
			}
		}
		std::lock_guard<std::mutex> lock (mPrvAampMtx);
		aamp = NULL;
	}
	SAFE_DELETE(mInternalStreamSink);

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
		AAMPLOG_WARN("[AAMP_JS] dlclose(%p)", mJSBinding_DL);
		dlclose(mJSBinding_DL);
	}
#endif
#ifdef USE_SECMANAGER
	if (isLastPlayerInstance)
	{
		AampSecManager::DestroyInstance();
	}
#endif
	if (isLastPlayerInstance && gpGlobalConfig)
	{
		AAMPLOG_WARN("[%p] Release GlobalConfig(%p)",this,gpGlobalConfig);
		SAFE_DELETE(gpGlobalConfig);
	}
}


/**
 * @brief API to reset configuration across tunes for single player instance
 *
 */
void PlayerInstanceAAMP::ResetConfiguration()
{
	AAMPLOG_WARN("Resetting Configuration to default values ");
	// Copy the default configuration to session configuration .App can modify the configuration set
	mConfig = *gpGlobalConfig;
	// Based on the default condition , reset the AsyncTune scheduler
	AsyncStartStop();
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
		PrivAAMPState state;
		aamp->GetState(state);

		//state will be eSTATE_IDLE or eSTATE_RELEASED, right after an init or post-processing of a Stop call
		if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
		{
			return;
		}

		if (mAsyncRunning)
		{
			// Before stop, clear up any remaining commands in queue
			RemoveAllTasks();
			aamp->DisableDownloads();
			AcquireExLock();
		}

		StopInternal(sendStateChangeEvent);

		if (mAsyncRunning)
		{
			//Release lock
			ReleaseExLock();
			EnableScheduleTask();
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
        bool IsOTAtoOTA =  false;

	if((aamp->IsOTAContent()) && (NULL != mainManifestUrl))
	{
		/* OTA to OTA tune does not need to call stop. */
		std::string urlStr(mainManifestUrl); // for convenience, convert to std::string
		if((urlStr.rfind("live:",0)==0) || (urlStr.rfind("tune:",0)==0))
		{
			IsOTAtoOTA = true;
		}
	}

	if ((state != eSTATE_IDLE) && (state != eSTATE_RELEASED) && (!IsOTAtoOTA))
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
 *   @brief UnRegister event handler.
 *
 *   @param  eventListener - pointer to implementation of EventListener to receive events.
 */
void PlayerInstanceAAMP::UnRegisterEvents(EventListener* eventListener)
{
	aamp->UnRegisterEvents(eventListener);
}

/**
 * @brief Set retry limit on Segment injection failure.
 *
 */
void PlayerInstanceAAMP::SetSegmentInjectFailCount(int value)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SegmentInjectThreshold,value);
}

/**
 * @brief Set retry limit on Segment drm decryption failure.
 *
 */
void PlayerInstanceAAMP::SetSegmentDecryptFailCount(int value)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DRMDecryptThreshold,value);
}

/**
 * @brief Set initial buffer duration in seconds
 *
 */
void PlayerInstanceAAMP::SetInitialBufferDuration(int durationSec)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_InitialBuffer,durationSec);
}

/**
 * @brief Set Maximum Cache Size for playlist store 
 *
 */
void PlayerInstanceAAMP::SetMaxPlaylistCacheSize(int cacheSize)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxPlaylistCacheSize,cacheSize);
}

/**
 * @brief Set profile ramp down limit.
 *
 */
void PlayerInstanceAAMP::SetRampDownLimit(int limit)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RampDownLimit,limit);
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
	//NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID(); // why was this here?
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LanguageCodePreference,(int)preferredFormat);
	if( useRole )
	{
		AAMPLOG_WARN("SetLanguageFormat bDescriptiveAudioTrack deprecated!" );
	}
	//gpGlobalConfig->bDescriptiveAudioTrack = useRole;
}

/**
 * @brief Set minimum bitrate value.
 * @param  url - stream url with vss service zone info as query string
 */
void PlayerInstanceAAMP::SetMinimumBitrate(long bitrate)
{
	if (bitrate > 0)
	{
		AAMPLOG_INFO("Setting minimum bitrate: %ld", bitrate);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MinBitrate,bitrate);
	}
	else
	{
		AAMPLOG_WARN("Invalid bitrate value %ld",  bitrate);
	}

}

/**
 * @brief Set maximum bitrate value.
 *
 */
void PlayerInstanceAAMP::SetMaximumBitrate(long bitrate)
{
	if (bitrate > 0)
	{
		AAMPLOG_INFO("Setting maximum bitrate : %ld", bitrate);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxBitrate,bitrate);
	}
	else
	{
		AAMPLOG_WARN("Invalid bitrate value %d", bitrate);
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
	AAMPLOG_INFO("PLAYER[%d] rate=%d.", aamp->mPlayerId, rate);

	ERROR_STATE_CHECK_VOID();

	if (!IsValidRate(rate))
	{
		AAMPLOG_WARN("SetRate ignored!! Invalid rate (%d)", rate);
		return;
	}
	//Hack For DELIA-51318 convert the incoming rates into acceptable rates
	if(ISCONFIGSET(eAAMPConfig_RepairIframes))
	{
		AAMPLOG_WARN("mRepairIframes is true, setting actual rate %d for the recieved rate %d", getWorkingTrickplayRate(rate), rate);
		rate = getWorkingTrickplayRate(rate);
	}

	if (aamp->mpStreamAbstractionAAMP && !(aamp->mbUsingExternalPlayer))
	{
		if (!aamp->mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0 && aamp->mMediaFormat != eMEDIAFORMAT_PROGRESSIVE)
		{
			AAMPLOG_WARN("Ignoring trickplay. No iframe tracks in stream");
			aamp->NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
			return;
		}
		if(!(aamp->mbPlayEnabled) && aamp->pipeline_paused && (0 != rate) && !aamp->mbDetached)
		{
			AAMPLOG_WARN("PLAYER[%d] Player %s=>%s.", aamp->mPlayerId, STRBGPLAYER, STRFGPLAYER );
			aamp->mbPlayEnabled = true;
			if (AAMP_NORMAL_PLAY_RATE == rate)
			{
				aamp->LogPlayerPreBuffered();
				aamp->mStreamSink->Configure(aamp->mVideoFormat, aamp->mAudioFormat, aamp->mAuxFormat, aamp->mpStreamAbstractionAAMP->GetESChangeStatus(), aamp->mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
				aamp->ResumeDownloads(); //To make sure that the playback resumes after a player switch if player was in paused state before being at background
				aamp->mpStreamAbstractionAAMP->StartInjection();
				aamp->mStreamSink->Stream();
				aamp->pipeline_paused = false;
				return;
			}
		}
		bool retValue = true;
		if (rate > 0 && aamp->IsLive() && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint() && aamp->rate >= AAMP_NORMAL_PLAY_RATE && !aamp->mbDetached)
		{
			AAMPLOG_WARN("Already at logical live point, hence skipping operation");
			aamp->NotifyOnEnteringLive();
			return;
		}

		// DELIA-39691 If input rate is same as current playback rate, skip duplicate operation
		// Additional check for pipeline_paused is because of 0(PAUSED) -> 1(PLAYING), where aamp->rate == 1.0 in PAUSED state
		if ((!aamp->pipeline_paused && rate == aamp->rate && !aamp->GetPauseOnFirstVideoFrameDisp()) || (rate == 0 && aamp->pipeline_paused))
		{
			AAMPLOG_WARN("Already running at playback rate(%d) pipeline_paused(%d), hence skipping set rate for (%d)", aamp->rate, aamp->pipeline_paused, rate);
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
				if (ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery))
				{
					// Get the last frame position when resume from the trick play.
					newSeekPosInSec = (aamp->mReportProgressPosn/1000);
				}
				else
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
				}

				if (newSeekPosInSec >= 0)
				{
					aamp->seek_pos_seconds = newSeekPosInSec;
				}
				else
				{
					AAMPLOG_WARN("new seek_pos_seconds calculated is invalid(%f), discarding it!", newSeekPosInSec);
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
			// DELIA-39530 - For 1.0->0.0 and 0.0->1.0 if eAAMPConfig_EnableGstPositionQuery is enabled, GStreamer position query will give proper value
			// Fallback case added for when eAAMPConfig_EnableGstPositionQuery is disabled, since we will be using elapsedTime to calculate position and
			// trickStartUTCMS has to be reset
			if (!ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery) && !aamp->mbDetached)
			{
				aamp->seek_pos_seconds = aamp->GetPositionMilliseconds()/1000;
				aamp->trickStartUTCMS = -1;
			}
		}

		AAMPLOG_WARN("aamp_SetRate (%d)overshoot(%d) ProgressReportDelta:(%d) ", rate,overshootcorrection,timeDeltaFromProgReport);
		AAMPLOG_WARN("aamp_SetRate rate(%d)->(%d) cur pipeline: %s. Adj position: %f Play/Pause Position:%lld", aamp->rate,rate,aamp->pipeline_paused ? "paused" : "playing",aamp->seek_pos_seconds,aamp->GetPositionMilliseconds()); // current position relative to tune time

		if (!aamp->mSeekFromPausedState && (rate == aamp->rate) && !aamp->mbDetached)
		{ // no change in desired play rate
			// no deferring for playback resume
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
				aamp->mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
				aamp->StopDownloads();
				retValue = aamp->mStreamSink->Pause(true, false);
				aamp->pipeline_paused = true;
			}
		}
		else
		{
			TuneType tuneTypePlay = eTUNETYPE_SEEK;
			if(aamp->mJumpToLiveFromPause)
			{
				tuneTypePlay = eTUNETYPE_SEEKTOLIVE;
				aamp->mJumpToLiveFromPause = false;
			}
			/* if Gstreamer pipeline set to paused state by user, change it to playing state */
			if( aamp->pipeline_paused == true )
			{
				aamp->mStreamSink->Pause(false, false);
			}
			aamp->rate = rate;
			aamp->pipeline_paused = false;
			aamp->mSeekFromPausedState = false;
			aamp->EnableDownloads();
			aamp->ResumeDownloads();
			aamp->AcquireStreamLock();
			aamp->TuneHelper(tuneTypePlay); // this unpauses pipeline as side effect
			aamp->ReleaseStreamLock();
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
		AAMPLOG_WARN("aamp_SetRate rate[%d] - mpStreamAbstractionAAMP[%p] state[%d]", aamp->rate, aamp->mpStreamAbstractionAAMP, state);
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
                AAMPLOG_WARN("operation is not allowed when player in eSTATE_ERROR state !");\
                return false;
        }

	if (AAMP_SEEK_TO_LIVE_POSITION == aamp->seek_pos_seconds )
	{
		isSeekToLive = true;
		tuneType = eTUNETYPE_SEEKTOLIVE;
	}

	AAMPLOG_WARN("aamp_Seek(%f) and seekToLive(%d)", aamp->seek_pos_seconds, isSeekToLive);

	if (isSeekToLive && !aamp->IsLive())
	{
		AAMPLOG_WARN("Not live, skipping seekToLive");
		return false;
	}

	if (aamp->IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint())
	{
		double currPositionSecs = aamp->GetPositionMilliseconds() / 1000.00;
		if (isSeekToLive || aamp->seek_pos_seconds >= currPositionSecs)
		{
			AAMPLOG_WARN("Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", aamp->seek_pos_seconds, currPositionSecs, isSeekToLive);
			aamp->NotifyOnEnteringLive();
			return false;
		}
	}

	if ((aamp->mbPlayEnabled) && aamp->pipeline_paused)
	{
		// resume downloads and clear paused flag for foreground instance. state change will be done
		// on streamSink configuration.
		AAMPLOG_WARN("paused state, so resume downloads");
		aamp->pipeline_paused = false;
		aamp->ResumeDownloads();
		sentSpeedChangedEv = true;
	}

	if (tuneType == eTUNETYPE_SEEK)
	{
		AAMPLOG_WARN("tune type is SEEK");
	}
	if (aamp->rate != AAMP_NORMAL_PLAY_RATE)
	{
		aamp->rate = AAMP_NORMAL_PLAY_RATE;
		sentSpeedChangedEv = true;
	}
	if (aamp->mpStreamAbstractionAAMP)
	{ // for seek while streaming
		aamp->SetState(eSTATE_SEEKING);
		aamp->AcquireStreamLock();
		aamp->TuneHelper(tuneType);
		aamp->ReleaseStreamLock();
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
		AAMPLOG_WARN("aamp_Seek(%f) at the middle of tune, no fragments downloaded yet.state(%d)", secondsRelativeToTuneTime,state);
		aamp->mpStreamAbstractionAAMP->SeekPosUpdate(secondsRelativeToTuneTime);
		SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
	}
	else if (eSTATE_INITIALIZED == state || eSTATE_PREPARING == state)
	{
		AAMPLOG_WARN("aamp_Seek(%f) will be called after preparing the content.state(%d)", secondsRelativeToTuneTime,state);
		aamp->seek_pos_seconds = secondsRelativeToTuneTime ;
		SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
		g_idle_add(SeekAfterPrepared, (gpointer)aamp);
	}
	else
	{
		if (secondsRelativeToTuneTime == AAMP_SEEK_TO_LIVE_POSITION)
		{
			isSeekToLive = true;
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}

		AAMPLOG_WARN("aamp_Seek(%f) and seekToLive(%d) state(%d)", secondsRelativeToTuneTime, isSeekToLive,state);

		if (isSeekToLive && !aamp->IsLive())
		{
			AAMPLOG_WARN("Not live, skipping seekToLive");
			return;
		}

		if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && ISCONFIGSET(eAAMPConfig_InterruptHandling) && aamp->IsTSBSupported())
		{
			secondsRelativeToTuneTime += aamp->mProgressReportOffset;
			AAMPLOG_WARN("aamp_Seek position adjusted to absolute value for TSB : %lf", secondsRelativeToTuneTime);
		}

		if (aamp->IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint())
		{
			double currPositionSecs = aamp->GetPositionMilliseconds() / 1000.00;

			if (isSeekToLive || secondsRelativeToTuneTime >= currPositionSecs)
			{
				AAMPLOG_WARN("Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", secondsRelativeToTuneTime, currPositionSecs, isSeekToLive);
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
			AAMPLOG_WARN("paused state, so resume downloads");
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
			SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
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
			aamp->AcquireStreamLock();
			aamp->TuneHelper(tuneType, seekWhilePause);
			aamp->ReleaseStreamLock();
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
	AAMPLOG_WARN("aamp_SetRateAndSeek(%d)(%f)", rate, secondsRelativeToTuneTime);
	if (!IsValidRate(rate))
	{
		AAMPLOG_WARN("SetRate ignored!! Invalid rate (%d)", rate);
		return;
	}
  //Hack For DELIA-51318 convert the incoming rates into acceptable rates
	if(ISCONFIGSET(eAAMPConfig_RepairIframes))
	{
		AAMPLOG_WARN("mRepairIframes is true, setting actual rate %d for the recieved rate %d", getWorkingTrickplayRate(rate), rate);
		rate = getWorkingTrickplayRate(rate);
	}

	if (aamp->mpStreamAbstractionAAMP)
	{
		if ((!aamp->mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0))
		{
			AAMPLOG_WARN("Ignoring trickplay. No iframe tracks in stream");
			aamp->NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
			return;
		}
		aamp->AcquireStreamLock();
		aamp->TeardownStream(false);
		aamp->seek_pos_seconds = secondsRelativeToTuneTime;
		aamp->rate = rate;
		aamp->TuneHelper(eTUNETYPE_SEEK);
		aamp->ReleaseStreamLock();
		if(rate == 0)
		{
			if (!aamp->pipeline_paused)
			{
				AAMPLOG_WARN("Pausing Playback at Position '%lld'.", aamp->GetPositionMilliseconds());
				aamp->mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
				aamp->StopDownloads();
				bool retValue = aamp->mStreamSink->Pause(true, false);
				aamp->pipeline_paused = true;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("aamp_SetRateAndSeek rate[%d] - mpStreamAbstractionAAMP[%p] state[%d]", aamp->rate, aamp->mpStreamAbstractionAAMP, state);
	}
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
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP )
	{
		aamp->SetVideoZoom(zoom);
	}
	else
	{
		AAMPLOG_WARN("Player is in state (eSTATE_IDLE), value has been cached");
	}
	aamp->ReleaseStreamLock();
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
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		aamp->SetVideoMute(muted);
	}
	else
	{
		AAMPLOG_WARN("Player is in state eSTATE_IDLE, value has been cached");
	}
	aamp->ReleaseStreamLock();
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
		AAMPLOG_WARN("Audio level (%d) is outside the range supported.. discarding it..",
		 volume);
	}
	else if (aamp != NULL)
	{
		aamp->audio_volume = volume;
		if (aamp->mpStreamAbstractionAAMP)
		{
			aamp->SetAudioVolume(volume);
		}
		else
		{
			AAMPLOG_WARN("Player is in state eSTATE_IDLE, value has been cached");
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
	SetPreferredLanguages(language);
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
	        AAMPLOG_WARN("    subscribedTags[%d] = '%s'", i, subscribedTags.at(i).data());
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
	AAMPLOG_WARN("[AAMP_JS] (%p)", context);
	if (mJSBinding_DL) {
		void(*loadJS)(void*, void*);
		const char* szLoadJS = "aamp_LoadJS";
		loadJS = (void(*)(void*, void*))dlsym(mJSBinding_DL, szLoadJS);
		if (loadJS) {
			AAMPLOG_WARN("[AAMP_JS]  dlsym(%p, \"%s\")=%p", mJSBinding_DL, szLoadJS, loadJS);
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
	AAMPLOG_WARN("[AAMP_JS] (%p)", context);
	if (mJSBinding_DL) {
		void(*unloadJS)(void*);
		const char* szUnloadJS = "aamp_UnloadJS";
		unloadJS = (void(*)(void*))dlsym(mJSBinding_DL, szUnloadJS);
		if (unloadJS) {
			AAMPLOG_WARN("[AAMP_JS] dlsym(%p, \"%s\")=%p", mJSBinding_DL, szUnloadJS, unloadJS);
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
	static char lang[MAX_LANGUAGE_TAG_LENGTH];
	lang[0] = 0;
	int trackIndex = GetAudioTrack();
	if( trackIndex>=0 )
	{
		std::vector<AudioTrackInfo> trackInfo = aamp->mpStreamAbstractionAAMP->GetAvailableAudioTracks();
		if (!trackInfo.empty())
		{
			strncpy(lang, trackInfo[trackIndex].language.c_str(), sizeof(lang) );
		}
	}
	return lang;
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
	if (type == eDRM_PlayReady)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,std::string(url));
	}
	else if (type == eDRM_WideVine)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,std::string(url));
	}
	else if (type == eDRM_ClearKey)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,std::string(url));
	}
	else if (type == eDRM_MAX_DRMSystems)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LicenseServerUrl,std::string(url));
	}
	else
    {
          AAMPLOG_ERR("PlayerInstanceAAMP:: invalid drm type(%d) received.", type);
    }
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AvgBWForABR,useAvgBW);
}

/**
*   @brief SetPreCacheTimeWindow Function to Set PreCache Time
*
*   @param  Time in minutes - Max PreCache Time 
*/
void PlayerInstanceAAMP::SetPreCacheTimeWindow(int nTimeWindow)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PreCachePlaylistTime,nTimeWindow);
}

/**
 *   @brief Set VOD Trickplay FPS.
 *
 *   @param  vodTrickplayFPS - FPS to be used for VOD Trickplay
 */
void PlayerInstanceAAMP::SetVODTrickplayFPS(int vodTrickplayFPS)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS);
}

/**
 *   @brief Set Linear Trickplay FPS.
 *
 *   @param  linearTrickplayFPS - FPS to be used for Linear Trickplay
 */
void PlayerInstanceAAMP::SetLinearTrickplayFPS(int linearTrickplayFPS)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LinearTrickPlayFPS,linearTrickplayFPS);
}

/**
 *   @brief Set Live Offset.
 *
 *   @param  liveoffset- Live Offset
 */
void PlayerInstanceAAMP::SetLiveOffset(int liveoffset)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LiveOffset,(double)liveoffset);
}

/**
 *   @brief To set the error code to be used for playback stalled error.
 *
 *   @param  errorCode - error code for playback stall errors.
 */
void PlayerInstanceAAMP::SetStallErrorCode(int errorCode)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StallErrorCode,errorCode);	
}

/**
 *   @brief To set the timeout value to be used for playback stall detection.
 *
 *   @param  timeoutMS - timeout in milliseconds for playback stall detection.
 */
void PlayerInstanceAAMP::SetStallTimeout(int timeoutMS)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StallTimeoutMS,timeoutMS);	
}

/**
 *   @brief Set report interval duration
 *
 *   @param  reportInterval - report interval duration in milliSeconds
 */
void PlayerInstanceAAMP::SetReportInterval(int reportInterval)
{
	ERROR_STATE_CHECK_VOID();
	if(reportInterval > 0)
	{
	    // We now want the value in seconds but it is given in milliseconds so convert it here
	    double dReportInterval = reportInterval;
	    dReportInterval /= 1000;

		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ReportProgressInterval,dReportInterval);
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
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_InitFragmentRetryCount,count);
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
	PrivAAMPState currentState = eSTATE_RELEASED;
	try 
	{
		std::lock_guard<std::mutex> lock (mPrvAampMtx);
		if(NULL == aamp)
		{	
			throw std::invalid_argument("NULL reference");
		}
		aamp->GetState(currentState);
	}
	catch (std::exception &e)
	{
		AAMPLOG_WARN("Invalid access to the instance of PrivateInstanceAAMP (%s), returning %s as current state",  e.what(),"eSTATE_RELEASED");
	}
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
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrate = aamp->mpStreamAbstractionAAMP->GetVideoBitrate();
	}
	aamp->ReleaseStreamLock();
	return bitrate;
}

/**
 *   @brief To set a preferred bitrate for video profile.
 *
 *   @param[in] preferred bitrate for video profile
 */
void PlayerInstanceAAMP::SetVideoBitrate(long bitrate)
{
	if (bitrate != 0)
	{
		// Single bitrate profile selection , with abr disabled
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableABR,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,bitrate);
	}
	else
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableABR,true);
		long gpDefaultBitRate;
		gpGlobalConfig->GetConfigValue( eAAMPConfig_DefaultBitrate ,gpDefaultBitRate);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,gpDefaultBitRate);
		AAMPLOG_WARN("Resetting default bitrate to  %ld", gpDefaultBitRate);
	}
}

/**
 *   @brief To get the bitrate of current audio profile.
 *
 *   @ret bitrate of audio profile
 */
long PlayerInstanceAAMP::GetAudioBitrate(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	long bitrate = 0;
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrate = aamp->mpStreamAbstractionAAMP->GetAudioBitrate();
	}
	aamp->ReleaseStreamLock();
	return bitrate;
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
		AAMPLOG_WARN(" GetAudioVolume is returning cached value since player is at %s",
		 "eSTATE_IDLE");
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
	std::vector<long> bitrates;
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrates = aamp->mpStreamAbstractionAAMP->GetVideoBitrates();
	}
	aamp->ReleaseStreamLock();
	return bitrates;
}

/**
 *   @brief To get the available manifest.
 *
 *   @ret available manifest
 */
std::string PlayerInstanceAAMP::GetManifest(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());
	GrowableBuffer manifest;
	ContentType ContentType;
	if ((aamp->GetContentType() == ContentType_VOD) && (aamp->mMediaFormat == eMEDIAFORMAT_DASH))
	{
		std::string manifestUrl = aamp->GetManifestUrl();
		memset(&manifest, 0, sizeof(manifest));
		if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(manifestUrl, &manifest, manifestUrl))
		{
			/*char pointer to string conversion*/
			std::string Manifest(manifest.ptr,manifest.len);
			aamp_Free(&manifest);
			AAMPLOG_INFO("PlayerInstanceAAMP: manifest retrieved from cache");
			return Manifest;
		}
	}
	return "";
}

/**
 *   @brief To get the available audio bitrates.
 *
 *   @ret available audio bitrates
 */
std::vector<long> PlayerInstanceAAMP::GetAudioBitrates(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::vector<long>());
	std::vector<long> bitrates;
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrates = aamp->mpStreamAbstractionAAMP->GetAudioBitrates();
	}
	aamp->ReleaseStreamLock();
	return bitrates;
}

/**
 *   @brief To set the initial bitrate value.
 *
 *   @param[in] initial bitrate to be selected
 */
void PlayerInstanceAAMP::SetInitialBitrate(long bitrate)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,bitrate);
}

/**
 *   @brief To set the initial bitrate value for 4K assets.
 *
 *   @param[in] initial bitrate to be selected for 4K assets
 */
void PlayerInstanceAAMP::SetInitialBitrate4K(long bitrate4K)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate4K,bitrate4K);
}

/**
 *   @brief To set the network download timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetNetworkTimeout(double timeout)
{
        ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NetworkTimeout,timeout);
}

/**
 *   @brief To set the manifest download timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetManifestTimeout(double timeout)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ManifestTimeout,timeout);
}

/**
 *   @brief To set the playlist download timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetPlaylistTimeout(double timeout)
{
        ERROR_STATE_CHECK_VOID();        
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistTimeout,timeout);
}

/**
 *   @brief To set the download buffer size value
 *
 *   @param[in] preferred download buffer size
 */
void PlayerInstanceAAMP::SetDownloadBufferSize(int bufferSize)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxFragmentCached,bufferSize);
}

/**
 *   @brief Set preferred DRM.
 *
 *   @param[in] drmType - preferred DRM type
 */
void PlayerInstanceAAMP::SetPreferredDRM(DRMSystems drmType)
{
	ERROR_STATE_CHECK_VOID();
	if(drmType != eDRM_NONE)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredDRM,(int)drmType);
		aamp->isPreferredDRMConfigured = true;
	}
	else
	{
		aamp->isPreferredDRMConfigured = false;
	}
}

/**
 *   @brief Set Stereo Only Playback.
 */
void PlayerInstanceAAMP::SetStereoOnlyPlayback(bool bValue)
{
	ERROR_STATE_CHECK_VOID();	
	if(bValue)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableEC3,true);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,true);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ForceEC3,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StereoOnly,true);
	}
	else
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableEC3,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StereoOnly,false);
	}
}

/**
 *   @brief Set Disable4K configuration flag
 */
void PlayerInstanceAAMP::SetDisable4K(bool bValue)
{
        ERROR_STATE_CHECK_VOID();
        SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_Disable4K,bValue);
}


/**
 *   @brief Set BulkTimedMetadata Reporting flag
 */
void PlayerInstanceAAMP::SetBulkTimedMetaReport(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_BulkTimedMetaReport,bValue);
}

/**
 *   @brief Set unpaired discontinuity retune flag
 */
void PlayerInstanceAAMP::SetRetuneForUnpairedDiscontinuity(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RetuneForUnpairDiscontinuity,bValue);
}

/**
 *   @brief Set retune configuration for gstpipeline internal data stream error.
 */
void PlayerInstanceAAMP::SetRetuneForGSTInternalError(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RetuneForGSTError,bValue);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NetworkProxy ,(std::string)proxy);
}

/**
 *   @brief To set the proxy for license request
 *
 *   @param[in] proxy to use for license request
 */
void PlayerInstanceAAMP::SetLicenseReqProxy(const char * licenseProxy)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LicenseProxy ,(std::string)licenseProxy);
}

/**
 *   @brief To set the curl stall timeout value
 *
 *   @param[in] curl stall timeout
 */
void PlayerInstanceAAMP::SetDownloadStallTimeout(long stallTimeout)
{
	ERROR_STATE_CHECK_VOID();
	if( stallTimeout >= 0 )
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CurlStallTimeout,stallTimeout);
	}
}

/**
 *   @brief To set the curl download start timeout value
 *
 *   @param[in] curl download start timeout
 */
void PlayerInstanceAAMP::SetDownloadStartTimeout(long startTimeout)
{
	ERROR_STATE_CHECK_VOID();
        if( startTimeout >= 0 )
        {
            SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CurlDownloadStartTimeout,startTimeout);
        }
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
        AAMPLOG_WARN("PlayerInstanceAAMP::(%s)->(%s)",  aamp->mSubLanguage.c_str(), language);

	if (aamp->mSubLanguage.compare(language) == 0)
		return;

	
	if (state == eSTATE_IDLE || state == eSTATE_RELEASED)
	{
		AAMPLOG_WARN("PlayerInstanceAAMP:: \"%s\" language set prior to tune start",  language);
	}
	else
	{
		AAMPLOG_WARN("PlayerInstanceAAMP:: \"%s\" language set - will take effect on next tune", language);
	}
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SubTitleLanguage,(std::string)language);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistParallelFetch,bValue);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistParallelRefresh,bValue);
}

/**
 *   @brief Get Async Tune configuration
 *
 *   @return bool - true if config set
 */
bool PlayerInstanceAAMP::GetAsyncTuneConfig()
{
	return ISCONFIGSET(eAAMPConfig_AsyncTune);
}

/**
 *   @brief Set Westeros sink Configuration
 *   @param[in] bValue - true if westeros sink enabled
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetWesterosSinkConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_UseWesterosSink,bValue);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SetLicenseCaching,bValue);	
}

/**
 *   @brief Set display resolution check for video profile filtering
 *   @param[in] bValue - true/false to enable/disable profile filtering
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetOutputResolutionCheck(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LimitResolution,bValue);
}

/**
 *   @brief Set Matching BaseUrl Config Configuration
 *
 *   @param[in] bValue - true if Matching BaseUrl enabled
 *   @return void
 */
void PlayerInstanceAAMP::SetMatchingBaseUrlConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MatchBaseUrl,bValue);
}

/**
 *   @brief Configure New ABR Enable/Disable
 *   @param[in] bValue - true if new ABR enabled
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetNewABRConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ABRBufferCheckEnabled,bValue);
	// Piggybagged following setting along with NewABR for Peacock
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,bValue);
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,bValue);
}

/**
 *   @brief Configure URI  parameters
 *   @param[in] bValue -true to enable
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetPropagateUriParameters(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PropogateURIParam,bValue);
}

/**
 *   @brief Call to optionally configure simulated per-download network latency for negative testing
 *   @param[in] DownloadDelayInMs - extra millisecond delay added in each download
 *
 *   @return void
 */
void PlayerInstanceAAMP::ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs)
{
	if( DownloadDelayInMs <= MAX_DOWNLOAD_DELAY_LIMIT_MS )
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DownloadDelay,(int)DownloadDelayInMs);
	}
}

/**
 *   @brief Configure URI  parameters
 *   @param[in] bValue -true to enable
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetSslVerifyPeerConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SslVerifyPeer,bValue);
}

/**
 *   @brief Set audio track by audio parameters like language , rendition, codec etc..
 * 	 @param[in][optional] language, rendition, codec, channel 
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetAudioTrack(std::string language,  std::string rendition, std::string codec, unsigned int channel)
{
	aamp->mAudioTuple.clear();
	aamp->mAudioTuple.setAudioTrackTuple(language, rendition, codec, channel);
	/* Now we have an option to set language and rendition only*/
	SetPreferredLanguages( language.c_str(), rendition.c_str() );
}

/**
 *   @brief Set optional preferred codec list
 *   @param[in] codecList[] - string with array with codec list
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetPreferredCodec(const char *codecList)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();

	aamp->preferredCodecString.clear();
	aamp->preferredCodecList.clear();

	if(codecList != NULL)
	{
		aamp->preferredCodecString = std::string(codecList);
		std::istringstream ss(aamp->preferredCodecString);
		std::string codec;
		while(std::getline(ss, codec, ','))
		{
			aamp->preferredCodecList.push_back(codec);
			AAMPLOG_INFO(" Parsed preferred codec: %s",  
					codec.c_str());
		}

		aamp->preferredCodecString = std::string(codecList);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioCodec,aamp->preferredCodecString);
	}

	AAMPLOG_INFO("Number of preferred codecs: %d", 
			aamp->preferredCodecList.size());
}

/**
 *   @brief Set optional preferred rendition list
 *   @param[in] renditionList - string with comma-delimited rendition list in ISO-639
 *             from most to least preferred. Set NULL to clear current list.
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetPreferredRenditions(const char *renditionList)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();

	aamp->preferredRenditionString.clear();
	aamp->preferredRenditionList.clear();

	if(renditionList != NULL)
	{
		aamp->preferredRenditionString = std::string(renditionList);
		std::istringstream ss(aamp->preferredRenditionString);
		std::string rendition;
		while(std::getline(ss, rendition, ','))
		{
			aamp->preferredRenditionList.push_back(rendition);
			AAMPLOG_INFO("Parsed preferred rendition: %s",  
					rendition.c_str());
		}

		aamp->preferredRenditionString = std::string(renditionList);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioRendition,aamp->preferredRenditionString);
	}

	AAMPLOG_INFO("Number of preferred renditions: %d",  
			aamp->preferredRenditionList.size());
}

/**
 *   @brief Set optional preferred language list
 *   @param[in] languageList - string with comma-delimited language list in ISO-639
 *             from most to least preferred. Set NULL to clear current list.
 *   @param[in] preferredRendition  - preferred rendition from role
 *   @param[in] preferredType -  preferred accessibility type
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType )
{
	aamp->SetPreferredLanguages(languageList, preferredRendition, preferredType);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,bValue);
	// Piggyback the PDT based processing for new Adbreaker processing for peacock.
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,bValue);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NativeCCRendering,enable);
#endif
}

/**
 *   @brief To set the tune-event according to the player.
 *
 *   @param[in] preferred tune event type
 */
void PlayerInstanceAAMP::SetTuneEventConfig(int tuneEventType)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_TuneEventConfig,tuneEventType);
}

/**
 *   @brief Set video rectangle property
 *
 *   @param[in] video rectangle property
 */
void PlayerInstanceAAMP::EnableVideoRectangle(bool rectProperty)
{
	if(!rectProperty)
	{
		if(ISCONFIGSET(eAAMPConfig_UseWesterosSink))
		{
			SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableRectPropertyCfg,false);
		}
		else
		{
			AAMPLOG_WARN("Skipping the configuration value[%d], since westerossink is disabled",  rectProperty);			
		}
	}
	else 
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableRectPropertyCfg,true);
	}
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
	std::vector<AudioTrackInfo> tracks = aamp->mpStreamAbstractionAAMP->GetAvailableAudioTracks();
	if (!tracks.empty() && (trackId >= 0 && trackId < tracks.size()))
	{
		//aamp->SetPreferredAudioTrack(tracks[trackId]);
		SetPreferredLanguages( tracks[trackId].language.c_str(), tracks[trackId].rendition.c_str() );
	}
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_InitRampDownLimit,limit);
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
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CEAPreferred,format);
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
	bool ret = false;
	aamp->AcquireStreamLock();
	if(thumbIndex >= 0 && aamp->mpStreamAbstractionAAMP)
	{
		ret = aamp->mpStreamAbstractionAAMP->SetThumbnailTrack(thumbIndex);
	}
	aamp->ReleaseStreamLock();
	return ret;
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
	// Stored as tune setting , this will get cleared after one tune session
	SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_AuthToken,sessionToken);
	return;
}

/**
 *   @brief Enable seekable range values in progress event
 *
 *   @param[in] enabled - true if enabled
 */
void PlayerInstanceAAMP::EnableSeekableRange(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableSeekRange,bValue);
}

/**
 *   @brief Enable video PTS reporting in progress event
 *
 *   @param[in] enabled - true if enabled
 */
void PlayerInstanceAAMP::SetReportVideoPTS(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ReportVideoPTS,bValue);
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
 *   @brief Enable/Disable async operation
 *
 *   @return void
 */
void PlayerInstanceAAMP::SetAsyncTuneConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AsyncTune,bValue);
	// Start it for the playerinstance if default not started and App wants
	// Stop Async operation for the playerinstance if default started and App doesnt want 
	AsyncStartStop();
}

void PlayerInstanceAAMP::AsyncStartStop()
{
	// Check if global configuration is set to false
	// Additional check added here, since this API can be called from jsbindings/native app
	if (ISCONFIGSET(eAAMPConfig_AsyncTune) && !mAsyncRunning)
	{
		AAMPLOG_WARN("Enable async tune operation!!" );
		mAsyncRunning = true;
		StartScheduler();
		aamp->SetEventPriorityAsyncTune(true);
		aamp->SetScheduler(this);
	}
	else if(!ISCONFIGSET(eAAMPConfig_AsyncTune) && mAsyncRunning)
	{
		AAMPLOG_WARN("Disable async tune operation!!");
		aamp->SetEventPriorityAsyncTune(false);
		StopScheduler();
		mAsyncRunning = false;
	}
}

/**
 *   @brief Enable/disable configuration to persist ABR profile over SAP/seek
 *
 *   @param[in] value - To enable/disable configuration
 *   @return void
 */
void PlayerInstanceAAMP::PersistBitRateOverSeek(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PersistentBitRateOverSeek,bValue);	
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

	if(ISCONFIGSET(eAAMPConfig_EnableMicroEvents) && (eSTATE_ERROR == state) && !(aamp->IsTuneCompleted()))
	{
		/*Sending metrics on tune Error; excluding mid-stream failure cases & aborted tunes*/
		aamp->sendTuneMetrics(false);
	}

	AAMPLOG_WARN("aamp_stop PlayerState=%d",state);

	if (sendStateChangeEvent)
	{
		aamp->SetState(eSTATE_IDLE);
	}

	AAMPLOG_WARN("%s PLAYER[%d] Stopping Playback at Position '%lld'.\n",(aamp->mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), aamp->mPlayerId, aamp->GetPositionMilliseconds());
	aamp->Stop();
	// Revert all custom specific setting, tune specific setting and stream specific setting , back to App/default setting
	mConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING, mLogObj);
	mConfig.RestoreConfiguration(AAMP_TUNE_SETTING, mLogObj);
	mConfig.RestoreConfiguration(AAMP_STREAM_SETTING, mLogObj);
}

/**
 *   @brief To set preferred paused state behavior
 *
 *   @param[in] int behavior
 */
void PlayerInstanceAAMP::SetPausedBehavior(int behavior)
{
	ERROR_STATE_CHECK_VOID();

	if(behavior >= 0 && behavior < ePAUSED_BEHAVIOR_MAX)
	{
		AAMPLOG_WARN("Player Paused behavior : %d", behavior);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LivePauseBehavior,behavior);
	}
}


/**
 *   @brief To set AST based progress reporting 
 *
 *   @param[in] bool On/Off
 */
void PlayerInstanceAAMP::SetUseAbsoluteTimeline(bool configState)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_UseAbsoluteTimeline,configState);

}


/**
		 *   @brief To set the repairIframes flag
		 *
		 *   @param[in] bool enable/disable configuration
		 */
void PlayerInstanceAAMP::SetRepairIframes(bool configState)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RepairIframes,configState);

}

/**
* @brief InitAAMPConfig - Initialize the media player session with json config
*/
bool PlayerInstanceAAMP::InitAAMPConfig(char *jsonStr)
{
	bool retVal = false;
	retVal = mConfig.ProcessConfigJson(jsonStr,AAMP_APPLICATION_SETTING);
	mConfig.DoCustomSetting(AAMP_APPLICATION_SETTING);
	if(GETCONFIGOWNER(eAAMPConfig_AsyncTune) == AAMP_APPLICATION_SETTING)
	{
		AsyncStartStop();
	}

	return retVal;
}

/**
* @brief GetAAMPConfig - Get AAMP Config as JSON String 
*/
std::string PlayerInstanceAAMP::GetAAMPConfig()
{
	std::string jsonStr;
	mConfig.GetAampConfigJSONStr(jsonStr);
	return jsonStr;
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
	AAMPLOG_ERR("Auxiliary audio language is not supported in this platform, ignoring the input!");
#endif
}

/**
 *   @brief To set whether the JS playback session is from XRE or not.
 *
 *   @param[in] bool On/Off
 */
void PlayerInstanceAAMP::XRESupportedTune(bool xreSupported)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_XRESupportedTune,xreSupported);
}

/**
 * @}
 */
