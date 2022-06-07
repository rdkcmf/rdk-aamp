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


/**
 * @file AampDrmHelper.cpp
 * @brief Drm Helper file to handle DRM operation
 */

#include <algorithm>
#include <regex>

#include "AampDrmHelper.h"

/**
 * @brief Compare against another helper instance
 */
bool AampDrmHelper::compare(std::shared_ptr<AampDrmHelper> other)
{
	if (other == nullptr) return false;
	if (mDrmInfo.systemUUID != other->mDrmInfo.systemUUID) return false;
	if (mDrmInfo.mediaFormat != other->mDrmInfo.mediaFormat) return false;
	if (ocdmSystemId() != other->ocdmSystemId()) return false;
	if (getDrmMetaData() != other->getDrmMetaData()) return false;

	std::vector<uint8_t> thisKeyId;
	getKey(thisKeyId);

	std::vector<uint8_t> otherKeyId;
	other->getKey(otherKeyId);

	if (thisKeyId != otherKeyId) return false;

	return true;
}
