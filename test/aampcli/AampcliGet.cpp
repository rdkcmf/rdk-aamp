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
 * @file AampcliGet.cpp
 * @brief Aampcli Get command handler
 */

#include"AampcliGet.h"

const char *mGetHelpText[eAAMP_GET_TYPE_COUNT];

/**
 * @brief Show help menu with aamp command line interface
 */
void Get::initGetHelpText()
{
	mGetHelpText[eAAMP_GET_CurrentState] = "Get current player state";
	mGetHelpText[eAAMP_GET_CurrentAudioLan] = "Get Current audio language";
	mGetHelpText[eAAMP_GET_CurrentDrm] = "Get Current DRM";
	mGetHelpText[eAAMP_GET_PlaybackPosition] = "Get Current Playback position";
	mGetHelpText[eAAMP_GET_PlaybackDuration] = "Get Playback Duration";
	mGetHelpText[eAAMP_GET_VideoBitrate] = "Get current video bitrate";
	mGetHelpText[eAAMP_GET_InitialBitrate] ="Get Initial Bitrate";
	mGetHelpText[eAAMP_GET_InitialBitrate4k] ="Get Initial Bitrate 4K";
	mGetHelpText[eAAMP_GET_MinimumBitrate]="Get Minimum Bitrate";
	mGetHelpText[eAAMP_GET_MaximumBitrate]="Get Maximum Bitrate";
	mGetHelpText[eAAMP_GET_AudioBitrate] = "Get current Audio bitrate";
	mGetHelpText[eAAMP_GET_VideoZoom] = "Get Video Zoom mode";
	mGetHelpText[eAAMP_GET_VideoMute] = "Get Video Mute status";
	mGetHelpText[eAAMP_GET_AudioVolume] = "Get current Audio volume";
	mGetHelpText[eAAMP_GET_PlaybackRate] = "Get Current Playback rate";
	mGetHelpText[eAAMP_GET_VideoBitrates] = "Get Video bitrates supported";
	mGetHelpText[eAAMP_GET_AudioBitrates] = "Get Audio bitrates supported";
	mGetHelpText[eAAMP_GET_CurrentPreferredLanguages] = "Get Current preferred languages";
	mGetHelpText[eAAMP_GET_RampDownLimit] ="Get number of  Ramp down limit during playback";
	mGetHelpText[eAAMP_GET_AvailableAudioTracks] = "Get Available Audio Tracks";
	mGetHelpText[eAAMP_GET_AvailableVideoTracks] = "Get All Available Video Tracks information from manifest";
	mGetHelpText[eAAMP_GET_AllAvailableAudioTracks] = "Get All Available Audio Tracks information from manifest";
	mGetHelpText[eAAMP_GET_AllAvailableTextTracks] = "Get All Available Text Tracks information from manifest";
	mGetHelpText[eAAMP_GET_AvailableTextTracks] = "Get Available Text Tracks";
	mGetHelpText[eAAMP_GET_AudioTrack] = "Get Audio Track";
	mGetHelpText[eAAMP_GET_InitialBufferDuration] ="Get Initial Buffer Duration( in sec)";
	mGetHelpText[eAAMP_GET_AudioTrackInfo] = "Get current Audio Track information in json format";
	mGetHelpText[eAAMP_GET_TextTrackInfo] = "Get current Text Track information in json format";
	mGetHelpText[eAAMP_GET_PreferredAudioProperties] = "Get current Preferred Audio properties in json format";
	mGetHelpText[eAAMP_GET_PreferredTextProperties] = "Get current Preferred Text properties in json format";
	mGetHelpText[eAAMP_GET_CCStatus]="Get CC Status";
	mGetHelpText[eAAMP_GET_TextTrack] = "Get Text Track";
	mGetHelpText[eAAMP_GET_ThumbnailConfig] = "Get Available ThumbnailTracks";
	mGetHelpText[eAAMP_GET_ThumbnailData] = "Get Thumbnail timerange data(int startpos, int endpos)";

}

/**
 * @brief Display Help menu for set
 * @param none
 */
void Get::showHelpGet()
{
	printf("******************************************************************************************\n");
	printf("*   get <command> [<arguments>]\n");
	printf("*   Usage of Commands, and arguments expected\n");
	printf("******************************************************************************************\n");

	for (int iLoop = 0; iLoop < eAAMP_GET_TYPE_COUNT; iLoop++)
	{
		printf("* get %2d // %s \n", iLoop+1, mGetHelpText[iLoop]);
	}

	printf("****************************************************************************\n");
}

bool Get::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char help[8];
	int opt, value1, value2;
	if (sscanf(cmd, "get %d", &opt) == 1){
		switch(opt-1){ // 1 based to zero based
			case eAAMP_GET_ThumbnailConfig:
				printf("[AAMPCLI] GETTING AVAILABLE THUMBNAIL TRACKS: %s\n", playerInstanceAamp->GetAvailableThumbnailTracks().c_str() );
				break;

			case eAAMP_GET_CurrentState:
				printf("[AAMPCLI] GETTING CURRENT STATE: %d\n", (int) playerInstanceAamp->GetState());
				break;

			case eAAMP_GET_ThumbnailData:
				sscanf(cmd, "get %d %d %d",&opt, &value1, &value2);
				printf("[AAMPCLI] GETTING THUMBNAIL TIME RANGE DATA for duration(%d,%d): %s\n",value1,value2,playerInstanceAamp->GetThumbnails(value1, value2).c_str());
				break;

			case eAAMP_GET_AudioTrack:
				printf("[AAMPCLI] CURRENT AUDIO TRACK NUMBER: %d\n", playerInstanceAamp->GetAudioTrack() );
				break;

			case eAAMP_GET_InitialBufferDuration:
				printf("[AAMPCLI] INITIAL BUFFER DURATION: %d\n", playerInstanceAamp->GetInitialBufferDuration() );
				break;

			case eAAMP_GET_AudioTrackInfo:
				printf("[AAMPCLI] CURRENT AUDIO TRACK INFO: %s\n", playerInstanceAamp->GetAudioTrackInfo().c_str() );
				break;

			case eAAMP_GET_TextTrackInfo:
				printf("[AAMPCLI] CURRENT TEXT TRACK INFO: %s\n", playerInstanceAamp->GetTextTrackInfo().c_str() );
				break;

			case eAAMP_GET_PreferredAudioProperties:
				printf("[AAMPCLI] CURRENT PREPRRED AUDIO PROPERTIES: %s\n", playerInstanceAamp->GetPreferredAudioProperties().c_str() );
				break;

			case eAAMP_GET_PreferredTextProperties:
				printf("[AAMPCLI] CURRENT PREPRRED TEXT PROPERTIES: %s\n", playerInstanceAamp->GetPreferredTextProperties().c_str() );
				break;

			case eAAMP_GET_CCStatus:
				printf("[AAMPCLI] CC VISIBILITY STATUS: %s\n",playerInstanceAamp->GetCCStatus()?"ENABLED":"DISABLED");
				break;

			case eAAMP_GET_TextTrack:
				printf("[AAMPCLI] CURRENT TEXT TRACK: %d\n", playerInstanceAamp->GetTextTrack() );
				break;

			case eAAMP_GET_AvailableAudioTracks:
				printf("[AAMPCLI] AVAILABLE AUDIO TRACKS: %s\n", playerInstanceAamp->GetAvailableAudioTracks(false).c_str() );
				break;
			case eAAMP_GET_AvailableVideoTracks:
				printf("[AAMPCLI] AVAILABLE VIDEO TRACKS: %s\n", playerInstanceAamp->GetAvailableVideoTracks().c_str() );
				break;

			case eAAMP_GET_AllAvailableAudioTracks:
				printf("[AAMPCLI] ALL AUDIO TRACKS: %s\n", playerInstanceAamp->GetAvailableAudioTracks(true).c_str() );
				break;

			case eAAMP_GET_AllAvailableTextTracks:
				printf("[AAMPCLI] ALL TEXT TRACKS: %s\n", playerInstanceAamp->GetAvailableTextTracks(true).c_str() );
				break;

			case eAAMP_GET_AvailableTextTracks:
				printf("[AAMPCLI] AVAILABLE TEXT TRACKS: %s\n", playerInstanceAamp->GetAvailableTextTracks(false).c_str() );
				break;

			case eAAMP_GET_CurrentAudioLan:
				printf("[AAMPCLI] CURRRENT AUDIO LANGUAGE = %s\n",
						playerInstanceAamp->GetCurrentAudioLanguage());
				break;

			case eAAMP_GET_CurrentDrm:
				printf("[AAMPCLI] CURRRENT DRM  = %s\n",
						playerInstanceAamp->GetCurrentDRM());
				break;

			case eAAMP_GET_PlaybackPosition:
				printf("[AAMPCLI] PLAYBACK POSITION = %lf\n",
						playerInstanceAamp->GetPlaybackPosition());
				break;

			case eAAMP_GET_PlaybackDuration:
				printf("[AAMPCLI] PLAYBACK DURATION = %lf\n",
						playerInstanceAamp->GetPlaybackDuration());
				break;

			case eAAMP_GET_VideoBitrate:
				printf("[AAMPCLI] CURRENT VIDEO PROFILE BITRATE = %ld\n",
						playerInstanceAamp->GetVideoBitrate());
				break;

			case eAAMP_GET_InitialBitrate:
				printf("[AAMPCLI] INITIAL BITRATE = %ld \n",
						playerInstanceAamp->GetInitialBitrate());
				break;

			case eAAMP_GET_InitialBitrate4k:
				printf("[AAMPCLI] INITIAL BITRATE 4K = %ld \n",
						playerInstanceAamp->GetInitialBitrate4k());
				break;

			case eAAMP_GET_MinimumBitrate:
				printf("[AAMPCLI] MINIMUM BITRATE = %ld \n",
						playerInstanceAamp->GetMinimumBitrate());
				break;

			case eAAMP_GET_MaximumBitrate:
				printf("[AAMPCLI] MAXIMUM BITRATE = %ld \n",
						playerInstanceAamp->GetMaximumBitrate());
				break;

			case eAAMP_GET_AudioBitrate:
				printf("[AAMPCLI] AUDIO BITRATE = %ld\n",
						playerInstanceAamp->GetAudioBitrate());
				break;

			case eAAMP_GET_VideoZoom:
				printf("[AAMPCLI] Video Zoom mode: %s\n",
						(playerInstanceAamp->GetVideoZoom())?"None(Normal)":"Full(Enabled)");
				break;

			case eAAMP_GET_VideoMute:
				printf("[AAMPCLI] Video Mute status:%s\n",
						(playerInstanceAamp->GetVideoMute())?"ON":"OFF");
				break;

			case eAAMP_GET_AudioVolume:
				printf("[AAMPCLI] AUDIO VOLUME = %d\n",
						playerInstanceAamp->GetAudioVolume());
				break;

			case eAAMP_GET_PlaybackRate:
				printf("[AAMPCLI] PLAYBACK RATE = %d\n",
						playerInstanceAamp->GetPlaybackRate());
				break;

			case eAAMP_GET_VideoBitrates:
				{
					std::vector<long int> videoBitrates;
					printf("[AAMPCLI] VIDEO BITRATES = [ ");
					videoBitrates = playerInstanceAamp->GetVideoBitrates();
					for(int i=0; i < videoBitrates.size(); i++){
						printf("%ld, ", videoBitrates[i]);
					}
					printf(" ]\n");
					break;
				}

			case eAAMP_GET_AudioBitrates:
				{
					std::vector<long int> audioBitrates;
					printf("[AAMPCLI] AUDIO BITRATES = [ ");
					audioBitrates = playerInstanceAamp->GetAudioBitrates();
					for(int i=0; i < audioBitrates.size(); i++){
						printf("%ld, ", audioBitrates[i]);
					}
					printf(" ]\n");
					break;
				}
			case eAAMP_GET_CurrentPreferredLanguages:
				{
					const char *prefferedLanguages = playerInstanceAamp->GetPreferredLanguages();
					printf("[AAMPCLI] PREFERRED LANGUAGES = \"%s\"\n", prefferedLanguages? prefferedLanguages : "<NULL>");
					break;
				}

			case eAAMP_GET_RampDownLimit:
				{
					printf("[AAMPCLI] RAMP DOWN LIMIT= %d\n", playerInstanceAamp->GetRampDownLimit());
					break;
				}

			default:
				printf("[AAMPCLI] Invalid get command %d\n", opt );
				break;
		}

	}
	else if (sscanf(cmd, "get %s", help) == 1)
	{
		if(0 == strncmp("help", help, 4)){
			showHelpGet();
		}else
		{
			printf("[AAMPCLI] Invalid usage of get operations %s\n", help);
		}
	}
	else
	{
		printf("[AAMPCLI] Invalid get command = %s\n", cmd);
	}

	return true;
}

