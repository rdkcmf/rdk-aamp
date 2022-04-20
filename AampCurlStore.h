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
 * @file AampCurlStore.h
 * @brief Advanced Adaptive Media Player (AAMP) Curl store
 */

#ifndef AAMPCURLSTORE_H
#define AAMPCURLSTORE_H

#include "priv_aamp.h"
#include <map>
#include <iterator>
#include <vector>
#include <glib.h>

#define eCURL_MAX_AGE_TIME			( (300) * (1000) )			/**< 5 mins - 300 secs - Max age for a connection /

/**
 * @enum AampCurlStoreErrorCode
 * @brief Error codes returned by curlstore
 */
enum AampCurlStoreErrorCode
{
	eCURL_STORE_HOST_NOT_AVAILABLE,
	eCURL_STORE_SOCK_NOT_AVAILABLE,
	eCURL_STORE_HOST_SOCK_AVAILABLE
};

/**
 * @struct curldatasharelock
 * @brief locks used when lock/unlock callback occurs for different shared data
 */
typedef struct curldatasharelock
{
	pthread_mutex_t mCurlSharedlock;
	pthread_mutex_t mDnsCurlShareMutex;
	pthread_mutex_t mSslCurlShareMutex;

	curldatasharelock():mCurlSharedlock(), mDnsCurlShareMutex(),mSslCurlShareMutex()
	{
		pthread_mutex_init(&mCurlSharedlock, NULL);
		pthread_mutex_init(&mDnsCurlShareMutex, NULL);
		pthread_mutex_init(&mSslCurlShareMutex, NULL);
	}
}CurlDataShareLock;

typedef struct curlstruct
{
	CURL *curl;
	int curlId;
	long long eHdlTimestamp;

	curlstruct():curl(NULL), eHdlTimestamp(0),curlId(0){}
}CurlHandleStruct;

/**
 * @struct curlstorestruct
 * @brief structure to store curl easy, shared handle & locks for a host
 */
typedef struct curlstorestruct
{
	std::deque<CurlHandleStruct> mFreeQ;
	CURLSH* mCurlShared;
	CurlDataShareLock *pstShareLocks;

	unsigned int mCurlStoreUserCount;
	long long timestamp;

	curlstorestruct():mCurlShared(NULL), pstShareLocks(NULL), timestamp(0), mCurlStoreUserCount(0), mFreeQ()
	{}

	//Disabled for now
	curlstorestruct(const curlstorestruct&) = delete;
	curlstorestruct& operator=(const curlstorestruct&) = delete;

}CurlSocketStoreStruct;

/**
 * @class CurlStore
 * @brief Singleton curlstore to save/reuse curl handles
 */
class CurlStore
{
private:
	static CurlStore *CurlInstance;
	static pthread_mutex_t mCurlInstLock;
	static int MaxCurlSockStore;

	typedef std::unordered_map <std::string, CurlSocketStoreStruct*> CurlSockData ;
	typedef std::unordered_map <std::string, CurlSocketStoreStruct*>::iterator CurlSockDataIter;
	CurlSockData umCurlSockDataStore;

protected:
	CurlStore( ):umCurlSockDataStore(){}
	~CurlStore( ){}

public:
	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[out] curl - curl easy handle from curl store.
	 * @return AampCurlStoreErrorCode enum type
	 */
	AampCurlStoreErrorCode GetFromCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL **curl );

	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[in] count - No of curl handles
	 * @param[out] priv - curl easy handle from curl store will get stored in priv instance
	 * @return AampCurlStoreErrorCode enum type
	 */
	AampCurlStoreErrorCode GetFromCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, void *priv, bool HostCurlFd );

	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[in] curl - curl easy handle to save in curl store.
	 * @return void
	 */
	void KeepInCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL *curl );

	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[in] count - No of curl handles
	 * @param[out] priv - curl easy handles in priv instance, saved in curl store
	 * @return void
	 */
	void KeepInCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, void *priv, bool HostCurlFd );

	/**
	 * @param void
	 * @return void
	 */
	void RemoveCurlSock ( void );

	/**
	 * @param trace - true to print curl store data, otherwise false.
	 * @return void
	 */
	void ShowCurlStoreData ( bool trace = true );

	/**
	 * @param[out] privContext - priv aamp instance in which created curl handles will be assigned
	 * @param[in] startIdx - Index of Curl instance
	 * @param[in] instanceCount - No of curl handles
	 * @param[in] proxyName - proxy name
	 * @return void
	 */
	void CurlInit(void *privContext, AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName, const std::string &remotehost=std::string("") );

	/**
	 * @param[out] privContext - priv aamp instance from which curl handles will be terminated or stored
	 * @param[in] startIdx - Index of Curl instance
	 * @param[in] instanceCount - No of curl handles
	 * @return void
	 */
	void CurlTerm(void *privContext, AampCurlInstance startIdx, unsigned int instanceCount, const std::string &remotehost=std::string(""));

	/**
	 * @param[in] pAamp - Private aamp instance
	 * @param[in] url - request url
	 * @param[in] startIdx - Index of curl instance.
	 * @return - curl easy handle
	 */
	CURL* GetCurlHandle(void *pAamp, std::string url, AampCurlInstance startIdx );

	/**
	 * @param[in] pAamp - Private aamp instance
	 * @param[in] url - request url
	 * @param[in] startIdx - Index of curl instance.
	 * @param[in] curl - curl handle to be saved
	 * @return void
	 */
	void SaveCurlHandle ( void *pAamp, std::string url, AampCurlInstance startIdx, CURL *curl );

	/**
	 * @param[in] hostname - Host name to create a curl store
	 * @return - Curl store struct pointer
	 */
	CurlSocketStoreStruct *CreateCurlStore ( const std::string &hostname );

	/**
	 * @param[in] privContext - Aamp context
	 * @param[in] proxyName - Network proxy Name
	 * @param[in] instId - Curl instance id
	 * @return - Curl easy handle
	 */
	CURL* CurlEasyInitWithOpt ( void *privContext, const std::string &proxyName, int instId );

	/**
	 * @param[in] CurlSock - Curl socket struct
	 * @param[in] instId - Curl instance id
	 * @return - Curl easy handle
	 */
	CURL* GetCurlHandleFromFreeQ ( CurlSocketStoreStruct *CurlSock, int instId );

	// Copy constructor and Copy assignment disabled
	CurlStore(const CurlStore&) = delete;
	CurlStore& operator=(const CurlStore&) = delete;

	/**
	 * @param[in] pContext - Private aamp instance
	 * @return CurlStore - Singleton instance object
	 */
	static CurlStore *GetCurlStoreInstance(void* pContext);
};

/**
 * @struct CurlCallbackContext
 * @brief context during curl callbacks
 */
struct CurlCallbackContext
{
	PrivateInstanceAAMP *aamp;
	MediaType fileType;
	std::vector<std::string> allResponseHeadersForErrorLogging;
	GrowableBuffer *buffer;
	httpRespHeaderData *responseHeaderData;
	long bitrate;
	bool downloadIsEncoded;
	//represents transfer-encoding based download
	bool chunkedDownload;
	std::string remoteUrl;
	size_t contentLength;

	CurlCallbackContext() : aamp(NULL), buffer(NULL), responseHeaderData(NULL),bitrate(0),downloadIsEncoded(false), chunkedDownload(false),  fileType(eMEDIATYPE_DEFAULT), remoteUrl(""), allResponseHeadersForErrorLogging{""}, contentLength(0)
	{

	}
	CurlCallbackContext(PrivateInstanceAAMP *_aamp, GrowableBuffer *_buffer) : aamp(_aamp), buffer(_buffer), responseHeaderData(NULL),bitrate(0),downloadIsEncoded(false),  chunkedDownload(false), fileType(eMEDIATYPE_DEFAULT), remoteUrl(""), allResponseHeadersForErrorLogging{""},  contentLength(0){}

	~CurlCallbackContext() {}

	CurlCallbackContext(const CurlCallbackContext &other) = delete;
	CurlCallbackContext& operator=(const CurlCallbackContext& other) = delete;
};

/**
 * @struct CurlProgressCbContext
 * @brief context during curl progress callbacks
 */
struct CurlProgressCbContext
{
	PrivateInstanceAAMP *aamp;
	MediaType fileType;
	CurlProgressCbContext() : aamp(NULL), fileType(eMEDIATYPE_DEFAULT), downloadStartTime(-1), abortReason(eCURL_ABORT_REASON_NONE), downloadUpdatedTime(-1), startTimeout(-1), stallTimeout(-1), downloadSize(-1), downloadNow(-1), downloadNowUpdatedTime(-1), dlStarted(false), fragmentDurationMs(-1), remoteUrl(""), lowBWTimeout(-1) {}
	CurlProgressCbContext(PrivateInstanceAAMP *_aamp, long long _downloadStartTime) : aamp(_aamp), fileType(eMEDIATYPE_DEFAULT),downloadStartTime(_downloadStartTime), abortReason(eCURL_ABORT_REASON_NONE), downloadUpdatedTime(-1), startTimeout(-1), stallTimeout(-1), downloadSize(-1), downloadNow(-1), downloadNowUpdatedTime(-1), dlStarted(false), fragmentDurationMs(-1), remoteUrl(""), lowBWTimeout(-1) {}

	~CurlProgressCbContext() {}

	CurlProgressCbContext(const CurlProgressCbContext &other) = delete;
	CurlProgressCbContext& operator=(const CurlProgressCbContext& other) = delete;

	long long downloadStartTime;
	long long downloadUpdatedTime;
	long startTimeout;
	long stallTimeout;
	long lowBWTimeout;
	double downloadSize;
	CurlAbortReason abortReason;
	double downloadNow;
	long long downloadNowUpdatedTime;
	bool dlStarted;
	int fragmentDurationMs;
	std::string remoteUrl;
};

#endif //AAMPCURLSTORE_H
