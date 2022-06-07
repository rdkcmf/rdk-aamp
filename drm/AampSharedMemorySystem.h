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

#ifndef AAMPSHAREDMEMORYSYSTEM_H
#define AAMPSHAREDMEMORYSYSTEM_H

/**
 * @file AampSharedMemorySystem.h
 * @brief Handles the functionalities for shared memory communication
 */

#include "AampMemorySystem.h"

#include <fcntl.h>
#include <string>


/**
 * @struct AampSharedMemoryInterchangeBuffer
 * @brief Holds the Shared memory details
 */
struct AampSharedMemoryInterchangeBuffer {
#ifdef AAMP_SHMEM_USE_SIZE_AND_INSTANCE
	uint32_t size;       /**< The size of this buffer, for testing */
	uint32_t instanceNo; /**< The value appended to the SM file, 0 means no number, currently unused */
#endif
	uint32_t dataSize;   /**< The size of data stored in the shared memory */
};

/**
 * @class AampSharedMemorySystem
 * @brief Handles the shared Memory operations
 */
class AampSharedMemorySystem : public AAMPMemorySystem {
public:
	AampSharedMemorySystem(AampLogManager *logObj);
	virtual ~AampSharedMemorySystem();

	/**
	 * @fn encode
	 * @param dataIn pointer to the data to encode
	 * @param dataInSz the size to encode
	 * @param[out] dataOut the data to send
	 * @return true if data is encoded
	 */
	virtual bool encode(const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut);
	/**
	 * @fn decode
	 * @param dataIn pointer to the data to decode
	 * @param dataInSz the size to decode
	 * @param[out] dataOut the data to recover
	 * @param[in] dataOutSz the size of the space for data to recover
	 */
	virtual bool decode(const uint8_t* dataIn, uint32_t dataInSz, uint8_t *dataOut, uint32_t dataOutSz);
private:
	const std::string AAMP_SHARED_MEMORY_NAME{"/aamp_drm"};
	const int AAMP_SHARED_MEMORY_CREATE_OFLAGS{O_RDWR | O_CREAT};
	const int AAMP_SHARED_MEMORY_READ_OFLAGS{O_RDONLY};
	const mode_t AAMP_SHARED_MEMORY_MODE{ S_IRWXU | S_IRWXG | S_IRWXO };
	
};


#endif /* AAMPSHAREDMEMORYSYSTEM_H */

