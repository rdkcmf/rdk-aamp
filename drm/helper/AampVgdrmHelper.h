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
#ifndef _AAMP_VGDRM_HELPER_H
#define _AAMP_VGDRM_HELPER_H

#include <memory>
#include <set>

#include "AampDrmHelper.h"

#if USE_ION_MEMORY
#include "AampIonMemorySystem.h"
#elif USE_SHARED_MEMORY
#include "AampSharedMemorySystem.h"
#else
#error "No memory model for interchange"
#endif

/**
 * @class AampVgdrmHelper
 * @brief Handles the operation for Vg DRM 
 */
class AampVgdrmHelper : public AampDrmHelper
{
public:
	friend class AampVgdrmHelperFactory;

	const uint32_t TEN_SECONDS = 10000U;

	virtual const std::string& ocdmSystemId() const;

	void createInitData(std::vector<uint8_t>& initData) const;

	bool parsePssh(const uint8_t* initData, uint32_t initDataLen);

	bool isClearDecrypt() const { return true; }

	bool isHdcp22Required() const { return true; }

	uint32_t keyProcessTimeout() const { return TEN_SECONDS; }
        uint32_t licenseGenerateTimeout() const { return TEN_SECONDS; }

	void getKey(std::vector<uint8_t>& keyID) const;

	virtual int getDrmCodecType() const { return CODEC_TYPE; }

	bool isExternalLicense() const { return true; };

	void generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const;

	AAMPMemorySystem* getMemorySystem() override { return &memorySystem; };

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; };

	AampVgdrmHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) : AampDrmHelper(drmInfo, logObj), mPsshStr(), memorySystem(logObj) {}
	~AampVgdrmHelper() { }

private:
	static const std::string VGDRM_OCDM_ID;
	const std::string FRIENDLY_NAME{"VGDRM"};
	const int CODEC_TYPE{4};
	const int KEY_ID_OFFSET{12};
	const int KEY_PAYLOAD_OFFSET{14};
	const int BASE_16{16};
	std::string mPsshStr;
#if USE_ION_MEMORY
	AampIonMemorySystem memorySystem;
#elif USE_SHARED_MEMORY
	AampSharedMemorySystem memorySystem;
#endif
};

/**
 * @class AampVgdrmHelperFactory 
 * @brief Helps to operate Vg DRM
 */
class AampVgdrmHelperFactory : public AampDrmHelperFactory
{
	std::shared_ptr<AampDrmHelper> createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj=NULL) const;

	void appendSystemId(std::vector<std::string>& systemIds) const;

	bool isDRM(const struct DrmInfo& drmInfo) const;

private:
	const std::string VGDRM_UUID{"A68129D3-575B-4F1A-9CBA-3223846CF7C3"};

	const std::set<std::string> VGDRM_URI_START = {
		"80701500000810",
		"81701500000810",
		"8070110000080C",
		"8170110000080C",
		"8070110000080c",  /* Lowercase version of above to avoid needing to do case insensative comparison  */
		"8170110000080c"
	};
};

#endif //_AAMP_VGDRM_HELPER_H
