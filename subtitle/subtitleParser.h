/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file  subtitleParser.h
 * 
 * @brief This file provides interfaces for a subtitle parser in AAMP
 *
 */

#ifndef __SUBTITLE_PARSER_H__
#define __SUBTITLE_PARSER_H__

#include "priv_aamp.h"


/**
* \enum       SubtitleMimeType
* \brief      Subtitle data types
*/
typedef enum
{
	eSUB_TYPE_WEBVTT,
	eSUB_TYPE_MP4,
	eSUB_TYPE_TTML,
	eSUB_TYPE_UNKNOWN
} SubtitleMimeType;


/**
* \class       SubtitleParser
* \brief       Subtitle parser class
*
* This is the base class for a subtitle parser impl in AAMP
*/
class SubtitleParser
{
public:

	SubtitleParser(AampLogManager* logObj, PrivateInstanceAAMP* aamp, SubtitleMimeType type) : mLogObj(logObj), mAamp(aamp),
		mType(type)
	{
	}

	/// Copy Constructor
	SubtitleParser(const SubtitleParser&) = delete;

	virtual ~SubtitleParser()
	{
	}

	/// Assignment operator Overloading
	SubtitleParser& operator=(const SubtitleParser&) = delete;

	virtual bool init(double startPosSeconds, unsigned long long basePTS) { return false; }
	virtual bool processData(char* buffer, size_t bufferLen, double position, double duration) = 0;
	virtual bool close() = 0;
	virtual void reset() = 0;
	virtual void setProgressEventOffset(double offset) = 0;
	virtual void updateTimestamp(unsigned long long positionMs) = 0;
	virtual void pause(bool pause) {}
	virtual void mute(bool mute) {}
	virtual void isLinear(bool isLinear) {}
	virtual void setTextStyle(const std::string &options){}
protected:

	SubtitleMimeType mType;
	PrivateInstanceAAMP* mAamp;
	AampLogManager* mLogObj;
};

#endif /* __SUBTITLE_PARSER_H__ */
