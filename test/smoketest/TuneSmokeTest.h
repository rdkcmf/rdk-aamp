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
 * @file TuneSmokeTest.h
 * @brief TuneSmokeTest header file
 */

#ifndef TUNESMOKETEST_H
#define TUNESMOKETEST_H

#include "gtest/gtest.h"
#include "priv_aamp.h"

class SmokeTest
{
	public:
		static bool dashPauseState;
		static bool dashFastForwardState;
		static bool dashPlayState;
		static bool dashRewindState;
	        static bool hlsPauseState;	
		static bool hlsFastForwardState;
		static bool hlsPlayState;
		static bool hlsRewindState;
		static bool livePlayState;
		static bool livePauseState;
		static std::vector<std::string> dashTuneData;
		static std::vector<std::string> hlsTuneData;
		static std::vector<std::string> liveTuneData;

	static std::map<std::string, std::string> smokeTestUrls;
		bool aampTune();
		void vodTune(const char *stream);
		void liveTune(const char *stream);
		void loadSmokeTestUrls();
		bool createTestFilePath(std::string &filePath);
		bool readVodData(const char *stream);
		bool readLiveData(const char *stream);
		void getTuneDetails();
		bool getFilePath(std::string &filePath);
};

#endif // TUNESMOKETEST_H
