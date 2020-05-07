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

// For shared memory
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "AampVgdrmHelper.h"
#include "AampJsonObject.h"
#include "GlobalConfigAAMP.h"

static AampVgdrmHelperFactory vgdrm_helper_factory;

const std::string AampVgdrmHelper::VGDRM_OCDM_ID = "com.videoguard.drm";

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

std::shared_ptr<AampDrmHelper> AampVgdrmHelperFactory::createHelper(const struct DrmInfo& drmInfo) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampVgdrmHelper>(drmInfo);
	}
	return NULL;
}

void AampVgdrmHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(VGDRM_UUID);
}

// This just closes a file on descope
class myContext {
public:
	myContext(int handle) : handle_(handle) {};
	~myContext() { if (handle_ > 0) { close(handle_); } }
private:
	int handle_;
};

bool AampVgdrmHelper::encode(const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut) 
{
	int shmHandle = shm_open(VGDRM_SHARED_MEMORY_NAME.c_str(), VGDRM_SHARED_MEMORY_CREATE_OFLAGS, VGDRM_SHARED_MEMORY_MODE);
	
	if (shmHandle < 0) 
	{
		AAMPLOG_WARN("Failed to create Shared memory object: %d", errno);
		return false;
	}

	// This will close the SM object regardless
	myContext mc(shmHandle);
	
	int status = ftruncate(shmHandle, dataInSz);
	if (status < 0) 
	{
		AAMPLOG_WARN("Failed to truncate the Shared memory object %d", status);
		return false;
	}

	void *dataWr = mmap(NULL, dataInSz, PROT_WRITE | PROT_READ, MAP_SHARED, shmHandle, 0);
	if (dataWr == 0) 
	{
		AAMPLOG_WARN("Failed to map the Shared memory object %d", errno);
		return false;
	}
	
	memmove(dataWr, dataIn, dataInSz);
	
	status = munmap(dataWr, dataInSz);
	if (status < 0)
	{
		AAMPLOG_WARN("Failed to unmap the Shared memory object %d", errno);
		return false;
	}
	// Only send the size of the shared memory, nothing else
	VgdrmInterchangeBuffer ib { dataInSz };
	
	// Look away now, this is horrid
	dataOut.resize(sizeof(ib));
	memcpy(dataOut.data(), &ib, sizeof(ib));

	return true;
}

bool AampVgdrmHelper::decode(const uint8_t * dataIn, uint32_t dataInSz, uint8_t* dataOut, uint32_t dataOutSz) 
{
	int shmHandle = shm_open(VGDRM_SHARED_MEMORY_NAME.c_str(), VGDRM_SHARED_MEMORY_READ_OFLAGS, VGDRM_SHARED_MEMORY_MODE);
	
	if (shmHandle < 0) 
	{
		AAMPLOG_WARN("Failed to create Shared memory object: %d", errno);
		return false;
	}

	if (dataInSz != sizeof(VgdrmInterchangeBuffer))
	{
		AAMPLOG_WARN("Wrong data packet size, expected %d, got %d", sizeof(VgdrmInterchangeBuffer), dataInSz);
		return false;
	}
	// This will close the SM object regardless
	myContext mc(shmHandle);
	
	const VgdrmInterchangeBuffer *pib = reinterpret_cast<const VgdrmInterchangeBuffer *>(dataIn);
	uint32_t packetSize = pib->dataSize;
	void *dataRd = mmap(NULL, packetSize, PROT_READ, MAP_SHARED, shmHandle, 0);
	if (dataRd == 0) 
	{
		AAMPLOG_WARN("Failed to map the Shared memory object %d", errno);
		return false;
	}
	
	if (packetSize > dataOutSz)
	{
		AAMPLOG_WARN("Received data is bigger than provided buffer. %d > %d", packetSize, dataOutSz);
	}
	memmove(dataOut, dataRd, std::min(packetSize, dataOutSz));
	
	int status = munmap(dataRd, packetSize);
	if (status < 0)
	{
		AAMPLOG_WARN("Failed to unmap the Shared memory object %d", errno);
		return false;
	}
	
	return true;
}
