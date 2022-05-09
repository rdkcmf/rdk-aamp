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
 * @file aamp_aes.cpp
 * @brief HLS AES drm decryptor
 */


#include "aamp_aes.h"

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>


#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define OPEN_SSL_CONTEXT mOpensslCtx
#else
#define OPEN_SSL_CONTEXT &mOpensslCtx
#endif
#define AES_128_KEY_LEN_BYTES 16

static pthread_mutex_t instanceLock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief key acquistion thread
 * @param arg AesDec pointer
 * @retval NULL
 */
static void * acquire_key(void* arg)
{
	AesDec *aesDec = (AesDec *)arg;
	aesDec->AcquireKey();
	return NULL;
}


void AesDec::NotifyDRMError(AAMPTuneFailure drmFailure)
{
	//If downloads are disabled, don't send error event upstream
	if (mpAamp->DownloadsAreEnabled())
	{
		mpAamp->DisableDownloads();
		if(AAMP_TUNE_UNTRACKED_DRM_ERROR == drmFailure)
		{
			char description[128] = {};
			sprintf(description, "AAMP: DRM Failure");
			mpAamp->SendErrorEvent(drmFailure, description);
		}
		else
		{
			mpAamp->SendErrorEvent(drmFailure);
		}
	}
	SignalDrmError();
	AAMPLOG_ERR("AesDec::NotifyDRMError: drmState:%d", mDrmState );
}



void AesDec::SignalDrmError()
{
	pthread_mutex_lock(&mMutex);
	mDrmState = eDRM_KEY_FAILED;
	pthread_cond_broadcast(&mCond);
	pthread_mutex_unlock(&mMutex);
}



void AesDec::SignalKeyAcquired()
{
	AAMPLOG_WARN("aamp:AesDRMListener drmState:%d moving to KeyAcquired", mDrmState);
	pthread_mutex_lock(&mMutex);
	mDrmState = eDRM_KEY_ACQUIRED;
	pthread_cond_broadcast(&mCond);
	pthread_mutex_unlock(&mMutex);
	mpAamp->LogDrmInitComplete();
}



void AesDec::AcquireKey()
{
	std::string tempEffectiveUrl;
	std::string keyURI;
	long http_error = 0;  //CID:88814 - Initialization
	double downloadTime = 0.0;
	bool keyAcquisitionStatus = false;
	AAMPTuneFailure failureReason = AAMP_TUNE_UNTRACKED_DRM_ERROR;

	if (aamp_pthread_setname(pthread_self(), "aampAesDRM"))
	{
		AAMPLOG_ERR("pthread_setname_np failed");
	}
	aamp_ResolveURL(keyURI, mDrmInfo.manifestURL, mDrmInfo.keyURI.c_str(), mDrmInfo.bPropagateUriParams);
	AAMPLOG_WARN("Key acquisition start uri = %s",  keyURI.c_str());
	bool fetched = mpAamp->GetFile(keyURI, &mAesKeyBuf, tempEffectiveUrl, &http_error, &downloadTime, NULL, mCurlInstance, true, eMEDIATYPE_LICENCE);
	if (fetched)
	{
		if (AES_128_KEY_LEN_BYTES == mAesKeyBuf.len)
		{
			AAMPLOG_WARN("Key fetch success len = %d",  (int)mAesKeyBuf.len);
			keyAcquisitionStatus = true;
		}
		else
		{
			AAMPLOG_ERR("Error Key fetch - size %d",  (int)mAesKeyBuf.len);
			failureReason = AAMP_TUNE_INVALID_DRM_KEY;
		}
	}
	else
	{
		AAMPLOG_ERR("Key fetch failed");
		if (http_error == CURLE_OPERATION_TIMEDOUT)
		{
			failureReason = AAMP_TUNE_LICENCE_TIMEOUT;
		}
		else
		{
			failureReason = AAMP_TUNE_LICENCE_REQUEST_FAILED;
		}
	}

	if(keyAcquisitionStatus)
	{
		SignalKeyAcquired();
	}
	else
	{
		aamp_Free(&mAesKeyBuf); //To cleanup previous successful key if any
		NotifyDRMError(failureReason);
	}
}



DrmReturn AesDec::SetMetaData( PrivateInstanceAAMP *aamp, void* metadata,int trackType, AampLogManager *mLogObj)
{
	return eDRM_SUCCESS;
}


void AesDec::AcquireKey( class PrivateInstanceAAMP *aamp, void *metadata,int trackType, AampLogManager *mLogObj)
{

}


DRMState AesDec::GetState()
{
	return mDrmState;
}


DrmReturn AesDec::SetDecryptInfo( PrivateInstanceAAMP *aamp, const struct DrmInfo *drmInfo, AampLogManager *mLogObj)
{
	DrmReturn err = eDRM_ERROR;
	pthread_mutex_lock(&mMutex);
	if (mDrmState == eDRM_ACQUIRING_KEY)
	{
		AAMPLOG_WARN("AesDec:: acquiring key in progress");
		WaitForKeyAcquireCompleteUnlocked(20*1000, err);
	}
	mpAamp = aamp;
	mDrmInfo = *drmInfo;

	if (!mDrmUrl.empty())
	{
		if ((eDRM_KEY_ACQUIRED == mDrmState) && (drmInfo->keyURI == mDrmUrl))
		{
			AAMPLOG_TRACE("AesDec: same url:%s - not acquiring key", mDrmUrl.c_str());
			pthread_mutex_unlock(&mMutex);
			return eDRM_SUCCESS;
		}
	}
	mDrmUrl = drmInfo->keyURI;
	mDrmState = eDRM_ACQUIRING_KEY;
	mPrevDrmState = eDRM_INITIALIZED;
	if (-1 == mCurlInstance)
	{
		mCurlInstance = eCURLINSTANCE_AES;
		aamp->CurlInit((AampCurlInstance)mCurlInstance,1,aamp->GetLicenseReqProxy());
	}

	if (licenseAcquisitionThreadStarted)
	{
		int ret = pthread_join(licenseAcquisitionThreadId, NULL);
		if (ret != 0)
		{
			AAMPLOG_ERR("AesDec:: pthread_join failed for license acquisition thread: %d",  licenseAcquisitionThreadId);
		}
		licenseAcquisitionThreadStarted = false;
	}

	int ret = pthread_create(&licenseAcquisitionThreadId, NULL, acquire_key, this);
	if(ret != 0)
	{
		AAMPLOG_ERR("AesDec:: pthread_create failed for acquire_key with errno = %d, %s",  errno, strerror(errno));
		mDrmState = eDRM_KEY_FAILED;
		licenseAcquisitionThreadStarted = false;
	}
	else
	{
		err = eDRM_SUCCESS;
		licenseAcquisitionThreadStarted = true;
	}
	pthread_mutex_unlock(&mMutex);
	AAMPLOG_INFO("AesDec: drmState:%d ", mDrmState);
	return err;
}


void AesDec::WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn &err )
{
	struct timespec ts;
	AAMPLOG_INFO( "aamp:waiting for key acquisition to complete,wait time:%d",timeInMs );
	ts = aamp_GetTimespec(timeInMs);

	if(0 != pthread_cond_timedwait(&mCond, &mMutex, &ts)) // block until drm ready
	{
		AAMPLOG_WARN("AesDec:: wait for key acquisition timed out");
		err = eDRM_KEY_ACQUSITION_TIMEOUT;
	}
}



DrmReturn AesDec::Decrypt( ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen,int timeInMs)
{
	DrmReturn err = eDRM_ERROR;

	pthread_mutex_lock(&mMutex);
	if (mDrmState == eDRM_ACQUIRING_KEY)
	{
		WaitForKeyAcquireCompleteUnlocked(timeInMs, err);
	}
	if (mDrmState == eDRM_KEY_ACQUIRED)
	{
		AAMPLOG_INFO("AesDec: Starting decrypt");
		unsigned char *decryptedDataBuf = (unsigned char *)malloc(encryptedDataLen);
		int decryptedDataLen = 0;
		if (decryptedDataBuf)
		{
			int decLen = encryptedDataLen;
			memset(decryptedDataBuf, 0, encryptedDataLen);
			mpAamp->LogDrmDecryptBegin(bucketType);
			if(!EVP_DecryptInit_ex(OPEN_SSL_CONTEXT, EVP_aes_128_cbc(), NULL, (unsigned char*)mAesKeyBuf.ptr, mDrmInfo.iv))
			{
				AAMPLOG_ERR( "AesDec::EVP_DecryptInit_ex failed mDrmState = %d",(int)mDrmState);
			}
			else
			{
				if (!EVP_DecryptUpdate(OPEN_SSL_CONTEXT, decryptedDataBuf, &decLen, (const unsigned char*) encryptedDataPtr, encryptedDataLen))
				{
					AAMPLOG_ERR("AesDec::EVP_DecryptUpdate failed mDrmState = %d",(int) mDrmState);
				}
				else
				{
					decryptedDataLen = decLen;
					decLen = 0;
					AAMPLOG_INFO("AesDec: EVP_DecryptUpdate success decryptedDataLen = %d encryptedDataLen %d", (int) decryptedDataLen, (int)encryptedDataLen);
					if (!EVP_DecryptFinal_ex(OPEN_SSL_CONTEXT, decryptedDataBuf + decryptedDataLen, &decLen))
					{
						AAMPLOG_ERR("AesDec::EVP_DecryptFinal_ex failed mDrmState = %d", 
						        (int) mDrmState);
					}
					else
					{
						decryptedDataLen += decLen;
						AAMPLOG_INFO("AesDec: decrypt success");
						err = eDRM_SUCCESS;
					}
				}
			}
			mpAamp->LogDrmDecryptEnd(bucketType);

			memcpy(encryptedDataPtr, decryptedDataBuf, encryptedDataLen);
			free(decryptedDataBuf);
		}
	}
	else
	{
		AAMPLOG_ERR( "AesDec::key acquisition failure! mDrmState = %d",(int)mDrmState);
	}
	pthread_mutex_unlock(&mMutex);
	return err;
}



void AesDec::Release()
{
	DrmReturn err = eDRM_ERROR;
	pthread_mutex_lock(&mMutex);
	//We wait for license acquisition to complete. Once license acquisition is complete
	//the appropriate state will be set to mDrmState and hence RestoreKeyState will be a no-op.
	if (mDrmState == eDRM_ACQUIRING_KEY || mPrevDrmState == eDRM_ACQUIRING_KEY)
	{
		WaitForKeyAcquireCompleteUnlocked(20*1000, err);
	}
	if (licenseAcquisitionThreadStarted)
	{
		int ret = pthread_join(licenseAcquisitionThreadId, NULL);
		if (ret != 0)
		{
			AAMPLOG_ERR("AesDec:: pthread_join failed for license acquisition thread: %d",licenseAcquisitionThreadId);
		}
		licenseAcquisitionThreadStarted = false;
	}
	pthread_cond_broadcast(&mCond);
	if (-1 != mCurlInstance)
	{
		if (mpAamp)
		{
			mpAamp->SyncBegin();
			mpAamp->CurlTerm((AampCurlInstance)mCurlInstance);
			mpAamp->SyncEnd();
		}
		mCurlInstance = -1;
	}
	pthread_mutex_unlock(&mMutex);
}



void AesDec::CancelKeyWait()
{
	pthread_mutex_lock(&mMutex);
	//save the current state in case required to restore later.
	if (mDrmState != eDRM_KEY_FLUSH)
	{
		mPrevDrmState = mDrmState;
	}
	//required for demuxed assets where the other track might be waiting on mMutex lock.
	mDrmState = eDRM_KEY_FLUSH;
	pthread_cond_broadcast(&mCond);
	pthread_mutex_unlock(&mMutex);
}


void AesDec::RestoreKeyState()
{
	pthread_mutex_lock(&mMutex);
	//In case somebody overwritten mDrmState before restore operation, keep that state
	if (mDrmState == eDRM_KEY_FLUSH)
	{
		mDrmState = mPrevDrmState;
	}
	pthread_mutex_unlock(&mMutex);
}

std::shared_ptr<AesDec> AesDec::mInstance = nullptr;


std::shared_ptr<AesDec> AesDec::GetInstance()
{
	pthread_mutex_lock(&instanceLock);
	if (nullptr == mInstance)
	{
		mInstance = std::make_shared<AesDec>();
	}
	pthread_mutex_unlock(&instanceLock);
	return mInstance;
}


AesDec::AesDec() : mpAamp(nullptr), mDrmState(eDRM_INITIALIZED),
		mPrevDrmState(eDRM_INITIALIZED), mDrmUrl(""),
		mCond(), mMutex(), mOpensslCtx(),
		mDrmInfo(), mAesKeyBuf(), mCurlInstance(-1),
		licenseAcquisitionThreadId(0),
		licenseAcquisitionThreadStarted(false)
{
	pthread_cond_init(&mCond, NULL);
	pthread_mutex_init(&mMutex, NULL);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	OPEN_SSL_CONTEXT = EVP_CIPHER_CTX_new();
#else
	EVP_CIPHER_CTX_init(OPEN_SSL_CONTEXT);
#endif
}



AesDec::~AesDec()
{
	CancelKeyWait();
	Release();
	pthread_mutex_destroy(&mMutex);
	pthread_cond_destroy(&mCond);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX_free(OPEN_SSL_CONTEXT);
#else
	EVP_CIPHER_CTX_cleanup(OPEN_SSL_CONTEXT);
#endif
}
