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

/**
 * @file AampcliHarvestor.cpp
 * @brief Stand alone AAMP player with command line interface.
 */

#include <unistd.h>
#include "AampcliHarvestor.h"

Harvestor::Harvestor() : mMasterHarvestorThreadID(0),
			 mSlaveHarvestorThreadID(0),
			 mReportThread(0),
			 mSlaveVideoThreads{0},
			 mSlaveAudioThreads{0},
			 mSlaveIFrameThread(0)
{}

std::map<pthread_t, HarvestProfileDetails> Harvestor::mHarvestInfo = std::map<pthread_t, HarvestProfileDetails>();
std::vector<pthread_t> Harvestor::mHarvestThreadId(0);
PlayerInstanceAAMP * Harvestor::mPlayerInstanceAamp = NULL;
bool Harvestor::mHarvestReportFlag = false;
Harvestor mHarvestor;

bool Harvestor::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char harvestCmd[mHarvestCommandLength] = {'\0'};
	bool slaveFlag = false;

	Harvestor::mPlayerInstanceAamp = playerInstanceAamp;

	int len = strlen(cmd) - 8;
	strncpy(harvestCmd, cmd+8, len);
	
	if(memcmp(harvestCmd,"harvestMode=Master",18) == 0)
	{
		printf("%s:%d: pthread_create:MasterHarvestor thread\n", __FUNCTION__, __LINE__);

		if(0 != pthread_create(&mMasterHarvestorThreadID, NULL, &masterHarvestor, harvestCmd))
		{
			printf("%s:%d: Error at pthread_create:MasterHarvestor thread\n", __FUNCTION__, __LINE__);
		}
	}
	else if(memcmp(harvestCmd,"harvestMode=Slave",17) == 0)
	{
		printf("%s:%d: pthread_create:SlaveHarvestor thread\n", __FUNCTION__, __LINE__);

		if(0 != pthread_create(&mSlaveHarvestorThreadID, NULL, &slaveHarvestor, harvestCmd))
		{
			printf("%s:%d: Error at  pthread_create:SlaveHarvestor thread\n", __FUNCTION__, __LINE__);
		}
	}
	else
	{
		printf("[AAMPCLI] Invalid harvest command %s\n", cmd);
	}

	return true;
}

void* Harvestor::masterHarvestor(void * arg)
{
	char * cmd = (char *)arg;
	std::map<std::string, std::string> cmdlineParams;
	std::vector<std::string> params;
	std::stringstream ss(cmd);
	std::string item;
	long currentAudioBitrate = 0,currentVideoBitrate = 0,currentIFrameBitrate = 0;
	FILE * pIframe;
	int videoThreadId = 0;
	int audioThreadId = 0;

	if(aamp_pthread_setname(pthread_self(), "MasterHarvestor"))
	{
		printf("%s:%d: aamp_pthread_setname failed\n",__FUNCTION__,__LINE__);
	}

	while(std::getline(ss, item, ' ')) {
		params.push_back(item);
	}

	params.push_back("harvestCountLimit=9999999");
	params.push_back("harvestConfig=65535");
	params.push_back("defaultBitrate=9999999");
	params.push_back("defaultBitrate4K=9999999");
	params.push_back("abr");
	params.push_back("suppressDecode=true");

	for(std::string param : params)
	{
		mHarvestor.mPlayerInstanceAamp->mConfig.ProcessConfigText(param, AAMP_DEV_CFG_SETTING);
		mHarvestor.mPlayerInstanceAamp->mConfig.DoCustomSetting(AAMP_DEV_CFG_SETTING);

		std::size_t delimiterPos = param.find("=");

		if(delimiterPos != std::string::npos)
		{
			cmdlineParams[ param.substr(0, delimiterPos) ] = param.substr(delimiterPos + 1);
		}
	}

	if(cmdlineParams.find("harvestUrl") != cmdlineParams.end())
	{
		mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

		if(0 != pthread_create(&mHarvestor.mReportThread, NULL, &mHarvestor.startHarvestReport, (char *) cmdlineParams["harvestUrl"].c_str()))
		{
			printf("%s:%d: Error at  pthread_create: startHarvestReport thread\n",__FUNCTION__,__LINE__);
		}

		std::string iFrameCommand = "aamp-cli harvest harvestMode=Slave harvestUrl="+cmdlineParams["harvestUrl"]+" harvestCountLimit=9999999"+" harvestConfig="+std::to_string(getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO))+" abr suppressDecode=true";

		if(cmdlineParams.find("harvestPath") != cmdlineParams.end())
		{
			iFrameCommand = iFrameCommand+" harvestPath="+cmdlineParams["harvestPath"];	
		}

		printf("%s:%d: Start process of IFrame command %s \n",__FUNCTION__,__LINE__,iFrameCommand.c_str());

		pIframe = popen(iFrameCommand.c_str(), "r");
		
		if (!pIframe) 
		{
			puts("Can't connect to bc");
		}
		else
		{
			if(0 != pthread_create(&mHarvestor.mSlaveIFrameThread, NULL, &slaveDataOutput, pIframe))
			{
				printf("%s:%d: Error at  pthread_create: slaveDataOutput thread\n",__FUNCTION__,__LINE__);
			}
			else
			{
				printf("%s:%d: Sucessfully launched harvest for IFrame\n",__FUNCTION__,__LINE__);
			}

		}

		currentVideoBitrate = mHarvestor.mPlayerInstanceAamp->GetVideoBitrate();
		currentAudioBitrate = mHarvestor.mPlayerInstanceAamp->GetAudioBitrate();

		std::vector<long> cacheVideoBitrates,cacheAudioBitrates,cacheTextTracks;

		while(1)
		{

			std::vector<long> tempVideoBitrates,tempAudioBitrates;
			std::vector<long> diffVideoBitrates,diffAudioBitrates;
			long tempIFrameBitrate,cacheIFrameBitrate;
			sleep (1);

			if(mHarvestor.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
			{
				sleep(5); //Waiting for slave process to complete
			}

			if ((mHarvestor.mPlayerInstanceAamp->GetState() != eSTATE_COMPLETE) && (mHarvestor.mPlayerInstanceAamp->GetState() > eSTATE_PLAYING))
			{
				printf("%s:%d: Tune stopped\n",__FUNCTION__,__LINE__);
				mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());
			}

			tempAudioBitrates = mHarvestor.mPlayerInstanceAamp->GetAudioBitrates();
			std::sort(tempAudioBitrates.begin(), tempAudioBitrates.end());

			tempVideoBitrates = mHarvestor.mPlayerInstanceAamp->GetVideoBitrates();
			std::sort(tempVideoBitrates.begin(), tempVideoBitrates.end());

			std::vector<long>::iterator position = std::find(tempVideoBitrates.begin(), tempVideoBitrates.end(), mHarvestor.mPlayerInstanceAamp->GetVideoBitrate());
			if (position != tempVideoBitrates.end()) 
				tempVideoBitrates.erase(position);

			if (cacheVideoBitrates.size() == 0)
			{
				diffVideoBitrates = tempVideoBitrates;
			}
			else
			{
				for (long tempVideoBitrate: tempVideoBitrates)
				{
					if ( ! (std::find(cacheVideoBitrates.begin(), cacheVideoBitrates.end(), tempVideoBitrate) != cacheVideoBitrates.end()) )
					{
						diffVideoBitrates.push_back(tempVideoBitrate);
					}
				}
			}

			if (cacheAudioBitrates.size() == 0)
			{
				diffAudioBitrates = tempAudioBitrates;
			}
			else
			{
				for (long tempAudioBitrate: tempAudioBitrates)
				{
					if ( ! (std::find(cacheAudioBitrates.begin(), cacheAudioBitrates.end(), tempAudioBitrate) != cacheAudioBitrates.end()) )
					{
						diffAudioBitrates.push_back(tempAudioBitrate);
					}
				}
			}

			if(diffVideoBitrates.size())
			{
				for(auto bitrate:diffVideoBitrates)
				{

					FILE *p;

					std::string videoCommand = "aamp-cli harvest harvestMode=Slave harvestUrl="+cmdlineParams["harvestUrl"]+" harvestCountLimit=9999999"+" harvestConfig="+std::to_string(getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_LICENCE))+" abr suppressDecode=true defaultBitrate="+std::to_string(bitrate);

					if(cmdlineParams.find("harvestPath") != cmdlineParams.end())
					{
						videoCommand = videoCommand+" harvestPath="+cmdlineParams["harvestPath"];	
					}

					printf("%s:%d: Start process of video command %s \n",__FUNCTION__,__LINE__,videoCommand.c_str());

					p = popen(videoCommand.c_str(), "r");
					
					if (!p) 
					{
						puts("Can't connect to bc");
						printf("%s:%d: Process fails ",__FUNCTION__,__LINE__);
					}
					else
					{
						if(0 != pthread_create(&mHarvestor.mSlaveVideoThreads[videoThreadId++], NULL, &slaveDataOutput, p))
						{
							printf("%s:%d: Error at  pthread_create: slaveDataOutput thread\n",__FUNCTION__,__LINE__);
						}
						else
						{
							printf("%s:%d: Sucessfully launched harvest for video bitrate = %ld\n",__FUNCTION__,__LINE__,bitrate );
							pthread_join(mHarvestor.mSlaveVideoThreads[videoThreadId++], NULL);
						}

					}
				}

				cacheVideoBitrates = tempVideoBitrates;
			}

			if(diffAudioBitrates.size())
			{
				for(auto bitrate:diffAudioBitrates)
				{

					FILE * p;

					std::string audioCommand = "aamp-cli harvest harvestMode=Slave harvestUrl="+cmdlineParams["harvestUrl"]+" harvestCountLimit=9999999"+" harvestConfig="+std::to_string(getHarvestConfigForMedia(eMEDIATYPE_AUDIO))+" abr suppressDecode=true defaultBitrate="+std::to_string(bitrate);

					if(cmdlineParams.find("harvestPath") != cmdlineParams.end())
					{
						audioCommand = audioCommand+" harvestPath="+cmdlineParams["harvestPath"];	
					}

					printf("%s:%d: Start process Audio command %s \n",__FUNCTION__,__LINE__,audioCommand.c_str());
					p = popen(audioCommand.c_str(), "r");
					
					if (!p) 
					{
						puts("Can't connect to bc");
					}
					else
					{
						if(0 != pthread_create(&mHarvestor.mSlaveAudioThreads[audioThreadId++], NULL, &slaveDataOutput, p))
						{
							printf("%s:%d: Error at  pthread_create: slaveDataOutput thread\n",__FUNCTION__,__LINE__);
						}
						else
						{
							printf("%s:%d: Sucessfully launched harvest for audio bitrate = %ld\n ",__FUNCTION__,__LINE__,bitrate );
							pthread_join(mHarvestor.mSlaveAudioThreads[videoThreadId++], NULL);
						}
					}
				}

				cacheAudioBitrates = tempAudioBitrates;
			}
		}
		
		pthread_join(mHarvestor.mSlaveIFrameThread, NULL);
	}
	else
	{
		printf("%s:%d: harvestUrl not found\n",__FUNCTION__,__LINE__);
	}

	return NULL;
}

void* Harvestor::slaveHarvestor(void * arg)
{
	char * cmd = (char *)arg;
	std::map<std::string, std::string> cmdlineParams;
	std::vector<std::string> params;
	std::stringstream ss(cmd);
	std::string item,harvestUrl;

	if(aamp_pthread_setname(pthread_self(), "SlaveHarvestor"))
	{
		printf("%s:%d: SlaveHarvestor_thread aamp_pthread_setname failed\n",__FUNCTION__,__LINE__);
	}

	while(std::getline(ss, item, ' ')) {
		params.push_back(item);
	}

	params.push_back("suppressDecode=true");

	for(std::string param : params)
	{

		mHarvestor.mPlayerInstanceAamp->mConfig.ProcessConfigText(param, AAMP_DEV_CFG_SETTING);
		mHarvestor.mPlayerInstanceAamp->mConfig.DoCustomSetting(AAMP_DEV_CFG_SETTING);

		std::size_t delimiterPos = param.find("=");

		if(delimiterPos != std::string::npos) 
		{
			cmdlineParams[ param.substr(0, delimiterPos) ] = param.substr(delimiterPos + 1);
		}
	}


	int harvestConfig = 0;
	mHarvestor.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig,harvestConfig) ;

	mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

	if(harvestConfig == (getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)))
	{
		float rate = 2;
		mHarvestor.mPlayerInstanceAamp->SetRate(rate);

	}

	while (1)
	{
		sleep(1);

		if(mHarvestor.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("%s:%d: Tune completed exiting harvesting mode\n",__FUNCTION__,__LINE__);
			break;
		}

		if (mHarvestor.mPlayerInstanceAamp->GetState() > eSTATE_PLAYING)
		{
			mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

			if(harvestConfig == (getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)))
			{
				float rate = 2;
				mHarvestor.mPlayerInstanceAamp->SetRate(rate);

			}
		}
	}
	
	return NULL;
}

void *Harvestor::slaveDataOutput(void * arg)
{
	FILE * pipe;
	pipe = (FILE *) arg;
	char buffer[500] = {'\0'};
	if(aamp_pthread_setname(pthread_self(), "SlaveOutput"))
	{
		printf("%s:%d: slaveDataOutput aamp_pthread_setname failed\n",__FUNCTION__, __LINE__);
	}

	while (!feof(pipe)) 
	{
		if (fgets(buffer, 500, pipe) != NULL)
		{
			mHarvestor.getHarvestReportDetails(buffer);
			printf("%s:%d: [0x%p] %s",__FUNCTION__, __LINE__,pipe,buffer);
		}
	}

	return NULL;
}

long Harvestor::getNumberFromString(std::string buffer) 
{
	std::stringstream strm;
	strm << buffer; 
	std::string tempStr;
	long value = 0;
	while(!strm.eof()) {
		strm >> tempStr; 
		if(std::stringstream(tempStr) >> value) 
		{
			return value;	
		}

		tempStr = ""; 
	}

	return value;
}

void * Harvestor::startHarvestReport(void * arg)
{
	char * harvesturl = (char *)arg;
	FILE *fp;
	FILE *errorfp;
	char filename[18] = {'\0'};
	char errorFilename[23] = {'\0'};
	
	sprintf(filename,"HarvestReport.txt");
	sprintf(errorFilename,"HarvestErrorReport.txt");

	fp = fopen(filename, "w");

	if (fp == NULL)
	{
		printf("Error opening the file %s\n", filename);
		return NULL;
	}

	fprintf(fp, "Harvest Profile Details\n\n");
	fprintf(fp, "Harvest url : %s\n",harvesturl);
	fprintf(fp, "Harvest mode : Master\n");
	fclose(fp);

	Harvestor::mHarvestReportFlag = true;

	errorfp = fopen(errorFilename, "w");

	if (errorfp == NULL)
	{
		printf("Error opening the file %s\n", filename);
		return NULL;
	}

	fprintf(errorfp, "Harvest Error Report\n\n");
	fprintf(errorfp, "Harvest url : %s\n",harvesturl);
	fclose(errorfp);
	return NULL;
}

bool Harvestor::getHarvestReportDetails(char *buffer)
{
	char *harvestPath = NULL;
	char *token = NULL;
	pthread_t threadId = 0;
	HarvestProfileDetails l_harvestProfileDetails;
	std::map<pthread_t, HarvestProfileDetails>::iterator itr;

	memset(&l_harvestProfileDetails,'\0',sizeof(l_harvestProfileDetails));
	threadId = pthread_self();

	if(mHarvestInfo.find(threadId) == mHarvestInfo.end())
	{
		mHarvestInfo.insert( std::make_pair(threadId, l_harvestProfileDetails) );
		mHarvestThreadId.push_back(threadId);
	}

	itr = mHarvestInfo.find(threadId);

	if ((itr->second.harvestConfig == 0) && (strstr(buffer, "harvestConfig -") != NULL))
	{
		token = strstr(buffer, "harvestConfig -");
		
		itr->second.harvestConfig = getNumberFromString(token);

		if(itr->second.harvestConfig == 193)
			strcpy(itr->second.media,"IFrame");

		if(itr->second.harvestConfig == 161)
			strcpy(itr->second.media,"Video");

		if(itr->second.harvestConfig == 2)
			strcpy(itr->second.media,"Audio");
	}

	if ((itr->second.bitrate == 0) && (strstr(buffer, "defaultBitrate -") != NULL))
	{
		token = strstr(buffer, "defaultBitrate -");
		itr->second.bitrate = getNumberFromString(token);
	}

	if (strstr(buffer, "harvestCountLimit:") != NULL)
	{
		token = strstr(buffer, "harvestCountLimit:");
		itr->second.harvestFragmentsCount = getNumberFromString(token);
	}

	if ((strstr(buffer, "error") != NULL) || (strstr(buffer, "File open failed") != NULL) )
	{
		writeHarvestErrorReport(itr->second,buffer);
			
	}

	if ( (itr->second.harvestEndFlag == false) && (strstr(buffer, "EndOfStreamReached") != NULL))
	{
		writeHarvestEndReport(itr->second,buffer);
		itr->second.harvestEndFlag = true;
	}

	return true;
}

void Harvestor::writeHarvestErrorReport(HarvestProfileDetails harvestProfileDetails, char *buffer)
{

	FILE *errorfp;
	char errorFilename[23] = {'\0'};
	sprintf(errorFilename,"HarvestErrorReport.txt");

	errorfp = fopen(errorFilename, "a");

	if (errorfp == NULL)
	{
		printf("Error opening the file %s\n", errorFilename);
	}

	if (strstr(buffer, "error") != NULL)
	{
		harvestProfileDetails.harvesterrorCount++;
		fprintf(errorfp, "Error : %s",buffer);
	}

	if (strstr(buffer, "File open failed") != NULL)
	{
		harvestProfileDetails.harvestFailureCount++;
		fprintf(errorfp, "Failed : %s",buffer);
	}

	fclose(errorfp);
}

void Harvestor::writeHarvestEndReport(HarvestProfileDetails harvestProfileDetails, char *buffer)
{
	FILE *fp;
	char filename[18] = {'\0'};
	int harvestCountLimit = 0;

		sprintf(filename,"HarvestReport.txt");
		fp = fopen(filename, "a");

		if (fp == NULL)
		{
			printf("Error opening the file %s\n", filename);
			return;
		}

		mHarvestor.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestCountLimit,harvestCountLimit);
		harvestProfileDetails.harvestFragmentsCount = harvestCountLimit - harvestProfileDetails.harvestFragmentsCount; 

		fprintf(fp, "\nMedia : %s\nBitrate : %lu\nTotal fragments count : %d\nError fragments count : %d\nFailure fragments count : %d\n",harvestProfileDetails.media,harvestProfileDetails.bitrate,harvestProfileDetails.harvestFragmentsCount,harvestProfileDetails.harvesterrorCount,harvestProfileDetails.harvestFailureCount);
	
		fclose(fp);

}

void Harvestor::harvestTerminateHandler(int signal)
{

	if((Harvestor::mHarvestReportFlag == true) && (!mHarvestThreadId.empty()))
	{
		FILE *fp;
		char filename[18] = {'\0'};
		int harvestCountLimit = 0;

		sprintf(filename,"HarvestReport.txt");
		fp = fopen(filename, "a");

		if (fp == NULL)
		{
			printf("Error opening the file %s\n", filename);
			return;
		}

		for(auto item: mHarvestThreadId)
		{
			auto itr = mHarvestInfo.find(item);

			if((itr != mHarvestInfo.end()) && (itr->second.harvestEndFlag == false))
			{
				mHarvestor.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestCountLimit,harvestCountLimit);
				itr->second.harvestFragmentsCount = harvestCountLimit - itr->second.harvestFragmentsCount; 

				fprintf(fp, "\nMedia : %s\nBitrate : %lu\nTotal fragments count : %d\nError fragments count : %d\nFailure fragments count : %d\n",itr->second.media,itr->second.bitrate,itr->second.harvestFragmentsCount,itr->second.harvesterrorCount,itr->second.harvestFailureCount);

				itr->second.harvestEndFlag = true;
			}
		}

		fclose(fp);
	}

	printf("%s:%d: Signal handler\n",__FUNCTION__, __LINE__);
	
	exit(signal);
}
