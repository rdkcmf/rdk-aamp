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

#include "AampClearKeyHelper.h"
#include "AampJsonObject.h"

#include "AampConfig.h"
#include "AampConstants.h"

static AampClearKeyHelperFactory clearkey_helper_factory;

const std::string AampClearKeyHelper::CLEARKEY_OCDM_ID = "org.w3.clearkey";

const std::string& AampClearKeyHelper::ocdmSystemId() const
{
	return CLEARKEY_OCDM_ID;
}

void AampClearKeyHelper::createInitData(std::vector<uint8_t>& initData) const
{
	// For DASH the init data should have been extracted from the PSSH
	// For HLS, we need to construct it
	if (mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
	{
		initData = this->mInitData;
	}
	else
	{
		AampJsonObject jsonInitDataObj;
		std::vector<std::string> keyIds = {CLEARKEY_KEY_ID};

		if (jsonInitDataObj.add("kids", keyIds))
		{
			jsonInitDataObj.print(initData);
		}
	}
}

bool AampClearKeyHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	this->mInitData.assign(initData, initData + initDataLen);

	mKeyID.assign(initData + CLEARKEY_DASH_KEY_ID_OFFSET, initData + CLEARKEY_DASH_KEY_ID_OFFSET + CLEARKEY_DASH_KEY_ID_LEN);

	return true;
}

void AampClearKeyHelper::getKey(std::vector<uint8_t>& keyID) const
{
	// For DASH the key should have been extracted from the PSSH
	// For HLS, we return a fixed key, which we also place in the init data
	if (mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
	{
		keyID = this->mKeyID;
	}
	else
	{
		keyID.clear();
		(void)keyID.insert(keyID.begin(), CLEARKEY_KEY_ID.begin(), CLEARKEY_KEY_ID.end());
	}
}

void AampClearKeyHelper::generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const
{
	licenseRequest.method = AampLicenseRequest::POST;

	if(licenseRequest.url.empty())
        {
		if (!mDrmInfo.keyURI.empty())
		{
			std::string keyURI;
			aamp_ResolveURL(keyURI, mDrmInfo.manifestURL, mDrmInfo.keyURI.c_str(), mDrmInfo.bPropagateUriParams);
			licenseRequest.url = keyURI;
		}
		else
		{
			licenseRequest.url = challengeInfo.url;
		}
        }

	licenseRequest.payload.assign(reinterpret_cast<const char *>(challengeInfo.data->getData()), challengeInfo.data->getDataLength());
}

void AampClearKeyHelper::transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const
{
	// HLS requires the returned key to be transformed into a JWK.
	// For DASH it will already be in JWK format
	if (mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
	{
		std::vector<uint8_t> licenseResponseData(licenseResponse->getData(),
												 licenseResponse->getData() + licenseResponse->getDataLength());

		std::vector<uint8_t> keyId(CLEARKEY_KEY_ID.begin(), CLEARKEY_KEY_ID.end());

		// Construct JSON Web Key (JWK)
		AampJsonObject keyInstance;
		keyInstance.add("alg", "cbc"); // Hard coded to cbc for now
		keyInstance.add("k", licenseResponseData, AampJsonObject::ENCODING_BASE64_URL);
		keyInstance.add("kid", keyId, AampJsonObject::ENCODING_BASE64_URL);

		std::vector<AampJsonObject*> values = {&keyInstance};

		AampJsonObject keyObj;
		keyObj.add("keys", values);
		std::string printedJson = keyObj.print();

		licenseResponse->setData((unsigned char*)printedJson.c_str(), printedJson.length());
	}
}

bool AampClearKeyHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return ((drmInfo.method == eMETHOD_AES_128) &&
			((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) ||
			(drmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
		);
}

std::shared_ptr<AampDrmHelper> AampClearKeyHelperFactory::createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampClearKeyHelper>(drmInfo,logObj);
	}
	return NULL;
}

void AampClearKeyHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(CLEARKEY_UUID);
}
