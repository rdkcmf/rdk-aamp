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

#ifndef AAMPIONMEMORYSYSTEM_H
#define AAMPIONMEMORYSYSTEM_H

#include "AampMemorySystem.h"

#include <fcntl.h>
#include <ion/ion.h>
#include <linux/ion.h>

struct AampIonMemoryInterchangeBuffer {
	uint32_t size;            /// The size of this buffer, for testing
	uint32_t dataSize;        /// The size of data stored in the ION memory
	unsigned long phyAddr;    /// The location in Physical ION memory where the data is [from ION spec]
};


class AampIonMemorySystem : public AAMPMemorySystem {
public:
	AampIonMemorySystem(AampLogManager *logObj);
	virtual ~AampIonMemorySystem();

	/**
	 * Encode a block of data to send over the divide
	 * @param dataIn pointer to the data to encode
	 * @param dataInSz the size to encode
	 * @param out dataOut the data to send
	 * @return true if data is encoded
	 */
	virtual bool encode(const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut) override;
	/**
	 * Decode from getting back
	 * @param dataIn pointer to the data to decode
	 * @param size the size to decode
	 * @param out dataOut the data to recover
	 * @param int dataOutSz the size of the space for data to recover
	 */
	virtual bool decode(const uint8_t* dataIn, uint32_t dataInSz, uint8_t *dataOut, uint32_t dataOutSz) override;

	/**
	 * Call this if there's an failure external to the MS and it needs to tidy up unexpectedly
	 */
	virtual void terminateEarly() override;

public:
	class AampIonMemoryContext {
	public:
		AampIonMemoryContext(AampLogManager *logObj=NULL);
		AampIonMemoryContext(const AampIonMemoryContext&) = delete;
		AampIonMemoryContext& operator=(const AampIonMemoryContext&) = delete;
		~AampIonMemoryContext();
		void close();
		bool createBuffer(size_t len);
		bool share(int& shareFd);
		bool phyAddr(unsigned long *addr);
	private:
		int fd_;
		ion_user_handle_t handle_;
		AampLogManager *mLogObj;
		const int AAMP_ION_MEMORY_REGION{RTK_PHOENIX_ION_HEAP_MEDIA_MASK};
		const int AAMP_ION_MEMORY_FLAGS{(ION_FLAG_NONCACHED | ION_FLAG_SCPUACC | ION_FLAG_HWIPACC)};
		const int AAMP_ION_MEMORY_ALIGN{0};
	};

	AampIonMemoryContext context_;

};



#endif /* AAMPIONMEMORYSYSTEM_H */

