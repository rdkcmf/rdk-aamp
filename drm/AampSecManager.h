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

#define SECMANAGER_CALL_SIGN "org.rdk.SecManager.1"

class AampSecManager
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
	bool AcquireLicense(const char* licenseUrl, const char* moneyTraceMetdata[][2],
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
private:

	/**
	 * @brief AampScheduler Constructor
	 */
	AampSecManager();

	/**
	 * @brief AampScheduler Destructor
	 */
	~AampSecManager()
	{
	}

	AampSecManager(const AampSecManager&) = delete;
	AampSecManager* operator=(const AampSecManager&) = delete;

	static AampSecManager *mInstance; // singleton instance
	ThunderAccessAAMP mSecManagerObj; // ThunderAccessAAMP object for communicating with SecManager
	std::mutex mMutex;		  // Lock for accessing mSecManagerObj
};

#endif /* __AAMP_SECMANAGER_H__ */
