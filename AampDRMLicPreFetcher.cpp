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

#include "AampDRMLicPreFetcher.h"
#include "AampFnLogger.h"
#include "AampDrmSession.h"
#include "AampDRMSessionManager.h"
#include "AampUtils.h"	// for aamp_GetDeferTimeMs

/**
 * @brief For generating IDs for LicensePreFetchObject
 * 
 */
int LicensePreFetchObject::staticId = 1;

/**
 * @brief Construct a new Aamp License Pre Fetcher object
 * 
 * @param logObj Log manager object
 * @param aamp PrivateInstanceAAMP instance
 * @param fetcherInstance AampLicenseFetcher instance
 */
AampLicensePreFetcher::AampLicensePreFetcher(AampLogManager *logObj, PrivateInstanceAAMP *aamp, AampLicenseFetcher *fetcherInstance) : mPreFetchThread(),
		mFetchQueue(),
		mQMutex(),
		mQCond(),
		mPreFetchThreadStarted(false),
		mExitLoop(false),
		mCommonKeyDuration(0),
		mTrackStatus(),
		mPrivAAMP(aamp),
		mLogObj(logObj),
		mFetchInstance(fetcherInstance)
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );
	mTrackStatus.fill(false);
}

/**
 * @brief Destroy the Aamp License Pre Fetcher object
 * 
 */
AampLicensePreFetcher::~AampLicensePreFetcher()
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );
	DeInit();
}

/**
 * @brief Initialize resources
 * 
 * @return true if successfully initialized
 * @return false if error occurred
 */
bool AampLicensePreFetcher::Init()
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );
	bool ret = true;
	if (mPreFetchThreadStarted)
	{
		AAMPLOG_ERR("PreFetch thread is already started when calling Init. Crashing application for debug!");
		assert(false);
	}
	mTrackStatus.fill(false);
	mExitLoop = false;
	return ret;
}

/**
 * @brief Queue a content protection info to be processed later
 * 
 * @param drmHelper AampDrmHelper shared_ptr
 * @param periodId ID of the period to which CP belongs to
 * @param adapId ID of the adaptation to which CP belongs to
 * @param type media type
 * @return true if successfully queued
 * @return false if error occurred
 */
bool AampLicensePreFetcher::QueueContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, std::string periodId, std::string adapId, MediaType type)
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );
	bool ret = true;
	LicensePreFetchObjectPtr fetchObject = std::make_shared<LicensePreFetchObject>(drmHelper, periodId, adapId, type);
	if (fetchObject)
	{
		std::lock_guard<std::mutex>lock(mQMutex);
		mFetchQueue.push_back(fetchObject);
		if (!mPreFetchThreadStarted)
		{
			AAMPLOG_WARN("Starting mPreFetchThread");
			mExitLoop = false;
			mPreFetchThread = std::thread(&AampLicensePreFetcher::PreFetchThread, this);
			mPreFetchThreadStarted = true;
		}
		else
		{
			AAMPLOG_WARN("Notify mPreFetchThread");
			mQCond.notify_one();
		}
	}
	return ret;
}

/**
 * @brief De-initialize/free resources
 * 
 * @return true if success
 * @return false if failed
 */
bool AampLicensePreFetcher::DeInit()
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );
	bool ret = true;
	mExitLoop = true;
	if (mPreFetchThreadStarted)
	{
		mQCond.notify_one();
		AAMPLOG_WARN("Joining mPreFetchThread");
		mPreFetchThread.join();
		mPreFetchThreadStarted = false;
	}
	while (!mFetchQueue.empty())
	{
		mFetchQueue.pop_front();
	}
	return ret;
}

/**
 * @brief Thread for processing content protection queued using QueueContentProtection
 * Thread will be joined when DeInit is called
 * 
 */
void AampLicensePreFetcher::PreFetchThread()
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );

	if(aamp_pthread_setname(pthread_self(), "aampfMP4DRM"))
	{
		AAMPLOG_ERR("aamp_pthread_setname failed");
	}

	std::unique_lock<std::mutex>queueLock(mQMutex);
	while (!mExitLoop)
	{
		if (mFetchQueue.empty())
		{
			AAMPLOG_INFO("Waiting for new entry in mFetchQueue");
			mQCond.wait(queueLock);
		}
		else
		{
			LicensePreFetchObjectPtr obj = mFetchQueue.front();
			mFetchQueue.pop_front();
			queueLock.unlock();

			if (mCommonKeyDuration > 0)
			{
				int deferTime = aamp_GetDeferTimeMs(mCommonKeyDuration);
				// Going to sleep for deferred key process
				mPrivAAMP->InterruptableMsSleep(deferTime);
				AAMPLOG_TRACE("Sleep over for deferred time:%d", deferTime);
			}

			if (!mExitLoop)
			{
				bool skip = false;
				bool keyStatus = false;
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM) || defined(USE_OPENCDM)
				std::vector<uint8_t> keyIdArray;
				obj->mHelper->getKey(keyIdArray);
				if (!keyIdArray.empty() && mPrivAAMP->mDRMSessionManager->IsKeyIdProcessed(keyIdArray, keyStatus))
				{
					AAMPLOG_WARN("Key already processed [status:%d] for obj:%p, Skipping", keyStatus, obj);
					skip = true;
				}
#endif
				if (!skip)
				{
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM) || defined(USE_OPENCDM)
					if (!keyIdArray.empty())
					{
						std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);
						AAMPLOG_INFO("Creating DRM session for type:%d period ID:%s and Key ID:%s", obj->mType, obj->mPeriodId.c_str(), keyIdDebugStr.c_str());
					}
#endif
					if (CreateDRMSession(obj))
					{
						keyStatus = true;
					}
				}

				if (keyStatus)
				{
					AAMPLOG_INFO("Updating mTrackStatus to true for type:%d", obj->mType);
					try
					{
						mTrackStatus.at(obj->mType) = true;
					}
					catch (std::out_of_range const& exc)
					{
						AAMPLOG_ERR("Unable to set the mTrackStatus for type:%d, caught exception: %s", obj->mType, exc.what());
					}
				}
			}
			queueLock.lock();
		}
	}
}

/**
 * @brief To notify DRM failure to player after proper checks
 * 
 * @param fetchObj object for which session creation failed
 * @param event drm metadata event object with failure details
 */
void AampLicensePreFetcher::NotifyDrmFailure(LicensePreFetchObjectPtr fetchObj, DrmMetaDataEventPtr event)
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );

	AAMPTuneFailure failure = event->getFailure();
	long responseCode = event->getResponseCode();
	bool isRetryEnabled = false;
	bool selfAbort = (failure == AAMP_TUNE_DRM_SELF_ABORT);
	bool skipErrorEvent = false;
	if (fetchObj)
	{
		try
		{
			// Check if license already acquired for this track, then skip the error event broadcast
			if (mTrackStatus.at(fetchObj->mType) == true)
			{
				skipErrorEvent = true;
				AAMPLOG_WARN("Skipping DRM failure event, since license already acquired for this track type:%d", fetchObj->mType);
			}
		}
		catch (std::out_of_range const& exc)
		{
			AAMPLOG_ERR("Unable to check the mTrackStatus for type:%d, caught exception: %s", fetchObj->mType, exc.what());
		}

		if (!skipErrorEvent)
		{
			// Check if the mFetchQueue has a request for this track type queued
			// TODO: Check for race conditions between license acquisition and adding into fetch queue
			std::lock_guard<std::mutex>lock(mQMutex);
			for (auto obj : mFetchQueue)
			{
				if (obj->mType == fetchObj->mType)
				{
					skipErrorEvent = true;
					AAMPLOG_WARN("Skipping DRM failure event, since a pending request exists for this track type:%d", fetchObj->mType);
					break;
				}
			}
		}
	}

	if (skipErrorEvent)
	{
		mFetchInstance->UpdateFailedDRMStatus(fetchObj.get());
	}
	else
	{
		if (!selfAbort)
		{
			isRetryEnabled =   (failure != AAMP_TUNE_AUTHORISATION_FAILURE)
						&& (failure != AAMP_TUNE_LICENCE_REQUEST_FAILED)
						&& (failure != AAMP_TUNE_LICENCE_TIMEOUT)
						&& (failure != AAMP_TUNE_DEVICE_NOT_PROVISIONED)
						&& (failure != AAMP_TUNE_HDCP_COMPLIANCE_ERROR);
		}

		mPrivAAMP->SendDrmErrorEvent(event, isRetryEnabled);
		mPrivAAMP->profiler.SetDrmErrorCode((int)failure);
		mPrivAAMP->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int)failure);
	}
}

/**
 * @brief Creates a DRM session for a content protection
 * 
 * @param fetchObj LicensePreFetchObject shared_ptr
 * @return true if successfully created DRM session
 * @return false if failed
 */
bool AampLicensePreFetcher::CreateDRMSession(LicensePreFetchObjectPtr fetchObj)
{
	FN_TRACE_F_LIC_PREFETCH( __FUNCTION__ );

	bool ret = false;
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	bool isSecClientError = true;
#else
	bool isSecClientError = false;
#endif
	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError);

	if (mPrivAAMP == nullptr)
	{
		AAMPLOG_ERR("no PrivateInstanceAAMP instance available");
		return ret;
	}
	if (fetchObj->mHelper == nullptr)
	{
		AAMPLOG_ERR("Failed DRM Session Creation,  no helper");
		NotifyDrmFailure(fetchObj, e);
		return ret;
	}
#if defined(AAMP_MPD_DRM) || defined(AAMP_HLS_DRM) || defined(USE_OPENCDM)
	AampDRMSessionManager* sessionManger = mPrivAAMP->mDRMSessionManager;

	if (sessionManger == nullptr)
	{
		AAMPLOG_ERR("no mPrivAAMP->mDrmSessionManager available");
		return ret;
	}
	mPrivAAMP->setCurrentDrm(fetchObj->mHelper);

	mPrivAAMP->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
	AampDrmSession *drmSession = sessionManger->createDrmSession(fetchObj->mHelper, e, mPrivAAMP, fetchObj->mType);

	if(NULL == drmSession)
	{
		AAMPLOG_ERR("Failed DRM Session Creation for systemId = %s", fetchObj->mHelper->getUuid().c_str());
		NotifyDrmFailure(fetchObj, e);
	}
	else
	{
		ret = true;
		if(e->getAccessStatusValue() != 3)
		{
			AAMPLOG_INFO("Sending DRMMetaData");
			mPrivAAMP->SendDRMMetaData(e);
		}
	}
	mPrivAAMP->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
#endif
	if(mPrivAAMP->mIsFakeTune)
	{
		mPrivAAMP->SetState(eSTATE_COMPLETE);
		mPrivAAMP->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS));
	}
	return ret;
}
