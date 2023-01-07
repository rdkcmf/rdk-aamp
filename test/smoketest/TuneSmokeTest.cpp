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

/**
 * @file TuneSmokeTest.cpp
 * @brief Tune SmokeTest cases 
 */

#include <time.h>
#include "AampSmokeTestPlayer.h"
#include "TuneSmokeTest.h"

extern AampPlayer mAampPlayer;

bool SmokeTest::dashPauseState = false;
bool SmokeTest::dashFastForwardState = false;
bool SmokeTest::dashPlayState = false;
bool SmokeTest::dashRewindState = false;
bool SmokeTest::hlsPauseState = false;
bool SmokeTest::hlsFastForwardState = false;
bool SmokeTest::hlsPlayState = false;
bool SmokeTest::hlsRewindState = false;
bool SmokeTest::livePlayState = false;
bool SmokeTest::livePauseState = false;

std::vector<std::string> SmokeTest::dashTuneData(0);
std::vector<std::string> SmokeTest::hlsTuneData(0);
std::vector<std::string> SmokeTest::liveTuneData(0);

std::map<std::string, std::string> SmokeTest::smokeTestUrls = std::map<std::string, std::string>();

bool SmokeTest::aampTune()
{
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

        if ( (fp = mAampPlayer.getConfigFile(smokeurlFile)) != NULL)
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
	
	mAampPlayer.mPlayerInstanceAamp->Tune(url);

	initialTime = time(NULL);

	while(1)
	{
		sleep(5);
		if(mAampPlayer.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("%s:%d: Tune sub task started\n",__FUNCTION__,__LINE__);
			mAampPlayer.mPlayerInstanceAamp->Tune(url);
			mAampPlayer.mPlayerInstanceAamp->SetRate(0); // To pause 
			mAampPlayer.mPlayerInstanceAamp->SetRate(4); // To fastforward 
			mAampPlayer.mPlayerInstanceAamp->SetRate(1); // To play
			sleep(20);
			mAampPlayer.mPlayerInstanceAamp->SetRate(-2); // To rewind
			sleep(10);
			mAampPlayer.mPlayerInstanceAamp->Stop();
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

	mAampPlayer.mPlayerInstanceAamp->Tune(url);
	sleep(10);
	mAampPlayer.mPlayerInstanceAamp->SetRate(0); // To pause 
	mAampPlayer.mPlayerInstanceAamp->SetRate(1); // To play
	sleep(10);
	mAampPlayer.mPlayerInstanceAamp->Stop();
	sleep(5);

	fclose(stdout);
	stdout = fp;

}

bool SmokeTest::createTestFilePath(std::string &filePath)
{
	getFilePath(filePath);
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

void SmokeTest::getTuneDetails()
{
	readVodData("Dash");
	readVodData("Hls");
	readLiveData("Live");
}

bool SmokeTest::getFilePath(std::string &filePath)
{
#ifdef SIMULATOR_BUILD
	char *ptr = getenv("HOME");
	if(ptr)
	{
		filePath.append(ptr); 
		filePath += "/smoketest/";
	}
	else
	{
		printf("%s:%d: Path not found \n",__FUNCTION__,__LINE__);
		return false;
	}
#else
	filePath = "/opt/smoketest/";
#endif
	return true;
}

bool SmokeTest::readVodData(const char *stream)
{
	int BUFFER_SIZE = 500;
	char buffer[BUFFER_SIZE];
	char *token = NULL;
	FILE *fp;
	std::string fileName;

	if(getFilePath(fileName))
	{
		if(strncmp(stream,"Dash",4) == 0)
			fileName = fileName + "tuneDashStream.txt";
		else if(strncmp(stream,"Hls",3) == 0)
			fileName = fileName + "tuneHlsStream.txt";

		if( fp = fopen(fileName.c_str(), "r"))
		{
			while(!feof(fp))
			{
				if(fgets(buffer, BUFFER_SIZE, fp) != NULL)
				{

					if (strstr(buffer, "IP_AAMP_TUNETIME:") != NULL)
					{
						token = strstr(buffer, "IP_AAMP_TUNETIME:");
						token[strcspn(token, "\n")] = 0;

						std::stringstream ss(token+17);
						std::string data;

						while(std::getline(ss, data, ',')) 
						{
							if(strncmp(stream,"Dash",4) == 0)
							{
								SmokeTest::dashTuneData.push_back(data);

							}
							else if(strncmp(stream,"Hls",3) == 0)
							{
								SmokeTest::hlsTuneData.push_back(data);

							}
						}
					}

					if (strstr(buffer, "AAMP_EVENT_STATE_CHANGED: PAUSED (6)") != NULL)
					{
						if(strncmp(stream,"Dash",4) == 0)
							dashPauseState = true;
						else if(strncmp(stream,"Hls",3) == 0)
							hlsPauseState = true;
					}

					if (strstr(buffer, "AAMP_EVENT_SPEED_CHANGED current rate=4") != NULL)
					{
						if(strncmp(stream,"Dash",4) == 0)
							dashFastForwardState = true;
						else if(strncmp(stream,"Hls",3) == 0)
							hlsFastForwardState = true;
					}

					if (strstr(buffer, "AAMP_EVENT_SPEED_CHANGED current rate=1") != NULL)
					{
						if(strncmp(stream,"Dash",4) == 0)
							dashPlayState = true;
						else if(strncmp(stream,"Hls",3) == 0)
							hlsPlayState = true;
					}

					if (strstr(buffer, "AAMP_EVENT_SPEED_CHANGED current rate=-2") != NULL)
					{
						if(strncmp(stream,"Dash",4) == 0)
							dashRewindState = true;
						else if(strncmp(stream,"Hls",3) == 0)
							hlsRewindState = true;

						break;
					}

				} 
			}

			fclose(fp);
			return true;
		}
		else
		{
			printf("%s:%d: %s file not found \n",__FUNCTION__,__LINE__,stream);
			return false;
		}
	}
	return false;
}

bool SmokeTest::readLiveData(const char *stream)
{
	int BUFFER_SIZE = 500;
	char buffer[BUFFER_SIZE];
	char *token = NULL;
	FILE *fp;
	std::string fileName;

	if(getFilePath(fileName))
	{
		fileName = fileName + "tuneLive.txt";

		if(fp = fopen(fileName.c_str(), "r"))
		{
			while(!feof(fp))
			{
				if(fgets(buffer, BUFFER_SIZE, fp) != NULL)
				{
					if (strstr(buffer, "IP_TUNETIME:") != NULL)
					{
						token = strstr(buffer, "IP_TUNETIME:");
						token[strcspn(token, "\n")] = 0;

						std::stringstream ss(token+12);
						std::string data;

						while(std::getline(ss, data, ',')) 
						{
							SmokeTest::liveTuneData.push_back(data);

						}
					}

					if (strstr(buffer, "AAMP_EVENT_STATE_CHANGED: PAUSED (6)") != NULL)
					{
						livePauseState = true;
					}

					if (strstr(buffer, "AAMP_EVENT_SPEED_CHANGED current rate=1") != NULL)
					{
						livePlayState = true;
					}
				} 
			}

			fclose(fp);
			return true;
		}
		else
		{
			printf("%s:%d: %s file not found \n",__FUNCTION__,__LINE__,stream);
			return false;
		}
	}
	return false;
}

/* To test the manifest download start time is less than 7ms(expected)*/
TEST(dashTuneTest, ManifestDLStartTime)	
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_LE(stoi(SmokeTest::dashTuneData[3]),7);
}      

/* To test the manifest download end time is less than 1s(expected)*/
TEST(dashTuneTest, ManifestDLEndTime)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_LE(stoi(SmokeTest::dashTuneData[4]),1000);
}

/* To test the manifest download fail count is zero*/
TEST(dashTuneTest, ManifestDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[5]),0);
}

/* To test the video playlist download fail count is zero*/
TEST(dashTuneTest, VideoPlaylistDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[8]),0);
}

/* To test the audio playlist download fail count is zero*/
TEST(dashTuneTest, AudioPlaylistDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[11]),0);
}

/* To test the video init download fail count is zero*/
TEST(dashTuneTest, VideoInitDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[14]),0);
}

/* To test the audio init download fail count is zero*/
TEST(dashTuneTest, AudioInitDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[17]),0);
}

/* To test the video fragment download fail count is zero*/
TEST(dashTuneTest, VideoFragmentDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[20]),0);
}

/* To test the audio fragment download fail count is zero*/
TEST(dashTuneTest, AudioFragmentDLFailCount)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[24]),0);
}

/* To test the DRM fail error code  is zero*/
TEST(dashTuneTest, DrmFailErrorCode)
{
	if(!SmokeTest::dashTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::dashTuneData[28]),0);
}

/* To test the pause state is true*/
TEST(dashTuneTest, PauseState)
{
	EXPECT_EQ(SmokeTest::dashPauseState,true);
}

/* To test the fast forward state is true*/
TEST(dashTuneTest, FastForwardState)
{
	EXPECT_EQ(SmokeTest::dashFastForwardState,true);
}

/* To test the play state is true*/
TEST(dashTuneTest, PlayState)
{
	EXPECT_EQ(SmokeTest::dashPlayState,true);
}

/* To test the rewind state is true*/
TEST(dashTuneTest, RewindState)
{
	EXPECT_EQ(SmokeTest::dashRewindState,true);
}

/* To test the manifest download start time is less than 7ms(expected)*/
TEST(hlsTuneTest, ManifestDLStartTime)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_LE(stoi(SmokeTest::hlsTuneData[3]),7);
}      

/* To test the manifest download end time is less than 1s(expected)*/
TEST(hlsTuneTest, ManifestDLEndTime)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_LE(stoi(SmokeTest::hlsTuneData[4]),1000);
}

/* To test the manifest download fail count is zero*/
TEST(hlsTuneTest, ManifestDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[5]),0);
}

/* To test the video playlist download fail count is zero*/
TEST(hlsTuneTest, VideoPlaylistDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[8]),0);
}

/* To test the audio playlist download fail count is zero*/
TEST(hlsTuneTest, AudioPlaylistDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[11]),0);
}

/* To test the video init download fail count is zero*/
TEST(hlsTuneTest, VideoInitDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[14]),0);
}

/* To test the audio init download fail count is zero*/
TEST(hlsTuneTest, AudioInitDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[17]),0);
}

/* To test the video fragment download fail count is zero*/
TEST(hlsTuneTest, VideoFragmentDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[20]),0);
}

/* To test the audio fragment download fail count is zero*/
TEST(hlsTuneTest, AudioFragmentDLFailCount)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[24]),0);
}

/* To test the DRM fail error code  is zero*/
TEST(hlsTuneTest, DrmFailErrorCode)
{
	if(!SmokeTest::hlsTuneData.empty())
		EXPECT_EQ(stoi(SmokeTest::hlsTuneData[28]),0);
}

/* To test the pause state is true*/
TEST(hlsTuneTest, PauseState)
{
	EXPECT_EQ(SmokeTest::hlsPauseState,true);
}

/* To test the fast forward state is true*/
TEST(hlsTuneTest, FastForwardState)
{
	EXPECT_EQ(SmokeTest::hlsFastForwardState,true);
}

/* To test the play state is true*/
TEST(hlsTuneTest, PlayState)
{
	EXPECT_EQ(SmokeTest::hlsPlayState,true);
}

/* To test the rewind state is true*/
TEST(hlsTuneTest, RewindState)
{
	EXPECT_EQ(SmokeTest::hlsRewindState,true);
}

/* To test live pause state is true*/
TEST(liveTuneTest, PauseState)
{
	EXPECT_EQ(SmokeTest::livePauseState,true);
}

/* To test live play state is true*/
TEST(liveTuneTest, PlayState)
{
	EXPECT_EQ(SmokeTest::livePlayState,true);
}

