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

#include"AampcliSet.h"
#include"Aampcli.h"

const char *mSetHelpText[eAAMP_SET_TYPE_COUNT];

/**
 * @brief Show help menu with aamp command line interface
 */
void Set::initSetHelpText()
{
	mSetHelpText[eAAMP_SET_RateAndSeek] =                        "<x> <y>         // Set Rate and Seek (int x=rate, double y=seconds)";
	mSetHelpText[eAAMP_SET_VideoRectangle] =                     "<x> <y> <w> <h> // Set Video Rectangle (int x,y,w,h)";
	mSetHelpText[eAAMP_SET_VideoZoom] =                          "<x>             // Set Video zoom  ( x = 1 for full, x = 0 for normal)";
	mSetHelpText[eAAMP_SET_VideoMute] =                          "<x>             // Set Video Mute ( x = 1  - Mute , x = 0 - Unmute)";
	mSetHelpText[eAAMP_SET_AudioVolume] =                        "<x>             // Set Audio Volume (int x=volume)";
	mSetHelpText[eAAMP_SET_Language] =                           "<x>             // Set Language (string x=lang)";
	mSetHelpText[eAAMP_SET_SubscribedTags] =                     "                // Set Subscribed Tag - dummy";
	mSetHelpText[eAAMP_SET_LicenseServerUrl] =                   "<x> <y>         // Set License Server URL (String x=url) (int y=drmType)";
	mSetHelpText[eAAMP_SET_AnonymousRequest] =                   "<x>             // Set Anonymous Request  (int x=0/1)";
	mSetHelpText[eAAMP_SET_VodTrickplayFps] =                    "<x>             // Set VOD Trickplay FPS (int x=trickPlayFPS)";
	mSetHelpText[eAAMP_SET_LinearTrickplayFps] =                 "<x>             // Set Linear Trickplay FPS (int x=trickPlayFPS)";
	mSetHelpText[eAAMP_SET_LiveOffset] =                         "<x>             // Set Live offset (int x=offset)";
	mSetHelpText[eAAMP_SET_StallErrorCode] =                     "<x>             // Set Stall error code (int x=errorCode)";
	mSetHelpText[eAAMP_SET_StallTimeout] =                       "<x>             // Set Stall timeout (int x=timeout)";
	mSetHelpText[eAAMP_SET_ReportInterval] =                     "<x>             // Set Report Interval (int x=interval)";
	mSetHelpText[eAAMP_SET_VideoBitarate] =                      "<x>             // Set Video Bitrate (long x=bitrate)";
	mSetHelpText[eAAMP_SET_InitialBitrate] =                     "<x>             // Set Initial Bitrate (long x = bitrate)";
	mSetHelpText[eAAMP_SET_InitialBitrate4k] =                   "<x>             // Set Initial Bitrate 4K (long x = bitrate4k)";
	mSetHelpText[eAAMP_SET_NetworkTimeout] =                     "<x>             // Set Network Timeout (long x = timeout in ms)";
	mSetHelpText[eAAMP_SET_ManifestTimeout] =                    "<x>             // Set Manifest Timeout (long x = timeout in ms)";
	mSetHelpText[eAAMP_SET_DownloadBufferSize] =                 "<x>             // Set Download Buffer Size (int x = bufferSize)";
	mSetHelpText[eAAMP_SET_PreferredDrm] =                       "<x>             // Set Preferred DRM (int x=1-WV, 2-PR, 4-Access, 5-AES 6-ClearKey)";
	mSetHelpText[eAAMP_SET_StereoOnlyPlayback] =                 "<x>             // Set Stereo only playback (x=1/0)";
	mSetHelpText[eAAMP_SET_AlternateContent] =                   "                // Set Alternate Contents - dummy ()";
	mSetHelpText[eAAMP_SET_NetworkProxy] =                       "<x>             // Set Set Network Proxy (string x = url)";
	mSetHelpText[eAAMP_SET_LicenseReqProxy] =                    "<x>             // Set License Request Proxy (string x=url)";
	mSetHelpText[eAAMP_SET_DownloadStallTimeout] =               "<x>             // Set Download Stall timeout (long x=timeout)";
	mSetHelpText[eAAMP_SET_DownloadStartTimeout] =               "<x>             // Set Download Start timeout (long x=timeout)";
	mSetHelpText[eAAMP_SET_DownloadLowBWTimeout] =               "<x>             // Set Download Low Bandwidth timeout (long x=timeout)";
	mSetHelpText[eAAMP_SET_PreferredSubtitleLang] =              "<x>             // Set Preferred Subtitle language (string x = lang)";
	mSetHelpText[eAAMP_SET_ParallelPlaylistDL] =                 "<x>             // Set Parallel Playlist download (x=0/1)";
	mSetHelpText[eAAMP_SET_PreferredLanguages] =                 "<x>             // Set Preferred languages (string lang1,lang2,... rendition type codec1,codec2.. ) , use null keyword to discard any argument";
	mSetHelpText[eAAMP_SET_RampDownLimit] =                      "<x>             // Set number of Ramp Down limit during the playback (x = number)";
	mSetHelpText[eAAMP_SET_InitRampdownLimit] =                  "<x>             // Set number of Initial Ramp Down limit prior to the playback (x = number)";
	mSetHelpText[eAAMP_SET_MinimumBitrate] =                     "<x>             // Set Minimum bitrate (x = bitrate)";
	mSetHelpText[eAAMP_SET_MaximumBitrate] =                     "<x>             // Set Maximum bitrate (x = bitrate)";
	mSetHelpText[eAAMP_SET_MaximumSegmentInjFailCount] =         "<x>             // Set Maximum segment injection fail count (int x = count)";
	mSetHelpText[eAAMP_SET_MaximumDrmDecryptFailCount] =         "<x>             // Set Maximum DRM Decryption fail count(int x = count)";
	mSetHelpText[eAAMP_SET_RegisterForID3MetadataEvents] =       "<x>             // Set Listen for ID3_METADATA events (x = 1 - add listener, x = 0 - remove)";
	mSetHelpText[eAAMP_SET_LanguageFormat] =                     "<y> <y>         // Set Language Format (x = preferredFormat(0-3), y = useRole(0/1))";
	mSetHelpText[eAAMP_SET_InitialBufferDuration] =              "<x>             // Set Initial Buffer Duration (int x = Duration in sec)";
	mSetHelpText[eAAMP_SET_AudioTrack] =                         "<x>             // Set Audio track ( x = track by index (number) OR track by language properties lan,rendition,type,codec )";
	mSetHelpText[eAAMP_SET_TextTrack] =                          "<x>             // Set Text track (int x = track index)";
	mSetHelpText[eAAMP_SET_CCStatus] =                           "<x>             // Set CC status (x = 0/1)";
	mSetHelpText[eAAMP_SET_CCStyle] =                            "<x>             // Set a predefined CC style option (x = 1/2/3)";
	//mSetHelpText[eAAMP_SET_AuxiliaryAudio] =                     "<x>             // Set auxiliary audio language (x = string lang)";
	mSetHelpText[eAAMP_SET_PropagateUriParam] =                  "<x>             // Set propagate uri parameters: (int x = 0 to disable)";
	//mSetHelpText[eAAMP_SET_RateOnTune] =                         "<x>             // Set Pre-tune rate (x= PreTuneRate)";
	mSetHelpText[eAAMP_SET_ThumbnailTrack] =                     "<x>             // Set Thumbnail Track (int x = Thumbnail Index)";
	mSetHelpText[eAAMP_SET_SslVerifyPeer] =                      "<x>             // Set Ssl Verify Peer flag (x = 1 for enabling)";
	mSetHelpText[eAAMP_SET_DownloadDelayOnFetch] =               "<x>             // Set delay while downloading fragments (unsigned int x = download delay in ms)";
	mSetHelpText[eAAMP_SET_PausedBehavior] =                     "<x>             // Set Paused behavior (int x (0-3) options -\"autoplay defer\",\"autoplay immediate\",\"live defer\",\"live immediate\"";
	mSetHelpText[eAAMP_SET_AuxiliaryAudio] =                     "<x>             // Set auxiliary audio language (x = string lang)";
	mSetHelpText[eAAMP_SET_RegisterForMediaMetadata] =           "<x>             // Set Listen for AAMP_EVENT_MEDIA_METADATA events (x = 1 - add listener, x = 0 - remove)";
	mSetHelpText[eAAMP_SET_VideoTrack] =			     "<x> <y> <z>     // Set Video tracks range (x = bitrate1, y = bitrate2, z = bitrate3) OR single bitrate provide same value for x, y,z ";
}

/**
 * @brief Display Help menu for set
 * @param none
 */
void Set::showHelpSet()
{
	printf("******************************************************************************************\n");
	printf("*   set <command> [<arguments>]\n");
	printf("*   Usage of Commands with arguemnts expected in ()\n");
	printf("******************************************************************************************\n");

	for (int iLoop = 0; iLoop < eAAMP_SET_TYPE_COUNT; iLoop++)
	{
		printf("* set %2d %s\n", iLoop+1, mSetHelpText[iLoop]);
	}

	printf("******************************************************************************************\n");
}

void Set::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	printf("%s:%d:cmd %s\n", __FUNCTION__, __LINE__,cmd);
	char help[8];
	int opt;
	int rate = 0;
	unsigned int DownloadDelayInMs = 0;
	Aampcli lAampcli;

	if (sscanf(cmd, "set %d", &opt) == 1){
		switch(opt-1){ // 1 based to zero based
			case eAAMP_SET_RateAndSeek:
				{
					int rate;
					double ralatineTuneTime;
					printf("[AAMPCLI] Matched Command eAAMP_SET_RateAndSeek - %s\n", cmd);
					if (sscanf(cmd, "set %d %d %lf", &opt, &rate, &ralatineTuneTime ) == 3){
						playerInstanceAamp->SetRateAndSeek(rate, ralatineTuneTime);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <rate=0,1 etc> <seek time in sec>\n", opt);
					}
					break;
				}

			case eAAMP_SET_VideoRectangle:
				{
					int x,y,w,h;
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoRectangle - %s\n", cmd);
					if (sscanf(cmd, "set %d %d %d %d %d", &opt, &x, &y, &w, &h) == 5){
						playerInstanceAamp->SetVideoRectangle(x,y,w,h);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <x> <y> <w> <h>\n", opt);
					}
					break;
				}

			case eAAMP_SET_VideoZoom:
				{
					int videoZoom;
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoZoom - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &videoZoom) == 2){
						playerInstanceAamp->SetVideoZoom((videoZoom > 0 )? VIDEO_ZOOM_FULL : VIDEO_ZOOM_NONE );
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value value>0? ZOOM_FULL : ZOOM_NONE>\n", opt);
					}
					break;
				}

			case eAAMP_SET_VideoMute:
				{
					int videoMute;
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoMute - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &videoMute) == 2){
						playerInstanceAamp->SetVideoMute((videoMute == 1 )? true : false );
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_AudioVolume:
				{
					int vol;
					printf("[AAMPCLI] Matched Command eAAMP_SET_AudioVolume - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &vol) == 2){
						playerInstanceAamp->SetAudioVolume(vol);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value to set audio volume>\n", opt);
					}
					break;
				}

			case eAAMP_SET_Language:
				{
					char lang[12];
					printf("[AAMPCLI] Matched Command eAAMP_SET_Language - %s\n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, lang) == 2){
						playerInstanceAamp->SetLanguage(lang);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <lang in string>\n", opt);
					}
					break;
				}

			case eAAMP_SET_SubscribedTags:
				{
					//Dummy implimentation
					std::vector<std::string> subscribedTags;
					printf("[AAMPCLI] Matched Command eAAMP_SET_SubscribedTags - %s\n", cmd);
					playerInstanceAamp->SetSubscribedTags(subscribedTags);
					break;
				}

			case eAAMP_SET_LicenseServerUrl:
				{
					char lisenceUrl[1024];
					int drmType;
					printf("[AAMPCLI] Matched Command eAAMP_SET_LicenseServerUrl - %s\n", cmd);
					if (sscanf(cmd, "set %d %s %d", &opt, lisenceUrl, &drmType) == 3){
						playerInstanceAamp->SetLicenseServerURL(lisenceUrl,
								(drmType == eDRM_PlayReady)?eDRM_PlayReady:eDRM_WideVine);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <license url in string> <value value==2? ?eDRM_PlayReady : eDRM_WideVine>\n", opt);
					}
					break;
				}

			case eAAMP_SET_AnonymousRequest:
				{
					int isAnonym;
					printf("[AAMPCLI] Matched Command eAAMP_SET_AnonymousRequest - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &isAnonym) == 2){
						playerInstanceAamp->SetAnonymousRequest((isAnonym == 1)?true:false);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_VodTrickplayFps:
				{
					int vodTFps;
					printf("[AAMPCLI] Matched Command eAAMP_SET_VodTrickplayFps - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &vodTFps) == 2){
						playerInstanceAamp->SetVODTrickplayFPS(vodTFps);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_LinearTrickplayFps:
				{
					int linearTFps;
					printf("[AAMPCLI] Matched Command eAAMP_SET_LinearTrickplayFps - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &linearTFps) == 2){
						playerInstanceAamp->SetLinearTrickplayFPS(linearTFps);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_LiveOffset:
				{
					int liveOffset;
					printf("[AAMPCLI] Matched Command eAAMP_SET_LiveOffset - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &liveOffset) == 2){
						playerInstanceAamp->SetLiveOffset(liveOffset);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_StallErrorCode:
				{
					int stallErrorCode;
					printf("[AAMPCLI] Matched Command eAAMP_SET_StallErrorCode - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &stallErrorCode) == 2){
						playerInstanceAamp->SetStallErrorCode(stallErrorCode);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_StallTimeout:
				{
					int stallTimeout;
					printf("[AAMPCLI] Matched Command eAAMP_SET_StallTimeout - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &stallTimeout) == 2){
						playerInstanceAamp->SetStallTimeout(stallTimeout);
					}
					break;
				}

			case eAAMP_SET_ReportInterval:
				{
					int reportInterval;
					printf("[AAMPCLI] Matched Command eAAMP_SET_ReportInterval - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &reportInterval) == 2){
						playerInstanceAamp->SetReportInterval(reportInterval);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_VideoBitarate:
				{
					long videoBitarate;
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoBitarate - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &videoBitarate) == 2){
						playerInstanceAamp->SetVideoBitrate(videoBitarate);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_InitialBitrate:
				{
					long initialBitrate;
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitialBitrate - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &initialBitrate) == 2){
						playerInstanceAamp->SetInitialBitrate(initialBitrate);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_InitialBitrate4k:
				{
					long initialBitrate4k;
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitialBitrate4k - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &initialBitrate4k) == 2){
						playerInstanceAamp->SetInitialBitrate4K(initialBitrate4k);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_NetworkTimeout:
				{
					double networkTimeout;
					printf("[AAMPCLI] Matched Command eAAMP_SET_NetworkTimeout - %s\n", cmd);
					if (sscanf(cmd, "set %d %lf", &opt, &networkTimeout) == 2){
						playerInstanceAamp->SetNetworkTimeout(networkTimeout);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_ManifestTimeout:
				{
					double manifestTimeout;
					printf("[AAMPCLI] Matched Command eAAMP_SET_ManifestTimeout - %s\n", cmd);
					if (sscanf(cmd, "set %d %lf", &opt, &manifestTimeout) == 2){
						playerInstanceAamp->SetManifestTimeout(manifestTimeout);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_DownloadBufferSize:
				{
					int downloadBufferSize;
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadBufferSize - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &downloadBufferSize) == 2){
						playerInstanceAamp->SetDownloadBufferSize(downloadBufferSize);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_PreferredDrm:
				{
					int preferredDrm;
					printf("[AAMPCLI] Matched Command eAAMP_SET_PreferredDrm - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &preferredDrm) == 2){
						playerInstanceAamp->SetPreferredDRM((DRMSystems)preferredDrm);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_StereoOnlyPlayback:
				{
					int stereoOnlyPlayback;
					printf("[AAMPCLI] Matched Command eAAMP_SET_StereoOnlyPlayback - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &stereoOnlyPlayback) == 2){
						playerInstanceAamp->SetStereoOnlyPlayback(
								(stereoOnlyPlayback == 1 )? true:false);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_AlternateContent:
				{
					//Dummy implementation
					std::string adBrkId = "";
					std::string adId = "";
					std::string url = "";
					printf("[AAMPCLI] Matched Command eAAMP_SET_AlternateContent - %s\n", cmd);
					playerInstanceAamp->SetAlternateContents(adBrkId, adId, url);
					break;
				}

			case eAAMP_SET_NetworkProxy:
				{
					char networkProxy[128];
					printf("[AAMPCLI] Matched Command eAAMP_SET_NetworkProxy - %s\n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, networkProxy) == 2){
						playerInstanceAamp->SetNetworkProxy(networkProxy);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value in string>\n", opt);
					}
					break;
				}

			case eAAMP_SET_LicenseReqProxy:
				{
					char licenseReqProxy[128];
					printf("[AAMPCLI] Matched Command eAAMP_SET_LicenseReqProxy - %s\n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, licenseReqProxy) == 2){
						playerInstanceAamp->SetLicenseReqProxy(licenseReqProxy);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value in string>\n", opt);
					}
					break;
				}

			case eAAMP_SET_DownloadStallTimeout:
				{
					long downloadStallTimeout;
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadStallTimeout - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &downloadStallTimeout) == 2){
						playerInstanceAamp->SetDownloadStallTimeout(downloadStallTimeout);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_DownloadStartTimeout:
				{
					long downloadStartTimeout;
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadStartTimeout - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &downloadStartTimeout) == 2){
						playerInstanceAamp->SetDownloadStartTimeout(downloadStartTimeout);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_DownloadLowBWTimeout:
				{
					long downloadLowBWTimeout;
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadLowBWTimeout - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &downloadLowBWTimeout) == 2){
						playerInstanceAamp->SetDownloadLowBWTimeout(downloadLowBWTimeout);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_PreferredSubtitleLang:
				{
					char preferredSubtitleLang[12];
					printf("[AAMPCLI] Matched Command eAAMP_SET_PreferredSubtitleLang - %s\n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, preferredSubtitleLang) == 2){
						playerInstanceAamp->SetPreferredSubtitleLanguage(preferredSubtitleLang);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value in string>\n", opt);
					}
					break;
				}

			case eAAMP_SET_ParallelPlaylistDL:
				{
					int parallelPlaylistDL;
					printf("[AAMPCLI] Matched Command eAAMP_SET_ParallelPlaylistDL - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &parallelPlaylistDL) == 2){
						playerInstanceAamp->SetParallelPlaylistDL( (parallelPlaylistDL == 1)? true:false );
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_PreferredLanguages:
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_PreferredLanguages - %s\n", cmd);

					if (sscanf(cmd, "set %d %s %s %s %s %s", &opt, preferredLanguages, rendition, type, preferredCodec, preferredLabel) == 6)
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
					else if (sscanf(cmd, "set %d %s %s %s %s", &opt, preferredLanguages, rendition, type, preferredCodec) == 5)
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
					else if (sscanf(cmd, "set %d %s %s %s", &opt, preferredLanguages, rendition, type) == 4)
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
					else if (sscanf(cmd, "set %d %s %s", &opt, preferredLanguages, rendition) == 3)
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
					else if (sscanf(cmd, "set %d %s", &opt, preferredLanguages) == 2)
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

			case eAAMP_SET_PreferredTextLanguages:
				{
					char preferredLanguages[128];
					if (sscanf(cmd, "set %d %s", &opt, preferredLanguages) == 2)
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

			case eAAMP_SET_InitRampdownLimit:
				{
					int rampDownLimit;
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitRampdownLimit - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &rampDownLimit) == 2){
						playerInstanceAamp->SetInitRampdownLimit(rampDownLimit);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_RampDownLimit:
				{
					int rampDownLimit;
					printf("[AAMPCLI] Matched Command eAAMP_SET_RampDownLimit - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &rampDownLimit) == 2){
						playerInstanceAamp->SetRampDownLimit(rampDownLimit);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_MinimumBitrate:
				{
					long minBitrate;
					printf("[AAMPCLI] Matched Command eAAMP_SET_MinimumBitrate - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &minBitrate) == 2){
						playerInstanceAamp->SetMinimumBitrate(minBitrate);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_MaximumBitrate:
				{
					long maxBitrate;
					printf("[AAMPCLI] Matched Command eAAMP_SET_MaximumBitrate - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &maxBitrate) == 2){
						playerInstanceAamp->SetMaximumBitrate(maxBitrate);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}
			case eAAMP_SET_VideoTrack:
				{
					long bitrate1, bitrate2, bitrate3;
					std::vector<long>bitrateList;
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoTrack - %s\n", cmd);
					if (sscanf(cmd, "set %d %ld %ld %ld", &opt, &bitrate1, &bitrate2, &bitrate3) == 4){
						bitrateList.push_back(bitrate1);
						bitrateList.push_back(bitrate2);
						bitrateList.push_back(bitrate3);
						playerInstanceAamp->SetVideoTracks(bitrateList);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value1> <value2> <value3>\n", opt);
					}
					break;
				}
			case eAAMP_SET_MaximumSegmentInjFailCount:
				{
					int failCount;
					printf("[AAMPCLI] Matched Command eAAMP_SET_MaximumSegmentInjFailCount - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &failCount) == 2){
						playerInstanceAamp->SetSegmentInjectFailCount(failCount);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_SslVerifyPeer:
				{
					int value;
					printf("[AAMPCLI] Matched Command eAAMP_SET_SslVerifyPeer - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &value) == 2){
						playerInstanceAamp->SetSslVerifyPeerConfig(value == 1);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_MaximumDrmDecryptFailCount:
				{
					int failCount;
					printf("[AAMPCLI] Matched Command eAAMP_SET_MaximumDrmDecryptFailCount - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &failCount) == 2){
						playerInstanceAamp->SetSegmentDecryptFailCount(failCount);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_RegisterForID3MetadataEvents:
				{
					int id3MetadataEventsEnabled;
					printf("[AAMPCLI] Matched Command eAAMP_SET_RegisterForID3MetadataEvents - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &id3MetadataEventsEnabled) == 2){
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
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_RegisterForMediaMetadata:
				{
					int mediaMetadataEventsEnabled;
					printf("[AAMPCLI] Matched Command eAAMP_SET_RegisterForMediaMetadata - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &mediaMetadataEventsEnabled) == 2){
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
						printf("[AAMPCLI] Expected: set %d <value 0 or 1>\n", opt);
					}
					break;
				}

			case eAAMP_SET_LanguageFormat:
				{
					LangCodePreference preference;
					int preferenceInt = 0;
					int bDescriptiveAudioTrack = 0;
					if (sscanf(cmd, "set %d %d %d", &opt, &preferenceInt, &bDescriptiveAudioTrack  ) >= 2)
					{
						preference = (LangCodePreference) preferenceInt;
						playerInstanceAamp->SetLanguageFormat(preference, bDescriptiveAudioTrack!=0 );
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value for preference> <value for track number>\n", opt);
					}
					break;
				}

			case eAAMP_SET_InitialBufferDuration:
				{
					int duration;
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitialBufferDuration - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &duration) == 2)
					{
						playerInstanceAamp->SetInitialBufferDuration(duration);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_AudioTrack:
				{
					int track;
					char strTrackInfo[512];
					memset(strTrackInfo, '\0', sizeof(strTrackInfo));
					printf("[AAMPCLI] Matched Command eAAMP_SET_AudioTrack - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &track) == 2)
					{
						playerInstanceAamp->SetAudioTrack(track);
					}
					else if (sscanf(cmd, "set %d %s", &opt, strTrackInfo) == 2)
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

			case eAAMP_SET_TextTrack:
				{
					int track;
					printf("[AAMPCLI] Matched Command eAAMP_SET_TextTrack - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &track) == 2)
					{
						playerInstanceAamp->SetTextTrack(track);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_CCStatus:
				{
					int status;
					printf("[AAMPCLI] Matched Command eAAMP_SET_CCStatus - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &status) == 2)
					{
						playerInstanceAamp->SetCCStatus(status == 1);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_CCStyle:
				{
					int value;
					printf("[AAMPCLI] Matched Command eAAMP_SET_CCStyle - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &value) == 2)
					{
						std::string options;
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
								printf("[AAMPCLI] Invalid option passed. skipping!\n");
								break;
						}
						if (!options.empty())
						{
							playerInstanceAamp->SetTextStyle(options);
						}
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 1,2 or 3>\n", opt);
					}
					break;
				}

			case eAAMP_SET_PropagateUriParam:
				{
					int propagateUriParam;
					printf("[AAMPCLI] Matched Command eAAMP_SET_PropogateUriParam - %s\n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &propagateUriParam) == 2){
						playerInstanceAamp->SetPropagateUriParameters((bool) propagateUriParam);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value>\n", opt);
					}
					break;
				}

			case eAAMP_SET_ThumbnailTrack:
				{
					printf("[AAMPCLI] Matched Command eAAMP_SET_ThumbnailTrack - %s\n",cmd);
					sscanf(cmd, "set %d %d", &opt, &rate);
					printf("[AAMPCLI] Setting ThumbnailTrack : %s\n",playerInstanceAamp->SetThumbnailTrack(rate)?"Success":"Failure");
					break;
				}
			case eAAMP_SET_DownloadDelayOnFetch:
				{
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadDelayOnFetch - %s\n",cmd);
					sscanf(cmd, "set %d %d", &opt, &DownloadDelayInMs);
					playerInstanceAamp->ApplyArtificialDownloadDelay(DownloadDelayInMs);
					break;
				}


			case eAAMP_SET_PausedBehavior:
				{
					char behavior[24];
					printf("[AAMPCLI] Matched Command eAAMP_SET_PausedBehavior - %s\n", cmd);
					if(sscanf(cmd, "set %d %d", &opt, &rate) == 2)
					{
						playerInstanceAamp->SetPausedBehavior(rate);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value 0,1,2 or 3>\n", opt);
					}
					break;
				}

			case eAAMP_SET_AuxiliaryAudio:
				{
					char lang[12];
					printf("[AAMPCLI] Matched Command eAAMP_SET_AuxiliaryAudio - %s\n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, lang) == 2)
					{
						playerInstanceAamp->SetAuxiliaryLanguage(lang);
					}
					else
					{
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
						printf("[AAMPCLI] Expected: set %d <value in string>\n", opt);
					}
					break;
				}
			default:
				printf("[AAMPCLI] Invalid set command %d\n", opt );
				break;
		}

	}
	else if (sscanf(cmd, "set %s", help) == 1)
	{
		printf("%s:%d:in help %s\n", __FUNCTION__, __LINE__,cmd);
		if(0 == strncmp("help", help, 4))
		{
			showHelpSet();
		}else
		{
			printf("[AAMPCLI] Invalid usage of set operations %s\n", help);
		}
	}
	else
	{
		printf("[AAMPCLI] Invalid set command = %s\n", cmd);
	}
}

