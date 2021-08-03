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
* @file AampDRMSessionManager.h
* @brief Header file for DRM session manager
*/
#ifndef AampDRMSessionManager_h
#define AampDRMSessionManager_h

#include "aampdrmsessionfactory.h"
#include "AampDrmSession.h"
#include "AampDRMutils.h"
#include "priv_aamp.h"
#include "main_aamp.h"
#include <string>
#include <curl/curl.h>
#include "AampDrmHelper.h"

#ifdef USE_SECCLIENT
#include "sec_client.h"
#endif

#define VIDEO_SESSION 0
#define AUDIO_SESSION 1

/**
 *  @struct	DrmSessionCacheInfo
 *  @brief	Drm Session Cache Information for keeping single DRM session always.
 */
typedef struct DrmSessionCacheInfo{
	pthread_t createDRMSessionThreadID; /**< Thread Id for DrM Session thread */
	bool drmSessionThreadStarted; /**< DRM Session start flag to identify the DRM Session thread running */
}DrmSessionCacheInfo;

/**
 *  @struct	DrmSessionDataInfo
 *  @brief	Drm Session Data Information 
 * for storing in a pool from parser.
 */
typedef struct DrmSessionDataInfo{
	struct DrmSessionParams* sessionData; /**< DRM Session Data */
	bool isProcessedLicenseAcquire; /**< Flag to avoid multiple acquire for a key */
	unsigned char *processedKeyId; /**< Pointer to store last processed key Id */
	int processedKeyIdLen; /**< Last processed key Id size */
}DrmSessionDataInfo;

/**
 *  @struct	DrmSessionContext
 *  @brief	To store drmSession and keyId data.
 */
struct DrmSessionContext
{
	std::vector<uint8_t> data;
	pthread_mutex_t sessionMutex;
	AampDrmSession * drmSession;

	DrmSessionContext() : sessionMutex(PTHREAD_MUTEX_INITIALIZER), drmSession(NULL),data()
	{
	}
	DrmSessionContext(const DrmSessionContext& other) : sessionMutex(other.sessionMutex), data(), drmSession()
	{
		// Releases memory allocated after destructing any of these objects
		drmSession = other.drmSession;
		data = other.data;
	}
	DrmSessionContext& operator=(const DrmSessionContext& other)
	{
		sessionMutex = other.sessionMutex;
		data = other.data;
		drmSession = other.drmSession;
		return *this;
	}
};

/**
 * @struct DrmSessionParams
 * @brief Holds data regarding drm session
 */
struct DrmSessionParams
{
	DrmSessionParams() : initData(NULL), initDataLen(0), stream_type(eMEDIATYPE_DEFAULT),
		aamp(NULL), drmType(eDRM_NONE), drmHelper()
	{};
	DrmSessionParams(const DrmSessionParams&) = delete;
	DrmSessionParams& operator=(const DrmSessionParams&) = delete;
	unsigned char *initData;
	int initDataLen;
	MediaType stream_type;
	PrivateInstanceAAMP *aamp;
	DRMSystems drmType;
	std::shared_ptr<AampDrmHelper> drmHelper;
};

/**
 *  @struct	KeyID
 *  @brief	Structure to hold, keyId and session creation time for
 *  		keyId
 */
struct KeyID
{
	std::vector<std::vector<uint8_t>> data;
	long long creationTime;
	bool isFailedKeyId;
	bool isPrimaryKeyId;

	KeyID();
};

/**
 *  @brief	Enum to represent session manager state.
 *  		Session manager would abort any createDrmSession
 *  		request if in eSESSIONMGR_INACTIVE state.
 */
typedef enum{
	eSESSIONMGR_INACTIVE,
	eSESSIONMGR_ACTIVE
}SessionMgrState;

/**
 *  @class	AampDRMSessionManager
 *  @brief	Controller for managing DRM sessions.
 */
class AampDRMSessionManager
{

private:
	DrmSessionContext *drmSessionContexts;
	KeyID *cachedKeyIDs;
	char* accessToken;
	int accessTokenLen;
	SessionMgrState sessionMgrState;
	pthread_mutex_t accessTokenMutex;
	pthread_mutex_t cachedKeyMutex;
	pthread_mutex_t mDrmSessionLock;
	bool curlSessionAbort;
	bool licenseRequestAbort;
	bool mEnableAccessAtrributes;
	int mMaxDRMSessions;

	AampDRMSessionManager(const AampDRMSessionManager &) = delete;
	AampDRMSessionManager& operator=(const AampDRMSessionManager &) = delete;

	static size_t write_callback(char *ptr, size_t size, size_t nmemb,
			void *userdata);
	static int progress_callback(void *clientp,	double dltotal, 
			double dlnow, double ultotal, double ulnow );
public:

	AampDRMSessionManager(int maxDrmSessions);

	void initializeDrmSessions();

	~AampDRMSessionManager();

	AampDrmSession * createDrmSession(const char* systemId, MediaFormat mediaFormat,
			const unsigned char * initDataPtr, uint16_t dataLength, MediaType streamType,
			PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr e, const unsigned char *contentMetadata = nullptr,
			bool isPrimarySession = false);

	AampDrmSession* createDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, MediaType streamType);

#ifdef USE_SECCLIENT
	DrmData * getLicenseSec(const AampLicenseRequest &licenseRequest, std::shared_ptr<AampDrmHelper> drmHelper,
			const AampChallengeInfo& challengeInfo, const PrivateInstanceAAMP* aampInstance, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle);
#endif
	DrmData * getLicense(AampLicenseRequest &licRequest, int32_t *httpError, MediaType streamType, PrivateInstanceAAMP* aamp, bool isContentMetadataAvailable = false, std::string licenseProxy="");

	bool IsKeyIdUsable(std::vector<uint8_t> keyIdArray);

	void clearSessionData();

	void clearAccessToken();

	void clearFailedKeyIds();

	void clearDrmSession(bool forceClearSession = false);

	void setSessionMgrState(SessionMgrState state);

	SessionMgrState getSessionMgrState();
	
	void setCurlAbort(bool isAbort);

	bool getCurlAbort(void);

	void setLicenseRequestAbort(bool isAbort);

	const char* getAccessToken(int &tokenLength, long &error_code ,bool bSslPeerVerify);

	KeyState getDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, int &selectedSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isPrimarySession = false);

	KeyState initializeDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, DrmMetaDataEventPtr eventHandle);

	KeyState acquireLicense(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
			DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, MediaType streamType);

	KeyState handleLicenseResponse(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
			int32_t httpResponseCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance);

	KeyState processLicenseResponse(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
			shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance);

	bool configureLicenseServerParameters(std::shared_ptr<AampDrmHelper> drmHelper, AampLicenseRequest& licRequest,
			std::string &licenseServerProxy, const AampChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance);

};

typedef struct writeCallbackData{
	DrmData *data ;
	AampDRMSessionManager* mDRMSessionManager;
}writeCallbackData;

#endif
