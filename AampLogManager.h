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

#ifndef AAMPLOGMANAGER_H
#define AAMPLOGMANAGER_H

/**
 * @file AampLogManager.h
 * @brief Log managed for Aamp
 */

#include <vector>
#include <memory.h>

#include "AampMediaType.h"

/*================================== AAMP Log Manager =========================================*/

/**
 * @brief Direct call for trace printf, can be enabled b defining TRACE here
 */
#ifdef TRACE
#define traceprintf logprintf
#else
#define traceprintf(FORMAT, ...)
#endif

/**
 * @brief Macro for validating the log level to be enabled
 *
 * if mConfig or gpGlobalConfig is not initialized, skip logging
 * if mconfig or gpGlobalConfig is initialized,  check the LogLevel
 */
#define AAMPLOG( MYLOGOBJ, LEVEL, LEVELSTR, FORMAT, ... ) \
do { \
	int PLAYERID; \
	if( MYLOGOBJ ) \
	{ \
		if( !MYLOGOBJ->isLogLevelAllowed(LEVEL) ) break; \
		PLAYERID = MYLOGOBJ->getPlayerId(); \
	} \
	else \
	{ \
		if( !gpGlobalConfig->logging.isLogLevelAllowed(LEVEL) ) break; \
		PLAYERID = -1; \
	} \
	logprintf_new( PLAYERID, LEVELSTR, __FUNCTION__, __LINE__, FORMAT, ##__VA_ARGS__); \
} while(0)

/**
 * @brief Macro for Triage Level Logging Support
 */
#define AAMP_LOG_NETWORK_LATENCY	mLogObj->LogNetworkLatency
#define AAMP_LOG_NETWORK_ERROR		mLogObj->LogNetworkError
#define AAMP_LOG_DRM_ERROR		gpGlobalConfig->logging.LogDRMError
#define AAMP_LOG_ABR_INFO		mLogObj->LogABRInfo
#define AAMP_IS_LOG_WORTHY_ERROR	mLogObj->isLogworthyErrorCode

#define AAMPLOG_FAILOVER(FORMAT, ...) \
		if (mLogObj && mLogObj->failover) { \
				logprintf_new(mLogObj->getPlayerId(), "FAILOVER",__FUNCTION__, __LINE__, FORMAT, ##__VA_ARGS__); \
		}

/**
 * @brief AAMP logging defines, this can be enabled through setLogLevel() as per the need
 */
#define AAMPLOG_TRACE(FORMAT, ...) AAMPLOG(mLogObj,eLOGLEVEL_TRACE, "TRACE", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_INFO(FORMAT, ...) AAMPLOG(mLogObj,eLOGLEVEL_INFO, "INFO", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_WARN(FORMAT, ...) AAMPLOG(mLogObj,eLOGLEVEL_WARN, "WARN", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_ERR(FORMAT, ...) AAMPLOG(mLogObj,eLOGLEVEL_ERROR, "ERROR", FORMAT, ##__VA_ARGS__)


/**
 * @brief maximum supported mediatype for latency logging
 */
#define MAX_SUPPORTED_LATENCY_LOGGING_TYPES	(eMEDIATYPE_IFRAME+1)

/**
 * @brief Log level's of AAMP
 */
enum AAMP_LogLevel
{
	eLOGLEVEL_TRACE,    /**< Trace level */
	eLOGLEVEL_INFO,     /**< Info level */
	eLOGLEVEL_WARN,     /**< Warn level */
	eLOGLEVEL_ERROR,    /**< Error level */
	eLOGLEVEL_FATAL     /**< Fatal log level */
};

/**
 * @brief Log level network error enum
 */
enum AAMPNetworkErrorType
{
	/* 0 */ AAMPNetworkErrorNone,     /**< No network Error */
	/* 1 */ AAMPNetworkErrorHttp,     /**< HTTP error */
	/* 2 */ AAMPNetworkErrorTimeout,  /**< Timeout Error */
	/* 3 */ AAMPNetworkErrorCurl      /**< curl Error */
};

/**
 * @brief ABR type enum
 */
enum AAMPAbrType
{
	/* 0 */ AAMPAbrBandwidthUpdate,
	/* 1 */ AAMPAbrManifestDownloadFailed,
	/* 2 */ AAMPAbrFragmentDownloadFailed,
	/* 3 */ AAMPAbrUnifiedVideoEngine
};


/**
 * @brief ABR info structure
 */
struct AAMPAbrInfo
{
	AAMPAbrType abrCalledFor;
	int currentProfileIndex;
	int desiredProfileIndex;
	long currentBandwidth;
	long desiredBandwidth;
	long networkBandwidth;
	AAMPNetworkErrorType errorType;
	int errorCode;
};



/**
 * @class AampLogManager
 * @brief AampLogManager Class
 */
class AampLogManager
{
public:

	bool info;           /**< Info level*/
	bool debug;          /**< Debug logs*/
	bool trace;          /**< Trace level*/
	bool gst;            /**< Gstreamer logs*/
	bool curl;           /**< Curl logs*/
	bool progress;       /**< Download progress logs*/
	bool failover;	     /**< server fail over logs*/
	bool stream;         /**< Display stream contents */
	bool curlHeader;     /**< Curl header logs*/
	bool curlLicense;    /**< Curl logs for License request*/
	bool logMetadata;    /**< Timed metadata logs*/
	bool id3;	     /**< Display ID3 tag from stream logs */
	static bool disableLogRedirection;
	int  mPlayerId;	     /**< Store PlayerId*/
	/**
	 * @brief AampLogManager constructor
	 */
	AampLogManager() : aampLoglevel(eLOGLEVEL_WARN), info(false), debug(false), trace(false), gst(false), curl(false), progress(false), failover(false), curlHeader(false), 
							logMetadata(false), curlLicense(false),stream(false), id3(false),mPlayerId(-1)
	{
	}

	/* ---------- Triage Level Logging Support ---------- */

	/**
	 * @fn LogNetworkLatency
	 *
	 * @param[in] url - content url
	 * @param[in] downloadTime - download time of the fragment or manifest
	 * @param[in] downloadThresholdTimeoutMs - specified download threshold time out value
	 * @param[in] type - media type
	 * @return void
	 */
	void LogNetworkLatency(const char* url, int downloadTime, int downloadThresholdTimeoutMs, MediaType type);

	/**
	 * @fn LogNetworkError
	 *
	 * @param[in] url - content url
	 * @param[in] errorType - it can be http or curl errors
	 * @param[in] errorCode - it can be http error or curl error code
	 * @param[in] type - media type
	 * @return void
	 */
	void LogNetworkError(const char* url, AAMPNetworkErrorType errorType, int errorCode, MediaType type);

	/**
	 * @fn ParseContentUrl
	 *
	 * @param[in] url - content url
	 * @param[out] contentType - it could be a manifest or other audio/video/iframe tracks
	 * @param[out] location - server location
	 * @param[out] symptom - issue exhibiting scenario for error case
	 * @param[in] type - media type
	 * @return void
	 */
	void ParseContentUrl(const char* url, std::string& contentType, std::string& location, std::string& symptom, MediaType type);

	/**
	 * @fn LogDRMError
	 *
	 * @param[in] major - drm major error code
	 * @param[in] minor - drm minor error code
	 * @return void
	 */
	void LogDRMError(int major, int minor);

	/**
	 * @fn LogABRInfo
	 *
	 * @param[in] pstAbrInfo - pointer to a structure which will have abr info to be logged
	 * @return void
	 */
	void LogABRInfo(AAMPAbrInfo *pstAbrInfo);
	/* !---------- Triage Level Logging Support ---------- */

	/**
	 * @fn isLogLevelAllowed
	 *
	 * @param[in] chkLevel - log level
	 * @retval true if the log level allowed for print mechanism
	 */
	bool isLogLevelAllowed(AAMP_LogLevel chkLevel);
	/**
	 * @brief Set PlayerId
	 *
	 * @param[in] playerId - Aamp Player Id
	 * @return void
	 */
	void setPlayerId(int playerId) { mPlayerId = playerId;}
	/**
	 * @brief Get PlayerId 
	 *
	 * @return int - playerId
	 */
	int getPlayerId() { return mPlayerId;}
	/**
	 * @fn setLogLevel
	 *
	 * @param[in] newLevel - log level new value
	 * @return void
	 */
	void setLogLevel(AAMP_LogLevel newLevel);

	/**
	 * @fn setLogAndCfgDirectory
	 */
	void setLogAndCfgDirectory(char driveName);

	/**
	 * @fn isLogworthyErrorCode
	 * @param[in] errorCode - curl error
     	 * @return true if it is not a curl error 23 and 42, bcasue those are not a real network errors.
	 */
	bool isLogworthyErrorCode(int errorCode);

	/**
	 * @fn getAampCfgPath
	 */
	const char* getAampCfgPath(void);

	/**
	 * @fn getAampCliCfgPath
	 */
	const char* getAampCliCfgPath(void);

	/**
	 * @fn getHexDebugStr
	 */
	static std::string getHexDebugStr(const std::vector<uint8_t>& data);

private:
	AAMP_LogLevel aampLoglevel;
};

extern AampLogManager *mLogObj;

/* Context-free utility function */

/**
 * @fn logprintf
 * @param[in] format - printf style string
 * @return void
 */
extern void logprintf(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));;
/**
 * @fn logprintf_new
 * @param[in] format - printf style string
 * @return void
 */
extern void logprintf_new(int playerId,const char* levelstr,const char* file, int line,const char *format, ...)  __attribute__ ((format (printf, 5, 6)));;

/**
 * @fn DumpBlob
 *
 * @param[in] ptr to the buffer
 * @param[in] len length of buffer
 *
 * @return void
 */
void DumpBlob(const unsigned char *ptr, size_t len);

#endif /* AAMPLOGMANAGER_H */
