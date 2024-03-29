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
 * @file AampcliGet.h
 * @brief AampcliGet header file
 */

#ifndef AAMPCLIGET_H
#define AAMPCLIGET_H

#include <cstring>
#include <priv_aamp.h>
#include "AampcliCommandHandler.h"

class Get : public Command {

	public:
		static std::vector<std::string> commands;
		static std::vector<std::string> numCommands;
		static std::map<string,string> getCommands;
		static std::map<string,string> getNumCommands;
		void addCommand(string command,string description);
		void registerGetCommands();
		static char *getCommandRecommender(const char *text, int state);
		void ShowHelpGet();
		bool execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp);
		void registerGetNumCommands();
		void addNumCommand(string command,string description);
		void ShowHelpGetNum();
};

#endif // AAMPCLIGET_H
