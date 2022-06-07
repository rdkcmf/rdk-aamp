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
#ifndef _AAMP_PLAYREADY_HELPER_H
#define _AAMP_PLAYREADY_HELPER_H

/**
 * @file AampPlayReadyHelper.h
 * @brief Handles the operation for Play ready DRM operations
 */

#include <memory>

#include "AampDrmHelper.h"
#include "AampConfig.h"

/** 
 * @class AampPlayReadyHelper
 * @brief Handles the operation for Play ready DRM operations
 */

class AampPlayReadyHelper : public AampDrmHelper
{
public:
	friend class AampPlayReadyHelperFactory;

	const std::string& ocdmSystemId() const;

	void createInitData(std::vector<uint8_t>& initData) const;

	bool parsePssh(const uint8_t* initData, uint32_t initDataLen);

	bool isClearDecrypt() const { return false; }

	bool isHdcp22Required() const { return bOutputProtectionEnabled; }

	void setDrmMetaData( const std::string& metaData );

	void getKey(std::vector<uint8_t>& keyID) const;

	virtual int getDrmCodecType() const { return CODEC_TYPE; }

	void generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const;

	const std::string& getDrmMetaData() const {return mContentMetaData;}

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; };

	AampPlayReadyHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) : AampDrmHelper(drmInfo, logObj), FRIENDLY_NAME("PlayReady"), CODEC_TYPE(2),
		PLAYREADY_DECODED_KEY_ID_LEN(16u), PLAYREADY_KEY_ID_LEN(37u), mPsshStr(),
		mInitData(), mKeyID(), mContentMetaData()
	{}

	~AampPlayReadyHelper() {}

private:
	static const std::string PLAYREADY_OCDM_ID;
	const size_t PLAYREADY_DECODED_KEY_ID_LEN; // Expected size of base64 decoded key ID from the PSSH
	const size_t PLAYREADY_KEY_ID_LEN; // PlayReady ID key length. A NULL character is included at the end
	const std::string FRIENDLY_NAME;
	const int CODEC_TYPE;

	std::string mPsshStr;
	std::vector<uint8_t> mInitData;
	std::vector<uint8_t> mKeyID;
	std::string mContentMetaData;
};

/**
 * @class AampPlayReadyHelperFactory
 * @brief Handles operations to support play ready DRM
 */
class AampPlayReadyHelperFactory : public AampDrmHelperFactory
{
public:
	std::shared_ptr<AampDrmHelper> createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj=NULL) const;

	void appendSystemId(std::vector<std::string>& systemIds) const;

	bool isDRM(const struct DrmInfo& drmInfo) const;
};

#endif //_AAMP_PLAYREADY_HELPER_H
