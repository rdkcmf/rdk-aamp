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

#include <iomanip>
#include"AampcliGet.h"

std::map<string,string> Get::getCommands = std::map<string,string>();
std::vector<std::string> Get::commands(0);

uint64_t constexpr mix(char m, uint64_t s)
{ 
	return ((s<<7) + ~(s>>3)) + ~m; 
}

uint64_t constexpr getHash(const char * m)
{
	return (*m) ? mix(*m,getHash(m+1)) : 0;
}

bool Get::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char help[8];
	int opt, value1, value2;
	char command[100];


	if (sscanf(cmd, "get %s", command) == 1)
	{

		if(0 == strncmp("help", command, 4))
		{
			ShowHelpGet();
		}
		else
		{
			switch(getHash(command)){ // 1 based to zero based
				case getHash("thumbnailConfig"):
					printf("[AAMPCLI] GETTING AVAILABLE THUMBNAIL TRACKS: %s\n", playerInstanceAamp->GetAvailableThumbnailTracks().c_str() );
					break;

				case getHash("currentState"):
					printf("[AAMPCLI] GETTING CURRENT STATE: %d\n", (int) playerInstanceAamp->GetState());
					break;

				case getHash("thumbnailData"):
					sscanf(cmd, "get %d %d %d",&opt, &value1, &value2);
					printf("[AAMPCLI] GETTING THUMBNAIL TIME RANGE DATA for duration(%d,%d): %s\n",value1,value2,playerInstanceAamp->GetThumbnails(value1, value2).c_str());
					break;

				case getHash("audioTrack"):
					printf("[AAMPCLI] CURRENT AUDIO TRACK NUMBER: %d\n", playerInstanceAamp->GetAudioTrack() );
					break;

				case getHash("initialBufferDuration"):
					printf("[AAMPCLI] INITIAL BUFFER DURATION: %d\n", playerInstanceAamp->GetInitialBufferDuration() );
					break;

				case getHash("audioTrackInfo"):
					printf("[AAMPCLI] CURRENT AUDIO TRACK INFO: %s\n", playerInstanceAamp->GetAudioTrackInfo().c_str() );
					break;

				case getHash("textTrackInfo"):
					printf("[AAMPCLI] CURRENT TEXT TRACK INFO: %s\n", playerInstanceAamp->GetTextTrackInfo().c_str() );
					break;

				case getHash("preferredAudioProperties"):
					printf("[AAMPCLI] CURRENT PREPRRED AUDIO PROPERTIES: %s\n", playerInstanceAamp->GetPreferredAudioProperties().c_str() );
					break;

				case getHash("preferredTextProperties"):
					printf("[AAMPCLI] CURRENT PREPRRED TEXT PROPERTIES: %s\n", playerInstanceAamp->GetPreferredTextProperties().c_str() );
					break;

				case getHash("ccStatus"):
					printf("[AAMPCLI] CC VISIBILITY STATUS: %s\n",playerInstanceAamp->GetCCStatus()?"ENABLED":"DISABLED");
					break;

				case getHash("textTrack"):
					printf("[AAMPCLI] CURRENT TEXT TRACK: %d\n", playerInstanceAamp->GetTextTrack() );
					break;

				case getHash("availableAudioTracks"):
					printf("[AAMPCLI] AVAILABLE AUDIO TRACKS: %s\n", playerInstanceAamp->GetAvailableAudioTracks(false).c_str() );
					break;
				case getHash("availableVideoTracks"):
					printf("[AAMPCLI] AVAILABLE VIDEO TRACKS: %s\n", playerInstanceAamp->GetAvailableVideoTracks().c_str() );
					break;

				case getHash("allAvailableAudioTracks"):
					printf("[AAMPCLI] ALL AUDIO TRACKS: %s\n", playerInstanceAamp->GetAvailableAudioTracks(true).c_str() );
					break;

				case getHash("allAvailableTextTracks"):
					printf("[AAMPCLI] ALL TEXT TRACKS: %s\n", playerInstanceAamp->GetAvailableTextTracks(true).c_str() );
					break;

				case getHash("availableTextTracks"):
					printf("[AAMPCLI] AVAILABLE TEXT TRACKS: %s\n", playerInstanceAamp->GetAvailableTextTracks(false).c_str() );
					break;

				case getHash("currentAudioLan"):
					printf("[AAMPCLI] CURRRENT AUDIO LANGUAGE = %s\n",
							playerInstanceAamp->GetCurrentAudioLanguage());
					break;

				case getHash("currentDrm"):
					printf("[AAMPCLI] CURRRENT DRM  = %s\n",
							playerInstanceAamp->GetCurrentDRM());
					break;

				case getHash("playbackPosition"):
					printf("[AAMPCLI] PLAYBACK POSITION = %lf\n",
							playerInstanceAamp->GetPlaybackPosition());
					break;

				case getHash("playbackDuration"):
					printf("[AAMPCLI] PLAYBACK DURATION = %lf\n",
							playerInstanceAamp->GetPlaybackDuration());
					break;

				case getHash("videoBitrate"):
					printf("[AAMPCLI] CURRENT VIDEO PROFILE BITRATE = %ld\n",
							playerInstanceAamp->GetVideoBitrate());
					break;

				case getHash("initialBitrate"):
					printf("[AAMPCLI] INITIAL BITRATE = %ld \n",
							playerInstanceAamp->GetInitialBitrate());
					break;

				case getHash("initialBitrate4k"):
					printf("[AAMPCLI] INITIAL BITRATE 4K = %ld \n",
							playerInstanceAamp->GetInitialBitrate4k());
					break;

				case getHash("minimumBitrate"):
					printf("[AAMPCLI] MINIMUM BITRATE = %ld \n",
							playerInstanceAamp->GetMinimumBitrate());
					break;

				case getHash("maximumBitrate"):
					printf("[AAMPCLI] MAXIMUM BITRATE = %ld \n",
							playerInstanceAamp->GetMaximumBitrate());
					break;

				case getHash("audioBitrate"):
					printf("[AAMPCLI] AUDIO BITRATE = %ld\n",
							playerInstanceAamp->GetAudioBitrate());
					break;

				case getHash("videoZoom"):
					printf("[AAMPCLI] Video Zoom mode: %s\n",
							(playerInstanceAamp->GetVideoZoom())?"None(Normal)":"Full(Enabled)");
					break;

				case getHash("videoMute"):
					printf("[AAMPCLI] Video Mute status:%s\n",
							(playerInstanceAamp->GetVideoMute())?"ON":"OFF");
					break;

				case getHash("audioVolume"):
					printf("[AAMPCLI] AUDIO VOLUME = %d\n",
							playerInstanceAamp->GetAudioVolume());
					break;

				case getHash("playbackRate"):
					printf("[AAMPCLI] PLAYBACK RATE = %d\n",
							playerInstanceAamp->GetPlaybackRate());
					break;

				case getHash("videoBitrates"):
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

				case getHash("audioBitrates"):
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
				case getHash("currentPreferredLanguages"):
					{
						const char *prefferedLanguages = playerInstanceAamp->GetPreferredLanguages();
						printf("[AAMPCLI] PREFERRED LANGUAGES = \"%s\"\n", prefferedLanguages? prefferedLanguages : "<NULL>");
						break;
					}

				case getHash("rampDownLimit"):
					{
						printf("[AAMPCLI] RAMP DOWN LIMIT= %d\n", playerInstanceAamp->GetRampDownLimit());
						break;
					}

				default:
					printf("[AAMPCLI] Invalid get command %s\n", cmd);
					break;
			}

		}
	}
	else
	{
		printf("[AAMPCLI] Invalid get command = %s\n", cmd);
	}

	return true;
}

/**
 * @brief Show help menu with aamp command line interface
 */
void Get::registerGetCommands()
{
	addCommand("currentState","Get current player state");
	addCommand("currentAudioLan","Get Current audio language");
	addCommand("currentDrm","Get Current DRM");
	addCommand("playbackPosition","Get Current Playback position");
	addCommand("playbackDuration","Get Playback Duration");
	addCommand("videoBitrate","Get current video bitrate");
	addCommand("initialBitrate","Get Initial Bitrate");
	addCommand("initialBitrate4k","Get Initial Bitrate 4K");
	addCommand("minimumBitrate","Get Minimum Bitrate");
	addCommand("maximumBitrate","Get Maximum Bitrate");
	addCommand("audioBitrate","Get current Audio bitrate");
	addCommand("videoZoom","Get Video Zoom mode");
	addCommand("videoMute","Get Video Mute status");
	addCommand("audioVolume","Get current Audio volume");
	addCommand("playbackRate","Get Current Playback rate");
	addCommand("videoBitrates","Get Video bitrates supported");
	addCommand("audioBitrates","Get Audio bitrates supported");
	addCommand("currentPreferredLanguages","Get Current preferred languages");
	addCommand("rampDownLimit","Get number of  Ramp down limit during playback");
	addCommand("availableAudioTracks","Get Available Audio Tracks");
	addCommand("availableVideoTracks","Get All Available Video Tracks information from manifest");
	addCommand("allAvailableAudioTracks","Get All Available Audio Tracks information from manifest");
	addCommand("allAvailableTextTracks","Get All Available Text Tracks information from manifest");
	addCommand("availableTextTracks","Get Available Text Tracks");
	addCommand("audioTrack","Get Audio Track");
	addCommand("initialBufferDuration","Get Initial Buffer Duration( in sec)");
	addCommand("audioTrackInfo","Get current Audio Track information in json format");
	addCommand("textTrackInfo","Get current Text Track information in json format");
	addCommand("preferredAudioProperties","Get current Preferred Audio properties in json format");
	addCommand("preferredTextProperties","Get current Preferred Text properties in json format");
	addCommand("ccStatus","Get CC Status");
	addCommand("textTrack","Get Text Track");
	addCommand("thumbnailConfig","Get Available ThumbnailTracks");
	addCommand("thumbnailData","Get Thumbnail timerange data(int startpos, int endpos)");
	commands.push_back("help");
}

void Get::addCommand(string command,string description)
{
	getCommands.insert(make_pair(command,description));
	commands.push_back(command);
}

/**
 * @brief Display Help menu for set
 * @param none
 */
void Get::ShowHelpGet(){

	std::map<string,string>::iterator getCmdItr;

	printf("******************************************************************************************\n");
	printf("*   get <command> [<arguments>]\n");
	printf("*   Usage of Commands, and arguments expected\n");
	printf("******************************************************************************************\n");

	if(!commands.empty())
	{
		for(auto itr:commands)
		{
			getCmdItr = getCommands.find(itr);

			if(getCmdItr != getCommands.end())
			{
				std::cout << "get " << std::setw(35) << std::left << (getCmdItr->first).c_str() << "// "<< (getCmdItr->second).c_str() << "\n";
			}
		}
	}

	printf("****************************************************************************\n");
}

char * Get::getCommandRecommender(const char *text, int state)
{
    char *name;
    static int len;
    static std::vector<std::string>::iterator itr;

    if (!state) 
    {
	itr = commands.begin();
        len = strlen(text);
    }

    while (itr != commands.end())
    {
	    name = (char *) itr->c_str();
	    itr++;
	    if (strncmp(name, text, len) == 0) 
	    {
		    return strdup(name);
	    }	
    }

    return NULL;
}
