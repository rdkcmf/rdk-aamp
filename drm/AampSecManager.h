/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file AampSecManager.h
 * @brief Class to communicate with SecManager Thunder plugin
 */

#ifndef __AAMP_SECMANAGER_H__
#define __AAMP_SECMANAGER_H__

#include <mutex>
#include "priv_aamp.h"
#include "ThunderAccess.h"


#define SECMANAGER_CALL_SIGN "org.rdk.SecManager.1"
#define WATERMARK_PLUGIN_CALLSIGN "org.rdk.Watermark.1"
//#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"   //need to be used instead of WATERMARK_PLUGIN_CALLSIGN if RDK Shell is used for rendering watermark

/**
 * @class AampSecManager
 * @brief Class to get License from Sec Manager
 */
class AampSecManager : public AampScheduler
{
public:

	/**
	 * @fn GetInstance
	 *
	 * @return AampSecManager instance
	 */
	static AampSecManager* GetInstance();

	/**
	 * @fn DestroyInstance
	 */
	static void DestroyInstance();

	/**
	 * @fn AcquireLicense
	 *
	 * @param[in] licenseUrl - url to fetch license from
	 * @param[in] moneyTraceMetdata - moneytrace info
	 * @param[in] accessAttributes - accessAttributes info
	 * @param[in] contentMetadata - content metadata info
	 * @param[in] licenseRequest - license challenge info
	 * @param[in] keySystemId - unique system ID of drm
	 * @param[in] mediaUsage - indicates whether its stream or download license request
	 * @param[in] accessToken - access token info
	 * @param[out] sessionId - ID of current session
	 * @param[out] licenseResponse - license response
	 * @param[out] licenseResponseLength - len of license response
	 * @param[out] statusCode - license fetch status code
	 * @param[out] reasonCode - license fetch reason code
	 * @return bool - true if license fetch successful, false otherwise
	 */
	bool AcquireLicense(PrivateInstanceAAMP* aamp, const char* licenseUrl, const char* moneyTraceMetdata[][2],
						const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
						const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
						const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
						int64_t* sessionId,
						char** licenseResponse, size_t* licenseResponseLength,
						int32_t* statusCode, int32_t* reasonCode);


	/**
	 * @fn UpdateSessionState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] active - true if session is active, false otherwise
	 */
	void UpdateSessionState(int64_t sessionId, bool active);

	/**
	 * @fn ReleaseSession
	 *
	 * @param[in] sessionId - session id
	 */
	void ReleaseSession(int64_t sessionId);
	/**
	 * @fn setVideoWindowSize
	 *
	 * @param[in] sessionId - session id
	 * @param[in] video_width - video width 
	 * @param[in] video_height - video height 
	 */
	bool setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height);
	/**
	 * @fn setPlaybackSpeedState
	 *
	 * @param[in] sessionId - session id
	 * @param[in] playback_speed - playback speed 
	 * @param[in] playback_position - playback position
	 * @param[in] delayNeeded - if delay is required, to avoid any wm flash before tune
	 */
	bool setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position, bool delayNeeded = false);
	/**
	 * @fn loadClutWatermark
	 * @param[in] sessionId - session id
	 *  
	 */
	bool loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize, const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio);

private:

	/**
	 * @fn AampSecManager
	 */
	AampSecManager();

	/**
	 * @fn ~AampSecManager
	 */
	~AampSecManager();
	/**     
     	 * @brief Copy constructor disabled
    	 *
     	 */
	AampSecManager(const AampSecManager&) = delete;
	/**
 	 * @brief assignment operator disabled
         *
         */
	AampSecManager* operator=(const AampSecManager&) = delete;
	/**
	 *   @fn RegisterEvent
	 *   @param[in] eventName : Event name
	 *   @param[in] functionHandler : Event funciton pointer
	 */
	void RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
	/**
	 *   @fn RegisterAllEvents
	 */
	void RegisterAllEvents ();
	/**
	 *   @fn UnRegisterAllEvents
	 */
	void UnRegisterAllEvents ();

	/*Event Handlers*/
	/**
	 *   @fn watermarkSessionHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void watermarkSessionHandler(const JsonObject& parameters);
	/**
	 *   @fn addWatermarkHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void addWatermarkHandler(const JsonObject& parameters);
	/**
	 *   @fn updateWatermarkHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void updateWatermarkHandler(const JsonObject& parameters);
	/**
	 *   @fn removeWatermarkHandler
	 *   @param  parameters - i/p JsonObject params
	 */
	void removeWatermarkHandler(const JsonObject& parameters);
	/**
	 *   @fn showWatermarkHandler
	 *   @param parameters - i/p JsonObject params	 
	 */
	void showWatermarkHandler(const JsonObject& parameters);

	/**
	 *   @fn ShowWatermark
	 */
	void ShowWatermark(bool show);
	/**
	 *   @fn CreateWatermark
	 */
	void CreateWatermark(int graphicId, int zIndex);
	/**
	 *   @fn DeleteWatermark
	 */
	void DeleteWatermark(int graphicId);
	/**
	 *   @fn UpdateWatermark
	 */
	void UpdateWatermark(int graphicId, int smKey, int smSize);
	/**
	 *   @fn AlwaysShowWatermarkOnTop
	 */
	void AlwaysShowWatermarkOnTop(bool show);
	/**
	 *   @fn GetWaterMarkPalette
	 */
	void GetWaterMarkPalette(int sessionId, int graphicId);
	/**
	 *   @fn ModifyWatermarkPalette
	 */
	void ModifyWatermarkPalette(int graphicId, int clutKey, int imageKey);

	static AampSecManager *mInstance;       /**< singleton instance*/
	PrivateInstanceAAMP* mAamp;             /**< Pointer to the PrivateInstanceAAMP*/
	ThunderAccessAAMP mSecManagerObj;       /**< ThunderAccessAAMP object for communicating with SecManager*/
	ThunderAccessAAMP mWatermarkPluginObj;  /**< ThunderAccessAAMP object for communicating with Watermark Plugin Obj*/
	std::mutex mSecMutex;    	        /**< Lock for accessing mSecManagerObj*/
	std::mutex mWatMutex;		        /**< Lock for accessing mWatermarkPluginObj*/
	std::mutex mSpeedStateMutex;		/**< Lock for accessing mWatermarkPluginObj*/
	std::list<std::string> mRegisteredEvents;
	bool mSchedulerStarted;
};

#endif /* __AAMP_SECMANAGER_H__ */
