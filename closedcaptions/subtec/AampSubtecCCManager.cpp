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
 *  @file AampSubtecCCManager.cpp
 *
 *  @brief Impl of Subtec ClosedCaption integration layer
 *
 */

#include "AampSubtecCCManager.h"

#include "priv_aamp.h" // Included for AAMPLOG
//TODO: Fix cyclic dependency btw GlobalConfig and AampLogManager

#include <closedcaptions/subtec/SubtecConnector.h>

/**
 * @brief Impl specific initialization code called before each public interface call
 * @return void
 */
void AampSubtecCCManager::EnsureInitialized()
{
	AAMPLOG_WARN("AampSubtecCCManager::%s %d mInitialized = %s", __FUNCTION__, __LINE__, mInitialized ? "true" : "false");
	if(not mInitialized)
	{
		AAMPLOG_WARN("AampSubtecCCManager::%s %d calling subtecConnector::initialize()", __FUNCTION__, __LINE__);
		subtecConnector::initialize();
		mInitialized = true;
	}
}

/**
 * @brief Release CC resources
 */
void AampSubtecCCManager::Release(void)
{
	Stop();
	if(mInitialized)
	{
		subtecConnector::close();
		mInitialized = false;
	}
	mTrickplayStarted = false;
}

/**
 * @brief To start CC rendering
 *
 * @return void
 */
void AampSubtecCCManager::StartRendering()
{
	subtecConnector::ccMgrAPI::ccShow();
}

/**
 * @brief To stop CC rendering
 *
 * @return void
 */
void AampSubtecCCManager::StopRendering()
{
	subtecConnector::ccMgrAPI::ccHide();
}

/**
 * @brief set digital channel with specified id
 *
 * @return CC_VL_OS_API_RESULT
 */
int AampSubtecCCManager::SetDigitalChannel(unsigned int id)
{
	const auto ret =  subtecConnector::ccMgrAPI::ccSetDigitalChannel(id);
	EnsureRendererStateConsistency();
	return ret;
}

/**
 * @brief set analog channel with specified id
 *
 * @return CC_VL_OS_API_RESULT
 */
int AampSubtecCCManager::SetAnalogChannel(unsigned int id)
{
	const auto ret =  subtecConnector::ccMgrAPI::ccSetAnalogChannel(id);
	EnsureRendererStateConsistency();
	return ret;
}

/**
 * @brief ensure mRendering is consistent with renderer state
 *
 * @return void
 */
void AampSubtecCCManager::EnsureRendererStateConsistency()
{
	if(mRendering)
	{
		StartRendering();
	}
	else
	{
		StopRendering();
	}
}
