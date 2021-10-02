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
 * @file aampcli.cpp
 * @brief Stand alone AAMP player with command line interface.
 */

#ifdef RENDER_FRAMES_IN_APP_CONTEXT
#ifdef __APPLE__
	#define GL_SILENCE_DEPRECATION
	#include <OpenGL/gl3.h>
	#include <GLUT/glut.h>
#else	//Linux
	#include <GL/glew.h>
	#include <GL/gl.h>
	#include <GL/freeglut.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif

#ifdef __APPLE__
#import <cocoa_window.h>
#endif
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <list>
#include <sstream>
#include <string>
#include <ctype.h>
#include <gst/gst.h>
#include <priv_aamp.h>
#include <main_aamp.h>
#include "AampConfig.h"
#include "../StreamAbstractionAAMP.h"

#define MAX_BUFFER_LENGTH 4096

#define CC_OPTION_1 "{\"penItalicized\":false,\"textEdgeStyle\":\"none\",\"textEdgeColor\":\"black\",\"penSize\":\"small\",\"windowFillColor\":\"black\",\"fontStyle\":\"default\",\"textForegroundColor\":\"white\",\"windowFillOpacity\":\"transparent\",\"textForegroundOpacity\":\"solid\",\"textBackgroundColor\":\"black\",\"textBackgroundOpacity\":\"solid\",\"windowBorderEdgeStyle\":\"none\",\"windowBorderEdgeColor\":\"black\",\"penUnderline\":false}"
#define CC_OPTION_2 "{\"penItalicized\":false,\"textEdgeStyle\":\"none\",\"textEdgeColor\":\"yellow\",\"penSize\":\"small\",\"windowFillColor\":\"black\",\"fontStyle\":\"default\",\"textForegroundColor\":\"yellow\",\"windowFillOpacity\":\"transparent\",\"textForegroundOpacity\":\"solid\",\"textBackgroundColor\":\"cyan\",\"textBackgroundOpacity\":\"solid\",\"windowBorderEdgeStyle\":\"none\",\"windowBorderEdgeColor\":\"black\",\"penUnderline\":true}"
#define CC_OPTION_3 "{\"penItalicized\":false,\"textEdgeStyle\":\"none\",\"textEdgeColor\":\"black\",\"penSize\":\"large\",\"windowFillColor\":\"blue\",\"fontStyle\":\"default\",\"textForegroundColor\":\"red\",\"windowFillOpacity\":\"transparent\",\"textForegroundOpacity\":\"solid\",\"textBackgroundColor\":\"white\",\"textBackgroundOpacity\":\"solid\",\"windowBorderEdgeStyle\":\"none\",\"windowBorderEdgeColor\":\"black\",\"penUnderline\":false}"

#define VIRTUAL_CHANNEL_VALID(x) ((x) > 0)

static PlayerInstanceAAMP *mSingleton = NULL;
static PlayerInstanceAAMP *mBackgroundPlayer = NULL;
static GMainLoop *AAMPGstPlayerMainLoop = NULL;
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
void updateYUVFrame(uint8_t *buffer, int size, int width, int height);
std::mutex appsinkData_mutex;
#endif

/**
 * @enum AAMPGetTypes
 * @brief Define the enum values of get types
 */
typedef enum {
	eAAMP_GET_CurrentState,
	eAAMP_GET_CurrentAudioLan,
	eAAMP_GET_CurrentDrm,
	eAAMP_GET_PlaybackPosition,
	eAAMP_GET_PlaybackDuration,
	eAAMP_GET_VideoBitrate,
	eAAMP_GET_AudioBitrate,
	eAAMP_GET_AudioVolume,
	eAAMP_GET_PlaybackRate,
	eAAMP_GET_VideoBitrates,
	eAAMP_GET_AudioBitrates,
	eAAMP_GET_CurrentPreferredLanguages,
	eAAMP_GET_AvailableAudioTracks,
	eAAMP_GET_AvailableTextTracks,
	eAAMP_GET_AudioTrack,
	eAAMP_GET_TextTrack,
	eAAMP_GET_ThumbnailConfig,
	eAAMP_GET_ThumbnailData,
	eAAMP_GET_TYPE_COUNT
}AAMPGetTypes;

/**
 * @enum AAMPSetTypes
 * @brief Define the enum values of set types
 */
typedef enum{
	eAAMP_SET_RateAndSeek,
	eAAMP_SET_VideoRectangle,
	eAAMP_SET_VideoZoom,
	eAAMP_SET_VideoMute,
	eAAMP_SET_AudioVolume,
	eAAMP_SET_Language,
	eAAMP_SET_SubscribedTags,
	eAAMP_SET_LicenseServerUrl,
	eAAMP_SET_AnonymousRequest,
	eAAMP_SET_VodTrickplayFps,
	eAAMP_SET_LinearTrickplayFps,
	eAAMP_SET_LiveOffset,
	eAAMP_SET_StallErrorCode,
	eAAMP_SET_StallTimeout,
	eAAMP_SET_ReportInterval,
	eAAMP_SET_VideoBitarate,
	eAAMP_SET_InitialBitrate,
	eAAMP_SET_InitialBitrate4k,
	eAAMP_SET_NetworkTimeout,
	eAAMP_SET_ManifestTimeout,
	eAAMP_SET_DownloadBufferSize,
	eAAMP_SET_PreferredDrm,
	eAAMP_SET_StereoOnlyPlayback,
	eAAMP_SET_AlternateContent,
	eAAMP_SET_NetworkProxy,
	eAAMP_SET_LicenseReqProxy,
	eAAMP_SET_DownloadStallTimeout,
	eAAMP_SET_DownloadStartTimeout,
	eAAMP_SET_PreferredSubtitleLang,
	eAAMP_SET_ParallelPlaylistDL,
	eAAMP_SET_PreferredLanguages,
	eAAMP_SET_RampDownLimit,
	eAAMP_SET_InitRampdownLimit,
	eAAMP_SET_MinimumBitrate,
	eAAMP_SET_MaximumBitrate,
	eAAMP_SET_MaximumSegmentInjFailCount,
	eAAMP_SET_MaximumDrmDecryptFailCount,
	eAAMP_SET_RegisterForID3MetadataEvents,
	eAAMP_SET_InitialBufferDuration,
	eAAMP_SET_AudioTrack,
	eAAMP_SET_TextTrack,
	eAAMP_SET_CCStatus,
	eAAMP_SET_CCStyle,
	eAAMP_SET_LanguageFormat,
	eAAMP_SET_PropagateUriParam,
	eAAMP_SET_ThumbnailTrack,
	eAAMP_SET_SslVerifyPeer,
	eAAMP_SET_DownloadDelayOnFetch,
	eAAMP_SET_PausedBehavior,
	eAAMP_SET_AuxiliaryAudio,
	eAAMP_SET_RegisterForMediaMetadata,
	eAAMP_SET_TYPE_COUNT
}AAMPSetTypes;

/**
 * @struct VirtualChannelInfo
 * @brief Holds information of a virtual channel
 */
struct VirtualChannelInfo
{
	VirtualChannelInfo() : channelNumber(0), name(), uri()
	{
	}
	int channelNumber;
	std::string name;
	std::string uri;
};

/**
 * @class VirtualChannelMap
 * @brief Holds all of the virtual channels
 */
class VirtualChannelMap
{
public:
	VirtualChannelMap() : mVirtualChannelMap(),mCurrentlyTunedChannel(0),mAutoChannelNumber(0) {}
	~VirtualChannelMap()
	{
		mVirtualChannelMap.clear();
	}
	void Add(VirtualChannelInfo& channelInfo)
	{
		if( !channelInfo.name.empty() )
		{
			if( !channelInfo.uri.empty() )
			{
				if( !VIRTUAL_CHANNEL_VALID(channelInfo.channelNumber) )
				{ // No channel set, use an auto. This could conflict with a later add, we can't check for that here
					channelInfo.channelNumber = mAutoChannelNumber+1;
				}
			
				if (IsPresent(channelInfo) == true)
				{
					return;  // duplicate
				}
				mAutoChannelNumber = channelInfo.channelNumber;
			}
		}
		mVirtualChannelMap.push_back(channelInfo);
	}
	
	VirtualChannelInfo* Find(const int channelNumber)
	{
		VirtualChannelInfo *found = NULL;
		for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
		{
			VirtualChannelInfo &existingChannelInfo = *it;
			if (channelNumber == existingChannelInfo.channelNumber)
			{
				found = &existingChannelInfo;
				break;
			}
		}
		return found;
	}
	
	bool IsPresent(const VirtualChannelInfo& channelInfo)
	{
		for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
		{
			VirtualChannelInfo &existingChannelInfo = *it;
			if(channelInfo.channelNumber == existingChannelInfo.channelNumber)
			{
				printf("[AAMPCLI] duplicate channel number: %d: '%s'\n", channelInfo.channelNumber, channelInfo.uri.c_str() );
				return true;
			}
			if(channelInfo.uri == existingChannelInfo.uri )
			{
				printf("[AAMPCLI] duplicate URL: %d: '%s'\n", channelInfo.channelNumber, channelInfo.uri.c_str() );
				return true;
			}
		}
		return false;
	}
	
	// NOTE: prev() and next() are IDENTICAL other than the direction of the iterator. They could be collapsed using a template,
	// but will all target compilers support this, it wouldn't save much code space, and may make the code harder to understand.
	// Can not simply use different runtime iterators, as the types of each in C++ are not compatible (really).
	VirtualChannelInfo* prev()
	{
		VirtualChannelInfo *pPrevChannel = NULL;
		VirtualChannelInfo *pLastChannel = NULL;
		bool prevFound = false;
		
		// mCurrentlyTunedChannel is 0 for manually entered urls, not having used mVirtualChannelMap yet or empty
		if (mCurrentlyTunedChannel == 0 && mVirtualChannelMap.size() > 0)
		{
			prevFound = true;  // return the last valid channel
		}
		
		for(std::list<VirtualChannelInfo>::reverse_iterator it = mVirtualChannelMap.rbegin(); it != mVirtualChannelMap.rend(); ++it)
		{
			VirtualChannelInfo &existingChannelInfo = *it;
			if (VIRTUAL_CHANNEL_VALID(existingChannelInfo.channelNumber) ) // skip group headings
			{
				if ( pLastChannel == NULL )
				{ // remember this channel for wrap case
					pLastChannel = &existingChannelInfo;
				}
				if ( prevFound )
				{
					pPrevChannel = &existingChannelInfo;
					break;
				}
				else if ( existingChannelInfo.channelNumber == mCurrentlyTunedChannel )
				{
					prevFound = true;
				}
			}
		}
		if (prevFound && pPrevChannel == NULL)
		{
			pPrevChannel = pLastChannel;  // if we end up here we are probably at the first channel -- wrap to back
		}
		return pPrevChannel;
	}
	
	VirtualChannelInfo* next()
	{
		VirtualChannelInfo *pNextChannel = NULL;
		VirtualChannelInfo *pFirstChannel = NULL;
		bool nextFound = false;
		
		// mCurrentlyTunedChannel is 0 for manually entered urls, not using mVirtualChannelMap
		if (mCurrentlyTunedChannel == 0 && mVirtualChannelMap.size() > 0)
		{
			nextFound = true; // return the first valid channel
		}
		
		for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
		{
			VirtualChannelInfo &existingChannelInfo = *it;
			if (VIRTUAL_CHANNEL_VALID(existingChannelInfo.channelNumber) ) // skip group headings
			{
				if ( pFirstChannel == NULL )
				{ // remember this channel for wrap case
					pFirstChannel = &existingChannelInfo;
				}
				if ( nextFound )
				{
					pNextChannel = &existingChannelInfo;
					break;
				}
				else if ( existingChannelInfo.channelNumber == mCurrentlyTunedChannel )
				{
					nextFound = true;
				}
			}
		}
		if (nextFound && pNextChannel == NULL)
		{
			pNextChannel = pFirstChannel;  // if we end up here we are probably at the last channel -- wrap to front
		}
		return pNextChannel;
	}
	 
	void Print()
	{
		if (mVirtualChannelMap.empty())
		{
			  return;
		}
			 
		printf("[AAMPCLI] aampcli.cfg virtual channel map:\n");

		int numCols = 0;
		for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it )
		{
			VirtualChannelInfo &pChannelInfo = *it;
			std::string channelName = pChannelInfo.name.c_str();
			size_t len = channelName.length();
			int maxNameLen = 20;
			if( len>maxNameLen )
			{
				len = maxNameLen;
				channelName = channelName.substr(0,len);
			}
			if( pChannelInfo.uri.empty() )
			{
				if( numCols!=0 )
				{
					printf( "\n" );
				}
				printf( "%s\n", channelName.c_str() );
				numCols = 0;
				continue;
			}
			printf("%4d: %s", pChannelInfo.channelNumber, channelName.c_str() );
			if( numCols>=4 )
			{ // four virtual channels per row
				printf("\n");
				numCols = 0;
			}
			else
			{
				while( len<maxNameLen )
				{ // pad each column to 20 characters, for clean layout
					printf( " " );
					len++;
				}
				numCols++;
			}
		}
		printf("\n\n");
	}
	
	void SetCurrentlyTunedChannel(int value)
	{
		mCurrentlyTunedChannel = value;
	}

	// CSV export
	//	for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it )
	//	{
	//		fprintf( fOut, "%d,%s,%s\n", it->channelNumber, it->name.c_str(), it->uri.c_str() );
	//	}
protected:
	std::list<VirtualChannelInfo> mVirtualChannelMap;
	int mAutoChannelNumber;
	int mCurrentlyTunedChannel;
};

static VirtualChannelMap mVirtualChannelMap;

static std::string mTuneFailureDescription;

/**
 * @brief Thread to run mainloop (for standalone mode)
 * @param[in] arg user_data
 * @retval void pointer
 */
static void* AAMPGstPlayer_StreamThread(void *arg);
static bool initialized = false;
GThread *aampMainLoopThread = NULL;

/**
 * @brief check if the char array is having numbers only
 * @param s
 * @retval true or false
 */
static bool isNumber(const char *s)
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
 * @brief Thread to run mainloop (for standalone mode)
 * @param[in] arg user_data
 * @retval void pointer
 */
static void* AAMPGstPlayer_StreamThread(void *arg)
{
	if (AAMPGstPlayerMainLoop)
	{
		g_main_loop_run(AAMPGstPlayerMainLoop); // blocks
		printf("[AAMPCLI] AAMPGstPlayer_StreamThread: exited main event loop\n");
	}
	g_main_loop_unref(AAMPGstPlayerMainLoop);
	AAMPGstPlayerMainLoop = NULL;
	return NULL;
}

/**
 * @brief To initialize Gstreamer and start mainloop (for standalone mode)
 * @param[in] argc number of arguments
 * @param[in] argv array of arguments
 */
void InitPlayerLoop(int argc, char **argv)
{
	if (!initialized)
	{
		initialized = true;
		gst_init(&argc, &argv);
		AAMPGstPlayerMainLoop = g_main_loop_new(NULL, FALSE);
		aampMainLoopThread = g_thread_new("AAMPGstPlayerLoop", &AAMPGstPlayer_StreamThread, NULL );
	}
}

/**
 * @brief Stop mainloop execution (for standalone mode)
 */
void TermPlayerLoop()
{
	if(AAMPGstPlayerMainLoop)
	{
		g_main_loop_quit(AAMPGstPlayerMainLoop);
		g_thread_join(aampMainLoopThread);
		gst_deinit ();
		printf("[AAMPCLI] %s(): Exit\n", __FUNCTION__);
	}
}

static void showList(void)
{
	printf("******************************************************************************************\n");
	printf("*   Virtual Channel Map \n");
	printf("******************************************************************************************\n");
	mVirtualChannelMap.Print();
}

const char *mGetHelpText[eAAMP_GET_TYPE_COUNT];
const char *mSetHelpText[eAAMP_SET_TYPE_COUNT];

/**
 * @brief Show help menu with aamp command line interface
 */
static void InitGetHelpText()
{
	mGetHelpText[eAAMP_GET_CurrentState] = "Get current player state";
	mGetHelpText[eAAMP_GET_CurrentAudioLan] = "Get Current audio language";
	mGetHelpText[eAAMP_GET_CurrentDrm] = "Get Current DRM";
	mGetHelpText[eAAMP_GET_PlaybackPosition] = "Get Current Playback position";
	mGetHelpText[eAAMP_GET_PlaybackDuration] = "Get Playback Duration";
	mGetHelpText[eAAMP_GET_VideoBitrate] = "Get current video bitrate";
	mGetHelpText[eAAMP_GET_AudioBitrate] = "Get current Audio bitrate";
	mGetHelpText[eAAMP_GET_AudioVolume] = "Get current Audio volume";
	mGetHelpText[eAAMP_GET_PlaybackRate] = "Get Current Playback rate";
	mGetHelpText[eAAMP_GET_VideoBitrates] = "Get Video bitrates supported";
	mGetHelpText[eAAMP_GET_AudioBitrates] = "Get Audio bitrates supported";
	mGetHelpText[eAAMP_GET_CurrentPreferredLanguages] = "Get Current preferred languages";
	mGetHelpText[eAAMP_GET_AvailableAudioTracks] = "Get Available Audio Tracks";
	mGetHelpText[eAAMP_GET_AvailableTextTracks] = "Get Available Text Tracks";
	mGetHelpText[eAAMP_GET_AudioTrack] = "Get Audio Track";
	mGetHelpText[eAAMP_GET_TextTrack] = "Get Text Track";
	mGetHelpText[eAAMP_GET_ThumbnailConfig] = "Get Available ThumbnailTracks";
	mGetHelpText[eAAMP_GET_ThumbnailData] = "Get Thumbnail timerange data(int startpos, int endpos)";

}
/**
 * @brief Show help menu with aamp command line interface
 */
static void InitSetHelpText()
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
	mSetHelpText[eAAMP_SET_PreferredSubtitleLang] =              "<x>             // Set Preferred Subtitle language (string x = lang)";
	mSetHelpText[eAAMP_SET_ParallelPlaylistDL] =                 "<x>             // Set Parallel Playlist download (x=0/1)";
	mSetHelpText[eAAMP_SET_PreferredLanguages] =                 "<x>             // Set Preferred languages (string lang1,lang2,... )";
	mSetHelpText[eAAMP_SET_RampDownLimit] =                      "<x>             // Set number of Ramp Down limit during the playback (x = number)";
	mSetHelpText[eAAMP_SET_InitRampdownLimit] =                  "<x>             // Set number of Initial Ramp Down limit prior to the playback (x = number)";
	mSetHelpText[eAAMP_SET_MinimumBitrate] =                     "<x>             // Set Minimum bitrate (x = bitrate)";
	mSetHelpText[eAAMP_SET_MaximumBitrate] =                     "<x>             // Set Maximum bitrate (x = bitrate)";
	mSetHelpText[eAAMP_SET_MaximumSegmentInjFailCount] =         "<x>             // Set Maximum segment injection fail count (int x = count)";
	mSetHelpText[eAAMP_SET_MaximumDrmDecryptFailCount] =         "<x>             // Set Maximum DRM Decryption fail count(int x = count)";
	mSetHelpText[eAAMP_SET_RegisterForID3MetadataEvents] =       "<x>             // Set Listen for ID3_METADATA events (x = 1 - add listener, x = 0 - remove)";
	mSetHelpText[eAAMP_SET_LanguageFormat] =                     "<y> <y>         // Set Language Format (x = preferredFormat(0-3), y = useRole(0/1))";
	mSetHelpText[eAAMP_SET_InitialBufferDuration] =              "<x>             // Set Initial Buffer Duration (int x = Duration in sec)";
	mSetHelpText[eAAMP_SET_AudioTrack] =                         "<x>             // Set Audio track ( x = track index , track language)";
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
}
/**
 * @brief Show help menu with aamp command line interface
 */
static void ShowHelp(void)
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
	printf("pause                 // Pause the existing playback\n");
	printf("play                  // Continue existing playback\n");
	printf("stop                  // Stop the existing playback\n");
	printf("status                // get the status of existing playback\n");
	printf("flush                 // Flush the existing playback\n");
	printf("sf                    // slow\n");
	printf("ff<x>                 // Trickmodes (x <= 128. y <= 128)\n");
	printf("rw<y>                 // Trickmodes (x <= 128. y <= 128)\n");
	printf("cache <url>/<chan>    // Cache a channel in the background\n");
	printf("toggle                // Toggle background and foreground channels\n");
	printf("stopb                 // Stop background channel.\n");
	printf("bps <x>               // set bitrate (x= bitrate)\n");
	printf("sap                   // Use SAP track (if avail)\n");
	printf("seek <seconds>        // Specify start time within manifest\n");
	printf("live                  // Seek to live point\n");
	printf("underflow             // Simulate underflow\n");
	printf("retune                // schedule retune\n");
	printf("reset                 // delete player instance and create new one\n");
	printf("unlock <cond>         // unlock a channel <long int - time in seconds>\n");
	printf("lock                  // lock the current channel\n");
	printf("next                  // Play next virtual channel\n");
	printf("rollover              // Schedule artificial pts rollover 10s after next tune\n" );
	printf("prev                  // Play previous virtual channel\n");
	printf("exit                  // Exit from application\n");
	printf("help                  // Show this list again\n");
	printf("******************************************************************************************\n");
}

/**
 * @brief Display Help menu for set
 * @param none
 */
void ShowHelpGet(){
	printf("******************************************************************************************\n");
	printf("*   get <command> [<arguments>]\n");
	printf("*   Usage of Commands, and arguments expected\n");
	printf("******************************************************************************************\n");
	for (int iLoop = 0; iLoop < eAAMP_GET_TYPE_COUNT; iLoop++){
		printf("* get %2d // %s \n", iLoop+1, mGetHelpText[iLoop]);
	}
	printf("****************************************************************************\n");
}

/**
 * @brief Display Help menu for set
 * @param none
 */
void ShowHelpSet(){
	printf("******************************************************************************************\n");
	printf("*   set <command> [<arguments>] \n");
	printf("*   Usage of Commands with arguemnts expected in ()\n");
	printf("******************************************************************************************\n");
	for (int iLoop = 0; iLoop < eAAMP_SET_TYPE_COUNT; iLoop++){
		printf("* set %2d %s \n", iLoop+1, mSetHelpText[iLoop]);
	}
	printf("******************************************************************************************\n");
}

static class PlayerInstanceAAMP *mpPlayerInstanceAAMP;

static const char *StringifyPrivAAMPState(PrivAAMPState state)
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
 * @class myAAMPEventListener
 * @brief
 */
class myAAMPEventListener :public AAMPEventObjectListener
{
public:

	/**
	 * @brief Implementation of event callback
	 * @param e Event
	 */
	void Event(const AAMPEventPtr& e)
	{
		switch (e->getType())
		{
		case AAMP_EVENT_STATE_CHANGED:
			{
				StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_STATE_CHANGED: %s (%d)", StringifyPrivAAMPState(ev->getState()), ev->getState());
				break;
			}
		case AAMP_EVENT_SEEKED:
			{
				SeekedEventPtr ev = std::dynamic_pointer_cast<SeekedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_SEEKED: new positionMs %f\n", ev->getPosition());
				break;
			}
		case AAMP_EVENT_MEDIA_METADATA:
			{
				MediaMetadataEventPtr ev = std::dynamic_pointer_cast<MediaMetadataEvent>(e);
				std::vector<std::string> languages = ev->getLanguages();
				int langCount = ev->getLanguagesCount();
				printf("[AAMPCLI] AAMP_EVENT_MEDIA_METADATA\n");
				for (int i = 0; i < langCount; i++)
				{
					printf("[AAMPCLI] language: %s\n", languages[i].c_str());
				}
				printf("[AAMPCLI] AAMP_EVENT_MEDIA_METADATA\n\tDuration=%ld\n\twidth=%d\n\tHeight=%d\n\tHasDRM=%d\n\tProgreamStartTime=%f\n", ev->getDuration(), ev->getWidth(), ev->getHeight(), ev->hasDrm(), ev->getProgramStartTime());
				int bitrateCount = ev->getBitratesCount();
				std::vector<long> bitrates = ev->getBitrates();
				printf("[AAMPCLI] Bitrates:\n");
				for(int i = 0; i < bitrateCount; i++)
				{
					printf("\t[AAMPCLI] bitrate(%d)=%ld\n", i, bitrates.at(i));
				}
				break;
			}
		case AAMP_EVENT_TUNED:
				{
					printf("[AAMPCLI] AAMP_EVENT_TUNED");
					break;
				}
		case AAMP_EVENT_TUNE_FAILED:
				{
					MediaErrorEventPtr ev = std::dynamic_pointer_cast<MediaErrorEvent>(e);
					mTuneFailureDescription = ev->getDescription();
					printf("[AAMPCLI] AAMP_EVENT_TUNE_FAILED reason=%s",mTuneFailureDescription.c_str());
					break;
				}
		case AAMP_EVENT_SPEED_CHANGED:
				{
					SpeedChangedEventPtr ev = std::dynamic_pointer_cast<SpeedChangedEvent>(e);
					printf("[AAMPCLI] AAMP_EVENT_SPEED_CHANGED current rae=%d\n", ev->getRate());
					break;
				}
		case AAMP_EVENT_DRM_METADATA:
				{
					DrmMetaDataEventPtr ev = std::dynamic_pointer_cast<DrmMetaDataEvent>(e);
					printf("[AAMPCLI] AAMP_DRM_FAILED Tune failure:%d\t\naccess status str:%s\t\naccess status val:%d\t\nResponce code:%ld\t\nIs SecClient error:%d\t\n",ev->getFailure(), ev->getAccessStatus().c_str(), ev->getAccessStatusValue(), ev->getResponseCode(), ev->getSecclientError());
					break;
				}
		case AAMP_EVENT_EOS:
				printf("[AAMPCLI] AAMP_EVENT_EOS");
				break;
		case AAMP_EVENT_PLAYLIST_INDEXED:
				printf("[AAMPCLI] AAMP_EVENT_PLAYLIST_INDEXED");
				break;
		case AAMP_EVENT_PROGRESS:
			{
				ProgressEventPtr ev = std::dynamic_pointer_cast<ProgressEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_PROGRESS\n\tDuration=%lf\n\tposition=%lf\n\tstart=%lf\n\tend=%lf\n\tcurrRate=%f\n\tBufferedDuration=%lf\n\tPTS=%lld\n\ttimecode=%s\n",ev->getDuration(),ev->getPosition(),ev->getStart(),ev->getEnd(),ev->getSpeed(),ev->getBufferedDuration(),ev->getPTS(),ev->getSEITimeCode());
				break;
			}
		case AAMP_EVENT_CC_HANDLE_RECEIVED:
			{
				CCHandleEventPtr ev = std::dynamic_pointer_cast<CCHandleEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_CC_HANDLE_RECEIVED CCHandle=%lu",ev->getCCHandle());
				break;
			}
		case AAMP_EVENT_BITRATE_CHANGED:
			{
				BitrateChangeEventPtr ev = std::dynamic_pointer_cast<BitrateChangeEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_BITRATE_CHANGED\n\tbitrate=%ld\n\tdescription=\"%s\"\n\tresolution=%dx%d@%ffps\n\ttime=%d\n\tposition=%lf", ev->getBitrate(), ev->getDescription().c_str(), ev->getWidth(), ev->getHeight(), ev->getFrameRate(), ev->getTime(), ev->getPosition());
				break;
			}
		case AAMP_EVENT_AUDIO_TRACKS_CHANGED:
				printf("[AAMPCLI] AAMP_EVENT_AUDIO_TRACKS_CHANGED");
				break;
		case AAMP_EVENT_ID3_METADATA:
				printf("[AAMPCLI] AAMP_EVENT_ID3_METADATA\n");
				break;
		case AAMP_EVENT_BLOCKED :
			{
				BlockedEventPtr ev = std::dynamic_pointer_cast<BlockedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_BLOCKED Reason:%s\n" ,ev->getReason().c_str());
				break;
			}
		default:
			break;
		}
	}
}; // myAAMPEventListener

static class myAAMPEventListener *myEventListener;

/**
* @brief Decide if input command consists of supported URI scheme to be tuned.
* @param cmd cmd to parse
*/
static bool IsTuneScheme(const char *cmd)
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

inline void StopCachedChannel()
{
	if(mBackgroundPlayer)
	{
		mBackgroundPlayer->Stop();
		delete mBackgroundPlayer;
		mBackgroundPlayer = NULL;
	}
}

void CacheChannel(const char *url)
{
	StopCachedChannel();
	mBackgroundPlayer = new PlayerInstanceAAMP(
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
			NULL
			,updateYUVFrame
#endif
			);
#ifdef LOG_CLI_EVENTS
	mBackgroundPlayer->RegisterEvents(myEventListener);
#endif
	mBackgroundPlayer->Tune(url, false);
}

static void TuneToChannel( VirtualChannelInfo &channel )
{
	mVirtualChannelMap.SetCurrentlyTunedChannel(channel.channelNumber);
	const char *name = channel.name.c_str();
	const char *locator = channel.uri.c_str();
	printf( "TUNING to '%s' %s\n", name, locator );
	if (mSingleton->GetAsyncTuneConfig())
	{
		std::string &uri = channel.uri;
		mSingleton->ScheduleTask(AsyncTaskObj([uri](void *data)
					{
						PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
						logprintf("Calling tune via async tune, uri:%s!!", uri.c_str());
						instance->Tune(uri.c_str());
					}, (void *)mSingleton));
	}
	else
	{
		mSingleton->Tune(channel.uri.c_str());
	}
}

/**
 * @brief trim a string
 * @param[in][out] cmd Buffer containing string
 */
static void trim(char **cmd)
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

extern void tsdemuxer_InduceRollover( bool enable );

/**
 * @brief Process command
 * @param cmd command
 */
static void ProcessCliCommand( char *cmd )
{
	double seconds = 0;
	int rate = 0;
	char lang[MAX_LANGUAGE_TAG_LENGTH];
	char cacheUrl[200];
	int keepPaused = 0;
	long unlockSeconds = 0;
	long grace = 0;
	long time = -1;
	bool eventChange=false;
	unsigned int DownloadDelayInMs = 0;
	trim(&cmd);
	while( *cmd==' ' ) cmd++;
	if (cmd[0] == 0)
	{
		if (mSingleton->aamp->mpStreamAbstractionAAMP)
		{
			mSingleton->aamp->mpStreamAbstractionAAMP->DumpProfiles();
		}
		printf("[AAMPCLI] current bitrate ~= %ld\n", mSingleton->aamp->GetCurrentlyAvailableBandwidth());
	}
	else if (strcmp(cmd, "help") == 0)
	{
		ShowHelp();
	}
	else if( strcmp(cmd,"rollover")==0 )
	{
		printf( "enabling artificial pts rollover (10s after next tune)\n" );
		tsdemuxer_InduceRollover( true );
	}
	else if (strcmp(cmd, "list") == 0)
	{
		showList();
	}
	else if (IsTuneScheme(cmd))
	{
		if (mSingleton->GetAsyncTuneConfig())
		{
			std::string uri = cmd;
			mSingleton->ScheduleTask(AsyncTaskObj([uri](void *data)
						{
							PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
							logprintf("Calling tune via async tune, uri:%s!!", uri.c_str());
							instance->Tune(uri.c_str());
						}, (void *)mSingleton));
		}
		else
		{
			mSingleton->Tune(cmd);
		}
	}
	else if( memcmp(cmd, "next", 4) == 0 )
	{
		VirtualChannelInfo *pNextChannel = mVirtualChannelMap.next();
		if (pNextChannel)
		{
			printf("[AAMPCLI] next %d: %s\n", pNextChannel->channelNumber, pNextChannel->name.c_str());
			TuneToChannel( *pNextChannel );
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
			TuneToChannel( *pPrevChannel );
		}
		else
		{
			printf("[AAMPCLI] can not fetch 'prev' channel, empty virtual channel map\n");
		}
	}
	else if (isNumber(cmd))
	{
		int channelNumber = atoi(cmd);  // invalid input results in 0 -- will not be found
		
		VirtualChannelInfo *pChannelInfo = mVirtualChannelMap.Find(channelNumber);
		if (pChannelInfo != NULL)
		{
			printf("[AAMPCLI] channel number: %d\n", channelNumber);
			TuneToChannel( *pChannelInfo );
		}
		else
		{
			printf("[AAMPCLI] channel number: %d was not found\n", channelNumber);
		}
	}
	else if (sscanf(cmd, "cache %s", cacheUrl) == 1)
	{
		if (memcmp(cacheUrl, "http", 4) ==0)
		{
			CacheChannel(cacheUrl);
		}
		else
		{
			int channelNumber = atoi(cacheUrl);
			
			VirtualChannelInfo *pChannelInfo = mVirtualChannelMap.Find(channelNumber);
			if (pChannelInfo != NULL)
			{
				printf("[AAMPCLI] channel number: %d\n", channelNumber);
				CacheChannel(pChannelInfo->uri.c_str());
			}
			else
			{
				printf("[AAMPCLI] channel number: %d was not found\n", channelNumber);
			}
		}
	}
	else if (strcmp(cmd, "toggle") == 0)
	{
		if(mBackgroundPlayer && mSingleton)
		{
			mSingleton->detach();
			mBackgroundPlayer->SetRate(AAMP_NORMAL_PLAY_RATE);

			PlayerInstanceAAMP *tmpPlayer = mSingleton;
			mSingleton = mBackgroundPlayer;
			mBackgroundPlayer = tmpPlayer;
			StopCachedChannel();
		}
	}
	else if (strcmp(cmd, "stopb") == 0)
	{
		StopCachedChannel();
	}
	else if (sscanf(cmd, "seek %lf %d", &seconds, &keepPaused) >= 1)
	{
		bool seekWhilePaused = (keepPaused==1);
		if (mSingleton->GetAsyncTuneConfig())
		{
			mSingleton->ScheduleTask(AsyncTaskObj([seconds, seekWhilePaused](void *data)
							{
								PlayerInstanceAAMP *instance = static_cast<PlayerInstanceAAMP *>(data);
								logprintf("Calling seek via async tune, seconds:%lf!!", seconds);
								instance->Seek(seconds, seekWhilePaused);
							}, (void *)mSingleton));
		}
		else
		{
			mSingleton->Seek(seconds, (keepPaused==1) );
		}
		keepPaused = 0;
	}
	else if (strcmp(cmd, "sf") == 0)
	{
		mSingleton->SetRate((int)0.5);
	}
	else if (sscanf(cmd, "ff%d", &rate) == 1)
	{
		mSingleton->SetRate((float)rate);
	}
	else if (strcmp(cmd, "play") == 0)
	{
		mSingleton->SetRate(1);
	}
	else if (strcmp(cmd, "pause") == 0)
	{
		mSingleton->SetRate(0);
	}
	else if (sscanf(cmd, "rw%d", &rate) == 1)
	{
		mSingleton->SetRate((float)(-rate));
	}
	else if (sscanf(cmd, "bps %d", &rate) == 1)
	{
		printf("[AAMPCLI] Set video bitrate %d.\n", rate);
		mSingleton->SetVideoBitrate(rate);
	}
	else if (strcmp(cmd, "flush") == 0)
	{
		mSingleton->aamp->mStreamSink->Flush();
	}
	else if (strcmp(cmd, "stop") == 0)
	{
		mSingleton->Stop();
		tsdemuxer_InduceRollover(false);
	}
	else if (strcmp(cmd, "underflow") == 0)
	{
		mSingleton->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, eMEDIATYPE_VIDEO);
	}
	else if (strcmp(cmd, "retune") == 0)
	{
		mSingleton->aamp->ScheduleRetune(eDASH_ERROR_STARTTIME_RESET, eMEDIATYPE_VIDEO);
	}
	else if (strcmp(cmd, "status") == 0)
	{
		mSingleton->aamp->mStreamSink->DumpStatus();
	}
	else if (strcmp(cmd, "live") == 0)
	{
		mSingleton->SeekToLive();
	}
	else if (strcmp(cmd, "exit") == 0)
	{
		mSingleton->Stop();
		delete mSingleton;
		mSingleton = NULL;
		if (mBackgroundPlayer)
		{
			delete mBackgroundPlayer;
			mBackgroundPlayer = NULL;
		}
		TermPlayerLoop();
		exit(0);
	}
	else if (memcmp(cmd, "rect", 4) == 0)
	{
		int x, y, w, h;
		if (sscanf(cmd, "rect %d %d %d %d", &x, &y, &w, &h) == 4)
		{
			mSingleton->SetVideoRectangle(x, y, w, h);
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
				mSingleton->SetVideoZoom(VIDEO_ZOOM_FULL);
			}
			else
			{
				printf("[AAMPCLI] Set zoom to none\n");
				mSingleton->SetVideoZoom(VIDEO_ZOOM_NONE);
			}
		}
	}
	else if( sscanf(cmd, "sap %s",lang ) )
	{
		size_t len = strlen(lang);
		printf("[AAMPCLI] aamp cli sap called for language %s\n",lang);
		if( len>0 )
		{
			mSingleton->SetLanguage( lang );
		}
		else
		{
			printf("[AAMPCLI] GetCurrentAudioLanguage: '%s'\n", mSingleton->GetCurrentAudioLanguage() );
		}
	}
	else if( strcmp(cmd,"getplaybackrate") == 0 )
	{
		printf("[AAMPCLI] Playback Rate: %d\n", mSingleton->GetPlaybackRate());
	}
	else if (memcmp(cmd, "islive", 6) == 0 )
	{
		printf("[AAMPCLI] VIDEO IS %s \n",
		(mSingleton->IsLive() == true )? "LIVE": "NOT LIVE");
	}
	else if (memcmp(cmd, "customheader", 12) == 0 )
	{
		//Dummy implimenations
		std::vector<std::string> headerValue;
		printf("[AAMPCLI] customheader Command is %s \n" , cmd);
		mSingleton->AddCustomHTTPHeader("", headerValue, false);
	}
	else if (memcmp(cmd, "reset", 5) == 0 )
	{
		printf("[AAMPCLI] Reset mSingleton instance %p \n", mSingleton);
		mSingleton->Stop();

		delete mSingleton;
		mSingleton = NULL;

		mSingleton = new PlayerInstanceAAMP(
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
				NULL
				,updateYUVFrame
#endif
				);

		myEventListener = new myAAMPEventListener();
		mSingleton->RegisterEvents(myEventListener);
		printf("[AAMPCLI] New mSingleton instance %p \n", mSingleton);

	}
	else if (sscanf(cmd, "unlock %ld", &unlockSeconds) >= 1)
	{
		printf("[AAMPCLI] unlocking for %ld seconds\n" , unlockSeconds);
		if(-1 == unlockSeconds)
			grace = -1;
		else
			time = unlockSeconds;
		mSingleton->DisableContentRestrictions(grace, time, eventChange);
	}
	else if (memcmp(cmd, "unlock", 6) == 0 )
	{
		printf("[AAMPCLI] unlocking till next program change\n");
		eventChange = true;
		mSingleton->DisableContentRestrictions(grace, time, eventChange);
	}
	else if (memcmp(cmd, "lock", 4) == 0 )
	{
		mSingleton->EnableContentRestrictions();
	}
	else if (memcmp(cmd, "set", 3) == 0 )
	{
		char help[8];
		int opt;
		if (sscanf(cmd, "set %d", &opt) == 1){
			switch(opt-1){ // 1 based to zero based
				case eAAMP_SET_RateAndSeek:
				{
					int rate;
					double ralatineTuneTime;
					printf("[AAMPCLI] Matched Command eAAMP_SET_RateAndSeek - %s \n", cmd);
					if (sscanf(cmd, "set %d %d %lf", &opt, &rate, &ralatineTuneTime ) == 3){
						mSingleton->SetRateAndSeek(rate, ralatineTuneTime);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoRectangle - %s \n", cmd);
					if (sscanf(cmd, "set %d %d %d %d %d", &opt, &x, &y, &w, &h) == 5){
						mSingleton->SetVideoRectangle(x,y,w,h);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoZoom - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &videoZoom) == 2){
						mSingleton->SetVideoZoom((videoZoom > 0 )? VIDEO_ZOOM_FULL : VIDEO_ZOOM_NONE );
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoMute - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &videoMute) == 2){
						mSingleton->SetVideoMute((videoMute == 1 )? true : false );
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_AudioVolume - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &vol) == 2){
						mSingleton->SetAudioVolume(vol);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_Language - %s \n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, lang) == 2){
						mSingleton->SetLanguage(lang);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_SubscribedTags - %s \n", cmd);
					mSingleton->SetSubscribedTags(subscribedTags);
					break;
				}
				
				case eAAMP_SET_LicenseServerUrl:
				{
					char lisenceUrl[1024];
					int drmType;
					printf("[AAMPCLI] Matched Command eAAMP_SET_LicenseServerUrl - %s \n", cmd);
					if (sscanf(cmd, "set %d %s %d", &opt, lisenceUrl, &drmType) == 3){
						mSingleton->SetLicenseServerURL(lisenceUrl,
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_AnonymousRequest - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &isAnonym) == 2){
						mSingleton->SetAnonymousRequest((isAnonym == 1)?true:false);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_VodTrickplayFps - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &vodTFps) == 2){
						mSingleton->SetVODTrickplayFPS(vodTFps);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_LinearTrickplayFps - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &linearTFps) == 2){
						mSingleton->SetLinearTrickplayFPS(linearTFps);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_LiveOffset - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &liveOffset) == 2){
						mSingleton->SetLiveOffset(liveOffset);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_StallErrorCode - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &stallErrorCode) == 2){
						mSingleton->SetStallErrorCode(stallErrorCode);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_StallTimeout - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &stallTimeout) == 2){
						mSingleton->SetStallTimeout(stallTimeout);
					}
					break;
				}

				case eAAMP_SET_ReportInterval:
				{
					int reportInterval;
					printf("[AAMPCLI] Matched Command eAAMP_SET_ReportInterval - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &reportInterval) == 2){
						mSingleton->SetReportInterval(reportInterval);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_VideoBitarate - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &videoBitarate) == 2){
						mSingleton->SetVideoBitrate(videoBitarate);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitialBitrate - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &initialBitrate) == 2){
						mSingleton->SetInitialBitrate(initialBitrate);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitialBitrate4k - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &initialBitrate4k) == 2){
						mSingleton->SetInitialBitrate4K(initialBitrate4k);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_NetworkTimeout - %s \n", cmd);
					if (sscanf(cmd, "set %d %lf", &opt, &networkTimeout) == 2){
						mSingleton->SetNetworkTimeout(networkTimeout);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_ManifestTimeout - %s \n", cmd);
					if (sscanf(cmd, "set %d %lf", &opt, &manifestTimeout) == 2){
						mSingleton->SetManifestTimeout(manifestTimeout);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadBufferSize - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &downloadBufferSize) == 2){
						mSingleton->SetDownloadBufferSize(downloadBufferSize);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_PreferredDrm - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &preferredDrm) == 2){
						mSingleton->SetPreferredDRM((DRMSystems)preferredDrm);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_StereoOnlyPlayback - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &stereoOnlyPlayback) == 2){
						mSingleton->SetStereoOnlyPlayback(
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_AlternateContent - %s \n", cmd);
					mSingleton->SetAlternateContents(adBrkId, adId, url);
					break;
				}

				case eAAMP_SET_NetworkProxy:
				{
					char networkProxy[128];
					printf("[AAMPCLI] Matched Command eAAMP_SET_NetworkProxy - %s \n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, networkProxy) == 2){
						mSingleton->SetNetworkProxy(networkProxy);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_LicenseReqProxy - %s \n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, licenseReqProxy) == 2){
						mSingleton->SetLicenseReqProxy(licenseReqProxy);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadStallTimeout - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &downloadStallTimeout) == 2){
						mSingleton->SetDownloadStallTimeout(downloadStallTimeout);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_DownloadStartTimeout - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &downloadStartTimeout) == 2){
						mSingleton->SetDownloadStartTimeout(downloadStartTimeout);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_PreferredSubtitleLang - %s \n", cmd);
					if (sscanf(cmd, "set %d %s", &opt, preferredSubtitleLang) == 2){
						mSingleton->SetPreferredSubtitleLanguage(preferredSubtitleLang);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_ParallelPlaylistDL - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &parallelPlaylistDL) == 2){
						mSingleton->SetParallelPlaylistDL( (parallelPlaylistDL == 1)? true:false );
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
                                        char rendition[32];
                                        memset(preferredLanguages, 0, sizeof(preferredLanguages));
                                        memset(rendition, 0, sizeof(rendition));
                                        printf("[AAMPCLI] Matched Command eAAMP_SET_PreferredLanguages - %s \n", cmd);
                                        
                                        if (sscanf(cmd, "set %d %s %s", &opt, preferredLanguages, rendition) == 3){
                                                printf("[AAMPCLI] setting PreferredLanguages (%s) with rendition (%s) \n" ,
                                                                preferredLanguages, rendition);  
                                                mSingleton->SetPreferredLanguages(preferredLanguages, rendition);
                                        }
                                        else if (sscanf(cmd, "set %d %s", &opt, preferredLanguages) == 2)
                                        {
                                                printf("[AAMPCLI] setting PreferredLanguages (%s) \n",preferredLanguages );  
                                                mSingleton->SetPreferredLanguages(preferredLanguages);
                                        }
                                        else
                                        {
						printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
                                                printf("[AAMPCLI] set preferred languages must be run with atleast 1 argument\n");
                                        }
                                        break;
				}
				
				case eAAMP_SET_InitRampdownLimit:
				{
					int rampDownLimit;
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitRampdownLimit - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &rampDownLimit) == 2){
						mSingleton->SetInitRampdownLimit(rampDownLimit);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_RampDownLimit - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &rampDownLimit) == 2){
						mSingleton->SetRampDownLimit(rampDownLimit);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_MinimumBitrate - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &minBitrate) == 2){
						mSingleton->SetMinimumBitrate(minBitrate);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_MaximumBitrate - %s \n", cmd);
					if (sscanf(cmd, "set %d %ld", &opt, &maxBitrate) == 2){
						mSingleton->SetMaximumBitrate(maxBitrate);
					}
					else
                                        {
                                                printf("[AAMPCLI] ERROR: Mismatch in arguments\n");
                                                printf("[AAMPCLI] Expected: set %d <value>\n", opt);
                                        }
					break;
				}

				case eAAMP_SET_MaximumSegmentInjFailCount:
				{
					int failCount;
					printf("[AAMPCLI] Matched Command eAAMP_SET_MaximumSegmentInjFailCount - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &failCount) == 2){
						mSingleton->SetSegmentInjectFailCount(failCount);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_SslVerifyPeer - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &value) == 2){
						mSingleton->SetSslVerifyPeerConfig(value == 1);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_MaximumDrmDecryptFailCount - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &failCount) == 2){
						mSingleton->SetSegmentDecryptFailCount(failCount);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_RegisterForID3MetadataEvents - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &id3MetadataEventsEnabled) == 2){
						if (id3MetadataEventsEnabled)
						{
							mSingleton->AddEventListener(AAMP_EVENT_ID3_METADATA, myEventListener);
						}
						else
						{
							mSingleton->RemoveEventListener(AAMP_EVENT_ID3_METADATA, myEventListener);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_RegisterForMediaMetadata - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &mediaMetadataEventsEnabled) == 2){
						if (mediaMetadataEventsEnabled)
						{
							mSingleton->AddEventListener(AAMP_EVENT_MEDIA_METADATA, myEventListener);
						}
						else
						{
							mSingleton->RemoveEventListener(AAMP_EVENT_MEDIA_METADATA, myEventListener);
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
						mSingleton->SetLanguageFormat(preference, bDescriptiveAudioTrack!=0 );
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_InitialBufferDuration - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &duration) == 2)
					{
						mSingleton->SetInitialBufferDuration(duration);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_AudioTrack - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &track) == 2)
					{
						mSingleton->SetAudioTrack(track);
					}
					else if (sscanf(cmd, "set %d %s", &opt, strTrackInfo) == 2)
					{
						std::string substr = "";
						std::string language = "";
						std::string rendition = "";
						std::string codec = "";
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

						/*codec TODO:not supported now */
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
						printf("[AAMPCLI] Selecting audio track based on language  - %s rendition - %s codec = %s channel = %d\n", 
						language.c_str(), rendition.c_str(), codec.c_str(), channel);
						mSingleton->SetAudioTrack(language, rendition, codec, channel);

					}
					break;
				}
				
				case eAAMP_SET_TextTrack:
				{
					int track;
					printf("[AAMPCLI] Matched Command eAAMP_SET_TextTrack - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &track) == 2)
					{
						mSingleton->SetTextTrack(track);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_CCStatus - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &status) == 2)
					{
						mSingleton->SetCCStatus(status == 1);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_CCStyle - %s \n", cmd);
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
							mSingleton->SetTextStyle(options);
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
					printf("[AAMPCLI] Matched Command eAAMP_SET_PropogateUriParam - %s \n", cmd);
					if (sscanf(cmd, "set %d %d", &opt, &propagateUriParam) == 2){
						mSingleton->SetPropagateUriParameters((bool) propagateUriParam);
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
					printf("[AAMPCLI] Setting ThumbnailTrack : %s\n",mSingleton->SetThumbnailTrack(rate)?"Success":"Failure");
					break;
				}
				case eAAMP_SET_DownloadDelayOnFetch:
				{
						logprintf("Matched Command eAAMP_SET_DownloadDelayOnFetch - %s",cmd);
						sscanf(cmd, "set %d %d", &opt, &DownloadDelayInMs);
						mSingleton->ApplyArtificialDownloadDelay(DownloadDelayInMs);
						break;
				}


				case eAAMP_SET_PausedBehavior:
				{
					char behavior[24];
					printf("[AAMPCLI] Matched Command eAAMP_SET_PausedBehavior - %s\n", cmd);
					if(sscanf(cmd, "set %d %d", &opt, &rate) == 2)
					{
						mSingleton->SetPausedBehavior(rate);
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
					logprintf("Matchde Command eAAMP_SET_AuxiliaryAudio - %s ", cmd);
					if (sscanf(cmd, "set %d %s", &opt, lang) == 2)
					{
						mSingleton->SetAuxiliaryLanguage(lang);
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
			if(0 == strncmp("help", help, 4))
			{
				ShowHelpSet();
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
	else if (memcmp(cmd, "get", 3) == 0 )
	{
		char help[8];
		int opt, value1, value2;
		if (sscanf(cmd, "get %d", &opt) == 1){
			switch(opt-1){ // 1 based to zero based
				case eAAMP_GET_ThumbnailConfig:
					printf("[AAMPCLI] GETTING AVAILABLE THUMBNAIL TRACKS: %s\n", mSingleton->GetAvailableThumbnailTracks().c_str() );
					break;
				case eAAMP_GET_CurrentState:
					printf("[AAMPCLI] GETTING CURRENT STATE: %d\n", (int) mSingleton->GetState());
					break;
				case eAAMP_GET_ThumbnailData:
					sscanf(cmd, "get %d %d %d",&opt, &value1, &value2);
					printf("[AAMPCLI] GETTING THUMBNAIL TIME RANGE DATA for duration(%d,%d): %s\n",value1,value2,mSingleton->GetThumbnails(value1, value2).c_str());
					break;
				case eAAMP_GET_AudioTrack:
					printf("[AAMPCLI] CURRENT AUDIO TRACK: %d\n", mSingleton->GetAudioTrack() );
					break;
				case eAAMP_GET_TextTrack:
					printf("[AAMPCLI] CURRENT TEXT TRACK: %d\n", mSingleton->GetTextTrack() );
					break;
				case eAAMP_GET_AvailableAudioTracks:
					printf("[AAMPCLI] AVAILABLE AUDIO TRACKS: %s\n", mSingleton->GetAvailableAudioTracks().c_str() );
					break;
				case eAAMP_GET_AvailableTextTracks:
					printf("[AAMPCLI] AVAILABLE TEXT TRACKS: %s\n", mSingleton->GetAvailableTextTracks().c_str() );
					break;

				case eAAMP_GET_CurrentAudioLan:
					printf("[AAMPCLI] CURRRENT AUDIO LANGUAGE = %s \n",
					mSingleton->GetCurrentAudioLanguage());
					break;

				case eAAMP_GET_CurrentDrm:
					printf("[AAMPCLI] CURRRENT DRM  = %s \n",
					mSingleton->GetCurrentDRM());
					break;

				case eAAMP_GET_PlaybackPosition:
					printf("[AAMPCLI] PLAYBACK POSITION = %lf \n",
					mSingleton->GetPlaybackPosition());
					break;

				case eAAMP_GET_PlaybackDuration:
					printf("[AAMPCLI] PLAYBACK DURATION = %lf \n",
					mSingleton->GetPlaybackDuration());
					break;

				case eAAMP_GET_VideoBitrate:
					printf("[AAMPCLI] VIDEO BITRATE = %ld \n",
					mSingleton->GetVideoBitrate());
					break;

				case eAAMP_GET_AudioBitrate:
					printf("[AAMPCLI] AUDIO BITRATE = %ld \n",
					mSingleton->GetAudioBitrate());
					break;

				case eAAMP_GET_AudioVolume:
					printf("[AAMPCLI] AUDIO VOLUME = %d \n",
					mSingleton->GetAudioVolume());
					break;
				
				case eAAMP_GET_PlaybackRate:
					printf("[AAMPCLI] PLAYBACK RATE = %d \n",
					mSingleton->GetPlaybackRate());
					break;

				case eAAMP_GET_VideoBitrates:
				{
					std::vector<long int> videoBitrates;
					printf("[AAMPCLI] VIDEO BITRATES = [ ");
					videoBitrates = mSingleton->GetVideoBitrates();
					for(int i=0; i < videoBitrates.size(); i++){
						printf("%ld, ", videoBitrates[i]);
					}
					printf(" ] \n");
					break;
				}

				case eAAMP_GET_AudioBitrates:
				{
					std::vector<long int> audioBitrates;
					printf("[AAMPCLI] AUDIO BITRATES = [ ");
					audioBitrates = mSingleton->GetAudioBitrates();
					for(int i=0; i < audioBitrates.size(); i++){
						printf("%ld, ", audioBitrates[i]);
					}
					printf(" ] \n");
					break;
				}
				case eAAMP_GET_CurrentPreferredLanguages:
				{
					const char *prefferedLanguages = mSingleton->GetPreferredLanguages();
					printf("[AAMPCLI] PREFERRED LANGUAGES = \"%s\" \n", prefferedLanguages? prefferedLanguages : "<NULL>");
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
				ShowHelpGet();
			}else
			{
				printf("[AAMPCLI] Invalid usage of get operations %s\n", help);
			}
		}
		else
		{
			printf("[AAMPCLI] Invalid get command = %s\n", cmd);
		}
	}
}

static void DoAutomation(const int startChannel=500, const int stopChannel=1000)
{
#ifdef __APPLE__
	std::string outPath(getenv("HOME"));
#else
	std::string outPath("/opt");
#endif
	outPath += "/test-results.csv";
	const char *mod = "wb"; // initially clear file
	
	if (mVirtualChannelMap.next() == NULL)
	{
		printf("[AAMPCLI] Can not auto channels, empty virtual channel map.\n");
		return;
	}
	
	for( int chan=startChannel; chan<=stopChannel; chan++ )
	{
		VirtualChannelInfo *info = mVirtualChannelMap.Find(chan);
		if( info )
		{
			if( strstr(info->name.c_str(),"ClearKey") ||
				strstr(info->name.c_str(),"MultiDRM") ||
				strstr(info->name.c_str(),"DTS Audio") ||
				strstr(info->name.c_str(),"AC-4") )
			{
#ifdef __APPLE__
				continue; // skip unsupported DRM AND Audio formats
#endif
			}
			printf( "%d,\"%s\",%s,%s\n",
				info->channelNumber, info->name.c_str(), info->uri.c_str(), "TUNING...");

			char cmd[32];
			sprintf( cmd, "%d", chan );
			mTuneFailureDescription.clear();
			ProcessCliCommand( cmd );
			PrivAAMPState state = eSTATE_IDLE;
			for(int i=0; i<5; i++ )
			{
				sleep(1);
				state = mSingleton->GetState();
				if( state == eSTATE_PLAYING || state == eSTATE_ERROR )
				{
					break;
				}
			}
			const char *stateName;
			switch( state )
			{
				case eSTATE_PLAYING:
					sleep(4); // let play for a bit longer, as visibility sanity check
					stateName = "OK";
					break;
				case eSTATE_ERROR:
					stateName = "FAIL";
					break;
				default:
					stateName = "TIMEOUT";
					break;
			}
			printf( "***%s\n", stateName );
			FILE *f = fopen( outPath.c_str(), mod );
			assert( f );
			fprintf( f, "%d,\"%s\",%s,%s,%s\n",
				info->channelNumber, info->name.c_str(), info->uri.c_str(), stateName, mTuneFailureDescription.c_str() );
			mod = "a";
			fclose( f );
		}
	}
}

static void * run_command( void* startUrl )
{
	char cmd[MAX_BUFFER_LENGTH];

	if( startUrl )
	{
		strcpy( cmd, (const char *)startUrl );
		ProcessCliCommand( cmd );
	}
	
	InitGetHelpText();
	InitSetHelpText();
	
	for(;;)
	{
		printf("[AAMPCLI] type 'help' for list of available commands\n");
		printf("[AAMPCLI] aamp-cli> ");
		char *ret = fgets(cmd, sizeof(cmd), stdin);
		if( ret==NULL)
		{
			break;
		}
		if( memcmp(cmd,"auto",4)==0 )
		{
			int start=0, end=0;
			int matched = sscanf(cmd, "auto %d %d", &start, &end);
			switch (matched)
			{
				case 1:
					DoAutomation(start);
					break;
				case 2:
					DoAutomation(start, end);
					break;
				default:
					DoAutomation();
					break;
			}
		}
		else
		{
			ProcessCliCommand(cmd);
		}
	}
	
	return NULL;
}

#ifdef RENDER_FRAMES_IN_APP_CONTEXT
#define ATTRIB_VERTEX 0
#define ATTRIB_TEXTURE 1

typedef struct{
	int width = 0;
	int height = 0;
	uint8_t *yuvBuffer = NULL;
}AppsinkData;

static AppsinkData appsinkData;

GLuint mProgramID;
GLuint id_y, id_u, id_v; // texture id
GLuint textureUniformY, textureUniformU,textureUniformV;
static GLuint _vertexArray;
static GLuint _vertexBuffer[2];
static const int FPS = 60;
GLfloat currentAngleOfRotation = 0.0;

static const char *VSHADER =
	"attribute vec2 vertexIn;"
	"attribute vec2 textureIn;"
	"varying vec2 textureOut;"
	"uniform mat4 trans;"
	"void main() {"
		"gl_Position = trans * vec4(vertexIn,0, 1);"
		"textureOut = textureIn;"
	"}";

static const char *FSHADER =
	"#ifdef GL_ES \n"
		"  precision mediump float; \n"
	"#endif \n"
	"varying vec2 textureOut;"
	"uniform sampler2D tex_y;"
	"uniform sampler2D tex_u;"
	"uniform sampler2D tex_v;"
	"void main() {"
		"vec3 yuv;"
		"vec3 rgb;"
		"yuv.x = texture2D(tex_y, textureOut).r;"
		"yuv.y = texture2D(tex_u, textureOut).r - 0.5;"
		"yuv.z = texture2D(tex_v, textureOut).r - 0.5;"
		"rgb = mat3( 1, 1, 1, 0, -0.39465, 2.03211, 1.13983, -0.58060,  0) * yuv;"
		"gl_FragColor = vec4(rgb, 1);"
	"}";

static GLuint LoadShader( GLenum type )
{
	GLuint shaderHandle = 0;
	const char *sources[1];

	if(GL_VERTEX_SHADER == type)
	{
		sources[0] = VSHADER;
	}
	else
	{
		sources[0] = FSHADER;
	}

	if( sources[0] )
	{
		shaderHandle = glCreateShader(type);
		glShaderSource(shaderHandle, 1, sources, 0);
		glCompileShader(shaderHandle);
		GLint compileSuccess;
		glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
		if (compileSuccess == GL_FALSE)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shaderHandle, sizeof(msg), 0, &msg[0]);
			printf("[AAMPCLI] %s\n", msg );
		}
	}

	return shaderHandle;
}

void InitShaders()
{
	GLint linked;

	GLint vShader = LoadShader(GL_VERTEX_SHADER);
	GLint fShader = LoadShader(GL_FRAGMENT_SHADER);
	mProgramID = glCreateProgram();
	glAttachShader(mProgramID,vShader);
	glAttachShader(mProgramID,fShader);

	glBindAttribLocation(mProgramID, ATTRIB_VERTEX, "vertexIn");
	glBindAttribLocation(mProgramID, ATTRIB_TEXTURE, "textureIn");
	glLinkProgram(mProgramID);
	glValidateProgram(mProgramID);

	glGetProgramiv(mProgramID, GL_LINK_STATUS, &linked);
	if( linked == GL_FALSE )
	{
		GLint logLen;
		glGetProgramiv(mProgramID, GL_INFO_LOG_LENGTH, &logLen);
		GLchar *msg = (GLchar *)malloc(sizeof(GLchar)*logLen);
		glGetProgramInfoLog(mProgramID, logLen, &logLen, msg );
		printf( "%s\n", msg );
		free( msg );
	}
	glUseProgram(mProgramID);
	glDeleteShader(vShader);
	glDeleteShader(fShader);

	//Get Uniform Variables Location
	textureUniformY = glGetUniformLocation(mProgramID, "tex_y");
	textureUniformU = glGetUniformLocation(mProgramID, "tex_u");
	textureUniformV = glGetUniformLocation(mProgramID, "tex_v");

	typedef struct _vertex
	{
		float p[2];
		float uv[2];
	} Vertex;

	static const Vertex vertexPtr[4] =
	{
		{{-1,-1}, {0.0,1 } },
		{{ 1,-1}, {1,1 } },
		{{ 1, 1}, {1,0.0 } },
		{{-1, 1}, {0.0,0.0} }
	};
	static const unsigned short index[6] =
	{
		0,1,2, 2,3,0
	};

	glGenVertexArrays(1, &_vertexArray);
	glBindVertexArray(_vertexArray);
	glGenBuffers(2, _vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPtr), vertexPtr, GL_STATIC_DRAW );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vertexBuffer[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW );
	glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE,
							sizeof(Vertex), (const GLvoid *)offsetof(Vertex,p) );
	glEnableVertexAttribArray(ATTRIB_VERTEX);

	glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, GL_FALSE,
						  sizeof(Vertex), (const GLvoid *)offsetof(Vertex, uv ) );
	glEnableVertexAttribArray(ATTRIB_TEXTURE);
	glBindVertexArray(0);

	glGenTextures(1, &id_y);
	glBindTexture(GL_TEXTURE_2D, id_y);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &id_u);
	glBindTexture(GL_TEXTURE_2D, id_u);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &id_v);
	glBindTexture(GL_TEXTURE_2D, id_v);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void glRender(void){
	/** Input in I420 (YUV420) format.
	  * Buffer structure:
	  * ----------
	  * |        |
	  * |   Y    | size = w*h
	  * |        |
	  * |________|
	  * |   U    |size = w*h/4
	  * |--------|
	  * |   V    |size = w*h/4
	  * ----------*
	  */
	int pixel_w = 0;
	int pixel_h = 0;
	uint8_t *yuvBuffer = NULL;
	unsigned char *yPlane, *uPlane, *vPlane;

	{
		std::lock_guard<std::mutex> lock(appsinkData_mutex);
		yuvBuffer = appsinkData.yuvBuffer;
		appsinkData.yuvBuffer = NULL;
		pixel_w = appsinkData.width;
		pixel_h = appsinkData.height;
	}
	if(yuvBuffer)
	{
		yPlane = yuvBuffer;
		uPlane = yPlane + (pixel_w*pixel_h);
		vPlane = uPlane + (pixel_w*pixel_h)/4;

		glClearColor(0.0,0.0,0.0,0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		//Y
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, id_y);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w, pixel_h, 0, GL_RED, GL_UNSIGNED_BYTE, yPlane);
		glUniform1i(textureUniformY, 0);

		//U
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, id_u);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w/2, pixel_h/2, 0, GL_RED, GL_UNSIGNED_BYTE, uPlane);
		glUniform1i(textureUniformU, 1);

		//V
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, id_v);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w/2, pixel_h/2, 0, GL_RED, GL_UNSIGNED_BYTE, vPlane);
		glUniform1i(textureUniformV, 2);

		//Rotate
		glm::mat4 trans = glm::rotate(
			glm::mat4(1.0f),
			currentAngleOfRotation * 360,
			glm::vec3(1.0f, 1.0f, 1.0f)
		);
		GLint uniTrans = glGetUniformLocation(mProgramID, "trans");
		glUniformMatrix4fv(uniTrans, 1, GL_FALSE, glm::value_ptr(trans));

		glBindVertexArray(_vertexArray);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );
		glBindVertexArray(0);

		glutSwapBuffers();
		delete yuvBuffer;
	}
}

void updateYUVFrame(uint8_t *buffer, int size, int width, int height)
{
	uint8_t* frameBuf = new uint8_t[size];
	memcpy(frameBuf, buffer, size);

	{
		std::lock_guard<std::mutex> lock(appsinkData_mutex);
		if(appsinkData.yuvBuffer)
		{
			printf("[AAMPCLI] Drops frame.\n");
			delete appsinkData.yuvBuffer;
		}
		appsinkData.yuvBuffer = frameBuf;
		appsinkData.width = width;
		appsinkData.height = height;
	}
}

void timer(int v) {
	currentAngleOfRotation += 0.0001;
	if (currentAngleOfRotation >= 1.0)
	{
		currentAngleOfRotation = 0.0;
	}
	glutPostRedisplay();

	glutTimerFunc(1000/FPS, timer, v);
}
#endif

static std::string GetNextFieldFromCSV( const char **pptr )
{
	const char *ptr = *pptr;
	const char *delim = ptr;
	const char *next = ptr;

	if (!isprint(*ptr) && *ptr != '\0')
	{  // Skip BOM UTF-8 start codes and not end of string
		while (!isprint(*ptr) && *ptr != '\0')
		{
			ptr++;
		}
		delim = ptr;
	}
	
	if( *ptr=='\"' )
	{ // Skip startquote
		ptr++;
		delim  = strchr(ptr,'\"');
		if( delim )
		{
			next = delim+1; // skip endquote
		}
		else
		{
			delim = ptr;
		}
	}
	else
	{  // Include space and greater chars and not , and not end of string
		while( *delim >= ' ' && *delim != ',' && *delim != '\0')
		{
			delim++;
		}
		next = delim;
	}

	if( *next==',' ) next++;
	*pptr = next;

	return std::string(ptr,delim-ptr);
}

static void LoadVirtualChannelMapFromCSV( FILE *f )
{
	char buf[MAX_BUFFER_LENGTH];
	while (fgets(buf, sizeof(buf), f))
	{
		VirtualChannelInfo channelInfo;
		const char *ptr = buf;
		std::string channelNumber = GetNextFieldFromCSV( &ptr );
		// invalid input results in 0 -- !VIRTUAL_CHANNEL_VALID, will be auto assigned
		channelInfo.channelNumber = atoi(channelNumber.c_str());
		channelInfo.name = GetNextFieldFromCSV(&ptr);
		channelInfo.uri = GetNextFieldFromCSV(&ptr);
		if (!channelInfo.name.empty() && !channelInfo.uri.empty())
		{
			mVirtualChannelMap.Add( channelInfo );
		}
		else
		{ // no name, no uri, no service
			printf("[AAMPCLI] can not parse virtual channel '%s'\n", buf);
		}
	}
}

static const char *skipwhitespace( const char *ptr )
{
	while( *ptr==' ' ) ptr++;
	return ptr;
}

/**
* @brief Parse config entries for aamp-cli, and update mVirtualChannelMap
*        based on the config.
* @param f File pointer to config to process
*/
static void LoadVirtualChannelMapLegacyFormat( FILE *f )
{
	char buf[MAX_BUFFER_LENGTH];
	while (fgets(buf, sizeof(buf), f))
	{
		const char *ptr = buf;
		ptr = skipwhitespace(ptr);
		if( *ptr=='#' )
		{ // comment line
			continue;
		}
		
		if( *ptr=='*' )
		{ // skip "*" character, if present
			ptr = skipwhitespace(ptr+1);
		}
		else
		{ // not a virtual channel
			continue;
		}
		
		VirtualChannelInfo channelInfo;		// extract channel number
		// invalid input results in 0 -- !VIRTUAL_CHANNEL_VALID, will be auto assigned
		channelInfo.channelNumber = atoi(ptr);
		while( *ptr>='0' && *ptr<='9' ) ptr++;
		ptr = skipwhitespace(ptr);
		
		// extract name
		const char *delim = ptr;
		while( *delim>' ' )
		{
			delim++;
		}
		channelInfo.name = std::string(ptr,delim-ptr);
	
		// extract locator
		ptr = skipwhitespace(delim);
		delim = ptr;
		while( *delim>' ' )
		{
			delim++;
		}
		channelInfo.uri = std::string(ptr,delim-ptr);
		
		mVirtualChannelMap.Add( channelInfo );
	}
} // LoadVirtualChannelMapLegacyFormat

static FILE *GetConfigFile(const std::string& cfgFile)
{
	if (cfgFile.empty())
	{
		return NULL;
	}
#ifdef __APPLE__
	std::string cfgBasePath(getenv("HOME"));
	std::string cfgPath = cfgBasePath + cfgFile;
	FILE *f = fopen(cfgPath.c_str(), "rb");
#else
	std::string cfgPath = "/opt" + cfgFile;
	FILE *f = fopen(cfgPath.c_str(), "rb");
#endif
	
	return f;
}

/**
 * @brief
 * @param argc
 * @param argv
 * @retval
 */
int main(int argc, char **argv)
{
	char driveName = (*argv)[0];
	AampLogManager mLogManager;
	AampLogManager::disableLogRedirection = true;
	ABRManager mAbrManager;

	/* Set log directory path for AAMP and ABR Manager */
	mLogManager.setLogAndCfgDirectory(driveName);
	mAbrManager.setLogDirectory(driveName);

	printf("**************************************************************************\n");
	printf("** ADVANCED ADAPTIVE MICRO PLAYER (AAMP) - COMMAND LINE INTERFACE (CLI) **\n");
	printf("**************************************************************************\n");

	InitPlayerLoop(0,NULL);

	mSingleton = new PlayerInstanceAAMP(
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
			NULL
			,updateYUVFrame
#endif
			);

	myEventListener = new myAAMPEventListener();
	mSingleton->RegisterEvents(myEventListener);
	//std::string name = "testApp";
	//mSingleton->SetAppName(name);
	
	// Read/create virtual channel map
	const std::string cfgCSV("/aampcli.csv");
	const std::string cfgLegacy("/aampcli.cfg");
	FILE *f;
	if ( (f = GetConfigFile(cfgCSV)) != NULL)
	{ // open virtual map from csv file
		printf("[AAMPCLI] opened aampcli.csv\n");
		LoadVirtualChannelMapFromCSV( f );
		fclose( f );
		f = NULL;
	}
	else if ( (f = GetConfigFile(cfgLegacy)) != NULL)
	{  // open virtual map from legacy cfg file
		printf("[AAMPCLI] opened aampcli.cfg\n");
		LoadVirtualChannelMapLegacyFormat(f);
		fclose(f);
		f = NULL;
	}

	pthread_t cmdThreadId;
	if(pthread_create(&cmdThreadId,NULL,run_command, argv[1]) != 0)
	{
		printf("[AAMPCLI] Failed at create pthread errno = %d\n", errno);  //CID:83593 - checked return
	}
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
        // Render frames in graphics plane using opengl
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowPosition(80, 80);
	glutInitWindowSize(640, 480);
	glutCreateWindow("AAMP Texture Player");
	printf("[AAMPCLI] OpenGL Version[%s] GLSL Version[%s]\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
#ifndef __APPLE__
	glewInit();
#endif
	InitShaders();
	glutDisplayFunc(glRender);
	glutTimerFunc(40, timer, 0);

	glutMainLoop();
#else
        // Render frames in video plane - default behavior
#ifdef __APPLE__
	createAndRunCocoaWindow();
#endif
#endif
	void *value_ptr = NULL;
	pthread_join(cmdThreadId, &value_ptr);
	if(mBackgroundPlayer)
	{
		mBackgroundPlayer->Stop();
		delete mBackgroundPlayer;
	}
	mSingleton->Stop();
	delete mSingleton;
}
