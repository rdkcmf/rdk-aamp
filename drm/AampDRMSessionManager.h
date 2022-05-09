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
	bool drmSessionThreadStarted; 	    /**< DRM Session start flag to identify the DRM Session thread running */
}DrmSessionCacheInfo;

/**
 *  @struct	DrmSessionDataInfo
 *  @brief	Drm Session Data Information 
 * for storing in a pool from parser.
 */
typedef struct DrmSessionDataInfo{
	struct DrmSessionParams* sessionData;   /**< DRM Session Data */
	bool isProcessedLicenseAcquire; 	/**< Flag to avoid multiple acquire for a key */
	unsigned char *processedKeyId; 		/**< Pointer to store last processed key Id */
	int processedKeyIdLen; 			/**< Last processed key Id size */
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
	~DrmSessionContext()
	{
		pthread_mutex_destroy(&sessionMutex);
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
	eSESSIONMGR_INACTIVE, /**< DRM Session mgr is inactive */
	eSESSIONMGR_ACTIVE    /**< DRM session mgr is active */	
}SessionMgrState;

/**
 *  @class	AampDRMSessionManager
 *  @brief	Controller for managing DRM sessions.
 */
class AampDRMSessionManager
{

private:
	AampLogManager *mLogObj;
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
#ifdef USE_SECMANAGER
	int64_t mSessionId;
#endif
	/**     
     	 * @brief Copy constructor disabled
     	 *
     	 */
	AampDRMSessionManager(const AampDRMSessionManager &) = delete;
	/**
 	 * @brief assignment operator disabled
	 *
 	 */
	AampDRMSessionManager& operator=(const AampDRMSessionManager &) = delete;
	/**
	 *  @brief		Curl write callback, used to get the curl o/p
	 *  			from DRM license, accessToken curl requests.
	 *
	 *  @param[in]	ptr - Pointer to received data.
	 *  @param[in]	size, nmemb - Size of received data (size * nmemb).
	 *  @param[out]	userdata - Pointer to buffer where the received data is copied.
	 *  @return		returns the number of bytes processed.
	 */
	static size_t write_callback(char *ptr, size_t size, size_t nmemb,
			void *userdata);
	/**
	 * @brief
	 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
	 * @param dltotal total bytes expected to download
	 * @param dlnow downloaded bytes so far
	 * @param ultotal total bytes expected to upload
	 * @param ulnow uploaded bytes so far
	 * @retval Return non-zero for CURLE_ABORTED_BY_CALLBACK
	 */
	static int progress_callback(void *clientp,	double dltotal, 
			double dlnow, double ultotal, double ulnow );
public:
	
	/**
	 *  @brief AampDRMSessionManager constructor.
	 */
	AampDRMSessionManager(AampLogManager *logObj, int maxDrmSessions);

	void initializeDrmSessions();
	
	/**
	 *  @brief AampDRMSessionManager Destructor.
	 */
	~AampDRMSessionManager();
	/**
	 *  @brief	Creates and/or returns the DRM session corresponding to keyId (Present in initDataPtr)
	 *  		AampDRMSession manager has two static AampDrmSession objects.
	 *  		This method will return the existing DRM session pointer if any one of these static
	 *  		DRM session objects are created against requested keyId. Binds the oldest DRM Session
	 *  		with new keyId if no matching keyId is found in existing sessions.
	 *
	 *  @param[in]	systemId - UUID of the DRM system.
	 *  @param[in]	initDataPtr - Pointer to PSSH data.
	 *  @param[in]	dataLength - Length of PSSH data.
	 *  @param[in]	streamType - Whether audio or video.
	 *  @param[in]	contentMetadata - Pointer to content meta data, when content meta data
	 *  			is already extracted during manifest parsing. Used when content meta data
	 *  			is available as part of another PSSH header, like DRM Agnostic PSSH
	 *  			header.
	 *  @param[in]	aamp - Pointer to PrivateInstanceAAMP, for DRM related profiling.
	 *  @retval  	error_code - Gets updated with proper error code, if session creation fails.
	 *  			No NULL checks are done for error_code, caller should pass a valid pointer.
	 *  @return		Pointer to DrmSession for the given PSSH data; NULL if session creation/mapping fails.
	 */
	AampDrmSession * createDrmSession(const char* systemId, MediaFormat mediaFormat,
			const unsigned char * initDataPtr, uint16_t dataLength, MediaType streamType,
			PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr e, const unsigned char *contentMetadata = nullptr,
			bool isPrimarySession = false);
	/**
	 * Create DrmSession by using the AampDrmHelper object
	 * @return AampdrmSession
	 */
	AampDrmSession* createDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, MediaType streamType);

#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	DrmData * getLicenseSec(const AampLicenseRequest &licenseRequest, std::shared_ptr<AampDrmHelper> drmHelper,
			const AampChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle);
#endif
	/**
	 *  @brief	Get DRM license key from DRM server.
	 *
	 *  @param[in]	licRequest - License request details (URL, headers etc.)
	 *  @param[out]	httpError - Gets updated with http error; default -1.
	 *  @param[in]	isContentMetadataAvailable - Flag to indicate whether MSO specific headers
	 *  			are to be used.
	 *  @param[in]	licenseProxy - Proxy to use for license requests.
	 *  @return		Structure holding DRM license key and it's length; NULL and 0 if request fails
	 *  @note		Memory for license key is dynamically allocated, deallocation
	 *				should be handled at the caller side.
	 *			customHeader ownership should be taken up by getLicense function
	 *
	 */
	DrmData * getLicense(AampLicenseRequest &licRequest, int32_t *httpError, MediaType streamType, PrivateInstanceAAMP* aamp, bool isContentMetadataAvailable = false, std::string licenseProxy="");
	/**
	 *  @brief	Get DRM license key from DRM server.
	 *  @param[in]	keyIdArray - key Id extracted from pssh data
	 *  @return		bool - true if key is not cached/cached with no failure,
	 * 				false if keyId is already marked as failed.
	 */
	bool IsKeyIdUsable(std::vector<uint8_t> keyIdArray);
	/**
	 *  @brief	Clean up the memory used by session variables.
	 *
	 *  @return	void.
	 */
	void clearSessionData();
	/**
	 *  @brief	Clean up the memory for accessToken.
	 *
	 *  @return	void.
	 */
	void clearAccessToken();
	/**
	 * @brief	Clean up the failed keyIds.
	 *
	 * @return	void.
	 */
	void clearFailedKeyIds();
	/**
	 * @brief	Clean up the Session Data if license key acquisition failed or if LicenseCaching is false.
	 *
	 * @param forceClearSession clear the drm session irrespective of failed keys if LicenseCaching is false.
	 * @return	void.
	 */
	void clearDrmSession(bool forceClearSession = false);

	void setVideoWindowSize(int width, int height);

	void setPlaybackSpeedState(int speed, double position, bool delayNeeded = false);
	/**
	 * @brief	Set Session manager state
	 * @param	state session manager sate to be set
	 * @return	void.
	 */
	void setSessionMgrState(SessionMgrState state);
	
	/**
	 * @brief Get Session manager state
	 * @return session manager state.
	 */
	SessionMgrState getSessionMgrState();
	/**
	 * @brief	Set Session abort flag
	 * @param	isAbort bool flag to curl abort
	 * @return	void.
	 */
	void setCurlAbort(bool isAbort);
	/**
	 * @brief Get Session abort flag
	 * @return bool flag curlSessionAbort.
	 */	
	bool getCurlAbort(void);
	/**
	 * @brief	Get Session abort flag
	 * @param	isAbort bool flag to curl abort
	 * @return	void
	 */
	void setLicenseRequestAbort(bool isAbort);
	/**
	 *  @brief	Get the accessToken from authService.
	 *
	 *  @param[out]	tokenLength - Gets updated with accessToken length.
	 *  @return		Pointer to accessToken.
	 *  @note		AccessToken memory is dynamically allocated, deallocation
	 *				should be handled at the caller side.
	 */
	const char* getAccessToken(int &tokenLength, long &error_code ,bool bSslPeerVerify);
	/**
	 * @brief Create a DRM Session using the Drm Helper
	 * Determine a slot in the drmSession Contexts which can be used
	 * @return index to the selected drmSessionContext which has been selected
	 */
	KeyState getDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, int &selectedSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isPrimarySession = false);
	/**
	 * Initialize the Drm System with InitData(PSSH)
	 */
	KeyState initializeDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance);
	/**
	 * @brief sent license challenge to the DRM server and provide the respone to CDM
	 */
	KeyState acquireLicense(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
			DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, MediaType streamType);

	KeyState handleLicenseResponse(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
			int32_t httpResponseCode, int32_t httpExtResponseCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance);

	KeyState processLicenseResponse(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
			shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance);
	/**
	 * @brief Configure the Drm license server parameters for URL/proxy and custom http request headers
	 */
	bool configureLicenseServerParameters(std::shared_ptr<AampDrmHelper> drmHelper, AampLicenseRequest& licRequest,
			std::string &licenseServerProxy, const AampChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance);

	void notifyCleanup();
};

/**
 * @struct writeCallbackData
 * @brief structure to hold DRM data to write
 */

typedef struct writeCallbackData{
	DrmData *data ;
	AampDRMSessionManager* mDRMSessionManager;
}writeCallbackData;

#endif
