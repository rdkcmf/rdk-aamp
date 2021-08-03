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
 * @file AampRDKCCManager.h
 *
 * @brief Integration layer of RDK ClosedCaption in AAMP
 *
 */

#ifndef __AAMP_RDK_CC_MANAGER_H__
#define __AAMP_RDK_CC_MANAGER_H__

#include <closedcaptions/AampCCManager.h>

#include <string>
#include "vlCCConstants.h"


class AampRDKCCManager : public AampCCManagerBase
{
public:

	/**
	 * @brief Release CC resources
	 */
	void Release(void) override;

	/**
	 * @brief Constructor
	 */
	AampRDKCCManager() = default;

	/**
	 * @brief Destructor
	 */
	~AampRDKCCManager() =default;

	AampRDKCCManager(const AampRDKCCManager&) = delete;
	AampRDKCCManager& operator=(const AampRDKCCManager&) = delete;

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
	 * @brief Impl specific initialization code called once in Init() function
	 *
	 * @return 0 - success, -1 - failure
	 */
	int Initialize(void *handle) override;

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
	 * @brief validate mCCHandle
	 *
	 * @return bool
	 */
	bool CheckCCHandle() const override{return mCCHandle!=NULL;}


	void *mCCHandle{nullptr}; /**< Decoder handle for intializing CC resources */

};

#endif /* __AAMP_RDK_CC_MANAGER_H__ */
