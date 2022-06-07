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
 * @file AampSubtecCCManager.h
 *
 * @brief Integration layer of Subtec ClosedCaption in AAMP
 *
 */

#ifndef __AAMP_SUBTEC_CC_MANAGER_H__
#define __AAMP_SUBTEC_CC_MANAGER_H__

#include <closedcaptions/AampCCManager.h>

#include <string>
#include <set>
#include <mutex>
#include <closedcaptions/subtec/SubtecConnector.h>


/**
 * @class AampSubtecCCManager
 * @brief Handling Subtec CC operation
 */

class AampSubtecCCManager : public AampCCManagerBase
{
public:

	/**
	 * @fn Release
	 * @param[in] id -  returned from GetId function
	 */
	void Release(int iID) override;

	/**
	 * @fn GetId
	 * @return int -  unique ID
	 */
	virtual int GetId();

	/**
	 * @fn AampSubtecCCManager
	 */
	AampSubtecCCManager();

	/**
	 * @brief Destructor
	 */
	~AampSubtecCCManager() = default;

	AampSubtecCCManager(const AampSubtecCCManager&) = delete;
	AampSubtecCCManager& operator=(const AampSubtecCCManager&) = delete;

private:
	/**
	 * @fn StartRendering
	 *
	 * @return void
	 */
	void StartRendering() override;

	/**
	 * @fn StopRendering
	 *
	 * @return void
	 */
	void StopRendering() override;

	/**
	 * @fn EnsureInitialized
	 * @return void
	 */
	void EnsureInitialized() override;

	/**
	 * @fn EnsureHALInitialized
	 * @return void
	 */
	void EnsureHALInitialized() override;

	/**
	 * @fn EnsureRendererCommsInitialized
	 * @return void
	 */
	void EnsureRendererCommsInitialized() override;

	/**
	 * @fn SetDigitalChannel
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetDigitalChannel(unsigned int id) override;
	/**
	 * @fn SetAnalogChannel
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetAnalogChannel(unsigned int id) override;

	/**
	 * @fn EnsureRendererStateConsistency
	 *
	 * @return void
	 */
	void EnsureRendererStateConsistency();


private:
	bool mRendererInitialized{false};
	bool mHALInitialized{false};
    std::mutex mIdLock;
    int mId{0};
    std::set<int> mIdSet;
};

#endif /* __AAMP_SUBTEC_CC_MANAGER_H__ */
