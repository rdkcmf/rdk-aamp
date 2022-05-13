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
 * @file AampcliVirtualChannel.h
 * @brief AampcliVirtualChannel header file.
 */

#ifndef AAMPCLIVIRTUALCHANNELMAP_H
#define AAMPCLIVIRTUALCHANNELMAP_H

#include <stdlib.h>
#include <sstream>
#include <priv_aamp.h>

#define VIRTUAL_CHANNEL_VALID(x) ((x) > 0)
#define MAX_BUFFER_LENGTH 4096

/**
 * @struct VirtualChannelInfo
 * @brief Holds information of a virtual channel
 */
struct VirtualChannelInfo
{
	VirtualChannelInfo() : channelNumber(0), name(), uri()
	{
	}
	
	int channelNumber;
	std::string name;
	std::string uri;
};

/**
 * @class VirtualChannelMap
 * @brief Holds all of the virtual channels
 */
class VirtualChannelMap
{
	public:
		VirtualChannelMap() : mVirtualChannelMap(),mCurrentlyTunedChannel(0),mAutoChannelNumber(0) {}
		~VirtualChannelMap()
		{
			mVirtualChannelMap.clear();
		}
		
		VirtualChannelInfo* find(const int channelNumber);
		VirtualChannelInfo* prev();
		VirtualChannelInfo* next();
		void add(VirtualChannelInfo& channelInfo);
		bool isPresent(const VirtualChannelInfo& channelInfo);
		void print();
		void setCurrentlyTunedChannel(int value);
		void showList(void);
		void tuneToChannel( VirtualChannelInfo &channel, PlayerInstanceAAMP *playerInstanceAamp);
		std::string getNextFieldFromCSV( const char **pptr );
		void loadVirtualChannelMapFromCSV( FILE *f );
		void loadVirtualChannelMapLegacyFormat( FILE *f );
		const char *skipwhitespace( const char *ptr );

	protected:
		std::list<VirtualChannelInfo> mVirtualChannelMap;
		int mAutoChannelNumber;
		int mCurrentlyTunedChannel;
};

#endif // AAMPCLIVIRTUALCHANNEL_H
