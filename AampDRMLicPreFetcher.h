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
#ifndef _AAMP_LICENSE_PREFETCHER_HPP
#define _AAMP_LICENSE_PREFETCHER_HPP

#include <thread>
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include "AampDrmHelper.h"
#include "AampLogManager.h"
#include "priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"

/**
 * @brief Structure for storing the pre-fetch data
 *
 */
struct LicensePreFetchObject
{
	std::shared_ptr<AampDrmHelper> mHelper; /** drm helper for the content protection*/
	std::string mPeriodId;                  /** Period ID*/
	std::string mAdaptationId;              /** Adapatation ID*/
	MediaType mType;                        /** Stream type*/
	int mId;                                /** Object ID*/
	static int staticId;

	/**
	 * @brief Construct a new License Pre Fetch Object object
	 * 
	 * @param drmHelper AampDrmHelper shared_ptr
	 * @param periodId ID of the period to which CP belongs to
	 * @param adapId ID of the adaptation to which CP belongs to
	 * @param type media type
	 */
	LicensePreFetchObject(std::shared_ptr<AampDrmHelper> drmHelper, std::string periodId, std::string adapId, MediaType type): mHelper(drmHelper),
															mPeriodId(periodId),
															mAdaptationId(adapId),
															mType(type),
															mId(staticId++)
	{
		AAMPLOG_TRACE("Creating new LicensePreFetchObject, id:%d", mId);
	}

	/**
	 * @brief Destroy the License Pre Fetch Object object
	 * 
	 */
	~LicensePreFetchObject()
	{
		AAMPLOG_TRACE("Deleting LicensePreFetchObject, id:%d", mId);
	}

	/**
	 * @brief Compare two LicensePreFetchObject objects
	 * 
	 * @param other LicensePreFetchObject to be compared to
	 * @return true if both objects are having same info
	 * @return false otherwise
	 */
	bool compare(std::shared_ptr<LicensePreFetchObject> other) const
	{
		return ((other != nullptr) && mHelper->compare(other->mHelper) && mType == other->mType);
	}
};

using LicensePreFetchObjectPtr = std::shared_ptr<LicensePreFetchObject>;

/**
 * @brief Class for License PreFetcher module.
 * Handles the license pre-fetching responsibilities in a playback for faster tune times
 */
class AampLicensePreFetcher
{
public:
	/**
	 * @brief Default constructor disabled
	 */
	AampLicensePreFetcher() = delete;

	/**
	 * @brief Construct a new Aamp License Pre Fetcher object
	 * 
	 * @param logObj Log manager object
	 * @param aamp PrivateInstanceAAMP instance
	 * @param fetcherInstance AampLicenseFetcher instance
	 */
	AampLicensePreFetcher(AampLogManager *logObj, PrivateInstanceAAMP *aamp, AampLicenseFetcher *fetcherInstance);

	/**
	 * @brief Copy constructor disabled
	 * 
	 */
	AampLicensePreFetcher(const AampLicensePreFetcher&) = delete;

	/**
	 * @brief Assignment operator disabled
	 * 
	 */
	void operator=(const AampLicensePreFetcher&) = delete;

	/**
	 * @brief Destroy the Aamp License Pre Fetcher object
	 * 
	 */
	~AampLicensePreFetcher();

	/**
	 * @brief Initialize resources
	 * 
	 * @return true if successfully initialized
	 * @return false if error occurred
	 */
	bool Init();

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
	bool QueueContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, std::string periodId, std::string adapId, MediaType type);

	/**
	 * @brief De-initialize/free resources
	 * 
	 * @return true if success
	 * @return false if failed
	 */
	bool DeInit();

	/**
	 * @brief Set the Common Key Duration object
	 * 
	 * @param keyDuration key duration
	 */
	void SetCommonKeyDuration(int keyDuration) { mCommonKeyDuration = keyDuration; }

	/**
	 * @brief Thread for processing content protection queued using QueueContentProtection
	 * Thread will be joined when DeInit is called
	 * 
	 */
	void PreFetchThread();
private:

	/**
	 * @brief To notify DRM failure to player after proper checks
	 * 
	 * @param fetchObj object for which session creation failed
	 * @param event drm metadata event object with failure details
	 */
	void NotifyDrmFailure(LicensePreFetchObjectPtr fetchObj, DrmMetaDataEventPtr event);

	/**
	 * @brief Creates a DRM session for a content protection
	 * 
	 * @param fetchObj LicensePreFetchObject shared_ptr
	 * @return true if successfully created DRM session
	 * @return false if failed
	 */
	bool CreateDRMSession(LicensePreFetchObjectPtr fetchObj);

	std::thread mPreFetchThread;                        /** Thread for pre-fetching license*/
	std::deque<LicensePreFetchObjectPtr> mFetchQueue;   /** Queue for storing content protection objects*/
	std::mutex mQMutex;                                 /** Mutex for accessing the mFetchQueue*/
	std::condition_variable mQCond;                     /** Conditional variable to notify addition of an obj to mFetchQueue*/
	bool mPreFetchThreadStarted;                        /** Flag denotes if thread started*/
	bool mExitLoop;                                     /** Flag denotes if pre-fetch thread has to be exited*/
	int mCommonKeyDuration;                             /** Common key duration for deferred license acquistion*/
	std::array<bool, AAMP_TRACK_COUNT> mTrackStatus;    /** To mark the status of license acquistion for tracks*/

	PrivateInstanceAAMP *mPrivAAMP;                     /** PrivateInstanceAAMP instance*/
	AampLogManager* mLogObj;                            /** AampLogManager instance */
	AampLicenseFetcher *mFetchInstance;                 /** AampLicenseFetcher instance for notifying DRM session status*/
};

#endif /* _AAMP_LICENSE_PREFETCHER_HPP */
