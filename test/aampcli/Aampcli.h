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
 * @file aampcli.h
 * @brief AAMPcli header file
 */

#ifndef AAMPCLI_H
#define AAMPCLI_H

#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <list>
#include <sstream>
#include <string>
#include <ctype.h>
#include <gst/gst.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <priv_aamp.h>
#include <main_aamp.h>
#include "AampConfig.h"
#include "AampDefine.h"
#include "StreamAbstractionAAMP.h"
#include "AampcliCommandHandler.h"
#include "AampcliVirtualChannelMap.h"
#include "AampcliGet.h"
#include "AampcliSet.h"
#include "AampcliShader.h"
#include "AampcliHarvestor.h"

#ifdef __APPLE__
#import <cocoa_window.h>
#endif

class MyAAMPEventListener : public AAMPEventObjectListener
{
	public:
		const char *stringifyPrivAAMPState(PrivAAMPState state);
		void Event(const AAMPEventPtr& e);
};

class Aampcli
{
	public:
		bool mInitialized;
		bool mEnableProgressLog;
		bool mbAutoPlay;
		static const int mMaxBufferLength = 4096;
		std::string mTuneFailureDescription;
		PlayerInstanceAAMP *mSingleton;
		MyAAMPEventListener *mEventListener;
		GMainLoop *mAampGstPlayerMainLoop;
		GThread *mAampMainLoopThread;
		std::vector<PlayerInstanceAAMP*> mPlayerInstances;

		static void * runCommand( void* args );
		static void * aampGstPlayerStreamThread(void *arg);
		void doAutomation( int startChannel, int stopChannel, int maxTuneTimeS, int playTimeS, int betweenTimeS );
		FILE * getConfigFile(const std::string& cfgFile);
		void initPlayerLoop(int argc, char **argv);
		void newPlayerInstance( void );
		Aampcli();
		Aampcli(const Aampcli& aampcli);
		Aampcli& operator=(const Aampcli& aampcli);
		~Aampcli();
};

#endif // AAMPCLI_H
