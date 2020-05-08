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
#ifndef _AAMP_WIDEVINE_DRM_HELPER_H
#define _AAMP_WIDEVINE_DRM_HELPER_H

#include <memory>

#include "AampDrmHelper.h"

class AampWidevineDrmHelper: public AampDrmHelper
{
public:
	friend class AampWidevineDrmHelperFactory;

	virtual const std::string& ocdmSystemId() const;

	void createInitData(std::vector<uint8_t>& initData) const;

	bool parsePssh(const uint8_t* initData, uint32_t initDataLen);

	bool isClearDecrypt() const { return false; }

	bool isExternalLicense() const { return false; };

	void getKey(std::vector<uint8_t>& keyID) const;

	const std::string& getDrmMetaData() const {return mContentMetadata;}

	void setDrmMetaData(const std::string& metaData);

	virtual int getDrmCodecType() const { return CODEC_TYPE; }

	void generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const;

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; };

	AampWidevineDrmHelper(const struct DrmInfo& drmInfo) : AampDrmHelper(drmInfo), FRIENDLY_NAME("Widevine"),
		CODEC_TYPE(1), WIDEVINE_KEY_ID_SIZE_INDICATOR(0x12), WIDEVINE_DASH_KEY_ID_OFFSET(32u),
		WIDEVINE_CONTENT_METADATA_OFFSET(28u), mInitData(), mKeyID(), mContentMetadata()
	{}

	~AampWidevineDrmHelper() { }

private:
	static const std::string WIDEVINE_OCDM_ID;
	const std::string FRIENDLY_NAME;
	const int CODEC_TYPE;
	const uint8_t WIDEVINE_KEY_ID_SIZE_INDICATOR;
	const size_t WIDEVINE_DASH_KEY_ID_OFFSET; // Offset in the PSSH to find the key ID for DASH
	const uint8_t WIDEVINE_CONTENT_METADATA_OFFSET; // Offset of content metadata in Widevine PSSH data

	std::vector<uint8_t> mInitData;
	std::vector<uint8_t> mKeyID;
	std::string mContentMetadata;
};

class AampWidevineDrmHelperFactory : public AampDrmHelperFactory
{
	std::shared_ptr<AampDrmHelper> createHelper(const struct DrmInfo& drmInfo) const;

	void appendSystemId(std::vector<std::string>& systemIds) const;

	bool isDRM(const struct DrmInfo& drmInfo) const;

private:
	const std::string WIDEVINE_UUID{"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"};
};


#endif //_AAMP_WIDEVINE_DRM_HELPER_H
