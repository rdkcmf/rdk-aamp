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
 * @file AampcliSet.cpp
 * @brief Aampcli Set command handler
 */

#include <iomanip>
#include"Aampcli.h"
#include"AampcliSet.h"

std::map<string,setCommandInfo> Set::setCommands = std::map<string,setCommandInfo>();
std::vector<std::string> Set::commands(0);

bool Set::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	printf("%s:%d:cmd %s\n", __FUNCTION__, __LINE__,cmd);
	char command[100];
	int setCmd = 0;
	int rate = 0;
	unsigned int DownloadDelayInMs = 0;
	Aampcli lAampcli;

	if (sscanf(cmd, "set %s", command) == 1)
	{
		if(isdigit(command[0]))
		{
			setCmd = atoi(command);
		}
		else
		{
			std::map<string,setCommandInfo>::iterator setCmdItr;
			setCmdItr = setCommands.find(command);

			if(setCmdItr != setCommands.end())
			{
				setCmd = setCmdItr->second.value;
			}
		}

		if(0 == strncmp("help", command, 4))
		{
			printf("%s:%d:in help %s\n", __FUNCTION__, __LINE__,cmd);
			ShowHelpSet();
		}
		else
		{
			switch(setCmd)
			{
				case 1:
					{
						int rate;
						double relativeTuneTime;
						printf("[AAMPCLI] Matched Command RateAndSeek - %s\n", cmd);
						if (sscanf(cmd, "set %s %d %lf", command, &rate, &relativeTuneTime ) == 3){
							playerInstanceAamp->SetRateAndSeek(rate, relativeTuneTime);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <rate=0,1 etc> <seek time in sec>\n", command);
						}
						break;
					}

				case 2:
					{
						int x,y,w,h;
						printf("[AAMPCLI] Matched Command VideoRectangle - %s\n", cmd);
						if (sscanf(cmd, "set %s %d %d %d %d", command, &x, &y, &w, &h) == 5){
							playerInstanceAamp->SetVideoRectangle(x,y,w,h);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <x> <y> <w> <h>\n", command);
						}
						break;
					}

				case 3:
					{
						int videoZoom;
						printf("[AAMPCLI] Matched Command VideoZoom - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &videoZoom) == 2){
							playerInstanceAamp->SetVideoZoom((videoZoom > 0 )? VIDEO_ZOOM_FULL : VIDEO_ZOOM_NONE );
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value value>0? ZOOM_FULL : ZOOM_NONE>\n", command);
						}
						break;
					}

				case 4:
					{
						int videoMute;
						printf("[AAMPCLI] Matched Command VideoMute - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &videoMute) == 2){
							playerInstanceAamp->SetVideoMute((videoMute == 1 )? true : false );
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 5:
					{
						int vol;
						printf("[AAMPCLI] Matched Command AudioVolume - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &vol) == 2){
							playerInstanceAamp->SetAudioVolume(vol);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value to set audio volume>\n", command);
						}
						break;
					}

				case 6:
					{
						char lang[12];
						printf("[AAMPCLI] Matched Command Language - %s\n", cmd);
						if (sscanf(cmd, "set %s %s", command, lang) == 2){
							playerInstanceAamp->SetLanguage(lang);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <lang in string>\n", command);
						}
						break;
					}

				case 7:
					{
						//Dummy implimentation
						std::vector<std::string> subscribedTags;
						printf("[AAMPCLI] Matched Command SubscribedTags - %s\n", cmd);
						playerInstanceAamp->SetSubscribedTags(subscribedTags);
						break;
					}

				case 8:
					{
						char lisenceUrl[1024];
						int drmType;
						printf("[AAMPCLI] Matched Command LicenseServerUrl - %s\n", cmd);
						if (sscanf(cmd, "set %s %s %d", command, lisenceUrl, &drmType) == 3){
							playerInstanceAamp->SetLicenseServerURL(lisenceUrl,
									(drmType == eDRM_PlayReady)?eDRM_PlayReady:eDRM_WideVine);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <license url in string> <value value==2? ?eDRM_PlayReady : eDRM_WideVine>\n", command);
						}
						break;
					}

				case 9:
					{
						int isAnonym;
						printf("[AAMPCLI] Matched Command AnonymousRequest - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &isAnonym) == 2){
							playerInstanceAamp->SetAnonymousRequest((isAnonym == 1)?true:false);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 10:
					{
						int vodTFps;
						printf("[AAMPCLI] Matched Command VodTrickplayFps - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &vodTFps) == 2){
							playerInstanceAamp->SetVODTrickplayFPS(vodTFps);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 11:
					{
						int linearTFps;
						printf("[AAMPCLI] Matched Command LinearTrickplayFps - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &linearTFps) == 2){
							playerInstanceAamp->SetLinearTrickplayFPS(linearTFps);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 12:
					{
						double liveOffset;
						printf("[AAMPCLI] Matched Command LiveOffset - %s\n", cmd);
						if (sscanf(cmd, "set %s %lf", command, &liveOffset) == 2){
							playerInstanceAamp->SetLiveOffset(liveOffset);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}
				case 56:
					{
						double liveOffset;
						printf("[AAMPCLI] Matched Command LiveOffset4K - %s\n", cmd);
						if (sscanf(cmd, "set %s %lf", command, &liveOffset) == 2){
							playerInstanceAamp->SetLiveOffset4K(liveOffset);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 13:
					{
						int stallErrorCode;
						printf("[AAMPCLI] Matched Command StallErrorCode - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &stallErrorCode) == 2){
							playerInstanceAamp->SetStallErrorCode(stallErrorCode);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 14:
					{
						int stallTimeout;
						printf("[AAMPCLI] Matched Command StallTimeout - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &stallTimeout) == 2){
							playerInstanceAamp->SetStallTimeout(stallTimeout);
						}
						break;
					}

				case 15:
					{
						int reportInterval;
						printf("[AAMPCLI] Matched Command ReportInterval - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &reportInterval) == 2){
							playerInstanceAamp->SetReportInterval(reportInterval);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 16:
					{
						long videoBitrate;
						printf("[AAMPCLI] Matched Command VideoBitarate - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &videoBitrate) == 2){
							playerInstanceAamp->SetVideoBitrate(videoBitrate);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 17:
					{
						long initialBitrate;
						printf("[AAMPCLI] Matched Command InitialBitrate - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &initialBitrate) == 2){
							playerInstanceAamp->SetInitialBitrate(initialBitrate);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 18:
					{
						long initialBitrate4k;
						printf("[AAMPCLI] Matched Command InitialBitrate4k - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &initialBitrate4k) == 2){
							playerInstanceAamp->SetInitialBitrate4K(initialBitrate4k);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 19:
					{
						double networkTimeout;
						printf("[AAMPCLI] Matched Command NetworkTimeout - %s\n", cmd);
						if (sscanf(cmd, "set %s %lf", command, &networkTimeout) == 2){
							playerInstanceAamp->SetNetworkTimeout(networkTimeout);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 20:
					{
						double manifestTimeout;
						printf("[AAMPCLI] Matched Command ManifestTimeout - %s\n", cmd);
						if (sscanf(cmd, "set %s %lf", command, &manifestTimeout) == 2){
							playerInstanceAamp->SetManifestTimeout(manifestTimeout);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 21:
					{
						int downloadBufferSize;
						printf("[AAMPCLI] Matched Command DownloadBufferSize - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &downloadBufferSize) == 2){
							playerInstanceAamp->SetDownloadBufferSize(downloadBufferSize);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 22:
					{
						int preferredDrm;
						printf("[AAMPCLI] Matched Command PreferredDrm - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &preferredDrm) == 2){
							playerInstanceAamp->SetPreferredDRM((DRMSystems)preferredDrm);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 23:
					{
						int stereoOnlyPlayback;
						printf("[AAMPCLI] Matched Command StereoOnlyPlayback - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &stereoOnlyPlayback) == 2){
							playerInstanceAamp->SetStereoOnlyPlayback(
									(stereoOnlyPlayback == 1 )? true:false);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 24:
					{
						//Dummy implementation
						std::string adBrkId = "";
						std::string adId = "";
						std::string url = "";
						printf("[AAMPCLI] Matched Command AlternateContent - %s\n", cmd);
						playerInstanceAamp->SetAlternateContents(adBrkId, adId, url);
						break;
					}

				case 25:
					{
						char networkProxy[128];
						printf("[AAMPCLI] Matched Command NetworkProxy - %s\n", cmd);
						if (sscanf(cmd, "set %s %s", command, networkProxy) == 2){
							playerInstanceAamp->SetNetworkProxy(networkProxy);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value in string>\n", command);
						}
						break;
					}

				case 26:
					{
						char licenseReqProxy[128];
						printf("[AAMPCLI] Matched Command LicenseReqProxy - %s\n", cmd);
						if (sscanf(cmd, "set %s %s", command, licenseReqProxy) == 2){
							playerInstanceAamp->SetLicenseReqProxy(licenseReqProxy);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value in string>\n", command);
						}
						break;
					}

				case 27:
					{
						long downloadStallTimeout;
						printf("[AAMPCLI] Matched Command DownloadStallTimeout - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &downloadStallTimeout) == 2){
							playerInstanceAamp->SetDownloadStallTimeout(downloadStallTimeout);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 28:
					{
						long downloadStartTimeout;
						printf("[AAMPCLI] Matched Command DownloadStartTimeout - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &downloadStartTimeout) == 2){
							playerInstanceAamp->SetDownloadStartTimeout(downloadStartTimeout);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 29:
					{
						long downloadLowBWTimeout;
						printf("[AAMPCLI] Matched Command DownloadLowBWTimeout - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &downloadLowBWTimeout) == 2){
							playerInstanceAamp->SetDownloadLowBWTimeout(downloadLowBWTimeout);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 30:
					{
						char preferredSubtitleLang[12];
						printf("[AAMPCLI] Matched Command PreferredSubtitleLang - %s\n", cmd);
						if (sscanf(cmd, "set %s %s", command, preferredSubtitleLang) == 2){
							playerInstanceAamp->SetPreferredSubtitleLanguage(preferredSubtitleLang);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value in string>\n", command);
						}
						break;
					}

				case 31:
					{
						int parallelPlaylistDL;
						printf("[AAMPCLI] Matched Command ParallelPlaylistDL - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &parallelPlaylistDL) == 2){
							playerInstanceAamp->SetParallelPlaylistDL( (parallelPlaylistDL == 1)? true:false );
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 32:
					{
						char preferredLanguages[128];
						char preferredLabel[128];
						char rendition[32];
						char type[16];
						char preferredCodec[128];
						memset(preferredLanguages, 0, sizeof(preferredLanguages));
						memset(preferredLabel, 0, sizeof(preferredLabel));
						memset(rendition, 0, sizeof(rendition));
						memset(type, 0, sizeof(type));
						memset(preferredCodec, 0, sizeof(preferredCodec));
						bool prefLanPresent = false;
						bool prefRendPresent = false;
						bool prefTypePresent = false;
						bool prefCodecPresent = false;
						bool prefLabelPresent = false;
						printf("[AAMPCLI] Matched Command PreferredLanguages - %s\n", cmd);

						if (sscanf(cmd, "set %s %s %s %s %s %s", command, preferredLanguages, rendition, type, preferredCodec, preferredLabel) == 6)
						{
							printf("[AAMPCLI] setting Preferred Languages (%s), rendition (%s), type (%s) , codec (%s) and label (%s)\n" ,
									preferredLanguages, rendition, type, preferredCodec, preferredLabel);
							if (0 != strcasecmp(preferredLanguages, "null"))
							{
								prefLanPresent = true;
							}
							if (0 != strcasecmp(preferredCodec, "null"))
							{
								prefCodecPresent = true;
							}
							if (0 != strcasecmp(rendition, "null"))
							{
								prefRendPresent = true;
							}
							if (0 != strcasecmp(type, "null"))
							{
								prefTypePresent = true;
							}
							if (0 != strcasecmp(preferredLabel, "null"))
							{
								prefLabelPresent = true;
							}

							playerInstanceAamp->SetPreferredLanguages(prefLanPresent?preferredLanguages:NULL,
									prefRendPresent?rendition:NULL,
									prefTypePresent?type:NULL,
									prefCodecPresent?preferredCodec:NULL,
									prefLabelPresent?preferredLabel:NULL);
						}
						else if (sscanf(cmd, "set %s %s %s %s %s", command, preferredLanguages, rendition, type, preferredCodec) == 5)
						{
							printf("[AAMPCLI] setting Preferred Languages (%s), rendition (%s), type (%s) and codec (%s)\n" ,
									preferredLanguages, rendition, type, preferredCodec);
							if (0 != strcasecmp(preferredLanguages, "null"))
							{
								prefLanPresent = true;
							}
							if (0 != strcasecmp(preferredCodec, "null"))
							{
								prefCodecPresent = true;
							}
							if (0 != strcasecmp(rendition, "null"))
							{
								prefRendPresent = true;
							}
							if (0 != strcasecmp(type, "null"))
							{
								prefTypePresent = true;
							}

							playerInstanceAamp->SetPreferredLanguages(prefLanPresent?preferredLanguages:NULL, 
									prefRendPresent?rendition:NULL, 
									prefTypePresent?type:NULL, 
									prefCodecPresent?preferredCodec:NULL);
						}
						else if (sscanf(cmd, "set %s %s %s %s", command, preferredLanguages, rendition, type) == 4)
						{
							printf("[AAMPCLI] setting PreferredLanguages (%s) with rendition (%s) and type (%s)\n" ,
									preferredLanguages, rendition, type);  
							if (0 != strcasecmp(preferredLanguages, "null"))
							{
								prefLanPresent = true;
							}
							if (0 != strcasecmp(rendition, "null"))
							{
								prefRendPresent = true;
							}
							if (0 != strcasecmp(type, "null"))
							{
								prefTypePresent = true;
							}
							playerInstanceAamp->SetPreferredLanguages(prefLanPresent?preferredLanguages:NULL, 
									prefRendPresent?rendition:NULL, 
									prefTypePresent?type:NULL);
						}
						else if (sscanf(cmd, "set %s %s %s", command, preferredLanguages, rendition) == 3)
						{
							printf("[AAMPCLI] setting PreferredLanguages (%s) with rendition (%s)\n" ,
									preferredLanguages, rendition); 
							if (0 != strcasecmp(preferredLanguages, "null"))
							{
								prefLanPresent = true;
							}
							if (0 != strcasecmp(rendition, "null"))
							{
								prefRendPresent = true;
							}
							playerInstanceAamp->SetPreferredLanguages(prefLanPresent?preferredLanguages:NULL, 
									prefRendPresent?rendition:NULL);
						}
						else if (sscanf(cmd, "set %s %s", command, preferredLanguages) == 2)
						{
							printf("[AAMPCLI] setting PreferredLanguages (%s)\n",preferredLanguages );
							if (0 != strcasecmp(preferredLanguages, "null"))
							{
								std::ifstream infile(preferredLanguages);
								std::string line = "";
								std::string data = "";
								if(infile.good())
								{
									/**< If it is file get the data **/
									printf("[AAMPCLI] File Path for Json Data (%s)\n",preferredLanguages );
									while ( getline (infile,line) )
									{
										if (!data.empty())
										{
											data += "\n";
										}
										data += line;
									}
								}
								else
								{
									/**< If it is reqular data assign it**/
									data = std::string(preferredLanguages);
								}

								printf("[AAMPCLI] setting PreferredLanguages (%s)\n",data.c_str());
								playerInstanceAamp->SetPreferredLanguages(data.c_str());
							}
							else
							{
								printf("[AAMPCLI] ERROR: Invalid arguments- %s\n", cmd);
								printf("[AAMPCLI] set preferred languages must be set at least a language\n");
							}

						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments- %s\n", cmd);
							printf("[AAMPCLI] set preferred languages must be run with atleast 2 argument\n");
						}
						break;
					}

				case 33:
					{
						char preferredLanguages[128];
						if (sscanf(cmd, "set %s %s", command, preferredLanguages) == 2)
						{
							printf("[AAMPCLI] setting PreferredText  (%s)\n",preferredLanguages );
							if (0 != strcasecmp(preferredLanguages, "null"))
							{
								std::ifstream infile(preferredLanguages);
								std::string line = "";
								std::string data = "";
								if(infile.good())
								{
									/**< If it is file get the data **/
									while ( getline (infile,line) )
									{
										if (!data.empty())
										{
											data += "\n";
										}
										data += line;
									}
								}
								else
								{
									/**Nomal case */
									data = std::string(preferredLanguages);
								}
								printf("[AAMPCLI] JSON Argument Recieved- %s\n", data.c_str());
								playerInstanceAamp->SetPreferredTextLanguages(data.c_str());
							}
							else
							{
								printf("[AAMPCLI] ERROR: Invalid arguments- %s\n", cmd);
								printf("[AAMPCLI] set preferred text languages must be set at least a language or json data\n");
							}
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments- %s\n", cmd);
							printf("[AAMPCLI] set preferred languages must be run with 2 argument\n");
						}
						break;
					}

				case 34:
					{
						int rampDownLimit;
						printf("[AAMPCLI] Matched Command InitRampdownLimit - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &rampDownLimit) == 2){
							playerInstanceAamp->SetInitRampdownLimit(rampDownLimit);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 35:
					{
						int rampDownLimit;
						printf("[AAMPCLI] Matched Command RampDownLimit - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &rampDownLimit) == 2){
							playerInstanceAamp->SetRampDownLimit(rampDownLimit);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 36:
					{
						long minBitrate;
						printf("[AAMPCLI] Matched Command MinimumBitrate - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &minBitrate) == 2){
							playerInstanceAamp->SetMinimumBitrate(minBitrate);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 37:
					{
						long maxBitrate;
						printf("[AAMPCLI] Matched Command MaximumBitrate - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld", command, &maxBitrate) == 2){
							playerInstanceAamp->SetMaximumBitrate(maxBitrate);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}
				case 54:
					{
						long bitrate1, bitrate2, bitrate3;
						std::vector<long>bitrateList;
						printf("[AAMPCLI] Matched Command VideoTrack - %s\n", cmd);
						if (sscanf(cmd, "set %s %ld %ld %ld", command, &bitrate1, &bitrate2, &bitrate3) == 4){
							bitrateList.push_back(bitrate1);
							bitrateList.push_back(bitrate2);
							bitrateList.push_back(bitrate3);
							playerInstanceAamp->SetVideoTracks(bitrateList);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value1> <value2> <value3>\n", command);
						}
						break;
					}
				case 38:
					{
						int failCount;
						printf("[AAMPCLI] Matched Command MaximumSegmentInjFailCount - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &failCount) == 2){
							playerInstanceAamp->SetSegmentInjectFailCount(failCount);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 49:
					{
						int value;
						printf("[AAMPCLI] Matched Command SslVerifyPeer - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &value) == 2){
							playerInstanceAamp->SetSslVerifyPeerConfig(value == 1);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 39:
					{
						int failCount;
						printf("[AAMPCLI] Matched Command MaximumDrmDecryptFailCount - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &failCount) == 2){
							playerInstanceAamp->SetSegmentDecryptFailCount(failCount);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 40:
					{
						int id3MetadataEventsEnabled;
						printf("[AAMPCLI] Matched Command RegisterForID3MetadataEvents - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &id3MetadataEventsEnabled) == 2){
							if (id3MetadataEventsEnabled)
							{
								playerInstanceAamp->AddEventListener(AAMP_EVENT_ID3_METADATA, lAampcli.mEventListener);
							}
							else
							{
								playerInstanceAamp->RemoveEventListener(AAMP_EVENT_ID3_METADATA, lAampcli.mEventListener);
							}

						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 53:
					{
						int mediaMetadataEventsEnabled;
						printf("[AAMPCLI] Matched Command RegisterForMediaMetadata - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &mediaMetadataEventsEnabled) == 2){
							if (mediaMetadataEventsEnabled)
							{
								playerInstanceAamp->AddEventListener(AAMP_EVENT_MEDIA_METADATA, lAampcli.mEventListener);
							}
							else
							{
								playerInstanceAamp->RemoveEventListener(AAMP_EVENT_MEDIA_METADATA, lAampcli.mEventListener);
							}
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0 or 1>\n", command);
						}
						break;
					}

				case 46:
					{
						LangCodePreference preference;
						int preferenceInt = 0;
						int bDescriptiveAudioTrack = 0;
						if (sscanf(cmd, "set %s %d %d", command, &preferenceInt, &bDescriptiveAudioTrack  ) >= 2)
						{
							preference = (LangCodePreference) preferenceInt;
							playerInstanceAamp->SetLanguageFormat(preference, bDescriptiveAudioTrack!=0 );
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value for preference> <value for track number>\n", command);
						}
						break;
					}

				case 41:
					{
						int duration;
						printf("[AAMPCLI] Matched Command InitialBufferDuration - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &duration) == 2)
						{
							playerInstanceAamp->SetInitialBufferDuration(duration);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 42:
					{
						int track;
						char strTrackInfo[512];
						memset(strTrackInfo, '\0', sizeof(strTrackInfo));
						printf("[AAMPCLI] Matched Command AudioTrack - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &track) == 2)
						{
							playerInstanceAamp->SetAudioTrack(track);
						}
						else if (sscanf(cmd, "set %s %s", command, strTrackInfo) == 2)
						{
							std::string substr = "";
							std::string language = "";
							std::string rendition = "";
							std::string codec = "";
							std::string type = "";
							std::string label = "";
							int channel = 0;
							std::stringstream ss(strTrackInfo);
							/** "language,rendition,codec,channel" **/
							/*language */
							if (std::getline(ss, substr, ',')){
								if(!substr.empty()){
									language = substr;
								}
							} 

							/*rendition */
							if (std::getline(ss, substr, ',')){
								if(!substr.empty()){
									rendition = substr;
								}
							}

							/*type */
							if (std::getline(ss, substr, ',')){
								if(!substr.empty()){
									type = substr;
								}
							} 

							/*codec */
							if (std::getline(ss, substr, ',')){
								if(!substr.empty()){
									codec = substr;
								}
							} 

							/*channel TODO:not supported now */
							if (std::getline(ss, substr, ',')){	
								if(!substr.empty()){
									channel = std::stoi(substr);
								}
							} 
							/*label*/
							if (std::getline(ss, substr, ',')){
								if(!substr.empty()){
									label = substr;
								}
							} 
							printf("[AAMPCLI] Selecting audio track based on language  - %s rendition - %s type = %s codec = %s channel = %d label = %s\n", 
									language.c_str(), rendition.c_str(), type.c_str(), codec.c_str(), channel, label.c_str());
							playerInstanceAamp->SetAudioTrack(language, rendition, type, codec, channel,label);

						}
						break;
					}

				case 43:
					{
						int track = 0;
						char sidecar[128];

						printf("[AAMPCLI] Matched Command TextTrack - %s\n", cmd);
						if (sscanf(cmd, "set %s %127s", command, sidecar) == 2)
						{
							std::ifstream inFile(sidecar);
							if (inFile.good())
							{
								/* Sidecar text file. */
								std::stringstream strStream;
								strStream << inFile.rdbuf();
								std::string str = strStream.str();
								char* data = new char[str.size() + 1];
								std::copy(str.begin(), str.end(), data);
								data[str.size()] = '\0';

								/* Ownership of data is passed to AAMP. */
								playerInstanceAamp->SetTextTrack(0, data);
							}
							else if (sscanf(cmd, "set %s %d", command, &track) == 2)
							{
								/* Text track number. */
								playerInstanceAamp->SetTextTrack(track);
							}
							else
							{
								printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
								printf("[AAMPCLI] Expected: set %s <value> OR set %s <file>\n", command, command);
							}
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] set %s must be run with 1 argument\n", command);
						}
						break;
					}

				case 44:
					{
						int status;
						printf("[AAMPCLI] Matched Command CCStatus - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &status) == 2)
						{
							playerInstanceAamp->SetCCStatus(status == 1);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 45:
					{
						std::string options;
						int value;
						char jsonFile[128];
						char tempChar;

						printf("[AAMPCLI] Matched Command CCStyle - %s\n", cmd);
						if (sscanf(cmd, "set %s %d %c", command, &value, &tempChar) == 2)
						{
							switch (value)
							{
								case 1:
									options = std::string(CC_OPTION_1);
									break;
								case 2:
									options = std::string(CC_OPTION_2);
									break;
								case 3:
									options = std::string(CC_OPTION_3);
									break;
								default:
									printf("[AAMPCLI] Invalid option passed %d. skipping!\n", value);
									break;
							}
						}
						else if (sscanf(cmd, "set %s %127s", command, jsonFile) == 2)
						{
							std::ifstream infile(jsonFile);
							std::string line = "";
							if(infile.good())
							{
								/**< If it is file get the data **/
								while ( getline (infile,line) )
								{
									if (!options.empty())
									{
										options += "\n";
									}
									options += line;
								}
							}
							else
							{
								printf("[AAMPCLI] Invalid filename passed %s. skipping!\n", jsonFile);
							}
						}

						if (!options.empty())
						{
							playerInstanceAamp->SetTextStyle(options);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 1,2 or 3> or json file\n", command);
						}
						break;
					}

				case 47:
					{
						int propagateUriParam;
						printf("[AAMPCLI] Matched Command PropogateUriParam - %s\n", cmd);
						if (sscanf(cmd, "set %s %d", command, &propagateUriParam) == 2){
							playerInstanceAamp->SetPropagateUriParameters((bool) propagateUriParam);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value>\n", command);
						}
						break;
					}

				case 48:
					{
						printf("[AAMPCLI] Matched Command ThumbnailTrack - %s\n",cmd);
						sscanf(cmd, "set %s %d", command, &rate);
						printf("[AAMPCLI] Setting ThumbnailTrack : %s\n",playerInstanceAamp->SetThumbnailTrack(rate)?"Success":"Failure");
						break;
					}
				case 50:
					{
						printf("[AAMPCLI] Matched Command DownloadDelayOnFetch - %s\n",cmd);
						sscanf(cmd, "set %s %d", command, &DownloadDelayInMs);
						playerInstanceAamp->ApplyArtificialDownloadDelay(DownloadDelayInMs);
						break;
					}


				case 51:
					{
						char behavior[24];
						printf("[AAMPCLI] Matched Command PausedBehavior - %s\n", cmd);
						if(sscanf(cmd, "set %s %d", command, &rate) == 2)
						{
							playerInstanceAamp->SetPausedBehavior(rate);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value 0,1,2 or 3>\n", command);
						}
						break;
					}

				case 52:
					{
						char lang[12];
						printf("[AAMPCLI] Matched Command AuxiliaryAudio - %s\n", cmd);
						if (sscanf(cmd, "set %s %s", command, lang) == 2)
						{
							playerInstanceAamp->SetAuxiliaryLanguage(lang);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value in string>\n", command);
						}
						break;
					}

				case 55:
					{
						int timeout;
						if (sscanf(cmd, "set %s %d", command, &timeout) == 2)
						{
							printf("[AAMPCLI] Enabling AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE event registration");
							playerInstanceAamp->AddEventListener(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE, lAampcli.mEventListener);
							playerInstanceAamp->SetContentProtectionDataUpdateTimeout(timeout);
						}
						else
						{
							printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
							printf("[AAMPCLI] Expected: set %s <value in Milliseconds>\n", command);
						}
						break;
					}
				
				default:
					printf("[AAMPCLI] Invalid set command %s\n", command);
					break;
			}

		}
	}
	else
	{
		printf("[AAMPCLI] Invalid set command = %s\n", cmd);
	}

	return true;
}


/**
 * @brief Show help menu with aamp command line interface
 */
void Set::registerSetCommands()
{
	addCommand(1,"rateAndSeek"," <x> <y>","Set Rate and Seek (int x=rate, double y=seconds)");
	addCommand(2,"videoRectangle"," <x> <y> <w> <h>","Set Video Rectangle (int x,y,w,h)");
	addCommand(3,"videoZoom"," <x>","Set Video zoom  ( x = 1 for full, x = 0 for normal)");
	addCommand(4,"videoMute"," <x>","Set Video Mute ( x = 1  - Mute , x = 0 - Unmute)");
	addCommand(5,"audioVolume"," <x>","Set Audio Volume (int x=volume)");
	addCommand(6,"language"," <x>","Set Language (string x=lang)");
	addCommand(7,"subscribedTags","","Set Subscribed Tag - dummy");
	addCommand(8,"licenseServerUrl"," <x> <y>","Set License Server URL (String x=url) (int y=drmType)");
	addCommand(9,"anonymousRequest"," <x>","Set Anonymous Request  (int x=0/1)");
	addCommand(10,"vodTrickplayFps"," <x>","Set VOD Trickplay FPS (int x=trickPlayFPS)");
	addCommand(11,"linearTrickplayFps"," <x>","Set Linear Trickplay FPS (int x=trickPlayFPS)");
	addCommand(12,"liveOffset"," <x>","Set Live offset (int x=offset)");
	addCommand(13,"stallErrorCode"," <x>","Set Stall error code (int x=errorCode)");
	addCommand(14,"stallTimeout"," <x>","Set Stall timeout (int x=timeout)");
	addCommand(15,"reportInterval"," <x>","Set Report Interval (int x=interval)");
	addCommand(16,"videoBitrate"," <x>","Set Video Bitrate (long x=bitrate)");
	addCommand(17,"initialBitrate"," <x>","Set Initial Bitrate (long x = bitrate)");
	addCommand(18,"initialBitrate4k"," <x>","Set Initial Bitrate 4K (long x = bitrate4k)");
	addCommand(19,"networkTimeout"," <x>","Set Network Timeout (long x = timeout in ms)");
	addCommand(20,"manifestTimeout"," <x>","Set Manifest Timeout (long x = timeout in ms)");
	addCommand(21,"downloadBufferSize"," <x>","Set Download Buffer Size (int x = bufferSize)");
	addCommand(22,"preferredDrm"," <x>","Set Preferred DRM (int x=1-WV, 2-PR, 4-Access, 5-AES 6-ClearKey)");
	addCommand(23,"stereoOnlyPlayback"," <x>","Set Stereo only playback (x=1/0)");
	addCommand(24,"alternateContent","","Set Alternate Contents - dummy ()");
	addCommand(25,"networkProxy"," <x>","Set Set Network Proxy (string x = url)");
	addCommand(26,"licenseReqProxy"," <x>","Set License Request Proxy (string x=url)");
	addCommand(27,"downloadStallTimeout"," <x>","Set Download Stall timeout (long x=timeout)");
	addCommand(28,"downloadStartTimeout"," <x>","Set Download Start timeout (long x=timeout)");
	addCommand(29,"downloadLowBWTimeout"," <x>","Set Download Low Bandwidth timeout (long x=timeout)");
	addCommand(30,"preferredSubtitleLang"," <x>","Set Preferred Subtitle language (string x = lang)");
	addCommand(31,"parallelPlaylistDL"," <x>","Set Parallel Playlist download (x=0/1)");
	addCommand(32,"preferredLanguages"," <x>","Set Preferred languages (string lang1,lang2,... rendition type codec1,codec2.. ) , use null keyword to discard any argument or path to json file");
	addCommand(33,"preferredTextLanguages"," <x>","Set Preferred languages (string lang1,lang2,... ) , or path json file");
	addCommand(34,"rampDownLimit"," <x>","Set number of Ramp Down limit during the playback (x = number)");
	addCommand(35,"initRampdownLimit"," <x>","Set number of Initial Ramp Down limit prior to the playback (x = number)");
	addCommand(36,"minimumBitrate"," <x>","Set Minimum bitrate (x = bitrate)");
	addCommand(37,"maximumBitrate"," <x>","Set Maximum bitrate (x = bitrate)");
	addCommand(38,"maximumSegmentInjFailCount"," <x>","Set Maximum segment injection fail count (int x = count)");
	addCommand(39,"maximumDrmDecryptFailCount"," <x>","Set Maximum DRM Decryption fail count(int x = count)");
	addCommand(40,"registerForID3MetadataEvents"," <x>","Set Listen for ID3_METADATA events (x = 1 - add listener, x = 0 - remove)");
	addCommand(41,"initialBufferDuration"," <x>","Set Initial Buffer Duration (int x = Duration in sec)");
	addCommand(42,"audioTrack"," <x>","Set Audio track ( x = track by index (number) OR track by language properties lan,rendition,type,codec )");
	addCommand(43,"textTrack"," <x>","Set Text track (int x = track index)");
	addCommand(44,"ccStatus"," <x>","Set CC status (x = 0/1)");
	addCommand(45,"ccStyle"," <x>","Set a predefined CC style commandion (x = 1/2/3)");
	addCommand(46,"languageFormat"," <x> <y> ","Set Language Format (x = preferredFormat(0-3), y = useRole(0/1))");
	//addCommand("auxiliaryAudio"," <x>","Set auxiliary audio language (x = string lang)");
	addCommand(47,"propagateUriParam"," <x>","Set propagate uri parameters: (int x = 0 to disable)");
	//addCommand("rateOnTune"," <x>","Set Pre-tune rate (x= PreTuneRate)");
	addCommand(48,"thumbnailTrack"," <x>","Set Thumbnail Track (int x = Thumbnail Index)");
	addCommand(49,"sslVerifyPeer"," <x>","Set Ssl Verify Peer flag (x = 1 for enabling)");
	addCommand(50,"downloadDelayOnFetch"," <x>","Set delay while downloading fragments (unsigned int x = download delay in ms)");        
	addCommand(51,"pausedBehavior"," <x>","Set Paused behavior (int x (0-3) options -\"autoplay defer\",\"autoplay immediate\",\"live defer\",\"live immediate\"");
	addCommand(52,"auxiliaryAudio"," <x>","Set auxiliary audio language (x = string lang)");
	addCommand(53,"registerForMediaMetadata"," <x>","Set Listen for AAMP_EVENT_MEDIA_METADATA events (x = 1 - add listener, x = 0 - remove)");
	addCommand(54,"videoTrack"," <x> <y> <z> ","Set Video tracks range (x = bitrate1, y = bitrate2, z = bitrate3) OR single bitrate provide same value for x, y,z ");
	addCommand(55,"dynamicDrm"," <x> ","set Dynamic DRM config in Json format x=Timeout value for response message ");
	addCommand(56,"liveOffset4K"," <x> ","Set Live offset 4K stream(int x=offset)");
	commands.push_back("help");
}

void Set::addCommand(int value,string command,string param,string description)
{
	setCommandInfo lCmdInfo;
	lCmdInfo.value = value;
	lCmdInfo.param = param;
	lCmdInfo.description = description;

	setCommands.insert(std::make_pair(command,lCmdInfo));
	commands.push_back(command);
}

/**
 * @brief Display Help menu for set
 * @param none
 */
void Set::ShowHelpSet()
{
	std::map<string,setCommandInfo>::iterator setCmdItr;
	
	printf("******************************************************************************************\n");
	printf("*   set <command> [<arguments>]\n");
	printf("*   Usage of Commands with arguemnts expected in ()\n");
	printf("******************************************************************************************\n");
	
	if(!commands.empty())
	{

		if(!commands.empty())
		{
			for(auto itr:commands)
			{
				setCmdItr = setCommands.find(itr);

				if(setCmdItr != setCommands.end())
				{
					std::string strCmd = setCmdItr->first + (setCmdItr->second).param;
					std::cout << "set " << std::right << std::setw(2) << (setCmdItr->second).value << " / " << std::setw(35) << std::left << (strCmd).c_str() << "// " << (setCmdItr->second.description).c_str() << "\n";
				}
			}
		}
	}
	
	printf("******************************************************************************************\n");
}

char * Set::setCommandRecommender(const char *text, int state)
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
