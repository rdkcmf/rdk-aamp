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
#include <stddef.h>
#include "vttCue.h"

/*! \mainpage
 *
 * \section intro_sec Introduction
 *
 * See PlayerInstanceAAMP for libaamp public C++ API's
 *
 */


/**
 * @brief Type of the events sending to the JSPP player.
 */
typedef enum
{
	AAMP_EVENT_TUNED = 1,           /**< Tune success*/
	AAMP_EVENT_TUNE_FAILED,         /**< Tune failure*/
	AAMP_EVENT_SPEED_CHANGED,       /**< Speed changed internally*/
	AAMP_EVENT_EOS,                 /**< End of stream*/
	AAMP_EVENT_PLAYLIST_INDEXED,    /**< Playlist downloaded and indexed*/
	AAMP_EVENT_PROGRESS,            /**< Progress event with playback stats. Report interval configurable */
	AAMP_EVENT_CC_HANDLE_RECEIVED,  /**< Sent when video decoder handle retrieved */
	AAMP_EVENT_JS_EVENT,            /**< Generic event generated by JavaScript binding */
	AAMP_EVENT_MEDIA_METADATA,      /**< Meta-data of asset currently playing*/
	AAMP_EVENT_ENTERING_LIVE,       /**< Event when live point reached*/
	AAMP_EVENT_BITRATE_CHANGED,     /**< Event when bitrate changes */
	AAMP_EVENT_TIMED_METADATA,      /**< Meta-data of a subscribed tag parsed from manifest*/
	AAMP_EVENT_STATE_CHANGED,       /**< Event when player state changes */
	AAMP_EVENT_SPEEDS_CHANGED,      /**< Event when supported playback speeds changes */
//Unified Video Engine API spec
	AAMP_EVENT_BUFFERING_CHANGED,   /**< Event when buffering starts/ends btw a playback*/
	AAMP_EVENT_DURATION_CHANGED,    /**< Event when duration changed */
	AAMP_EVENT_AUDIO_TRACKS_CHANGED,/**< Event when available audio tracks changes */
	AAMP_EVENT_TEXT_TRACKS_CHANGED, /**< Event when available test tracks changes */
	AAMP_EVENT_AD_BREAKS_CHANGED,   /**< Event when content/ad breaks changes */
	AAMP_EVENT_AD_STARTED,          /**< Ad playback started */
	AAMP_EVENT_AD_COMPLETED,        /**< Ad playback completed */
	AAMP_EVENT_DRM_METADATA,
	AAMP_EVENT_REPORT_ANOMALY,       /**< Playback Anomaly reporting */
	AAMP_EVENT_WEBVTT_CUE_DATA,     /**< WebVTT Cue data */
	AAMP_EVENT_REPORT_METRICS_DATA,       /**< AAMP VideoEnd info reporting */
	AAMP_EVENT_AD_RESOLVED,         /**< Ad fulfill status */
	AAMP_EVENT_AD_RESERVATION_START,/**< Adbreak playback starts */
	AAMP_EVENT_AD_RESERVATION_END,  /**< Adbreak playback ends */
	AAMP_EVENT_AD_PLACEMENT_START,  /**< Ad playback starts */
	AAMP_EVENT_AD_PLACEMENT_END,    /**< Ad playback ends */
	AAMP_EVENT_AD_PLACEMENT_ERROR,  /**< Ad playback error */
	AAMP_EVENT_AD_PLACEMENT_PROGRESS, /**< Ad playback progress */
	AAMP_MAX_NUM_EVENTS
} AAMPEventType;

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
 * @brief AAMP playback error codes
 */
typedef enum
{
	AAMP_TUNE_INIT_FAILED,                  /**< Tune failure due to initialization error*/
	AAMP_TUNE_MANIFEST_REQ_FAILED,          /**< Tune failure caused by manifest fetch failure*/
	AAMP_TUNE_AUTHORISATION_FAILURE,        /**< Not authorised to view the content*/
	AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE,    /**<  When fragment download fails for 5 consecutive fragments*/
	AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, /**< Unable to download init fragment*/
	AAMP_TUNE_UNTRACKED_DRM_ERROR,          /**<  DRM error*/
	AAMP_TUNE_DRM_INIT_FAILED,              /**< DRM initialization failure */
	AAMP_TUNE_DRM_DATA_BIND_FAILED,         /**< InitData binding with DRM failed */
	AAMP_TUNE_DRM_SESSIONID_EMPTY,			/**< DRM session ID empty */
	AAMP_TUNE_DRM_CHALLENGE_FAILED,         /**< DRM key request challenge generation failed */
	AAMP_TUNE_LICENCE_TIMEOUT,              /**< DRM license request timeout */
	AAMP_TUNE_LICENCE_REQUEST_FAILED,       /**< DRM license got invalid response */
	AAMP_TUNE_INVALID_DRM_KEY,              /**< DRM reporting invalid license key */
	AAMP_TUNE_UNSUPPORTED_STREAM_TYPE,      /**< Unsupported stream type */
	AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE,       /**< Unsupported audio type in manifest */
	AAMP_TUNE_FAILED_TO_GET_KEYID,          /**< Failed to parse key id from init data*/
	AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN,   /**< Failed to get session token from AuthService*/
	AAMP_TUNE_CORRUPT_DRM_DATA,             /**< DRM failure due to corrupt drm data, self heal might clear further errors*/
	AAMP_TUNE_CORRUPT_DRM_METADATA,         /**< DRM failure due to corrupt drm metadata in the stream*/
	AAMP_TUNE_DRM_DECRYPT_FAILED,           /**< DRM Decryption Failed for Fragments */
	AAMP_TUNE_GST_PIPELINE_ERROR,           /**< Playback failure due to error from GStreamer pipeline or associated plugins */
	AAMP_TUNE_PLAYBACK_STALLED,             /**< Playback was stalled due to valid fragments not available in playlist */
	AAMP_TUNE_CONTENT_NOT_FOUND,            /**< The resource was not found at the URL provided (HTTP 404) */
	AAMP_TUNE_DRM_KEY_UPDATE_FAILED,        /**< Failed to process DRM key, see the error code returned from Update() for more info */
	AAMP_TUNE_DEVICE_NOT_PROVISIONED,       /**< STB not provisioned/corrupted; need to re-provision. */
	AAMP_TUNE_HDCP_COMPLIANCE_ERROR,	/**< HDCP Compliance Check failure.Not compatible hdcp version for playback */
	AAMP_TUNE_INVALID_MANIFEST_FAILURE,     /**< Manifest is invalid */
	AAMP_TUNE_FAILED_PTS_ERROR,             /**< Playback failed due to PTS error */
	AAMP_TUNE_FAILURE_UNKNOWN               /**<  Unknown failure */
}AAMPTuneFailure;

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
 * @brief  Mapping all required status codes based on JS player requirement. These requirements may be
 * forced by psdk player.AAMP may not use all the statuses mentioned below:
 * Mainly required statuses - idle, initializing, initialized, preparing, prepared, playing, paused, seek, complete and error
 */
typedef enum
{
	eSTATE_IDLE,         /**< 0  - Player is idle */

	eSTATE_INITIALIZING, /**< 1  - Player is initializing a particular content */

	eSTATE_INITIALIZED,  /**< 2  - Player has initialized for a content successfully */

	eSTATE_PREPARING,    /**< 3  - Player is loading all associated resources */

	eSTATE_PREPARED,     /**< 4  - Player has loaded all associated resources successfully */

	eSTATE_BUFFERING,    /**< 5  - Player is in buffering state */

	eSTATE_PAUSED,       /**< 6  - Playback is paused */

	eSTATE_SEEKING,      /**< 7  - Seek is in progress */

	eSTATE_PLAYING,      /**< 8  - Playback is in progress */

	eSTATE_STOPPING,     /**< 9  - Player is stopping the playback */

	eSTATE_STOPPED,      /**< 10 - Player has stopped playback successfully */

	eSTATE_COMPLETE,     /**< 11 - Playback completed */

	eSTATE_ERROR,        /**< 12 - Error encountered and playback stopped */

	eSTATE_RELEASED      /**< 13 - Player has released all resources for playback */

} PrivAAMPState;

#define MAX_LANGUAGE_COUNT 4
#define MAX_LANGUAGE_TAG_LENGTH 4
#define MAX_ERROR_DESCRIPTION_LENGTH 128
#define AD_ID_LENGTH 32
#define MAX_BITRATE_COUNT 10
#define MAX_SUPPORTED_SPEED_COUNT 11 /* [-64, -32, -16, -4, -1, 0, 1, 4, 16, 32, 64] */
#define AAMP_NORMAL_PLAY_RATE 1 /** < Normal Play Rate */
#define MAX_ANOMALY_BUFF_SIZE   256
#define METRIC_UUID_BUFF_LEN  256

typedef enum E_MetricsDataType
{
	AAMP_DATA_NONE,
	AAMP_DATA_VIDEO_END
}MetricsDataType;


/**
 * @brief Structure of the AAMP events.
 */
struct AAMPEvent
{
	AAMPEventType type; 		/**< Event type */

	union
	{
		struct
		{
			int severity; /**< informative number indicates severity of msg, e.g Warning, Error, Trace etc */
			char msg[MAX_ANOMALY_BUFF_SIZE];
		} anomalyReport;


		struct
		{
			MetricsDataType type; 		/**< type of data , e.g AAMP_DATA_VIDEO_END for VideoEndEvent data */
			char metricUUID[METRIC_UUID_BUFF_LEN]; // unique session id passed during tune,
			char * data;   /**< data for event  */
		} metricsData;

		/**
		 * @brief Structure of the progress event data
		 */
		struct
		{
			double durationMiliseconds; /**< current size of time shift buffer */
			double positionMiliseconds; /**< current play/pause position relative to tune time - starts at zero) */
			float playbackSpeed;        /**< current trick speed (1.0 for normal play rate) */
			double startMiliseconds;    /**< time shift buffer start position (relative to tune time - starts at zero) */
			double endMiliseconds;      /**< time shift buffer end position (relative to tune time - starts at zero) */
			long long videoPTS; 		/**< Video Presentation 90 Khz time-stamp  */
		} progress;

		/**
		 * @brief Structure of the speed change event
		 */
		struct
		{
			int rate; /**< Playback rate */
		} speedChanged;

		/**
		 * @brief Structure of the bitrate change event
		 */
		struct
		{
			int time;                   /**< Playback time */
			long bitrate;                /**< Playback bitrate */
			char description[128];      /**< Description */
			int width;                  /**< Video width */
			int height;                 /**< Video height */
		} bitrateChanged;

		/**
		 * @brief Structure of the metadata event
		 */
		struct
		{
			long durationMiliseconds;                                       /**< Asset duration */
			int languageCount;                                              /**< Available language count */
			char languages[MAX_LANGUAGE_COUNT][MAX_LANGUAGE_TAG_LENGTH];    /**< Available languages */
			int bitrateCount;                                               /**< Available bitrate count */
			long bitrates[MAX_BITRATE_COUNT];                               /**< Available bitrates */
			int width;                                                      /**< Maximum video width */
			int height;                                                     /**< Maximum video height */
			bool hasDrm;                                                    /**< Drm enabled */
			int supportedSpeedCount;                                        /**< Supported playback speed count */
			int supportedSpeeds[MAX_SUPPORTED_SPEED_COUNT];                 /**< Supported playback speeds */
		} metadata;

		/**
		 * @brief Structure of the closed caption handle event
		 */
		struct
		{
			unsigned long handle;	/**< Closed caption handle */
		} ccHandle;

		/**
		 * @brief Structure of the timed metadata event
		 */
		struct
		{
			/*
			 * Temporary fix for DELIA-37985:
			 * szName    = additionalEventData[0]
			 * id        = additionalEventData[1]
			 * szContent = additionalEventData[2]
			 */
			const char* szName;         /**< Metadata name */
			const char* id;             /**< Id of the timedMetadata */
			double timeMilliseconds;    /**< Playback position - relative to tune time - starts at zero */
			double durationMilliSeconds;/**< Duration of the timed event. */
			const char* szContent;      /**< Metadata content */
		} timedMetadata;

		/**
		 * @brief Structure of the Java Script event
		 */
		struct
		{
			const char* szEventType;    /**< Event Type */
			void*  jsObject;            /**< Pointer to the Java Scipt Object */
		} jsEvent;

		/**
		 * @brief Structure of the media error event
		 */
		struct
		{
			AAMPTuneFailure failure;                            /**< Error Type */
			int code;                                           /**< Error code */
			char description[MAX_ERROR_DESCRIPTION_LENGTH];     /**< Error description */
			bool shouldRetry;                                   /**< If recovery on retry is possible */
		} mediaError;

		struct
		{
			AAMPTuneFailure failure;                            /**< Error Type */
			const char *accessStatus;
			int accessStatus_value;
			long responseCode;
		} dash_drmmetadata;

		/**
		 * @brief Structure of the player state changed event
		 */
		struct
		{
			PrivAAMPState state;        /**< Player state */
		} stateChanged;

		/**
		 * @brief Structure of the buffering changed event
		 */
		struct
		{
			bool buffering;            /**< true if buffering started, false otherwise */
		} bufferingChanged;

		/**
		 * @brief Structure of the supported speeds changed event
		 */
		struct
		{
			int supportedSpeedCount;                            /**< Supported playback speed count */
			int supportedSpeeds[MAX_SUPPORTED_SPEED_COUNT];     /**< Supported playback speeds */
		} speedsChanged;

		/**
		 *
		 */
		struct
		{
			VTTCue* cueData;
		} cue;

		/**
		 * @brief Structure for ad fulfill status event
		 */
		struct
		{
			bool resolveStatus;
			char adId[AD_ID_LENGTH];
			uint64_t startMS;
			uint64_t durationMs;
		} adResolved;

		/**
		 * @brief Structure for ad reservation events
		 */
		struct
		{
			char adBreakId[AD_ID_LENGTH];     /**< Reservation Id */
			uint64_t position;
		} adReservation;

		/**
		 * @brief Structure for ad placement events
		 */
		struct
		{
			char adId[AD_ID_LENGTH];     /**< Placement Id */
			uint32_t position;			/**<Ad Position relative to Reservation Start */
			uint32_t offset;			/**<Ad start offset */
			uint32_t duration;
			int errorCode;
		} adPlacement;
	} data;

	std::vector<std::string> additionalEventData;

	/**
	 * @brief AAMPEvent Constructor
	 */
	AAMPEvent() : type(), data(),additionalEventData()
	{

	}

	/**
	 * @brief AAMPEvent Constructor
	 * @param[in]  Event type
	 */
	AAMPEvent(AAMPEventType t) : type(t), data(),additionalEventData()
	{

	}

	/**
	 * @brief Copy Constructor
	 */
	AAMPEvent(const AAMPEvent &e): type(e.type), data(e.data),additionalEventData(e.additionalEventData)
	{
		if(AAMP_EVENT_TIMED_METADATA == type && additionalEventData.size() >= 3)
		{
			data.timedMetadata.szName = additionalEventData[0].c_str();
			data.timedMetadata.id = additionalEventData[1].c_str();
			data.timedMetadata.szContent = additionalEventData[2].c_str();
		}
	}

	/**
	 * @brief Overloaded assignment operator
	 */
	AAMPEvent& operator=(const AAMPEvent &e)
	{
		type = e.type;
		memcpy(&data, &(e.data), sizeof(data));
		additionalEventData = e.additionalEventData;
		if(AAMP_EVENT_TIMED_METADATA == type && additionalEventData.size() >= 3)
		{
			data.timedMetadata.szName = additionalEventData[0].c_str();
			data.timedMetadata.id = additionalEventData[1].c_str();
			data.timedMetadata.szContent = additionalEventData[2].c_str();
		}
		return *this;
	}
};

/**
 * @brief Abstract class for AAMP event listening
 */
class AAMPEventListener
{
public:

	/**
	 * @brief Method for sending event.
	 * @param[in] event - AAMPEvent data
	 */
	virtual void Event(const AAMPEvent& event) = 0;

	/**
	 * @brief AAMPEventListener destructor.
	 */
	virtual ~AAMPEventListener(){};
};

/**
 * @brief Media types
 */
enum MediaType
{
	eMEDIATYPE_VIDEO,               /**< Type video */
	eMEDIATYPE_AUDIO,               /**< Type audio */
	eMEDIATYPE_SUBTITLE,            /**< Type subtitle */
	eMEDIATYPE_MANIFEST,            /**< Type manifest */
	eMEDIATYPE_LICENCE,             /**< Type license */
	eMEDIATYPE_IFRAME,              /**< Type iframe */
	eMEDIATYPE_INIT_VIDEO,          /**< Type video init fragment */
	eMEDIATYPE_INIT_AUDIO,          /**< Type audio init fragment */
	eMEDIATYPE_INIT_SUBTITLE,          /**< Type audio init fragment */
	eMEDIATYPE_PLAYLIST_VIDEO,      /**< Type video playlist */
	eMEDIATYPE_PLAYLIST_AUDIO,      /**< Type audio playlist */
	eMEDIATYPE_PLAYLIST_SUBTITLE,	/**< Type subtitle playlist */
	eMEDIATYPE_PLAYLIST_IFRAME,		 /**< Type Iframe playlist */
	eMEDIATYPE_INIT_IFRAME,			/**< Type IFRAME init fragment */
	eMEDIATYPE_DEFAULT              /**< Type unknown */
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
	FORMAT_AUDIO_ES_ATMOS,   /**< ATMOS Audio stream */
	FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
	FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
	FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
	FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
	FORMAT_NONE             /**< Unknown Format */
};

/**
 * @brief Video zoom mode
 */
enum VideoZoomMode
{
	VIDEO_ZOOM_FULL,    /**< Video Zoom Enabled */
	VIDEO_ZOOM_NONE     /**< Video Zoom Disabled */
};

/**
 * @brief DRM system types
 */
enum DRMSystems
{
	eDRM_NONE,              /**< No DRM */
	eDRM_WideVine,          /**< Widevine */
	eDRM_PlayReady,         /**< Playready */
	eDRM_CONSEC_agnostic,   /**< CONSEC Agnostic DRM */
	eDRM_Adobe_Access,      /**< Adobe Access */
	eDRM_Vanilla_AES,       /**< Vanilla AES */
	eDRM_ClearKey,          /**< Clear key */
	eDRM_MAX_DRMSystems     /**< Drm system count */
};

using AdObject = std::pair<std::string, std::string>;

/**
 *  @brief Auth Token Failure codes
 */
enum AuthTokenErrors {
	eAUTHTOKEN_TOKEN_PARSE_ERROR = -1,
	eAUTHTOKEN_INVALID_STATUS_CODE = -2
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
	 *   @param[in]  bESChangeStatus - Flag to keep force configure the pipeline value
	 *   @return void
	 */
	virtual void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, bool bESChangeStatus)=0;

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
	virtual void Send( MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration)= 0;

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
	virtual void Send( MediaType mediaType, struct GrowableBuffer* buffer, double fpts, double fdts, double duration)= 0;

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
	 *   @return void
	 */
	virtual void Flush(double position = 0, int rate = AAMP_NORMAL_PLAY_RATE){}


	/**
	 *   @brief Enabled or disable playback pause
	 *
	 *   @param[in]  pause  Enable/Disable
	 *   @return true if content successfully paused
	 */
	virtual bool Pause(bool pause){ return true; }

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
	 * @retval true if PTS is changing
	 */
	virtual bool CheckForPTSChange() {return true;};

	/**
	 *   @brief Check whether cach is empty
	 *
	 *   @param[in]  mediaType - Media Type
	 *   @return true: empty, false: not empty
	 */
	virtual bool IsCacheEmpty(MediaType mediaType){ return true; };

	/**
	 *   @brief API to notify that fragment caching done
	 *
	 *   @return void
	 */
	virtual void NotifyFragmentCachingComplete(){};

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
	virtual void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len) {};

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
};


/**
 * @brief Player interface class for the JS pluggin.
 */
class PlayerInstanceAAMP
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
	 *
	 *   @param[in]  url - HTTP/HTTPS url to be played.
	 *   @param[in]  contentType - Content type of the asset
	 *   @return void
	 */
	void Tune(const char *mainManifestUrl, const char *contentType = NULL, bool bFirstAttempt = true, bool bFinalAttempt = false,const char *traceUUID = NULL);

	/**
	 *   @brief Stop playback and release resources.
	 *
	 */
	void Stop(void);

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
	 */
	void Seek(double secondsRelativeToTuneTime);

	/**
	 *   @brief Seek to live point.
	 *
	 */
	void SeekToLive(void);

	/**
	 *   @brief Set seek position and speed.
	 *
	 *   @param[in]  rate - Rate of playback.
	 *   @param[in]  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 */
	void SetRateAndSeek(int rate, double secondsRelativeToTuneTime);

	/**
	 *   @brief Register event handler.
	 *
	 *   @param[in]  eventListener - pointer to implementation of AAMPEventListener to receive events.
	 *   @return void
	 */
	void RegisterEvents(AAMPEventListener* eventListener);

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
	void AddEventListener(AAMPEventType eventType, AAMPEventListener* eventListener);

	/**
	 *   @brief Remove event listener for eventType.
	 *
	 *   @param[in]  eventType - type of event.
	 *   @param[in]  eventListener - listener to be removed for the eventType.
	 *   @return void
	 */
	void RemoveEventListener(AAMPEventType eventType, AAMPEventListener* eventListener);

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
	 *   @return char* - current audio language
	 */
	char* GetCurrentAudioLanguage();

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
	 *   @param[in]  subscribedTags - Value to be pased along with HTTP header.
	 *   @return void
	 */
	void AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue);

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
	 *   @brief Indicates if session token has to be used with license request or not.
	 *
	 *   @param[in]  isAnonymous - True if session token should be blank and false otherwise.
	 *   @return void
	 */
	void SetAnonymousRequest(bool isAnonymous);

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
	 *   @brief To set the network download timeout value.
	 *
	 *   @param[in] preferred timeout value
	 */
	void SetNetworkTimeout(long timeout);

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

	class PrivateInstanceAAMP *aamp;    /**< AAMP player's private instance */
private:
	StreamSink* mInternalStreamSink;    /**< Pointer to stream sink */
	void* mJSBinding_DL;                /**< Handle to AAMP plugin dynamic lib.  */
};

#endif // MAINAAMP_H
