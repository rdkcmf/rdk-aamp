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
 * @file AampVgdrmHelper.cpp
 * @brief Handles operations for Vg DRM
 */


#include <memory>
#include <iostream>

#include "AampVgdrmHelper.h"
#include "AampJsonObject.h"
#include "AampConfig.h"

static AampVgdrmHelperFactory vgdrm_helper_factory;

const std::string AampVgdrmHelper::VGDRM_OCDM_ID = "net.vgdrm";

const std::string& AampVgdrmHelper::ocdmSystemId() const
{
	return VGDRM_OCDM_ID;
}

void AampVgdrmHelper::createInitData(std::vector<uint8_t>& initData) const
{
	AampJsonObject jsonInitDataObj;

	if (jsonInitDataObj.add(std::string("initData"), mDrmInfo.initData) &&
		jsonInitDataObj.add(std::string("uri"), mDrmInfo.keyURI) &&
		(mPsshStr.empty() || jsonInitDataObj.add(std::string("pssh"), mPsshStr)))
	{
		// Note: PSSH may be empty, all other strings are expected to be present
		jsonInitDataObj.print(initData);
	}
}

bool AampVgdrmHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	//TODO
	return true;
}

void AampVgdrmHelper::getKey(std::vector<uint8_t>& keyID) const
{
	if ((mDrmInfo.keyURI.length() != 40) && (mDrmInfo.keyURI.length() != 48))
	{
		AAMPLOG_ERR("Invalid key URI: %s", mDrmInfo.keyURI);
		return;
	}

	/* Each byte is represented as 2 characters in the URI */
	int keyLen = std::stoi(mDrmInfo.keyURI.substr(KEY_ID_OFFSET, 2), 0, BASE_16); /* no CRC32 */

	if ((KEY_PAYLOAD_OFFSET + (2 * keyLen)) > mDrmInfo.keyURI.length())
	{
		AAMPLOG_ERR("Invalid key length extracted from URI: %d", keyLen);
		return;
	}

	keyID.clear();
	keyID.reserve(keyLen);
	for (int i = 0; i < keyLen; i++)
	{
		keyID.push_back(std::stoi(mDrmInfo.keyURI.substr(KEY_PAYLOAD_OFFSET + (2 * i), 2), 0, BASE_16));
	}
}

void AampVgdrmHelper::generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const
{
	licenseRequest.method = AampLicenseRequest::DRM_RETRIEVE;
	licenseRequest.url = "";
	licenseRequest.payload = "";
}

bool AampVgdrmHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	// VGDRM only supports HLS for now
	if (drmInfo.mediaFormat != eMEDIAFORMAT_HLS) return false;
	
	if (VGDRM_UUID == drmInfo.keyFormat) return true;
	if (AampVgdrmHelper::VGDRM_OCDM_ID == drmInfo.keyFormat) return true;

	/* For legacy DMR */
	if (((drmInfo.keyURI.length() == 40) ||
		 (drmInfo.keyURI.length() == 48)) &&
		 (drmInfo.keyURI.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos))
	{
		if (VGDRM_URI_START.count(drmInfo.keyURI.substr(0,14)) != 0) return true;
	}

	return false;
}

std::shared_ptr<AampDrmHelper> AampVgdrmHelperFactory::createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampVgdrmHelper>(drmInfo, logObj);
	}
	return NULL;
}

void AampVgdrmHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(VGDRM_UUID);
}

