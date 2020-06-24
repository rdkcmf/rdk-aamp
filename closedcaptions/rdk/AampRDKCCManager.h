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

#include <string>

class AampRDKCCManager
{
public:
	/**
	 * @brief Get the singleton instance
	 *
	 * @return AampRDKCCManager - singleton instance
	 */
	static AampRDKCCManager * GetInstance();

	/**
	 * @brief Initialize CC resource.
	 *
	 * @param[in] handle - decoder handle
	 * @return int - 0 on sucess, -1 on failure
	 */
	int Start(void *handle);

	/**
	 * @brief Release CC resources
	 */
	void Stop(void);

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
	 * @brief Set CC track
	 *
	 * @param[in] track - CC track to be selected
	 * @return int - 0 on success, -1 on failure
	 */
	int SetTrack(const std::string &track);

	/**
	 * @brief Get current CC track
	 *
	 * @return std::string - current CC track
	 */
	const std::string &GetTrack() { return mTrack; }

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

private:
	/**
	 * @brief Constructor
	 */
	AampRDKCCManager();

	/**
	 * @brief Destructor
	 */
	~AampRDKCCManager();

        AampRDKCCManager(const AampRDKCCManager&) = delete;
        AampRDKCCManager& operator=(const AampRDKCCManager&) = delete;

public:
	static AampRDKCCManager *mInstance; /**< Singleton instance */

private:
	void *mCCHandle; /**< Decoder handle for intializing CC resources */
	bool mEnabled; /**< true if CC rendering enabled, false otherwise */
	std::string mTrack; /**< CC track */
	std::string mOptions; /**< CC rendering styles */
};

#endif /* __AAMP_RDK_CC_MANAGER_H__ */
