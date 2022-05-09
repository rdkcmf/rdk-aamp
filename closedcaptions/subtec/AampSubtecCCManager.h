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
	 * @brief Release CC resources
	 * @param[in] id -  returned from GetId function
	 */
	void Release(int iID) override;

	/**
	* @brief Gets Handle or ID, Every client using subtec must call GetId  in the begining , save id, which is required for Release funciton.
	* @return int -  unique ID
	*/
	virtual int GetId();

	/**
	 * @brief Constructor
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
	 * @brief To start CC rendering
	 *
	 * @return void
	 */
	void StartRendering() override;

	/**
	 * @brief To stop CC rendering
	 *
	 * @return void
	 */
	void StopRendering() override;

	/**
	 * @brief Impl specific initialization code called before each public interface call
	 * @return void
	 */
	void EnsureInitialized() override;

	/**
	 * @brief Impl specific initialization code for HAL
	 * @return void
	 */
	void EnsureHALInitialized() override;

	/**
	 * @brief Impl specific initialization code for Communication with renderer
	 * @return void
	 */
	void EnsureRendererCommsInitialized() override;

	/**
	 * @brief set digital channel with specified id
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetDigitalChannel(unsigned int id) override;
	/**
	 * @brief set analog channel with specified id
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetAnalogChannel(unsigned int id) override;

	/**
	 * @brief ensure mRendering is consistent with renderer state
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
