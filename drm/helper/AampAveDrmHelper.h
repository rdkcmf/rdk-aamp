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

#ifndef AAMPAVEDRMHELPER_H
#define AAMPAVEDRMHELPER_H

/**
 * @file AampAveDrmHelper.h
 * @brief Helper functions for Aamp Ave Drm
 */


#include "AampDrmHelper.h"
#include <string>
#include <vector>

/**
 * @class AampAveDrmHelper
 * @brief AampAve DRM operations are handled
 */ 

class AampAveDrmHelper : public AampDrmHelper
{
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

	virtual void cancelDrmSession();

	virtual bool canCancelDrmSession() { return true; }

	virtual void createInitData(std::vector<uint8_t>& initData) const {};

	virtual bool isClearDecrypt() const { return true; };

	virtual void getKey(std::vector<uint8_t>& keyID) const {};

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; }

	AampAveDrmHelper(AampLogManager *logObj) : AampDrmHelper(DrmInfo {}, logObj), FRIENDLY_NAME("Adobe_Access"), EMPTY_STRING(), CODEC_TYPE(2) {}

private:
	const std::string FRIENDLY_NAME;
	const std::string EMPTY_STRING;
	const int CODEC_TYPE;
};

#endif /* AAMPAVEDRMHELPER_H */

