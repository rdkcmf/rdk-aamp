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

#define PLAYING_STATE_CHECK() \
	PrivAAMPState state = GetState(); \
	if( state != eSTATE_STOPPED && state != eSTATE_IDLE && state != eSTATE_COMPLETE && state != eSTATE_RELEASED){ \
		AAMPLOG_WARN("Operation is not allowed when player in playing state !!");\
		return; \
	}

static bool iarmInitialized = false;
std::mutex PlayerInstanceAAMP::mPrvAampMtx;

/**
 *  @brief PlayerInstanceAAMP Constructor.
 */
PlayerInstanceAAMP::PlayerInstanceAAMP(StreamSink* streamSink
	, std::function< void(uint8_t *, int, int, int) > exportFrames
	) : aamp(NULL), sp_aamp(nullptr), mInternalStreamSink(NULL), mJSBinding_DL(),mAsyncRunning(false),mConfig(),mAsyncTuneEnabled(false),mScheduler()
{

#ifdef IARM_MGR
if(!iarmInitialized)
{
        char processName[20] = {0};
        IARM_Result_t result;
        sprintf (processName, sizeof(processName), "AAMP-PLAYER-%u", getpid());
        if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Init((const char*) &processName))) {
                logprintf("IARM Interface Inited in AAMP");
        }
        else {
            logprintf("IARM Interface Inited Externally : %d", result);
        }

        if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Connect())) {
                logprintf("IARM Interface Connected  in AAMP");
        }
        else {
            logprintf ("IARM Interface Connected Externally :%d", result);
        }
	iarmInitialized = true;
}
#endif

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
	if(gpGlobalConfig == NULL)
	{		
#ifdef AAMP_BUILD_INFO
		std::string tmpstr = MACRO_TO_STRING(AAMP_BUILD_INFO);
		logprintf(" AAMP_BUILD_INFO: %s",tmpstr.c_str());
#endif
		gpGlobalConfig =  new AampConfig();
		// Init the default values
		gpGlobalConfig->Initialize();
		gpGlobalConfig->ReadDeviceCapability();
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
	// start Scheduler Worker for task handling
 	mScheduler.SetLogger(mLogObj);
        mScheduler.StartScheduler();

	if (NULL == streamSink)
	{
		mInternalStreamSink = new AAMPGstPlayer(mConfig.GetLoggerInstance(), aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
                , exportFrames
#endif
		);

		streamSink = mInternalStreamSink;
	}
	else
	{
		// Disable async tune in aamp as plugin mode, since it already called from aamp gst as async call
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING, eAAMPConfig_AsyncTune, false);
		mAsyncRunning = false;
	}
	aamp->SetStreamSink(streamSink);
	aamp->SetScheduler(&mScheduler);
	AsyncStartStop();
}


/**
 *  @brief PlayerInstanceAAMP Destructor.
 */
PlayerInstanceAAMP::~PlayerInstanceAAMP()
{
	mLogObj = gpGlobalConfig->GetLoggerInstance();
#ifdef AAMP_CC_ENABLED
	AampCCManager::GetInstance()->SetLogger(mLogObj);
#endif
	if (aamp)
	{
		PrivAAMPState state;
		aamp->GetState(state);
		// Acquire the lock , to prevent new entries into scheduler
		mScheduler.SuspendScheduler();			
		// Remove all the tasks 
		mScheduler.RemoveAllTasks();
		if (state != eSTATE_IDLE && state != eSTATE_RELEASED)
		{
			//Avoid stop call since already stopped
			aamp->Stop();
		}
		
		std::lock_guard<std::mutex> lock (mPrvAampMtx);
		aamp = NULL;
	}
	SAFE_DELETE(mInternalStreamSink);

	// Stop the scheduler 
	mAsyncRunning = false;
	mScheduler.StopScheduler();

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
 *   @brief API to reset configuration across tunes for single player instance
 */
void PlayerInstanceAAMP::ResetConfiguration()
{
	AAMPLOG_WARN("Resetting Configuration to default values ");
	// Copy the default configuration to session configuration .App can modify the configuration set
	mConfig = *gpGlobalConfig;

	mLogObj = mConfig.GetLoggerInstance();
#ifdef AAMP_CC_ENABLED
	AampCCManager::GetInstance()->SetLogger(mLogObj);
#endif

	// Based on the default condition , reset the AsyncTune scheduler
	AsyncStartStop();
}

/**
 *  @brief Stop playback and release resources.
 */
void PlayerInstanceAAMP::Stop(bool sendStateChangeEvent)
{
	if (aamp)
	{
		PrivAAMPState state;
		aamp->GetState(state);

		// 1. Ensure scheduler is suspended and all tasks if any to be cleaned 
		// 2. Check for state ,if already in Idle / Released , ignore stopInternal 
		// 3. Restart the scheduler , needed if same instance is used for tune again

		mScheduler.SuspendScheduler();
		mScheduler.RemoveAllTasks();

		//state will be eSTATE_IDLE or eSTATE_RELEASED, right after an init or post-processing of a Stop call
		if (state != eSTATE_IDLE && state != eSTATE_RELEASED)
		{
			StopInternal(sendStateChangeEvent);
		}

		//Release lock
		mScheduler.ResumeScheduler();
	}
}

/**
 *  @brief Tune to a URL.
 *         DEPRECATED!  This is included for backwards compatibility with current Sky AS integration
 *         audioDecoderStreamSync is a broadcom-specific hack (for original xi6 POC build) - this doesn't belong in Tune API.
 */
void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync)
{
	Tune(mainManifestUrl, /*autoPlay*/ true, contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync);
}

/**
 *  @brief Tune to a URL.
 */
void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync)
{
#ifdef AMLOGIC
	ManageAsyncTuneConfig(mainManifestUrl);
#endif
	if(mAsyncTuneEnabled)
	{
		const std::string manifest {mainManifestUrl};
		const std::string cType = (contentType != NULL) ? std::string(contentType) : std::string();
		const std::string sTraceUUID = (traceUUID != NULL)? std::string(traceUUID) : std::string();

		mScheduler.ScheduleTask(AsyncTaskObj(
			[manifest, autoPlay , cType, bFirstAttempt, bFinalAttempt, sTraceUUID, audioDecoderStreamSync](void *data)
			{
				PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
				const char * trace_uuid = sTraceUUID.empty() ? nullptr : sTraceUUID.c_str();
				
				instance->TuneInternal(manifest.c_str(), autoPlay, cType.c_str(), bFirstAttempt, bFinalAttempt, trace_uuid, audioDecoderStreamSync);
			},
			(void *) this,
			__FUNCTION__));
	}
	else
	{
		TuneInternal(mainManifestUrl, autoPlay , contentType, bFirstAttempt, bFinalAttempt,traceUUID,audioDecoderStreamSync);
	}
}

/**
 * @brief Tune to a URL.
 */
void PlayerInstanceAAMP::TuneInternal(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync)
{
	PrivAAMPState state;
	if(aamp){

	aamp->StopPausePositionMonitoring("Tune() called");

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
}


/**
 *  @brief Soft stop the player instance.
 */
void PlayerInstanceAAMP::detach()
{
	// detach is similar to Stop , need to run like stop in Sync mode
	if(aamp){

	//Acquire lock
	mScheduler.SuspendScheduler();
	aamp->StopPausePositionMonitoring("detach() called");
	aamp->detach();
	//Release lock
	mScheduler.ResumeScheduler();
	
	}
}

/**
 *  @brief Register event handler.
 */
void PlayerInstanceAAMP::RegisterEvents(EventListener* eventListener)
{
	aamp->RegisterEvents(eventListener);
}

/**
 *  @brief UnRegister event handler.
 */
void PlayerInstanceAAMP::UnRegisterEvents(EventListener* eventListener)
{
	aamp->UnRegisterEvents(eventListener);
}

/**
 *  @brief Set retry limit on Segment injection failure.      
 */
void PlayerInstanceAAMP::SetSegmentInjectFailCount(int value)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SegmentInjectThreshold,value);
}

/**
 *  @brief Set retry limit on Segment drm decryption failure. 
 */
void PlayerInstanceAAMP::SetSegmentDecryptFailCount(int value)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DRMDecryptThreshold,value);
}

/**
 *  @brief Set initial buffer duration in seconds
 */
void PlayerInstanceAAMP::SetInitialBufferDuration(int durationSec)
{
	NOT_IDLE_AND_NOT_RELEASED_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_InitialBuffer,durationSec);
}

/**
 *  @brief Get initial buffer duration in seconds
 */
int PlayerInstanceAAMP::GetInitialBufferDuration(void)
{
	int durationSec;
	GETCONFIGVALUE(eAAMPConfig_InitialBuffer,durationSec);
	return durationSec;
}

/**
 *  @brief Set Maximum Cache Size for storing playlist
 */
void PlayerInstanceAAMP::SetMaxPlaylistCacheSize(int cacheSize)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxPlaylistCacheSize,cacheSize);
}

/**
 *  @brief Set profile ramp down limit.
 */
void PlayerInstanceAAMP::SetRampDownLimit(int limit)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RampDownLimit,limit);
}

/**
 *  @brief Get profile ramp down limit.
 */
int PlayerInstanceAAMP::GetRampDownLimit(void)
{
	int limit;
	GETCONFIGVALUE(eAAMPConfig_RampDownLimit,limit);
	return limit;
}

/**
 *  @brief Set Language preferred Format
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
 *  @brief Set minimum bitrate value.
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
 *  @brief Get minimum bitrate value.
 */
long PlayerInstanceAAMP::GetMinimumBitrate(void)
{
	long bitrate;
	GETCONFIGVALUE(eAAMPConfig_MinBitrate,bitrate);
	return bitrate;
}

/**
 *  @brief Set maximum bitrate value.
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
 *  @brief Get maximum bitrate value.
 */
long PlayerInstanceAAMP::GetMaximumBitrate(void)
{
	long bitrate;
	GETCONFIGVALUE(eAAMPConfig_MaxBitrate,bitrate);
	return bitrate;
}

/**
 *  @brief Check given rate is valid.
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
 *  @brief Set playback rate.
 */
void PlayerInstanceAAMP::SetRate(float rate,int overshootcorrection)
{
	AAMPLOG_INFO("PLAYER[%d] rate=%f.", aamp->mPlayerId, rate);
	if(aamp)
	{
		if (!IsValidRate(rate))
		{
			AAMPLOG_WARN("SetRate ignored!! Invalid rate (%f)", rate);
			return;
		}

		if(mAsyncTuneEnabled)
		{
			mScheduler.ScheduleTask(AsyncTaskObj([rate,overshootcorrection](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						instance->SetRateInternal(rate,overshootcorrection);
					}, (void *) this,__FUNCTION__));
		}
		else
		{
			SetRateInternal(rate,overshootcorrection);
		}
	}
}


/**
 *  @brief Set playback rate - Internal function
 */
void PlayerInstanceAAMP::SetRateInternal(float rate,int overshootcorrection)
{
	AAMPLOG_INFO("PLAYER[%d] rate=%f.", aamp->mPlayerId, rate);

	ERROR_STATE_CHECK_VOID();

	if (!IsValidRate(rate))
	{
		AAMPLOG_WARN("SetRate ignored!! Invalid rate (%f)", rate);
		return;
	}
	//Hack For DELIA-51318 convert the incoming rates into acceptable rates
	if(ISCONFIGSET(eAAMPConfig_RepairIframes))
	{
		AAMPLOG_WARN("mRepairIframes is true, setting actual rate %f for the received rate %f", getWorkingTrickplayRate(rate), rate);
		rate = getWorkingTrickplayRate(rate);
	}

	aamp->StopPausePositionMonitoring("SetRate() called");

	if (aamp->mpStreamAbstractionAAMP && !(aamp->mbUsingExternalPlayer))
	{
		if ( AAMP_SLOWMOTION_RATE != rate && !aamp->mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0 && aamp->mMediaFormat != eMEDIAFORMAT_PROGRESSIVE)
		{
			AAMPLOG_WARN("Ignoring trickplay. No iframe tracks in stream");
			aamp->NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
			return;
		}
		if(!(aamp->mbPlayEnabled) && aamp->pipeline_paused && (AAMP_RATE_PAUSE != rate) && (aamp->mbSeeked || !aamp->mbDetached))
		{
			AAMPLOG_WARN("PLAYER[%d] Player %s=>%s.", aamp->mPlayerId, STRBGPLAYER, STRFGPLAYER );
			aamp->mbPlayEnabled = true;
			if (AAMP_NORMAL_PLAY_RATE == rate)
			{
				aamp->LogPlayerPreBuffered();
				aamp->mStreamSink->Configure(aamp->mVideoFormat, aamp->mAudioFormat, aamp->mAuxFormat, aamp->mSubtitleFormat, aamp->mpStreamAbstractionAAMP->GetESChangeStatus(), aamp->mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
				aamp->ResumeDownloads(); //To make sure that the playback resumes after a player switch if player was in paused state before being at background
				aamp->mpStreamAbstractionAAMP->StartInjection();
				aamp->mStreamSink->Stream();
				aamp->pipeline_paused = false;
				aamp->mbSeeked = false;
				return;
			}
			else if(AAMP_RATE_PAUSE != rate)
			{
				AAMPLOG_INFO("Player switched at trickplay %f", rate);
				aamp->playerStartedWithTrickPlay = true; //to be used to show atleast one frame
			}
		}
		bool retValue = true;
		if ( AAMP_SLOWMOTION_RATE != rate && rate > 0 && aamp->IsLive() && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint() && aamp->rate >= AAMP_NORMAL_PLAY_RATE && !aamp->mbDetached)
		{
			AAMPLOG_WARN("Already at logical live point, hence skipping operation");
			aamp->NotifyOnEnteringLive();
			return;
		}

		// DELIA-39691 If input rate is same as current playback rate, skip duplicate operation
		// Additional check for pipeline_paused is because of 0(PAUSED) -> 1(PLAYING), where aamp->rate == 1.0 in PAUSED state
		if ((!aamp->pipeline_paused && rate == aamp->rate && !aamp->GetPauseOnFirstVideoFrameDisp()) || (rate == 0 && aamp->pipeline_paused))
		{
			AAMPLOG_WARN("Already running at playback rate(%f) pipeline_paused(%d), hence skipping set rate for (%f)", aamp->rate, aamp->pipeline_paused, rate);
			return;
		}

		//DELIA-30274  -- Get the trick play to a closer position
		//Logic adapted
		// XRE gives fixed overshoot position , not suited for aamp . So ignoring overshoot correction value
			// instead use last reported posn vs the time player get play command
		// a. During trickplay , last XRE reported position is aamp->mNewSeekInfo.getInfo().Position()
					/// and last reported time is aamp->mNewSeekInfo.getInfo().UpdateTime()
		// b. Calculate the time delta	from last reported time
		// c. Using this diff , calculate the best/nearest match position (works out 70-80%)
		// d. If time delta is < 100ms ,still last video fragment rendering is not removed ,but position updated very recently
			// So switch last displayed position - NewPosn -= Posn - ((aamp->rate/4)*1000)
		// e. If time delta is > 950ms , possibility of next frame to come by the time play event is processed.
			//So go to next fragment which might get displayed
		// f. If none of above ,maintain the last displayed position .
		//
		// h. TODO (again trial n error) - for 3x/4x , within 1sec there might multiple frame displayed . Can use timedelta to calculate some more near,to be tried
		const auto SeekInfo = aamp->mNewSeekInfo.GetInfo();

		const int  timeDeltaFromProgReport = (int)SeekInfo.getTimeSinceUpdateMs();

		//Skip this logic for either going to paused to coming out of paused scenarios with HLS
		//What we would like to avoid here is the update of seek_pos_seconds because gstreamer position will report proper position
		//Check for 1.0 -> 0.0 and 0.0 -> 1.0 usecase and avoid below logic
		if (!((aamp->rate == AAMP_NORMAL_PLAY_RATE && rate == 0) || (aamp->pipeline_paused && rate == AAMP_NORMAL_PLAY_RATE)))
		{
			// when switching from trick to play mode only
			if(aamp->rate && ( AAMP_SLOWMOTION_RATE == rate || rate == AAMP_NORMAL_PLAY_RATE) && !aamp->pipeline_paused)
			{
				const auto seek_pos_seconds_copy = aamp->seek_pos_seconds;	//ensure the same value of seek_pos_seconds used in the check is logged
				if(!SeekInfo.isPositionValid(seek_pos_seconds_copy))
				{
					AAMPLOG_WARN("Cached seek position (%f) is invalid. seek_pos_seconds = %f, seek_pos_seconds @ last report = %f.",SeekInfo.getPosition(), seek_pos_seconds_copy, SeekInfo.getSeekPositionSec());
				}
				else
				{
					double newSeekPosInSec = -1;
					if (ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery))
					{
						// Get the last frame position when resume from the trick play.
						newSeekPosInSec = (SeekInfo.getPosition()/1000);
					}
					else
					{
						if(timeDeltaFromProgReport > 950) // diff > 950 mSec
						{
							// increment by 1x trickplay frame , next possible displayed frame
							newSeekPosInSec = (SeekInfo.getPosition()+(aamp->rate*1000))/1000;
						}
						else if(timeDeltaFromProgReport > 100) // diff > 100 mSec
						{
							// Get the last shown frame itself
							newSeekPosInSec = SeekInfo.getPosition()/1000;
						}
						else
						{
							// Go little back to last shown frame
							newSeekPosInSec = (SeekInfo.getPosition()-(aamp->rate*1000))/1000;
						}
					}

					if (newSeekPosInSec >= 0)
					{
						/* Note circular calculation:
						 * newSeekPosInSec is based on aamp->mNewSeekInfo
						 * aamp->mNewSeekInfo's position value is based on PrivateInstanceAAMP::GetPositionMilliseconds()
						 * PrivateInstanceAAMP::GetPositionMilliseconds() uses seek_pos_seconds
						*/
						aamp->seek_pos_seconds = newSeekPosInSec;
					}
					else
					{
						AAMPLOG_WARN("new seek_pos_seconds calculated is invalid(%f), discarding it!", newSeekPosInSec);
					}
				}
			}
			else
			{
				// Coming out of pause mode(aamp->rate=0) or when going into pause mode (rate=0)
				// Show the last position
				aamp->seek_pos_seconds = aamp->GetPositionSeconds();
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
				aamp->seek_pos_seconds = aamp->GetPositionSeconds();
				aamp->trickStartUTCMS = -1;
			}
		}

		if( AAMP_SLOWMOTION_RATE == rate )
		{
			/* Handling of fwd slowmotion playback */
			SetSlowMotionPlayRate(rate);
			aamp->NotifySpeedChanged(rate, false);

			return;
		}

		AAMPLOG_WARN("aamp_SetRate (%f)overshoot(%d) ProgressReportDelta:(%d) ", rate,overshootcorrection,timeDeltaFromProgReport);
		AAMPLOG_WARN("aamp_SetRate rate(%f)->(%f) cur pipeline: %s. Adj position: %f Play/Pause Position:%lld", aamp->rate,rate,aamp->pipeline_paused ? "paused" : "playing",aamp->seek_pos_seconds,aamp->GetPositionMilliseconds()); // current position relative to tune time

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
			//Enable playback if setRate call after detach
			if(aamp->mbDetached){ 
				aamp->mbPlayEnabled = true;
			}

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
			/* Clear setting playerrate flag */
			aamp->mSetPlayerRateAfterFirstframe=false;
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
		AAMPLOG_WARN("aamp_SetRate rate[%f] - mpStreamAbstractionAAMP[%p] state[%d]", aamp->rate, aamp->mpStreamAbstractionAAMP, state);
	}
}

/**
 *  @brief Set PauseAt position.
 */
void PlayerInstanceAAMP::PauseAt(double position)
{
	if(aamp)
	{
		if(mAsyncTuneEnabled)
		{
			(void)mScheduler.ScheduleTask(AsyncTaskObj([position](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						instance->PauseAtInternal(position);
					}, (void *) this,__FUNCTION__));
		}
		else
		{
			PauseAtInternal(position);
		}
	}
}

/**
 *  @brief Set PauseAt position - Internal function
 */
void PlayerInstanceAAMP::PauseAtInternal(double position)
{
	AAMPLOG_WARN("PLAYER[%d] aamp_PauseAt position=%f", aamp->mPlayerId, position);

	ERROR_STATE_CHECK_VOID();

	aamp->StopPausePositionMonitoring("PauseAt() called");

	if (position >= 0)
	{
		if (!aamp->pipeline_paused)
		{
			aamp->StartPausePositionMonitoring(static_cast<long long>(position * 1000));
		}
		else
		{
			AAMPLOG_WARN("PauseAt called when already paused");
		}
	}
}

static gboolean SeekAfterPrepared(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP*) ptr;
	bool sentSpeedChangedEv = false;
	bool isSeekToLiveOrEnd = false;
	TuneType tuneType = eTUNETYPE_SEEK;
	PrivAAMPState state;
        aamp->GetState(state);
        if( state == eSTATE_ERROR){
                AAMPLOG_WARN("operation is not allowed when player in eSTATE_ERROR state !");\
                return false;
        }

	if (AAMP_SEEK_TO_LIVE_POSITION == aamp->seek_pos_seconds )
	{
		isSeekToLiveOrEnd = true;
	}

	AAMPLOG_WARN("aamp_Seek(%f) and seekToLiveOrEnd(%d)", aamp->seek_pos_seconds, isSeekToLiveOrEnd);

	if (isSeekToLiveOrEnd)
	{
		if (aamp->IsLive())
		{
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}
		else
		{
			tuneType = eTUNETYPE_SEEKTOEND;
		}
	}

	if (aamp->IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint())
	{
		double currPositionSecs = aamp->GetPositionSeconds();
		if ((tuneType == eTUNETYPE_SEEKTOLIVE) || (aamp->seek_pos_seconds >= currPositionSecs))
		{
			AAMPLOG_WARN("Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", aamp->seek_pos_seconds, currPositionSecs, isSeekToLiveOrEnd);
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

		/* LLAMA-7124
		 * PositionMilisecondLock is intended to ensure both state and seek_pos_seconds (in TuneHelper)
		 * are updated before GetPositionMilliseconds() can be used*/
		auto PositionMilisecondLocked = aamp->LockGetPositionMilliseconds();
		aamp->SetState(eSTATE_SEEKING);
		/* Clear setting playerrate flag */
		aamp->mSetPlayerRateAfterFirstframe=false;
		aamp->AcquireStreamLock();
		aamp->TuneHelper(tuneType);
		if(PositionMilisecondLocked)
		{
			aamp->UnlockGetPositionMilliseconds();
		}
		aamp->ReleaseStreamLock();
		if (sentSpeedChangedEv)
		{
			aamp->NotifySpeedChanged(aamp->rate, false);
		}
	}
	return false;  // G_SOURCE_REMOVE = false , G_SOURCE_CONTINUE = true
}


/**
 *  @brief Seek to a time.
 */
void PlayerInstanceAAMP::Seek(double secondsRelativeToTuneTime, bool keepPaused)
{
	if(aamp)
	{
		PrivAAMPState state;
		aamp->GetState(state);
		if(mAsyncTuneEnabled && state != eSTATE_IDLE && state != eSTATE_RELEASED)
		{
			mScheduler.ScheduleTask(AsyncTaskObj([secondsRelativeToTuneTime,keepPaused](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						instance->SeekInternal(secondsRelativeToTuneTime,keepPaused);
					}, (void *) this,__FUNCTION__));
		}
		else
		{
			SeekInternal(secondsRelativeToTuneTime,keepPaused);
		}
	}
}


/**
 *  @brief Seek to a time - Internal function
 */
void PlayerInstanceAAMP::SeekInternal(double secondsRelativeToTuneTime, bool keepPaused)
{
	bool sentSpeedChangedEv = false;
	bool isSeekToLiveOrEnd = false;
	TuneType tuneType = eTUNETYPE_SEEK;

	ERROR_STATE_CHECK_VOID();

	aamp->StopPausePositionMonitoring("Seek() called");

	if ((aamp->mMediaFormat == eMEDIAFORMAT_HLS || aamp->mMediaFormat == eMEDIAFORMAT_HLS_MP4) && (eSTATE_INITIALIZING == state)  && aamp->mpStreamAbstractionAAMP)
	{
		AAMPLOG_WARN("aamp_Seek(%f) at the middle of tune, no fragments downloaded yet.state(%d), keep paused(%d)", secondsRelativeToTuneTime,state, keepPaused);
		aamp->mpStreamAbstractionAAMP->SeekPosUpdate(secondsRelativeToTuneTime);
		SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
	}
	else if (eSTATE_INITIALIZED == state || eSTATE_PREPARING == state)
	{
		AAMPLOG_WARN("aamp_Seek(%f) will be called after preparing the content.state(%d), keep paused(%d)", secondsRelativeToTuneTime,state, keepPaused);
		aamp->seek_pos_seconds = secondsRelativeToTuneTime ;
		SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
		g_idle_add(SeekAfterPrepared, (gpointer)aamp);
	}
	else
	{
		if (secondsRelativeToTuneTime == AAMP_SEEK_TO_LIVE_POSITION)
		{
			isSeekToLiveOrEnd = true;
		}

		AAMPLOG_WARN("aamp_Seek(%f) and seekToLiveOrEnd(%d) state(%d), keep paused(%d)", secondsRelativeToTuneTime, isSeekToLiveOrEnd,state, keepPaused);

		if (isSeekToLiveOrEnd)
		{
			if (aamp->IsLive())
			{
				tuneType = eTUNETYPE_SEEKTOLIVE;
			}
			else
			{
				tuneType = eTUNETYPE_SEEKTOEND;
			}
		}

		if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && 
		   ISCONFIGSET(eAAMPConfig_InterruptHandling) && 
		   aamp->IsTSBSupported() &&
		   !isSeekToLiveOrEnd)
		{
			secondsRelativeToTuneTime += aamp->mProgressReportOffset;
			AAMPLOG_WARN("aamp_Seek position adjusted to absolute value for TSB : %lf", secondsRelativeToTuneTime);
		}

		if (aamp->IsLive() && aamp->mpStreamAbstractionAAMP && aamp->mpStreamAbstractionAAMP->IsStreamerAtLivePoint())
		{
			double currPositionSecs = aamp->GetPositionSeconds();

			if ((tuneType == eTUNETYPE_SEEKTOLIVE) || secondsRelativeToTuneTime >= currPositionSecs)
			{
				AAMPLOG_WARN("Already at live point, skipping operation since requested position(%f) >= currPosition(%f) or seekToLive(%d)", secondsRelativeToTuneTime, currPositionSecs, isSeekToLiveOrEnd);
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

			if(keepPaused && aamp->mMediaFormat != eMEDIAFORMAT_PROGRESSIVE)
			{
				// Enable seek while paused if not Progressive stream
				seekWhilePause = true;
			}

			// Clear paused flag. state change will be done
			// on streamSink configuration.
			if (!seekWhilePause)
			{
				AAMPLOG_WARN("Clearing paused flag");
				aamp->pipeline_paused = false;
				sentSpeedChangedEv = true;
			}
#ifdef AMLOGIC
			else
			{
				// For amlogic only, delay going into the paused state until we receive the 
				// 'displayed first frame' notification
				AAMPLOG_WARN("Clearing paused flag for seek while paused");
				aamp->pipeline_paused = false;
			}
#endif			
			// Resume downloads
			AAMPLOG_INFO("Resuming downloads");
			aamp->ResumeDownloads();
		}

		/* LLAMA-7124
		 * PositionMilisecondLock is intended to ensure both state and seek_pos_seconds
		 * are updated before GetPositionMilliseconds() can be used*/
		auto PositionMilisecondLocked = aamp->LockGetPositionMilliseconds();

		if (tuneType == eTUNETYPE_SEEK)
		{
			SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);
			aamp->seek_pos_seconds = secondsRelativeToTuneTime;
		}
		else if (tuneType == eTUNETYPE_SEEKTOEND)
		{
			SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_PlaybackOffset,-1);
			aamp->seek_pos_seconds = -1;
		}

		if (aamp->rate != AAMP_NORMAL_PLAY_RATE)
		{
			aamp->rate = AAMP_NORMAL_PLAY_RATE;
			sentSpeedChangedEv = true;
		}

		/**Set the flag true to indicate seeked **/
		aamp->mbSeeked = true;

		if (aamp->mpStreamAbstractionAAMP)
		{ // for seek while streaming
			aamp->SetState(eSTATE_SEEKING);
			if(PositionMilisecondLocked)
			{
				aamp->UnlockGetPositionMilliseconds();
			}
			/* Clear setting playerrate flag */
			aamp->mSetPlayerRateAfterFirstframe=false;
			aamp->AcquireStreamLock();
			aamp->TuneHelper(tuneType, seekWhilePause);
			aamp->ReleaseStreamLock();
			if (sentSpeedChangedEv && (!seekWhilePause) )
			{
				aamp->NotifySpeedChanged(aamp->rate, false);
			}
		}
		else if(PositionMilisecondLocked)
		{
			aamp->UnlockGetPositionMilliseconds();
		}
		if (aamp->mbPlayEnabled)
		{
			// Clear seeked flag for FG instance after SEEK
			aamp->mbSeeked = false;
		}
	}
}

/**
 *  @brief Seek to live point.
 */
void PlayerInstanceAAMP::SeekToLive(bool keepPaused)
{
	if(aamp)
	{
		if(mAsyncTuneEnabled)
		{

			mScheduler.ScheduleTask(AsyncTaskObj([keepPaused](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						instance->SeekInternal(AAMP_SEEK_TO_LIVE_POSITION, keepPaused);
					}, (void *) this,__FUNCTION__));
		}
		else
		{
			SeekInternal(AAMP_SEEK_TO_LIVE_POSITION, keepPaused);
		}
	}
}

/**
 *  @brief Set slow motion player speed.
 */
void PlayerInstanceAAMP::SetSlowMotionPlayRate( float rate )
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	AAMPLOG_WARN("SetSlowMotionPlay(%f)", rate );

	if (aamp->mpStreamAbstractionAAMP)
	{
		if (aamp->mbPlayEnabled && aamp->pipeline_paused)
		{
			//Clear pause state flag & resume download
			aamp->pipeline_paused = false;
			aamp->ResumeDownloads();
		}

		if(AAMP_SLOWMOTION_RATE == rate)
		{
			aamp->mSetPlayerRateAfterFirstframe=true;
			aamp->playerrate=rate;
		}
		AAMPLOG_WARN("SetSlowMotionPlay(%f) %lf", rate, aamp->seek_pos_seconds );
		aamp->AcquireStreamLock();
		aamp->TeardownStream(false);
		aamp->rate = AAMP_NORMAL_PLAY_RATE;
		aamp->TuneHelper(eTUNETYPE_SEEK);
		aamp->ReleaseStreamLock();
	}
	else
	{
		AAMPLOG_WARN("SetSlowMotionPlay rate[%f] - mpStreamAbstractionAAMP[%p] state[%d]", aamp->rate, aamp->mpStreamAbstractionAAMP, state);
	}
}

/**
 *  @brief Seek to a time and playback with a new rate.
 */
void PlayerInstanceAAMP::SetRateAndSeek(int rate, double secondsRelativeToTuneTime)
{
	TuneType tuneType = eTUNETYPE_SEEK;

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
		AAMPLOG_WARN("mRepairIframes is true, setting actual rate %d for the received rate %d", getWorkingTrickplayRate(rate), rate);
		rate = getWorkingTrickplayRate(rate);
	}

	if (secondsRelativeToTuneTime == AAMP_SEEK_TO_LIVE_POSITION)
	{
		if (aamp->IsLive())
		{
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}
		else
		{
			tuneType = eTUNETYPE_SEEKTOEND;
		}
	}

	if (aamp->mpStreamAbstractionAAMP)
	{
		if ((!aamp->mIsIframeTrackPresent && rate != AAMP_NORMAL_PLAY_RATE && rate != 0))
		{
			AAMPLOG_WARN("Ignoring trickplay. No iframe tracks in stream");
			aamp->NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE); // Send speed change event to XRE to reset the speed to normal play since the trickplay ignored at player level.
			return;
		}
		/* Clear setting playerrate flag */
		aamp->mSetPlayerRateAfterFirstframe=false;
		aamp->AcquireStreamLock();
		aamp->TeardownStream(false);
		aamp->seek_pos_seconds = secondsRelativeToTuneTime;
		aamp->rate = rate;
		aamp->TuneHelper(tuneType);
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
		AAMPLOG_WARN("aamp_SetRateAndSeek rate[%f] - mpStreamAbstractionAAMP[%p] state[%d]", aamp->rate, aamp->mpStreamAbstractionAAMP, state);
	}
}

/**
 *  @brief Set video rectangle.
 */
void PlayerInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp)
	{
		aamp->SetVideoRectangle(x, y, w, h);
	}
}

/**
 *  @brief Set video zoom.
 */
void PlayerInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp){
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
	}// end of if aamp
}

/**
 *  @brief Enable/ Disable Video.
 */
void PlayerInstanceAAMP::SetVideoMute(bool muted)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp){
		AAMPLOG_WARN(" mute == %s subtitles_muted == %s", muted?"true":"false", aamp->subtitles_muted?"true":"false");
		aamp->video_muted = muted;
		
		//If lock could not be acquired, then cache it
		if(aamp->TryStreamLock())
		{
			if (aamp->mpStreamAbstractionAAMP)
			{
				aamp->SetVideoMute(muted);
				SetCCStatus(muted ? false : !aamp->subtitles_muted);
			}
			else
			{
				AAMPLOG_WARN("Player is in state eSTATE_IDLE, value has been cached");
				aamp->mApplyCachedVideoMute = true;
			}
			aamp->ReleaseStreamLock();
		}
		else
		{
			AAMPLOG_WARN("StreamLock is not available, value has been cached");
			aamp->mApplyCachedVideoMute = true;
		}
	}
}

/**
 *   @brief Enable/ Disable Subtitles.
 *
 *   @param  muted - true to disable subtitles, false to enable subtitles.
 */
void PlayerInstanceAAMP::SetSubtitleMute(bool muted)
{
	ERROR_STATE_CHECK_VOID();
	
	AAMPLOG_WARN(" mute == %s", muted?"true":"false");
	aamp->subtitles_muted = muted;
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		aamp->SetSubtitleMute(muted);
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
	AAMPLOG_WARN(" volume == %d", volume);
	if(aamp){
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
	}// end of if aamp
}

/**
 *  @brief Set Audio language.
 */
void PlayerInstanceAAMP::SetLanguage(const char* language)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp){
		PrivAAMPState state;
		aamp->GetState(state);
		if (mAsyncTuneEnabled && state != eSTATE_IDLE && state != eSTATE_RELEASED)
		{
			std::string sLanguage = std::string(language);
			mScheduler.ScheduleTask(AsyncTaskObj(
				[sLanguage](void *data)
				{
					PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
					instance->SetPreferredLanguages(sLanguage.c_str());
				}, (void *) this,__FUNCTION__));
		}
		else
		{
			SetPreferredLanguages(language);
		}
	}// end of if
}

/**
 *  @brief Set array of subscribed tags.
 */
void PlayerInstanceAAMP::SetSubscribedTags(std::vector<std::string> subscribedTags)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp){
	aamp->subscribedTags = subscribedTags;

	for (int i=0; i < aamp->subscribedTags.size(); i++) {
	        AAMPLOG_WARN("    subscribedTags[%d] = '%s'", i, subscribedTags.at(i).data());
	}
	}// end of if aamp
}

/**
 *  @brief Subscribe array of http response headers.
 */
void PlayerInstanceAAMP::SubscribeResponseHeaders(std::vector<std::string> responseHeaders)
{
	ERROR_STATE_CHECK_VOID();

	if(aamp){
	aamp->responseHeaders = responseHeaders;

	for (int header=0; header < aamp->responseHeaders.size(); header++) {
	    AAMPLOG_INFO("    responseHeaders[%d] = '%s'", header, responseHeaders.at(header).data());
	}
	} // end of if aaamp
}

#ifdef SUPPORT_JS_EVENTS 

/**
 *  @brief Load AAMP JS object in the specified JS context.
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
 *  @brief Unload AAMP JS object in the specified JS context.
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
 *  @brief Support multiple listeners for multiple event type
 */
void PlayerInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	if(aamp){
	aamp->AddEventListener(eventType, eventListener);
	}
}

/**
 *  @brief Remove event listener for eventType.
 */
void PlayerInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	if(aamp){
	aamp->RemoveEventListener(eventType, eventListener);
	}
}

/**
 *  @brief To check whether the asset is live or not.
 */
bool PlayerInstanceAAMP::IsLive()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(false);
	bool isLive = false;
	if(aamp) isLive = aamp->IsLive();
	return isLive;
}
/**
 *  @brief Get jsinfo config value (default false) 
 */

bool PlayerInstanceAAMP::IsJsInfoLoggingEnabled(void)

 {
	 return  ISCONFIGSET(eAAMPConfig_JsInfoLogging);
 }

/**
 *  @brief Get current audio language.
 */
const char* PlayerInstanceAAMP::GetCurrentAudioLanguage(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL("");
	static char lang[MAX_LANGUAGE_TAG_LENGTH];
	lang[0] = 0;
	if(aamp && aamp->mpStreamAbstractionAAMP){

	int trackIndex = GetAudioTrack();
	if( trackIndex>=0 )
	{
		std::vector<AudioTrackInfo> trackInfo = aamp->mpStreamAbstractionAAMP->GetAvailableAudioTracks();
		if (!trackInfo.empty())
		{
			strncpy(lang, trackInfo[trackIndex].language.c_str(), sizeof(lang));
			lang[sizeof(lang)-1] = '\0';  //CID:173324 - Buffer size warning
		}
	}
	}// end of if aamp
	return lang;
}

/**
 *  @brief Get current drm
 */
const char* PlayerInstanceAAMP::GetCurrentDRM(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL("");
	if(aamp){
	std::shared_ptr<AampDrmHelper> helper = aamp->GetCurrentDRM();
	if (helper) 
	{
		return helper->friendlyName().c_str();
	}
	}// end of if aamp
	return "NONE";
}

/**
 *  @brief Applies the custom http headers for page (Injector bundle) received from the js layer
 */
void PlayerInstanceAAMP::AddPageHeaders(std::map<std::string, std::string> pageHeaders)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp && ISCONFIGSET(eAAMPConfig_AllowPageHeaders))
	{
		for(auto &header : pageHeaders)
		{
			AAMPLOG_INFO("PrivateInstanceAAMP: applying the http header key: %s, value: %s", header.first.c_str(), header.second.c_str());
			aamp->AddCustomHTTPHeader(header.first, std::vector<std::string>{header.second}, false);
		}
	}
}

/**
 *   @brief Add/Remove a custom HTTP header and value.
 */
void PlayerInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp){
	aamp->AddCustomHTTPHeader(headerName, headerValue, isLicenseHeader);
	}
}

/**
 *  @brief Set License Server URL.
 */
void PlayerInstanceAAMP::SetLicenseServerURL(const char *url, DRMSystems type)
{
	ERROR_STATE_CHECK_VOID();
	if(aamp){
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
	}// end of if aamp
}

/**
 *  @brief Indicates if session token has to be used with license request or not.
 */
void PlayerInstanceAAMP::SetAnonymousRequest(bool isAnonymous)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AnonymousLicenseRequest,isAnonymous);
}

/**
 *  @brief Indicates average BW to be used for ABR Profiling.
 */
void PlayerInstanceAAMP::SetAvgBWForABR(bool useAvgBW)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AvgBWForABR,useAvgBW);
}

/**
 *  @brief SetPreCacheTimeWindow Function to Set PreCache Time
 */
void PlayerInstanceAAMP::SetPreCacheTimeWindow(int nTimeWindow)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PreCachePlaylistTime,nTimeWindow);
}

/**
 *  @brief Set VOD Trickplay FPS.
 */
void PlayerInstanceAAMP::SetVODTrickplayFPS(int vodTrickplayFPS)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS);
}

/**
 *  @brief Set Linear Trickplay FPS.
 */
void PlayerInstanceAAMP::SetLinearTrickplayFPS(int linearTrickplayFPS)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LinearTrickPlayFPS,linearTrickplayFPS);
}

/**
 *  @brief Set Live Offset
 */
void PlayerInstanceAAMP::SetLiveOffset(double liveoffset)
{
	PLAYING_STATE_CHECK();
	aamp->SetLiveOffsetAppRequest(true);
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LiveOffset, liveoffset);
}

/**
 *  @brief Set Live Offset
 */
void PlayerInstanceAAMP::SetLiveOffset4K(double liveoffset)
{
	PLAYING_STATE_CHECK();
	aamp->SetLiveOffsetAppRequest(true);
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LiveOffset4K, liveoffset);
}

/**
 *  @brief To set the error code to be used for playback stalled error.
 */
void PlayerInstanceAAMP::SetStallErrorCode(int errorCode)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StallErrorCode,errorCode);	
}

/**
 *  @brief To set the timeout value to be used for playback stall detection.
 */
void PlayerInstanceAAMP::SetStallTimeout(int timeoutMS)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StallTimeoutMS,timeoutMS);	
}

/**
 *  @brief To set the Playback Position reporting interval.
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
 *  @brief To set the max retry attempts for init frag curl timeout failures
 */
void PlayerInstanceAAMP::SetInitFragTimeoutRetryCount(int count)
{
	if(count >= 0)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_InitFragmentRetryCount,count);
	}
}

/**
 *  @brief To get the current playback position.
 */
double PlayerInstanceAAMP::GetPlaybackPosition()
{
	ERROR_STATE_CHECK_VAL(0.00);
	return aamp->GetPositionSeconds();
}

/**
 *  @brief To get the current asset's duration.
 */
double PlayerInstanceAAMP::GetPlaybackDuration()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0.00);
	return (aamp->GetDurationMs() / 1000.00);
}


/**
 *  @fn GetId
 *
 *  @return returns unique id of player,
 */
 int PlayerInstanceAAMP::GetId(void)
 {
	 int iPlayerId = -1;

	 if(NULL != aamp)
	 {
		 iPlayerId = aamp->mPlayerId;
	 }

	 return iPlayerId;
 }

/**
 *  @brief To get the current AAMP state.
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
 *  @brief To get the bitrate of current video profile.
 */
long PlayerInstanceAAMP::GetVideoBitrate(void)
{
	long bitrate = 0;
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	if(aamp){
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrate = aamp->mpStreamAbstractionAAMP->GetVideoBitrate();
	}
	aamp->ReleaseStreamLock();
	} // if aamp
	return bitrate;
}

/**
 *  @brief To set a preferred bitrate for video profile.
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
 *  @brief To get the bitrate of current audio profile.
 */
long PlayerInstanceAAMP::GetAudioBitrate(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	long bitrate = 0;
	if(aamp){
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrate = aamp->mpStreamAbstractionAAMP->GetAudioBitrate();
	}
	aamp->ReleaseStreamLock();
	} // end of if
	return bitrate;
}

/**
 *  @brief To set a preferred bitrate for audio profile.
 */
void PlayerInstanceAAMP::SetAudioBitrate(long bitrate)
{
	//no-op for now
}

/**
 *  @brief To get video zoom mode
 */
int PlayerInstanceAAMP::GetVideoZoom(void)
{
        ERROR_STATE_CHECK_VAL(0);
        return aamp->zoom_mode;
}

/**
 *  @brief To get video mute status
 */
bool PlayerInstanceAAMP::GetVideoMute(void)
{
        ERROR_STATE_CHECK_VAL(0);
        return aamp->video_muted;
}

/**
 *  @brief To get the current audio volume.
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
 */
int PlayerInstanceAAMP::GetPlaybackRate(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(0);
	return (aamp->pipeline_paused ? 0 : aamp->rate);
}

/**
 *  @brief To get the available video bitrates.
 */
std::vector<long> PlayerInstanceAAMP::GetVideoBitrates(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::vector<long>());
	std::vector<long> bitrates;
	if(aamp){
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrates = aamp->mpStreamAbstractionAAMP->GetVideoBitrates();
	}
	aamp->ReleaseStreamLock();
	} // end of if aamp
	return bitrates;
}

/**
 *  @brief To get the available manifest.
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
 *  @brief To get the available audio bitrates.
 */
std::vector<long> PlayerInstanceAAMP::GetAudioBitrates(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::vector<long>());
	std::vector<long> bitrates;
	if(aamp){
	aamp->AcquireStreamLock();
	if (aamp->mpStreamAbstractionAAMP)
	{
		bitrates = aamp->mpStreamAbstractionAAMP->GetAudioBitrates();
	}
	aamp->ReleaseStreamLock();
	}
	return bitrates;
}

/**
 *  @brief To set the initial bitrate value.
 */
void PlayerInstanceAAMP::SetInitialBitrate(long bitrate)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate,bitrate);
}

/**
 *  @brief To get the initial bitrate value.
 */
long PlayerInstanceAAMP::GetInitialBitrate(void)
{
	long bitrate;
	GETCONFIGVALUE(eAAMPConfig_DefaultBitrate,bitrate);
	return bitrate;
}

/**
 *  @brief To set the initial bitrate value for 4K assets.
 */
void PlayerInstanceAAMP::SetInitialBitrate4K(long bitrate4K)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DefaultBitrate4K,bitrate4K);
}

/**
 *  @brief To get the initial bitrate value for 4K assets.
 */
long PlayerInstanceAAMP::GetInitialBitrate4k(void)
{
	long bitrate4K;
	GETCONFIGVALUE(eAAMPConfig_DefaultBitrate4K,bitrate4K);
	return bitrate4K;
}

/**
 *   @brief To override default curl timeout for playlist/fragment downloads
 */
void PlayerInstanceAAMP::SetNetworkTimeout(double timeout)
{
        ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NetworkTimeout,timeout);
}

/**
 *   @brief Optionally override default HLS main manifest download timeout with app-specific value.
 */
void PlayerInstanceAAMP::SetManifestTimeout(double timeout)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ManifestTimeout,timeout);
}

/**
 *  @brief Optionally override default HLS main manifest download timeout with app-specific value.
 */
void PlayerInstanceAAMP::SetPlaylistTimeout(double timeout)
{
        ERROR_STATE_CHECK_VOID();        
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistTimeout,timeout);
}

/**
 *  @brief To set the download buffer size value
 */
void PlayerInstanceAAMP::SetDownloadBufferSize(int bufferSize)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MaxFragmentCached,bufferSize);
}

/**
 *  @brief Set Preferred DRM.
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
 *  @brief Set Stereo Only Playback.
 */
void PlayerInstanceAAMP::SetStereoOnlyPlayback(bool bValue)
{
	ERROR_STATE_CHECK_VOID();	
	if(bValue)
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableEC3,true);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC3,true);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC4,true);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,true);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ForceEC3,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StereoOnly,true);
	}
	else
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableEC3,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC3,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableAC4,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_DisableATMOS,false);
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_StereoOnly,false);
	}
}

/**
 *  @brief Disable 4K Support in player
 */
void PlayerInstanceAAMP::SetDisable4K(bool bValue)
{
        ERROR_STATE_CHECK_VOID();
        SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_Disable4K,bValue);
}


/**
 *  @brief Set Bulk TimedMetadata Reporting flag
 */
void PlayerInstanceAAMP::SetBulkTimedMetaReport(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_BulkTimedMetaReport,bValue);
}

/**
 *  @brief Set unpaired discontinuity retune flag
 */
void PlayerInstanceAAMP::SetRetuneForUnpairedDiscontinuity(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RetuneForUnpairDiscontinuity,bValue);
}

/**
 *  @brief Set retune configuration for gstpipeline internal data stream error.
 */
void PlayerInstanceAAMP::SetRetuneForGSTInternalError(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RetuneForGSTError,bValue);
}

/**
 *  @brief Setting the alternate contents' (Ads/blackouts) URL
 */
void PlayerInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->SetAlternateContents(adBreakId, adId, url);
}

/**
 *  @brief To set the network proxy
 */
void PlayerInstanceAAMP::SetNetworkProxy(const char * proxy)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NetworkProxy ,(std::string)proxy);
}

/**
 *  @brief To set the proxy for license request
 */
void PlayerInstanceAAMP::SetLicenseReqProxy(const char * licenseProxy)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LicenseProxy ,(std::string)licenseProxy);
}

/**
 *  @brief To set the curl stall timeout value
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
 *  @brief To set the curl download start timeout
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
 *  @brief To set the curl download low bandwidth timeout value
 */
void PlayerInstanceAAMP::SetDownloadLowBWTimeout(long lowBWTimeout)
{
	ERROR_STATE_CHECK_VOID();
	if( lowBWTimeout >= 0 )
	{
		SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CurlDownloadLowBWTimeout,lowBWTimeout);
	}
}

/**
 *  @brief Set preferred subtitle language.
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
 *  @brief Set parallel playlist download config value.
 */
void PlayerInstanceAAMP::SetParallelPlaylistDL(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistParallelFetch,bValue);
}

/**
 *  @brief Set parallel playlist download config value for linear
 */
void PlayerInstanceAAMP::SetParallelPlaylistRefresh(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PlaylistParallelRefresh,bValue);
}

/**
 *  @brief Set Westeros sink configuration
 */
void PlayerInstanceAAMP::SetWesterosSinkConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_UseWesterosSink,bValue);
}

/**
 *  @brief Set license caching
 */
void PlayerInstanceAAMP::SetLicenseCaching(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SetLicenseCaching,bValue);	
}

/**
 *  @brief Set Display resolution check for video profile filtering
 */
void PlayerInstanceAAMP::SetOutputResolutionCheck(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_LimitResolution,bValue);
}

/**
 *  @brief Set Matching BaseUrl Config Configuration
 */
void PlayerInstanceAAMP::SetMatchingBaseUrlConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_MatchBaseUrl,bValue);
}

/**
 *  @brief Configure New ABR Enable/Disable
 */
void PlayerInstanceAAMP::SetNewABRConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ABRBufferCheckEnabled,bValue);
	// Piggybagged following setting along with NewABR for Peacock
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,bValue);
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,bValue);
}

/**
 *  @brief to configure URI parameters for fragment downloads
 */
void PlayerInstanceAAMP::SetPropagateUriParameters(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PropogateURIParam,bValue);
}

/**
 *  @brief to optionally configure simulated per-download network latency for negative testing
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
 */
void PlayerInstanceAAMP::SetSslVerifyPeerConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_SslVerifyPeer,bValue);
}


/**
 *   @brief Set audio track
 */
void PlayerInstanceAAMP::SetAudioTrack(std::string language, std::string rendition, std::string type, std::string codec, unsigned int channel, std::string label)
{
	if(aamp)
	{

		if (mAsyncTuneEnabled)
		{
			mScheduler.ScheduleTask(AsyncTaskObj(
						[language,rendition,type,codec,channel, label](void *data)
						{
							PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
							instance->SetAudioTrackInternal(language,rendition,type,codec,channel, label);
						}, (void *) this,__FUNCTION__));
		}
		else
		{
			SetAudioTrackInternal(language,rendition,type,codec,channel,label);
		}
	}
}

/**
 *  @brief Set audio track by audio parameters like language , rendition, codec etc..
 */
void PlayerInstanceAAMP::SetAudioTrackInternal(std::string language,  std::string rendition, std::string type, std::string codec, unsigned int channel, std::string label)
{
	aamp->mAudioTuple.clear();
	aamp->mAudioTuple.setAudioTrackTuple(language, rendition, codec, channel);
	/* Now we have an option to set language and rendition only*/
	SetPreferredLanguages( language.empty()?NULL:language.c_str(),
							rendition.empty()?NULL:rendition.c_str(),
							type.empty()?NULL:type.c_str(), 
							codec.empty()?NULL:codec.c_str(),
							label.empty()?NULL:label.c_str());
}

/**
 *  @brief Set optional preferred codec list
 */
void PlayerInstanceAAMP::SetPreferredCodec(const char *codecList)
{
	aamp->SetPreferredLanguages(NULL, NULL, NULL, codecList, NULL);
}

/**
 *  @brief Set optional preferred label list
 */
void PlayerInstanceAAMP::SetPreferredLabels(const char *labelList)
{
	aamp->SetPreferredLanguages(NULL, NULL, NULL, NULL, labelList);
}

/**
 *  @brief Set optional preferred rendition list
 */
void PlayerInstanceAAMP::SetPreferredRenditions(const char *renditionList)
{
	aamp->SetPreferredLanguages(NULL, renditionList, NULL, NULL, NULL);
}

/**
 *  @brief Get preferred audio prioperties
 */
std::string PlayerInstanceAAMP::GetPreferredAudioProperties()
{
	return aamp->GetPreferredAudioProperties();
}

/**
 *   @brief Get preferred text prioperties
 *
 *   @return text preferred proprties in json format
 */
std::string PlayerInstanceAAMP::GetPreferredTextProperties()
{
	return aamp->GetPreferredTextProperties();
}

/**
 *  @brief Set optional preferred language list
 */
void PlayerInstanceAAMP::SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char* codecList, const char* labelList )
{
	aamp->SetPreferredLanguages(languageList, preferredRendition, preferredType, codecList, labelList);
}

/**
 *  @brief Set optional preferred language list
 */
void PlayerInstanceAAMP::SetPreferredTextLanguages(const char *param)
{
	aamp->SetPreferredTextLanguages(param);
}

/**
 *  @brief Get Preferred DRM.
 */
DRMSystems PlayerInstanceAAMP::GetPreferredDRM()
{
	return aamp->GetPreferredDRM();
}

/**
 *  @brief Get current preferred language list
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
 *  @brief Configure New AdBreaker Enable/Disable
 */
void PlayerInstanceAAMP::SetNewAdBreakerConfig(bool bValue)
{	
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NewDiscontinuity,bValue);
	// Piggyback the PDT based processing for new Adbreaker processing for peacock.
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_HLSAVTrackSyncUsingStartTime,bValue);
}

/**
 *  @brief Get available video tracks.
 */
std::string PlayerInstanceAAMP::GetAvailableVideoTracks()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetAvailableVideoTracks();
}

/**
 *  @brief Set video tracks.
 */
void PlayerInstanceAAMP::SetVideoTracks(std::vector<long> bitrates)
{
	return aamp->SetVideoTracks(bitrates);
}

/**
 *  @brief Get available audio tracks.
 */
std::string PlayerInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetAvailableAudioTracks(allTrack);
}

/**
 *  @brief Get current audio track index
 */
std::string PlayerInstanceAAMP::GetAudioTrackInfo()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetAudioTrackInfo();
}

/**
 *  @brief Get current audio track index
 */
std::string PlayerInstanceAAMP::GetTextTrackInfo()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetTextTrackInfo();
}

/**
 *  @brief Get available text tracks.
 *
 *  @return std::string JSON formatted list of text tracks
 */
std::string PlayerInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());

	return aamp->GetAvailableTextTracks(allTrack);
}

/**
 *   @brief Get the video window co-ordinates
 */
std::string PlayerInstanceAAMP::GetVideoRectangle()
{
	ERROR_STATE_CHECK_VAL(std::string());

	return aamp->GetVideoRectangle();
}

/**
 *  @brief Set the application name which has created PlayerInstanceAAMP, for logging purposes
 */
void PlayerInstanceAAMP::SetAppName(std::string name)
{
	aamp->SetAppName(name);
}

/**
 *  @brief Enable/disable the native CC rendering feature
 */
void PlayerInstanceAAMP::SetNativeCCRendering(bool enable)
{
#ifdef AAMP_CC_ENABLED
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_NativeCCRendering,enable);
#endif
}

/**
 *  @brief To set the vod-tune-event according to the player.
 */
void PlayerInstanceAAMP::SetTuneEventConfig(int tuneEventType)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_TuneEventConfig,tuneEventType);
}

/**
 *  @brief Set video rectangle property
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
 *  @brief Set audio track
 */
void PlayerInstanceAAMP::SetAudioTrack(int trackId)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	if(aamp && aamp->mpStreamAbstractionAAMP){

		std::vector<AudioTrackInfo> tracks = aamp->mpStreamAbstractionAAMP->GetAvailableAudioTracks();
		if (!tracks.empty() && (trackId >= 0 && trackId < tracks.size()))
		{
			if (mAsyncTuneEnabled)
			{
				mScheduler.ScheduleTask(AsyncTaskObj(
						[tracks , trackId](void *data)
						{
							PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
							instance->SetPreferredLanguages(tracks[trackId].language.c_str(), tracks[trackId].rendition.c_str(), tracks[trackId].accessibilityType.c_str(), tracks[trackId].codec.c_str(), tracks[trackId].label.c_str());
						}, (void *) this,__FUNCTION__));
			}
			else
			{
				SetPreferredLanguages(tracks[trackId].language.c_str(), tracks[trackId].rendition.c_str(), tracks[trackId].accessibilityType.c_str(), tracks[trackId].codec.c_str(), tracks[trackId].label.c_str());
			}
		}
	} // end of if
}

/**
 *  @brief Get current audio track index
 */
int PlayerInstanceAAMP::GetAudioTrack()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(-1);

	return aamp->GetAudioTrack();
}

/**
 *  @brief Set text track
 */
void PlayerInstanceAAMP::SetTextTrack(int trackId, char *ccData)
{
        ERROR_OR_IDLE_STATE_CHECK_VOID();
	if(aamp && aamp->mpStreamAbstractionAAMP)
	{

		std::vector<TextTrackInfo> tracks = aamp->mpStreamAbstractionAAMP->GetAvailableTextTracks();
		AAMPLOG_INFO("trackId: %d tracks size %d", trackId, tracks.size());
		if (!tracks.empty() && (MUTE_SUBTITLES_TRACKID == trackId || (trackId >= 0 && trackId < tracks.size())))
		{
			if (mAsyncTuneEnabled)
			{
				mScheduler.ScheduleTask(AsyncTaskObj(
							[trackId, ccData ](void *data)
							{
								PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
								instance->SetTextTrackInternal(trackId, ccData);
							}, (void *) this,__FUNCTION__));
			}
			else
			{
				SetTextTrackInternal(trackId, ccData);
			}
		}
		else
			SetTextTrackInternal(trackId, ccData);
	}
}

/**
 *  @brief Set text track by Id
 */
void PlayerInstanceAAMP::SetTextTrackInternal(int trackId, char *data)
{
	if(aamp && aamp->mpStreamAbstractionAAMP)
	{
		aamp->SetTextTrack(trackId, data);
	}
}


/**
 *  @brief Get current text track index
 */
int PlayerInstanceAAMP::GetTextTrack()
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(-1);

	return aamp->GetTextTrack();
}

/**
 *  @brief Set CC visibility on/off
 */
void PlayerInstanceAAMP::SetCCStatus(bool enabled)
{
	ERROR_STATE_CHECK_VOID();

	aamp->SetCCStatus(enabled);
}

/**
 *  @brief Get CC visibility on/off
 */
bool PlayerInstanceAAMP::GetCCStatus(void)
{
	return aamp->GetCCStatus();
}

/**
 *  @brief Set style options for text track rendering
 */
void PlayerInstanceAAMP::SetTextStyle(const std::string &options)
{
	ERROR_STATE_CHECK_VOID();

	aamp->SetTextStyle(options);
}

/**
 *  @brief Get style options for text track rendering
 */
std::string PlayerInstanceAAMP::GetTextStyle()
{
	ERROR_STATE_CHECK_VAL(std::string());

	return aamp->GetTextStyle();
}

/**
 *  @brief Set Initial profile ramp down limit.
 */
void PlayerInstanceAAMP::SetInitRampdownLimit(int limit)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_InitRampDownLimit,limit);
}


/**
 *  @brief Set the CEA format for force setting
 */
void PlayerInstanceAAMP::SetCEAFormat(int format)
{
#ifdef AAMP_CC_ENABLED	
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CEAPreferred,format);
#endif
}


/**
 *   @brief To get the available bitrates for thumbnails.
 */
std::string PlayerInstanceAAMP::GetAvailableThumbnailTracks(void)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());
	return aamp->GetThumbnailTracks();
}

/**
 *  @brief To set a preferred bitrate for thumbnail profile.
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
 *  @brief To get preferred thumbnails for the duration.
 */
std::string PlayerInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	ERROR_OR_IDLE_STATE_CHECK_VAL(std::string());
	return aamp->GetThumbnails(tStart, tEnd);
}

/**
 *  @brief Set the session token for player
 */
void PlayerInstanceAAMP::SetSessionToken(std::string sessionToken)
{
	ERROR_STATE_CHECK_VOID();	
	// Stored as tune setting , this will get cleared after one tune session
	SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_AuthToken,sessionToken);
	aamp->mDynamicDrmDefaultconfig.authToken = sessionToken;
	return;
}

/**
 *  @brief Enable seekable range values in progress event
 */
void PlayerInstanceAAMP::EnableSeekableRange(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_EnableSeekRange,bValue);
}

/**
 *  @brief Enable video PTS reporting in progress event
 */
void PlayerInstanceAAMP::SetReportVideoPTS(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ReportVideoPTS,bValue);
}

/**
 *  @brief Disable Content Restrictions - unlock
 */
void PlayerInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->DisableContentRestrictions(grace, time, eventChange);
}

/**
 *  @brief Enable Content Restrictions - lock
 */
void PlayerInstanceAAMP::EnableContentRestrictions()
{
	ERROR_OR_IDLE_STATE_CHECK_VOID();
	aamp->EnableContentRestrictions();
}

/**
 *  @brief Manage async tune configuration for specific contents
 */
void PlayerInstanceAAMP::ManageAsyncTuneConfig(const char* mainManifestUrl)
{
	MediaFormat mFormat = eMEDIAFORMAT_UNKNOWN;
	mFormat = aamp->GetMediaFormatType(mainManifestUrl);
	if(mFormat == eMEDIAFORMAT_HDMI || mFormat == eMEDIAFORMAT_COMPOSITE || mFormat == eMEDIAFORMAT_OTA)
	{
		SetAsyncTuneConfig(false);
	}
}

/**
 *  @brief Set async tune configuration
 */
void PlayerInstanceAAMP::SetAsyncTuneConfig(bool bValue)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AsyncTune,bValue);
	// Start it for the playerinstance if default not started and App wants
	// Stop Async operation for the playerinstance if default started and App doesnt want 
	AsyncStartStop();
}

/**
 *  @brief Enable/Disable async operation
 */
void PlayerInstanceAAMP::AsyncStartStop()
{
	// Check if global configuration is set to false
	// Additional check added here, since this API can be called from jsbindings/native app
	mAsyncTuneEnabled = ISCONFIGSET(eAAMPConfig_AsyncTune);
	if (mAsyncTuneEnabled && !mAsyncRunning)
	{
		AAMPLOG_WARN("Enable async tune operation!!" );
		mAsyncRunning = true;
		//mScheduler.StartScheduler();
		aamp->SetEventPriorityAsyncTune(true);		
	}
	else if(!mAsyncTuneEnabled && mAsyncRunning)
	{
		AAMPLOG_WARN("Disable async tune operation!!");
		aamp->SetEventPriorityAsyncTune(false);
		//mScheduler.StopScheduler();
		mAsyncRunning = false;
	}
}

/**
 *  @brief Enable/disable configuration to persist ABR profile over seek/SAP
 */
void PlayerInstanceAAMP::PersistBitRateOverSeek(bool bValue)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PersistentBitRateOverSeek,bValue);	
}


/**
 *  @brief Stop playback and release resources.
 */
void PlayerInstanceAAMP::StopInternal(bool sendStateChangeEvent)
{
	PrivAAMPState state;

	aamp->StopPausePositionMonitoring("Stop() called");

	aamp->GetState(state);
	if(!aamp->IsTuneCompleted())
	{
		aamp->TuneFail(true);

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
	aamp->mIsStream4K = false;
}

/**
 *  @brief To set preferred paused state behavior
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
 *  @brief To set UseAbsoluteTimeline for DASH
 */
void PlayerInstanceAAMP::SetUseAbsoluteTimeline(bool configState)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_UseAbsoluteTimeline,configState);

}

/**
 *  @brief To set the repairIframes flag
 */
void PlayerInstanceAAMP::SetRepairIframes(bool configState)
{
	ERROR_STATE_CHECK_VOID();
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RepairIframes,configState);

}

/**
 *  @brief InitAAMPConfig - Initialize the media player session with json config
 */
bool PlayerInstanceAAMP::InitAAMPConfig(char *jsonStr)
{
	bool retVal = false;
	cJSON *cfgdata = NULL;
	if(jsonStr)
	{
		cfgdata = cJSON_Parse(jsonStr);
		if(cfgdata != NULL)
		{
			retVal = mConfig.ProcessConfigJson(cfgdata,AAMP_APPLICATION_SETTING);
		}
	}
	mConfig.DoCustomSetting(AAMP_APPLICATION_SETTING);
	if(GETCONFIGOWNER(eAAMPConfig_AsyncTune) == AAMP_APPLICATION_SETTING)
	{
		AsyncStartStop();
	}
	
	if(cfgdata != NULL){
		cJSON *drmConfig = cJSON_GetObjectItem(cfgdata,"drmConfig");
		if(drmConfig) {
			std::string LicenseServerUrl = "";
			GETCONFIGVALUE(eAAMPConfig_PRLicenseServerUrl, LicenseServerUrl);
			aamp->mDynamicDrmDefaultconfig.licenseEndPoint.insert(std::pair<std::string, std::string>("com.microsoft.playready", LicenseServerUrl.c_str()));
			GETCONFIGVALUE(eAAMPConfig_WVLicenseServerUrl, LicenseServerUrl);
			aamp->mDynamicDrmDefaultconfig.licenseEndPoint.insert(std::pair<std::string, std::string>("com.widevine.alpha",LicenseServerUrl.c_str()));
			GETCONFIGVALUE(eAAMPConfig_CKLicenseServerUrl, LicenseServerUrl);
			aamp->mDynamicDrmDefaultconfig.licenseEndPoint.insert(std::pair<std::string, std::string>("org.w3.clearkey",LicenseServerUrl.c_str()));
			std::string customData = "";
			GETCONFIGVALUE(eAAMPConfig_CustomLicenseData,customData);
			aamp->mDynamicDrmDefaultconfig.customData = customData;
                }
		cJSON_Delete(cfgdata);
	}
	return retVal;
}

/**
 *  @brief GetAAMPConfig - GetAamp Config as JSON string 
 */
std::string PlayerInstanceAAMP::GetAAMPConfig()
{
	std::string jsonStr;
	mConfig.GetAampConfigJSONStr(jsonStr);
	return jsonStr;
}

/**
 *  @brief To set whether the JS playback session is from XRE or not.
 */
void PlayerInstanceAAMP::XRESupportedTune(bool xreSupported)
{
        SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_XRESupportedTune,xreSupported);
}


/**
 *  @brief Set auxiliary language
 */
void PlayerInstanceAAMP::SetAuxiliaryLanguage(const std::string &language)
{
	if(mAsyncTuneEnabled)
	{

		mScheduler.ScheduleTask(AsyncTaskObj([language](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						instance->SetAuxiliaryLanguageInternal(language);
					}, (void *)this , __FUNCTION__));
	}
	else
	{
		SetAuxiliaryLanguageInternal(language);
	}

}

/**
 *  @brief Set auxiluerry track language.
 */
void PlayerInstanceAAMP::SetAuxiliaryLanguageInternal(const std::string &language)
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

				aamp->seek_pos_seconds = aamp->GetPositionSeconds();
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
 *  @brief Set License Custom Data
 */
void PlayerInstanceAAMP::SetLicenseCustomData(const char *customData)
{
    ERROR_STATE_CHECK_VOID();
    SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,std::string(customData));
}

/**
 *  @brief Get playback statistics formated for partner apps
 */
std::string PlayerInstanceAAMP::GetPlaybackStats()
{
	std::string stats;
	if(aamp)
	{
		stats = aamp->GetPlaybackStats();
	}
	return stats;
}

void PlayerInstanceAAMP::ProcessContentProtectionDataConfig(const char *jsonbuffer)
{
	ERROR_STATE_CHECK_VOID();
	AAMPLOG_INFO("ProcessContentProtectionDataConfig received DRM config data from app");
	if(aamp){
		std::vector<uint8_t> tempKeyId;
		DynamicDrmInfo dynamicDrmCache;
		if(aamp->vDynamicDrmData.size()>9)
		{
			aamp->vDynamicDrmData.erase(aamp->vDynamicDrmData.begin());
		}
		int empty_config;
		cJSON *cfgdata = cJSON_Parse(jsonbuffer);
		empty_config = cJSON_GetArraySize(cfgdata);
		if(cfgdata)
		{
			cJSON *arryitem = cJSON_GetObjectItem(cfgdata, "keyID" );
			if(arryitem) {
				cJSON *iterator = NULL;
				cJSON_ArrayForEach(iterator, arryitem) {
					if (cJSON_IsNumber(iterator)) {
						tempKeyId.push_back(iterator->valueint);
					}
				}
				dynamicDrmCache.keyID=tempKeyId;
			}
			else {
				AAMPLOG_WARN("Response message doesn't have keyID ignoring the message");
				return;
			}
			
			//Remove old config if response keyId already in cache
			int iter1 = 0;
			while (iter1 < aamp->vDynamicDrmData.size()) {
				DynamicDrmInfo dynamicDrmCache = aamp->vDynamicDrmData.at(iter1);
				if(tempKeyId == dynamicDrmCache.keyID) {
					AAMPLOG_WARN("Deleting old config and updating new config");
					aamp->vDynamicDrmData.erase(aamp->vDynamicDrmData.begin()+iter1);
					break;
				}
				iter1++;
			}
			cJSON *playReadyObject = cJSON_GetObjectItem(cfgdata, "com.microsoft.playready");
			std::string playreadyurl="";
			if(playReadyObject) {
				playreadyurl = playReadyObject->valuestring;
				AAMPLOG_TRACE("App configured Playready License server URL : %s",playreadyurl.c_str());
				
			}
			dynamicDrmCache.licenseEndPoint.insert(std::pair<std::string, std::string>("com.microsoft.playready",playreadyurl.c_str()));
			
			cJSON *wideVineObject = cJSON_GetObjectItem(cfgdata, "com.widevine.alpha");
			std::string widevineurl = "";
			if(wideVineObject) {
				widevineurl = wideVineObject->valuestring;
				AAMPLOG_TRACE("App configured widevine License server URL : %s",widevineurl.c_str());
			}
			dynamicDrmCache.licenseEndPoint.insert(std::pair<std::string, std::string>("com.widevine.alpha",widevineurl.c_str()));
			
			cJSON *clearKeyObject = cJSON_GetObjectItem(cfgdata, "org.w3.clearkey");
			std::string clearkeyurl = "";
			if(clearKeyObject) {
				clearkeyurl = clearKeyObject->valuestring;
				AAMPLOG_TRACE("App configured clearkey License server URL : %s",clearkeyurl.c_str());
			}
			dynamicDrmCache.licenseEndPoint.insert(std::pair<std::string, std::string>("org.w3.clearkey",clearkeyurl.c_str()));
			
			cJSON *customDataObject = cJSON_GetObjectItem(cfgdata, "customData");
			std::string customdata = "";
			if(customDataObject) {
				customdata = customDataObject->valuestring;
				AAMPLOG_TRACE("App configured customData : %s",customdata.c_str());
			}
			dynamicDrmCache.customData = customdata;
			
			cJSON *authTokenObject = cJSON_GetObjectItem(cfgdata, "authToken");
			std::string authToken = "";
			if(authTokenObject) {
				authToken = authTokenObject->valuestring;
				AAMPLOG_TRACE("App configured authToken : %s",authToken.c_str());
			}
			dynamicDrmCache.authToken = authToken;
			
			cJSON *licenseResponseObject = cJSON_GetObjectItem(cfgdata, "licenseResponse");
			if(licenseResponseObject) {
				std::string licenseResponse = licenseResponseObject->valuestring;
				if(!licenseResponse.empty()) {
					AAMPLOG_TRACE("App configured License Response");
				}
			}
			if(empty_config == 1){
				aamp->mDynamicDrmDefaultconfig.keyID=tempKeyId;
				AAMPLOG_WARN("Received empty config applying default config");	
				aamp->vDynamicDrmData.push_back(aamp->mDynamicDrmDefaultconfig);
				DynamicDrmInfo dynamicDrmCache = aamp->mDynamicDrmDefaultconfig;
				std::map<std::string,std::string>::iterator itr;
				for(itr = dynamicDrmCache.licenseEndPoint.begin();itr!=dynamicDrmCache.licenseEndPoint.end();itr++) {
					if(strcasecmp("com.microsoft.playready",itr->first.c_str())==0) {
						playreadyurl = itr->second;
					}
					if(strcasecmp("com.widevine.alpha",itr->first.c_str())==0) {
						widevineurl = itr->second;
					}
					if(strcasecmp("org.w3.clearkey",itr->first.c_str())==0) {
						clearkeyurl = itr->second;
					}
				}
				authToken = dynamicDrmCache.authToken;
				customdata = dynamicDrmCache.customData;
			}
			else {
				aamp->vDynamicDrmData.push_back(dynamicDrmCache);
			}
			
			if(tempKeyId == aamp->mcurrent_keyIdArray){
				AAMPLOG_WARN("Player received the config for requested keyId applying the configs");
				SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,playreadyurl);
				SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,widevineurl);
				SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,clearkeyurl);
				SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,customdata);
				SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_AuthToken,authToken);
				pthread_mutex_lock(&aamp->mDynamicDrmUpdateLock);
				pthread_cond_signal(&aamp->mWaitForDynamicDRMToUpdate);
				AAMPLOG_WARN("Updated new Content Protection Data Configuration");
				pthread_mutex_unlock(&aamp->mDynamicDrmUpdateLock);
			}

		}
		cJSON_Delete(cfgdata);
	}
}

/**
 *   @brief To set the dynamic drm update on key rotation timeout value.
 *
 *   @param[in] preferred timeout value
 */
void PlayerInstanceAAMP::SetContentProtectionDataUpdateTimeout(int timeout)
{
	ERROR_STATE_CHECK_VOID();
	int timeout_ms = timeout * 1000;
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_ContentProtectionDataUpdateTimeout,timeout);
}

/**
 *  @brief To set Dynamic DRM feature by Application 
 */
void PlayerInstanceAAMP::SetRuntimeDRMConfigSupport(bool DynamicDRMSupported)
{
	SETCONFIGVALUE(AAMP_APPLICATION_SETTING,eAAMPConfig_RuntimeDRMConfig,DynamicDRMSupported);
}

/**
 * @fn IsOOBCCRenderingSupported
 *
 * @return bool, True if Out of Band Closed caption/subtitle rendering supported
 */
bool PlayerInstanceAAMP::IsOOBCCRenderingSupported()
{
#ifdef AAMP_CC_ENABLED
	return AampCCManager::GetInstance()->IsOOBCCRenderingSupported();
#else
	return false;
#endif 
}


/**
 * @}
 */
