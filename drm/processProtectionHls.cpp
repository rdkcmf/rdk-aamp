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
* \file processProtection.cpp
*
* Process protection is for handling process protection of open cdm drm 
* for HSL Streaming  
* Functionalities are parse the file , get the drm type , PSSH data.
* Create DRM Session in thread 
* 
*/
#include "_base64.h"
#include "AampDRMSessionManager.h"
#include "AampDrmSession.h"
#include "fragmentcollector_hls.h"

#include <cstdlib>
#include <string>
using namespace std;

#ifdef AAMP_HLS_DRM

extern void *CreateDRMSession(void *arg);
extern void ReleaseDRMLicenseAcquireThread(PrivateInstanceAAMP *aamp);

/**
 * Global aamp config data 
 */
extern AampConfig *gpGlobalConfig;
DrmSessionDataInfo* ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName);

/**
 * Local APIs declarations
 */
static int GetFieldValue(string &attrName, string keyName, string &valuePtr);
static int getKeyId(string attrName, string &keyId);
static int getPsshData(string attrName, string &psshData);
static shared_ptr<AampDrmHelper> getDrmHelper(string attrName , bool bPropagateUriParams);
static uint8_t getPsshDataVersion(string attrName);

/**
 * @brief Return the string value, from the input KEY="value"
 * @param [in] attribute list to be searched 
 * @param [in] Key name to be checked to get the value
 * @param [out] value of the key
 * @return none
 */
static int GetFieldValue(string &attrName, string keyName, string &valuePtr){

	int valueStartPos = 0;
	int valueEndPos = attrName.length();
	int keylen = keyName.length();
	int status = DRM_API_FAILED;
	int found = 0, foundpos = 0;

	AAMPLOG_TRACE("Entring..");

	while ( (foundpos = attrName.find(keyName,found)) != std::string::npos)
	{
		AAMPLOG_TRACE("keyName = %s",
		 keyName.c_str());

		valueStartPos = foundpos + keylen;
		if (attrName.at(valueStartPos) == '=')
		{
			string valueTempPtr = attrName.substr(valueStartPos+1);

			AAMPLOG_TRACE("valueTempPtr = %s",
			valueTempPtr.c_str());

			/* update start position based on substring */
			valueStartPos = 0;
			if (valueTempPtr.at(0) == '"')
			{
				valueTempPtr = valueTempPtr.substr(1);
				valueEndPos = valueTempPtr.find('"');
			}
			else if ( (valueEndPos = valueTempPtr.find(',')) == std::string::npos)
			{
				/*look like end string*/
				valueEndPos = valueTempPtr.length();
			}

			valuePtr = valueTempPtr.substr(valueStartPos, valueEndPos);
			AAMPLOG_INFO("Value found : %s for Key : %s",
			valuePtr.c_str(), keyName.c_str());
			status = DRM_API_SUCCESS;
			break;
		}
		else
		{
			AAMPLOG_TRACE("Checking next occurence of %s= in %s",
			keyName.c_str(), attrName.c_str());
			found = valueStartPos+1;
		}
	}

	if(DRM_API_SUCCESS != status)
	{
		AAMPLOG_ERR("Could not able to find %s in %s",
		keyName.c_str(), attrName.c_str());
	}

	return status;
}

/* Widevine Example 
#EXT-X-KEY:METHOD=SAMPLE-AES-CTR,
KEYFORMAT="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
KEYFORMATVERSIONS="1",URI="data:text/plain;base64,AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsIARIgZTI1ZmFkYjQ4YmZiNDkyMjljZTBhNGFmZGZlMDUxOTcaB3NsaW5ndHYiBUhHVFZEKgVTRF9IRA==",
KEYID=0xe25fadb48bfb49229ce0a4afdfe05197
*/
/* PlayReady Example 
#EXT-X-KEY:METHOD=SAMPLE-AES-CTR,
URI="data:text/plain;charset=UTF-16;base64,BgIAAAEAAQD8ATwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsASQBEAD4AbgA0AEkARABBAEsATwAxAHMARwByAGcAegBpAHkAOAA4AFgAcgBqAGYAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgB1AGkAbwA4AFcAVQBwAFQANAA0ADAAPQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=",
KEYFORMAT="com.microsoft.playready",
KEYFORMATVERSIONS="1"
*/

/**
 * @brief API to get the Widevine PSSH Data from the manifest attribute list, getWVPsshData
 * @param [in] Attribute list
 * @param [out] keyId string as reference 
 * @return status of the API
 */
static int getKeyId(string attrName, string &keyId){

	int status = GetFieldValue(attrName, "KEYID", keyId );
	if(DRM_API_SUCCESS != status){
		AAMPLOG_INFO("Could not able to get Key Id from manifest"
		);
		return status;
	}

	/* Remove 0x from begining */
	keyId = keyId.substr(2);

	return status;
}

/**
 * @brief API to get the PSSH Data from the manifest attribute list, getPsshData
 * @param [in] Attribute list
 * @param [out] pssData as reference 
 * @return status of the API
 */
static int getPsshData(string attrName, string &psshData){

	int status = GetFieldValue(attrName, "URI", psshData );
	if(DRM_API_SUCCESS != status){
		AAMPLOG_ERR("Could not able to get psshData from manifest"
		);
		return status;
	}
	/* Split string based on , and get the PSSH Data */
	psshData = psshData.substr(psshData.find(',')+1);

	return status;
}

/**
 * @brief API to get the DRM type from the manifest attribute list, getDrmType
 * @param [in] Attribute list
 * 
 * @return pssh Data version number, default is 0
 */
static uint8_t getPsshDataVersion(string attrName){

	uint8_t psshDataVer = 0;
	string psshDataVerStr = "";

	if(DRM_API_SUCCESS != GetFieldValue(attrName, "KEYFORMATVERSIONS", psshDataVerStr )){
		AAMPLOG_WARN("Could not able to receive pssh data version from manifest"
		"returning default value as 0"
		);
	}else {
		psshDataVer = (uint8_t)std::atoi(psshDataVerStr.c_str());
	}

	return psshDataVer;
}

/**
 * @brief API to get the DRM helper from the manifest attribute list, getDrmType
 * @param [in] Attribute list
 * 
 * @return AampDrmHelper - DRM Helper (nullptr in case of unexpected behaviour)
 */
static std::shared_ptr<AampDrmHelper> getDrmHelper(string attrName , bool bPropagateUriParams){

	string systemId = "";
	
	if(DRM_API_SUCCESS != GetFieldValue(attrName, "KEYFORMAT", systemId )){
		AAMPLOG_ERR("Could not able to receive key id from manifest"
		);
		return nullptr;
	}

	/** Remove urn:uuid: from it */
	if (systemId.find("urn:uuid:") != std::string::npos){
		systemId = systemId.substr(strlen("urn:uuid:"));
	}
	DrmInfo drmInfo;
	drmInfo.mediaFormat = eMEDIAFORMAT_HLS_MP4;
	drmInfo.systemUUID=systemId;
	drmInfo.bPropagateUriParams = bPropagateUriParams;
	return AampDrmHelperEngine::getInstance().createHelper(drmInfo);
}

/**
 * @brief Process content protection of track
 * @param TrackState object 
 * @param attribute list from manifest
 * @return none
 */
DrmSessionDataInfo* ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName)
{
	/* StreamAbstractionAAMP_HLS* context; */
	/* Pseudo code for ProcessContentProtection in HLS is below
	 * Get Aamp instance as aamp
	 * 1. Get DRM type from manifest (KEYFORMAT uuid)
	 * 2. Get pssh data from manifest (extract URI value)
	 * 3. Check whether keyID with last processed keyId
	 * 4. if not, Create DrmSessionParams instance and fill it 
	 *    4.1 Create a thread with CreateDRMSession and DrmSessionParams as parameter
	 *    4.2 Reuse the thread function CreateDRMSession which is used in MPD for HLS also
	 * 5. Else delete keyId and return
	 */
	AampLogManager *mLogObj = aamp->mConfig->GetLoggerInstance();
	shared_ptr<AampDrmHelper> drmHelper;
	unsigned char* data = NULL;
	unsigned char* contentMetadata = NULL;
	size_t dataLength = 0;
	int status = DRM_API_FAILED;  
	string psshDataStr = "";
	char* psshData = NULL;
	unsigned char * keyId = NULL;
	int keyIdLen = 0;
	MediaType mediaType = eMEDIATYPE_VIDEO;
	DrmSessionDataInfo *drmSessioData = NULL;
	do{
		drmHelper = getDrmHelper(attrName , ISCONFIGSET(eAAMPConfig_PropogateURIParam));
		if (nullptr == drmHelper){
			AAMPLOG_ERR("Failed to get DRM type/helper from manifest!");
			break;
		}

		status  = getPsshData(attrName, psshDataStr);
		if (DRM_API_SUCCESS != status){
			AAMPLOG_ERR("Failed to get PSSH Data from manifest!");
			break;
		}
		psshData = (char*) malloc(psshDataStr.length() + 1);
		memset(psshData, 0x00 , psshDataStr.length() + 1);
		strncpy(psshData, psshDataStr.c_str(), psshDataStr.length());

		if(drmHelper->friendlyName().compare("Verimatrix") == 0)
		{
			logprintf("%s:%d Verimatrix DRM.", __FUNCTION__, __LINE__);
			data = (unsigned char *)psshData;
			dataLength = psshDataStr.length();
		}
		else
		{
			data = base64_Decode(psshData, &dataLength);
			/* No more use */
			free(psshData);
			psshData = NULL;
		}

		if (dataLength == 0)
		{
			AAMPLOG_ERR("Could not able to retrive DRM data from PSSH");
			break;
		}
		if (gpGlobalConfig->logging.trace)
		{
			AAMPLOG_TRACE("content metadata from manifest; length %d",
			dataLength);
			printf("*****************************************************************\n");
			for (int i = 0; i < dataLength; i++)
			{
				printf("%c", data[i]);
			}
			printf("\n*****************************************************************\n");
			for (int i = 0; i < dataLength; i++)
			{
				printf("%02x ", data[i]);
			}
			printf("\n*****************************************************************\n");

		}
		if (!drmHelper->parsePssh(data, dataLength)) 
		{
			AAMPLOG_ERR("Failed to get key Id from manifest");
			break;
		}

		MediaType mediaType  = eMEDIATYPE_VIDEO;

		if (drmHelper){
			/** Push Drm Information for later use do not free the memory here*/
			//AAMPLOG_INFO("Storing DRM Info at keyId %s",
			//keyId);

			/** Populate session data **/
			DrmSessionParams* sessionParams = new DrmSessionParams;
			sessionParams->initData = data;
			sessionParams->initDataLen = dataLength;
			sessionParams->stream_type = mediaType;
			sessionParams->aamp = aamp;
			sessionParams->drmHelper = drmHelper;

			/** populate pool data **/
			drmSessioData = new DrmSessionDataInfo() ;
			drmSessioData->isProcessedLicenseAcquire = false;
			drmSessioData->sessionData = sessionParams;
			drmSessioData->processedKeyIdLen = keyIdLen;
			drmSessioData->processedKeyId = (unsigned char *) malloc(keyIdLen + 1);
			memcpy(drmSessioData->processedKeyId, keyId, keyIdLen);
		}

		if (keyId) {
			free(keyId);
			keyId = NULL;
		}
		
	}while(0);
	if(data)
	{
		free(data);  //CID:128617 - Resource leak
	}
	return drmSessioData;
}

#else

void* ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName){
	AAMPLOG_INFO("AAMP_HLS_DRM not enabled" 
	);
	return NULL;
}
#endif /** AAMP_HLS_DRM */

/**
 * EOF
 */
