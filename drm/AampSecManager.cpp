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
 * @file AampSecManager.cpp
 * @brief Class impl for AampSecManager
 */

#include "AampSecManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64
#include "priv_aamp.h" // Included for AAMPLOG
//TODO: Fix cyclic dependency btw GlobalConfig and AampLogManager


AampSecManager* AampSecManager::mInstance = NULL;

/**
 * @brief To get AampSecManager instance
 *
 * @return AampSecManager instance
 */
AampSecManager* AampSecManager::GetInstance()
{
	if (mInstance == NULL)
	{
		mInstance = new AampSecManager();
	}
	return mInstance;
}

/**
 * @brief To release AampSecManager singelton instance
 */
void AampSecManager::DestroyInstance()
{
	if (mInstance)
	{
		delete mInstance;
		mInstance = NULL;
	}
}

/**
 * @brief AampScheduler Constructor
 */
AampSecManager::AampSecManager() : mSecManagerObj(SECMANAGER_CALL_SIGN), mMutex()
{
	std::lock_guard<std::mutex> lock(mMutex);
	mSecManagerObj.ActivatePlugin();
}

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
bool AampSecManager::AcquireLicense(const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata,
					const char* licenseRequest, const char* keySystemId,
					const char* mediaUsage, const char* accessToken,
					int64_t* sessionId,
					char** licenseResponse, size_t* licenseResponseLength,
					int64_t* statusCode, int64_t* reasonCode)
{
	// licenseUrl un-used now
	(void) licenseUrl;

	bool ret = false;
	bool rpcResult = false;

	const char* apiName = "openPlaybackSession";
	JsonObject param;
	JsonObject response;
	JsonObject sessionConfig;
	JsonObject aspectDimensions;

	sessionConfig["distributedTraceType"] = "money";
	sessionConfig["distributedTraceId"] = moneyTraceMetdata[0][1];
	sessionConfig["sessionState"] = "active";

	// TODO: Remove hardcoded values
	aspectDimensions["height"] = 1920;
	aspectDimensions["width"] = 1080;

	param["clientId"] = "aamp";
	param["sessionConfiguration"] = sessionConfig;
	param["contentAspectDimensions"] = aspectDimensions;

	param["keySystem"] = keySystemId;
	param["licenseRequest"] = licenseRequest;
	param["contentMetadata"] = contentMetdata;
	param["mediaUsage"] = mediaUsage;
	param["accessToken"] = accessToken;

	// If sessionId is present, we are trying to acquire a new license within the same session
	if (*sessionId != -1)
	{
		apiName = "updatePlaybackSession";
		param["sessionId"] = *sessionId;
	}

#ifdef DEBUG_SECMAMANER
	std::string params;
	param.ToString(params);
	AAMPLOG_WARN("%s:%d SecManager openPlaybackSession param: %s", __FUNCTION__, __LINE__, params.c_str());
#endif

	{
		std::lock_guard<std::mutex> lock(mMutex);
		rpcResult = mSecManagerObj.InvokeJSONRPC(apiName, param, response, 10000);
	}

	if (rpcResult)
	{
#ifdef DEBUG_SECMAMANER
		std::string output;
		response.ToString(output);
		AAMPLOG_WARN("%s:%d SecManager openPlaybackSession o/p: %s", __FUNCTION__, __LINE__, output.c_str());
#endif
		if (response["success"].Boolean())
		{
			std::string license = response["license"].String();
			AAMPLOG_TRACE("%s:%d SecManager obtained license with length: %d and data: %s", __FUNCTION__, __LINE__, license.size(), license.c_str());
			if (!license.empty())
			{
				// Here license is base64 encoded
				unsigned char * licenseDecoded = NULL;
				size_t licenseDecodedLen = 0;
				licenseDecoded = base64_Decode(license.c_str(), &licenseDecodedLen);
				AAMPLOG_TRACE("%s:%d SecManager license decoded len: %d and data: %p", __FUNCTION__, __LINE__, licenseDecodedLen, licenseDecoded);

				if (licenseDecoded != NULL && licenseDecodedLen != 0)
				{
					AAMPLOG_INFO("%s:%d SecManager license post base64 decode length: %d", __FUNCTION__, __LINE__, *licenseResponseLength);
					*licenseResponse = (char*) malloc(licenseDecodedLen);
					if (*licenseResponse)
					{
						memcpy(*licenseResponse, licenseDecoded, licenseDecodedLen);
						*licenseResponseLength = licenseDecodedLen;
					}
					else
					{
						AAMPLOG_ERR("%s:%d SecManager failed to allocate memory for license!", __FUNCTION__, __LINE__);
					}
					free(licenseDecoded);
					ret = true;
				}
				else
				{
					AAMPLOG_ERR("%s:%d SecManager license base64 decode failed!", __FUNCTION__, __LINE__);
				}
			}
		}
		// Save session ID
		if (*sessionId == -1)
		{
			*sessionId = response["sessionId"].Number();
		}
		// TODO: Sort these values out for backward compatibility
		JsonObject resultContext = response["secManagerResultContext"].Object();
		*statusCode = resultContext["class"].Number();
		*reasonCode = resultContext["reason"].Number();
	}
	else
	{
		AAMPLOG_ERR("%s:%d SecManager openPlaybackSession failed", __FUNCTION__, __LINE__);
	}
	return ret;
}

/**
 * @brief To update session state to SecManager
 *
 * @param[in] sessionId - session id
 * @param[in] active - true if session is active, false otherwise
 */
void AampSecManager::UpdateSessionState(int64_t sessionId, bool active)
{
	bool rpcResult = false;
	JsonObject result;
	JsonObject param;
	param["clientId"] = "aamp";
	param["sessionId"] = sessionId;
	AAMPLOG_INFO("%s:%d SecManager call setPlaybackSessionState for ID: %" PRId64 " and state: %d", __FUNCTION__, __LINE__, sessionId, active);
	if (active)
	{
		param["sessionState"] = "active";
	}
	else
	{
		param["sessionState"] = "inactive";
	}

	{
		std::lock_guard<std::mutex> lock(mMutex);
		rpcResult = mSecManagerObj.InvokeJSONRPC("setPlaybackSessionState", param, result);
	}

	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("%s:%d SecManager setPlaybackSessionState failed for ID: %" PRId64 ", active:%d and result: %s", __FUNCTION__, __LINE__, sessionId, active, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d SecManager setPlaybackSessionState failed for ID: %" PRId64 " and active: %d", __FUNCTION__, __LINE__, sessionId, active);
	}
}

/**
 * @brief To notify SecManager to release a session
 *
 * @param[in] sessionId - session id
 */
void AampSecManager::ReleaseSession(int64_t sessionId)
{
	bool rpcResult = false;
	JsonObject result;
	JsonObject param;
	param["clientId"] = "aamp";
	param["sessionId"] = sessionId;
	AAMPLOG_INFO("%s:%d SecManager call closePlaybackSession for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

	{
		std::lock_guard<std::mutex> lock(mMutex);
		rpcResult = mSecManagerObj.InvokeJSONRPC("closePlaybackSession", param, result);
	}

	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("%s:%d SecManager closePlaybackSession failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d SecManager closePlaybackSession failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
	}
}
