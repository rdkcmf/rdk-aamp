/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file AampCurlStore.cpp
 * @brief Advanced Adaptive Media Player (AAMP) Curl store
 */

#include "AampCurlStore.h"
#include "AampDefine.h"

#define CURL_EASY_SETOPT(curl, CURLoption, option)\
	if (curl_easy_setopt(curl, CURLoption, option) != 0) {\
			logprintf("Failed at curl_easy_setopt ");\
	}  //CID:132698,135078 - checked return

// Curl callback functions
static pthread_mutex_t gCurlShMutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * @brief
 * @param curl ptr to CURL instance
 * @param data curl data lock
 * @param acess curl access lock
 * @param user_ptr CurlCallbackContext pointer
 * @retval void
 */
static void curl_lock_callback(CURL *curl, curl_lock_data data, curl_lock_access access, void *user_ptr)
{
	pthread_mutex_t *pCurlShareLock = NULL;
	CurlDataShareLock *locks =(CurlDataShareLock *)user_ptr;

	(void)access; /* unused */
	(void)curl; /* unused */

	if(locks)
	{
		switch ( data )
		{
			case CURL_LOCK_DATA_DNS:
			pCurlShareLock = &(locks->mDnsCurlShareMutex);
			break;
			case CURL_LOCK_DATA_SSL_SESSION:
			pCurlShareLock = &(locks->mSslCurlShareMutex);
			break;

			default:
			pCurlShareLock = &(locks->mCurlSharedlock);
			break;
		}

		pthread_mutex_lock(pCurlShareLock);
	}
	else
	{
		pthread_mutex_lock(&gCurlShMutex);
	}
}

/**
 * @brief
 * @param curl ptr to CURL instance
 * @param data curl data lock
 * @param acess curl access lock
 * @param user_ptr CurlCallbackContext pointer
 * @retval void
 */
static void curl_unlock_callback(CURL *curl, curl_lock_data data, void *user_ptr)
{
	pthread_mutex_t *pCurlShareLock = NULL;
	CurlDataShareLock *locks =(CurlDataShareLock *)user_ptr;

	(void)curl; /* unused */

	if(locks)
	{
		switch ( data )
		{
			case CURL_LOCK_DATA_DNS:
			pCurlShareLock = &(locks->mDnsCurlShareMutex);
			break;
			case CURL_LOCK_DATA_SSL_SESSION:
			pCurlShareLock = &(locks->mSslCurlShareMutex);
			break;

			default:
			pCurlShareLock = &(locks->mCurlSharedlock);
			break;
		}

		pthread_mutex_unlock(pCurlShareLock);
	}
	else
	{
		pthread_mutex_unlock(&gCurlShMutex);
	}
}

/**
 * @brief write callback to be used by CURL
 * @param ptr pointer to buffer containing the data
 * @param size size of the buffer
 * @param nmemb number of bytes
 * @param userdata CurlCallbackContext pointer
 * @retval size consumed or 0 if interrupted
 */
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t ret = 0;
    CurlCallbackContext *context = (CurlCallbackContext *)userdata;
    if(!context) return ret;

	ret = context->aamp->HandleSSLWriteCallback( ptr, size, nmemb, userdata);
    return ret;
}

/**
 * @brief callback invoked on http header by curl
 * @param ptr pointer to buffer containing the data
 * @param size size of the buffer
 * @param nmemb number of bytes
 * @param user_data  CurlCallbackContext pointer
 * @retval
 */
static size_t header_callback(const char *ptr, size_t size, size_t nmemb, void *user_data)
{
	CurlCallbackContext *context = static_cast<CurlCallbackContext *>(user_data);

	return context->aamp->HandleSSLHeaderCallback(ptr, size, nmemb, user_data);
}

/**
 * @brief
 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
 * @param dltotal total bytes expected to download
 * @param dlnow downloaded bytes so far
 * @param ultotal total bytes expected to upload
 * @param ulnow uploaded bytes so far
 * @retval
 */
static int progress_callback(
	void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
	double dltotal, // total bytes expected to download
	double dlnow, // downloaded bytes so far
	double ultotal, // total bytes expected to upload
	double ulnow // uploaded bytes so far
	)
{
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;

	return context->aamp->HandleSSLProgressCallback ( clientp, dltotal, dlnow, ultotal, ulnow );
}

/**
 * @brief
 * @param curl ptr to CURL instance
 * @param ssl_ctx SSL context used by CURL
 * @param user_ptr data pointer set as param to CURLOPT_SSL_CTX_DATA
 * @retval CURLcode CURLE_OK if no errors, otherwise corresponding CURL code
 */
CURLcode ssl_callback(CURL *curl, void *ssl_ctx, void *user_ptr)
{
	PrivateInstanceAAMP *context = (PrivateInstanceAAMP *)user_ptr;
	AAMPLOG_WARN("priv aamp :%p", context);
	CURLcode rc = CURLE_OK;
	pthread_mutex_lock(&context->mLock);
	if (!context->mDownloadsEnabled)
	{
		rc = CURLE_ABORTED_BY_CALLBACK ; // CURLE_ABORTED_BY_CALLBACK
	}
	pthread_mutex_unlock(&context->mLock);
	return rc;
}
/**
 * @fn GetCurlHandle
 * @brief GetCurlHandle - Get a free curl easy handle for given url & curl index
 */
CURL* CurlStore::GetCurlHandle(void *pAamp,std::string url, AampCurlInstance startIdx )
{
	CURL * curl = NULL;
	assert (startIdx <= eCURLINSTANCE_MAX);

	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)pAamp;
	std::string HostName;
	int protocol = 0, IsRemotehost = 1;
	HostName = aamp_getHostFromURL ( url, &protocol, &IsRemotehost );

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && ( IsRemotehost ) /*&& ( eAAMP_HTTPS_PROTOCOL == protocol )*/ )
	{
		GetFromCurlStore ( HostName, startIdx, &curl );
	}
	else
	{
		curl = curl_easy_init();
	}

	return curl;
}

/**
 * @fn SaveCurlHandle
 * @brief SaveCurlHandle - Save a curl easy handle for given host & curl index
 */
void CurlStore::SaveCurlHandle (void *pAamp, std::string url, AampCurlInstance startIdx, CURL *curl )
{
	assert (startIdx <= eCURLINSTANCE_MAX);

	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)pAamp;
	std::string HostName;
	int protocol = 0, IsRemotehost = 1;
	HostName = aamp_getHostFromURL ( url, &protocol, &IsRemotehost );

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && ( IsRemotehost ) /*&& ( eAAMP_HTTPS_PROTOCOL == protocol )*/ )
	{
		KeepInCurlStore ( HostName, startIdx, curl, false );
	}
	else
	{
		curl_easy_cleanup(curl);
	}
}

/**
 * @fn CurlInit
 * @brief CurlInit - Initialize or get easy handles for given host & curl index from curl store
 */
void CurlStore::CurlInit(void *privContext, AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName)
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)privContext;
	std::string UserAgentString;
	UserAgentString=aamp->mConfig->GetUserAgentString();

	int instanceEnd = startIdx + instanceCount;
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	std::string HostName;
	int protocol = 0, IsRemotehost = 1;
	AampCurlStoreErrorCode CurlStoreErrCode=eCURL_STORE_HOST_NOT_AVAILABLE;
	HostName = aamp_getHostFromURL ( aamp->mManifestUrl, &protocol, &IsRemotehost );

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && ( IsRemotehost ) /*&& ( eAAMP_HTTPS_PROTOCOL == protocol )*/ )
	{
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			aamp->curl[i]=NULL;
		}
		AAMPLOG_INFO("Check curl store for host:%s inst:%d-%d %p\n", HostName.c_str(), startIdx, instanceEnd, aamp->curl[startIdx] );
		CurlStoreErrCode = GetFromCurlStoreBulk(HostName, startIdx, instanceEnd, aamp );
		AAMPLOG_INFO("From curl store for inst:%d-%d %p ShHdl:%p\n", startIdx, instanceEnd, aamp->curl[startIdx], aamp->mCurlShared );
	}
	else
	{
		aamp->mCurlShared = curl_share_init();
		curl_share_setopt(aamp->mCurlShared, CURLSHOPT_USERDATA, (void*)NULL);
		curl_share_setopt(aamp->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
		curl_share_setopt(aamp->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
		curl_share_setopt(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
//		curl_share_setopt(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

		if (ISCONFIGSET(eAAMPConfig_EnableSharedSSLSession))
		{
			curl_share_setopt(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		}
	}

	for (unsigned int i = startIdx; i < instanceEnd; i++)
	{
		if (!aamp->curl[i])
		{
			aamp->curl[i] = curl_easy_init();

			AAMPLOG_INFO("Created new curl handle:%p for inst:%d", aamp->curl[i], i );

			if (ISCONFIGSET(eAAMPConfig_CurlLogging))
			{
				CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_VERBOSE, 1L);
			}
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_NOSIGNAL, 1L);
			//curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback); // unused
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_PROGRESSFUNCTION, progress_callback);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_HEADERFUNCTION, header_callback);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_WRITEFUNCTION, write_callback);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_TIMEOUT, DEFAULT_CURL_TIMEOUT);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_CONNECTTIMEOUT, DEFAULT_CURL_CONNECTTIMEOUT);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_FOLLOWLOCATION, 1L);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_NOPROGRESS, 0L); // enable progress meter (off by default)

			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_USERAGENT, UserAgentString.c_str());
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_ACCEPT_ENCODING, "");//Enable all the encoding formats supported by client
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_SSL_CTX_FUNCTION, ssl_callback); //Check for downloads disabled in btw ssl handshake
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_SSL_CTX_DATA, aamp);
			long dns_cache_timeout = 3*60;
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_DNS_CACHE_TIMEOUT, dns_cache_timeout);
			CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_SHARE, aamp->mCurlShared);
		//	CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_TCP_KEEPALIVE, 1 );

			aamp->curlDLTimeout[i] = DEFAULT_CURL_TIMEOUT * 1000;

			if (!proxyName.empty())
			{
				/* use this proxy */
				CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_PROXY, proxyName.c_str());
				/* allow whatever auth the proxy speaks */
				CURL_EASY_SETOPT(aamp->curl[i], CURLOPT_PROXYAUTH, CURLAUTH_ANY);
			}
			//log_current_time("curl initialized");
		}
	}

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && ( IsRemotehost ) /*&& ( eAAMP_HTTPS_PROTOCOL == protocol )*/ && ( eCURL_STORE_HOST_SOCK_AVAILABLE != CurlStoreErrCode ) )
	{
		AAMPLOG_INFO("Keep new curl handle:%p in Curlstore for inst:%d-%d", aamp->curl[startIdx], startIdx, instanceEnd );
		KeepInCurlStoreBulk( HostName, startIdx, instanceEnd, aamp, true);
	}
}

/**
 * @fn CurlTerm
 * @brief CurlTerm - Terminate or store easy handles in curlstore
 */
void CurlStore::CurlTerm(void *privContext, AampCurlInstance startIdx, unsigned int instanceCount)
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)privContext;
	int instanceEnd = startIdx + instanceCount;
	std::string HostName;
	int protocol = 0, IsRemotehost = 1;
	HostName = aamp_getHostFromURL ( aamp->mManifestUrl, &protocol, &IsRemotehost );

	if( ISCONFIGSET(eAAMPConfig_EnableCurlStore)  && ( IsRemotehost )/*&& ( eAAMP_HTTPS_PROTOCOL == protocol )*/)
	{
		AAMPLOG_INFO("Keep unused curl handle:%p in Curlstore for inst:%d-%d", aamp->curl[startIdx], startIdx, instanceEnd );
		KeepInCurlStoreBulk ( HostName, startIdx, instanceEnd, aamp, false);
	}
	else
	{
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			if (aamp->curl[i])
			{
				curl_easy_cleanup(aamp->curl[i]);
				aamp->curl[i] = NULL;
				aamp->curlDLTimeout[i] = 0;
			}
		}
	}
}

CurlStore* CurlStore::CurlInstance = NULL;
pthread_mutex_t CurlStore::mCurlInstLock = PTHREAD_MUTEX_INITIALIZER;
int CurlStore::MaxCurlSockStore=MAX_CURL_SOCK_STORE;

/**
 * @fn GetCurlStoreInstance
 * @brief GetCurlStoreInstance - Get static curlstore singleton object
 */
CurlStore *CurlStore::GetCurlStoreInstance ( void *pContext )
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)pContext;

	pthread_mutex_lock(&mCurlInstLock);
	if ( NULL == CurlInstance )
	{
		int MaxSockStoreSize=0;
		CurlInstance = new CurlStore();

		if ( NULL == CurlInstance )
		{
			AAMPLOG_WARN("Failed to alloc mem for Curl instance");
			return NULL;
		}

		GETCONFIGVALUE(eAAMPConfig_MaxCurlSockStore,MaxSockStoreSize);
		if( MAX_CURL_SOCK_STORE != MaxSockStoreSize)
		{
			MaxCurlSockStore = MaxSockStoreSize;
		}
		AAMPLOG_INFO("Max sock store size:%d", MaxCurlSockStore);
	}
	pthread_mutex_unlock(&mCurlInstLock);
	return CurlInstance;
}

/**
 * @fn GetFromCurlStoreBulk
 * @brief GetFromCurlStoreBulk - Get free curl easy handle in bulk for given host & curl indices
 */
AampCurlStoreErrorCode CurlStore::GetFromCurlStoreBulk ( std::string hostname, AampCurlInstance CurlIndex, int count, void *priv )
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP*)priv;
	AampCurlStoreErrorCode ret = eCURL_STORE_HOST_SOCK_AVAILABLE;

	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		int CurlFDIndex=0;
		CurlSocketStoreStruct *CurlSock = it->second;
		aamp->mCurlShared = CurlSock->mCurlShared;

		for( int loop = (int)CurlIndex; loop < count; ++loop )
		{
			for ( CurlFDIndex = loop; CurlFDIndex < eCURLINSTANCE_MAX_PLUSBG; CurlFDIndex += eCURLINSTANCE_MAX )
			{
				if ( CurlSock->curl[CurlFDIndex] && !CurlSock->aui8InUse[CurlFDIndex] )
				{
					aamp->curl[loop] = CurlSock->curl[CurlFDIndex];
					CurlSock->aui8InUse[CurlFDIndex] = true;
					CURL_EASY_SETOPT(aamp->curl[loop], CURLOPT_SSL_CTX_DATA, aamp);

					AAMPLOG_INFO("Curl Inst %d for %s Curl hdl:%p at %d in store", loop, hostname.c_str(), aamp->curl[loop], CurlFDIndex);
					break;
				}
			}

			if (!( CurlFDIndex < eCURLINSTANCE_MAX_PLUSBG ))
			{
				AAMPLOG_WARN("Existing instances in store are busy, Let's create hdl for Curl inst:%d" , loop );
				ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
			}
		}

		if ( umCurlSockDataStore.size() > MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_INFO("Curl Inst %d for %s not in store", CurlIndex, hostname.c_str());
		ret = eCURL_STORE_HOST_NOT_AVAILABLE;

		CurlSocketStoreStruct *CurlSock = new curlstorestruct();
		CurlDataShareLock *locks = new curldatasharelock();
		if ( NULL == CurlSock || NULL == locks )
		{
			AAMPLOG_WARN("Failed to alloc memory for curl store");
			return ret;
		}

		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->pstShareLocks = locks;

		CURLSHcode sh;
		aamp->mCurlShared = CurlSock->mCurlShared = curl_share_init();
		sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_USERDATA, (void*)locks);
		sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
		sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
		sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
//		sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

		if (ISCONFIGSET(eAAMPConfig_EnableSharedSSLSession))
		{
			curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		}

		if ( umCurlSockDataStore.size() >= MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}

		umCurlSockDataStore[hostname]=CurlSock;
		AAMPLOG_INFO("Curl Inst %d for %s created, Added shared ctx %p in curlstore %p, size:%d maxsize:%d", CurlIndex, hostname.c_str(),
						CurlSock->mCurlShared, CurlSock, umCurlSockDataStore.size(), MaxCurlSockStore);
	}

	ShowCurlStoreData();

	pthread_mutex_unlock(&mCurlInstLock);
	return ret;
}

/**
 * @fn GetFromCurlStore
 * @brief GetFromCurlStore - Get a free curl easy handle for given host & curl index
 */
AampCurlStoreErrorCode CurlStore::GetFromCurlStore ( std::string hostname, AampCurlInstance CurlIndex, CURL **curl )
{
	AampCurlStoreErrorCode ret = eCURL_STORE_HOST_SOCK_AVAILABLE;
	CurlSocketStoreStruct *CurlSock = NULL;
	int CurlFDIndex=CurlIndex;
	*curl = NULL;

	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		CurlSock = it->second;

		for ( CurlFDIndex = CurlIndex; CurlFDIndex < eCURLINSTANCE_MAX_PLUSBG; CurlFDIndex += eCURLINSTANCE_MAX )
		{
			if ( CurlSock->curl[CurlFDIndex] && !CurlSock->aui8InUse[CurlFDIndex] )
			{
				*curl = CurlSock->curl[CurlFDIndex];
				CurlSock->aui8InUse[CurlFDIndex] = true;

				AAMPLOG_INFO("Curl Inst %d for %s Curl hdl:%p at %d in store", CurlIndex, hostname.c_str(), *curl, CurlFDIndex);
				break;
			}
			
			if (NULL == CurlSock->curl[CurlFDIndex])
			{
				break;
			}
		}

		if (!( CurlFDIndex < eCURLINSTANCE_MAX_PLUSBG ))
		{
			AAMPLOG_WARN("Existing instances in store are busy, No free slot available for %d inst" , CurlIndex );
			ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
		}
	}

	if ( NULL == *curl && eCURL_STORE_SOCK_NOT_AVAILABLE != ret)
	{
		AAMPLOG_INFO("Curl Inst %d for %s not available", CurlIndex, hostname.c_str());

		if(NULL == CurlSock)
		{
			ret = eCURL_STORE_HOST_NOT_AVAILABLE;

			CurlSocketStoreStruct *newCurlSock = new curlstorestruct();
			CurlDataShareLock *locks = new curldatasharelock();
			if ( NULL == newCurlSock || NULL == locks )
			{
				AAMPLOG_WARN("Failed to alloc memory for curl store");
				return ret;
			}

			newCurlSock->pstShareLocks = locks;

			CURLSHcode sh;
			newCurlSock->mCurlShared = curl_share_init();
			sh = curl_share_setopt(newCurlSock->mCurlShared, CURLSHOPT_USERDATA, (void*)locks);
			sh = curl_share_setopt(newCurlSock->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
			sh = curl_share_setopt(newCurlSock->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
			sh = curl_share_setopt(newCurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
			curl_share_setopt(newCurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);

			CurlSock = newCurlSock;

			if ( umCurlSockDataStore.size() >= MaxCurlSockStore )
			{
				// Remove not recently used handle.
				RemoveCurlSock();
			}

			umCurlSockDataStore[hostname]=CurlSock;
			AAMPLOG_INFO("Curl Inst %d for %s created, Added shared ctx %p in curlstore %p, size:%d maxsize:%d", CurlIndex, hostname.c_str(),
							CurlSock->mCurlShared, CurlSock, umCurlSockDataStore.size(), MaxCurlSockStore);
		}

		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->curl[CurlFDIndex] = *curl = curl_easy_init();
		CurlSock->aui8InUse[CurlFDIndex] = true;

		CURL_EASY_SETOPT(*curl, CURLOPT_SHARE, CurlSock->mCurlShared);
	}

	pthread_mutex_unlock(&mCurlInstLock);
	return ret;
}

/**
 * @fn KeepInCurlStoreBulk
 * @brief KeepInCurlStoreBulk - Store curl easy handle in bulk for given host & curl index
 */
void CurlStore::KeepInCurlStoreBulk ( std::string hostname, AampCurlInstance CurlIndex, int count, void *priv, bool inuse )
{
	PrivateInstanceAAMP *aamp =  (PrivateInstanceAAMP*)priv;

	CurlSocketStoreStruct *CurlSock = NULL;
	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if(it != umCurlSockDataStore.end())
	{
		int index;
		CurlSock = it->second;
		for( int loop = (int)CurlIndex; loop < count; ++loop )
		{
			for ( index = loop; index < eCURLINSTANCE_MAX_PLUSBG; index += eCURLINSTANCE_MAX )
			{
				if ((NULL == CurlSock->curl[index]) || (aamp->curl[loop] == CurlSock->curl[index]))
				{
					CurlSock->curl[index]=aamp->curl[loop];
					CurlSock->aui8InUse[index]=inuse;
					AAMPLOG_INFO("Curl Inst %d for %s CurlCtx:%p stored at %d", loop, hostname.c_str(),aamp->curl[loop], index);
					break;
				}
			}

			if (!(index < eCURLINSTANCE_MAX_PLUSBG))
			{
				AAMPLOG_WARN("Curl %p for %s not stored, exceeded max limit of bg instances", aamp->curl[loop], hostname.c_str());
			}
		}
		CurlSock->timestamp = aamp_GetCurrentTimeMS();

		if ( umCurlSockDataStore.size() > MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_INFO("Host %s not in store, Let's create curl store Add Curl Inst %d-%d", hostname.c_str(), CurlIndex,count);
		CurlSock = new curlstorestruct();
		CurlDataShareLock *locks = new curldatasharelock();
		if ( NULL == CurlSock || NULL == locks )
		{
			AAMPLOG_WARN("Failed to alloc memory for curl store");
			return;
		}

		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->pstShareLocks = locks;

		for( int loop = (int)CurlIndex; loop < count; ++loop )
		{
			CurlSock->curl[loop]=aamp->curl[loop];
			CurlSock->aui8InUse[loop]=inuse;
		}

		if ( umCurlSockDataStore.size() >= MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}

		umCurlSockDataStore[hostname]=CurlSock;
		AAMPLOG_INFO("Curl Inst %d for %s created, Added curlcontext %d in curlstore, size:%d maxsize:%d", CurlIndex, hostname.c_str(),
						CurlIndex, umCurlSockDataStore.size(), MaxCurlSockStore);
	}

	pthread_mutex_unlock(&mCurlInstLock);
}

/**
 * @fn KeepInCurlStore
 * @brief KeepInCurlStore - Store a curl easy handle for given host & curl index
 */
void CurlStore::KeepInCurlStore ( std::string hostname, AampCurlInstance CurlIndex, CURL *curl, bool inuse )
{
	CurlSocketStoreStruct *CurlSock = NULL;
	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);
	if(it != umCurlSockDataStore.end())
	{
		int index = (int)CurlIndex;
		AAMPLOG_INFO("Curl Inst %d for %s available, Context:%p, Let's add to curl store", CurlIndex, hostname.c_str(),curl);
		CurlSock = it->second;

		for ( index = CurlIndex; index < eCURLINSTANCE_MAX_PLUSBG; index += eCURLINSTANCE_MAX )
		{
			if ((NULL == CurlSock->curl[index]) || (curl == CurlSock->curl[index]))
			{
				CurlSock->curl[index]=curl;
				CurlSock->aui8InUse[index]=inuse;
				AAMPLOG_INFO("Curl Inst %d for %s CurlCtx:%p stored at %d", CurlIndex, hostname.c_str(),curl, index);
				break;
			}
		}

		if (!(index < eCURLINSTANCE_MAX_PLUSBG))
		{
			AAMPLOG_WARN("Curl %p for %s not stored, exceeded max limit of bg instances", curl, hostname.c_str());
		}

		CurlSock->timestamp = aamp_GetCurrentTimeMS();
	}
	else
	{
		AAMPLOG_INFO("Curl Inst %d for %s not available, Context:%p", CurlIndex, hostname.c_str(), curl);
	}
	pthread_mutex_unlock(&mCurlInstLock);
}

/**
 * @fn CheckCurlStoreIsInuse
 * @brief CheckCurlStoreIsInuse - Return the state of curl instance
 */
bool CurlStore::CheckCurlStoreIsInuse ( CurlSocketStoreStruct *CurlSock )
{
	return( CurlSock->aui8InUse[eCURLINSTANCE_VIDEO] || \
			CurlSock->aui8InUse[eCURLINSTANCE_AUDIO] || \
			CurlSock->aui8InUse[eCURLINSTANCE_AES] || \
			CurlSock->aui8InUse[eCURLINSTANCE_DAI] );
}

/**
 * @fn RemoveCurlSock
 * @brief RemoveCurlSock - Remove not recently used entry from curl store
 */
void CurlStore::RemoveCurlSock ( void )
{
	unsigned long long time=-1UL;
	AAMPLOG_INFO("Before remove Curl Sock Store size:%d\n", umCurlSockDataStore.size());

	CurlSockDataIter it=umCurlSockDataStore.begin();
	CurlSockDataIter RemIt=umCurlSockDataStore.end();

	for(; it != umCurlSockDataStore.end(); ++it )
	{
		CurlSocketStoreStruct *CurlSock = it->second;
		if( !(CheckCurlStoreIsInuse(CurlSock)) && ( time > CurlSock->timestamp ) )
		{
			time = CurlSock->timestamp;
			RemIt = it;
		}
	}

	if( umCurlSockDataStore.end() != RemIt )
	{
		CurlSocketStoreStruct *RmCurlSock = RemIt->second;
		AAMPLOG_INFO("Removing host:%s lastused:%lld", (RemIt->first).c_str(), RmCurlSock->timestamp);
		for(int index=0; index<eCURLINSTANCE_MAX_PLUSBG; ++index)
		{
			curl_easy_cleanup(RmCurlSock->curl[index]);
			RmCurlSock->curl[index] = NULL;
		}

		if(RmCurlSock->mCurlShared)
		{
			CurlDataShareLock *locks = RmCurlSock->pstShareLocks;
			curl_share_cleanup(RmCurlSock->mCurlShared);

			pthread_mutex_destroy(&locks->mCurlSharedlock);
			pthread_mutex_destroy(&locks->mDnsCurlShareMutex);
			pthread_mutex_destroy(&locks->mSslCurlShareMutex);
			SAFE_DELETE(RmCurlSock->pstShareLocks);
		}
		SAFE_DELETE(RmCurlSock);
		umCurlSockDataStore.erase(RemIt);
	}
	else
	{
		/**
		 * Lets extend the size of curlstore, if curlstore size less than max size.
		 */
		if ( umCurlSockDataStore.size() < (MAX_CURL_SOCK_STORE-1) )
		{
			MaxCurlSockStore = 1 + umCurlSockDataStore.size();
		}

		ShowCurlStoreData();
	}

	AAMPLOG_INFO("After remove Curl Sock Store size:%d %d\n", umCurlSockDataStore.size(), MaxCurlSockStore);
}

/**
 * @fn ShowCurlStoreData
 * @brief ShowCurlStoreData - Print curl store details
 */
void CurlStore::ShowCurlStoreData ( void )
{
	AAMPLOG_INFO("Curl Store Size:%d, MaxSize:%d", umCurlSockDataStore.size(), MaxCurlSockStore);

	CurlSockDataIter it=umCurlSockDataStore.begin();
	for(; it != umCurlSockDataStore.end(); ++it )
	{
		CurlSocketStoreStruct *CurlSock = it->second;
		AAMPLOG_INFO("Host:%s ShHdl:%p LastUsed:%lld", (it->first).c_str(), CurlSock->mCurlShared, CurlSock->timestamp);
		AAMPLOG_INFO("List of Curl contexts:");
		for(int i=0; i < eCURLINSTANCE_MAX_PLUSBG;++i)
		{
			if(CurlSock->curl[i])
			AAMPLOG_INFO("CurlFd:%p Inst:%d Inuse:%d", CurlSock->curl[i], i, CurlSock->aui8InUse[i]);
		}
	}
}
