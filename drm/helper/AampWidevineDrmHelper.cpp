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

#include <memory>
#include <iostream>

#include "AampWidevineDrmHelper.h"
#include "AampDRMutils.h"
#include "GlobalConfigAAMP.h"

static AampWidevineDrmHelperFactory widevine_helper_factory;

const std::string AampWidevineDrmHelper::WIDEVINE_OCDM_ID = "com.widevine.alpha";

bool AampWidevineDrmHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	this->mInitData.assign(initData, initData+initDataLen);

	// Extract key
	const char* keyIdBegin;
	uint8_t keyIdSize = 0u;
	uint8_t psshDataVer = initData[8];
	bool ret = false;

	AAMPLOG_INFO("%s:%d wv pssh data version - %u ", __FUNCTION__, __LINE__, psshDataVer);
	if (psshDataVer == 0)
	{
		//PSSH version 0
		//4+4+4+16(system id)+4(data size)+2(keyId size indicator + keyid size)+ keyId +
		//2 (unknown byte + content id size) + content id
		uint32_t header = 0;
		if (initData[32] == WIDEVINE_KEY_ID_SIZE_INDICATOR)
		{
			header = 33; //pssh data in comcast format
		}
		else if(initData[34] == WIDEVINE_KEY_ID_SIZE_INDICATOR)
		{
			header = 35; //pssh data in sling format
		}
		else
		{
			AAMPLOG_WARN("%s:%d wv version %d keyid indicator byte not found using default logic", __FUNCTION__, __LINE__);
			header = 33; //pssh data in comcast format
		}

		keyIdSize = initData[header];
		keyIdBegin = reinterpret_cast<const char*>(initData + header + 1);
	}
	else if (psshDataVer == 1)
	{
		//PSSH version 1
		//8 byte BMFF box header + 4 byte Full box header + 16 (system id) +
		//4(KID Count) + 16 byte KID 1 + .. + 4 byte Data Size
		//TODO : Handle multiple key Id logic, right now we are choosing only first one if have multiple key Id
		uint32_t header = WIDEVINE_DASH_KEY_ID_OFFSET;
		keyIdSize = 16;
		keyIdBegin = reinterpret_cast<const char*>(initData + header);
	}
	else
	{
		AAMPLOG_ERR("%s:%d unsupported PSSH version: %u", __FUNCTION__, __LINE__, psshDataVer);
	}

	if (keyIdSize != 0u)
	{
		AAMPLOG_INFO("%s:%d wv version %d keyIdlen: %d", __FUNCTION__, __LINE__, psshDataVer, keyIdSize);
		mKeyID.assign(keyIdBegin, keyIdBegin + keyIdSize);
		ret = true;
	}

	return ret;
}


void AampWidevineDrmHelper::setDrmMetaData(const std::string& metaData)
{
	mContentMetadata = metaData;
}


const std::string& AampWidevineDrmHelper::ocdmSystemId() const
{
	return WIDEVINE_OCDM_ID;
}

void AampWidevineDrmHelper::createInitData(std::vector<uint8_t>& initData) const
{
	initData = this->mInitData;
}

void AampWidevineDrmHelper::getKey(std::vector<uint8_t>& keyID) const
{
	keyID = this->mKeyID;
}

void AampWidevineDrmHelper::generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const
{
	licenseRequest.method = AampLicenseRequest::POST;

	if (gpGlobalConfig->wvLicenseServerURL)
	{
		licenseRequest.url = gpGlobalConfig->wvLicenseServerURL;
	}
	else
	{
		licenseRequest.url = challengeInfo.url;
	}

	licenseRequest.headers = {{"Content-Type:", {"application/octet-stream"}}};

	licenseRequest.payload.assign(reinterpret_cast<const char *>(challengeInfo.data->getData()), challengeInfo.data->getDataLength());
}

bool AampWidevineDrmHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return (((WIDEVINE_UUID == drmInfo.systemUUID) || (AampWidevineDrmHelper::WIDEVINE_OCDM_ID == drmInfo.keyFormat))
		&& ((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) || (drmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
		);
}

std::shared_ptr<AampDrmHelper> AampWidevineDrmHelperFactory::createHelper(const struct DrmInfo& drmInfo) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampWidevineDrmHelper>(drmInfo);
	}
	return NULL;
}

void AampWidevineDrmHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(WIDEVINE_UUID);
}

