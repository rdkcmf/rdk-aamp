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
AampSecManager::AampSecManager() : mSecManagerObj(SECMANAGER_CALL_SIGN), mMutex(),mSchedulerStarted(false),
				   mRegisteredEvents(), mWatermarkPluginObj(WATERMARK_PLUGIN_CALLSIGN)
{
	std::lock_guard<std::mutex> lock(mMutex);
	mSecManagerObj.ActivatePlugin();
	mWatermarkPluginObj.ActivatePlugin();

	/*Start Scheduler for handling RDKShell API invocation*/    
	if(false == mSchedulerStarted)
	{
		StartScheduler();
		mSchedulerStarted = true;
	}

	RegisterAllEvents();
}

/**
 * @brief AampScheduler Destructor
 */
AampSecManager::~AampSecManager()
{
	std::lock_guard<std::mutex> lock(mMutex);

	/*Stop Scheduler used for handling RDKShell API invocation*/    
	if(true == mSchedulerStarted)
	{
		StopScheduler();
		mSchedulerStarted = false;
	}

	UnRegisterAllEvents();
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
bool AampSecManager::AcquireLicense(PrivateInstanceAAMP* aamp, const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					int64_t* sessionId,
					char** licenseResponse, size_t* licenseResponseLength,
					int64_t* statusCode, int64_t* reasonCode)
{
	// licenseUrl un-used now
	(void) licenseUrl;

	bool ret = false;
	bool rpcResult = false;
	
	//Shared memory pointer, key declared here,
	//Access token, content metadata and licnese request will be passed to
	//secmanager via shared memory
	void * shmPt_accToken = NULL;
	key_t shmKey_accToken = 0;
	void * shmPt_contMeta = NULL;
	key_t shmKey_contMeta = 0;
	void * shmPt_licReq = NULL;
	key_t shmKey_licReq = 0;
		
	const char* apiName = "openPlaybackSession";
	JsonObject param;
	JsonObject response;
	JsonObject sessionConfig;
	JsonObject aspectDimensions;

	if(NULL != aamp)
		mAamp = aamp;

	sessionConfig["distributedTraceType"] = "money";
	sessionConfig["distributedTraceId"] = moneyTraceMetdata[0][1];
	sessionConfig["sessionState"] = "active";

	// TODO: Remove hardcoded values
	aspectDimensions["width"] = 1920;
	aspectDimensions["height"] = 1080;

	param["clientId"] = "aamp";
	param["sessionConfiguration"] = sessionConfig;
	param["contentAspectDimensions"] = aspectDimensions;

	param["keySystem"] = keySystemId;
	param["mediaUsage"] = mediaUsage;
	
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
		if(accTokenLen > 0 && contMetaLen > 0 && licReqLen > 0)
		{
			shmPt_accToken = aamp_CreateSharedMem(accTokenLen, shmKey_accToken);
			shmPt_contMeta = aamp_CreateSharedMem(contMetaLen, shmKey_contMeta);
			shmPt_licReq = aamp_CreateSharedMem(licReqLen, shmKey_licReq);
		}
		
		//Set shared memory with the buffer
		if(NULL != shmPt_accToken && NULL != accessToken &&
		   NULL != shmPt_contMeta && NULL != contentMetdata &&
		   NULL != shmPt_licReq && NULL != licenseRequest)
		{
			//copy buffer to shm
			memcpy(shmPt_accToken, accessToken, accTokenLen);
			memcpy(shmPt_contMeta, contentMetdata, contMetaLen);
			memcpy(shmPt_licReq, licenseRequest, licReqLen);
			
			AAMPLOG_INFO("Access token, Content metadata and license request are copied to the shared memory successfully, passing details with SecManager");
			
			//Set json params to be used by sec manager
			param["accessTokenBufferKey"] = shmKey_accToken;
			param["accessTokenLength"] = accTokenLen;
			
			param["contentMetadataBufferKey"] = shmKey_contMeta;
			param["contentMetadataLength"] = contMetaLen;
			
			param["licenseRequestBufferKey"] = shmKey_licReq;
			param["licenseRequestLength"] = licReqLen;
			
			//invoke "openPlaybackSession"
			rpcResult = mSecManagerObj.InvokeJSONRPC(apiName, param, response, 10000);
			
			//Cleanup the shared memory after sharing it with secmanager
			aamp_CleanUpSharedMem( shmPt_accToken, shmKey_accToken, accTokenLen);
			aamp_CleanUpSharedMem( shmPt_contMeta, shmKey_contMeta, contMetaLen);
			aamp_CleanUpSharedMem( shmPt_licReq, shmKey_licReq, licReqLen);
		}
		else
		{
			AAMPLOG_WARN("%s:%d Failed to copy access token to the shared memory, open playback session is aborted", __FUNCTION__, __LINE__);
		}
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
	/*Clear aampInstanse pointer*/
	mAamp = NULL;
}
/**
 * @brief To update session state to SecManager
 *
 * @param[in] sessionId - session id
 * @param[in] video_width - video width 
 * @param[in] video_height - video height 
 */
bool AampSecManager::setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
       bool rpcResult = false;
       JsonObject result;
       JsonObject param;

       param["sessionId"] = sessionId;
       param["videoWidth"] = video_width;
       param["videoHeight"] = video_height;

       AAMPLOG_INFO("%s:%d SecManager call setVideoWindowSize for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       {
               std::lock_guard<std::mutex> lock(mMutex);
               rpcResult = mSecManagerObj.InvokeJSONRPC("setVideoWindowSize", param, result);
       }

       if (rpcResult)
       {
               if (!result["success"].Boolean())
               {
                       std::string responseStr;
                       result.ToString(responseStr);
                       AAMPLOG_ERR("%s:%d SecManager setVideoWindowSize failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }

       }
       else
       {
               AAMPLOG_ERR("%s:%d SecManager setVideoWindowSize failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
       return rpcResult;
}

/**
 * @brief To set Playback Speed State to SecManager
 *
 * @param[in] sessionId - session id
 * @param[in] playback_speed - playback speed 
 * @param[in] playback_position - playback position 
 */
bool AampSecManager::setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position)
{
       bool rpcResult = false;
       JsonObject result;
       JsonObject param;

       param["sessionId"] = sessionId;
       param["playbackSpeed"] = playback_speed;
       param["playbackPosition"] = playback_position;

       AAMPLOG_INFO("%s:%d SecManager call setPlaybackSpeedState for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

       {
               std::lock_guard<std::mutex> lock(mMutex);
               rpcResult = mSecManagerObj.InvokeJSONRPC("setPlaybackSpeedState", param, result);
       }
	   
       if (rpcResult)
       {
               if (!result["success"].Boolean())
               {
                       std::string responseStr;
                       result.ToString(responseStr);
                       AAMPLOG_ERR("%s:%d SecManager setPlaybackSpeedState failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }
       }
       else
       {
               AAMPLOG_ERR("%s:%d SecManager setPlaybackSpeedState failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
       return rpcResult;
}

/**
 * @brief To Load ClutWatermark
 * @param[in] 
 *  */
bool AampSecManager::loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize,
                                                                       const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio)
{
       bool rpcResult = false;
      JsonObject result;
       JsonObject param;

       param["sessionId"] = sessionId;
       param["graphicId"] = graphicId;
       param["watermarkClutBufferKey"] = watermarkClutBufferKey;
       param["watermarkImageBufferKey"] = watermarkImageBufferKey;
       param["clutPaletteSize"] = clutPaletteSize;
       param["clutPaletteFormat"] = clutPaletteFormat;
       param["watermarkWidth"] = watermarkWidth;
       param["watermarkHeight"] = watermarkHeight;
       param["aspectRatio"] = std::to_string(aspectRatio).c_str();

       AAMPLOG_INFO("%s:%d SecManager call loadClutWatermark for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

       {
               std::lock_guard<std::mutex> lock(mMutex);
               rpcResult = mSecManagerObj.InvokeJSONRPC("loadClutWatermark", param, result);
       }

       if (rpcResult)
       {
               if (!result["success"].Boolean())
               {
                       std::string responseStr;
                       result.ToString(responseStr);
                       AAMPLOG_ERR("%s:%d SecManager loadClutWatermark failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }
       }
       else
       {
               AAMPLOG_ERR("%s:%d SecManager loadClutWatermark failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
       return rpcResult;

}

/**
 *   @brief  Registers  Event to input plugin and to mRegisteredEvents list for later use.
 *   @param[in] eventName : Event name
 *   @param[in] functionHandler : Event funciton pointer
 */
void AampSecManager::RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
	bool bSubscribed;
	bSubscribed = mSecManagerObj.SubscribeEvent(_T(eventName), functionHandler);
	if(bSubscribed)
	{
		mRegisteredEvents.push_back(eventName);
	}
}

/**
 *   @brief  Registers all Events to input plugin
 */
void AampSecManager::RegisterAllEvents ()
{
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> watermarkSessionMethod = std::bind(&AampSecManager::watermarkSessionHandler, this, std::placeholders::_1);

	RegisterEvent("onWatermarkSession",watermarkSessionMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> addWatermarkMethod = std::bind(&AampSecManager::addWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onAddWatermark",addWatermarkMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> updateWatermarkMethod = std::bind(&AampSecManager::updateWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onUpdateWatermark",updateWatermarkMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> removeWatermarkMethod = std::bind(&AampSecManager::removeWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onRemoveWatermark",removeWatermarkMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> showWatermarkMethod = std::bind(&AampSecManager::showWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onDisplayWatermark",showWatermarkMethod);

}

/**
 *   @brief  UnRegisters all Events from plugin
 */
void AampSecManager::UnRegisterAllEvents ()
{
	for (auto const& evtName : mRegisteredEvents) {
		mSecManagerObj.UnSubscribeEvent(_T(evtName));
	}
	mRegisteredEvents.clear();
}

/**
 *   @brief  Detects watermarking session conditions
 */
void AampSecManager::watermarkSessionHandler(const JsonObject& parameters)
{
	std::string param;
	parameters.ToString(param);
	AAMPLOG_WARN("AampSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
	if(NULL != mAamp)
	{
		mAamp->SendWatermarkSessionUpdateEvent( parameters["sessionId"].Number(),parameters["conditionContext"].Number(),parameters["watermarkingSystem"].String());
	}
}
/**
 *   @brief  Gets watermark image details and manages watermark rendering
 */
void AampSecManager::addWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMAMANER
	std::string param;
	parameters.ToString(param);

	AAMPLOG_WARN("AampSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(mSchedulerStarted)
	{
		int graphicId = parameters["graphicId"].Number();
		int zIndex = parameters["zIndex"].Number();
		AAMPLOG_WARN("AampSecManager::%s:%d graphicId : %d index : %d ", __FUNCTION__, __LINE__, graphicId, zIndex);
		ScheduleTask(AsyncTaskObj([graphicId, zIndex](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->CreateWatermark(graphicId, zIndex);
					  }, (void *) this));

		int smKey = parameters["graphicImageBufferKey"].Number();
		int smSize = parameters["graphicImageSize"].Number();/*ToDo : graphicImageSize (long) long conversion*/
		AAMPLOG_WARN("AampSecManager::%s:%d graphicId : %d smKey: %d smSize: %d", __FUNCTION__, __LINE__, graphicId, smKey, smSize);
		ScheduleTask(AsyncTaskObj([graphicId, smKey, smSize](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->UpdateWatermark(graphicId, smKey, smSize);
					  }, (void *) this));
		
		if (parameters["adjustVisibilityRequired"].Boolean())
		{
			int sessionId = parameters["sessionId"].Number();
			AAMPLOG_WARN("AampSecManager::%s:%d adjustVisibilityRequired is true, invoking GetWaterMarkPalette. graphicId : %d", __FUNCTION__, __LINE__, graphicId);
			ScheduleTask(AsyncTaskObj([sessionId, graphicId](void *data)
									  {
				AampSecManager *instance = static_cast<AampSecManager *>(data);
				instance->GetWaterMarkPalette(sessionId, graphicId);
			}, (void *) this));
		}
		else
		{
			AAMPLOG_WARN("AampSecManager::%s:%d adjustVisibilityRequired is false, graphicId : %d", __FUNCTION__, __LINE__, graphicId);
		}

#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		ScheduleTask(AsyncTaskObj([show](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->AlwaysShowWatermarkOnTop(show);
					  }, (void *) this));
#endif
	}
}
/**
 *   @brief  Gets updated watermark image details and manages watermark rendering
 */
void AampSecManager::updateWatermarkHandler(const JsonObject& parameters)
{
	if(mSchedulerStarted)
	{
		int graphicId = parameters["graphicId"].Number();
		int clutKey = parameters["watermarkClutBufferKey"].Number();
		int imageKey = parameters["watermarkImageBufferKey"].Number();
		AAMPLOG_TRACE("graphicId : %d ",graphicId);
		ScheduleTask(AsyncTaskObj([graphicId, clutKey, imageKey](void *data)
								  {
			AampSecManager *instance = static_cast<AampSecManager *>(data);
			instance->ModifyWatermarkPalette(graphicId, clutKey, imageKey);
		}, (void *) this));
	}
}
/**
 *   @brief Removes watermark image
*/
void AampSecManager::removeWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMAMANER
	std::string param;
	parameters.ToString(param);
	AAMPLOG_WARN("AampSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(mSchedulerStarted)
	{
		int graphicId = parameters["graphicId"].Number();
		AAMPLOG_WARN("AampSecManager::%s:%d graphicId : %d ", __FUNCTION__, __LINE__, graphicId);
		ScheduleTask(AsyncTaskObj([graphicId](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->DeleteWatermark(graphicId);
					  }, (void *) this));
#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		ScheduleTask(AsyncTaskObj([show](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->AlwaysShowWatermarkOnTop(show);
					  }, (void *) this));
#endif
	}

}

/**
 *   @brief Handles watermark calls to be only once
*/
void AampSecManager::showWatermarkHandler(const JsonObject& parameters)
{
	bool show = true;
	if(parameters["hideWatermark"].Boolean())
	{
		show = false;
	}
	AAMPLOG_INFO("Received onDisplayWatermark, show: %d ", show);
	if(mSchedulerStarted)
	{
		ScheduleTask(AsyncTaskObj([show](void *data)
		{
			AampSecManager *instance = static_cast<AampSecManager *>(data);
			instance->ShowWatermark(show);
		}, (void *) this));
	}
	
	return;
}


/**
 *   @brief Show watermark image
*/
void AampSecManager::ShowWatermark(bool show)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
	std::lock_guard<std::mutex> lock(mMutex);

	param["show"] = show;
	rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("showWatermark", param, result);
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}

	return;
}

/**
 *   @brief Create Watermark
*/
void AampSecManager::CreateWatermark(int graphicId, int zIndex )
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
	std::lock_guard<std::mutex> lock(mMutex);

	param["id"] = graphicId;
	param["zorder"] = zIndex;
	rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("createWatermark", param, result);
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
	return;
}

/**
 *   @brief Delete Watermark
*/
void AampSecManager::DeleteWatermark(int graphicId)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
	std::lock_guard<std::mutex> lock(mMutex);

	param["id"] = graphicId;
	rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("deleteWatermark", param, result);
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}

	return;
}

/**
 *   @brief Create Watermark
*/
void AampSecManager::UpdateWatermark(int graphicId, int smKey, int smSize )
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
	std::lock_guard<std::mutex> lock(mMutex);

	param["id"] = graphicId;
	param["key"] = smKey;
	param["size"] = smSize;
	rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("updateWatermark", param, result);
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}

	return;
}
/**
 *   @brief Show watermark image
 *   This method need to be used only when RDKShell is used for rendering. Not supported by Watermark Plugin
*/
void AampSecManager::AlwaysShowWatermarkOnTop(bool show)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
	std::lock_guard<std::mutex> lock(mMutex);

	param["show"] = show;
	rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("alwaysShowWatermarkOnTop", param, result);
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
}

/**
 *   @brief GetWaterMarkPalette
*/
void AampSecManager::GetWaterMarkPalette(int sessionId, int graphicId)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;
	param["id"] = graphicId;
	AAMPLOG_WARN("AampSecManager %s:%d Graphic id: %d ", __FUNCTION__, __LINE__, graphicId);
	{
		std::lock_guard<std::mutex> lock(mMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("getPalettedWatermark", param, result);
	}

	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
		else //if success, request sec manager to load the clut into the clut shm
		{

			AAMPLOG_WARN("AampSecManager::%s getWatermarkPalette invoke success for graphicId %d, calling loadClutWatermark", __FUNCTION__, graphicId);
			int clutKey = result["clutKey"].Number();
			int clutSize = result["clutSize"].Number();
			int imageKey = result["imageKey"].Number();
			int imageWidth = result["imageWidth"].Number();
			int imageHeight = result["imageHeight"].Number();
			float aspectRatio = (imageHeight != 0) ? (float)imageWidth/(float)imageHeight : 0.0;
			AampSecManager::GetInstance()->loadClutWatermark(sessionId, graphicId, clutKey, imageKey,
															 clutSize, "RGBA8888", imageWidth, imageHeight,
															 aspectRatio);
		}

	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
}

/**
 *   @brief SetWatermarkPalette
*/
void AampSecManager::ModifyWatermarkPalette(int graphicId, int clutKey, int imageKey)
{
	JsonObject param;
	JsonObject result;

	bool rpcResult = false;
	param["id"] = graphicId;
	param["clutKey"] = clutKey;
	param["imageKey"] = imageKey;
	{
		std::lock_guard<std::mutex> lock(mMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("modifyPalettedWatermark", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			AAMPLOG_ERR("modifyPalettedWatermark failed with result: %s, graphic id: %d", responseStr.c_str(), graphicId);
		}
		else
		{
			AAMPLOG_TRACE("AampSecManager::%s setWatermarkPalette invoke success, graphic id: %d", graphicId);
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!, graphic id: %d", graphicId);
	}
}
