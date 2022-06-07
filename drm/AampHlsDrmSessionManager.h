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

#ifndef _AAMP_HLS_DRM_SESSION_MGR_H
#define _AAMP_HLS_DRM_SESSION_MGR_H

/**
 * @file AampHlsDrmSessionManager.h
 * @brief Operations for HLS DRM
 */


#include "HlsDrmBase.h"
#include "AampDrmSession.h"


/**
 * @class AampHlsDrmSessionManager
 * @brief DRM Session manager for HLS stream operations
 */

class AampHlsDrmSessionManager
{
	AampDrmSession* mDrmSession;
public:
	/**
	 * @fn getInstance 
	 * @return Aamp Hls Drm Session Manager instance
	 */
	static AampHlsDrmSessionManager& getInstance();

	/**
	 * @fn isDrmSupported
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM support available, false otherwise
	 */
	 bool isDrmSupported(const struct DrmInfo& drmInfo) const;

	/**
	 * @fn createSession
	 * @param aampInstance aampContext
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM support available, false otherwise
	 */
	std::shared_ptr<HlsDrmBase> createSession(PrivateInstanceAAMP* aampInstance, const struct DrmInfo& drmInfo, MediaType streamType, AampLogManager *logObj=NULL);
};

#endif //_AAMP_HLS_DRM_SESSION_MGR_H
