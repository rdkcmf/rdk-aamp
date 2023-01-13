/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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
* @file mediaprocessor.h
* @brief Header file for base class of media container processor
*/

#ifndef __MEDIA_PROCESSOR_H__
#define __MEDIA_PROCESSOR_H__

#include <stddef.h>

/**
 * @enum _PlayMode
 * @brief Defines the parameters required for Recording Playback
 */
typedef enum _PlayMode
{
	PlayMode_normal,		/**< Playing a recording in normal mode */
	PlayMode_retimestamp_IPB,	/**< Playing with I-Frame, P-Frame and B-Frame */
	PlayMode_retimestamp_IandP,	/**< Playing with I-Frame and P-Frame */
	PlayMode_retimestamp_Ionly,	/**< Playing a recording with I-Frame only */
	PlayMode_reverse_GOP,		/**< Playing a recording with rewind mode */
} PlayMode;

/**
 * @class MediaProcessor
 * @brief Base Class for Media Container Processor
 */
class MediaProcessor
{
public:
	/**
	 * @brief MediaProcessor constructor
	 */
	MediaProcessor()
	{

	}

	/**
	 * @brief MediaProcessor destructor
	 */
	virtual ~MediaProcessor()
	{

	}

	MediaProcessor(const MediaProcessor&) = delete;
	MediaProcessor& operator=(const MediaProcessor&) = delete;

	/**
	 * @fn sendSegment
	 *
	 * @param[in] segment - fragment buffer pointer
	 * @param[in] size - fragment buffer size
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return true if fragment was sent, false otherwise
	 */
	virtual bool sendSegment( char *segment, size_t& size, double position, double duration, bool discontinuous, bool &ptsError) = 0;

	/**
	 * @brief Set playback rate
	 *
	 * @param[in] rate - playback rate
	 * @param[in] mode - playback mode
	 * @return void
	 */
	virtual void setRate(double rate, PlayMode mode) = 0;

	/**
	 * @brief Enable or disable throttle
	 *
	 * @param[in] enable - throttle enable/disable
	 * @return void
	 */
	virtual void setThrottleEnable(bool enable) = 0;

	/**
	 * @brief Set frame rate for trickmode
	 *
	 * @param[in] frameRate - rate per second
	 * @return void
	 */
	virtual void setFrameRateForTM (int frameRate) = 0;

	/**
	 * @brief Abort all operations
	 *
	 * @return void
	 */
	virtual void abort() = 0;

	/**
	 * @brief Reset all variables
	 *
	 * @return void
	 */
	virtual void reset() = 0;

	/**
	* @fn Change Muxed Audio Track
	* @param[in] AudioTrackIndex
	*/
	virtual void ChangeMuxedAudioTrack(unsigned char index){};

	/**
	* @brief Function to set the group-ID
	* @param[in] string - id
	*/
	virtual void SetAudioGroupId(std::string& id){};

	/**
        * @brief Function to set a offsetflag. if the value is fasle, no need to apply offset while doing pts restamping
        * @param[in] bool - true/false
        */
	virtual void setApplyOffsetFlag(bool enable){};
};
#endif /* __MEDIA_PROCESSOR_H__ */
