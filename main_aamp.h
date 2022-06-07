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
 * @file main_aamp.h
 * @brief Types and APIs exposed by the AAMP player.
 */

/**
 @mainpage Advanced Adaptive Micro Player (AAMP)

 <b>AAMP</b> is a native video engine build on top of gstreamer, optimized for
 performance, memory use, and code size.
 <br><b>AAMP</b> downloads and parses HLS/DASH manifests. <b>AAMP</b> has been
 integrated with Adobe Access, PlayReady, CONSEC agnostic, and Widevine DRM

 <b>AAMP</b> is fronted by JS PP (JavaScript Player Platform), which provides an
 additional layer of functionality including player analytics, configuration
 management, and ad insertion.
 */

#ifndef MAINAAMP_H
#define MAINAAMP_H

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <string.h>
#include <mutex>
#include <stddef.h>
#include <functional>
#include "AampEvent.h"
#include "AampEventListener.h"

#include "AampDrmSystems.h"
#include "AampMediaType.h"
#include "AampScheduler.h"
#include "AampConfig.h"

/*! \mainpage
 *
 * \section intro_sec Introduction
 *
 * See PlayerInstanceAAMP for libaamp public C++ API's
 *
 */

/**
 * @enum AAMPAnomalyMessageType
 * @brief AAMP anomaly message types
 */
typedef enum
{
	ANOMALY_ERROR,    /**< Error Message */
	ANOMALY_WARNING,  /**< Warning Message */
	ANOMALY_TRACE	  /**< Trace Message */
} AAMPAnomalyMessageType;

/**
 * @struct TuneFailureMap
 * @brief  Structure holding aamp tune failure code and corresponding application error code and description
 */
struct TuneFailureMap
{
	AAMPTuneFailure tuneFailure;    /**< Failure ID */
	int code;                       /**< Error code */
	const char* description;        /**< Textual description */
};


/**
 * @enum MediaTypeTelemetry
 * @brief Media types for telemetry
 */
enum MediaTypeTelemetry
{
	eMEDIATYPE_TELEMETRY_AVS,               /**< Type audio, video or subtitle */
	eMEDIATYPE_TELEMETRY_DRM,               /**< Type DRM license */
	eMEDIATYPE_TELEMETRY_INIT,              /**< Type audio or video init fragment */
	eMEDIATYPE_TELEMETRY_MANIFEST,          /**< Type main or sub manifest file */
	eMEDIATYPE_TELEMETRY_UNKNOWN,           /**< Type unknown*/
};


/**
 * @enum StreamOutputFormat
 * @brief Media output format
 */
enum StreamOutputFormat
{
	FORMAT_INVALID,         /**< Invalid format */
	FORMAT_MPEGTS,          /**< MPEG Transport Stream */
	FORMAT_ISO_BMFF,        /**< ISO Base Media File format */
	FORMAT_AUDIO_ES_AAC,    /**< AAC Audio Elementary Stream */
	FORMAT_AUDIO_ES_AC3,    /**< AC3 Audio Elementary Stream */
	FORMAT_AUDIO_ES_EC3,    /**< Dolby Digital Plus Elementary Stream */
	FORMAT_AUDIO_ES_ATMOS,  /**< ATMOS Audio stream */
	FORMAT_AUDIO_ES_AC4,    /**< AC4 Dolby Audio stream */
	FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
	FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
	FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
	FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
	FORMAT_UNKNOWN          /**< Unknown Format */
};

/**
 * @enum VideoZoomMode
 * @brief Video zoom mode
 */
enum VideoZoomMode
{
	VIDEO_ZOOM_FULL,    /**< Video Zoom Enabled */
	VIDEO_ZOOM_NONE     /**< Video Zoom Disabled */
};


using AdObject = std::pair<std::string, std::string>;

/**
 * @enum AuthTokenErrors
 *  @brief Auth Token Failure codes
 */
enum AuthTokenErrors {
	eAUTHTOKEN_TOKEN_PARSE_ERROR = -1,    /**< Auth token parse Error */
	eAUTHTOKEN_INVALID_STATUS_CODE = -2   /**< Auth token Invalid status */
};

/**
 * @struct PreCacheUrlData
 * @brief Pre cache the data information
 */
typedef struct PreCacheUrlData
{
	std::string url;
	MediaType type;
	PreCacheUrlData():url(""),type(eMEDIATYPE_VIDEO)
	{
	}
} PreCacheUrlStruct;

typedef std::vector < PreCacheUrlStruct> PreCacheUrlList;

/**
 *  @brief Language Code Preference types
 */
typedef enum
{
    ISO639_NO_LANGCODE_PREFERENCE,
    ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE,
    ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE,
    ISO639_PREFER_2_CHAR_LANGCODE
} LangCodePreference;

/**
 * @class Accessibility
 * @brief Data type to stro Accessibility Node data
 */
class Accessibility 
{
	std::string strSchemeId;
    int intValue;
	std::string strValue;
	std::string valueType;
	
	bool isNumber(const std::string& str)
	{
	    std::string::const_iterator it = str.begin();
		while (it != str.end() && std::isdigit(*it)) ++it;
		return !str.empty() && it == str.end();
	};

  public:
	Accessibility(std::string schemId, std::string val): strSchemeId(schemId), intValue(-1), strValue(""), valueType("") 
	{
		if (isNumber(val))
		{
			valueType = "int_value";
			intValue = std::stoi(val);
			strValue = "";
		}
		else
		{
			valueType = "string_value";
			intValue = -1;
			strValue = val;
		}

	};

	Accessibility():strSchemeId(""), intValue(-1), strValue(""), valueType("") {};

	void setAccessibilityData(std::string schemId, std::string val)
	{
		strSchemeId = schemId;
		if (isNumber(val))
		{
			valueType = "int_value";
			intValue = std::stoi(val);
			strValue = "";
		}
		else
		{
			valueType = "string_value";
			intValue = -1;
			strValue = val;
		}
	};

	void setAccessibilityData(std::string schemId, int val)
	{
		strSchemeId = schemId;
		valueType = "int_value";
		intValue = val;
		strValue = "";
	};

	std::string& getTypeName() {return valueType;};
	std::string& getSchemeId() {return strSchemeId;};
	int getIntValue() {return intValue;};
	std::string& getStrValue() {return strValue;};
	void clear()
	{
		strSchemeId = "";
		intValue = -1;
		strValue = "";
		valueType = "";
	};

	bool operator == (const Accessibility& track) const
	{
		return ((strSchemeId == track.strSchemeId) &&	
			(valueType == "int_value"?(intValue == track.intValue):(strValue == track.strValue)));	
	};
	
	Accessibility& operator = (const Accessibility& track)
	{
		strSchemeId = track.strSchemeId;
		intValue = track.intValue;
		strValue = track.strValue;
		valueType = track.valueType;
		
		return *this;
	};

	std::string print()
	{
		char strData [228];
		std::string retVal = "";
		if (!strSchemeId.empty())
		{
			std::snprintf(strData, sizeof(strData), "{ scheme:%s, %s:", strSchemeId.c_str(), valueType.c_str());
			retVal += strData;
			if (valueType == "int_value")
			{
				std::snprintf(strData, sizeof(strData), "%d }", intValue);
			}else
			{
				std::snprintf(strData, sizeof(strData), "%s }", strValue.c_str());
			}
			retVal += strData;
		}
		else
		{
			retVal = "NULL";
		}
		return retVal;
	};
};

/**
 * @struct AudioTrackInfo
 * @brief Structure for audio track information
 *        Holds information about an audio track in playlist
 */
struct AudioTrackInfo
{
	std::string index;			/**< Index of track */
	std::string language; 			/**< Language of track */
	std::string rendition;			/**< role for DASH, group-id for HLS */
	std::string name;			/**< Name of track info */
	std::string codec;			/**< Codec of Audio track */
	std::string characteristics;		/**< Charesterics field of audio track */
	std::string label;			/**< label of audio track info */
	int channels;				/**< number channels of track */
	long bandwidth;				/**< Bandwidth value of track **/
	int primaryKey; 			/**< used for ATSC to store key , this should not be exposed to app */
	std::string contentType; 		/**< used for ATSC to propogate content type */
	std::string mixType; 			/**< used for ATSC to propogate mix type */
	std::string accessibilityType; 	 	/**< value of Accessibility */
	bool isMuxed; 				/**< Flag to indicated muxed audio track ; this is used by AC4 tracks */
	Accessibility accessibilityItem; 	/**< Field to store Accessibility Node */
	std::string mType;			/**< Type field of track, to be populated by player */
	bool isAvailable;

	AudioTrackInfo() : index(), language(), rendition(), name(), codec(), characteristics(), channels(0), 
	bandwidth(0),primaryKey(0), contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(), isAvailable(true) 
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, std::string cha, int ch):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(cha), channels(ch), bandwidth(-1), primaryKey(0) , contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
		isAvailable(true)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, int pk, std::string conType, std::string mixType):
			index(idx), language(lang), rendition(rend), name(trackName),
			codec(codecStr), characteristics(), channels(0), bandwidth(-1), primaryKey(pk),
                        contentType(conType), mixType(mixType), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
			isAvailable(true)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, std::string typ, bool available):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(typ), isMuxed(false), label(), mType(), accessibilityItem(),
		isAvailable(true)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, int channel):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(), channels(channel), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
		isAvailable(true)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, int channel, bool muxed, bool available):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(), channels(channel), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(), isMuxed(muxed), label(), mType(), accessibilityItem(),
		isAvailable(available)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, std::string typ, bool muxed, std::string lab, std::string type, bool available):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(typ), isMuxed(muxed), label(lab), mType(type), accessibilityItem(),
		isAvailable(available)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, std::string typ, bool muxed, std::string lab, std::string type, Accessibility accessbility, bool available):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(typ), isMuxed(muxed), label(lab), mType(type), accessibilityItem(accessbility),
		isAvailable(available)
	{
	}

	bool operator == (const AudioTrackInfo& track) const
	{
		return ((language == track.language) &&	
			(rendition == track.rendition) &&	
			(contentType == track.contentType) &&	
			(codec == track.codec) &&
			(channels == track.channels) &&
			(bandwidth == track.bandwidth) &&
			(isMuxed == track.isMuxed) &&
			(label == track.label) &&
			(mType == track.mType) &&
			(accessibilityItem == track.accessibilityItem));
	}

	bool operator < (const AudioTrackInfo& track) const
	{
		return (bandwidth < track.bandwidth);
	}

	bool operator > (const AudioTrackInfo& track) const
	{
		return (bandwidth > track.bandwidth);
	}
};

/**
 * @brief Structure for text track information
 *        Holds information about a text track in playlist
 */
struct TextTrackInfo
{
	std::string index;
	std::string language;
	bool isCC;
	std::string rendition; //role for DASH, group-id for HLS
	std::string name;
	std::string instreamId;
	std::string characteristics;
	std::string codec;
	std::string label; //Field for label	
	int primaryKey; // used for ATSC to store key , this should not be exposed to app.
	std::string accessibilityType; //value of Accessibility
	Accessibility accessibilityItem; /**< Field to store Accessibility Node */
	std::string mType;
	bool isAvailable;

	TextTrackInfo() : index(), language(), isCC(false), rendition(), name(), instreamId(), characteristics(), codec(), primaryKey(0), accessibilityType(), label(), mType(), accessibilityItem(),
			  isAvailable(true)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string id, std::string cha):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(id), characteristics(cha),
		codec(), primaryKey(0), accessibilityType(), label(), mType(), accessibilityItem(), isAvailable(true)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string id, std::string cha, int pk):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(id), characteristics(cha),
		codec(), primaryKey(pk), accessibilityType(), label(), mType(), accessibilityItem(), isAvailable(true)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr, std::string cha, std::string typ):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(), characteristics(cha),
		codec(codecStr), primaryKey(0), accessibilityType(typ), label(), mType(), accessibilityItem(), isAvailable(true)
	{
	}
	
	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr, std::string cha, std::string typ, std::string lab, std::string type):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(), characteristics(cha),
		codec(codecStr), primaryKey(0), accessibilityType(typ), label(lab), mType(type), accessibilityItem(), isAvailable(true)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr, std::string cha, std::string typ, std::string lab, std::string type, Accessibility acc, bool available):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(), characteristics(cha),
		codec(codecStr), primaryKey(0), accessibilityType(typ), label(lab), mType(type), accessibilityItem(acc), isAvailable(available)
	{
	}

	void set (std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr, std::string cha, 
			std::string acctyp, std::string lab, std::string type, Accessibility acc)
	{
		index = idx; 
		language = lang;
		isCC = cc;
		rendition = rend;
		name = trackName; 
		characteristics = cha;
		codec = codecStr; 
		accessibilityType = acctyp;
		label = lab; 
		accessibilityItem = acc;
		mType = type;
	}

	bool operator == (const TextTrackInfo& track) const
	{
		return ((language == track.language) &&	
			(isCC == track.isCC) &&	
			(rendition == track.rendition) &&
			(name == track.name) &&
			(characteristics == track.characteristics) &&
			(codec == track.codec) &&
			(accessibilityType == track.accessibilityType) &&
			(label == track.label) &&
			(accessibilityItem == track.accessibilityItem) &&
			(mType == track.mType));
	}

	bool operator < (const TextTrackInfo& track) const
	{
		return (index < track.index);
	}

	bool operator > (const TextTrackInfo& track) const
	{
		return (index > track.index);
	}

};

/**
 * @class StreamSink
 * @brief GStreamer Abstraction class for the implementation of AAMPGstPlayer and gstaamp plugin
 */
class StreamSink
{
public:

	/**
	 *   @brief  Configure output formats
	 *
	 *   @param[in]  format - Video output format.
	 *   @param[in]  audioFormat - Audio output format.
	 *   @param[in]  auxFormat - Aux audio output format.
	 *   @param[in]  bESChangeStatus - Flag to keep force configure the pipeline value
	 *   @param[in]  forwardAudioToAux - Flag denotes if audio buffers have to be forwarded to aux pipeline
         *   @param[in]  setReadyAfterPipelineCreation - Flag denotes if pipeline has to be reset to ready or not
	 *   @return void
	 */
	virtual void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation=false)=0;

	/**
	 *   @brief  API to send audio/video buffer into the sink.
	 *
	 *   @param[in]  mediaType - Type of the media.
	 *   @param[in]  ptr - Pointer to the buffer; caller responsible of freeing memory
	 *   @param[in]  len - Buffer length.
	 *   @param[in]  fpts - Presentation Time Stamp.
	 *   @param[in]  fdts - Decode Time Stamp
	 *   @param[in]  duration - Buffer duration.
	 *   @return void
	 */
	virtual void SendCopy( MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration)= 0;

	/**
	 *   @brief  API to send audio/video buffer into the sink.
	 *
	 *   @param[in]  mediaType - Type of the media.
	 *   @param[in]  buffer - Pointer to the GrowableBuffer; ownership is taken by the sink
	 *   @param[in]  fpts - Presentation Time Stamp.
	 *   @param[in]  fdts - Decode Time Stamp
	 *   @param[in]  duration - Buffer duration.
         *   @param[in]  initFragment - flag for buffer type (init, data)
	 *   @return void
	 */
	virtual void SendTransfer( MediaType mediaType, struct GrowableBuffer* buffer, double fpts, double fdts, double duration, bool initFragment = false)= 0;

	/**
	 *   @brief  Notifies EOS to sink
	 *
	 *   @param[in]  mediaType - Media Type
	 *   @return void
	 */
	virtual void EndOfStreamReached(MediaType mediaType){}

	/**
	 *   @brief Start the stream
	 *
	 *   @return void
	 */
	virtual void Stream(void){}

	/**
	 *   @fn Stop
	 *
	 *   @param[in]  keepLastFrame - Keep the last frame on screen (true/false)
	 *   @return void
	 */
	virtual void Stop(bool keepLastFrame){}

	/**
	 *   @brief Dump the sink status for debugging purpose
	 *
	 *   @return void
	 */
	virtual void DumpStatus(void){}

	/**
	 *   @brief Flush the pipeline
	 *
	 *   @param[in]  position - playback position
	 *   @param[in]  rate - Speed
	 *   @param[in]  shouldTearDown - if pipeline is not in a valid state, tear down pipeline
	 *   @return void
	 */
	virtual void Flush(double position = 0, int rate = AAMP_NORMAL_PLAY_RATE, bool shouldTearDown = true){}

	/**
	 *   @brief Adjust the pipeline
	 *
	 *   @param[in]  position - playback position
	 *   @param[in]  rate - Speed
	 *   @return void
	 */
	virtual bool AdjustPlayBackRate(double position, double rate){ return true; }

	/**
	 *   @brief Enabled or disable playback pause
	 *
	 *   @param[in] pause  Enable/Disable
	 *   @param[in] forceStopGstreamerPreBuffering - true for disabling bufferinprogress
	 *   @return true if content successfully paused
	 */
	virtual bool Pause(bool pause, bool forceStopGstreamerPreBuffering){ return true; }

	/**
	 *   @brief Get playback duration in milliseconds
	 *
	 *   @return duration in ms.
	 */
	virtual long GetDurationMilliseconds(void){ return 0; };

	/**
	 *   @brief Get playback position in milliseconds
	 *
	 *   @return Position in ms.
	 */
	virtual long GetPositionMilliseconds(void){ return 0; };

	/**
	 *   @brief Get Video 90 KHz Video PTS
	 *
	 *   @return video PTS
	 */
	virtual long long GetVideoPTS(void){ return 2; };

	/**
	 *   @brief Get closed caption handle
	 *
	 *   @return Closed caption handle
	 */
	virtual unsigned long getCCDecoderHandle(void) { return 0; };

	/**
	 *   @brief Set video display rectangle co-ordinates
	 *
	 *   @param[in]  x - x position
	 *   @param[in]  y - y position
	 *   @param[in]  w - Width
	 *   @param[in]  h - Height
	 *   @return void
	 */
	virtual void SetVideoRectangle(int x, int y, int w, int h){};

	/**
	 *   @brief Set video zoom state
	 *
	 *   @param[in]  zoom - Zoom mode
	 *   @return void
	 */
	virtual void SetVideoZoom(VideoZoomMode zoom){};

	/**
	 *   @brief Set video mute state
	 *
	 *   @param[in] muted - true: video muted, false: video unmuted
	 *   @return void
	 */
	virtual void SetVideoMute(bool muted){};

	/**
	 *   @brief Set volume level
	 *
	 *   @param[in]  volume - Minimum 0, maximum 100.
	 *   @return void
	 */
	virtual void SetAudioVolume(int volume){};

	/**
	 *   @brief StreamSink Dtor
	 */
	virtual ~StreamSink(){};

	/**
	 *   @brief Process PTS discontinuity for a stream type
	 *
	 *   @param[in]  mediaType - Media Type
	 *   @return TRUE if discontinuity processed
	 */
	virtual bool Discontinuity( MediaType mediaType) = 0;


	/**
	 * @brief Check if PTS is changing
	 *
	 * @param[in] timeout - max time period within which PTS hasn't changed
	 * @retval true if PTS is changing, false if PTS hasn't changed for timeout msecs
	 */
	virtual bool CheckForPTSChangeWithTimeout(long timeout) { return true; }

	/**
	 *   @brief Check whether cach is empty
	 *
	 *   @param[in]  mediaType - Media Type
	 *   @return true: empty, false: not empty
	 */
	virtual bool IsCacheEmpty(MediaType mediaType){ return true; };

	/**
	 * @brief Reset EOS SignalledFlag
	 */
	virtual void ResetEOSSignalledFlag(){};

	/**
	 *   @brief API to notify that fragment caching done
	 *
	 *   @return void
	 */
	virtual void NotifyFragmentCachingComplete(){};

	/**
	 *   @brief API to notify that fragment caching is ongoing
	 *
	 *   @return void
	 */
	virtual void NotifyFragmentCachingOngoing(){};

	/**
	 *   @brief Get the video dimensions
	 *
	 *   @param[out]  w - Width
	 *   @param[out]  h - Height
	 *   @return void
	 */
	virtual void GetVideoSize(int &w, int &h){};

	/**
	 *   @brief Queue-up the protection event.
	 *
	 *   @param[in]  protSystemId - DRM system ID.
	 *   @param[in]  ptr - Pointer to the protection data.
	 *   @param[in]  len - Length of the protection data.
	 *   @return void
	 */
	virtual void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, MediaType type) {};

	/**
	 *   @brief Clear the protection event.
	 *
	 *   @return void
	 */
	virtual void ClearProtectionEvent() {};

	/**
	 *   @brief Signal discontinuity on trickmode if restamping is done by stream sink.
	 *
	 *   @return void
	 */
	virtual void SignalTrickModeDiscontinuity() {};

	/**
	 *   @brief Seek stream sink to desired position and playback rate with a flushing seek
	 *
	 *   @param[in]  position - desired playback position.
	 *   @param[in]  rate - desired playback rate.
	 *   @return void
	 */
	virtual void SeekStreamSink(double position, double rate) {};

	/**
	 *   @brief Get the video window co-ordinates
	 *
	 *   @return current video co-ordinates in x,y,w,h format
	 */
	virtual std::string GetVideoRectangle() { return std::string(); };

	/**
	 *   @brief Stop buffering in sink
	 *
	 *   @param[in] forceStop - true if buffering to be stopped without any checks
	 *   @return void
	 */
	virtual void StopBuffering(bool forceStop) { };

	/**
	 * @brief API to set track Id to audio sync property in case of AC4 audio
	 * 
	 * @param[in] trackId - AC4 track Id parsed by aamp based of preference
	 * @return bol sttaus of API
	 */
};


/**
 * @class PlayerInstanceAAMP
 * @brief Player interface class for the JS pluggin.
 */
class PlayerInstanceAAMP 
{
public:
	/**
	 *   @fn PlayerInstanceAAMP
	 *
	 *   @param  streamSink - custom stream sink, NULL for default.
	 *   @param  exportFrames - Callback function to export video frames of signature 'void fn(uint8_t *yuvBuffer, int size, int pixel_w, int pixel_h)'
	 */
	PlayerInstanceAAMP(StreamSink* streamSink = NULL
			, std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
            );

	/**
	 *   @fn ~PlayerInstanceAAMP
	 */
	~PlayerInstanceAAMP();

	/**
	 *   @brief copy constructor
	 *
	 *   @param other object to copy
	 */
	PlayerInstanceAAMP(const PlayerInstanceAAMP& other) = delete;

	/**
	 *   @brief To overload = operator for copying
	 *
	 *   @param other object to copy
	 */
	PlayerInstanceAAMP& operator=(const PlayerInstanceAAMP& other) = delete;

	/**
	 *   @fn Tune
	 *
	 *   @param[in]  mainManifestUrl - HTTP/HTTPS url to be played.
	 *   @param[in]  contentType - Content type of the asset
	 *   @param[in]  audioDecoderStreamSync - Enable or disable audio decoder stream sync,
	 *                set to 'false' if audio fragments come with additional padding at the end (BCOM-4203)
	 *   @return void
	 */
	void Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync);

	/**
	 *   @fn Tune
	 *
	 *   @param[in]  mainManifestUrl - HTTP/HTTPS url to be played.
	 *   @param[in]  autoPlay - Start playback immediately or not
	 *   @param[in]  contentType - Content type of the asset
	 *   @param[in]  audioDecoderStreamSync - Enable or disable audio decoder stream sync,
	 *                set to 'false' if audio fragments come with additional padding at the end (BCOM-4203)
	 *   @return void
	 */
	void Tune(const char *mainManifestUrl, bool autoPlay = true, const char *contentType = NULL, bool bFirstAttempt = true, bool bFinalAttempt = false,const char *traceUUID = NULL,bool audioDecoderStreamSync = true);

	/**
	 *   @brief Stop playback and release resources.
	 *   @param[in]  sendStateChangeEvent - true if state change events need to be sent for Stop operation
	 *   @return void
	 */
	void Stop(bool sendStateChangeEvent = true);
	
	/**
	 *   @fn ResetConfiguration
	 *   @return void
	 */
	void ResetConfiguration();

	/**
	 *   @fn SetRate
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  overshootcorrection - overshoot correction in milliseconds.
	 *   @return void
	 */
	void SetRate(int rate, int overshootcorrection=0);

	/**
	 *   @fn Seek
	 *
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 *   @param[in]  keepPaused - set true if want to keep paused state after seek
	 */
	void Seek(double secondsRelativeToTuneTime, bool keepPaused = false);

	/**
	 *   @fn SeekToLive
	 *
	 *   @param[in]  keepPaused - set true if want to keep paused state after seek
	 *   @return void	
	 */
	void SeekToLive(bool keepPaused = false);

	/**
	 *   @fn SetRateAndSeek
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 *   @return void
	 */
	void SetRateAndSeek(int rate, double secondsRelativeToTuneTime);

	/**
	 * @fn detach
	 * @return void
	 */
	void detach();

	/**
	 *   @fn RegisterEvents
	 *
	 *   @param[in]  eventListener - pointer to implementation of EventListener to receive events.
	 *   @return void
	 */
	void RegisterEvents(EventListener* eventListener);
	/**
	 *   @fn UnRegisterEvents
	 *
	 *   @param[in]  eventListener - pointer to implementation of EventListener to receive events.
	 *   @return void
	 */
	void UnRegisterEvents(EventListener* eventListener);

	/**
	 *   @fn SetVideoRectangle
	 *
	 *   @param[in]  x - horizontal start position.
	 *   @param[in]  y - vertical start position.
	 *   @param[in]  w - width.
	 *   @param[in]  h - height.
	 *   @return void
	 */
	void SetVideoRectangle(int x, int y, int w, int h);

	/**
	 *   @fn SetVideoZoom
	 *
	 *   @param[in]  zoom - zoom mode.
	 *   @return void
	 */
	void SetVideoZoom(VideoZoomMode zoom);

	/**
	 *   @fn SetVideoMute
	 *
	 *   @param[in]  muted - true to disable video, false to enable video.
	 *   @return void
	 */
	void SetVideoMute(bool muted);

	/**
	 *   @fn SetAudioVolume
	 *
	 *   @param[in]  volume - Minimum 0, maximum 100.
	 *   @return void
	 */
	void SetAudioVolume(int volume);

	/**
	 *   @fn SetLanguage
	 *
	 *   @param[in]  language - Language of audio track.
	 *   @return void
	 */
	void SetLanguage(const char* language);

	/**
	 *   @brief Set array of subscribed tags.
	 *
	 *   @param[in]  subscribedTags - Array of subscribed tags.
	 *   @return void
	 */
	void SetSubscribedTags(std::vector<std::string> subscribedTags);

	/**
         *   @fn SubscribeResponseHeaders
         *
         *   @param  responseHeaders - Array of response headers.
	 *   @return void
	 */
	void SubscribeResponseHeaders(std::vector<std::string> responseHeaders);

	/**
	 *   @fn LoadJS
	 *
	 *   @param[in]  context - JS context.
	 *   @return void
	 */
	void LoadJS(void* context);

	/**
	 *   @fn UnloadJS
	 *
	 *   @param[in]  context - JS context.
	 *   @return void
	 */
	void UnloadJS(void* context);

	/**
	 *   @fn AddEventListener
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener for the eventType.
	 *   @return void
	 */
	void AddEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 *   @fn RemoveEventListener
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener to be removed for the eventType.
	 *   @return void
	 */
	void RemoveEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 *   @fn IsLive
	 *
	 *   @return bool - True if live content, false otherwise
	 */
	bool IsLive();

	/**
	 *   @brief Schedule insertion of ad at given position.
	 *
	 *   @param[in]  url - HTTP/HTTPS url of the ad
	 *   @param[in]  positionSeconds - position at which ad shall be inserted
	 *   @return void
	 */
	void InsertAd(const char *url, double positionSeconds);

	/**
	 *   @fn GetCurrentAudioLanguage
	 *
	 *   @return const char* - current audio language
	 */
	const char* GetCurrentAudioLanguage();

	/**
	 *   @fn GetCurrentDRM
	 *
	 *   @return char* - current drm
	 */
	const char* GetCurrentDRM();
	/**
	 * @fn AddPageHeaders
	 * @param customHttpHeaders - customHttpHeaders map of custom http headers
	 * @return void
	 */
	void AddPageHeaders(std::map<std::string, std::string> customHttpHeaders);

	/**
	 *   @fn AddCustomHTTPHeader
	 *
	 *   @param[in]  headerName - Name of custom HTTP header
	 *   @param[in]  headerValue - Value to be pased along with HTTP header.
	 *   @param[in]  isLicenseHeader - true if header is to be used for license HTTP requests
	 *   @return void
	 */
	void AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader = false);

	/**
	 *   @fn SetLicenseServerURL
	 *
	 *   @param[in]  url - URL of the server to be used for license requests
	 *   @param[in]  type - DRM Type(PR/WV) for which the server URL should be used, global by default
	 *   @return void
	 */
	void SetLicenseServerURL(const char *url, DRMSystems type = eDRM_MAX_DRMSystems);

	/**
	 *   @fn SetPreferredDRM
	 *
	 *   @param[in] drmType - Preferred DRM type
	 *   @return void
	 */
	void SetPreferredDRM(DRMSystems drmType);

	/**
	 *   @fn GetPreferredDRM
	 *
	 *   @return Preferred DRM type
	 */
	DRMSystems GetPreferredDRM();

	/**
	 *   @fn  SetStereoOnlyPlayback
	 *   @param[in] bValue - disable EC3/ATMOS if the value is true
	 *
	 *   @return void
	 */
	void SetStereoOnlyPlayback(bool bValue);

	/**
	 *   @fn SetBulkTimedMetaReport
	 *   @param[in] bValue - if true Bulk event reporting enabled
	 *
	 *   @return void
	 */
	void SetBulkTimedMetaReport(bool bValue);

	/**
	 *   @fn SetRetuneForUnpairedDiscontinuity
	 *   @param[in] bValue - true if unpaired discontinuity retune set
	 *
	 *   @return void
	 */
	void SetRetuneForUnpairedDiscontinuity(bool bValue);

	/**
	 *   @fn SetRetuneForGSTInternalError
	 *   @param[in] bValue - true if gst internal error retune set
	 *
	 *   @return void
	 */
	void SetRetuneForGSTInternalError(bool bValue);

	/**
	 *   @fn SetAnonymousRequest
	 *
	 *   @param[in]  isAnonymous - True if session token should be blank and false otherwise.
	 *   @return void
	 */
	void SetAnonymousRequest(bool isAnonymous);

	/**
	 *   @fn SetAvgBWForABR
	 *
	 *   @param  useAvgBW - Flag for true / false
	 */
	void SetAvgBWForABR(bool useAvgBW);

	/**
	 *   @fn SetPreCacheTimeWindow
	 *
	 *   @param nTimeWindow Time in minutes - Max PreCache Time 
	 *   @return void
	 */
	void SetPreCacheTimeWindow(int nTimeWindow);

	/**
	 *   @fn SetVODTrickplayFPS
	 *
	 *   @param[in]  vodTrickplayFPS - FPS to be used for VOD Trickplay
	 *   @return void
	 */
	void SetVODTrickplayFPS(int vodTrickplayFPS);

	/**
	 *   @fn SetLinearTrickplayFPS
	 *
	 *   @param[in]  linearTrickplayFPS - FPS to be used for Linear Trickplay
	 *   @return void
	 */
	void SetLinearTrickplayFPS(int linearTrickplayFPS);

	/**
	 *   @fn SetLiveOffset
	 *
	 *   @param[in]  liveoffset- Live Offset
	 *   @return void
	 */
	void SetLiveOffset(int liveoffset);

	/**
	 *   @fn SetStallErrorCode
	 *
	 *   @param[in]  errorCode - error code for playback stall errors.
	 *   @return void
	 */
	void SetStallErrorCode(int errorCode);

	/**
	 *   @fn SetStallTimeout
	 *
	 *   @param[in]  timeoutMS - timeout in milliseconds for playback stall detection.
	 *   @return void
	 */
	void SetStallTimeout(int timeoutMS);

	/**
	 *   @fn SetReportInterval
	 *
	 *   @param  reportInterval - playback reporting interval in milliSeconds.
	 *   @return void
	 */
	void SetReportInterval(int reportInterval);

	/**
	 *   @fn SetInitFragTimeoutRetryCount
	 *
	 *   @param  count - max attempt for timeout retry count
	 *   @return void
	 */
	void SetInitFragTimeoutRetryCount(int count);

	/**
	 *   @fn GetPlaybackPosition
	 *
	 *   @return current playback position in seconds
	 */
	double GetPlaybackPosition(void);

	/**
	 *   @fn GetPlaybackDuration
	 *
	 *   @return duration in seconds
	 */
	double GetPlaybackDuration(void);

	/**
	 *   @fn GetState
	 *
	 *   @return current AAMP state
	 */
	PrivAAMPState GetState(void);

	/**
	 *   @fn GetVideoBitrate
	 *
	 *   @return bitrate of video profile
	 */
	long GetVideoBitrate(void);

	/**
	 *   @fn SetVideoBitrate
	 *
	 *   @param[in] bitrate preferred bitrate for video profile
	 */
	void SetVideoBitrate(long bitrate);

	/**
	 *   @fn GetAudioBitrate
	 *
	 *   @return bitrate of audio profile
	 */
	long GetAudioBitrate(void);

	/**
	 *   @fn SetAudioBitrate
	 *
	 *   @param[in] bitrate preferred bitrate for audio profile
	 */
	void SetAudioBitrate(long bitrate);

	/**
	 *   @fn GetVideoZoom
	 *
	 *   @return video zoom mode
	 */
	int GetVideoZoom(void);

	/**
	 *   @fn GetVideoMute
	 *
	 *   @return video mute status
	 *
	 */
	bool GetVideoMute(void);

	/**
	 *   @fn GetAudioVolume
	 *
	 *   @return audio volume
	 */
	int GetAudioVolume(void);

	/**
	 *   @fn GetPlaybackRate
	 *
	 *   @return current playback rate
	 */
	int GetPlaybackRate(void);

	/**
	 *   @fn GetVideoBitrates
	 *
	 *   @return available video bitrates
	 */
	std::vector<long> GetVideoBitrates(void);

	/**
         *   @fn GetManifest
         *
         *   @return available manifest
         */
        std::string GetManifest(void);

	/**
	 *   @fn GetAudioBitrates
	 *
	 *   @return available audio bitrates
	 */
	std::vector<long> GetAudioBitrates(void);

	/**
	 *   @fn SetInitialBitrate
	 *
	 *   @param[in] bitrate initial bitrate to be selected
	 *   @return void
	 */
	void SetInitialBitrate(long bitrate);

	/**
	 *   @fn GetInitialBitrate
	 *
	 *   @return initial bitrate value.
	 */
	long  GetInitialBitrate(void);

	/**
	 *   @fn SetInitialBitrate4K
	 *
	 *   @param[in] bitrate4K initial bitrate to be selected for 4K assets.
	 *   @return void
	 */
	void SetInitialBitrate4K(long bitrate4K);

	/**
	 *   @fn GetInitialBitrate4k
	 *
	 *   @return initial bitrate value for 4k assets
	 */
	long GetInitialBitrate4k(void);

	/**
	 *   @fn SetNetworkTimeout
	 *   
	 *   @param[in] timeout preferred timeout value
	 *   @return void
	 */
	void SetNetworkTimeout(double timeout);

	/**
	 *   @fn SetManifestTimeout
	 *
	 *   @param[in] timeout preferred timeout value
	 *   @return void
	 */
	void SetManifestTimeout(double timeout);

	/**
	 *   @fn SetPlaylistTimeout
	 *
	 *   @param[in] timeout preferred timeout value
	 *   @return void
	 */
	void SetPlaylistTimeout(double timeout);

	/**
	 *   @fn SetDownloadBufferSize
	 *
	 *   @param[in] bufferSize preferred download buffer size
	 *   @return void
	 */
	void SetDownloadBufferSize(int bufferSize);

	/**
	 *   @fn SetNetworkProxy
	 *
	 *   @param[in] proxy network proxy to use
	 *   @return void
	 */
	void SetNetworkProxy(const char * proxy);

	/**
	 *   @fn SetLicenseReqProxy
	 *   @param[in] licenseProxy proxy to use for license request
	 *   @return void
	 */
	void SetLicenseReqProxy(const char * licenseProxy);

	/**
	 *   @fn SetDownloadStallTimeout
	 *
	 *   @param[in] stallTimeout curl stall timeout value
	 *   @return void
	 */
	void SetDownloadStallTimeout(long stallTimeout);

	/**
	 *   @fn SetDownloadStartTimeout
	 *
	 *   @param[in] startTimeout curl download start timeout
	 *   @return void
	 */
	void SetDownloadStartTimeout(long startTimeout);

	/**
	 *   @fn SetDownloadLowBWTimeout
	 *
	 *   @param[in] lowBWTimeout curl download low bandwidth timeout
	 *   @return void
	 */
	void SetDownloadLowBWTimeout(long lowBWTimeout);

	/**
	 *   @fn SetPreferredSubtitleLanguage
	 *
	 *   @param[in]  language - Language of text track.
	 *   @return void
	 */
	void SetPreferredSubtitleLanguage(const char* language);

	/**
	 *   @fn SetAlternateContents
	 *
	 *   @param[in] adBreakId Adbreak's unique identifier.
	 *   @param[in] adId Individual Ad's id
	 *   @param[in] url Ad URL
	 *   @return void
	 */
	void SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url);

	/**
	 *   @fn SetParallelPlaylistDL
	 *   @param[in] bValue - true if a/v playlist to be downloaded in parallel
	 *
	 *   @return void
	 */
	void SetParallelPlaylistDL(bool bValue);
	/**
	 *   @fn SetAsyncTuneConfig
	 *   @param[in] bValue - true if async tune enabled 
	 *
	 *   @return void
	 */
	void SetAsyncTuneConfig(bool bValue);

	/**
	 *	@fn SetParallelPlaylistRefresh
	 *	@param[in] bValue - true if a/v playlist to be downloaded in parallel
	 *
	 *	@return void
	 */
	void SetParallelPlaylistRefresh(bool bValue);

	/**
	 *   @fn SetWesterosSinkConfig
	 *   @param[in] bValue - true if westeros sink enabled
	 *
	 *   @return void
	 */
	void SetWesterosSinkConfig(bool bValue);

	/**
	 *	@fn SetLicenseCaching
	 *	@param[in] bValue - true/false to enable/disable license caching
	 *
	 *	@return void
	 */
	void SetLicenseCaching(bool bValue);

	/**
         *      @fn SetOutputResolutionCheck
         *      @param[in] bValue - true/false to enable/disable profile filtering by display resoluton
         *
         *      @return void
         */
	void SetOutputResolutionCheck(bool bValue);

	/**
	 *   @fn SetMatchingBaseUrlConfig
	 *
	 *   @param[in] bValue - true if Matching BaseUrl enabled
	 *   @return void
	 */
	void SetMatchingBaseUrlConfig(bool bValue);

        /**
         *   @fn SetPropagateUriParameters
         *
         *   @param[in] bValue - default value: true
         *   @return void
         */
	void SetPropagateUriParameters(bool bValue);

        /**
         *   @fn ApplyArtificialDownloadDelay
         *
         *   @param[in] DownloadDelayInMs - default value: zero
         *   @return void
         */
        void ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs);

	/**
	 *   @fn SetSslVerifyPeerConfig
	 *
	 *   @param[in] bValue - default value: false
	 *   @return void
	 */
	void SetSslVerifyPeerConfig(bool bValue);

	/**
	 *   @brief Configure New ABR Enable/Disable
	 *   @param[in] bValue - true if new ABR enabled
	 *
	 *   @return void
	 */
	void SetNewABRConfig(bool bValue);

	/**
	 *   @fn SetNewAdBreakerConfig
	 *   @param[in] bValue - true if new AdBreaker enabled
	 *
	 *   @return void
	 */
	void SetNewAdBreakerConfig(bool bValue);

	/**
	 *   @fn GetAvailableVideoTracks
	 *
	 *   @return std::string JSON formatted list of video tracks
	 */
	std::string GetAvailableVideoTracks();

	/**
	 *   @fn SetVideoTracks
	 *   @param[in] bitrate - video bitrate list
	 *   
	 *   @return void
	 */
	void SetVideoTracks(std::vector<long> bitrates);

	/**
	 *   @fn GetAvailableAudioTracks
	 *
	 *   @return std::string JSON formatted list of audio tracks
	 */
	std::string GetAvailableAudioTracks(bool allTrack=false);

	/**
	 *   @fn GetAvailableTextTracks
	 */
	std::string GetAvailableTextTracks(bool allTrack = false);

	/**
	 *   @fn GetVideoRectangle 
	 *
	 *   @return current video co-ordinates in x,y,w,h format
	 */
	std::string GetVideoRectangle();

	/**
	 *   @fn SetAppName
	 *
	 *   @return void
	 */
	void SetAppName(std::string name);

	/**
	 *   @fn SetPreferredLanguages
	 *   @param[in] languageList - string with comma-delimited language list in ISO-639
	 *             from most to least preferred: "lang1,lang2". Set NULL to clear current list.
     	 *   @param[in] preferredRendition  - preferred rendition from role
     	 *   @param[in] preferredType -  preferred accessibility type
	 *   @param[in] codecList - string with comma-delimited codec list
	 *             from most to least preferred: "codec1,codec2". Set NULL to clear current list.
	 * 	 @param[in] labelList - string with comma-delimited label list
	 *             from most to least preferred: "label1,label2". Set NULL to clear current list.
	 *   @return void
	 */
	void SetPreferredLanguages(const char* languageList, const char *preferredRendition = NULL, const char *preferredType = NULL, const char* codecList = NULL, const char* labelList = NULL ); 


	/**
	 *   @fn SetPreferredTextLanguages 
	 *   @param[in] languageList - string with comma-delimited language list in ISO-639
	 *   @return void
	 */
	void SetPreferredTextLanguages(const char* param); 

	/**
	 *   @fn SetAudioTrack
	 *   @param[in] language - Language to set 
	 *   @param[in] rendition - Role/rendition to set
	 *   @param[in] codec - Codec to set
	 *   @param[in] channel - Channel number to set
	 *   @param[in] label - Label to set
	 *
	 *   @return void
	 */
	void SetAudioTrack(std::string language="", std::string rendition="", std::string type="", std::string codec="", unsigned int channel=0, std::string label="");
	/**
	 *   @fn SetPreferredCodec
	 *   @param[in] codecList - string with array with codec list
	 *
	 *   @return void
	 */
	void SetPreferredCodec(const char *codecList);

	/**
	 *   @fn SetPreferredLabels
	 *   @param[in] lableList - string with array with label list
	 *
	 *   @return void
	 */
	void SetPreferredLabels(const char *lableList);

	/**
	 *   @fn SetPreferredRenditions
	 *   @param[in] renditionList - string with comma-delimited rendition list in ISO-639
	 *             from most to least preferred. Set NULL to clear current list.
	 *
	 *   @return void
	 */
	void SetPreferredRenditions(const char *renditionList);

	/**
	 *   @fn GetPreferredLanguages
	 *
	 *   @return const char* - current comma-delimited language list or NULL if not set
	 */
	 const char* GetPreferredLanguages();

	/**
	 *   @fn SetTuneEventConfig
	 *
	 *   @param[in] tuneEventType preferred tune event type
	 */
	void SetTuneEventConfig(int tuneEventType);

	/**
	 *   @fn EnableVideoRectangle
	 *
	 *   @param[in] rectProperty video rectangle property
	 */
	void EnableVideoRectangle(bool rectProperty);

	/**
	 *   @fn SetRampDownLimit
	 *   @return void
	 */
	void SetRampDownLimit(int limit);

	/**
	 * @fn GetRampDownLimit
	 * @return rampdownlimit config value
	 */
	int GetRampDownLimit(void);

	/**
	 * @fn SetInitRampdownLimit
	 * @return void
	 */
	void SetInitRampdownLimit(int limit);

	/**
	 * @fn SetMinimumBitrate
	 * @return void
	 */
	void SetMinimumBitrate(long bitrate);

	/**
	 * @fn GetMinimumBitrate
	 * @return Minimum bitrate value
	 *
	 */
	long GetMinimumBitrate(void);

	/**
	 * @fn SetMaximumBitrate
	 * @return void
	 */
	void SetMaximumBitrate(long bitrate);

	/**
	 * @fn GetMaximumBitrate
	 * @return Max bit rate value 
	 */
	long GetMaximumBitrate(void);

	/**
	 * @fn SetSegmentInjectFailCount
	 * @return void        
	 */
	void SetSegmentInjectFailCount(int value);

	/**
	 * @fn SetSegmentDecryptFailCount
	 * @return void
	 */
	void SetSegmentDecryptFailCount(int value);

	/**
	 * @fn SetInitialBufferDuration
	 * @return void
	 */
	void SetInitialBufferDuration(int durationSec);

	/**
	 * @fn GetInitialBufferDuration
	 *
	 * @return int - Initial Buffer Duration
	 */
	int GetInitialBufferDuration(void);

	/**
	 *   @fn SetNativeCCRendering
	 *
	 *   @param[in] enable - true for native CC rendering on
	 *   @return void
	 */
	void SetNativeCCRendering(bool enable);

	/**
	 *   @fn SetAudioTrack
	 *
	 *   @param[in] trackId - index of audio track in available track list
	 *   @return void
	 */
	void SetAudioTrack(int trackId);

	/**
	 *   @fn GetAudioTrack
	 *
	 *   @return int - index of current audio track in available track list
	 */
	int GetAudioTrack();

	/**
	 *   @fn GetAudioTrackInfo
	 *
	 *   @return int - index of current audio track in available track list
	 */
	std::string GetAudioTrackInfo();

	/**
	 *   @fn GetTextTrackInfo
	 *
	 *   @return int - index of current audio track in available track list
	 */
	std::string GetTextTrackInfo();

	/**
	 *   @fn GetPreferredAudioProperties
	 *
	 *   @return json string
	 */
	std::string GetPreferredAudioProperties();

	/**
	 *   @fn GetPreferredTextProperties
	 */
	std::string GetPreferredTextProperties();
	
	/**
	 *   @fn SetTextTrack
	 *
	 *   @param[in] trackId - index of text track in available track list
	 *   @return void
	 */
	void SetTextTrack(int trackId);

	/**
	 *   @fn GetTextTrack
	 *
	 *   @return int - index of current text track in available track list
	 */
	int GetTextTrack();

	/**
	 *   @fn SetCCStatus
	 *
	 *   @param[in] enabled - true for CC on, false otherwise
	 *   @return void
	 */
	void SetCCStatus(bool enabled);

	/**
	 *   @fn GetCCStatus
	 *
	 *   @return bool true (enabled) else false(disabled)
	 */
	bool GetCCStatus(void);

	/**
	 *   @fn SetTextStyle
	 *
	 *   @param[in] options - JSON formatted style options
	 *   @return void
	 */
	void SetTextStyle(const std::string &options);

	/**
	 *   @fn GetTextStyle
	 *
	 *   @return std::string - JSON formatted style options
	 */
	std::string GetTextStyle();
	/**
	 * @fn SetLanguageFormat
	 * @param[in] preferredFormat - one of \ref LangCodePreference
	 * @param[in] useRole - if enabled, the language in format <lang>-<role>
	 *                      if <role> attribute available in stream
	 *
	 * @return void
	 */
	void SetLanguageFormat(LangCodePreference preferredFormat, bool useRole = false);

	/**
	 *   @brief Set the CEA format for force setting
	 *
	 *   @param[in] format - 0 for 608, 1 for 708
	 *   @return void
	 */
	void SetCEAFormat(int format);

	/**
	 *   @brief Set the session token for player
	 *
	 *   @param[in]string -  sessionToken
	 *   @return void
	 */
	void SetSessionToken(std::string sessionToken);

	/**
	 *   @fn SetMaxPlaylistCacheSize
	 *   @param cacheSize size of cache to store
	 *   @return void
	 */
	void SetMaxPlaylistCacheSize(int cacheSize);

	/**
	 *   @fn EnableSeekableRange
	 *
	 *   @param[in] enabled - true if enabled
	 */
	void EnableSeekableRange(bool enabled);

	/**
	 *   @fn SetReportVideoPTS
	 *  
	 *   @param[in] enabled - true if enabled
	 */
	void SetReportVideoPTS(bool enabled);

	/**
	 *   @fn SetDisable4K
	 *
	 *   @param[in] value - disabled if true
	 *   @return void
	 */
	void SetDisable4K(bool value);

	/**
	 *   @fn DisableContentRestrictions
	 *   @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 *   @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 *   @param[in] eventChange - disable restriction handling till next program event boundary
	 *
	 *   @return void
	 */
	void DisableContentRestrictions(long grace, long time, bool eventChange);

	/**
	 *   @fn EnableContentRestrictions
	 *   @return void
	 */
	void EnableContentRestrictions();

	/**
	 *   @fn AsyncStartStop 
	 *
	 *   @return void
	 */
	void AsyncStartStop();

	/**
	 *   @fn PersistBitRateOverSeek
	 *
	 *   @param[in] value - To enable/disable configuration
	 *   @return void
	 */
	void PersistBitRateOverSeek(bool value);

	/**
	 *   @fn GetAvailableThumbnailTracks
	 *
	 *   @return bitrate of thumbnail track.
	 */
	std::string GetAvailableThumbnailTracks(void);

	/**
	 *   @fn SetThumbnailTrack
	 *
	 *   @param[in] thumbIndex preferred bitrate for thumbnail profile
	 */
	bool SetThumbnailTrack(int thumbIndex);

	/**
	 *   @fn GetThumbnails
	 *
	 *   @param[in] eduration duration  for thumbnails
	 */
	std::string GetThumbnails(double sduration, double eduration);

	/**
	 *   @fn SetPausedBehavior
	 *
	 *   @param[in]  behavior paused behavior
	 */
	void SetPausedBehavior(int behavior);

	/**
	 * @fn InitAAMPConfig
	 */
	bool InitAAMPConfig(char *jsonStr);

	/**
	 * @fn GetAAMPConfig 
	 */
	std::string GetAAMPConfig();

	/**
	 *   @fn SetUseAbsoluteTimeline
	 *
	 *   @param[in] configState bool enable/disable configuration
	 */
	void SetUseAbsoluteTimeline(bool configState);

	/**
  	 *   @fn XRESupportedTune
   	 *   @param[in] xreSupported bool On/Off
     	 */
	void XRESupportedTune(bool xreSupported);

	/**
	 *   @brief Enable async operation and initialize resources
	 *
	 *   @return void
	 */
	void EnableAsyncOperation();

	/**
	 *   @fn SetRepairIframes
	 *
	 *   @param[in] configState bool enable/disable configuration
	 */
	void SetRepairIframes(bool configState);

	/**
	 *   @fn SetAuxiliaryLanguage
	 *
	 *   @param[in] language - auxiliary language
	 *   @return void
	 */
	void SetAuxiliaryLanguage(const std::string &language);

	/**
	 *   @fn SetLicenseCustomData
	 *
	 *   @param[in]  customData - custom data string to be passed to the license server.
	 *   @return void
	 */
	void SetLicenseCustomData(const char *customData);

	class PrivateInstanceAAMP *aamp;  		  /**< AAMP player's private instance */
	std::shared_ptr<PrivateInstanceAAMP> sp_aamp; 	  /**< shared pointer for aamp resource */

	AampConfig mConfig;
	/**
	 *   @fn GetPlaybackStats
         *
   	 *   @return json string reperesenting the stats
  	 */
	std::string GetPlaybackStats();
private:
	
	/**
	 *   @fn IsValidRate
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @retval return true if the given rate is valid.
	 */
	bool IsValidRate(int rate);
        /**
         *   @fn TuneInternal
         *
         *   @param  mainManifestUrl - HTTP/HTTPS url to be played.
         *   @param[in] autoPlay - Start playback immediately or not
         *   @param  contentType - content Type.
         */
	void TuneInternal(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync);
	/**
         *   @fn SetRateInternal
         *
         *   @param  rate - Rate of playback.
         *   @param  overshootcorrection - overshoot correction in milliseconds.
         */
	void SetRateInternal(int rate,int overshootcorrection);
	/**
         *   @fn SeekInternal
         *
         *   @param  secondsRelativeToTuneTime - Seek position for VOD,
         *           relative position from first tune command.
         *   @param  keepPaused - set true if want to keep paused state after seek
         */
	void SeekInternal(double secondsRelativeToTuneTime, bool keepPaused);
	/**
	 *   @fn SetAudioTrackInternal
	 *   @param[in] language, rendition, codec, channel 
	 *   @return void
	 */
	void SetAudioTrackInternal(std::string language,  std::string rendition, std::string codec,  std::string type, unsigned int channel, std::string label);
	/**
	 *   @fn SetAuxiliaryLanguageInternal
	 *   @param[in][optional] language
	 *   @return void
	 */
	void SetAuxiliaryLanguageInternal(const std::string &language);
	/**
	 *   @fn SetTextTrackInternal
	 *   @param[in] trackId
	 *   @return void
	 */
	void SetTextTrackInternal(int trackId);
private:	

	/**
	 *   @fn StopInternal
	 *
	 *   @param[in]  sendStateChangeEvent - true if state change events need to be sent for Stop operation
	 *   @return void
	 */
	void StopInternal(bool sendStateChangeEvent);

	StreamSink* mInternalStreamSink;    /**< Pointer to stream sink */
	void* mJSBinding_DL;                /**< Handle to AAMP plugin dynamic lib.  */
	static std::mutex mPrvAampMtx;      /**< Mutex to protect aamp instance in GetState() */
	bool mAsyncRunning;                 /**< Flag denotes if async mode is on or not */
	bool mAsyncTuneEnabled;		    /**< Flag indicating async tune status */
	AampScheduler mScheduler;
};

#endif // MAINAAMP_H
