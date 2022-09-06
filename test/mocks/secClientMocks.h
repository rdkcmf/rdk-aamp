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

#ifndef AAMP_MOCK_SEC_CLIENT_H
#define AAMP_MOCK_SEC_CLIENT_H

#include <sys/types.h>
#include <stdint.h>
#include "sec_security_datatype.h"
#include "sec_client.h"
#include <gmock/gmock.h>

class AampMockSecClient
{
	public:
		MOCK_METHOD(int32_t, SecClient_AcquireLicense, (const char *serviceHostUrl,
														uint8_t numberOfRequestMetadataKeys,
														const char *requestMetadata[][2],
														uint8_t numberOfAccessAttributes,
														const char *accessAttributes[][2],
														const char *contentMetadata,
														size_t contentMetadataLength,
														const char *licenseRequest,
														size_t licenseRequestLength,
														const char *keySystemId,
														const char *mediaUsage,
														const char *accessToken,
														char **licenseResponse,
														size_t *licenseResponseLength,
														uint32_t *refreshDurationSeconds));
		MOCK_METHOD(int32_t, SecClient_FreeResource, (const char *resource));
};

extern AampMockSecClient *g_mockSecClient;

#endif /* AAMP_MOCK_SEC_CLIENT_H */
