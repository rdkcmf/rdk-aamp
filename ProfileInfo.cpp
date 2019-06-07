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

#include "ProfileInfo.h"


/**
 *   @brief  Converts class object data to Json object
 *
 *   @param[in]  NONE
 *
 *   @return cJSON pointer
 */
cJSON * CProfileInfo::ToJson() const
{
	cJSON *monitor = cJSON_CreateObject();
	if(monitor)
	{
		cJSON * jsonObj = NULL;
		bool isDataAdded = false;
		if(mpManifestStat)
		{
			jsonObj = mpManifestStat->ToJson();
			if(jsonObj)
			{
				cJSON_AddItemToObject(monitor, "manifestStat", jsonObj);
				isDataAdded = true;
			}
		}

		if(mpFragmentStat)
		{
			jsonObj = mpFragmentStat->ToJson();
			if(jsonObj)
			{
				cJSON_AddItemToObject(monitor, "fragStat", jsonObj);
				isDataAdded = true;
			}
		}

		if( mplicenseStatistics)
		{
			jsonObj = mplicenseStatistics->ToJson();
			if(jsonObj)
			{
				cJSON_AddItemToObject(monitor, "licStat", jsonObj);
				isDataAdded = true;
			}
		}

		if(!isDataAdded)
		{
			//nothing is added to monitor
			//delete monitor
			cJSON_Delete(monitor);
			monitor = NULL;
		}
	}
	return monitor;
}



