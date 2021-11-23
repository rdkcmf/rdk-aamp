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
#include "ThunderAccess.h"
#include "priv_aamp.h"

#define SECMANAGER_CALL_SIGN "org.rdk.SecManager.1"
#define WATERMARK_PLUGIN_CALLSIGN "org.rdk.Watermark.1"
//#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"   //need to be used instead of WATERMARK_PLUGIN_CALLSIGN if RDK Shell is used for rendering watermark

class AampSecManager : public AampScheduler
{
public:

	/**
	 * @brief To get AampSecManager instance
	 *
	 * @return AampSecManager instance
	 */
	static AampSecManager* GetInstance();

	/**
	 * @brief To release AampSecManager singelton instance
	 */
	static void DestroyInstance();

	/**
	 * @brief To acquire license from SecManager
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
						const char* accessAttributes[][2], const char* contentMetadata,
						const char* licenseRequest, const char* keySystemId,
						const char* mediaUsage, const char* accessToken,
						int64_t* sessionId,
						char** licenseResponse, size_t* licenseResponseLength,
						int64_t* statusCode, int64_t* reasonCode);


	/**
	 * @brief To update session state to SecManager
	 *
	 * @param[in] sessionId - session id
	 * @param[in] active - true if session is active, false otherwise
	 */
	void UpdateSessionState(int64_t sessionId, bool active);

	/**
	 * @brief To notify SecManager to release a session
	 *
	 * @param[in] sessionId - session id
	 */
	void ReleaseSession(int64_t sessionId);

	bool setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height);

	bool setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position);

	bool setContentAspectRatio(int64_t sessionId, float aspect_ratio);

	bool loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize, const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio);

private:

	/**
	 * @brief AampScheduler Constructor
	 */
	AampSecManager();

	/**
	 * @brief AampScheduler Destructor
	 */
	~AampSecManager();

	AampSecManager(const AampSecManager&) = delete;
	AampSecManager* operator=(const AampSecManager&) = delete;

	void RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
	void RegisterAllEvents ();
	void UnRegisterAllEvents ();

	/*Event Handlers*/
	void watermarkSessionHandler(const JsonObject& parameters);
	void addWatermarkHandler(const JsonObject& parameters);
	void updateWatermarkHandler(const JsonObject& parameters);
	void removeWatermarkHandler(const JsonObject& parameters);
	void showWatermarkHandler(const JsonObject& parameters);

	void ShowWatermark(bool show);
	void CreateWatermark(int graphicId, int zIndex);
	void DeleteWatermark(int graphicId);
	void UpdateWatermark(int graphicId, int smKey, int smSize);
	void AlwaysShowWatermarkOnTop(bool show);

	static AampSecManager *mInstance;  /**< singleton instance*/
	PrivateInstanceAAMP* mAamp;        /**< Pointer to the PrivateInstanceAAMP*/
	ThunderAccessAAMP mSecManagerObj;  /**< ThunderAccessAAMP object for communicating with SecManager*/
	ThunderAccessAAMP mWatermarkPluginObj;    /**< ThunderAccessAAMP object for communicating with Watermark Plugin Obj*/
	std::mutex mMutex;		   /**<  Lock for accessing mSecManagerObj*/
	std::list<std::string> mRegisteredEvents;
	bool mSchedulerStarted;
};

#endif /* __AAMP_SECMANAGER_H__ */
