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

#ifndef AAMPDRMSYSTEMS_H
#define AAMPDRMSYSTEMS_H

/**
 * @brief DRM system types
 * @note these are now deprecated in favor of DrmHelpers, don't expand this
 */
enum DRMSystems
{
	eDRM_NONE,              /**< No DRM */
	eDRM_WideVine,          /**< Widevine, used to set legacy API */
	eDRM_PlayReady,         /**< Playread, used to set legacy APIy */
	eDRM_CONSEC_agnostic,   /**< CONSEC Agnostic DRM, deprecated */
	eDRM_Adobe_Access,      /**< Adobe Access, fully deprecated */
	eDRM_Vanilla_AES,       /**< Vanilla AES, fully deprecated */
	eDRM_ClearKey,          /**< Clear key, used to set legacy API */
	eDRM_MAX_DRMSystems     /**< Drm system count */
};


#endif /* AAMPDRMSYSTEMS_H */

