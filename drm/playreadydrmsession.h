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
 * @file playreadydrmsession.h
 * @brief Playready Session management
 */

#ifndef PlayReadyDrmSession_h
#define PlayReadyDrmSession_h

#include "AampDrmSession.h"
#include "aampoutputprotection.h"
#include <drmbuild_oem.h>
#include <drmcommon.h>
#include <drmmanager.h>
#include <drmmathsafe.h>
#include <drmtypes.h>
#include <drmerr.h>
#undef __in
#undef __out
using namespace std;

#define ChkBufferSize(a,b) do { \
    ChkBOOL((a) <= (b), DRM_E_FAIL); \
} while(FALSE)

/**
 * @class PlayReadyDRMSession
 * @brief Class for PlayReady DRM operations
 */
class PlayReadyDRMSession : public AampDrmSession
{

private:
	DRM_APP_CONTEXT *m_ptrAppContext;
	DRM_DECRYPT_CONTEXT m_oDecryptContext;

	DRM_BYTE *m_sbOpaBuf;
	DRM_DWORD m_cbOpaBuf;

	DRM_BYTE *m_sbRevocateBuf;
	KeyState m_eKeyState;
	DRM_CHAR m_rgchSesnID[CCH_BASE64_EQUIV(SIZEOF(DRM_ID)) + 1];
	DRM_BOOL m_fCommit;

	DRM_BYTE *m_pbPRO;
	DRM_DWORD m_cbPRO;

	DRM_BYTE *m_pbChallenge;
	DRM_DWORD m_cbChallenge;
	DRM_CHAR *m_ptrDestURL;
	pthread_mutex_t decryptMutex;

	AampOutputProtection* m_pOutputProtection;

	/**
	 * @brief Initialize PR DRM session, state will be set as KEY_INIT
	 *        on success KEY_ERROR if failure.
	 */
	void initAampDRMSession();
	/**
	 * @brief Retrieve PlayReady Object(PRO) from init data
	 * @param f_pbInitData : Pointer to initdata
	 * @param f_cbInitData : size of initdata
	 * @param f_pibPRO : Gets updated with PRO
	 * @param f_pcbPRO : size of PRO
	 * @retval DRM_SUCCESS if no errors encountered
	 */
	int _GetPROFromInitData(const DRM_BYTE *f_pbInitData,
			DRM_DWORD f_cbInitData, DRM_DWORD *f_pibPRO, DRM_DWORD *f_pcbPRO);
	/**
	 * @brief Parse init data to retrieve PRO from it
	 * @param f_pbInitData : Pointer to initdata
	 * @param f_cbInitData : size of init data
	 * @retval DRM_SUCCESS if no errors encountered
	 */
	int _ParseInitData(const uint8_t *f_pbInitData, uint32_t f_cbInitData);


public:
	/**
	 * @brief PlayReadyDRMSession Constructor
	 */
	PlayReadyDRMSession(AampLogManager *logObj);
	/**
	 * @brief PlayReadyDRMSession Destructor
	 */
	~PlayReadyDRMSession();
	/**
	 * @brief Create drm session with given init data
	 *        state will be KEY_INIT on success KEY_ERROR if failed
	 * @param f_pbInitData pointer to initdata
	 * @param f_cbInitData init data size
	 */
	void generateAampDRMSession(const uint8_t *f_pbInitData,
			uint32_t f_cbInitData, std::string &customData);
	/**
	 * @brief Generate key request from DRM session
	 *        Caller function should free the returned memory.
	 * @param destinationURL : gets updated with license server url
	 * @param timeout : max timeout untill which to wait for cdm key generation.
	 * @retval Pointer to DrmData containing license request, NULL if failure.
	 */
	DrmData * aampGenerateKeyRequest(string& destinationURL, uint32_t timeout);
	/**
	 * @brief Updates the received key to DRM session
	 * @param key : License key from license server.
	 * @param timeout : max timeout untill which to wait for cdm key processing.
	 * @retval DRM_SUCCESS if no errors encountered
	 */
	int aampDRMProcessKey(DrmData* key, uint32_t timeout);
	/**
	 * @brief Function to decrypt stream  buffer.
	 * @param f_pbIV : Initialization vector.
	 * @param f_cbIV : Initialization vector length.
	 * @param payloadData : Data to decrypt.
	 * @param payloadDataSize : Size of data.
	 * @param ppOpaqueData : pointer to opaque buffer in case of SVP.
	 * @retval Returns 1 on success 0 on failure.
	 */
	int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV,
			const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData);
	/**
	 * @brief Get the current state of DRM Session.
	 * @retval KeyState
	 */
	KeyState getState();
	
	/**
	 * @brief Clear the current session context
	 *        So that new init data can be bound.
	 */
	void clearDecryptContext();

};

#endif

