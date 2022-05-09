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
 * @file AampSharedMemorySystem.cpp
 * @brief Handles the functionalities for shared memory Aamp DRM
 */

#include "AampSharedMemorySystem.h"

// For shared memory
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "AampConfig.h"

AampSharedMemorySystem::AampSharedMemorySystem(AamplOgManager *logObj): AAMPMemorySystem(logObj) {
}

AampSharedMemorySystem::~AampSharedMemorySystem() {
}

bool AampSharedMemorySystem::encode(const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut) 
{
	int shmHandle = shm_open(AAMP_SHARED_MEMORY_NAME.c_str(), AAMP_SHARED_MEMORY_CREATE_OFLAGS, AAMP_SHARED_MEMORY_MODE);
	
	if (shmHandle < 0) 
	{
		AAMPLOG_WARN("Failed to create Shared memory object: %d", errno);
		return false;
	}

	// This will close the SM object regardless
	AampMemoryHandleCloser mc(shmHandle);
	
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
	AampSharedMemoryInterchangeBuffer ib { };
#ifdef AAMP_SHMEM_USE_SIZE_AND_INSTANCE
	ib.size = sizeof(ib);
	ib.instanceNo = 0;
#endif	
	ib.dataSize = dataInSz;
	
	// Look away now, this is horrid
	dataOut.resize(sizeof(ib));
	memcpy(dataOut.data(), &ib, sizeof(ib));

	return true;
}


bool AampSharedMemorySystem::decode(const uint8_t * dataIn, uint32_t dataInSz, uint8_t* dataOut, uint32_t dataOutSz) 
{
	int shmHandle = shm_open(AAMP_SHARED_MEMORY_NAME.c_str(), AAMP_SHARED_MEMORY_READ_OFLAGS, AAMP_SHARED_MEMORY_MODE);
	
	if (shmHandle < 0) 
	{
		AAMPLOG_WARN("Failed to create Shared memory object: %d", errno);
		return false;
	}

	if (dataInSz != sizeof(AampSharedMemoryInterchangeBuffer))
	{
		AAMPLOG_WARN("Wrong data packet size, expected %d, got %d", sizeof(AampSharedMemoryInterchangeBuffer), dataInSz);
		return false;
	}
	// This will close the SM object regardless
	AampMemoryHandleCloser mc(shmHandle);
	
	const AampSharedMemoryInterchangeBuffer *pib = reinterpret_cast<const AampSharedMemoryInterchangeBuffer *>(dataIn);
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
