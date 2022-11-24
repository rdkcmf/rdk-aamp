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
#ifndef _AAMP_LICENSE_FETCHER_INTERFACE_HPP
#define _AAMP_LICENSE_FETCHER_INTERFACE_HPP

#include "priv_aamp.h"

class PrivateInstanceAAMP;
struct LicensePreFetchObject;

class AampLicenseFetcher
{
public:

	/**
	 * @fn UpdateFailedDRMStatus
	 * @brief Function to update the failed DRM status to mark the adaptation sets to be omitted
	 * @param[in] object  - Prefetch object instance which failed
	 */
	virtual void UpdateFailedDRMStatus(LicensePreFetchObject *object) = 0;
};

#endif /* _AAMP_LICENSE_FETCHER_INTERFACE_HPP */
