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
 * @file AampVanillaDrmHelper.h
 * @brief Handles the operation foe Vanilla DRM
 */

#ifndef AAMPVANILLADRMHELPER_H
#define AAMPVANILLADRMHELPER_H

#include "AampDrmHelper.h"
#include <string>
#include <vector>

/**
 *  @class AampVanillaDrmHelper
 *  @brief Handles the operation foe Vanilla DRM
 */

class AampVanillaDrmHelper : public AampDrmHelper
{
private:
	const std::string EMPTY_STRING;
public:
	virtual const std::string& ocdmSystemId() const { return EMPTY_STRING; };

	virtual void createInitData(std::vector<uint8_t>& initData){};

	virtual bool parsePssh(const uint8_t* initData, uint32_t initDataLen){ return false; };

	virtual bool isClearDecrypt() { return true; };

	virtual void setDrmMetaData(const std::string& metaData) { }

	virtual int getDrmCodecType() const { return CODEC_TYPE; }

	virtual void getKey(std::vector<uint8_t>& keyID) { keyID.clear(); };

	virtual bool isExternalLicense() const { return true; };

	virtual void generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const {};

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; }

	virtual void createInitData(std::vector<uint8_t>& initData) const {};

	virtual bool isClearDecrypt() const { return true; };

	virtual void getKey(std::vector<uint8_t>& keyID) const {};

	AampVanillaDrmHelper(AampLogManager *logObj) : AampDrmHelper(DrmInfo {},logObj), FRIENDLY_NAME("Vanilla_AES"), CODEC_TYPE(3), EMPTY_STRING() {}

private:
	const std::string FRIENDLY_NAME;
	const int CODEC_TYPE;

};

#endif /* AAMPVANILLADRMHELPER_H */

