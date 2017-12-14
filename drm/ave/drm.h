/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
#ifndef DRM_H
#define DRM_H

#include <stddef.h> // for size_t
#include "HlsDrmBase.h"

#ifdef AVE_DRM
#include "ave-adapter/MyFlashAccessAdapter.h"
#else
typedef enum
{
	eMETHOD_NONE,
	eMETHOD_AES_128, // encrypted using Advanced Encryption Standard 128-bit key and PKCS7 padding
} DrmMethod;

struct DrmMetadata
{ // from EXT-X-FAXS-CM
	unsigned char * metadataPtr;
	size_t metadataSize;
};

struct DrmInfo
{ // from EXT-X-KEY
	DrmMethod method;
	bool useFirst16BytesAsIV;
	unsigned char *iv; // [16]
	char *uri;
//	unsigned char *CMSha1Hash;// [20]; // unused
//	unsigned char *encryptedRotationKey;
};
#endif /*AVE_DRM*/


class AveDrm : public HlsDrmBase
{
public:

	/**
	 * @brief Get static instance
	 */
	static AveDrm* GetInstance();

	/**
	 * @brief prepare for decryption - individualization & license acquisition
	 *
	 * @param metadata pointed to DrmMetadata structure - unpacked binary metadata from EXT-X-FAXS-CM
	 * @param metadataSize length in bytes of data pointed to by metadataPtr
	 * @param iv 128-bit (16 byte) initialization vector for decryption
	 * @param encryptedRotationKey (not currently used/present)
	 */
	int SetContext( class PrivateInstanceAAMP *aamp, void* metadata, const struct DrmInfo *drmInfo);

	/**
	 * @param encryptedDataPtr pointer to encyrpted payload
	 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
	 */
	DrmReturn Decrypt(ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs);

	/**
	 * @brief Release drm session
	 */
	void Release();

	/**
	 * @brief Cancel timed_wait operation drm_Decrypt
	 *
	 */
	void CancelKeyWait();

	/**
	 * @brief Restore key state post cleanup of
	 * audio/video TrackState in case DRM data is persisted
	 */
	void RestoreKeyState();

private:
	AveDrm(){};
	static AveDrm *mInstance;
};

#endif // DRM_H
