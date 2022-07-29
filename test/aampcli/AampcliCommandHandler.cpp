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
 * @file AampcliCommandHandler.cpp
 * @brief Aampcli Command register and handler
 */

#include "AampcliCommandHandler.h"
#include "AampcliGet.h"
#include "AampcliSet.h"
#include "AampcliPlaybackCommand.h"
#include "AampcliSmokeTest.h"

std::map<std::string, Command*> CommandHandler::mCommandMap = std::map<std::string, Command*>();

void CommandHandler::registerAampcliCommands()
{
	registerAllCommands();
	registerCommandObjects();
}

void CommandHandler::registerCommandObjects()
{
	registerCommand( "set", new Set);
	registerCommand( "get", new Get);
	registerCommand( "harvest", new Harvestor);
	registerCommand( "smoketest", new SmokeTest);
	registerCommand( "default", new PlaybackCommand);
}

void CommandHandler::registerCommand(const std::string& commandName, Command* command)
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

bool CommandHandler::dispatchAampcliCommands(char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	bool l_status = true;
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
		l_status = l_Command->execute(cmd,playerInstanceAamp);

	}
	
	return l_status;
}

void CommandHandler::registerAllCommands() 
{
	PlaybackCommand lPlaybackCommand;
	Set lSet;
	Get lGet;

	lPlaybackCommand.registerPlaybackCommands();
	lGet.registerGetCommands();
	lSet.registerSetCommands();

}

char ** CommandHandler::commandCompletion(const char *text, int start, int end)
{
	PlaybackCommand lPlaybackCommand;
	Set lSet;
	Get lGet;

	char *buffer = rl_line_buffer;

    rl_attempted_completion_over = 1;

    if(strncmp(buffer, "get", 3) == 0)
    {
    	return rl_completion_matches(text, lGet.getCommandRecommender);
    }
    else if (strncmp(buffer, "set", 3) == 0)
    {
    	return rl_completion_matches(text, lSet.setCommandRecommender);
    }
    else
    {
    	return rl_completion_matches(text, lPlaybackCommand.commandRecommender);
    }

}

