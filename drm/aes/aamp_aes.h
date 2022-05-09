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

#ifndef _AAMP_AES_H_
#define _AAMP_AES_H_

/**
 * @file aamp_aes.h
 * @brief HLS AES drm decryptor
 */

#include <stddef.h> // for size_t
#include "HlsDrmBase.h"
#include "drm.h"
#include <openssl/evp.h>
#include <memory>

/**
 * @class AesDec
 * @brief Vanilla AES based DRM management
 */
class AesDec : public HlsDrmBase
{
public:
	/**
	 * @brief Get singleton instance
	 */
	static std::shared_ptr<AesDec> GetInstance();
	/**
	 * @brief Set DRM meta-data. Stub implementation
	 *
	 * @param aamp AAMP instance to be associated with this decryptor
	 * @param metadata - Ignored
	 *
	 * @retval eDRM_SUCCESS
	 */
	DrmReturn SetMetaData( PrivateInstanceAAMP *aamp, void* metadata,int trackType, AampLogManager *logObj=NULL);
	/**
	 * @brief GetState Function to get current DRM State
	 *
	 *
	 * @retval DRMState
	 */
	DRMState GetState();
	/**
	 * @brief AcquireKey Function to acquire key . Stub implementation
	 *
	 * @param[in] aamp       AAMP instance to be associated with this decryptor
	 * @param[in] metadata   Ignored
	 *
	 * @retval None
	 */
	void AcquireKey( class PrivateInstanceAAMP *aamp, void *metadata,int trackType, AampLogManager *logObj=NULL);
	/**
	 * @brief Set information required for decryption
	 *
	 * @param aamp AAMP instance to be associated with this decryptor
	 * @param drmInfo Drm information
	 * @retval eDRM_SUCCESS on success
	 */
	DrmReturn SetDecryptInfo( PrivateInstanceAAMP *aamp, const struct DrmInfo *drmInfo, AampLogManager *logObj=NULL);
	/**
	 * @brief Decrypts an encrypted buffer
	 * @param bucketType Type of bucket for profiling
	 * @param encryptedDataPtr pointer to encyrpted payload
	 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
	 * @param timeInMs wait time
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

	/*Functions to support internal operations*/
	/**
	 * @brief Acquire drm key from URI
	 */
	void AcquireKey();
	/**
	 * @brief Signal key acquired event
	 */
	void SignalKeyAcquired();
	/**
	 * @brief Notify drm error
	 * @param drmFailure drm error type
	 */
	void NotifyDRMError(AAMPTuneFailure drmFailure);
	/**
     * @brief Signal drm error
	 */
	void SignalDrmError();
	/**
	 * @brief Wait for key acquisition completion
	 * @param[in] timeInMs timeout
	 * @param[out] err error on failure
	 */
	void WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn &err);
	/**
	 * @brief AesDec Constructor
	 * 
	 */
	AesDec();
	/**
	 * @brief AesDec Destructor
	 */
	~AesDec();
	AesDec(const AesDec&) = delete;
	AesDec& operator=(const AesDec&) = delete;

private:

	static std::shared_ptr<AesDec> mInstance;
	PrivateInstanceAAMP *mpAamp;
	pthread_cond_t mCond;
	pthread_mutex_t mMutex;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX *mOpensslCtx;
#else
	EVP_CIPHER_CTX mOpensslCtx;
#endif
	DrmInfo mDrmInfo ;
	GrowableBuffer mAesKeyBuf;
	DRMState mDrmState;
	DRMState mPrevDrmState;
	std::string mDrmUrl;
	int mCurlInstance;
	pthread_t licenseAcquisitionThreadId;
	bool licenseAcquisitionThreadStarted;
};

#endif // _AAMP_AES_H_
