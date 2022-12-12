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
 * @file AampcliHarvestor.h
 * @brief AampcliHarvestor header file
 */

#ifndef AAMPCLIHARVESTOR_H
#define AAMPCLIHARVESTOR_H

#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include "AampcliCommandHandler.h"

#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

#ifdef __linux__
    #if defined(__sun)
        #define PROC_SELF_EXE "/proc/self/path/a.out"
    #else
        #define PROC_SELF_EXE "/proc/self/exe"
    #endif
#endif

typedef struct harvestProfileDetails
{
	bool harvestEndFlag;
	char media[7];
	long harvestConfig;
	long harvestFragmentsCount;
	int harvesterrorCount;
	int harvestFailureCount;
	long bitrate;
}HarvestProfileDetails;

class Harvestor : public Command 
{

	public:
		static bool mHarvestReportFlag;
		static const int mHarvestCommandLength = 4096;
		static const int mHarvestSlaveThreadCount = 50;
		static char exePathName[PATH_MAX];
		static std::string mHarvestPath;
		static std::map<pthread_t, harvestProfileDetails> mHarvestInfo;
		static std::vector<pthread_t> mHarvestThreadId;
		static PlayerInstanceAAMP *mPlayerInstanceAamp;

		pthread_t mMasterHarvestorThreadID;
		pthread_t mSlaveHarvestorThreadID;
		pthread_t mReportThread;
		pthread_t mSlaveIFrameThread;
		pthread_t mSlaveVideoThreads[mHarvestSlaveThreadCount];
		pthread_t mSlaveAudioThreads[mHarvestSlaveThreadCount];

		static void* masterHarvestor(void * arg);
		static void* slaveHarvestor(void * arg);
		static void* slaveDataOutput(void * arg);
		long getNumberFromString(std::string buffer);
		static void* startHarvestReport(void * arg);
		bool getHarvestReportDetails(char *buffer);
		void writeHarvestErrorReport(HarvestProfileDetails, char *buffer);
		void writeHarvestEndReport(HarvestProfileDetails, char *buffer);
		static void harvestTerminateHandler(int signal);
		void getExecutablePath();
		bool execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp);
		Harvestor();
};


#endif // AAMPCLIHARVESTOR_H
