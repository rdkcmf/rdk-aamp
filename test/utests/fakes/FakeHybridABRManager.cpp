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

#include "HybridABRManager.h"


void HybridABRManager::ReadPlayerConfig(AampAbrConfig *mAampAbrConfig)
{
}

long HybridABRManager::CheckAbrThresholdSize(int bufferlen, int downloadTimeMs ,long currentProfilebps ,int fragmentDurationMs , CurlAbortReason abortReason)
{
	return 0;
}

void HybridABRManager::UpdateABRBitrateDataBasedOnCacheLength(std::vector < std::pair<long long,long> > &mAbrBitrateData,long downloadbps,bool LowLatencyMode)
{
}

void HybridABRManager::UpdateABRBitrateDataBasedOnCacheLife(std::vector < std::pair<long long,long> > &mAbrBitrateData , std::vector< long> &tmpData)
{
}

long HybridABRManager::UpdateABRBitrateDataBasedOnCacheOutlier(std::vector< long> &tmpData)
{
	return 0;
}

bool HybridABRManager::CheckProfileChange(double totalFetchedDuration ,int currProfileIndex , long availBW)
{
	return false;
}

bool HybridABRManager::IsLowestProfile(int currentProfileIndex,bool IstrickplayMode)
{
	return false;
}

void HybridABRManager::GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId)
{
}


void HybridABRManager::CheckRampupFromSteadyState(int currProfileIndex,int &newProfileIndex,long nwBandwidth,double bufferValue,long newBandwidth,BitrateChangeReason &mhBitrateReason,int &mMaxBufferCountCheck,const std::string& periodId)
{
}

void HybridABRManager::CheckRampdownFromSteadyState(int currProfileIndex, int &newProfileIndex,BitrateChangeReason &mBitrateReason,int mABRLowBufferCounter,const std::string& periodId)
{
}

long long HybridABRManager::ABRGetCurrentTimeMS(void)
{
	return 0;
}

bool HybridABRManager::GetLowLatencyStartABR()
{
	return false;
}

void HybridABRManager::SetLowLatencyStartABR(bool bStart)
{
}

bool HybridABRManager::GetLowLatencyServiceConfigured()
{
	return false;
}

void HybridABRManager::SetLowLatencyServiceConfigured(bool bConfig)
{
}

bool HybridABRManager::IsABRDataGoodToEstimate(long time_diff) 
{
	return false;
}

void HybridABRManager::CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache,long &bitsPerSecond,long time_now,long total_dl_diff,long time_diff,long currentTotalDownloaded)
{
}

