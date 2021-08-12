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
 * @brief AAMP anomaly message types
 */
typedef enum
{
	ANOMALY_ERROR,
	ANOMALY_WARNING,
	ANOMALY_TRACE
} AAMPAnomalyMessageType;

/**
 * @brief  Structure holding aamp tune failure code and corresponding application error code and description
 */
struct TuneFailureMap
{
	AAMPTuneFailure tuneFailure;    /**< Failure ID */
	int code;                       /**< Error code */
	const char* description;        /**< Textual description */
};


/**
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
	FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
	FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
	FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
	FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
	FORMAT_UNKNOWN          /**< Unknown Format */
};

/**
 * @brief Video zoom mode
 */
enum VideoZoomMode
{
	VIDEO_ZOOM_FULL,    /**< Video Zoom Enabled */
	VIDEO_ZOOM_NONE     /**< Video Zoom Disabled */
};


using AdObject = std::pair<std::string, std::string>;

/**
 *  @brief Auth Token Failure codes
 */
enum AuthTokenErrors {
	eAUTHTOKEN_TOKEN_PARSE_ERROR = -1,
	eAUTHTOKEN_INVALID_STATUS_CODE = -2
};

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
 * @brief Structure for audio track information
 *        Holds information about an audio track in playlist
 */
struct AudioTrackInfo
{
	std::string index;
	std::string language;
	std::string rendition; //role for DASH, group-id for HLS
	std::string name;
	std::string codec;
	std::string characteristics;
	int channels;
	long bandwidth;
	int primaryKey; // used for ATSC to store key , this should not be exposed to app.
	std::string contentType; // used for ATSC to propogate content type
	std::string mixType; // used for ATSC to propogate mix type

	AudioTrackInfo() : index(), language(), rendition(), name(), codec(), characteristics(), channels(0), bandwidth(0),primaryKey(0), contentType(), mixType()
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, std::string cha, int ch):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(cha), channels(ch), bandwidth(-1), primaryKey(0) , contentType(), mixType()
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, int pk, std::string conType, std::string mixType):
			index(idx), language(lang), rendition(rend), name(trackName),
			codec(codecStr), characteristics(), channels(0), bandwidth(-1), primaryKey(pk),
                        contentType(conType), mixType(mixType)
	{
	}

	AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw):
		index(idx), language(lang), rendition(rend), name(trackName),
		codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType()
	{
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
	int primaryKey; // used for ATSC to store key , this should not be exposed to app.

	TextTrackInfo() : index(), language(), isCC(false), rendition(), name(), instreamId(), characteristics(), codec(), primaryKey(0)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string id, std::string cha):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(id), characteristics(cha),
		codec(), primaryKey(0)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string id, std::string cha, int pk):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(id), characteristics(cha),
		codec(), primaryKey(pk)
	{
	}

	TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr):
		index(idx), language(lang), isCC(cc), rendition(rend),
		name(trackName), instreamId(), characteristics(),
		codec(codecStr), primaryKey(0)
	{
	}
};

/**
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
	 *   @return void
	 */
	virtual void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, bool bESChangeStatus, bool forwardAudioToAux)=0;

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
	 *   @return void
	 */
	virtual void SendTransfer( MediaType mediaType, struct GrowableBuffer* buffer, double fpts, double fdts, double duration)= 0;

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
	 *   @brief Stop the stream
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
	 *   @brief Flush the pipeline
	 *
	 *   @param[in]  position - playback position
	 *   @param[in]  rate - Speed
	 *   @param[in]  shouldTearDown - if pipeline is not in a valid state, tear down pipeline
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
};


/**
 * @brief Player interface class for the JS pluggin.
 */
class PlayerInstanceAAMP : public AampScheduler
{
public:
	/**
	 *   @brief PlayerInstanceAAMP Constructor.
	 *
	 *   @param  streamSink - custom stream sink, NULL for default.
	 *   @param  exportFrames - Callback function to export video frames of signature 'void fn(uint8_t *yuvBuffer, int size, int pixel_w, int pixel_h)'
	 */
	PlayerInstanceAAMP(StreamSink* streamSink = NULL
			, std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
            );

	/**
	 *   @brief PlayerInstanceAAMP Destructor.
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
	 *   @brief Tune to a URL.
	 *      DEPRECATED!  This is included for backwards compatibility with current Sky AS integration
	 *      audioDecoderStreamSync is a broadcom-specific hack (for original xi6 POC build) - this doesn't belong in Tune API.
	 *
	 *   @param[in]  url - HTTP/HTTPS url to be played.
	 *   @param[in]  contentType - Content type of the asset
	 *   @param[in]  audioDecoderStreamSync - Enable or disable audio decoder stream sync,
	 *                set to 'false' if audio fragments come with additional padding at the end (BCOM-4203)
	 *   @return void
	 */
	void Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync);

	/**
	 *   @brief Tune to a URL.
	 *
	 *   @param[in]  url - HTTP/HTTPS url to be played.
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
	 *	 @brief API to reset configuration across tunes if same player instance used
	 *
	 */
	void ResetConfiguration();

	/**
	 *   @brief Set playback rate.
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  overshoot - overshoot correction in milliseconds.
	 *   @return void
	 */
	void SetRate(int rate, int overshootcorrection=0);

	/**
	 *   @brief Seek to a time.
	 *
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 *   @param[in]  keepPaused - set true if want to keep paused state after seek
	 */
	void Seek(double secondsRelativeToTuneTime, bool keepPaused = false);

	/**
	 *   @brief Seek to live point.
	 *
	 *   @param[in]  keepPaused - set true if want to keep paused state after seek
	 */
	void SeekToLive(bool keepPaused = false);

	/**
	 *   @brief Set seek position and speed.
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 */
	void SetRateAndSeek(int rate, double secondsRelativeToTuneTime);

	/**
	 * @brief Soft stop the player instance.
	 *
	 */
	void detach();

	/**
	 *   @brief Register event handler.
	 *
	 *   @param[in]  eventListener - pointer to implementation of EventListener to receive events.
	 *   @return void
	 */
	void RegisterEvents(EventListener* eventListener);

	/**
	 *   @brief Specify video rectangle.
	 *
	 *   @param[in]  x - horizontal start position.
	 *   @param[in]  y - vertical start position.
	 *   @param[in]  w - width.
	 *   @param[in]  h - height.
	 *   @return void
	 */
	void SetVideoRectangle(int x, int y, int w, int h);

	/**
	 *   @brief Set video zoom.
	 *
	 *   @param[in]  zoom - zoom mode.
	 *   @return void
	 */
	void SetVideoZoom(VideoZoomMode zoom);

	/**
	 *   @brief Enable/ Disable Video.
	 *
	 *   @param[in]  muted - true to disable video, false to enable video.
	 *   @return void
	 */
	void SetVideoMute(bool muted);

	/**
	 *   @brief Set Audio Volume.
	 *
	 *   @param[in]  volume - Minimum 0, maximum 100.
	 *   @return void
	 */
	void SetAudioVolume(int volume);

	/**
	 *   @brief Set Audio language.
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
	 *   @brief Load AAMP JS object in the specified JS context.
	 *
	 *   @param[in]  context - JS context.
	 *   @return void
	 */
	void LoadJS(void* context);

	/**
	 *   @brief Unload AAMP JS object in the specified JS context.
	 *
	 *   @param[in]  context - JS context.
	 *   @return void
	 */
	void UnloadJS(void* context);

	/**
	 *   @brief Support multiple listeners for multiple event type
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener for the eventType.
	 *   @return void
	 */
	void AddEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 *   @brief Remove event listener for eventType.
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener to be removed for the eventType.
	 *   @return void
	 */
	void RemoveEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 *   @brief To check whether the asset is live or not.
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
	 *   @brief Get current audio language.
	 *
	 *   @return const char* - current audio language
	 */
	const char* GetCurrentAudioLanguage();

	/**
	 *   @brief Get current drm
	 *
	 *   @return char* - current drm
	 */
	const char* GetCurrentDRM();

	/**
	 *   @brief Add/Remove a custom HTTP header and value.
	 *
	 *   @param[in]  headerName - Name of custom HTTP header
	 *   @param[in]  headerValue - Value to be pased along with HTTP header.
	 *   @param[in]  isLicenseHeader - true if header is to be used for license HTTP requests
	 *   @return void
	 */
	void AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader = false);

	/**
	 *   @brief Set License Server URL.
	 *
	 *   @param[in]  url - URL of the server to be used for license requests
	 *   @param[in]  type - DRM Type(PR/WV) for which the server URL should be used, global by default
	 *   @return void
	 */
	void SetLicenseServerURL(const char *url, DRMSystems type = eDRM_MAX_DRMSystems);

	/**
	 *   @brief Set Preferred DRM.
	 *
	 *   @param[in] drmType - Preferred DRM type
	 *   @return void
	 */
	void SetPreferredDRM(DRMSystems drmType);


	/**
	 *   @brief Get Preferred DRM.
	 *
	 *   @return Preferred DRM type
	 */
	DRMSystems GetPreferredDRM();

	/**
	 *   @brief Set Stereo Only Playback.
	 *   @param[in] bValue - disable EC3/ATMOS if the value is true
	 *
	 *   @return void
	 */
	void SetStereoOnlyPlayback(bool bValue);

	/**
	 *   @brief Set Bulk TimedMetadata Reporting flag
	 *   @param[in] bValue - if true Bulk event reporting enabled
	 *
	 *   @return void
	 */
	void SetBulkTimedMetaReport(bool bValue);

	/**
	 *	 @brief Set unpaired discontinuity retune flag
	 *	 @param[in] bValue - true if unpaired discontinuity retune set
	 *
	 *	 @return void
	 */
	void SetRetuneForUnpairedDiscontinuity(bool bValue);

	/**
	 *	 @brief Set retune configuration for gstpipeline internal data stream error.
	 *	 @param[in] bValue - true if gst internal error retune set
	 *
	 *	 @return void
	 */
	void SetRetuneForGSTInternalError(bool bValue);

	/**
	 *   @brief Indicates if session token has to be used with license request or not.
	 *
	 *   @param[in]  isAnonymous - True if session token should be blank and false otherwise.
	 *   @return void
	 */
	void SetAnonymousRequest(bool isAnonymous);

	/**
	 *   @brief Indicates average BW to be used for ABR Profiling.
	 *
	 *   @param  useAvgBW - Flag for true / false
	 */
	void SetAvgBWForABR(bool useAvgBW);

	/**
	*	@brief SetPreCacheTimeWindow Function to Set PreCache Time
	*
	*	@param	Time in minutes - Max PreCache Time 
	*/
	void SetPreCacheTimeWindow(int nTimeWindow);

	/**
	 *   @brief Set VOD Trickplay FPS.
	 *
	 *   @param[in]  vodTrickplayFPS - FPS to be used for VOD Trickplay
	 *   @return void
	 */
	void SetVODTrickplayFPS(int vodTrickplayFPS);

	/**
	 *   @brief Set Linear Trickplay FPS.
	 *
	 *   @param[in]  linearTrickplayFPS - FPS to be used for Linear Trickplay
	 *   @return void
	 */
	void SetLinearTrickplayFPS(int linearTrickplayFPS);

	/**
	 *   @brief Set Live Offset
	 *
	 *   @param[in]  liveoffset- Live Offset
	 *   @return void
	 */
	void SetLiveOffset(int liveoffset);

	/**
	 *   @brief To set the error code to be used for playback stalled error.
	 *
	 *   @param[in]  errorCode - error code for playback stall errors.
	 *   @return void
	 */
	void SetStallErrorCode(int errorCode);

	/**
	 *   @brief To set the timeout value to be used for playback stall detection.
	 *
	 *   @param[in]  timeoutMS - timeout in milliseconds for playback stall detection.
	 *   @return void
	 */
	void SetStallTimeout(int timeoutMS);

	/**
	 *   @brief To set the Playback Position reporting interval.
	 *
	 *   @param  reportIntervalMS - playback reporting interval in milliseconds.
	 */
	void SetReportInterval(int reportIntervalMS);

	/**
	 *   @brief To set the max retry attempts for init frag curl timeout failures
	 *
	 *   @param  count - max attempt for timeout retry count
	 */
	void SetInitFragTimeoutRetryCount(int count);

	/**
	 *   @brief To get the current playback position.
	 *
	 *   @ret current playback position in seconds
	 */
	double GetPlaybackPosition(void);

	/**
	 *   @brief To get the current asset's duration.
	 *
	 *   @ret duration in seconds
	 */
	double GetPlaybackDuration(void);

	/**
	 *   @brief To get the current AAMP state.
	 *
	 *   @ret current AAMP state
	 */
	PrivAAMPState GetState(void);

	/**
	 *   @brief To get the bitrate of current video profile.
	 *
	 *   @ret bitrate of video profile
	 */
	long GetVideoBitrate(void);

	/**
	 *   @brief To set a preferred bitrate for video profile.
	 *
	 *   @param[in] preferred bitrate for video profile
	 */
	void SetVideoBitrate(long bitrate);

	/**
	 *   @brief To get the bitrate of current audio profile.
	 *
	 *   @ret bitrate of audio profile
	 */
	long GetAudioBitrate(void);

	/**
	 *   @brief To set a preferred bitrate for audio profile.
	 *
	 *   @param[in] preferred bitrate for audio profile
	 */
	void SetAudioBitrate(long bitrate);

	/**
	 *   @brief To get the current audio volume.
	 *
	 *   @ret audio volume
	 */
	int GetAudioVolume(void);

	/**
	 *   @brief To get the current playback rate.
	 *
	 *   @ret current playback rate
	 */
	int GetPlaybackRate(void);

	/**
	 *   @brief To get the available video bitrates.
	 *
	 *   @ret available video bitrates
	 */
	std::vector<long> GetVideoBitrates(void);

	/**
	 *   @brief To get the available audio bitrates.
	 *
	 *   @ret available audio bitrates
	 */
	std::vector<long> GetAudioBitrates(void);

	/**
	 *   @brief To set the initial bitrate value.
	 *
	 *   @param[in] initial bitrate to be selected
	 */
	void SetInitialBitrate(long bitrate);

	/**
	 *   @brief To set the initial bitrate value for 4K assets.
	 *
	 *   @param[in] initial bitrate to be selected for 4K assets
	 */
	void SetInitialBitrate4K(long bitrate4K);

	/**
	 *   @brief To override default curl timeout for playlist/fragment downloads
	 *
	 *   @param[in] preferred timeout value
	 */
	void SetNetworkTimeout(double timeout);

	/**
	 *   @brief Optionally override default HLS main manifest download timeout with app-specific value.
	 *
	 *   @param[in] preferred timeout value
	 */
	void SetManifestTimeout(double timeout);

	/**
	 *   @brief Optionally override default HLS main manifest download timeout with app-specific value.
	 *
	 *   @param[in] preferred timeout value
	*/
	void SetPlaylistTimeout(double timeout);

	/**
	 *   @brief To set the download buffer size value
	 *
	 *   @param[in] preferred download buffer size
	 */
	void SetDownloadBufferSize(int bufferSize);

	/**
	 *   @brief To set the network proxy
	 *
	 *   @param[in] network proxy to use
	 */
	void SetNetworkProxy(const char * proxy);

	/**
	 *   @brief To set the proxy for license request
	 *
	 *   @param[in] proxy to use for license request
	 */
	void SetLicenseReqProxy(const char * licenseProxy);

	/**
	 *   @brief To set the curl stall timeout value
	 *
	 *   @param[in] curl stall timeout value
	 */
	void SetDownloadStallTimeout(long stallTimeout);

	/**
	 *   @brief To set the curl download start timeout
	 *
	 *   @param[in] curl download start timeout
	 */
	void SetDownloadStartTimeout(long startTimeout);

	/**
	 *   @brief Set preferred subtitle language.
	 *
	 *   @param[in]  language - Language of text track.
	 *   @return void
	 */
	void SetPreferredSubtitleLanguage(const char* language);

	/*
	 *   @brief Setting the alternate contents' (Ads/blackouts) URL
	 *
	 *   @param[in] Adbreak's unique identifier.
	 *   @param[in] Individual Ad's id
	 *   @param[in] Ad URL
	 */
	void SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url);

	/**
	 *   @brief Set parallel playlist download config value.
	 *   @param[in] bValue - true if a/v playlist to be downloaded in parallel
	 *
	 *   @return void
	 */
	void SetParallelPlaylistDL(bool bValue);
	/**
	 *   @brief Set async tune configuration
	 *   @param[in] bValue - true if async tune enabled 
	 *
	 *   @return void
	*/
	void SetAsyncTuneConfig(bool bValue);

	/**
	 *   @brief Get async tune configuration
	 *
	 *   @return bool - true if async tune enabled
	*/
	bool GetAsyncTuneConfig();

	/**
	 *	@brief Set parallel playlist download config value for linear
	 *	@param[in] bValue - true if a/v playlist to be downloaded in parallel
	 *
	 *	@return void
	 */
	void SetParallelPlaylistRefresh(bool bValue);

	/**
	 *   @brief Set Westeros sink configuration
	 *   @param[in] bValue - true if westeros sink enabled
	 *
	 *   @return void
	*/
	void SetWesterosSinkConfig(bool bValue);

	/**
	 *	 @brief Set license caching
	 *	 @param[in] bValue - true/false to enable/disable license caching
	 *
	 *	 @return void
	 */
	void SetLicenseCaching(bool bValue);

	/**
         *       @brief Set Display resolution check
         *       @param[in] bValue - true/false to enable/disable profile filtering by display resoluton
         *
         *       @return void
         */
        void SetOutputResolutionCheck(bool bValue);

	/**
	 *   @brief Set Matching BaseUrl Config Configuration
	 *
	 *   @param[in] bValue - true if Matching BaseUrl enabled
	 *   @return void
	 */
	void SetMatchingBaseUrlConfig(bool bValue);

        /**
         *   @brief to configure URI parameters for fragment downloads
         *
         *   @param[in] bValue - default value: true
         *   @return void
         */
	void SetPropagateUriParameters(bool bValue);

        /**
         *   @brief to optionally configure simulated per-download network latency for negative testing
         *
         *   @param[in] DownloadDelayInMs - default value: zero
         *   @return void
         */
        void ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs);

	/**
	 *   @brief to configure ssl verify peer parameter
	 *
	 *   @param[in] bValue - default value: false
	 *   @return void
	 */
	void SetSslVerifyPeerConfig(bool bValue);

	/**
	 *	 @brief Configure New ABR Enable/Disable
	 *	 @param[in] bValue - true if new ABR enabled
	 *
	 *	 @return void
	 */
	void SetNewABRConfig(bool bValue);

	/**
	 *	 @brief Configure New AdBreaker Enable/Disable
	 *	 @param[in] bValue - true if new AdBreaker enabled
	 *
	 *	 @return void
	 */
	void SetNewAdBreakerConfig(bool bValue);

	/**
	 *   @brief Get available audio tracks.
	 *
	 *   @return std::string JSON formatted list of audio tracks
	 */
	std::string GetAvailableAudioTracks();

	/**
	 *   @brief Get available text tracks.
	 *
	 *   @return std::string JSON formatted list of text tracks
	 */
	std::string GetAvailableTextTracks();

	/*
	 *   @brief Get the video window co-ordinates
	 *
	 *   @return current video co-ordinates in x,y,w,h format
	 */
	std::string GetVideoRectangle();

	/**
	 *   @brief Set the application name which has created PlayerInstanceAAMP, for logging purposes
	 *
	 *   @return void
	 */
	void SetAppName(std::string name);

	/**
	 *   @brief Set optional preferred language list
	 *   @param[in] languageList - string with comma-delimited language list in ISO-639
	 *             from most to least preferred: "lang1,lang2". Set NULL to clear current list.
	 *
	 *   @return void
	 */
	 void SetPreferredLanguages(const char* languageList, const char *preferredRendition = NULL );

	/**
	 *   @brief Set audio track by audio parameters like language , rendition, codec etc..
	 *   @param[in][optional] language, rendition, codec, bitrate, channel 
	 *
	 *   @return void
	 */
	void SetAudioTrack(std::string language="", std::string rendition="", std::string codec="", unsigned int channel=0);

	/**
	 *   @brief Set optional preferred codec list
	 *   @param[in] codecList[] - string with array with codec list
	 *
	 *   @return void
	 */
	void SetPreferredCodec(const char *codecList);

	/**
	 *   @brief Set optional preferred rendition list
	 *   @param[in] renditionList - string with comma-delimited rendition list in ISO-639
	 *             from most to least preferred. Set NULL to clear current list.
	 *
	 *   @return void
	 */
	void SetPreferredRenditions(const char *renditionList);

	/**
	 *   @brief Get current preferred language list
	 *
	 *   @return const char* - current comma-delimited language list or NULL if not set
	 *
	 */
	 const char* GetPreferredLanguages();

	/**
	 *   @brief To set the vod-tune-event according to the player.
	 *
	 *   @param[in] preferred tune event type
	 */
	void SetTuneEventConfig(int tuneEventType);

	/**
	 *   @brief Set video rectangle property
	 *
	 *   @param[in] video rectangle property
	 */
	void EnableVideoRectangle(bool rectProperty);

	/*
	 * @brief Set profile ramp down limit.
	 *
	 */
	void SetRampDownLimit(int limit);

	/*
	 * @brief Set Initial profile ramp down limit.
	 *
	 */
	void SetInitRampdownLimit(int limit);

	/**
	 * @brief Set minimum bitrate value.
	 *
	 */
	void SetMinimumBitrate(long bitrate);

	/**
	 * @brief Set maximum bitrate value.
	 *
	 */
	void SetMaximumBitrate(long bitrate);

	/**
	 * @brief Set retry limit on Segment injection failure.
	 *
	 */
	void SetSegmentInjectFailCount(int value);

	/**
	 * @brief Set retry limit on Segment drm decryption failure.
	 *
	 */
	void SetSegmentDecryptFailCount(int value);

	/**
	 * @brief Set initial buffer duration in seconds
	 *
	 */
	void SetInitialBufferDuration(int durationSec);

	/**
	 *   @brief Enable/disable the native CC rendering feature
	 *
	 *   @param[in] enable - true for native CC rendering on
	 *   @return void
	 */
	void SetNativeCCRendering(bool enable);

	/**
	 *   @brief Set audio track
	 *
	 *   @param[in] trackId - index of audio track in available track list
	 *   @return void
	 */
	void SetAudioTrack(int trackId);

	/**
	 *   @brief Get current audio track index
	 *
	 *   @return int - index of current audio track in available track list
	 */
	int GetAudioTrack();

	/**
	 *   @brief Set text track
	 *
	 *   @param[in] trackId - index of text track in available track list
	 *   @return void
	 */
	void SetTextTrack(int trackId);

	/**
	 *   @brief Get current text track index
	 *
	 *   @return int - index of current text track in available track list
	 */
	int GetTextTrack();

	/**
	 *   @brief Set CC visibility on/off
	 *
	 *   @param[in] enabled - true for CC on, false otherwise
	 *   @return void
	 */
	void SetCCStatus(bool enabled);

	/**
	 *   @brief Set style options for text track rendering
	 *
	 *   @param[in] options - JSON formatted style options
	 *   @return void
	 */
	void SetTextStyle(const std::string &options);

	/**
	 *   @brief Get style options for text track rendering
	 *
	 *   @return std::string - JSON formatted style options
	 */
	std::string GetTextStyle();
	/**
	 * @brief Set Language Format
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
	 *       @brief Set Maximum Cache Size for storing playlist
	 *       @return void
	*/
	void SetMaxPlaylistCacheSize(int cacheSize);

	/**
	 *   @brief Enable seekable range values in progress event
	 *
	 *   @param[in] enabled - true if enabled
	 */
	void EnableSeekableRange(bool enabled);

	/**
	 *   @brief Enable video PTS reporting in progress event
	 *
	 *   @param[in] enabled - true if enabled
	 */
	void SetReportVideoPTS(bool enabled);

	/**
	 *	 @brief Disable Content Restrictions - unlock
	 *       @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 *       @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 *       @param[in] eventChange - disable restriction handling till next program event boundary
	 *
	 *	 @return void
	 */
	void DisableContentRestrictions(long grace, long time, bool eventChange);

	/**
	 *	 @brief Enable Content Restrictions - lock
	 *	 @return void
	 */
	void EnableContentRestrictions();

	/**
	 * @brief Enable/Disable async operation 
	 *
	 * @return void
	 */
	void AsyncStartStop();

	/**
	 *   @brief Enable/disable configuration to persist ABR profile over seek/SAP
	 *
	 *   @param[in] value - To enable/disable configuration
	 *   @return void
	 */
	void PersistBitRateOverSeek(bool value);

	/**
	 *   @brief To get the available bitrates for thumbnails.
	 *
	 *   @ret bitrate of thumbnail track.
	 */
	std::string GetAvailableThumbnailTracks(void);

	/**
	 *   @brief To set a preferred bitrate for thumbnail profile.
	 *
	 *   @param[in] preferred bitrate for thumbnail profile
	 */
	bool SetThumbnailTrack(int thumbIndex);

	/**
	 *   @brief To get preferred thumbnails for the duration.
	 *
	 *   @param[in] duration  for thumbnails
	 */
	std::string GetThumbnails(double sduration, double eduration);

	/**
	 *   @brief To set preferred paused state behavior
	 *
	 *   @param[in] int behavior
	 */
	void SetPausedBehavior(int behavior);
	/**
	* @brief InitAAMPConfig - Initialize the media player session with json config
	*/
	bool InitAAMPConfig(char *jsonStr);
	/**
	* @brief GetAAMPConfig - GetAamp Config as JSON string 
	*/
	std::string GetAAMPConfig();

	/**
         *   @brief To set UseAbsoluteTimeline for DASH
         *
         *   @param[in] bool enable/disable configuration
         */
	void SetUseAbsoluteTimeline(bool configState);

	/**
	 *   @brief Enable async operation and initialize resources
	 *
	 *   @return void
	 */
	void EnableAsyncOperation();
	/**
	 *   @brief To set the repairIframes flag
	 *
	 *   @param[in] bool enable/disable configuration
	 */
	void SetRepairIframes(bool configState);
	/**
	 *   @brief Set auxiliary language
	 *
	 *   @param[in] language - auxiliary language
	 *   @return void
	 */
	void SetAuxiliaryLanguage(const std::string &language);

	class PrivateInstanceAAMP *aamp;    /**< AAMP player's private instance */
	std::shared_ptr<PrivateInstanceAAMP> sp_aamp; /* shared pointer for aamp resource */

	AampConfig mConfig;
private:
	
	/**
	 *	 @brief Check given rate is valid.
	 *
	 *	 @param[in]  rate - Rate of playback.
	 *	 @retval return true if the given rate is valid.
	 */
	bool IsValidRate(int rate);

private:	

	/**
	 *   @brief Stop playback and release resources.
	 *
	 *   @param[in]  sendStateChangeEvent - true if state change events need to be sent for Stop operation
	 *   @return void
	 */
	void StopInternal(bool sendStateChangeEvent);

	StreamSink* mInternalStreamSink;    /**< Pointer to stream sink */
	void* mJSBinding_DL;                /**< Handle to AAMP plugin dynamic lib.  */
	static std::mutex mPrvAampMtx;      /**< Mutex to protect aamp instance in GetState() */
	bool mAsyncRunning;                 /**< Flag denotes if async mode is on or not */
};

#endif // MAINAAMP_H
