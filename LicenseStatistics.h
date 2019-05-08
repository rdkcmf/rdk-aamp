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

#ifndef __LICENSE_STATISTICS_H__
#define __LICENSE_STATISTICS_H__

#include "HTTPStatistics.h"

class CLicenseStatistics
{
private:
	int mTotalRotations; // total liecense rotation/switch
	int mTotalEncryptedToClear; // Encrypted to clear liecense switch
	int mTotalClearToEncrypted; // Clear to encrypted liecense switch
public:
	CLicenseStatistics() :mTotalRotations(COUNT_NONE), mTotalEncryptedToClear(COUNT_NONE) , mTotalClearToEncrypted(COUNT_NONE)
	{

	}

	/**
	 *   @brief  Increments License stat count
	 *   @param[in]  VideoStatCountType
     *
	 *   @return None
	 */
	void IncrementCount(VideoStatCountType type);

	/**
	 *   @brief  Converts class object data to Json object
	 *
	 *   @param[in]  NONE
     *
	 *   @return cJSON pointer
	 */
	cJSON * ToJson() const;
};




#endif /* __LICENSE_STATISTICS_H__ */
