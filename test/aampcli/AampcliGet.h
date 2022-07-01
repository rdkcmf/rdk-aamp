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

#include <string>
#include <priv_aamp.h>
#include "AampcliProcessCommand.h"

/**
 * @enum AAMPGetTypes
 * @brief Define the enum values of get types
 */
typedef enum {
	eAAMP_GET_CurrentState,
	eAAMP_GET_CurrentAudioLan,
	eAAMP_GET_CurrentDrm,
	eAAMP_GET_PlaybackPosition,
	eAAMP_GET_PlaybackDuration,
	eAAMP_GET_VideoBitrate,
	eAAMP_GET_InitialBitrate,
	eAAMP_GET_InitialBitrate4k,
	eAAMP_GET_MinimumBitrate,
	eAAMP_GET_MaximumBitrate,
	eAAMP_GET_AudioBitrate,
	eAAMP_GET_VideoZoom,
	eAAMP_GET_VideoMute,
	eAAMP_GET_AudioVolume,
	eAAMP_GET_PlaybackRate,
	eAAMP_GET_VideoBitrates,
	eAAMP_GET_AudioBitrates,
	eAAMP_GET_CurrentPreferredLanguages,
	eAAMP_GET_RampDownLimit,
	eAAMP_GET_AvailableAudioTracks,
	eAAMP_GET_AllAvailableAudioTracks,
	eAAMP_GET_AvailableTextTracks,
	eAAMP_GET_AllAvailableTextTracks,
	eAAMP_GET_AudioTrack,
	eAAMP_GET_InitialBufferDuration,
	eAAMP_GET_AudioTrackInfo,
	eAAMP_GET_TextTrackInfo,
	eAAMP_GET_PreferredAudioProperties,
	eAAMP_GET_PreferredTextProperties,
	eAAMP_GET_CCStatus,
	eAAMP_GET_TextTrack,
	eAAMP_GET_ThumbnailConfig,
	eAAMP_GET_ThumbnailData,
	eAAMP_GET_AvailableVideoTracks,
	eAAMP_GET_TYPE_COUNT
}AAMPGetTypes;

class Get : public Command {

	public:
		void initGetHelpText();
		void showHelpGet();
		bool execute(char *cmd, PlayerInstanceAAMP *playerInstanceAamp);
};

#endif // AAMPCLIGET_H
