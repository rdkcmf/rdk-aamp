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
 * @file AampcliPlaybackCommand.h
 * @brief AAMPcliPlaybackCommand header file
 */

#ifndef AAMPCLIPLAYBACKCOMMAND_H
#define AAMPCLIPLAYBACKCOMMAND_H

#include "Aampcli.h"
#include "AampcliCommandHandler.h"


class PlaybackCommand : public Command
{

	public:
		static std::vector<std::string> commands;
		static std::map<string,string> playbackCommands;
		void addCommand(string command,string description);
		void registerPlaybackCommands();
		static char *commandRecommender(const char *text, int state);
		bool isNumber(const char *s);
		void showHelp(void);
		bool isTuneScheme(const char *cmd);
		void trim(char **cmd);
		void termPlayerLoop();
		bool execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp);

};

#endif // AAMPCLIPLAYBACKCOMMAND_H
