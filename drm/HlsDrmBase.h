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

#ifndef _DRM_HLSDRMBASE_H_
#define _DRM_HLSDRMBASE_H_

#include "priv_aamp.h"

enum DrmReturn
{
	eDRM_SUCCESS,
	eDRM_ERROR,
	eDRM_KEY_ACQUSITION_TIMEOUT
};

enum DRMState
{
	eDRM_INITIALIZED,
	eDRM_ACQUIRING_KEY,
	eDRM_KEY_ACQUIRED,
	eDRM_KEY_FAILED,
	eDRM_KEY_FLUSH
};

class HlsDrmBase
{
public:

	/**
	 * @brief prepare for decryption - individualization & license acquisition
	 *
	 * @param metadata DRM specific metadata
	 * @param metadataSize length in bytes of data pointed to by metadataPtr
	 * @param iv 128-bit (16 byte) initialization vector for decryption
	 * @param encryptedRotationKey (not currently used/present)
	 */
	virtual int SetContext( class PrivateInstanceAAMP *aamp, void* metadata, const struct DrmInfo *drmInfo) = 0;

	/**
	 * @param encryptedDataPtr pointer to encyrpted payload
	 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
	 */
	virtual DrmReturn Decrypt(ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs = 3000) = 0;

	/**
	 * @brief Release drm session
	 */
	virtual void Release() = 0;

	/**
	 * @brief Cancel timed_wait operation drm_Decrypt
	 *
	 */
	virtual void CancelKeyWait() = 0;

	/**
	 * @brief Restore key state post cleanup of
	 * audio/video TrackState in case DRM data is persisted
	 */
	virtual void RestoreKeyState() = 0;

	/**
	 * @brief Destructor
	 */
	virtual ~HlsDrmBase(){};

};
#endif /* _DRM_HLSDRMBASE_H_ */
