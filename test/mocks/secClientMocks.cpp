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

#include "secClientMocks.h"

AampMockSecClient *g_mockSecClient;

int32_t SecClient_AcquireLicense(const char *serviceHostUrl,
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
								 uint32_t *refreshDurationSeconds,
								 SecClient_ExtendedStatus *statusInfo)
{
	/* Note that statusInfo is not passed due to mock limitations. */
	return g_mockSecClient->SecClient_AcquireLicense(serviceHostUrl,
													 numberOfRequestMetadataKeys,
													 requestMetadata,
													 numberOfAccessAttributes,
													 accessAttributes,
													 contentMetadata,
													 contentMetadataLength,
													 licenseRequest,
													 licenseRequestLength,
													 keySystemId,
													 mediaUsage,
													 accessToken,
													 licenseResponse,
													 licenseResponseLength,
													 refreshDurationSeconds);
}

int32_t SecClient_FreeResource(const char *resource)
{
	return g_mockSecClient->SecClient_FreeResource(resource);
}
