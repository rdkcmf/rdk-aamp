/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file AampSmokeTestPlayer.cpp
 * @brief Aamp SmokeTest standalone Player 
 */

#include "AampSmokeTestPlayer.h"

AampPlayer mAampPlayer;
bool AampPlayer::mInitialized = false;
GMainLoop* AampPlayer::mAampGstPlayerMainLoop = NULL;
GThread* AampPlayer::mAampMainLoopThread = NULL;
SmokeTestEventListener* AampPlayer::mEventListener = NULL;
PlayerInstanceAAMP* AampPlayer::mPlayerInstanceAamp = NULL;

AampPlayer :: AampPlayer(): 
	mEnableProgressLog(false),
	mTuneFailureDescription("")
{
};

AampPlayer :: AampPlayer(const AampPlayer &aampPlayer):
	mEnableProgressLog(aampPlayer.mEnableProgressLog),
	mTuneFailureDescription(aampPlayer.mTuneFailureDescription)
{
};

AampPlayer& AampPlayer::operator=(const AampPlayer &aampPlayer)
{
	mEnableProgressLog = aampPlayer.mEnableProgressLog;
	mTuneFailureDescription = aampPlayer.mTuneFailureDescription;
	return *this;
};	

/**
 * @brief Thread to run mainloop (for standalone mode)
 * @param[in] arg user_data
 * @retval void pointer
 */
void * AampPlayer::aampGstPlayerStreamThread(void *arg)
{
	if (mAampPlayer.mAampGstPlayerMainLoop)
	{
		g_main_loop_run(mAampPlayer.mAampGstPlayerMainLoop); // blocks
		printf("aampGstPlayerStreamThread: exited main event loop\n");
	}
	
	g_main_loop_unref(mAampPlayer.mAampGstPlayerMainLoop);
	mAampPlayer.mAampGstPlayerMainLoop = NULL;
	return NULL;
}

/**
 * @brief To initialize Gstreamer and start mainloop (for standalone mode)
 * @param[in] argc number of arguments
 * @param[in] argv array of arguments
 */
void AampPlayer::initPlayerLoop(int argc, char **argv)
{
	if (!mInitialized)
	{
		mInitialized = true;
		gst_init(&argc, &argv);
		mAampGstPlayerMainLoop = g_main_loop_new(NULL, FALSE);
		mAampMainLoopThread = g_thread_new("AAMPGstPlayerLoop", &aampGstPlayerStreamThread, NULL );
	}
	
	mPlayerInstanceAamp = new PlayerInstanceAAMP();
	if( !mEventListener )
	{ 
		printf( "allocating new SmokeTestEventListener\n");
		mEventListener = new SmokeTestEventListener();
	}
	mPlayerInstanceAamp->RegisterEvents(mEventListener);
}

FILE * AampPlayer::getConfigFile(const std::string& cfgFile)
{
	if (cfgFile.empty())
	{
		return NULL;
	}

#ifdef SIMULATOR_BUILD
	std::string cfgBasePath(getenv("HOME"));
	std::string cfgPath = cfgBasePath + cfgFile;
	FILE *f = fopen(cfgPath.c_str(), "rb");
#else
	std::string cfgPath = "/opt" + cfgFile;
	FILE *f = fopen(cfgPath.c_str(), "rb");
#endif

	return f;
}


const char *SmokeTestEventListener::stringifyPrivAAMPState(PrivAAMPState state)
{
	static const char *stateName[] =
	{
		"IDLE",
		"INITIALIZING",
		"INITIALIZED",
		"PREPARING",
		"PREPARED",
		"BUFFERING",
		"PAUSED",
		"SEEKING",
		"PLAYING",
		"STOPPING",
		"STOPPED",
		"COMPLETE",
		"ERROR",
		"RELEASED"
	};
	if( state>=eSTATE_IDLE && state<=eSTATE_RELEASED )
	{
		return stateName[state];
	}
	else
	{
		return "UNKNOWN";
	}
}

/**
 * @brief Implementation of event callback
 * @param e Event
 */
void SmokeTestEventListener::Event(const AAMPEventPtr& e)
{
	switch (e->getType())
	{
		case AAMP_EVENT_STATE_CHANGED:
			{
				StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
				printf("AAMP_EVENT_STATE_CHANGED: %s (%d)\n", mAampPlayer.mEventListener->stringifyPrivAAMPState(ev->getState()), ev->getState());
				break;
			}
		case AAMP_EVENT_SEEKED:
			{
				SeekedEventPtr ev = std::dynamic_pointer_cast<SeekedEvent>(e);
				printf("AAMP_EVENT_SEEKED: new positionMs %f\n", ev->getPosition());
				break;
			}
		case AAMP_EVENT_MEDIA_METADATA:
			{
				MediaMetadataEventPtr ev = std::dynamic_pointer_cast<MediaMetadataEvent>(e);
				std::vector<std::string> languages = ev->getLanguages();
				int langCount = ev->getLanguagesCount();
				printf("AAMP_EVENT_MEDIA_METADATA\n");
				for (int i = 0; i < langCount; i++)
				{
					printf("[] language: %s\n", languages[i].c_str());
				}
				printf("AAMP_EVENT_MEDIA_METADATA\n\tDuration=%ld\n\twidth=%d\n\tHeight=%d\n\tHasDRM=%d\n\tProgreamStartTime=%f\n", ev->getDuration(), ev->getWidth(), ev->getHeight(), ev->hasDrm(), ev->getProgramStartTime());
				int bitrateCount = ev->getBitratesCount();
				std::vector<long> bitrates = ev->getBitrates();
				printf("Bitrates:\n");
				for(int i = 0; i < bitrateCount; i++)
				{
					printf("\tbitrate(%d)=%ld\n", i, bitrates.at(i));
				}
				break;
			}
		case AAMP_EVENT_TUNED:
			{
				printf("AAMP_EVENT_TUNED\n");
				break;
			}
		case AAMP_EVENT_TUNE_FAILED:
			{
				MediaErrorEventPtr ev = std::dynamic_pointer_cast<MediaErrorEvent>(e);
				mAampPlayer.mTuneFailureDescription = ev->getDescription();
				printf("AAMP_EVENT_TUNE_FAILED reason=%s\n",mAampPlayer.mTuneFailureDescription.c_str());
				break;
			}
		case AAMP_EVENT_SPEED_CHANGED:
			{
				SpeedChangedEventPtr ev = std::dynamic_pointer_cast<SpeedChangedEvent>(e);
				printf("AAMP_EVENT_SPEED_CHANGED current rate=%f\n", ev->getRate());
				break;
			}
		case AAMP_EVENT_DRM_METADATA:
			{
				DrmMetaDataEventPtr ev = std::dynamic_pointer_cast<DrmMetaDataEvent>(e);
				printf("AAMP_DRM_FAILED Tune failure:%d\t\naccess status str:%s\t\naccess status val:%d\t\nResponce code:%ld\t\nIs SecClient error:%d\t\n",ev->getFailure(), ev->getAccessStatus().c_str(), ev->getAccessStatusValue(), ev->getResponseCode(), ev->getSecclientError());
				break;
			}
		case AAMP_EVENT_EOS:
			printf("AAMP_EVENT_EOS\n");
			break;
		case AAMP_EVENT_PLAYLIST_INDEXED:
			printf("AAMP_EVENT_PLAYLIST_INDEXED\n");
			break;
		case AAMP_EVENT_PROGRESS:
			{
				ProgressEventPtr ev = std::dynamic_pointer_cast<ProgressEvent>(e);
				if(mAampPlayer.mEnableProgressLog)
				{
					printf("AAMP_EVENT_PROGRESS\n\tDuration=%lf\n\tposition=%lf\n\tstart=%lf\n\tend=%lf\n\tcurrRate=%f\n\tBufferedDuration=%lf\n\tPTS=%lld\n\ttimecode=%s\n",ev->getDuration(),ev->getPosition(),ev->getStart(),ev->getEnd(),ev->getSpeed(),ev->getBufferedDuration(),ev->getPTS(),ev->getSEITimeCode());
				}
			}
			break;
		case AAMP_EVENT_CC_HANDLE_RECEIVED:
			{
				CCHandleEventPtr ev = std::dynamic_pointer_cast<CCHandleEvent>(e);
				printf("AAMP_EVENT_CC_HANDLE_RECEIVED CCHandle=%lu\n",ev->getCCHandle());
				break;
			}
		case AAMP_EVENT_BITRATE_CHANGED:
			{
				BitrateChangeEventPtr ev = std::dynamic_pointer_cast<BitrateChangeEvent>(e);
				printf("AAMP_EVENT_BITRATE_CHANGED\n\tbitrate=%ld\n\tdescription=\"%s\"\n\tresolution=%dx%d@%ffps\n\ttime=%d\n\tposition=%lf\n", ev->getBitrate(), ev->getDescription().c_str(), ev->getWidth(), ev->getHeight(), ev->getFrameRate(), ev->getTime(), ev->getPosition());
				break;
			}
		case AAMP_EVENT_AUDIO_TRACKS_CHANGED:
			printf("AAMP_EVENT_AUDIO_TRACKS_CHANGED\n");
			break;
		case AAMP_EVENT_TEXT_TRACKS_CHANGED:
			printf("AAMP_EVENT_TEXT_TRACKS_CHANGED\n");
			break;
		case AAMP_EVENT_ID3_METADATA:
			printf("AAMP_EVENT_ID3_METADATA\n");
			break;
		case AAMP_EVENT_BLOCKED :
			{
				BlockedEventPtr ev = std::dynamic_pointer_cast<BlockedEvent>(e);
				printf("AAMP_EVENT_BLOCKED Reason:%s\n" ,ev->getReason().c_str());
				break;
			}
		case AAMP_EVENT_CONTENT_GAP :
			{
				ContentGapEventPtr ev = std::dynamic_pointer_cast<ContentGapEvent>(e);
				printf("AAMP_EVENT_CONTENT_GAP\n\tStart:%lf\n\tDuration:%lf\n", ev->getTime(), ev->getDuration());
				break;
			}
		case AAMP_EVENT_WATERMARK_SESSION_UPDATE:
			{
				WatermarkSessionUpdateEventPtr ev = std::dynamic_pointer_cast<WatermarkSessionUpdateEvent>(e);
				printf("AAMP_EVENT_WATERMARK_SESSION_UPDATE SessionHandle:%d Status:%d System:%s\n" ,ev->getSessionHandle(), ev->getStatus(), ev->getSystem().c_str());
				break;
			}
		case AAMP_EVENT_BUFFERING_CHANGED:
			{
				BufferingChangedEventPtr ev = std::dynamic_pointer_cast<BufferingChangedEvent>(e);
				printf("AAMP_EVENT_BUFFERING_CHANGED Sending Buffer Change event status (Buffering): %s", (ev->buffering() ? "End": "Start"));
				break;
			}
		case AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE:
			{
				ContentProtectionDataEventPtr ev =  std::dynamic_pointer_cast<ContentProtectionDataEvent>(e);
				printf("AAMP_EVENT_CONTENT_PROTECTION_UPDATE received stream type %s\n",ev->getStreamType().c_str());
				std::vector<uint8_t> key = ev->getKeyID();
				printf("AAMP_EVENT_CONTENT_PROTECTION_UPDATE received key is ");
				for(int i=0;i<key.size();i++)
					printf("%x",key.at(i)&0xff);
				printf("\n");
				cJSON *root = cJSON_CreateObject();
				cJSON *KeyId = cJSON_CreateArray();
				for(int i=0;i<key.size();i++)
					cJSON_AddItemToArray(KeyId, cJSON_CreateNumber(key.at(i)));
				cJSON_AddItemToObject(root,"keyID",KeyId);
				std::string json = cJSON_Print(root);
				mAampPlayer.mPlayerInstanceAamp->ProcessContentProtectionDataConfig(json.c_str());
				break;
			}
		default:
			break;
	}
}

