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
 * @file AampDRMSessionManager.cpp
 * @brief Source file for DrmSessionManager.
 */

#include "AampDRMSessionManager.h"
#include "priv_aamp.h"
#include <pthread.h>
#include "_base64.h"
#include <iostream>
#include "AampMutex.h"
#include "AampDrmHelper.h"
#include "AampJsonObject.h"
#include "AampUtils.h"
#include "AampRfc.h"
#include <inttypes.h>
#ifdef USE_SECMANAGER
#include "AampSecManager.h"
#endif

#include "AampCurlStore.h"

//#define LOG_TRACE 1
#define LICENCE_REQUEST_HEADER_ACCEPT "Accept:"

#define LICENCE_REQUEST_HEADER_CONTENT_TYPE "Content-Type:"

#define LICENCE_RESPONSE_JSON_LICENCE_KEY "license"
#define DRM_METADATA_TAG_START "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">"
#define DRM_METADATA_TAG_END "</ckm:policy>"
#define SESSION_TOKEN_URL "http://localhost:50050/authService/getSessionToken"

#define INVALID_SESSION_SLOT -1
#define DEFUALT_CDM_WAIT_TIMEOUT_MS 2000

#define CURL_EASY_SETOPT(curl, CURLoption, option)\
    if (curl_easy_setopt(curl, CURLoption, option) != 0) {\
          AAMPLOG_WARN("Failed at curl_easy_setopt ");\
    }  //CID:128208 - checked return

static const char *sessionTypeName[] = {"video", "audio", "subtitle", "aux-audio"};

static pthread_mutex_t drmSessionMutex = PTHREAD_MUTEX_INITIALIZER;

KeyID::KeyID() : creationTime(0), isFailedKeyId(false), isPrimaryKeyId(false), data()
{
}


void CreateDRMSession(void *arg);
int SpawnDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp, DrmSessionDataInfo* drmData);
void ReleaseDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp);

#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
/**
 *  @brief Get formatted URL of license server
 *
 *  @param[in] url URL of license server
 *  @return		formatted url for secclient license acqusition.
 */
static string getFormattedLicenseServerURL(string url)
{
	size_t startpos = 0;
	size_t endpos, len;
	endpos = len = url.size();

	if (memcmp(url.data(), "https://", 8) == 0)
	{
		startpos += 8;
	}
	else if (memcmp(url.data(), "http://", 7) == 0)
	{
		startpos += 7;
	}

	if (startpos != 0)
	{
		endpos = url.find('/', startpos);
		if (endpos != string::npos)
		{
			len = endpos - startpos;
		}
	}

	return url.substr(startpos, len);
}
#endif

/**
 *  @brief AampDRMSessionManager constructor.
 */
AampDRMSessionManager::AampDRMSessionManager(AampLogManager *logObj, int maxDrmSessions) : drmSessionContexts(NULL),
		cachedKeyIDs(NULL), accessToken(NULL),
		accessTokenLen(0), sessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE), accessTokenMutex(PTHREAD_MUTEX_INITIALIZER),
		cachedKeyMutex(PTHREAD_MUTEX_INITIALIZER)
		,curlSessionAbort(false), mEnableAccessAtrributes(true)
		,mDrmSessionLock(), licenseRequestAbort(false)
		,mMaxDRMSessions(maxDrmSessions)
		,mLogObj(logObj)
#ifdef USE_SECMANAGER
		,mSessionId(AAMP_SECMGR_INVALID_SESSION_ID)
#endif
{
	if(maxDrmSessions < 1)
	{
		mMaxDRMSessions = 1; // minimum of 1 drm session needed
	}
	else if(maxDrmSessions > MAX_DASH_DRM_SESSIONS)
	{
		mMaxDRMSessions = MAX_DASH_DRM_SESSIONS; // limit to 30 
	}
	drmSessionContexts	= new DrmSessionContext[mMaxDRMSessions];
	cachedKeyIDs		= new KeyID[mMaxDRMSessions];
	AAMPLOG_INFO("AampDRMSessionManager MaxSession:%d",mMaxDRMSessions);
	pthread_mutex_init(&mDrmSessionLock, NULL);
}

/**
 *  @brief AampDRMSessionManager Destructor.
 */
AampDRMSessionManager::~AampDRMSessionManager()
{
	clearAccessToken();
	clearSessionData();
	SAFE_DELETE_ARRAY(drmSessionContexts);
	SAFE_DELETE_ARRAY(cachedKeyIDs);
	pthread_mutex_destroy(&mDrmSessionLock);
	pthread_mutex_destroy(&accessTokenMutex);
	pthread_mutex_destroy(&cachedKeyMutex);
}

/**
 *  @brief  Clean up the memory used by session variables.
 */
void AampDRMSessionManager::clearSessionData()
{
	AAMPLOG_WARN(" AampDRMSessionManager:: Clearing session data");
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		if (drmSessionContexts != NULL && drmSessionContexts[i].drmSession != NULL)
		{			
#ifdef USE_SECMANAGER
			// Cached session is about to be destroyed, send release to SecManager
			if (drmSessionContexts[i].drmSession->getSessionId() != AAMP_SECMGR_INVALID_SESSION_ID)
			{
				AampSecManager::GetInstance()->ReleaseSession(drmSessionContexts[i].drmSession->getSessionId());
			}
#endif
			SAFE_DELETE(drmSessionContexts[i].drmSession);
			drmSessionContexts[i] = DrmSessionContext();
		}

		if (cachedKeyIDs != NULL)
		{
			cachedKeyIDs[i] = KeyID();
		}
	}
}

/**
 * @brief Set Session manager state
 */
void AampDRMSessionManager::setSessionMgrState(SessionMgrState state)
{
	sessionMgrState = state;
}

/**
 * @brief Get Session manager state
 */
SessionMgrState AampDRMSessionManager::getSessionMgrState()
{
	return sessionMgrState;
}

/**
 * @brief Set Session abort flag
 */
void AampDRMSessionManager::setCurlAbort(bool isAbort){
	curlSessionAbort = isAbort;
}

/**
 * @brief Get Session abort flag
 */
bool AampDRMSessionManager::getCurlAbort(){
	return curlSessionAbort;
}

/**
 * @brief Get Session abort flag
 */
void AampDRMSessionManager::setLicenseRequestAbort(bool isAbort)
{
	setCurlAbort(isAbort);
	licenseRequestAbort = isAbort;
}

/**
 * @brief Clean up the failed keyIds.
 */
void AampDRMSessionManager::clearFailedKeyIds()
{
	pthread_mutex_lock(&cachedKeyMutex);
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		if(cachedKeyIDs[i].isFailedKeyId)
		{
			if(!cachedKeyIDs[i].data.empty())
			{
				cachedKeyIDs[i].data.clear();
			}
			cachedKeyIDs[i].isFailedKeyId = false;
			cachedKeyIDs[i].creationTime = 0;
		}
		cachedKeyIDs[i].isPrimaryKeyId = false;
	}
	pthread_mutex_unlock(&cachedKeyMutex);
}

/**
 *  @brief Clean up the memory for accessToken.
 */
void AampDRMSessionManager::clearAccessToken()
{
	if(accessToken)
	{
		free(accessToken);
		accessToken = NULL;
		accessTokenLen = 0;
	}
}

/**
 * @brief Clean up the Session Data if license key acquisition failed or if LicenseCaching is false.
 */
void AampDRMSessionManager::clearDrmSession(bool forceClearSession)
{
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		// Clear the session data if license key acquisition failed or if forceClearSession is true in the case of LicenseCaching is false.
		if((cachedKeyIDs[i].isFailedKeyId || forceClearSession) && drmSessionContexts != NULL)
		{
			AampMutexHold sessionMutex(drmSessionContexts[i].sessionMutex);
			if (drmSessionContexts[i].drmSession != NULL)
			{
				AAMPLOG_WARN("AampDRMSessionManager:: Clearing failed Session Data Slot : %d", i);				
#ifdef USE_SECMANAGER
				// Cached session is about to be destroyed, send release to SecManager
				if (drmSessionContexts[i].drmSession->getSessionId() != AAMP_SECMGR_INVALID_SESSION_ID)
				{
					AampSecManager::GetInstance()->ReleaseSession(drmSessionContexts[i].drmSession->getSessionId());
				}
#endif
				SAFE_DELETE(drmSessionContexts[i].drmSession);
			}
		}
	}
}


void AampDRMSessionManager::setVideoWindowSize(int width, int height)
{
#ifdef USE_SECMANAGER
	AAMPLOG_WARN("In AampDRMSessionManager:: setting video windor size w:%d x h:%d mMaxDRMSessions=%d mSessionId=[%" PRId64 "]",width,height,mMaxDRMSessions,mSessionId);
	if(mSessionId != AAMP_SECMGR_INVALID_SESSION_ID)
	{
		AAMPLOG_WARN("In AampDRMSessionManager:: valid session ID");
		AampSecManager::GetInstance()->setVideoWindowSize(mSessionId, width, height);
	}
#endif
}


void AampDRMSessionManager::setPlaybackSpeedState(int speed, double position, bool delayNeeded)
{
#ifdef USE_SECMANAGER
	AAMPLOG_WARN("In AampDRMSessionManager::after calling setPlaybackSpeedState speed=%d position=%f mSessionId=[%" PRId64 "]",speed, position, mSessionId);
	if(mSessionId != AAMP_SECMGR_INVALID_SESSION_ID)
	{
		AAMPLOG_WARN("In AampDRMSessionManager::valid session ID");
		AampSecManager::GetInstance()->setPlaybackSpeedState(mSessionId, speed, position, delayNeeded);
	}
#endif
}


int AampDRMSessionManager::progress_callback(
	void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
	double dltotal, // total bytes expected to download
	double dlnow, // downloaded bytes so far
	double ultotal, // total bytes expected to upload
	double ulnow // uploaded bytes so far
	)
{
	int returnCode = 0 ;
	AampDRMSessionManager *drmSessionManager = (AampDRMSessionManager *)clientp;
	if(drmSessionManager->getCurlAbort())
	{
		logprintf("Aborting DRM curl operation.. - CURLE_ABORTED_BY_CALLBACK");
		returnCode = -1; // Return non-zero for CURLE_ABORTED_BY_CALLBACK
		//Reset the abort variable
		drmSessionManager->setCurlAbort(false);
	}

	return returnCode;
}

/**
 *  @brief  Curl write callback, used to get the curl o/p
 *          from DRM license, accessToken curl requests.
 */
size_t AampDRMSessionManager::write_callback(char *ptr, size_t size,
		size_t nmemb, void *userdata)
{
	writeCallbackData *callbackData = (writeCallbackData*)userdata;
        DrmData *data = (DrmData *)callbackData->data;
        size_t numBytesForBlock = size * nmemb;
        if(callbackData->mDRMSessionManager->getCurlAbort())
        {
                logprintf("Aborting DRM curl operation.. - CURLE_ABORTED_BY_CALLBACK");
                numBytesForBlock = 0; // Will cause CURLE_WRITE_ERROR
		//Reset the abort variable
		callbackData->mDRMSessionManager->setCurlAbort(false);

        }
        else if (data->getData().empty())        
        {
		data->setData((unsigned char *)ptr, numBytesForBlock);
	}
	else
	{
		data->addData((unsigned char *)ptr, numBytesForBlock);
	}
	if (gpGlobalConfig->logging.trace)
	{
		logprintf(" wrote %zu number of blocks", numBytesForBlock);
	}
	return numBytesForBlock;
}

/**
 *  @brief	Extract substring between (excluding) two string delimiters.
 *
 *  @param[in]	parentStr - Parent string from which substring is extracted.
 *  @param[in]	startStr, endStr - String delimiters.
 *  @return	Returns the extracted substring; Empty string if delimiters not found.
 */
string _extractSubstring(string parentStr, string startStr, string endStr)
{
	string ret = "";
	int startPos = parentStr.find(startStr);
	if(string::npos != startPos)
	{
		int offset = strlen(startStr.c_str());
		int endPos = parentStr.find(endStr, startPos + offset + 1);
		if(string::npos != endPos)
		{
			ret = parentStr.substr(startPos + offset, endPos - (startPos + offset));
		}
	}
	return ret;
}

/**
 *  @brief Get the accessToken from authService.
 */
const char * AampDRMSessionManager::getAccessToken(int &tokenLen, long &error_code , bool bSslPeerVerify)
{
	if(accessToken == NULL)
	{
		DrmData * tokenReply = new DrmData();
		writeCallbackData *callbackData = new writeCallbackData();
		callbackData->data = tokenReply;
		callbackData->mDRMSessionManager = this;
		CURLcode res;
		long httpCode = -1;

		CURL *curl = curl_easy_init();;
		CURL_EASY_SETOPT(curl, CURLOPT_NOSIGNAL, 1L);
		CURL_EASY_SETOPT(curl, CURLOPT_WRITEFUNCTION, write_callback);
		CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
		CURL_EASY_SETOPT(curl, CURLOPT_TIMEOUT, DEFAULT_CURL_TIMEOUT);
		CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSDATA, this);
		CURL_EASY_SETOPT(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
		CURL_EASY_SETOPT(curl, CURLOPT_FOLLOWLOCATION, 1L);
		CURL_EASY_SETOPT(curl, CURLOPT_NOPROGRESS, 0L);
		CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, callbackData);
		if(!bSslPeerVerify){
		     CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		}
		CURL_EASY_SETOPT(curl, CURLOPT_URL, SESSION_TOKEN_URL);

		res = curl_easy_perform(curl);

		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			if (httpCode == 200 || httpCode == 206)
			{
				string tokenReplyStr = tokenReply->getData();
				string tokenStatusCode = _extractSubstring(tokenReplyStr, "status\":", ",\"");
				if(tokenStatusCode.length() == 0)
				{
					//StatusCode could be last element in the json
					tokenStatusCode = _extractSubstring(tokenReplyStr, "status\":", "}");
				}
				if(tokenStatusCode.length() == 1 && tokenStatusCode.c_str()[0] == '0')
				{
					string token = _extractSubstring(tokenReplyStr, "token\":\"", "\"");
					size_t len = token.length();
					if(len > 0)
					{
						accessToken = (char*)malloc(len+1);
						if(accessToken)
						{
							accessTokenLen = len;
							memcpy( accessToken, token.c_str(), len );
							accessToken[len] = 0x00;
							AAMPLOG_WARN(" Received session token from auth service ");
						}
						else
						{
							AAMPLOG_WARN("accessToken is null");  //CID:83536 - Null Returns
						}
					}
					else
					{
						AAMPLOG_WARN(" Could not get access token from session token reply");
						error_code = (long)eAUTHTOKEN_TOKEN_PARSE_ERROR;
					}
				}
				else
				{
					AAMPLOG_ERR(" Missing or invalid status code in session token reply");
					error_code = (long)eAUTHTOKEN_INVALID_STATUS_CODE;
				}
			}
			else
			{
				AAMPLOG_ERR(" Get Session token call failed with http error %d", httpCode);
				error_code = httpCode;
			}
		}
		else
		{
			AAMPLOG_ERR(" Get Session token call failed with curl error %d", res);
			error_code = res;
		}
		SAFE_DELETE(tokenReply);
		SAFE_DELETE(callbackData);
		curl_easy_cleanup(curl);
	}

	tokenLen = accessTokenLen;
	return accessToken;
}

/**
 *  @brief Get DRM license key from DRM server.
 */
bool AampDRMSessionManager::IsKeyIdUsable(std::vector<uint8_t> keyIdArray)
{
	bool ret = true;
	pthread_mutex_lock(&cachedKeyMutex);
	for (int sessionSlot = 0; sessionSlot < mMaxDRMSessions; sessionSlot++)
	{
		auto keyIDSlot = cachedKeyIDs[sessionSlot].data;
		if (!keyIDSlot.empty() && keyIDSlot.end() != std::find(keyIDSlot.begin(), keyIDSlot.end(), keyIdArray))
		{
			std::string debugStr = AampLogManager::getHexDebugStr(keyIdArray);
			AAMPLOG_INFO("Session created/inprogress with same keyID %s at slot %d", debugStr.c_str(), sessionSlot);
			break;
		}
	}
	pthread_mutex_unlock(&cachedKeyMutex);

	return ret;
}


#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)

DrmData * AampDRMSessionManager::getLicenseSec(const AampLicenseRequest &licenseRequest, std::shared_ptr<AampDrmHelper> drmHelper,
		const AampChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle)
{
	DrmData *licenseResponse = nullptr;
	const char *mediaUsage = "stream";
	string contentMetaData = drmHelper->getDrmMetaData();
	char *encodedData = base64_Encode(reinterpret_cast<const unsigned char*>(contentMetaData.c_str()), contentMetaData.length());
	char *encodedChallengeData = base64_Encode(reinterpret_cast<const unsigned char*>(challengeInfo.data->getData().c_str()), challengeInfo.data->getDataLength());
	//Calculate the lengths using the logic in base64_Encode
	size_t encodedDataLen = ((contentMetaData.length() + 2) /3) * 4;
	size_t encodedChallengeDataLen = ((challengeInfo.data->getDataLength() + 2) /3) * 4;
	
	const char *keySystem = drmHelper->ocdmSystemId().c_str();
	const char *secclientSessionToken = challengeInfo.accessToken.empty() ? NULL : challengeInfo.accessToken.c_str();

	char *licenseResponseStr = NULL;
	size_t licenseResponseLength = 2;
	uint32_t refreshDuration = 3;
	const char *requestMetadata[1][2];
	uint8_t numberOfAccessAttributes = 0;
	const char *accessAttributes[2][2] = {NULL, NULL, NULL, NULL};
	std::string serviceZone, streamID;
	if(aampInstance->mIsVSS)
	{
		if (aampInstance->GetEnableAccessAtrributesFlag())
		{
			serviceZone = aampInstance->GetServiceZone();
			streamID = aampInstance->GetVssVirtualStreamID();
			if (!serviceZone.empty())
			{
				accessAttributes[numberOfAccessAttributes][0] = VSS_SERVICE_ZONE_KEY_STR;
				accessAttributes[numberOfAccessAttributes][1] = serviceZone.c_str();
				numberOfAccessAttributes++;
			}
			if (!streamID.empty())
			{
				accessAttributes[numberOfAccessAttributes][0] = VSS_VIRTUAL_STREAM_ID_KEY_STR;
				accessAttributes[numberOfAccessAttributes][1] = streamID.c_str();
				numberOfAccessAttributes++;
			}
		}
		AAMPLOG_INFO("accessAttributes : {\"%s\" : \"%s\", \"%s\" : \"%s\"}", accessAttributes[0][0], accessAttributes[0][1], accessAttributes[1][0], accessAttributes[1][1]);
	}
	std::string moneytracestr;
	requestMetadata[0][0] = "X-MoneyTrace";
	aampInstance->GetMoneyTraceString(moneytracestr);
	requestMetadata[0][1] = moneytracestr.c_str();

	AAMPLOG_WARN("[HHH] Before calling SecClient_AcquireLicense-----------");
	AAMPLOG_WARN("destinationURL is %s (drm server now used)", licenseRequest.url.c_str());
	AAMPLOG_WARN("MoneyTrace[%s]", requestMetadata[0][1]);
#if USE_SECMANAGER
	if(aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager))
	{
		int32_t statusCode;
		int32_t reasonCode;
		int32_t businessStatus;
		bool res = AampSecManager::GetInstance()->AcquireLicense(aampInstance, licenseRequest.url.c_str(),
																 requestMetadata,
																 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
																 encodedData, encodedDataLen,
																 encodedChallengeData, encodedChallengeDataLen,
																 keySystem,
																 mediaUsage,
																 secclientSessionToken, challengeInfo.accessToken.length(),
																 &mSessionId,
																 &licenseResponseStr, &licenseResponseLength,
																 &statusCode, &reasonCode, &businessStatus);
		if (res)
		{
			AAMPLOG_WARN("acquireLicense via SecManager SUCCESS!");
			//TODO: Sort this out for backward compatibility
			*httpCode = 200;
			*httpExtStatusCode = 0;
			if (licenseResponseStr)
			{
				licenseResponse = new DrmData((unsigned char *)licenseResponseStr, licenseResponseLength);
				free(licenseResponseStr);
			}
		}
		else
		{
			eventHandle->SetVerboseErrorCode( statusCode,  reasonCode, businessStatus);
			AAMPLOG_WARN("Verbose error set with class : %d, Reason Code : %d Business status: %d ", statusCode, reasonCode, businessStatus);
			*httpCode = statusCode;
			*httpExtStatusCode = reasonCode;

			AAMPLOG_ERR("acquireLicense via SecManager FAILED!, httpCode %d, httpExtStatusCode %d", *httpCode, *httpExtStatusCode);
			//TODO: Sort this out for backward compatibility
		}
	}
#endif
#if USE_SECCLIENT
#if USE_SECMANAGER
	else
	{
#endif
		int32_t sec_client_result = SEC_CLIENT_RESULT_FAILURE;
		SecClient_ExtendedStatus statusInfo;
		unsigned int attemptCount = 0;
		int sleepTime ;
		aampInstance->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime,sleepTime) ;
		if(sleepTime<=0) sleepTime = 100;
		while (attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
		{
			attemptCount++;
			sec_client_result = SecClient_AcquireLicense(licenseRequest.url.c_str(), 1,
														 requestMetadata, numberOfAccessAttributes,
														 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
														 encodedData,
														 strlen(encodedData),
														 encodedChallengeData, strlen(encodedChallengeData), keySystem, mediaUsage,
														 secclientSessionToken,
														 &licenseResponseStr, &licenseResponseLength, &refreshDuration, &statusInfo);
		
			if (((sec_client_result >= 500 && sec_client_result < 600)||
				 (sec_client_result >= SEC_CLIENT_RESULT_HTTP_RESULT_FAILURE_TLS  && sec_client_result <= SEC_CLIENT_RESULT_HTTP_RESULT_FAILURE_GENERIC ))
				&& attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
			{
				AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : sec_client %d", attemptCount, sec_client_result);
				if (licenseResponseStr) SecClient_FreeResource(licenseResponseStr);
				AAMPLOG_WARN(" acquireLicense : Sleeping %d milliseconds before next retry.", sleepTime);
				mssleep(sleepTime);
			}
			else
			{
				break;
			}
		}
		
			AAMPLOG_TRACE("licenseResponse is %s", licenseResponseStr);
			AAMPLOG_TRACE("licenseResponse len is %zd", licenseResponseLength);
			AAMPLOG_TRACE("accessAttributesStatus is %d", statusInfo.accessAttributeStatus);
			AAMPLOG_TRACE("refreshDuration is %d", refreshDuration);
		
		if (sec_client_result != SEC_CLIENT_RESULT_SUCCESS)
		{
			AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : sec_client %d extStatus %d", attemptCount, sec_client_result, statusInfo.statusCode);
			
			eventHandle->ConvertToVerboseErrorCode( sec_client_result,  statusInfo.statusCode);
			
			*httpCode = sec_client_result;
			*httpExtStatusCode = statusInfo.statusCode;
			
			AAMPLOG_WARN("Converted the secclient httpCode : %d, httpExtStatusCode: %d to verbose error with class : %d, Reason Code : %d Business status: %d ", *httpCode, *httpExtStatusCode, eventHandle->getSecManagerClassCode(), eventHandle->getSecManagerReasonCode(), eventHandle->getBusinessStatus());
		}
		else
		{
			AAMPLOG_WARN(" acquireLicense SUCCESS! license request attempt %d; response code : sec_client %d", attemptCount, sec_client_result);
			eventHandle->setAccessStatusValue(statusInfo.accessAttributeStatus);
			licenseResponse = new DrmData((unsigned char *)licenseResponseStr, licenseResponseLength);
		}
		if (licenseResponseStr) SecClient_FreeResource(licenseResponseStr);
#if USE_SECMANAGER
	}
#endif
#endif
	free(encodedData);
	free(encodedChallengeData);
	return licenseResponse;
}
#endif

/**
 *  @brief Get DRM license key from DRM server.
 */
DrmData * AampDRMSessionManager::getLicense(AampLicenseRequest &licenseRequest,
		int32_t *httpCode, MediaType streamType, PrivateInstanceAAMP* aamp, bool isContentMetadataAvailable, std::string licenseProxy)
{
	*httpCode = -1;
	CURL *curl;
	CURLcode res;
	double totalTime = 0;
	struct curl_slist *headers = NULL;
	DrmData * keyInfo = new DrmData();
	writeCallbackData *callbackData = new writeCallbackData();
	callbackData->data = keyInfo;
	callbackData->mDRMSessionManager = this;
	long challengeLength = 0;
	long long downloadTimeMS = 0;

	curl = CurlStore::GetCurlStoreInstance(aamp)->GetCurlHandle(aamp, licenseRequest.url, eCURLINSTANCE_AES);

	for (auto& header : licenseRequest.headers)
	{
		std::string customHeaderStr = header.first;
		customHeaderStr.push_back(' ');
		// For scenarios with multiple header values, its most likely a custom defined.
		// Below code will have to extended to support the same (eg: money trace headers)
		customHeaderStr.append(header.second.at(0));
		headers = curl_slist_append(headers, customHeaderStr.c_str());
		if (aamp->mConfig->IsConfigSet(eAAMPConfig_CurlLicenseLogging))
		{
			AAMPLOG_WARN(" CustomHeader :%s",customHeaderStr.c_str());
		}
	}

	AAMPLOG_WARN(" Sending license request to server : %s ", licenseRequest.url.c_str());
	if (aamp->mConfig->IsConfigSet(eAAMPConfig_CurlLicenseLogging))
	{
		CURL_EASY_SETOPT(curl, CURLOPT_VERBOSE, 1L);
	}
	CURL_EASY_SETOPT(curl, CURLOPT_WRITEFUNCTION, write_callback);
	CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
	CURL_EASY_SETOPT(curl, CURLOPT_PROGRESSDATA, this);
	CURL_EASY_SETOPT(curl, CURLOPT_TIMEOUT, 5L);
	CURL_EASY_SETOPT(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	CURL_EASY_SETOPT(curl, CURLOPT_FOLLOWLOCATION, 1L);
	CURL_EASY_SETOPT(curl, CURLOPT_URL, licenseRequest.url.c_str());
	CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, callbackData);
	if(!ISCONFIGSET(eAAMPConfig_SslVerifyPeer)){
		CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	else {
		CURL_EASY_SETOPT(curl, CURLOPT_SSLVERSION, aamp->mSupportedTLSVersion);
		CURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	}

	CURL_EASY_SETOPT(curl, CURLOPT_HTTPHEADER, headers);

	if(licenseRequest.method == AampLicenseRequest::POST)
	{
		challengeLength = licenseRequest.payload.size();
		CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDSIZE, challengeLength);
		CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDS,(uint8_t * )licenseRequest.payload.data());
	}
	else
	{
		CURL_EASY_SETOPT(curl, CURLOPT_HTTPGET, 1L);
	}

	if (!licenseProxy.empty())
	{
		CURL_EASY_SETOPT(curl, CURLOPT_PROXY, licenseProxy.c_str());
		/* allow whatever auth the proxy speaks */
		CURL_EASY_SETOPT(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}
	unsigned int attemptCount = 0;
	bool requestFailed = true;
	/* Check whether stopped or not before looping - download will be disabled */
	while(attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS && !licenseRequestAbort)
	{
		bool loopAgain = false;
		attemptCount++;
		long long tStartTime = NOW_STEADY_TS_MS;
		res = curl_easy_perform(curl);
		long long tEndTime = NOW_STEADY_TS_MS;
		long long downloadTimeMS = tEndTime - tStartTime;
		/** Restrict further processing license if stop called in between  */
		if(licenseRequestAbort)
		{
			AAMPLOG_ERR(" Aborting License acquisition");
			// Update httpCode as 42-curl abort, so that DRM error event will not be sent by upper layer
			*httpCode = CURLE_ABORTED_BY_CALLBACK;
			break;
		}
		if (res != CURLE_OK)
		{
			// To avoid scary logging
			if (res != CURLE_ABORTED_BY_CALLBACK && res != CURLE_WRITE_ERROR)
			{
	                        if (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT)
	                        {
	                                // Retry for curl 28 and curl 7 errors.
	                                loopAgain = true;

	                                SAFE_DELETE(keyInfo);
					SAFE_DELETE(callbackData);
	                                keyInfo = new DrmData();
	                                callbackData = new writeCallbackData();
	                                callbackData->data = keyInfo;
	                                callbackData->mDRMSessionManager = this;
	                                CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, callbackData);
        	                }
                	        AAMPLOG_ERR(" curl_easy_perform() failed: %s", curl_easy_strerror(res));
                        	AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : curl %d", attemptCount, res);
			}
			*httpCode = res;
		}
		else
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, httpCode);
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTime);
			if (*httpCode != 200 && *httpCode != 206)
			{
				int  licenseRetryWaitTime;
				aamp->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime,licenseRetryWaitTime) ;
				AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : http %d", attemptCount, *httpCode);
				if(*httpCode >= 500 && *httpCode < 600
						&& attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS && licenseRetryWaitTime > 0)
				{
					SAFE_DELETE(keyInfo);
					SAFE_DELETE(callbackData);
					keyInfo = new DrmData();
                                        callbackData = new writeCallbackData();
                                        callbackData->data = keyInfo;
                                        callbackData->mDRMSessionManager = this;
					CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA, callbackData);
					AAMPLOG_WARN("acquireLicense : Sleeping %d milliseconds before next retry.",licenseRetryWaitTime);
					mssleep(licenseRetryWaitTime);
				}
			}
			else
			{
				AAMPLOG_WARN(" DRM Session Manager Received license data from server; Curl total time  = %.1f", totalTime);
				AAMPLOG_WARN(" acquireLicense SUCCESS! license request attempt %d; response code : http %d", attemptCount, *httpCode);
				requestFailed = false;
			}
		}

		double total, connect, startTransfer, resolve, appConnect, preTransfer, redirect, dlSize;
		long reqSize;
		double totalPerformRequest = (double)(downloadTimeMS)/1000;

		curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &resolve);
		curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connect);
		curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &appConnect);
		curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME, &preTransfer);
		curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &startTransfer);
		curl_easy_getinfo(curl, CURLINFO_REDIRECT_TIME, &redirect);
		curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &dlSize);
		curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE, &reqSize);

		MediaTypeTelemetry mediaType = eMEDIATYPE_TELEMETRY_DRM;
		std::string appName, timeoutClass;
		if (!aamp->GetAppName().empty())
		{
			// append app name with class data
			appName = aamp->GetAppName() + ",";
		}
		if (CURLE_OPERATION_TIMEDOUT == res || CURLE_PARTIAL_FILE == res || CURLE_COULDNT_CONNECT == res)
		{
			// introduce  extra marker for connection status curl 7/18/28,
			// example 18(0) if connection failure with PARTIAL_FILE code
			timeoutClass = "(" + to_string(reqSize > 0) + ")";
		}
		AAMPLOG_WARN("HttpRequestEnd: %s%d,%d,%ld%s,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%g,%ld,%.500s",
						appName.c_str(), mediaType, streamType, *httpCode, timeoutClass.c_str(), totalPerformRequest, totalTime, connect, startTransfer, resolve, appConnect, 
						preTransfer, redirect, dlSize, reqSize, licenseRequest.url.c_str());

		if(!loopAgain)
			break;
	}

	if(*httpCode == -1)
	{
		AAMPLOG_WARN(" Updating Curl Abort Response Code");
		// Update httpCode as 42-curl abort, so that DRM error event will not be sent by upper layer
		*httpCode = CURLE_ABORTED_BY_CALLBACK;
	}

	if(requestFailed && keyInfo != NULL)
	{
		SAFE_DELETE(keyInfo);
	}

	SAFE_DELETE(callbackData);
	curl_slist_free_all(headers);
	CurlStore::GetCurlStoreInstance(aamp)->SaveCurlHandle(aamp, licenseRequest.url, eCURLINSTANCE_AES, curl);

	return keyInfo;
}

/**
 *  @brief      Creates and/or returns the DRM session corresponding to keyId (Present in initDataPtr)
 *              AampDRMSession manager has two static AampDrmSession objects.
 *              This method will return the existing DRM session pointer if any one of these static
 *              DRM session objects are created against requested keyId. Binds the oldest DRM Session
 *              with new keyId if no matching keyId is found in existing sessions.
 *  @return     Pointer to DrmSession for the given PSSH data; NULL if session creation/mapping fails.
 */
AampDrmSession * AampDRMSessionManager::createDrmSession(
		const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, MediaType streamType,
		PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr e, const unsigned char* contentMetadataPtr,
		bool isPrimarySession)
{
	DrmInfo drmInfo;
	std::shared_ptr<AampDrmHelper> drmHelper;
	AampDrmSession *drmSession = NULL;

	drmInfo.method = eMETHOD_AES_128;
	drmInfo.mediaFormat = mediaFormat;
	drmInfo.systemUUID = systemId;
	drmInfo.bPropagateUriParams = ISCONFIGSET(eAAMPConfig_PropogateURIParam);

	if (!AampDrmHelperEngine::getInstance().hasDRM(drmInfo))
	{
		AAMPLOG_ERR(" Failed to locate DRM helper");
	}
	else
	{
		drmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo,mLogObj);

		if(contentMetadataPtr)
		{
			std::string contentMetadataPtrString = reinterpret_cast<const char*>(contentMetadataPtr);
			drmHelper->setDrmMetaData(contentMetadataPtrString);
		}

		if (!drmHelper->parsePssh(initDataPtr, initDataLen))
		{
			AAMPLOG_ERR(" Failed to Parse PSSH from the DRM InitData");
			e->setFailure(AAMP_TUNE_CORRUPT_DRM_METADATA);
		}
		else
		{
			drmSession = AampDRMSessionManager::createDrmSession(drmHelper, e, aamp, streamType);
		}
	}

	return drmSession;
}

/**
 *  @brief Create DrmSession by using the AampDrmHelper object
 */
AampDrmSession* AampDRMSessionManager::createDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, MediaType streamType)
{
	if (!drmHelper || !eventHandle || !aampInstance)
	{
		/* This should never happen, since the caller should have already
		ensure the provided DRMInfo is supported using hasDRM */
		AAMPLOG_ERR(" Failed to create DRM Session invalid parameters ");
		return nullptr;
	}

	// Mutex lock to handle createDrmSession multi-thread calls to avoid timing issues observed in AXi6 as part of DELIA-43939 during Playready-4.0 testing.
	AampMutexHold drmSessionLock(mDrmSessionLock);

	int cdmError = -1;
	KeyState code = KEY_ERROR;

	if (SessionMgrState::eSESSIONMGR_INACTIVE == sessionMgrState)
	{
		AAMPLOG_ERR(" SessionManager state inactive, aborting request");
		return nullptr;
	}

	int selectedSlot = INVALID_SESSION_SLOT;

	AAMPLOG_INFO("StreamType :%d keySystem is %s",streamType, drmHelper->ocdmSystemId().c_str());

	/**
	 * Create drm session without primaryKeyId markup OR retrieve old DRM session.
	 */
	code = getDrmSession(drmHelper, selectedSlot, eventHandle, aampInstance);

	/**
	 * KEY_READY code indicates that a previously created session is being reused.
	 */
	if (code == KEY_READY)
	{
		return drmSessionContexts[selectedSlot].drmSession;
	}

	if ((code != KEY_INIT) || (selectedSlot == INVALID_SESSION_SLOT))
	{
		AAMPLOG_WARN(" Unable to get DrmSession : Key State %d ", code);
		return nullptr;
	}

	std::vector<uint8_t> keyId;
	drmHelper->getKey(keyId);
	bool RuntimeDRMConfigSupported = aampInstance->mConfig->IsConfigSet(eAAMPConfig_RuntimeDRMConfig);
	if(RuntimeDRMConfigSupported && aampInstance->IsEventListenerAvailable(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE) && (streamType < 4))
	{
		aampInstance->mcurrent_keyIdArray = keyId;
		AAMPLOG_INFO("App registered the ContentProtectionDataEvent to send new drm config");
		ContentProtectionDataUpdate(aampInstance, keyId, streamType);
		aampInstance->mcurrent_keyIdArray.clear();
	}

	code = initializeDrmSession(drmHelper, selectedSlot, eventHandle, aampInstance);
	if (code != KEY_INIT)
	{
		AAMPLOG_WARN(" Unable to initialize DrmSession : Key State %d ", code);
		AampMutexHold keymutex(cachedKeyMutex);
 		cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		return nullptr;
	}

	if(aampInstance->mIsFakeTune)
	{
		AAMPLOG(mLogObj, eLOGLEVEL_FATAL, "FATAL", "Exiting fake tune after DRM initialization.");
		AampMutexHold keymutex(cachedKeyMutex);
		cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		return nullptr;
	}

	code = acquireLicense(drmHelper, selectedSlot, cdmError, eventHandle, aampInstance, streamType);
	if (code != KEY_READY)
	{
		AAMPLOG_WARN(" Unable to get Ready Status DrmSession : Key State %d ", code);
		AampMutexHold keymutex(cachedKeyMutex);
		cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		return nullptr;
	}

#ifdef USE_SECMANAGER
	// License acquisition was done, so mSessionId will be populated now
	if (mSessionId != AAMP_SECMGR_INVALID_SESSION_ID)
	{
		AAMPLOG_WARN(" Setting sessionId[%" PRId64 "] to current drmSession", mSessionId);
		drmSessionContexts[selectedSlot].drmSession->setSessionId(mSessionId);
	}
#endif

	return drmSessionContexts[selectedSlot].drmSession;
}

/**
 * @brief Create a DRM Session using the Drm Helper
 *        Determine a slot in the drmSession Contexts which can be used
 */
KeyState AampDRMSessionManager::getDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, int &selectedSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isPrimarySession)
{
	KeyState code = KEY_ERROR;
	bool keySlotFound = false;
	bool isCachedKeyId = false;
	unsigned char *keyId = NULL;

	std::vector<uint8_t> keyIdArray;
	std::map<int, std::vector<uint8_t>> keyIdArrays;
	drmHelper->getKeys(keyIdArrays);

	drmHelper->getKey(keyIdArray);

	//Need to Check , Are all Drm Schemes/Helpers capable of providing a non zero keyId?
	if (keyIdArray.empty())
	{
		eventHandle->setFailure(AAMP_TUNE_FAILED_TO_GET_KEYID);
		return code;
	}

	if (keyIdArrays.empty())
	{
		// Insert keyId into map
		keyIdArrays[0] = keyIdArray;
	}

	std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);

	/* Slot Selection
	* Find drmSession slot by going through cached keyIds
	* Check if requested keyId is already cached
	*/
	int sessionSlot = 0;

	{
		AampMutexHold keymutex(cachedKeyMutex);

		for (; sessionSlot < mMaxDRMSessions; sessionSlot++)
		{
			auto keyIDSlot = cachedKeyIDs[sessionSlot].data;
			if (!keyIDSlot.empty() && keyIDSlot.end() != std::find(keyIDSlot.begin(), keyIDSlot.end(), keyIdArray))
			{
				AAMPLOG_INFO("Session created/inprogress with same keyID %s at slot %d", keyIdDebugStr.c_str(), sessionSlot);
				keySlotFound = true;
				isCachedKeyId = true;
				break;
			}
		}

		if (!keySlotFound)
		{
			/* Key Id not in cached list so we need to find out oldest slot to use;
			 * Oldest slot may be used by current playback which is marked primary
			 * Avoid selecting that slot
			 * */
			/*select the first slot that is not primary*/
			for (int index = 0; index < mMaxDRMSessions; index++)
			{
				if (!cachedKeyIDs[index].isPrimaryKeyId)
				{
					keySlotFound = true;
					sessionSlot = index;
					break;
				}
			}

			if (!keySlotFound)
			{
				AAMPLOG_WARN("  Unable to find keySlot for keyId %s ", keyIdDebugStr.c_str());
				return KEY_ERROR;
			}

			/*Check if there's an older slot */
			for (int index= sessionSlot + 1; index< mMaxDRMSessions; index++)
			{
				if (cachedKeyIDs[index].creationTime < cachedKeyIDs[sessionSlot].creationTime)
				{
					sessionSlot = index;
				}
			}
			AAMPLOG_WARN("  Selected slot %d for keyId %s", sessionSlot, keyIdDebugStr.c_str());
		}
		else
		{
			// Already same session Slot is marked failed , not to proceed again .
			if(cachedKeyIDs[sessionSlot].isFailedKeyId)
			{
				AAMPLOG_WARN(" Found FailedKeyId at sesssionSlot :%d, return key error",sessionSlot);
				return KEY_ERROR;
			}
		}
		

		if (!isCachedKeyId)
		{
			if(cachedKeyIDs[sessionSlot].data.size() != 0)
			{
				cachedKeyIDs[sessionSlot].data.clear();
			}
			cachedKeyIDs[sessionSlot].isFailedKeyId = false;

			std::vector<std::vector<uint8_t>> data;
			for(auto& keyId : keyIdArrays)
			{
				std::string debugStr = AampLogManager::getHexDebugStr(keyId.second);
				AAMPLOG_INFO("Insert[%d] - slot:%d keyID %s", keyId.first, sessionSlot, debugStr.c_str());
				data.push_back(keyId.second);
			}

			cachedKeyIDs[sessionSlot].data = data;
		}
		cachedKeyIDs[sessionSlot].creationTime = aamp_GetCurrentTimeMS();
		cachedKeyIDs[sessionSlot].isPrimaryKeyId = isPrimarySession;
	}

	selectedSlot = sessionSlot;
	const std::string systemId = drmHelper->ocdmSystemId();
	AampMutexHold sessionMutex(drmSessionContexts[sessionSlot].sessionMutex);
	if (drmSessionContexts[sessionSlot].drmSession != NULL)
	{
		if (drmHelper->ocdmSystemId() != drmSessionContexts[sessionSlot].drmSession->getKeySystem())
		{
			AAMPLOG_WARN("changing DRM session for %s to %s", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), drmHelper->ocdmSystemId().c_str());
		}
		else if (cachedKeyIDs[sessionSlot].data.end() != std::find(cachedKeyIDs[sessionSlot].data.begin(), cachedKeyIDs[sessionSlot].data.end() ,drmSessionContexts[sessionSlot].data))
		{
			KeyState existingState = drmSessionContexts[sessionSlot].drmSession->getState();
			if (existingState == KEY_READY)
			{
				AAMPLOG_WARN("Found drm session READY with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
#ifdef USE_SECMANAGER
				// Cached session is re-used, set its session ID to active.
				// State management will be done from getLicenseSec function in case of KEY_INIT
				if (drmSessionContexts[sessionSlot].drmSession->getSessionId() != AAMP_SECMGR_INVALID_SESSION_ID &&
					mSessionId == AAMP_SECMGR_INVALID_SESSION_ID)
				{
					// Set the drmSession's ID as mSessionId so that this code will not be repeated for multiple calls for createDrmSession
					mSessionId = drmSessionContexts[sessionSlot].drmSession->getSessionId();
					AampSecManager::GetInstance()->UpdateSessionState(drmSessionContexts[sessionSlot].drmSession->getSessionId(), true);
				}
#endif
				return KEY_READY;
			}
			if (existingState == KEY_INIT)
			{
				AAMPLOG_WARN("Found drm session in INIT state with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
				return KEY_INIT;
			}
			else if (existingState <= KEY_READY)
			{
				if (drmSessionContexts[sessionSlot].drmSession->waitForState(KEY_READY, drmHelper->keyProcessTimeout()))
				{
					AAMPLOG_WARN("Waited for drm session READY with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
					return KEY_READY;
				}
				AAMPLOG_WARN("key was never ready for %s ", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
				cachedKeyIDs[selectedSlot].isFailedKeyId = true;
				return KEY_ERROR;
			}
			else
			{
				AAMPLOG_WARN("existing DRM session for %s has error state %d", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), existingState);
				cachedKeyIDs[selectedSlot].isFailedKeyId = true;
				return KEY_ERROR;
			}
		}
		else
		{
			AAMPLOG_WARN("existing DRM session for %s has different key in slot %d", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), sessionSlot);
		}
		AAMPLOG_WARN("deleting existing DRM session for %s ", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
#ifdef USE_SECMANAGER
		// Cached session is about to be destroyed, send release to SecManager
		if (drmSessionContexts[sessionSlot].drmSession->getSessionId() != AAMP_SECMGR_INVALID_SESSION_ID)
		{
			AampSecManager::GetInstance()->ReleaseSession(drmSessionContexts[sessionSlot].drmSession->getSessionId());
		}
#endif		
		SAFE_DELETE(drmSessionContexts[sessionSlot].drmSession);
	}

	aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_PREPROC);

	drmSessionContexts[sessionSlot].drmSession = AampDrmSessionFactory::GetDrmSession(mLogObj, drmHelper, aampInstance);
	if (drmSessionContexts[sessionSlot].drmSession != NULL)
	{
		AAMPLOG_INFO("Created new DrmSession for DrmSystemId %s", systemId.c_str());
		drmSessionContexts[sessionSlot].data = keyIdArray;
		code = drmSessionContexts[sessionSlot].drmSession->getState();
		// exception : by default for all types of drm , outputprotection is not handled in player 
		// for playready , its configured within player 
		if (systemId == PLAYREADY_KEY_SYSTEM_STRING && aampInstance->mConfig->IsConfigSet(eAAMPConfig_EnablePROutputProtection))
		{
			drmSessionContexts[sessionSlot].drmSession->setOutputProtection(true);
			drmHelper->setOutputProtectionFlag(true);
		}
	}
	else
	{
		AAMPLOG_WARN("Unable to Get DrmSession for DrmSystemId %s", systemId.c_str());
		eventHandle->setFailure(AAMP_TUNE_DRM_INIT_FAILED);
	}

#if defined(USE_OPENCDM_ADAPTER)
	drmSessionContexts[sessionSlot].drmSession->setKeyId(keyIdArray);
#endif

	return code;
}

/**
 * @brief Initialize the Drm System with InitData(PSSH)
 */
KeyState AampDRMSessionManager::initializeDrmSession(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, DrmMetaDataEventPtr eventHandle,  PrivateInstanceAAMP* aampInstance)
{
	KeyState code = KEY_ERROR;

	std::vector<uint8_t> drmInitData;
	drmHelper->createInitData(drmInitData);

	AampMutexHold sessionMutex(drmSessionContexts[sessionSlot].sessionMutex);
	std::string customData = aampInstance->GetLicenseCustomData();
	AAMPLOG_INFO("DRM session Custom Data - %s ", customData.empty()?"NULL":customData.c_str());
	drmSessionContexts[sessionSlot].drmSession->generateAampDRMSession(drmInitData.data(), drmInitData.size(), customData);

	code = drmSessionContexts[sessionSlot].drmSession->getState();
	if (code != KEY_INIT)
	{
		AAMPLOG_ERR("DRM session was not initialized : Key State %d ", code);
		if (code == KEY_ERROR_EMPTY_SESSION_ID)
		{
			AAMPLOG_ERR("DRM session ID is empty: Key State %d ", code);
			eventHandle->setFailure(AAMP_TUNE_DRM_SESSIONID_EMPTY);
		}
		else
		{
			eventHandle->setFailure(AAMP_TUNE_DRM_DATA_BIND_FAILED);
		}
	}

	return code;
}

/**
 * @brief sent license challenge to the DRM server and provide the respone to CDM
 */
KeyState AampDRMSessionManager::acquireLicense(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
		DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, MediaType streamType)
{
	shared_ptr<DrmData> licenseResponse;
	int32_t httpResponseCode = -1;
	int32_t httpExtendedStatusCode = -1;
	KeyState code = KEY_ERROR;
	if (drmHelper->isExternalLicense())
	{
		// External license, assuming the DRM system is ready to proceed
		code = KEY_PENDING;
		if(drmHelper->friendlyName().compare("Verimatrix") == 0)
		{
			licenseResponse = std::make_shared<DrmData>();
			drmHelper->transformLicenseResponse(licenseResponse);
		}
	}
	else
	{
		AampMutexHold sessionMutex(drmSessionContexts[sessionSlot].sessionMutex);
		
		/**
		 * Generate a License challenge from the CDM
		 */
		AAMPLOG_INFO("Request to generate license challenge to the aampDRMSession(CDM)");

		AampChallengeInfo challengeInfo;
		challengeInfo.data.reset(drmSessionContexts[sessionSlot].drmSession->aampGenerateKeyRequest(challengeInfo.url, drmHelper->licenseGenerateTimeout()));
		code = drmSessionContexts[sessionSlot].drmSession->getState();

		if (code != KEY_PENDING)
		{
			AAMPLOG_ERR("Error in getting license challenge : Key State %d ", code);
			aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_PREPROC, AAMP_TUNE_DRM_CHALLENGE_FAILED);
			eventHandle->setFailure(AAMP_TUNE_DRM_CHALLENGE_FAILED);
			aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_PREPROC);
		}
		else
		{
			/** flag for authToken set externally by app **/
			bool usingAppDefinedAuthToken = !aampInstance->mSessionToken.empty();
			bool anonymouslicReq 	=	 aampInstance->mConfig->IsConfigSet(eAAMPConfig_AnonymousLicenseRequest);
			aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_PREPROC);

			if (!(drmHelper->getDrmMetaData().empty() || anonymouslicReq))
			{
				AampMutexHold accessTokenMutexHold(accessTokenMutex);

				int tokenLen = 0;
				long tokenError = 0;
				char *sessionToken = NULL;
				if(!usingAppDefinedAuthToken)
				{ /* authToken not set externally by app */
					sessionToken = (char *)getAccessToken(tokenLen, tokenError , aampInstance->mConfig->IsConfigSet(eAAMPConfig_SslVerifyPeer));
					AAMPLOG_WARN("Access Token from AuthServer");
				}
				else
				{
					sessionToken = (char *)aampInstance->mSessionToken.c_str();
					tokenLen = aampInstance->mSessionToken.size();
					AAMPLOG_WARN("Got Access Token from External App");
				}
				if (NULL == sessionToken)
				{
					// Failed to get access token
					// licenseAnonymousRequest is not set, Report failure
					AAMPLOG_WARN("failed to get access token, Anonymous request not enabled");
					eventHandle->setFailure(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN);
					eventHandle->setResponseCode(tokenError);
					if(!licenseRequestAbort)
					{
						// report error
						return KEY_ERROR;
					}
				}
				else
				{
					AAMPLOG_INFO("access token is available");
					challengeInfo.accessToken = std::string(sessionToken, tokenLen);
				}
			}
			if(licenseRequestAbort)
			{
				AAMPLOG_ERR("Error!! License request was aborted. Resetting session slot %d", sessionSlot);
				eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
				eventHandle->setResponseCode(CURLE_ABORTED_BY_CALLBACK);
				return KEY_ERROR;
			}

			AampLicenseRequest licenseRequest;
			DRMSystems drmType = GetDrmSystem(drmHelper->getUuid());
			licenseRequest.url = aampInstance->GetLicenseServerUrlForDrm(drmType);
			licenseRequest.licenseAnonymousRequest = anonymouslicReq;
			drmHelper->generateLicenseRequest(challengeInfo, licenseRequest);
			if (code != KEY_PENDING || ((licenseRequest.method == AampLicenseRequest::POST) && (!challengeInfo.data.get())))
			{
				AAMPLOG_ERR("Error!! License challenge was not generated by the CDM : Key State %d", code);
				eventHandle->setFailure(AAMP_TUNE_DRM_CHALLENGE_FAILED);
			}
			else
			{
				/**
				 * Configure the License acquisition parameters
				 */
				std::string licenseServerProxy;
				bool isContentMetadataAvailable = configureLicenseServerParameters(drmHelper, licenseRequest, licenseServerProxy, challengeInfo, aampInstance);

				/**
				 * Perform License acquistion by invoking http license request to license server
				 */
				AAMPLOG_WARN("Request License from the Drm Server %s", licenseRequest.url.c_str());
				aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_NETWORK);

#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
				if (isContentMetadataAvailable)
				{
					eventHandle->setSecclientError(true);
					licenseResponse.reset(getLicenseSec(licenseRequest, drmHelper, challengeInfo, aampInstance, &httpResponseCode, &httpExtendedStatusCode, eventHandle));
					
					bool sec_accessTokenExpired = aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager) && SECMANGER_DRM_FAILURE == httpResponseCode && SECMANGER_ACCTOKEN_EXPIRED == httpExtendedStatusCode;
					// Reload Expired access token only on http error code 412 with status code 401
					if (((412 == httpResponseCode && 401 == httpExtendedStatusCode) || sec_accessTokenExpired) && !usingAppDefinedAuthToken)
					{
						AAMPLOG_INFO("License Req failure by Expired access token httpResCode %d statusCode %d", httpResponseCode, httpExtendedStatusCode);
						if(accessToken)
						{
							free(accessToken);
							accessToken = NULL;
							accessTokenLen = 0;
						}
						int tokenLen = 0;
						long tokenError = 0;
						const char *sessionToken = getAccessToken(tokenLen, tokenError,aampInstance->mConfig->IsConfigSet(eAAMPConfig_SslVerifyPeer));
						if (NULL != sessionToken)
						{
							AAMPLOG_INFO("Requesting License with new access token");
							challengeInfo.accessToken = std::string(sessionToken, tokenLen);
							httpResponseCode = httpExtendedStatusCode = -1;
							licenseResponse.reset(getLicenseSec(licenseRequest, drmHelper, challengeInfo, aampInstance, &httpResponseCode, &httpExtendedStatusCode, eventHandle));
						}
					}
				}
				else
#endif
				{
					if(usingAppDefinedAuthToken)
					{
						AAMPLOG_WARN("Ignore  AuthToken Provided for non-ContentMetadata DRM license request");
					}
					eventHandle->setSecclientError(false);
					licenseResponse.reset(getLicense(licenseRequest, &httpResponseCode, streamType, aampInstance, isContentMetadataAvailable, licenseServerProxy));
				}

			}
		}
	}

	if (code == KEY_PENDING)
	{
		code = handleLicenseResponse(drmHelper, sessionSlot, cdmError, httpResponseCode, httpExtendedStatusCode, licenseResponse, eventHandle, aampInstance);
	}

	return code;
}


KeyState AampDRMSessionManager::handleLicenseResponse(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError, int32_t httpResponseCode, int32_t httpExtendedStatusCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aamp)
{
	if (!drmHelper->isExternalLicense())
	{
		if ((licenseResponse != NULL) && (licenseResponse->getDataLength() != 0))
		{
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);

#if !defined(USE_SECCLIENT) && !defined(USE_SECMANAGER)
			if (!drmHelper->getDrmMetaData().empty())
			{
				/*
					Licence response from MDS server is in JSON form
					Licence to decrypt the data can be found by extracting the contents for JSON key licence
					Format : {"licence":"b64encoded licence","accessAttributes":"0"}
				*/
				string jsonStr(licenseResponse->getData().c_str(), licenseResponse->getDataLength());

				try
				{
					AampJsonObject jsonObj(jsonStr);

					std::vector<uint8_t> keyData;
					if (!jsonObj.get(LICENCE_RESPONSE_JSON_LICENCE_KEY, keyData, AampJsonObject::ENCODING_BASE64))
					{
						AAMPLOG_WARN("Unable to retrieve license from JSON response", jsonStr.c_str());
					}
					else
					{
						licenseResponse = make_shared<DrmData>(keyData.data(), keyData.size());
					}
				}
				catch (AampJsonParseException& e)
				{
					AAMPLOG_WARN("Failed to parse JSON response", jsonStr.c_str());
				}
			}
#endif
			AAMPLOG_INFO("license acquisition completed");
			drmHelper->transformLicenseResponse(licenseResponse);
		}
		else
		{
			aamp->profiler.ProfileError(PROFILE_BUCKET_LA_NETWORK, httpResponseCode);
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);

			AAMPLOG_ERR("Error!! Invalid License Response was provided by the Server");
			
			bool SecAuthFailure = false, SecTimeout = false;
			
			//Handle secmaanger specific error codes here
			if(ISCONFIGSET(eAAMPConfig_UseSecManager))
			{
				//The documentation for secmanager error codes are here
				//https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+-+SecManagerApi
				eventHandle->setResponseCode(httpResponseCode);
				eventHandle->setSecManagerReasonCode(httpExtendedStatusCode);
				if(SECMANGER_DRM_FAILURE == httpResponseCode && SECMANGER_ENTITLEMENT_FAILURE == httpExtendedStatusCode)
				{
					if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
					{
						eventHandle->setFailure(AAMP_TUNE_AUTHORISATION_FAILURE);
					}
					AAMPLOG_WARN("DRM session for %s, Authorisation failed", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
				}
				else if(SECMANGER_DRM_FAILURE == httpResponseCode && SECMANGER_SERVICE_TIMEOUT == httpExtendedStatusCode)
				{
					eventHandle->setFailure(AAMP_TUNE_LICENCE_TIMEOUT);
				}
				else
				{
					eventHandle->setFailure(AAMP_TUNE_LICENCE_REQUEST_FAILED);
				}
			}
			else if (412 == httpResponseCode)
			{
				if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
				{
					eventHandle->setFailure(AAMP_TUNE_AUTHORISATION_FAILURE);
				}
				AAMPLOG_WARN("DRM session for %s, Authorisation failed", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());

			}
			else if (CURLE_OPERATION_TIMEDOUT == httpResponseCode)
			{
				eventHandle->setFailure(AAMP_TUNE_LICENCE_TIMEOUT);
			}
			else if(CURLE_ABORTED_BY_CALLBACK == httpResponseCode || CURLE_WRITE_ERROR == httpResponseCode)
			{
				// Set failure reason as AAMP_TUNE_DRM_SELF_ABORT to avoid unnecessary error reporting.
				eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
				eventHandle->setResponseCode(httpResponseCode);
			}
			else
			{
				eventHandle->setFailure(AAMP_TUNE_LICENCE_REQUEST_FAILED);
				eventHandle->setResponseCode(httpResponseCode);
			}
			return KEY_ERROR;
		}
	}

	return processLicenseResponse(drmHelper, sessionSlot, cdmError, licenseResponse, eventHandle, aamp);
}


KeyState AampDRMSessionManager::processLicenseResponse(std::shared_ptr<AampDrmHelper> drmHelper, int sessionSlot, int &cdmError,
		shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance)
{
	/**
	 * Provide the acquired License response from the DRM license server to the CDM.
	 * For DRMs with external license acquisition, we will provide a NULL response
	 * for processing and the DRM session should await the key from the DRM implementation
	 */
	AAMPLOG_INFO("Updating the license response to the aampDRMSession(CDM)");
	aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_POSTPROC);
	cdmError = drmSessionContexts[sessionSlot].drmSession->aampDRMProcessKey(licenseResponse.get(), drmHelper->keyProcessTimeout());

	aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_POSTPROC);


	KeyState code = drmSessionContexts[sessionSlot].drmSession->getState();

	if (code == KEY_ERROR)
	{
		if (AAMP_TUNE_FAILURE_UNKNOWN == eventHandle->getFailure())
		{
			// check if key failure is due to HDCP , if so report it appropriately instead of Failed to get keys
			if (cdmError == HDCP_OUTPUT_PROTECTION_FAILURE || cdmError == HDCP_COMPLIANCE_CHECK_FAILURE)
			{
				eventHandle->setFailure(AAMP_TUNE_HDCP_COMPLIANCE_ERROR);
			}
			else
			{
				eventHandle->setFailure(AAMP_TUNE_DRM_KEY_UPDATE_FAILED);
			}
		}
	}
	else if (code == KEY_PENDING)
	{
		AAMPLOG_ERR(" Failed to get DRM keys");
		if (AAMP_TUNE_FAILURE_UNKNOWN == eventHandle->getFailure())
		{
			eventHandle->setFailure(AAMP_TUNE_INVALID_DRM_KEY);
		}
	}

	return code;
}

/**
 * @brief Configure the Drm license server parameters for URL/proxy and custom http request headers
 */
bool AampDRMSessionManager::configureLicenseServerParameters(std::shared_ptr<AampDrmHelper> drmHelper, AampLicenseRequest &licenseRequest,
		std::string &licenseServerProxy, const AampChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance)
{
	string contentMetaData = drmHelper->getDrmMetaData();
	bool isContentMetadataAvailable = !contentMetaData.empty();

	if (!contentMetaData.empty())
	{
		if (!licenseRequest.url.empty())
		{
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
			licenseRequest.url = getFormattedLicenseServerURL(licenseRequest.url);
#endif
		}
	}

	if(!isContentMetadataAvailable)
	{
		std::unordered_map<std::string, std::vector<std::string>> customHeaders;
		aampInstance->GetCustomLicenseHeaders(customHeaders);

		if (!customHeaders.empty())
		{
			// Override headers from the helper with custom headers
			licenseRequest.headers = customHeaders;
		}

		if (isContentMetadataAvailable)
		{
#ifdef AAMP_RFC_ENABLED
			std:string lhrAcceptValue = RFCSettings::getLHRAcceptValue();
			std::string lhrContentType = RFCSettings::getLRHContentType();
			// Content metadata is available, Add corresponding headers 
			if (customHeaders.empty())
			{
				// Not using custom headers, These headers will also override any headers from the helper
				licenseRequest.headers.clear();
			}
			licenseRequest.headers.insert({LICENCE_REQUEST_HEADER_ACCEPT, {lhrAcceptValue.c_str()}});
			licenseRequest.headers.insert({LICENCE_REQUEST_HEADER_CONTENT_TYPE, {lhrContentType.c_str()}});
#endif
		}
		licenseServerProxy = aampInstance->GetLicenseReqProxy(); 
	}

	return isContentMetadataAvailable;
}

/**
 * @brief Resethe current seesion ID
 */
void AampDRMSessionManager::notifyCleanup()
{
#ifdef USE_SECMANAGER
	if(AAMP_SECMGR_INVALID_SESSION_ID != mSessionId)
	{
		// Set current session to inactive
		AampSecManager::GetInstance()->UpdateSessionState(mSessionId, false);
		// Reset the session ID, the session ID is preserved within AampDrmSession instances
		mSessionId = AAMP_SECMGR_INVALID_SESSION_ID;
	}
#endif
}

void AampDRMSessionManager::ContentProtectionDataUpdate(PrivateInstanceAAMP* aampInstance, std::vector<uint8_t> keyId, MediaType streamType)
{
	std::string contentType = sessionTypeName[streamType];
	int iter1 = 0;
	bool DRM_Config_Available = false;

	while (iter1 < aampInstance->vDynamicDrmData.size())
	{
		DynamicDrmInfo dynamicDrmCache = aampInstance->vDynamicDrmData.at(iter1);
		if(keyId == dynamicDrmCache.keyID) 
		{
			AAMPLOG_WARN("DrmConfig already present in cached data in slot : %d applying config",iter1);
			std::map<std::string,std::string>::iterator itr;
			for(itr = dynamicDrmCache.licenseEndPoint.begin();itr!=dynamicDrmCache.licenseEndPoint.end();itr++)
			{
				if(strcasecmp("com.microsoft.playready",itr->first.c_str())==0)
				{
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,itr->second);
				}
				if(strcasecmp("com.widevine.alpha",itr->first.c_str())==0)
				{
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,itr->second);
				}
				if(strcasecmp("org.w3.clearkey",itr->first.c_str())==0)
				{
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,itr->second);
				}
			}
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_AuthToken,dynamicDrmCache.authToken);
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,dynamicDrmCache.customData);
			DRM_Config_Available = true;
			break;
		}
		iter1++;
	}
	if(!DRM_Config_Available) {
		ContentProtectionDataEventPtr eventData = std::make_shared<ContentProtectionDataEvent>(keyId, contentType);
		std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyId);
		pthread_mutex_lock(&aampInstance->mDynamicDrmUpdateLock);
		int pthreadReturnValue = 0;
		struct timespec ts;
		int drmUpdateTimeout;
		aampInstance->mConfig->GetConfigValue(eAAMPConfig_ContentProtectionDataUpdateTimeout, drmUpdateTimeout);
		AAMPLOG_WARN("Timeout Wait for DRM config message from application :%d",drmUpdateTimeout);
		ts = aamp_GetTimespec(drmUpdateTimeout); /** max delay to update dynamic drm on key rotation **/
		AAMPLOG_INFO("Found new KeyId %s and not in drm config cache, sending ContentProtectionDataEvent to App", keyIdDebugStr.c_str());
		aampInstance->SendEvent(eventData);
		pthreadReturnValue = pthread_cond_timedwait(&aampInstance->mWaitForDynamicDRMToUpdate, &aampInstance->mDynamicDrmUpdateLock, &ts);
		if (ETIMEDOUT == pthreadReturnValue)
		{
			AAMPLOG_WARN("pthread_cond_timedwait returned TIMEOUT, Applying Default config");
			DynamicDrmInfo dynamicDrmCache = aampInstance->mDynamicDrmDefaultconfig;
			std::map<std::string,std::string>::iterator itr;
			for(itr = dynamicDrmCache.licenseEndPoint.begin();itr!=dynamicDrmCache.licenseEndPoint.end();itr++) {
				if(strcasecmp("com.microsoft.playready",itr->first.c_str())==0) {
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,itr->second);
				}
				if(strcasecmp("com.widevine.alpha",itr->first.c_str())==0) {
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,itr->second);
				}
				if(strcasecmp("org.w3.clearkey",itr->first.c_str())==0) {
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,itr->second);
				}
			}
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_AuthToken,dynamicDrmCache.authToken);
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,dynamicDrmCache.customData);
		}
		else if (0 != pthreadReturnValue)
		{
			AAMPLOG_WARN("pthread_cond_timedwait returned %s ", strerror(pthreadReturnValue));
		}
		else{
			AAMPLOG_WARN("%s:%d [WARN] pthread_cond_timedwait(dynamicDrmUpdate) returned success!", __FUNCTION__, __LINE__);
		}
		pthread_mutex_unlock(&aampInstance->mDynamicDrmUpdateLock);
	}
}

/**
 *  @brief		Function to release the DrmSession if it running
 *  @param[out]	aamp private aamp instance
 *  @return		None.
 */
void ReleaseDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp){
		
	if(aamp->drmSessionThreadStarted) //In the case of license rotation
	{
		aamp->createDRMSessionThreadID.join();
		aamp->drmSessionThreadStarted = false;
		AAMPLOG_WARN("DRMSession Thread Released");
	}
}

/**
 *  @brief	Function to spawn the DrmSession Thread based on the
 *              preferred drm set.  
 *  @param[out]	drmData private aamp instance
 *  @return		None.
 */
int SpawnDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp, DrmSessionDataInfo* drmData)
{
	int iState = DRM_API_FAILED;
	do{

		/** API protection added **/
		if (NULL == drmData){
			AAMPLOG_ERR("Could not able to process with the NULL Drm data" 
				);
			break;
		}
		/** Achieve single thread logic for DRM Session Creation **/
		ReleaseDRMLicenseAcquireThread(aamp);
		AAMPLOG_INFO("Creating thread with sessionData = 0x%08x",
					drmData->sessionData );
		try
		{
			aamp->createDRMSessionThreadID = std::thread(CreateDRMSession, drmData->sessionData);
			drmData->isProcessedLicenseAcquire = true;
			aamp->drmSessionThreadStarted = true;
			AAMPLOG_INFO("Thread created for CreateDRMSession [%u]", aamp->createDRMSessionThreadID.get_id());
			aamp->setCurrentDrm(drmData->sessionData->drmHelper);
			iState = DRM_API_SUCCESS;
		}
		catch(const std::exception& e)
		{
			AAMPLOG_ERR("thread creation failed for CreateDRMSession: %s", e.what());
		}
	}while(0);

	return iState;
}

/**
 *  @brief	Thread function to create DRM Session which would be invoked in thread from
 *              HLS , PlayReady or from pipeline  
 *
 *  @param[out]	arg - DrmSessionParams structure with filled data
 *  @return		None.
 */
void CreateDRMSession(void *arg)
{
	if(aamp_pthread_setname(pthread_self(), "aampfMP4DRM"))
	{
		AAMPLOG_ERR("aamp_pthread_setname failed");
	}
	struct DrmSessionParams* sessionParams = (struct DrmSessionParams*)arg;

	if(sessionParams == nullptr) {
		AAMPLOG_ERR("sessionParams is null");
                return;
	}
	if(sessionParams->aamp == nullptr) {
		AAMPLOG_ERR("no aamp in sessionParams");
		return;
	}   //CID:144411 - Reverse_inull
	AampDRMSessionManager* sessionManger = sessionParams->aamp->mDRMSessionManager;
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	bool isSecClientError = true;
#else
	bool isSecClientError = false;
#endif

	sessionParams->aamp->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);

	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError);

	AampDrmSession *drmSession = NULL;
	const char * systemId;

	if (sessionParams->aamp == nullptr) {
		AAMPLOG_ERR("no aamp in sessionParams");
		return;
	}

	if (sessionParams->aamp->mDRMSessionManager == nullptr) {
		AAMPLOG_ERR("no aamp->mDrmSessionManager in sessionParams");
		return;
	}


	if (sessionParams->drmHelper == nullptr)
	{
		AAMPTuneFailure failure = e->getFailure();
		AAMPLOG_ERR("Failed DRM Session Creation,  no helper");
	
		sessionParams->aamp->SendDrmErrorEvent(e, false);
		sessionParams->aamp->profiler.SetDrmErrorCode((int)failure);
		sessionParams->aamp->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int)failure);
		sessionParams->aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
	}
	else
	{
		std::vector<uint8_t> data;
		systemId = sessionParams->drmHelper->getUuid().c_str();
		sessionParams->drmHelper->createInitData(data);
		sessionParams->aamp->mStreamSink->QueueProtectionEvent(systemId, data.data(), data.size(), sessionParams->stream_type);
		drmSession = sessionManger->createDrmSession(sessionParams->drmHelper, e, sessionParams->aamp, sessionParams->stream_type);

		if(NULL == drmSession)
		{
			AAMPLOG_ERR("Failed DRM Session Creation for systemId = %s", systemId);
			AAMPTuneFailure failure = e->getFailure();
			long responseCode = e->getResponseCode();
			bool selfAbort = (failure == AAMP_TUNE_DRM_SELF_ABORT);
			if (!selfAbort)
			{
				bool isRetryEnabled =      (failure != AAMP_TUNE_AUTHORISATION_FAILURE)
							&& (failure != AAMP_TUNE_LICENCE_REQUEST_FAILED)
							&& (failure != AAMP_TUNE_LICENCE_TIMEOUT)
							&& (failure != AAMP_TUNE_DEVICE_NOT_PROVISIONED)
							&& (failure != AAMP_TUNE_HDCP_COMPLIANCE_ERROR);
				sessionParams->aamp->SendDrmErrorEvent(e, isRetryEnabled);
			}
			sessionParams->aamp->profiler.SetDrmErrorCode((int) failure);
			sessionParams->aamp->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int) failure);
			sessionParams->aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);	
		}

		else
		{
			if(e->getAccessStatusValue() != 3)
			{
				AAMPLOG_INFO("Sending DRMMetaData");
				sessionParams->aamp->SendDRMMetaData(e);
			}
			sessionParams->aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
		}
	}
	if(sessionParams->aamp->mIsFakeTune)
	{
		sessionParams->aamp->SetState(eSTATE_COMPLETE);
		sessionParams->aamp->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS));
	}
	return;
}


