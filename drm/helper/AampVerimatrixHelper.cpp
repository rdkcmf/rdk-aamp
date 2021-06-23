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

#include "AampVerimatrixHelper.h"
#include "AampDRMutils.h"
#include "AampConfig.h"
#include "AampConstants.h"

#define KEYURL_TAG_START "<KeyUrl><![CDATA["
#define KEYURL_TAG_END "]]></KeyUrl>"

static AampVerimatrixHelperFactory verimatrix_helper_factory;

const std::string AampVerimatrixHelper::VERIMATRIX_OCDM_ID = "com.verimatrix.ott";

bool AampVerimatrixHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	this->mInitData.assign(initData, initData+initDataLen);

	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
	{
		logprintf("mediaFormat is not DASH");
		return true;
	}

	if(initDataLen < VERIMATRIX_PSSH_DATA_POSITION)
	{
		logprintf("initDataLen less than %d", VERIMATRIX_PSSH_DATA_POSITION);
		return false;
	}

	initData += VERIMATRIX_PSSH_DATA_POSITION;
	initDataLen -= VERIMATRIX_PSSH_DATA_POSITION;
	char *init = new char[initDataLen + 1];
	memcpy(init, initData, initDataLen);
	init[initDataLen] = 0;

	std::string pssh(init);
	delete []init;

	logprintf("pssh %s", pssh.c_str());

	size_t sp = pssh.find(KEYURL_TAG_START);
	size_t ep = pssh.find(KEYURL_TAG_END);
	if((sp == std::string::npos) || (ep == std::string::npos))
	{
		logprintf("not found KeyUrl TAG");
		return false;
	}

	sp += strlen(KEYURL_TAG_START);

	std::string keyfile = pssh.substr(sp, ep - sp);
	logprintf("keyfile %s", keyfile.c_str());

	mKeyID.assign((uint8_t *)keyfile.c_str(), (uint8_t *)keyfile.c_str() + keyfile.length());

	return true;
}


void AampVerimatrixHelper::setDrmMetaData(const std::string& metaData)
{
	mContentMetadata = metaData;
}

const std::string& AampVerimatrixHelper::ocdmSystemId() const
{
	return VERIMATRIX_OCDM_ID;
}

void AampVerimatrixHelper::createInitData(std::vector<uint8_t>& initData) const
{
	const char *init;

	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
		init = "{\"contentType\" : \"HLS\"}";
	else if(mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
		init = "{\"contentType\" : \"DASH\"}";
	else
		logprintf("unknown mediaFormat %d", mDrmInfo.mediaFormat);

	initData.assign((uint8_t *)init, (uint8_t *)init + strlen(init));
}

void AampVerimatrixHelper::getKey(std::vector<uint8_t>& keyID) const
{
	keyID.clear();
	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
		keyID.insert(keyID.begin(), mDrmInfo.keyURI.begin(), mDrmInfo.keyURI.end());
	else if(mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
		keyID.insert(keyID.begin(), mKeyID.begin(), mKeyID.end());
	else
		logprintf("unknown mediaFormat %d", mDrmInfo.mediaFormat);
}

void AampVerimatrixHelper::generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const
{
	licenseRequest.method = AampLicenseRequest::DRM_RETRIEVE;
	licenseRequest.url = "";
	licenseRequest.payload = "";
}

void AampVerimatrixHelper::transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const
{
	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
		licenseResponse->setData((unsigned char*)mDrmInfo.keyURI.c_str(), mDrmInfo.keyURI.length());
	else if(mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
		licenseResponse->setData((unsigned char*)mKeyID.data(), mKeyID.size());
	else
		logprintf("unknown mediaFormat %d", mDrmInfo.mediaFormat);
}

bool AampVerimatrixHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return (((VERIMATRIX_UUID == drmInfo.systemUUID) || (AampVerimatrixHelper::VERIMATRIX_OCDM_ID == drmInfo.keyFormat))
		&& ((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) || (drmInfo.mediaFormat == eMEDIAFORMAT_HLS))
		);
}

std::shared_ptr<AampDrmHelper> AampVerimatrixHelperFactory::createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampVerimatrixHelper>(drmInfo, logObj);
	}
	return NULL;
}

void AampVerimatrixHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(VERIMATRIX_UUID);
}
