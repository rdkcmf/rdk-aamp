/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
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

#include <time.h>
#include "Aampcli.h"
#include "AampcliSmokeTest.h"

extern Aampcli mAampcli;
static PlayerInstanceAAMP *mPlayerInstanceAamp;
std::map<std::string, std::string> SmokeTest::smokeTestUrls = std::map<std::string, std::string>();

bool SmokeTest::execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	int result;

	mPlayerInstanceAamp = playerInstanceAamp;

	loadSmokeTestUrls();	
	vodTune("Dash");
	vodTune("Hls");
	liveTune("Live");

	return true;
}

void SmokeTest::loadSmokeTestUrls()
{

	const std::string smokeurlFile("/smoketest.csv");
	int BUFFER_SIZE = 500;
	char buffer[BUFFER_SIZE];
	FILE *fp;

        if ( (fp = mAampcli.getConfigFile(smokeurlFile)) != NULL)
        { 
                printf("%s:%d: opened smoketest file\n",__FUNCTION__,__LINE__);
         
		while(!feof(fp))
		{
			if(fgets(buffer, BUFFER_SIZE, fp) != NULL)
			{
				buffer[strcspn(buffer, "\n")] = 0;
				std::string urlData(buffer);

				std::size_t delimiterPos = urlData.find(",");

				if(delimiterPos != std::string::npos)
				{
					smokeTestUrls[ urlData.substr(0, delimiterPos) ] = urlData.substr(delimiterPos + 1);
				}
			}	
		}
       
		fclose(fp);
        }

}

void SmokeTest::vodTune(const char *stream)
{
	const char *url = NULL;
	std::string fileName;
	std::string testFilePath;
	FILE *fp;
	time_t initialTime = 0, currentTime = 0;
	std::map<std::string, std::string>::iterator itr;

	createTestFilePath(testFilePath);
	if(strncmp(stream,"Dash",4) == 0)
	{
		fileName = testFilePath + "tuneDashStream.txt";
		itr = smokeTestUrls.find("VOD_DASH");

		itr = smokeTestUrls.find("VOD_DASH");

		if(itr != smokeTestUrls.end())
		{	
			url = (itr->second).c_str();
		}

		if(url == NULL)
		{
			url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd";
		}
		
	}
	else if(strncmp(stream,"Hls",3) == 0)
	{
		fileName = testFilePath + "tuneHlsStream.txt";
		itr = smokeTestUrls.find("VOD_HLS");

		if(itr != smokeTestUrls.end())
		{	
			url = (itr->second).c_str();
		}

		if(url == NULL)
		{
			url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8";
		}
	}
	
	fp = stdout;
	stdout = fopen(fileName.c_str(),"w");
	
	mPlayerInstanceAamp->Tune(url);

	initialTime = time(NULL);

	while(1)
	{
		sleep(5);
		if(mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("%s:%d: Tune sub task started\n",__FUNCTION__,__LINE__);
			mPlayerInstanceAamp->Tune(url);
			mPlayerInstanceAamp->SetRate(0); // To pause 
			mPlayerInstanceAamp->SetRate(4); // To fastforward 
			mPlayerInstanceAamp->SetRate(1); // To play
			sleep(20);
			mPlayerInstanceAamp->SetRate(-2); // To rewind
			sleep(10);
			mPlayerInstanceAamp->Stop();
			sleep(3);

			printf("%s:%d: Tune %s completed\n",__FUNCTION__,__LINE__,stream);
			break;
		}

		currentTime = time(NULL);
		if((currentTime - initialTime) > 1200)
		{
			break;
		}
	}

	fclose(stdout);
	stdout = fp;
}

void SmokeTest::liveTune(const char *stream)
{
	const char *url = NULL;
	std::string fileName;
	std::string testFilePath;
	FILE *fp;

	createTestFilePath(testFilePath);

	url = "https://ll-usw2.akamaized.net/live/disk/sky/DASH-LL-sky.toml/sky.mpd";
	fileName = testFilePath +"tuneLive.txt";

	fp = stdout;
	stdout = fopen(fileName.c_str(),"w");

	mPlayerInstanceAamp->Tune(url);
	sleep(10);
	mPlayerInstanceAamp->SetRate(0); // To pause 
	mPlayerInstanceAamp->SetRate(1); // To play
	sleep(10);
	mPlayerInstanceAamp->Stop();
	sleep(5);

	fclose(stdout);
	stdout = fp;

}

bool SmokeTest::createTestFilePath(std::string &filePath)
{
#ifdef AAMP_SIMULATOR_BUILD
	char *ptr = getenv("HOME");
	if(ptr)
	{
		filePath.append(ptr); 
		filePath += "/smoketest/";
	}
	else
	{
		printf("%s:%d: Path not found\n",__FUNCTION__,__LINE__);
		return false;
	}
#else
	filePath = "/opt/smoketest/";
#endif
	DIR *dir = opendir(filePath.c_str());
	if (!dir)
	{
		mkdir(filePath.c_str(), 0777);
	}
	else
	{
		closedir(dir);
	}

	return true;
}

