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

/**
 * @file drm.h
 * @brief AVE DRM helper declarations
 */

#ifndef DRM_H
#define DRM_H

#include <stddef.h> // for size_t
#include "HlsDrmBase.h"

#ifdef AVE_DRM
#include "ave-adapter/MyFlashAccessAdapter.h"
#else
/**
 * @enum DrmMethod
 * @brief AVE drm method
 */
typedef enum
{
	eMETHOD_NONE,
	eMETHOD_AES_128, /// encrypted using Advanced Encryption Standard 128-bit key and PKCS7 padding
} DrmMethod;

/**
 * @struct DrmMetadata
 * @brief AVE drm metadata extracted from EXT-X-FAXS-CM
 */
struct DrmMetadata
{ // from EXT-X-FAXS-CM
	unsigned char * metadataPtr;
	size_t metadataSize;
};

/**
 * @struct DrmInfo
 * @brief DRM information required to decrypt
 */
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


/**
 * @class AveDrm
 * @brief Adobe AVE DRM management
 */
class AveDrm : public HlsDrmBase
{
public:
	static AveDrm* GetInstance();
	int SetContext( class PrivateInstanceAAMP *aamp, void* metadata, const struct DrmInfo *drmInfo);
	DrmReturn Decrypt(ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs);
	void Release();
	void CancelKeyWait();
	void RestoreKeyState();
private:
	/**
	 * @brief AveDrm private Constructor
	 */
	AveDrm(){};
	
	static AveDrm *mInstance;
};


/**
* @struct       data
* @brief        data structure for poplulating values from DRM listner to senderrorevent.
*/
typedef struct DRMErrorData
{
        void *ptr;
        char description[MAX_ERROR_DESCRIPTION_LENGTH];
        AAMPTuneFailure drmFailure;
        bool isRetryEnabled;
}DRMErrorData;


#endif // DRM_H
