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
#ifndef _AAMP_VERIMATRIX_DRM_HELPER_H
#define _AAMP_VERIMATRIX_DRM_HELPER_H

#include <memory>

#include "AampDrmHelper.h"

class AampVerimatrixHelper: public AampDrmHelper
{
public:
	friend class AampVerimatrixHelperFactory;

	virtual const std::string& ocdmSystemId() const;

	void createInitData(std::vector<uint8_t>& initData) const;

	bool parsePssh(const uint8_t* initData, uint32_t initDataLen);

	bool isClearDecrypt() const { return false; }

	bool isExternalLicense() const { return true; };

	void getKey(std::vector<uint8_t>& keyID) const;

	const std::string& getDrmMetaData() const {return mContentMetadata;}

	void setDrmMetaData(const std::string& metaData);

	virtual int getDrmCodecType() const { return CODEC_TYPE; }

	void generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const;

	void transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const;

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; };

	AampVerimatrixHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) : AampDrmHelper(drmInfo, logObj), FRIENDLY_NAME("Verimatrix"),
		CODEC_TYPE(1), VERIMATRIX_PSSH_DATA_POSITION(52),
		mInitData(), mKeyID(), mContentMetadata()
	{}

	~AampVerimatrixHelper() { }

private:
	static const std::string VERIMATRIX_OCDM_ID;
	const std::string FRIENDLY_NAME;
	const int CODEC_TYPE;
	const uint8_t VERIMATRIX_PSSH_DATA_POSITION;
	
	std::vector<uint8_t> mInitData;
	std::vector<uint8_t> mKeyID;
	std::string mContentMetadata;
};

class AampVerimatrixHelperFactory : public AampDrmHelperFactory
{
	std::shared_ptr<AampDrmHelper> createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj=NULL) const;

	void appendSystemId(std::vector<std::string>& systemIds) const;

	bool isDRM(const struct DrmInfo& drmInfo) const;
};


#endif //_AAMP_VERIMATRIX_DRM_HELPER_H
