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
* @file aampdrmsessionfactory.cpp
* @brief Source file for AampDrmSessionFactory
*/

#include "aampdrmsessionfactory.h"
#if defined(USE_OPENCDM_ADAPTER)
#include "AampOcdmBasicSessionAdapter.h"
#include "AampOcdmGstSessionAdapter.h"
#elif defined(USE_OPENCDM)
#include "opencdmsession.h"
#else
#if defined(USE_PLAYREADY)
#include "playreadydrmsession.h"
#endif
#endif
#include "ClearKeyDrmSession.h"

/**
 *  @brief		Creates an appropriate DRM session based on the given DrmHelper
 *
 *  @param[in]	drmHelper - DrmHelper instance
 *  @return		Pointer to DrmSession.
 */
AampDrmSession* AampDrmSessionFactory::GetDrmSession(AampLogManager *logObj, std::shared_ptr<AampDrmHelper> drmHelper, AampDrmCallbacks *drmCallbacks)
{
	const std::string systemId = drmHelper->ocdmSystemId();

#if defined (USE_OPENCDM_ADAPTER)
	if (drmHelper->isClearDecrypt())
	{
#if defined(USE_CLEARKEY)
		if (systemId == CLEAR_KEY_SYSTEM_STRING)
		{
			return new ClearKeySession(logObj);
		}
		else
#endif
		{
			return new AAMPOCDMBasicSessionAdapter(logObj, drmHelper, drmCallbacks);
		}
	}
	else
	{
		return new AAMPOCDMGSTSessionAdapter(logObj, drmHelper);
	}
#elif defined (USE_OPENCDM)
	return new AAMPOCDMSession(systemId);
#else // No form of OCDM support. Attempt to fallback to hardcoded session classes
	if (systemId == PLAYREADY_KEY_SYSTEM_STRING)
	{
#if defined(USE_PLAYREADY)
		return new PlayReadyDRMSession(logObj);
#endif // USE_PLAYREADY
	}
	else if (systemId == CLEAR_KEY_SYSTEM_STRING)
	{
#if defined(USE_CLEARKEY)
		return new ClearKeySession(logObj);
#endif // USE_CLEARKEY
	}
#endif // Not USE_OPENCDM
	return NULL;
}
