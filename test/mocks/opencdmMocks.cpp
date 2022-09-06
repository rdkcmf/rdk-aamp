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

#include "opencdmMocks.h"

AampMockOpenCdm *g_mockOpenCdm;

struct OpenCDMSystem* opencdm_create_system(const char keySystem[])
{
	return g_mockOpenCdm->opencdm_create_system(keySystem);
}

OpenCDMError opencdm_construct_session(struct OpenCDMSystem* system,
									   const LicenseType licenseType,
									   const char initDataType[],
									   const uint8_t initData[],
									   const uint16_t initDataLength,
									   const uint8_t CDMData[],
									   const uint16_t CDMDataLength,
									   OpenCDMSessionCallbacks* callbacks,
									   void* userData,
									   struct OpenCDMSession** session)
{
	return g_mockOpenCdm->opencdm_construct_session(system,
													licenseType,
													initDataType,
													initData,
													initDataLength,
													CDMData,
													CDMDataLength,
													callbacks,
													userData,
													session);
}

OpenCDMError opencdm_destruct_system(struct OpenCDMSystem* system)
{
	return g_mockOpenCdm->opencdm_destruct_system(system);
}

KeyStatus opencdm_session_status(const struct OpenCDMSession* session,
								 const uint8_t keyId[],
								 const uint8_t length)
{
	return g_mockOpenCdm->opencdm_session_status(session, keyId, length);
}

OpenCDMError opencdm_session_update(struct OpenCDMSession* session,
									const uint8_t keyMessage[],
									const uint16_t keyLength)
{
	return g_mockOpenCdm->opencdm_session_update(session, keyMessage, keyLength);
}

OpenCDMError opencdm_gstreamer_session_decrypt(struct OpenCDMSession* session,
											   GstBuffer* buffer,
											   GstBuffer* subSample,
											   const uint32_t subSampleCount,
											   GstBuffer* IV,
											   GstBuffer* keyID,
											   uint32_t initWithLast15)
{
	return g_mockOpenCdm->opencdm_gstreamer_session_decrypt(session, buffer, subSample, subSampleCount, IV, keyID,initWithLast15);
}

OpenCDMError opencdm_session_decrypt(struct OpenCDMSession* session,
									 uint8_t encrypted[],
									 const uint32_t encryptedLength,
									 const uint8_t* IV,
									 uint16_t IVLength,
									 const uint8_t* keyId,
									 const uint16_t keyIdLength,
									 uint32_t initWithLast15,
									 uint8_t* streamInfo,
									 uint16_t streamInfoLength)
{
	return g_mockOpenCdm->opencdm_session_decrypt(session, encrypted, encryptedLength, IV, IVLength, keyId, keyIdLength, initWithLast15, streamInfo, streamInfoLength);
}

OpenCDMError opencdm_session_close(struct OpenCDMSession* session)
{
	return g_mockOpenCdm->opencdm_session_close(session);
}

OpenCDMError opencdm_destruct_session(struct OpenCDMSession* session)
{
	return ERROR_NONE;
}
