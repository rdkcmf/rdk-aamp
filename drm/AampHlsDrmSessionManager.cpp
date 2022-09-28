/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file AampHlsDrmSessionManager.cpp
 * @brief Operations for HLS DRM 
 */

#include "AampHlsDrmSessionManager.h"
#include "AampDRMSessionManager.h"
#include "AampDrmHelper.h"
#include "AampHlsOcdmBridge.h"

using namespace std;

/**
 * @brief getInstance Get DRM instance
 *        Get an instance of the Hls DRM Session Manager
 */
AampHlsDrmSessionManager& AampHlsDrmSessionManager::getInstance()
{
	static AampHlsDrmSessionManager instance;
	return instance;
}

/**
 * @brief Check stream is DRM supported
 */
bool AampHlsDrmSessionManager::isDrmSupported(const struct DrmInfo& drmInfo) const
{
	return AampDrmHelperEngine::getInstance().hasDRM(drmInfo);
}

/**
 * @brief createSession create session for DRM
 */
std::shared_ptr<HlsDrmBase> AampHlsDrmSessionManager::createSession(PrivateInstanceAAMP* aampInstance, const struct DrmInfo& drmInfo,MediaType streamType, AampLogManager *mLogObj)
{
	std::shared_ptr<HlsDrmBase> bridge = nullptr;
	std::shared_ptr<AampDrmHelper> drmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo, mLogObj);
	aampInstance->mDRMSessionManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);

	aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
	DrmMetaDataEventPtr event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false);
	mDrmSession = aampInstance->mDRMSessionManager->createDrmSession(drmHelper, event, aampInstance, streamType);
	if (!mDrmSession)
	{
		AAMPLOG_WARN("Failed to create Drm Session ");

		if (aampInstance->DownloadsAreEnabled())
		{
			AAMPTuneFailure failure = event->getFailure();

			aampInstance->DisableDownloads();
			aampInstance->SendErrorEvent(failure);

			aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int) failure);
		}
	}
	else
	{
		AAMPLOG_WARN("created Drm Session ");
		bridge = std::make_shared<AampHlsOcdmBridge>(mLogObj, mDrmSession);
	}
	aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
	return bridge;
}
