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
 * @file AampCCManager.h
 *
 * @brief Integration layer of ClosedCaption in AAMP
 *
 */

#ifndef __AAMP_CC_MANAGER_H__
#define __AAMP_CC_MANAGER_H__

#include <string>
#include <vector>
#include "main_aamp.h"
#include "AampLogManager.h"

/**
 * @brief Different CC formats
 */
enum CCFormat
{
	eCLOSEDCAPTION_FORMAT_608 = 0,
	eCLOSEDCAPTION_FORMAT_708,
	eCLOSEDCAPTION_FORMAT_DEFAULT
};


class AampCCManagerBase
{
public:
	/**
	 * @brief Initialize CC resource.
	 *
	 * @param[in] handle - decoder handle
	 * @return int - 0 on sucess, -1 on failure
	 */
	int Init(void *handle);

	/**
	* @brief Gets Handle or ID, Every client using subtec must call GetId  in the begining , save id, which is required for Release funciton.
	* @return int -  unique ID
	*/
	virtual int GetId() { return 0; };

	/**
	 * @brief Release CC resources
	 * @param[in] id -  returned from GetId function
	 */
	virtual void Release(int iID) = 0;

	/**
	 * @brief Enable/disable CC rendering
	 *
	 * @param[in] enable - true to enable CC rendering
	 * @return int - 0 on success, -1 on failure
	 */
	int SetStatus(bool enable);

	/**
	 * @brief Get CC rendering status
	 *
	 * @return bool - true if enabled, false otherwise
	 */
	bool GetStatus() { return mEnabled; };

	/**
	 * @brief Get current CC track
	 *
	 * @return std::string - current CC track
	 */
	const std::string &GetTrack() { return mTrack; }

	/**
	 * @brief Set CC track
	 *
	 * @param[in] track - CC track to be selected
	 * @param[in] format - force track to 608/708 or default
	 * @return int - 0 on success, -1 on failure
	 */
	int SetTrack(const std::string &track, const CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT);

	/**
	 * @brief Set CC styles for rendering
	 *
	 * @param[in] options - rendering style options
	 * @return int - 0 on success, -1 on failure
	 */
	int SetStyle(const std::string &options);

	/**
	 * @brief Get current CC styles
	 *
	 * @return std::string - current CC options
	 */
	//TODO: Default values can't be queried
	const std::string &GetStyle() { return mOptions; }

	/**
	 * @brief To enable/disable CC when trickplay starts/ends
	 *
	 * @param[in] enable - true when trickplay starts, false otherwise
	 * @return void
	 */
	void SetTrickplayStatus(bool enable);

	/**
	 * @brief To enable/disable CC when parental control locked/unlocked
	 *
	 * @param[in] locked - true when parental control lock enabled, false otherwise
	 * @return void
	 */
	void SetParentalControlStatus(bool locked);

	/**
	 * @brief To restore cc state after new tune
	 *
	 * @return void
	 */
	void RestoreCC();

	virtual ~AampCCManagerBase(){ };

	/**
	 * @brief update stored list of text tracks
	 *
	 * @param[in] newTextTracks - list of text tracks to store
	 * @return void
	 */
	void updateLastTextTracks(const std::vector<TextTrackInfo>& newTextTracks) { mLastTextTracks = newTextTracks; }

	/**
	 * @brief Get list of text tracks
	 *
	 * @return const std::vector<TextTrackInfo>& - list of text tracks
	 */
	const std::vector<TextTrackInfo>& getLastTextTracks() const { return mLastTextTracks; }

	void SetLogger(AampLogManager *logObj) { mLogObj = logObj;}

protected:
	/**
	 * @brief To start CC rendering
	 *
	 * @return void
	 */
	virtual void StartRendering() = 0;

	/**
	 * @brief To stop CC rendering
	 *
	 * @return void
	 */
	virtual void StopRendering() = 0;

	/**
	 * @brief Impl specific initialization code called before each public interface call
	 * @return void
	 */
	virtual void EnsureInitialized(){};

	/**
	 * @brief Impl specific initialization code for HAL
	 * @return void
	 */
	virtual void EnsureHALInitialized(){};

	/**
	 * @brief Impl specific initialization code for Communication with renderer
	 * @return void
	 */
	virtual void EnsureRendererCommsInitialized(){};

	/**
	 * @brief Impl specific initialization code called once in Init() function
	 *
	 * @return 0 - success, -1 - failure
	 */
	virtual int Initialize(void *handle){return 0;}

	/**
	 * @brief set digital channel with specified id
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	virtual int SetDigitalChannel(unsigned int id) = 0;

	/**
	 * @brief set analog channel with specified id
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	virtual int SetAnalogChannel(unsigned int id) = 0;

	/**
	 * @brief validate mCCHandle
	 *
	 * @return bool
	 */
	virtual bool CheckCCHandle() const {return true;}

	/**
	 * @brief To start CC rendering
	 *
	 * @return void
	 */
	void Start();

	/**
	 * @brief To stop CC rendering
	 *
	 * @return void
	 */
	void Stop();

	
	std::string mOptions{}; /**< CC rendering styles */
	std::string mTrack{}; /**< CC track */
	std::vector<TextTrackInfo> mLastTextTracks{};
	bool mEnabled{false}; /**< true if CC rendering enabled, false otherwise */
	bool mTrickplayStarted{false}; /** If a trickplay is going on or not */
	bool mParentalCtrlLocked{false}; /** If Parental Control lock enabled on not */
	AampLogManager *mLogObj{NULL};
};

class AampCCManager
{
public:
	/**
	 * @brief Get the singleton instance
	 *
	 * @return AampCCManager - singleton instance
	 */
	static AampCCManagerBase * GetInstance();

	/**
	 * @brief Destroy instance
	 *
	 * @return void
	 */
	static void DestroyInstance();

private:
	static AampCCManagerBase *mInstance; /**< Singleton instance */
};


#endif /* __AAMP_CC_MANAGER_H__ */
