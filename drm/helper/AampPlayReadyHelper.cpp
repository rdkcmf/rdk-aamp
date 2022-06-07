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

/**
 * @file AampPlayReadyHelper.cpp
 * @brief play Ready DRM helper Engine
 */

#include <uuid/uuid.h>

#include "AampPlayReadyHelper.h"
#include "AampJsonObject.h"
#include "AampDRMutils.h"
#include "AampConstants.h"

#include "_base64.h"

#define KEYID_TAG_START "<KID>"
#define KEYID_TAG_END "</KID>"
#define PLAYREADY_VERSION "4.0.0.0"

static AampPlayReadyHelperFactory playready_helper_factory;

const std::string AampPlayReadyHelper::PLAYREADY_OCDM_ID = "com.microsoft.playready";

const std::string& AampPlayReadyHelper::ocdmSystemId() const
{
	return PLAYREADY_OCDM_ID;
}

void AampPlayReadyHelper::createInitData(std::vector<uint8_t>& initData) const
{
	initData = this->mInitData;
}

bool AampPlayReadyHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	this->mInitData.assign(initData, initData + initDataLen);
	int keyIdLen = 0;
	bool res = false;
	// Extract key
	unsigned char *keydata = aamp_ExtractDataFromPssh(reinterpret_cast<const char*>(initData),
			initDataLen, KEYID_TAG_START, KEYID_TAG_END, &keyIdLen, PLAYREADY_VERSION);
	//AAMPLOG_INFO("pr keyid: %s keyIdlen: %d", keydata, keyIdLen);
	if (keydata)
	{
		size_t decodedDataLen = 0;
		unsigned char* decodedKeydata = base64_Decode((const char *)keydata, &decodedDataLen);

		if (decodedDataLen != PLAYREADY_DECODED_KEY_ID_LEN)
		{
			AAMPLOG_ERR("Invalid key size found while extracting PR Decoded-KeyID-Length: %d (PR KeyID: %s  KeyID-Length: %d)", decodedDataLen, keydata, keyIdLen);
		}
		else
		{
			unsigned char swappedKeydata[PLAYREADY_DECODED_KEY_ID_LEN] = {0};
			aamp_ConvertEndianness(decodedKeydata, swappedKeydata);
			unsigned char keyId[PLAYREADY_KEY_ID_LEN] = {0};
			uuid_t *keyiduuid = (uuid_t*)swappedKeydata;
			uuid_unparse_lower(*keyiduuid, reinterpret_cast<char*>(keyId));
			mKeyID.assign(keyId, keyId + PLAYREADY_KEY_ID_LEN);
			res = true;
		}

		free(decodedKeydata);
		free(keydata);
	}

	// Extract content metadata. May not be present
	int drmMetaDataLen = 0;
	unsigned char *drmMetaData = aamp_ExtractDataFromPssh(reinterpret_cast<const char*>(initData), initDataLen,
			DRM_METADATA_TAG_START, DRM_METADATA_TAG_END, &drmMetaDataLen, PLAYREADY_VERSION);
	if (drmMetaData)
	{
		mContentMetaData.assign(reinterpret_cast<char *>(drmMetaData), drmMetaDataLen);
		free(drmMetaData);
	}

	return res;
}

void AampPlayReadyHelper::setDrmMetaData(const std::string& metaData)
{
	if (mContentMetaData.empty())
	{
		mContentMetaData = metaData;
	}
}

void AampPlayReadyHelper::getKey(std::vector<uint8_t>& keyID) const
{
	keyID = this->mKeyID;
}

void AampPlayReadyHelper::generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const
{
	licenseRequest.method = AampLicenseRequest::POST;

	if (licenseRequest.url.empty())
	{
		licenseRequest.url = challengeInfo.url;
	}

	licenseRequest.headers = {{"Content-Type:", {"text/xml; charset=utf-8"}}};

	if (!mContentMetaData.empty())
	{
		std::vector<uint8_t> challengeData(reinterpret_cast<const char*>(challengeInfo.data->getData().c_str()),reinterpret_cast<const char*>(challengeInfo.data->getData().c_str()) + challengeInfo.data->getDataLength());

		AampJsonObject comChallengeObj;
		comChallengeObj.add("keySystem", "playReady");
		comChallengeObj.add("mediaUsage", "stream");
		comChallengeObj.add("licenseRequest", challengeData, AampJsonObject::ENCODING_BASE64);
		comChallengeObj.add("contentMetadata", mContentMetaData, AampJsonObject::ENCODING_BASE64);

		if ((!challengeInfo.accessToken.empty()) && !licenseRequest.licenseAnonymousRequest)
		{
			comChallengeObj.add("accessToken", challengeInfo.accessToken);
		}

		licenseRequest.payload = comChallengeObj.print();
	}
	else if (challengeInfo.data)
	{
	   licenseRequest.payload = challengeInfo.data->getData();
	}
}

bool AampPlayReadyHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return (((drmInfo.systemUUID == PLAYREADY_UUID) || (drmInfo.keyFormat == AampPlayReadyHelper::PLAYREADY_OCDM_ID))
		&& ((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) || (drmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
		);
}

std::shared_ptr<AampDrmHelper> AampPlayReadyHelperFactory::createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampPlayReadyHelper>(drmInfo,logObj);
	}
	return NULL;
}

void AampPlayReadyHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(PLAYREADY_UUID);
}
