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
 * @file AampcliProcessCommand.cpp
 * @brief Aampcli Command register and handler
 */

#include "AampcliProcessCommand.h"
#include "AampcliGet.h"
#include "AampcliSet.h"
#include "AampcliPlaybackCommand.h"

CommandDispatcher mCommandDispatcher;

CommandDispatcher::CommandDispatcher() : mCommandMap(std::map<std::string, Command*>())
{}

void CommandDispatcher::registerAampcliCommands()
{
	registerCommand( "set", new Set);
	registerCommand( "get", new Get);
	registerCommand( "default", new PlaybackCommand);
}

void CommandDispatcher::registerCommand(const std::string& commandName, Command* command)
{
	std::map<std::string, Command*>::iterator cmdPair = mCommandMap.find(commandName);
	if (cmdPair != mCommandMap.end())
	{
		printf("%s:%d: Command already registered\n", __FUNCTION__, __LINE__);
	}
	else
	{
		mCommandMap[commandName] = command;
	}
}

bool CommandDispatcher::dispatchAampcliCommands(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char l_cmd[4096] = {'\0'};
	strcpy(l_cmd,cmd);
	char *token = strtok(l_cmd," ");

	if (token != NULL)
	{
		std::map<std::string, Command*>::iterator cmdPair;
		cmdPair = mCommandMap.find(token);

		if(cmdPair == mCommandMap.end())
		{	
			cmdPair = mCommandMap.find("default");
		}

		Command* l_Command = cmdPair->second;
		l_Command->execute(cmd,playerInstanceAamp);

		return true;
	}
}

