/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
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
 * @file AampcliPlaybackCommand.cpp
 * @brief Aampcli Playback Commands handler
 */

#include "AampcliPlaybackCommand.h"

extern VirtualChannelMap mVirtualChannelMap;
extern Aampcli mAampcli;
extern void tsdemuxer_InduceRollover( bool enable );

/**
 * @brief Process command
 * @param cmd command
 */
bool PlaybackCommand::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	bool eventChange = false;
	char lang[MAX_LANGUAGE_TAG_LENGTH];
	char cacheUrl[200];
	int keepPaused = 0;
	int rate = 0;
	double seconds = 0;
	long unlockSeconds = 0;
	long grace = 0;
	long time = -1;
	int ms = 0;
	int playerIndex = -1;

	trim(&cmd);
	while( *cmd==' ' ) cmd++;

	if( cmd[0]=='#' )
	{
		printf( "skipping comment\n" );
	}
	else if(cmd[0] == 0)
	{
		if (playerInstanceAamp->aamp->mpStreamAbstractionAAMP)
		{
			playerInstanceAamp->aamp->mpStreamAbstractionAAMP->DumpProfiles();
		}
		printf("[AAMPCLI] current network bandwidth ~= %ld\n", playerInstanceAamp->aamp->GetPersistedBandwidth());
	}
	else if (strcmp(cmd, "help") == 0)
	{
		showHelp();
	}
	else if( strcmp(cmd,"rollover")==0 )
	{
		printf( "enabling artificial pts rollover (10s after next tune)\n" );
		tsdemuxer_InduceRollover( true );
	}
	else if (strcmp(cmd, "list") == 0)
	{
		mVirtualChannelMap.showList();
	}
	else if( strcmp(cmd,"autoplay") == 0 )
	{
		mAampcli.mbAutoPlay = !mAampcli.mbAutoPlay;
		printf( "autoplay = %s\n", mAampcli.mbAutoPlay?"true":"false" );
	}
	else if( strcmp(cmd,"new") == 0 )
	{
		mAampcli.newPlayerInstance();
	}
	else if( sscanf(cmd, "sleep %d", &ms ) == 1 )
	{
		if( ms>0 )
		{
			printf( "sleeping for %f seconds\n", ms/1000.0 );
			g_usleep (ms * 1000);
			printf( "sleep complete\n" );
		}
	}
	else if( sscanf(cmd, "select %d", &playerIndex ) == 1 )
	{
		int len = mAampcli.mPlayerInstances.size();
		if( playerIndex>=0 && playerIndex<len )
		{
			playerInstanceAamp = mAampcli.mPlayerInstances.at(playerIndex);
		}
		else
		{
			printf( "invalid index\n" );
			if( len>0 )
			{
				printf( "valid range = 0..%d\n", len-1 );
			}
			else
			{
				printf( "no player instances\n" );
			}
		}
	}
	else if (strcmp(cmd, "select") == 0)
	{ // List available player instances
		printf( "player instances:\n" );
		playerIndex = 0;
		for( auto playerInstance : mAampcli.mPlayerInstances )
		{
			printf( "\t%d", playerIndex++ );
			if( playerInstance == playerInstanceAamp )
			{
				printf( " (selected)");
			}
			printf( "\n ");
		}
	}
	else if( strcmp(cmd,"detach") == 0 )
	{
		playerInstanceAamp->detach();
	}
	else if (isTuneScheme(cmd))
	{
		{
			if (memcmp(cmd, "live", 4) == 0)
			{
				playerInstanceAamp->SeekToLive();
			}
			else
			{
				playerInstanceAamp->Tune(cmd,mAampcli.mbAutoPlay);
			}
		}
	}
	else if( memcmp(cmd, "next", 4) == 0 )
	{
		VirtualChannelInfo *pNextChannel = mVirtualChannelMap.next();
		if (pNextChannel)
		{
			printf("[AAMPCLI] next %d: %s\n", pNextChannel->channelNumber, pNextChannel->name.c_str());
			mVirtualChannelMap.tuneToChannel( *pNextChannel, playerInstanceAamp);
		}
		else
		{
			printf("[AAMPCLI] can not fetch 'next' channel, empty virtual channel map\n");
		}
	}
	else if( memcmp(cmd, "prev", 4) == 0 )
	{
		VirtualChannelInfo *pPrevChannel = mVirtualChannelMap.prev();
		if (pPrevChannel)
		{
			printf("[AAMPCLI] next %d: %s\n", pPrevChannel->channelNumber, pPrevChannel->name.c_str());
			mVirtualChannelMap.tuneToChannel( *pPrevChannel, playerInstanceAamp);
		}
		else
		{
			printf("[AAMPCLI] can not fetch 'prev' channel, empty virtual channel map\n");
		}
	}
	else if (isNumber(cmd))
	{
		int channelNumber = atoi(cmd);  // invalid input results in 0 -- will not be found

		VirtualChannelInfo *pChannelInfo = mVirtualChannelMap.find(channelNumber);
		if (pChannelInfo != NULL)
		{
			printf("[AAMPCLI] channel number: %d\n", channelNumber);
			mVirtualChannelMap.tuneToChannel( *pChannelInfo, playerInstanceAamp);
		}
		else
		{
			printf("[AAMPCLI] channel number: %d was not found\n", channelNumber);
		}
	}
	else if (sscanf(cmd, "seek %lf %d", &seconds, &keepPaused) >= 1)
	{
		bool seekWhilePaused = (keepPaused==1);
		{
			playerInstanceAamp->Seek(seconds, (keepPaused==1) );
		}
		keepPaused = 0;
	}
	else if (strcmp(cmd, "sf") == 0)
	{
		playerInstanceAamp->SetRate((float)0.5);
	}
	else if (sscanf(cmd, "ff%d", &rate) == 1)
	{
		playerInstanceAamp->SetRate((float)rate);
	}
	else if (strcmp(cmd, "play") == 0)
	{
		playerInstanceAamp->SetRate(1);
	}
	else if (strcmp(cmd, "pause") == 0)
	{
		playerInstanceAamp->SetRate(0);
	}
	else if (sscanf(cmd, "rw%d", &rate) == 1)
	{
		playerInstanceAamp->SetRate((float)(-rate));
	}
	else if (sscanf(cmd, "bps %d", &rate) == 1)
	{
		printf("[AAMPCLI] Set video bitrate %d.\n", rate);
		playerInstanceAamp->SetVideoBitrate(rate);
	}
	else if (strcmp(cmd, "flush") == 0)
	{
		playerInstanceAamp->aamp->mStreamSink->Flush();
	}
	else if (strcmp(cmd, "stop") == 0)
	{
		playerInstanceAamp->Stop();
		tsdemuxer_InduceRollover(false);
	}
	else if (strcmp(cmd, "underflow") == 0)
	{
		playerInstanceAamp->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, eMEDIATYPE_VIDEO);
	}
	else if (strcmp(cmd, "retune") == 0)
	{
		playerInstanceAamp->aamp->ScheduleRetune(eDASH_ERROR_STARTTIME_RESET, eMEDIATYPE_VIDEO);
	}
	else if (strcmp(cmd, "status") == 0)
	{
		playerInstanceAamp->aamp->mStreamSink->DumpStatus();
	}
	else if (strcmp(cmd, "live") == 0)
	{
		playerInstanceAamp->SeekToLive();
	}
	else if (strcmp(cmd, "exit") == 0)
	{
		playerInstanceAamp = NULL;
		for( auto playerInstance : mAampcli.mPlayerInstances )
		{
			playerInstance->Stop();
			SAFE_DELETE( playerInstance );
		}
		termPlayerLoop();
		return false;	//to exit
	}
	else if (memcmp(cmd, "rect", 4) == 0)
	{
		int x, y, w, h;
		if (sscanf(cmd, "rect %d %d %d %d", &x, &y, &w, &h) == 4)
		{
			playerInstanceAamp->SetVideoRectangle(x, y, w, h);
		}
	}
	else if (memcmp(cmd, "zoom", 4) == 0)
	{
		int zoom;
		if (sscanf(cmd, "zoom %d", &zoom) == 1)
		{
			if (zoom)
			{
				printf("[AAMPCLI] Set zoom to full\n");
				playerInstanceAamp->SetVideoZoom(VIDEO_ZOOM_FULL);
			}
			else
			{
				printf("[AAMPCLI] Set zoom to none\n");
				playerInstanceAamp->SetVideoZoom(VIDEO_ZOOM_NONE);
			}
		}
	}
	else if( sscanf(cmd, "sap %s",lang ) )
	{
		size_t len = strlen(lang);
		printf("[AAMPCLI] aamp cli sap called for language %s\n",lang);
		if( len>0 )
		{
			playerInstanceAamp->SetLanguage( lang );
		}
		else
		{
			printf("[AAMPCLI] GetCurrentAudioLanguage: '%s'\n", playerInstanceAamp->GetCurrentAudioLanguage() );
		}
	}
	else if( strcmp(cmd,"getplaybackrate") == 0 )
	{
		printf("[AAMPCLI] Playback Rate: %d\n", playerInstanceAamp->GetPlaybackRate());
	}
	else if (memcmp(cmd, "islive", 6) == 0 )
	{
		printf("[AAMPCLI] VIDEO IS %s\n",
				(playerInstanceAamp->IsLive() == true )? "LIVE": "NOT LIVE");
	}
	else if (memcmp(cmd, "customheader", 12) == 0 )
	{
		//Dummy implimenations
		std::vector<std::string> headerValue;
		printf("[AAMPCLI] customheader Command is %s\n" , cmd);
		playerInstanceAamp->AddCustomHTTPHeader("", headerValue, false);
	}
	else if (sscanf(cmd, "unlock %ld", &unlockSeconds) >= 1)
	{
		printf("[AAMPCLI] unlocking for %ld seconds\n" , unlockSeconds);
		if(-1 == unlockSeconds)
			grace = -1;
		else
			time = unlockSeconds;
		playerInstanceAamp->DisableContentRestrictions(grace, time, eventChange);
	}
	else if (memcmp(cmd, "unlock", 6) == 0 )
	{
		printf("[AAMPCLI] unlocking till next program change\n");
		eventChange = true;
		playerInstanceAamp->DisableContentRestrictions(grace, time, eventChange);
	}
	else if (memcmp(cmd, "lock", 4) == 0 )
	{
		playerInstanceAamp->EnableContentRestrictions();
	}
	else if (strcmp(cmd, "progress") == 0)
	{
		mAampcli.mEnableProgressLog = mAampcli.mEnableProgressLog ? false : true;
	}
	else if(strcmp(cmd, "stats") == 0)
	{
		printf("[AAMPCLI] Playback stats: %s", playerInstanceAamp->GetPlaybackStats().c_str());
	}

	return true;
}

/**
 * @brief check if the char array is having numbers only
 * @param s
 * @retval true or false
 */
bool PlaybackCommand::isNumber(const char *s)
{
	if (*s)
	{
		if (*s == '-')
		{ // skip leading minus
			s++;
		}
		for (;;)
		{
			if (*s >= '0' && *s <= '9')
			{
				s++;
				continue;
			}
			if (*s == 0x00)
			{
				return true;
			}
			break;
		}
	}
	return false;
}

/**
 * @brief Show help menu with aamp command line interface
 */
void PlaybackCommand::showHelp(void)
{
	printf("******************************************************************************************\n");
	printf("*   <command> [<arguments>]\n");
	printf("*   Usage of Commands, and arguments expected\n");
	printf("******************************************************************************************\n");
	printf("list                  // Type list to view virtual channel map\n");
	printf("get help              // Show help of get command\n");
	printf("set help              // Show help of set command\n");
	printf("<channelNumber>       // Play selected channel from guide\n");
	printf("<url>                 // Play arbitrary stream\n");
	printf("sleep <ms>            // Sleep <ms> milliseconds\n" );
	printf("autoplay              // Toggle tune autoplay (default = true)\n" );
	printf("new                   // Create new player instance\n" );
	printf("select                // Enumerate available player instances\n" );
	printf("select <index>        // Select player instance for use\n" );
	printf("detach                // Detach (lightweight stop) selected player instance\n" );
	printf("pause                 // Pause the existing playback\n");
	printf("play                  // Continue existing playback\n");
	printf("stop                  // Stop the existing playback\n");
	printf("status                // get the status of existing playback\n");
	printf("flush                 // Flush the existing playback\n");
	printf("sf                    // slow\n");
	printf("ff<x>                 // Trickmodes (x <= 128. y <= 128)\n");
	printf("rw<y>                 // Trickmodes (x <= 128. y <= 128)\n");
	printf("bps <x>               // set bitrate (x= bitrate)\n");
	printf("sap                   // Use SAP track (if avail)\n");
	printf("seek <seconds>        // Specify start time within manifest\n");
	printf("live                  // Seek to live point\n");
	printf("underflow             // Simulate underflow\n");
	printf("retune                // schedule retune\n");
	printf("unlock <cond>         // unlock a channel <long int - time in seconds>\n");
	printf("lock                  // lock the current channel\n");
	printf("next                  // Play next virtual channel\n");
	printf("rollover              // Schedule artificial pts rollover 10s after next tune\n" );
	printf("prev                  // Play previous virtual channel\n");
	printf("exit                  // Exit from application\n");
	printf("help                  // Show this list again\n");
	printf("progress              // Enable/disable Progress event logging\n");
	printf("harvest <Configs>     // To harvest assets\n");
	printf("******************************************************************************************\n");
}

/**
 * @brief Decide if input command consists of supported URI scheme to be tuned.
 * @param cmd cmd to parse
 */
bool PlaybackCommand::isTuneScheme(const char *cmd)
{
	bool isTuneScheme = false;
	const char *protocol[5] = { "http:","https:","live:","hdmiin:","file:" };
	for( int i=0; i<5; i++ )
	{
		size_t len = strlen(protocol[i]);
		if( memcmp( cmd, protocol[i],len )==0 )
		{
			isTuneScheme=true;
			break;
		}
	}
	return isTuneScheme;
}

/**
 * @brief trim a string
 * @param[in][out] cmd Buffer containing string
 */
void PlaybackCommand::trim(char **cmd)
{
	std::string src = *cmd;
	size_t first = src.find_first_not_of(' ');
	if (first != std::string::npos)
	{
		size_t last = src.find_last_not_of(" \r\n");
		std::string dst = src.substr(first, (last - first + 1));
		strncpy(*cmd, (char*)dst.c_str(), dst.size());
		(*cmd)[dst.size()] = '\0';
	}
}

/**
 * @brief Stop mainloop execution (for standalone mode)
 */
void PlaybackCommand::termPlayerLoop()
{
	if(mAampcli.mAampGstPlayerMainLoop)
	{
		g_main_loop_quit(mAampcli.mAampGstPlayerMainLoop);
		g_thread_join(mAampcli.mAampMainLoopThread);
		gst_deinit ();
		printf("[AAMPCLI] %s(): Exit\n", __FUNCTION__);
	}
}

