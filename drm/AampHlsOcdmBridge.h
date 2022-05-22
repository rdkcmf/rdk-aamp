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

#ifndef _AAMP_HLS_OCDM_BRIDGE_H_
#define _AAMP_HLS_OCDM_BRIDGE_H_

#include "HlsDrmBase.h"
#include "AampDRMutils.h"
#include "AampDrmSession.h"

#define DECRYPT_WAIT_TIME_MS 3000

class AampHlsOcdmBridge : public HlsDrmBase
{
	DRMState m_drmState;

	const DrmInfo* m_drmInfo;
	AampDrmSession* m_drmSession;
	PrivateInstanceAAMP* m_aampInstance;
	pthread_mutex_t m_Mutex;
	AampLogManager* mLogObj;
public:
	AampHlsOcdmBridge(AampLogManager *logObj, AampDrmSession * aampDrmSession);

	~AampHlsOcdmBridge();

	AampHlsOcdmBridge(const AampHlsOcdmBridge&) = delete;

	AampHlsOcdmBridge& operator=(const AampHlsOcdmBridge&) = delete;

	/*HlsDrmBase Methods*/

	virtual DrmReturn SetMetaData( class PrivateInstanceAAMP *aamp, void* metadata,int trackType, AampLogManager *logObj=NULL) override {return eDRM_SUCCESS;};

	virtual DrmReturn SetDecryptInfo( PrivateInstanceAAMP *aamp, const struct DrmInfo *drmInfoi, AampLogManager *logObj=NULL) override;

	virtual DrmReturn Decrypt(ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs = DECRYPT_WAIT_TIME_MS) override;

	virtual void Release() override;

	virtual void CancelKeyWait() override;

	virtual void RestoreKeyState() override {};

	virtual void AcquireKey( class PrivateInstanceAAMP *aamp, void *metadata,int trackType, AampLogManager *logObj=NULL) override {};

	virtual DRMState GetState() override {return m_drmState;}
};

#endif // _AAMP_HLS_OCDM_BRIDGE_H_
