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
	if(context)
	{
		ret = context->aamp->HandleSSLWriteCallback( ptr, size, nmemb, userdata);
	}
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
	size_t ret = 0;
	CurlCallbackContext *context = static_cast<CurlCallbackContext *>(user_data);
	if(context)
	{
		ret = context->aamp->HandleSSLHeaderCallback(ptr, size, nmemb, user_data);
	}
	return ret;
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
	int ret = 0;
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;

	if(context)
	{
		ret = context->aamp->HandleSSLProgressCallback ( clientp, dltotal, dlnow, ultotal, ulnow );
	}
	return ret;
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
	AAMPLOG_TRACE("priv aamp :%p", context);
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
 * @brief
 * @param handle ptr to CURL instance
 * @param type type of data passed in the callback
 * @param data data pointer, NOT null terminated
 * @param size size of the data
 * @param userp user pointer set with CURLOPT_DEBUGDATA
 * @retval return 0
 */
static int eas_curl_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
	(void)handle;
	(void)userp;
	if(type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN)
	{
		//limit log spam to only TEXT and HEADER_IN
		size_t len = size;
		while( len>0 && data[len-1]<' ' ) len--;
		std::string printable(data,len);
		switch (type)
		{
		case CURLINFO_TEXT:
			AAMPLOG_WARN("curl: %s", printable.c_str() );
			break;
		case CURLINFO_HEADER_IN:
			AAMPLOG_WARN("curl header: %s", printable.c_str() );
			break;
		default:
			break; //CID:94999 - Resolve deadcode
		}
	}
	return 0;
}

/**
 * @fn CreateCurlStore
 * @brief CreateCurlStore - Create a new curl store for given host
 */
CurlSocketStoreStruct *CurlStore::CreateCurlStore ( const std::string &hostname )
{
	CurlSocketStoreStruct *CurlSock = new curlstorestruct();
	CurlDataShareLock *locks = new curldatasharelock();
	if ( NULL == CurlSock || NULL == locks )
	{
		AAMPLOG_WARN("Failed to alloc memory for curl store");
		return NULL;
	}

	CurlSock->timestamp = aamp_GetCurrentTimeMS();
	CurlSock->pstShareLocks = locks;
	CurlSock->mCurlStoreUserCount += 1;

	CURLSHcode sh;
	CurlSock->mCurlShared = curl_share_init();
	sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_USERDATA, (void*)locks);
	sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
	sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
	sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	sh = curl_share_setopt(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);

	if ( umCurlSockDataStore.size() >= MaxCurlSockStore )
	{
		// Remove not recently used handle.
		RemoveCurlSock();
	}

	umCurlSockDataStore[hostname]=CurlSock;
	AAMPLOG_INFO("Curl store for %s created, Added shared ctx %p in curlstore %p, size:%d maxsize:%d", hostname.c_str(),
					CurlSock->mCurlShared, CurlSock, umCurlSockDataStore.size(), MaxCurlSockStore);
	return CurlSock;
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
	HostName = aamp_getHostFromURL ( url );

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && !( aamp_IsLocalHost(HostName) ))
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
	HostName = aamp_getHostFromURL ( url );

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && !( aamp_IsLocalHost(HostName) ))
	{
		KeepInCurlStore ( HostName, startIdx, curl );
	}
	else
	{
		curl_easy_cleanup(curl);
	}
}

/**
 * @fn CurlEasyInitWithOpt
 * @brief CurlEasyInitWithOpt - Create a curl easy handle with set of aamp opts
 */
CURL* CurlStore::CurlEasyInitWithOpt ( void *privContext, const std::string &proxyName, int instId )
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)privContext;
	std::string UserAgentString;
	UserAgentString=aamp->mConfig->GetUserAgentString();

	CURL *curlEasyhdl = curl_easy_init();
	if (ISCONFIGSET(eAAMPConfig_CurlLogging))
	{
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_VERBOSE, 1L);
	}
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_NOSIGNAL, 1L);
	//curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback); // unused
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_PROGRESSFUNCTION, progress_callback);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_HEADERFUNCTION, header_callback);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_WRITEFUNCTION, write_callback);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_TIMEOUT, DEFAULT_CURL_TIMEOUT);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_CONNECTTIMEOUT, DEFAULT_CURL_CONNECTTIMEOUT);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_FOLLOWLOCATION, 1L);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_NOPROGRESS, 0L); // enable progress meter (off by default)

	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_USERAGENT, UserAgentString.c_str());
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_ACCEPT_ENCODING, "");//Enable all the encoding formats supported by client
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_SSL_CTX_FUNCTION, ssl_callback); //Check for downloads disabled in btw ssl handshake
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_SSL_CTX_DATA, aamp);
	long dns_cache_timeout = 3*60;
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_DNS_CACHE_TIMEOUT, dns_cache_timeout);
	CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_SHARE, aamp->mCurlShared);
	//CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_TCP_KEEPALIVE, 1 );

	aamp->curlDLTimeout[instId] = DEFAULT_CURL_TIMEOUT * 1000;

	if (!proxyName.empty())
	{
		/* use this proxy */
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_PROXY, proxyName.c_str());
		/* allow whatever auth the proxy speaks */
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}

	if(aamp->IsEASContent())
	{
		//enable verbose logs so we can debug field issues
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_VERBOSE, 1);
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_DEBUGFUNCTION, eas_curl_debug_callback);
		//set eas specific timeouts to handle faster cycling through bad hosts and faster total timeout
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_TIMEOUT, EAS_CURL_TIMEOUT);
		CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_CONNECTTIMEOUT, EAS_CURL_CONNECTTIMEOUT);

		aamp->curlDLTimeout[instId] = EAS_CURL_TIMEOUT * 1000;

		//on ipv6 box force curl to use ipv6 mode only (DELIA-20209)
		struct stat tmpStat;
		bool isv6(::stat( "/tmp/estb_ipv6", &tmpStat) == 0);
		if(isv6)
		{
			CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
		}
		AAMPLOG_WARN("aamp eas curl config: timeout=%d, connecttimeout%d, ipv6=%d", EAS_CURL_TIMEOUT, EAS_CURL_CONNECTTIMEOUT, isv6);
	}
	//log_current_time("curl initialized");

	return curlEasyhdl;
}

/**
 * @fn CurlInit
 * @brief CurlInit - Initialize or get easy handles for given host & curl index from curl store
 */
void CurlStore::CurlInit(void *privContext, AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName, const std::string &RemoteHost)
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)privContext;

	int instanceEnd = startIdx + instanceCount;
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	std::string HostName;
	bool IsRemotehost = true, CurlFdHost=false;
	AampCurlStoreErrorCode CurlStoreErrCode=eCURL_STORE_HOST_NOT_AVAILABLE;

	if(RemoteHost.size())
	{
		HostName = RemoteHost;
		CurlFdHost = true;
	}
	else
	{
		HostName = aamp->mOrigManifestUrl.hostname;
		IsRemotehost = aamp->mOrigManifestUrl.isRemotehost;
	}

	if ( IsRemotehost )
	{
		if (ISCONFIGSET(eAAMPConfig_EnableCurlStore))
		{
			AAMPLOG_INFO("Check curl store for host:%s inst:%d-%d Fds[%p:%p]\n", HostName.c_str(), startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl );
			CurlStoreErrCode = GetFromCurlStoreBulk(HostName, startIdx, instanceEnd, aamp, CurlFdHost );
			AAMPLOG_TRACE("From curl store for inst:%d-%d Fds[%p:%p] ShHdl:%p\n", startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl, aamp->mCurlShared );
		}
		else
		{
			if(NULL==aamp->mCurlShared)
			{
				aamp->mCurlShared = curl_share_init();
				curl_share_setopt(aamp->mCurlShared, CURLSHOPT_USERDATA, (void*)NULL);
				curl_share_setopt(aamp->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
				curl_share_setopt(aamp->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
				curl_share_setopt(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

				if (ISCONFIGSET(eAAMPConfig_EnableSharedSSLSession))
				{
					curl_share_setopt(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
				}
			}
		}
	}

	if(eCURL_STORE_HOST_SOCK_AVAILABLE != CurlStoreErrCode)
	{
		CURL **CurlFd = NULL;
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			if(CurlFdHost)
			{
				CurlFd = &aamp->curlhost[i]->curl;
			}
			else
			{
				CurlFd = &aamp->curl[i];
			}

			if (NULL == *CurlFd )
			{
				*CurlFd = CurlEasyInitWithOpt(aamp, proxyName, i);
				AAMPLOG_INFO("Created new curl handle:%p for inst:%d", *CurlFd, i );
			}
		}
	}
}

/**
 * @fn CurlTerm
 * @brief CurlTerm - Terminate or store easy handles in curlstore
 */
void CurlStore::CurlTerm(void *privContext, AampCurlInstance startIdx, unsigned int instanceCount, const std::string &RemoteHost )
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP *)privContext;
	int instanceEnd = startIdx + instanceCount;
	std::string HostName;
	bool IsRemotehost = true, CurlFdHost=false;

	if(RemoteHost.size())
	{
		HostName = RemoteHost;
		CurlFdHost = true;
	}
	else
	{
		HostName = aamp->mOrigManifestUrl.hostname;
		IsRemotehost = aamp->mOrigManifestUrl.isRemotehost;
	}

	if( ISCONFIGSET(eAAMPConfig_EnableCurlStore)  && ( IsRemotehost ))
	{
		AAMPLOG_INFO("Store unused curl handle:%p in Curlstore for inst:%d-%d", aamp->curl[startIdx], startIdx, instanceEnd );
		KeepInCurlStoreBulk ( HostName, startIdx, instanceEnd, aamp, CurlFdHost);
		ShowCurlStoreData(ISCONFIGSET(eAAMPConfig_TraceLogging));
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
 * @fn GetCurlHandleFromFreeQ
 * @brief GetCurlHandleFromFreeQ - Get curl handle from free queue
 */
CURL *CurlStore::GetCurlHandleFromFreeQ ( CurlSocketStoreStruct *CurlSock, int instId )
{
	CURL *curlhdl = NULL;
	long long MaxAge = CurlSock->timestamp-eCURL_MAX_AGE_TIME;

	for (int i = instId; i < instId+1 && !CurlSock->mFreeQ.empty(); )
	{
		CurlHandleStruct mObj = CurlSock->mFreeQ.front();

		if( MaxAge > mObj.eHdlTimestamp )
		{
			CurlSock->mFreeQ.pop_front();
			AAMPLOG_TRACE("Remove old curl hdl:%p", mObj.curl);
			curl_easy_cleanup(mObj.curl);
			mObj.curl = NULL;
			continue;
		}

		if ( mObj.curlId == i )
		{
			CurlSock->mFreeQ.pop_front();
			curlhdl = mObj.curl;
			break;
		}

		for(auto it=CurlSock->mFreeQ.begin()+1; it!=CurlSock->mFreeQ.end(); ++it)
		{
			if (( MaxAge < it->eHdlTimestamp ) && ( it->curlId == i ))
			{
				curlhdl=it->curl;
				CurlSock->mFreeQ.erase(it);
				break;
			}
		}

		++i;
	}

	return curlhdl;
}

/**
 * @fn GetFromCurlStoreBulk
 * @brief GetFromCurlStoreBulk - Get free curl easy handle in bulk for given host & curl indices
 */
AampCurlStoreErrorCode CurlStore::GetFromCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, void *priv, bool CurlFdHost )
{
	PrivateInstanceAAMP *aamp = (PrivateInstanceAAMP*)priv;
	AampCurlStoreErrorCode ret = eCURL_STORE_HOST_SOCK_AVAILABLE;

	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		int CurlFdCount=0,loop=0;
		CURL **CurlFd=NULL;
		CurlSocketStoreStruct *CurlSock = it->second;
		CurlSock->mCurlStoreUserCount += 1;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		aamp->mCurlShared = CurlSock->mCurlShared;

		for( loop = (int)CurlIndex; loop < count; )
		{
			if(CurlFdHost)
			{
				CurlFd=&aamp->curlhost[loop]->curl;
			}
			else
			{
				CurlFd=&aamp->curl[loop];
			}

			if (!CurlSock->mFreeQ.empty())
			{
				*CurlFd= GetCurlHandleFromFreeQ ( CurlSock, loop );
				if(NULL!=*CurlFd)
				{
					CURL_EASY_SETOPT(*CurlFd, CURLOPT_SSL_CTX_DATA, aamp);
					++CurlFdCount;
				}
				else
				{
					ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				}
				++loop;
			}
			else
			{
				AAMPLOG_TRACE("Queue is empty");
				ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				break;
			}
		}

		AAMPLOG_INFO ("%d fd(s) got from CurlStore User count:%d", CurlFdCount, CurlSock->mCurlStoreUserCount);

		if ( umCurlSockDataStore.size() > MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_TRACE("Curl Inst %d for %s not in store", CurlIndex, hostname.c_str());
		ret = eCURL_STORE_HOST_NOT_AVAILABLE;

		CurlSocketStoreStruct *CurlSock = CreateCurlStore(hostname);

		if(NULL != CurlSock)
		{
			aamp->mCurlShared = CurlSock->mCurlShared;
		}
	}

	pthread_mutex_unlock(&mCurlInstLock);
	return ret;
}

/**
 * @fn GetFromCurlStore
 * @brief GetFromCurlStore - Get a free curl easy handle for given host & curl index
 */
AampCurlStoreErrorCode CurlStore::GetFromCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL **curl )
{
	AampCurlStoreErrorCode ret = eCURL_STORE_HOST_SOCK_AVAILABLE;
	CurlSocketStoreStruct *CurlSock = NULL;
	*curl = NULL;

	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		CurlSock = it->second;
		CurlSock->mCurlStoreUserCount += 1;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		long long MaxAge = CurlSock->timestamp-eCURL_MAX_AGE_TIME;

		for( int loop = (int)CurlIndex; loop < CurlIndex+1; )
		{
			if (!CurlSock->mFreeQ.empty())
			{
				*curl = GetCurlHandleFromFreeQ ( CurlSock, loop);

				if(NULL==*curl)
				{
					ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				}
				++loop;
			}
			else
			{
				AAMPLOG_TRACE("Queue is empty");
				ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				break;
			}
		}
	}

	if ( NULL == *curl )
	{
		AAMPLOG_TRACE("Curl Inst %d for %s not available", CurlIndex, hostname.c_str());

		if(NULL == CurlSock)
		{
			ret = eCURL_STORE_HOST_NOT_AVAILABLE;

			CurlSock = CreateCurlStore(hostname);
		}

		*curl = curl_easy_init();
		CURL_EASY_SETOPT(*curl, CURLOPT_SHARE, CurlSock->mCurlShared);
	}

	pthread_mutex_unlock(&mCurlInstLock);
	return ret;
}

/**
 * @fn KeepInCurlStoreBulk
 * @brief KeepInCurlStoreBulk - Store curl easy handle in bulk for given host & curl index
 */
void CurlStore::KeepInCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, void *priv, bool CurlFdHost )
{
	PrivateInstanceAAMP *aamp =  (PrivateInstanceAAMP*)priv;
	CurlSocketStoreStruct *CurlSock = NULL;

	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if(it != umCurlSockDataStore.end())
	{
		CurlSock = it->second;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->mCurlStoreUserCount -= 1;

		for( int loop = (int)CurlIndex; loop < count; ++loop)
		{
			CurlHandleStruct mObj;
			if(CurlFdHost)
			{
				mObj.curl = aamp->curlhost[loop]->curl;
				aamp->curlhost[loop]->curl = NULL;
			}
			else
			{
				mObj.curl = aamp->curl[loop];
				aamp->curl[loop] = NULL;
			}

			mObj.eHdlTimestamp = CurlSock->timestamp;
			mObj.curlId = loop;
			CurlSock->mFreeQ.push_back(mObj);
			AAMPLOG_TRACE("Curl Inst %d CurlCtx:%p stored at %d", loop, mObj.curl, CurlSock->mFreeQ.size());
		}

		AAMPLOG_TRACE ("CurlStore User count:%d for:%s", CurlSock->mCurlStoreUserCount, hostname.c_str());
		if ( umCurlSockDataStore.size() > MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_INFO("Host %s not in store, Curl Inst %d-%d", hostname.c_str(), CurlIndex,count);
	}

	pthread_mutex_unlock(&mCurlInstLock);
}

/**
 * @fn KeepInCurlStore
 * @brief KeepInCurlStore - Store a curl easy handle for given host & curl index
 */
void CurlStore::KeepInCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL *curl )
{
	CurlSocketStoreStruct *CurlSock = NULL;
	pthread_mutex_lock(&mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);
	if(it != umCurlSockDataStore.end())
	{
		CurlSock = it->second;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->mCurlStoreUserCount -= 1;

		CurlHandleStruct mObj;
		mObj.curl = curl;
		mObj.eHdlTimestamp = CurlSock->timestamp;
		mObj.curlId = (int)CurlIndex;
		CurlSock->mFreeQ.push_back(mObj);
		AAMPLOG_TRACE("Curl Inst %d for %s CurlCtx:%p stored at %d, User:%d", CurlIndex, hostname.c_str(),
						curl,CurlSock->mFreeQ.size(), CurlSock->mCurlStoreUserCount);
	}
	else
	{
		AAMPLOG_INFO("Host %s not in store, Curlfd:%p", hostname.c_str(), curl);
	}
	pthread_mutex_unlock(&mCurlInstLock);
}

/**
 * @fn RemoveCurlSock
 * @brief RemoveCurlSock - Remove not recently used entry from curl store
 */
void CurlStore::RemoveCurlSock ( void )
{
	unsigned long long time=aamp_GetCurrentTimeMS() + 1;

	CurlSockDataIter it=umCurlSockDataStore.begin();
	CurlSockDataIter RemIt=umCurlSockDataStore.end();

	for(; it != umCurlSockDataStore.end(); ++it )
	{
		CurlSocketStoreStruct *CurlSock = it->second;
		if( !CurlSock->mCurlStoreUserCount && ( time > CurlSock->timestamp ) )
		{
			time = CurlSock->timestamp;
			RemIt = it;
		}
	}

	if( umCurlSockDataStore.end() != RemIt )
	{
		CurlSocketStoreStruct *RmCurlSock = RemIt->second;
		AAMPLOG_INFO("Removing host:%s lastused:%lld UserCount:%d", (RemIt->first).c_str(), RmCurlSock->timestamp, RmCurlSock->mCurlStoreUserCount);

		for(auto it = RmCurlSock->mFreeQ.begin(); it != RmCurlSock->mFreeQ.end(); )
		{
			if(it->curl)
			{
				curl_easy_cleanup(it->curl);
			}
			it=RmCurlSock->mFreeQ.erase(it);
		}
		std::deque<CurlHandleStruct>().swap(RmCurlSock->mFreeQ);

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

		ShowCurlStoreData();
	}
	else
	{
		/**
		 * Lets extend the size of curlstore, since all entries are busy..
		 * Later it will get shrunk to user configured size.
		 */
	}
}

/**
 * @fn ShowCurlStoreData
 * @brief ShowCurlStoreData - Print curl store details
 */
void CurlStore::ShowCurlStoreData ( bool trace )
{
	if(trace)
	{
		AAMPLOG_INFO("Curl Store Size:%d, MaxSize:%d", umCurlSockDataStore.size(), MaxCurlSockStore);

		CurlSockDataIter it=umCurlSockDataStore.begin();
		for(int loop=1; it != umCurlSockDataStore.end(); ++it,++loop )
		{
			CurlSocketStoreStruct *CurlSock = it->second;
			AAMPLOG_INFO("%d.Host:%s ShHdl:%p LastUsed:%lld UserCount:%d", loop, (it->first).c_str(), CurlSock->mCurlShared, CurlSock->timestamp, CurlSock->mCurlStoreUserCount);
			AAMPLOG_INFO("%d.Total Curl fds:%d,", loop, CurlSock->mFreeQ.size());

			for(auto it = CurlSock->mFreeQ.begin(); it != CurlSock->mFreeQ.end(); ++it)
			{
				AAMPLOG_TRACE("CurlFd:%p Time:%lld Inst:%d", it->curl, it->eHdlTimestamp, it->curlId);
			}
		}
	}
}

int GetCurlResponseCode( CURL *curlhandle )
{
	long lHttpCode = -1;
	curl_easy_getinfo(curlhandle, CURLINFO_RESPONSE_CODE, &lHttpCode);
	return (int)lHttpCode;
}

