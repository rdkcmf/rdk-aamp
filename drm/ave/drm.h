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
#include <memory>

#define MAX_DRM_CONTEXT 6
#define DRM_SHA1_HASH_LEN 40
#define DRM_IV_LEN 16
#define SESSION_TOKEN_URL "http://localhost:50050/authService/getSessionToken"

#ifdef AVE_DRM
#include "ave-adapter/MyFlashAccessAdapter.h"
#else

/**
 * @struct DrmMetadata
 * @brief AVE drm metadata extracted from EXT-X-FAXS-CM
 */
struct DrmMetadata
{ // from EXT-X-FAXS-CM
	unsigned char * metadataPtr;
	size_t metadataSize;
};
#endif /*AVE_DRM*/


/**
 * @class AveDrm
 * @brief Adobe AVE DRM management
 */
class AveDrm : public HlsDrmBase
{
public:
	/**
	 * @fn AveDrm
	 */
	AveDrm();
	AveDrm(const AveDrm&) = delete;
	AveDrm& operator=(const AveDrm&) = delete;
	/**
	 * @fn ~AveDrm
	 *
	 */
	~AveDrm();
	/**
	 * @fn SetMetaData
	 *
	 * @param[in] aamp      Pointer to PrivateInstanceAAMP object associated with player
	 * @param[in] metadata  Pointed to DrmMetadata structure - unpacked binary metadata from EXT-X-FAXS-CM
	 * @param[in] trackType Track Type ( audio / video)
	 *
	 * @retval eDRM_SUCCESS on success
	 */
	DrmReturn SetMetaData(class PrivateInstanceAAMP *aamp, void* metadata,int trackType, AampLogManager *logObj=NULL);
	/**
	 * @fn SetDecryptInfo
	 *
	 * @param aamp AAMP instance to be associated with this decryptor
	 * @param drmInfo DRM information required to decrypt
	 * @retval eDRM_SUCCESS on success
	 */
	DrmReturn SetDecryptInfo(PrivateInstanceAAMP *aamp, const struct DrmInfo *drmInfo, AampLogManager *logObj=NULL);
	/**
	 * @fn Decrypt
	 * @param bucketType Type of bucket for profiling
	 * @param encryptedDataPtr pointer to encyrpted payload
	 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
	 * @param timeInMs wait time
	 */
	DrmReturn Decrypt(ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs);
	/**
	 * @fn Release
	 */
	void Release();
	/**
	 * @fn CancelKeyWait
	 */
	void CancelKeyWait();
	/**
	 * @fn RestoreKeyState
	 */
	void RestoreKeyState();
	/**
	 * @fn SetState
	 *
	 * @param state State to be set
	 */
	void SetState(DRMState state);
	/**
	 * @fn GetState
	 *
	 * @retval DRMState
	 */
	DRMState GetState();
	/**
	 * @fn AcquireKey
	 *
	 * @param[in] aamp      Pointer to PrivateInstanceAAMP object associated with player
	 * @param[in] metadata  Pointed to DrmMetadata structure - unpacked binary metadata from EXT-X-FAXS-CM
	 */
	void AcquireKey( class PrivateInstanceAAMP *aamp, void *metadata,int trackType, AampLogManager *logObj);
	DRMState mDrmState;
private:
	PrivateInstanceAAMP *mpAamp;
	class MyFlashAccessAdapter *m_pDrmAdapter;
	class TheDRMListener *m_pDrmListner;
	DRMState mPrevDrmState;
	DrmMetadata mMetaData;
	DrmInfo mDrmInfo;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	// Function to store the new DecrypytInfo 
	/**
	 * @fn StoreDecryptInfoIfChanged 
	 *
	 * @param[in] drmInfo  DRM information required to decrypt
	 *
	 * @retval True on new DrmInfo , false if already exist
	 */
	bool StoreDecryptInfoIfChanged( const DrmInfo *drmInfo);
	AampLogManager *mLogObj;
};


/**
* @struct       DRMErrorData 
* @brief        data structure for poplulating values from DRM listner to senderrorevent.
*/
typedef struct DRMErrorData
{
        void *ptr;
        char description[MAX_ERROR_DESCRIPTION_LENGTH];
        AAMPTuneFailure drmFailure;
        bool isRetryEnabled;
}DRMErrorData;


/**
* @struct	DrmMetadataNode
* @brief	DrmMetadataNode structure for DRM Metadata/Hash storage
*/
struct DrmMetadataNode
{
	DrmMetadata metaData;
	int deferredInterval ;
	long long drmKeyReqTime;
	char* sha1Hash;
};

/**
* @class	AveDrmManager
* @brief	Manages AveDrm instances and provide functions for license acquisition and rotation.
* 			Methods are not multi-thread safe. Caller is responsible for synchronization.
*/
class AveDrmManager
{
public:
	/**
	 * @fn ResetAll
         */
	static void ResetAll();
	/**
	 * @fn CancelKeyWaitAll
	 */
	static void CancelKeyWaitAll();
	/**
	 * @fn ReleaseAll
	 */
	static void ReleaseAll();
	/**
	 * @fn RestoreKeyStateAll
	 */
	static void RestoreKeyStateAll();
	/**
	 * @fn SetMetadata
	 *
	 * @param[in] aamp          AAMP instance associated with the operation.
	 * @param[in] metaDataNode  DRM meta data node containing meta-data to be set.
	 * @param[in] trackType     Source track type (audio/video)
	 */
	static void SetMetadata(PrivateInstanceAAMP *aamp, DrmMetadataNode *metaDataNode,int trackType, AampLogManager *mLogObj = NULL);
	/**
	 * @fn PrintSha1Hash
	 *
	 * @param sha1Hash SHA1 hash to be printed
	 */
	static void PrintSha1Hash( char* sha1Hash);
	/**
	 * @fn DumpCachedLicenses
	 */	
	static void DumpCachedLicenses();
	/**
	 * @fn FlushAfterIndexList
	 */
	static void FlushAfterIndexList(const char* trackname,int trackType);
	/**
         * @fn UpdateBeforeIndexList 
	 */
	static void UpdateBeforeIndexList(const char* trackname,int trackType);
	/**
	 * @fn IsMetadataAvailable
	 *
	 */
	static int IsMetadataAvailable(char* sha1Hash);
	/**
	 * @fn GetAveDrm
	 *
	 * @param[in] sha1Hash SHA1 hash of meta-data
	 * @param[in] trackType Sourec track type (audio/video)
	 *
	 * @return AveDrm  Instance corresponds to sha1Hash
	 * @return NULL    If AveDrm instance configured with the meta-data is not available
	 */
	static std::shared_ptr<AveDrm> GetAveDrm(char* sha1Hash,int trackType, AampLogManager *logObj=NULL);
	/**
	 * @fn AcquireKey
	 *
	 * @param[in] aamp      Pointer to PrivateInstanceAAMP object associated with player
	 * @param[in] metaDataNode  Pointed to DrmMetadata structure - unpacked binary metadata from EXT-X-FAXS-CM
	 * @param[in] trackType Track type audio / video
	 * @param[in] overrideDeferring Flag to indicate override deferring and request key immediately
	 */
	static bool AcquireKey(PrivateInstanceAAMP *aamp, DrmMetadataNode *metaDataNode,int trackType, AampLogManager *logObj = NULL, bool overrideDeferring=false);
	static int GetNewMetadataIndex(DrmMetadataNode* drmMetadataIdx, int drmMetadataCount);
	static void ApplySessionToken();
private:
	/**
	 * @fn AveDrmManager
	 *
	 */
	AveDrmManager();
	/**
	 * @fn Reset
	 */
	void Reset();
	char mSha1Hash[DRM_SHA1_HASH_LEN];
	std::shared_ptr<AveDrm> mDrm;
	bool mDrmContexSet;
	bool mHasBeenUsed;
	int mUserCount;
	int mTrackType;
	long long mDeferredTime;
	static bool mSessionTokenAcquireStarted;
	static bool mSessionTokenWaitAbort;
	static std::vector<AveDrmManager*> sAveDrmManager;
	static long setSessionToken();
	/**
	 *  @fn write_callback_session
	 *
	 *  @param[in]	ptr - Pointer to received data.
	 *  @param[in]	size, nmemb - Size of received data (size * nmemb).
	 *  @param[out]	userdata - Pointer to buffer where the received data is copied.
	 *  @return		returns the number of bytes processed.
	 */
	static size_t write_callback_session(char *ptr, size_t size,size_t nmemb, void *userdata);
	/**
	 * @brief
	 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
	 * @param dltotal total bytes expected to download
	 * @param dlnow downloaded bytes so far
	 * @param ultotal total bytes expected to upload
	 * @param ulnow uploaded bytes so far
	 *
	 */
	static int progress_callback(void *clientp,double dltotal,double dlnow,double ultotal, double ulnow );
};

#endif // DRM_H
