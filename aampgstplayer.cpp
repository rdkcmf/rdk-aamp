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
 * @file aampgstplayer.cpp
 * @brief Gstreamer based player impl for AAMP
 */


#include "aampgstplayer.h"
#include "AampFnLogger.h"
#include "isobmffbuffer.h"
#include "AampUtils.h"
#include "AampGstUtils.h"
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h> // for sprintf
#include "priv_aamp.h"
#include <pthread.h>
#include <atomic>

#ifdef __APPLE__
	#include "gst/video/videooverlay.h"
	guintptr (*gCbgetWindowContentView)() = NULL;
#endif

#ifdef AAMP_MPD_DRM
#include "aampoutputprotection.h"
#endif

/**
 * @enum GstPlayFlags 
 * @brief Enum of configuration flags used by playbin
 */
typedef enum {
	GST_PLAY_FLAG_VIDEO = (1 << 0),             /**< value is 0x001 */
	GST_PLAY_FLAG_AUDIO = (1 << 1),             /**< value is 0x002 */
	GST_PLAY_FLAG_TEXT = (1 << 2),              /**< value is 0x004 */
	GST_PLAY_FLAG_VIS = (1 << 3),               /**< value is 0x008 */
	GST_PLAY_FLAG_SOFT_VOLUME = (1 << 4),       /**< value is 0x010 */
	GST_PLAY_FLAG_NATIVE_AUDIO = (1 << 5),      /**< value is 0x020 */
	GST_PLAY_FLAG_NATIVE_VIDEO = (1 << 6),      /**< value is 0x040 */
	GST_PLAY_FLAG_DOWNLOAD = (1 << 7),          /**< value is 0x080 */
	GST_PLAY_FLAG_BUFFERING = (1 << 8),         /**< value is 0x100 */
	GST_PLAY_FLAG_DEINTERLACE = (1 << 9),       /**< value is 0x200 */
	GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10) /**< value is 0x400 */
} GstPlayFlags;

//#define SUPPORT_MULTI_AUDIO
#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5

/*Playersinkbin events*/
#define GSTPLAYERSINKBIN_EVENT_HAVE_VIDEO 0x01
#define GSTPLAYERSINKBIN_EVENT_HAVE_AUDIO 0x02
#define GSTPLAYERSINKBIN_EVENT_FIRST_VIDEO_FRAME 0x03
#define GSTPLAYERSINKBIN_EVENT_FIRST_AUDIO_FRAME 0x04
#define GSTPLAYERSINKBIN_EVENT_ERROR_VIDEO_UNDERFLOW 0x06
#define GSTPLAYERSINKBIN_EVENT_ERROR_AUDIO_UNDERFLOW 0x07
#define GSTPLAYERSINKBIN_EVENT_ERROR_VIDEO_PTS 0x08
#define GSTPLAYERSINKBIN_EVENT_ERROR_AUDIO_PTS 0x09

#ifdef INTELCE
#define INPUT_GAIN_DB_MUTE  (gdouble)-145
#define INPUT_GAIN_DB_UNMUTE  (gdouble)0
#endif
#define DEFAULT_BUFFERING_TO_MS 10                       /**< TimeOut interval to check buffer fullness */
#if defined(REALTEKCE)
#define DEFAULT_BUFFERING_QUEUED_FRAMES_MIN (3)          /**< if the video decoder has this many queued frames start.. */
#define DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS 12         /**< Currently MAX FRAG DURATION + 2 per Realtek */
#else
#define DEFAULT_BUFFERING_QUEUED_FRAMES_MIN (5)          /**< if the video decoder has this many queued frames start.. even at 60fps, close to 100ms... */
#endif

#define DEFAULT_BUFFERING_MAX_MS (1000)                  /**< max buffering time */
#define DEFAULT_BUFFERING_MAX_CNT (DEFAULT_BUFFERING_MAX_MS/DEFAULT_BUFFERING_TO_MS)   /**< max buffering timeout count */
#define AAMP_MIN_PTS_UPDATE_INTERVAL 4000
#define AAMP_DELAY_BETWEEN_PTS_CHECK_FOR_EOS_ON_UNDERFLOW 500
#define BUFFERING_TIMEOUT_PRIORITY -70
#define AAMP_MIN_DECODE_ERROR_INTERVAL 10000
#define VIDEO_COORDINATES_SIZE 32

/**
 * @name gmapDecoderLoookUptable
 * 
 * @brief Decoder map list lookup table 
 * convert from codec to string map list of gstreamer 
 * component.
 */
static std::map <std::string, std::vector<std::string>> gmapDecoderLoookUptable = 
{
	{"ac-3", {"omxac3dec", "avdec_ac3", "avdec_ac3_fixed"}},
	{"ac-4", {"omxac4dec"}}
};

/**
 * @struct media_stream
 * @brief Holds stream(A/V) specific variables.
 */
struct media_stream
{
	GstElement *sinkbin;
	GstElement *source;
	StreamOutputFormat format;
	gboolean using_playersinkbin;
	bool flush;
	bool resetPosition;
	bool bufferUnderrun;
	bool eosReached;
	bool sourceConfigured;
	pthread_mutex_t sourceLock;
	uint32_t timeScale;
	int32_t trackId;

	media_stream() : sinkbin(NULL), source(NULL), format(FORMAT_INVALID),
			 using_playersinkbin(FALSE), flush(false), resetPosition(false),
			 bufferUnderrun(false), eosReached(false), sourceConfigured(false), sourceLock(PTHREAD_MUTEX_INITIALIZER)
			, timeScale(1), trackId(-1)
	{

	}
};

/**
 * @struct AAMPGstPlayerPriv
 * @brief Holds private variables of AAMPGstPlayer
 */
struct AAMPGstPlayerPriv
{
	AAMPGstPlayerPriv(const AAMPGstPlayerPriv&) = delete;
	AAMPGstPlayerPriv& operator=(const AAMPGstPlayerPriv&) = delete;

	media_stream stream[AAMP_TRACK_COUNT];
	GstElement *pipeline; 				/**< GstPipeline used for playback. */
	GstBus *bus;					/**< Bus for receiving GstEvents from pipeline. */
	int current_rate; 
	guint64 total_bytes;
	gint n_audio; 					/**< Number of audio tracks. */
	gint current_audio; 				/**< Offset of current audio track. */
	std::mutex TaskControlMutex; 			/**< For scheduling/de-scheduling or resetting async tasks/variables and timers */
	TaskControlData firstProgressCallbackIdleTask;
	guint periodicProgressCallbackIdleTaskId; 	/**< ID of timed handler created for notifying progress events. */
	guint bufferingTimeoutTimerId; 			/**< ID of timer handler created for buffering timeout. */
	GstElement *video_dec; 				/**< Video decoder used by pipeline. */
	GstElement *audio_dec; 				/**< Audio decoder used by pipeline. */
	GstElement *video_sink; 			/**< Video sink used by pipeline. */
	GstElement *audio_sink; 			/**< Audio sink used by pipeline. */
	GstElement *subtitle_sink; 			/**< Subtitle sink used by pipeline. */
#ifdef INTELCE_USE_VIDRENDSINK
	GstElement *video_pproc; 			/**< Video element used by pipeline.(only for Intel). */
#endif

	int rate; 					/**< Current playback rate. */
	double playbackrate; 				/**< playback rate in fractions */
	VideoZoomMode zoom; 				/**< Video-zoom setting. */
	bool videoMuted; 				/**< Video mute status. */
	bool audioMuted; 				/**< Audio mute status. */
	bool subtitleMuted; 				/**< Subtitle mute status. */
	double audioVolume; 				/**< Audio volume. */
	guint eosCallbackIdleTaskId; 			/**< ID of idle handler created for notifying EOS event. */
	std::atomic<bool> eosCallbackIdleTaskPending; 	/**< Set if any eos callback is pending. */
	bool firstFrameReceived; 			/**< Flag that denotes if first frame was notified. */
	char videoRectangle[VIDEO_COORDINATES_SIZE]; 	/**< Video-rectangle co-ordinates in format x,y,w,h. */
	bool pendingPlayState; 				/**< Flag that denotes if set pipeline to PLAYING state is pending. */
	bool decoderHandleNotified; 			/**< Flag that denotes if decoder handle was notified. */
	guint firstFrameCallbackIdleTaskId; 		/**< ID of idle handler created for notifying first frame event. */
	GstEvent *protectionEvent[AAMP_TRACK_COUNT]; 	/**< GstEvent holding the pssi data to be sent downstream. */
	std::atomic<bool> firstFrameCallbackIdleTaskPending; /**< Set if any first frame callback is pending. */
	bool using_westerossink; 			/**< true if westros sink is used as video sink */
	guint busWatchId;
	std::atomic<bool> eosSignalled; 		/**< Indicates if EOS has signaled */
	gboolean buffering_enabled; 			/**< enable buffering based on multiqueue */
	gboolean buffering_in_progress; 		/**< buffering is in progress */
	guint buffering_timeout_cnt;    		/**< make sure buffering_timout doesn't get stuck */
	GstState buffering_target_state; 		/**< the target state after buffering */
#ifdef INTELCE
	bool keepLastFrame; 				/**< Keep last frame over next pipeline delete/ create cycle */
#endif
	gint64 lastKnownPTS; 				/**< To store the PTS of last displayed video */
	long long ptsUpdatedTimeMS; 			/**< Timestamp when PTS was last updated */
	guint ptsCheckForEosOnUnderflowIdleTaskId; 	/**< ID of task to ensure video PTS is not moving before notifying EOS on underflow. */
	int numberOfVideoBuffersSent; 			/**< Number of video buffers sent to pipeline */
	gint64 segmentStart; 				/**< segment start value; required when qtdemux is enabled and restamping is disabled */
	GstQuery *positionQuery; 			/**< pointer that holds a position query object */
        GstQuery *durationQuery; 			/**< pointer that holds a duration query object */
	bool paused; 					/**< if pipeline is deliberately put in PAUSED state due to user interaction */
	GstState pipelineState; 			/**< current state of pipeline */
	guint firstVideoFrameDisplayedCallbackIdleTaskId; /**< ID of idle handler created for notifying state changed to Playing */
	std::atomic<bool> firstVideoFrameDisplayedCallbackIdleTaskPending; /**< Set if any state changed to Playing callback is pending. */
#if defined(REALTEKCE)
	bool firstTuneWithWesterosSinkOff; 		/**<  DELIA-33640: track if first tune was done for Realtekce build */
#endif
	long long decodeErrorMsgTimeMS; 		/**< Timestamp when decode error message last posted */
	int decodeErrorCBCount; 			/**< Total decode error cb received within thresold time */
	bool progressiveBufferingEnabled;
	bool progressiveBufferingStatus;
	bool forwardAudioBuffers; 			/**< flag denotes if audio buffers to be forwarded to aux pipeline */
	bool enableSEITimeCode;
	bool firstVideoFrameReceived; 			/**< flag that denotes if first video frame was notified. */
	bool firstAudioFrameReceived; 			/**< flag that denotes if first audio frame was notified */
	int  NumberOfTracks;	      			/**< Indicates the number of tracks */
	AAMPGstPlayerPriv() : pipeline(NULL), bus(NULL), current_rate(0),
			total_bytes(0), n_audio(0), current_audio(0), 
			periodicProgressCallbackIdleTaskId(AAMP_TASK_ID_INVALID),
			bufferingTimeoutTimerId(AAMP_TASK_ID_INVALID), video_dec(NULL), audio_dec(NULL),TaskControlMutex(),firstProgressCallbackIdleTask("FirstProgressCallback"),
			video_sink(NULL), audio_sink(NULL), subtitle_sink(NULL),
#ifdef INTELCE_USE_VIDRENDSINK
			video_pproc(NULL),
#endif
			rate(AAMP_NORMAL_PLAY_RATE), zoom(VIDEO_ZOOM_FULL), videoMuted(false), audioMuted(false), subtitleMuted(false),
			audioVolume(1.0), eosCallbackIdleTaskId(AAMP_TASK_ID_INVALID), eosCallbackIdleTaskPending(false),
			firstFrameReceived(false), pendingPlayState(false), decoderHandleNotified(false),
			firstFrameCallbackIdleTaskId(AAMP_TASK_ID_INVALID), firstFrameCallbackIdleTaskPending(false),
			using_westerossink(false), busWatchId(0), eosSignalled(false),
			buffering_enabled(FALSE), buffering_in_progress(FALSE), buffering_timeout_cnt(0),
			buffering_target_state(GST_STATE_NULL),
			playbackrate(AAMP_NORMAL_PLAY_RATE),
#ifdef INTELCE
			keepLastFrame(false),
#endif
			lastKnownPTS(0), ptsUpdatedTimeMS(0), ptsCheckForEosOnUnderflowIdleTaskId(AAMP_TASK_ID_INVALID),
			numberOfVideoBuffersSent(0), segmentStart(0), positionQuery(NULL), durationQuery(NULL),
			paused(false), pipelineState(GST_STATE_NULL), firstVideoFrameDisplayedCallbackIdleTaskId(AAMP_TASK_ID_INVALID),
			firstVideoFrameDisplayedCallbackIdleTaskPending(false),
#if defined(REALTEKCE)
			firstTuneWithWesterosSinkOff(false),
#endif
			decodeErrorMsgTimeMS(0), decodeErrorCBCount(0),
			progressiveBufferingEnabled(false), progressiveBufferingStatus(false)
			, forwardAudioBuffers (false), enableSEITimeCode(true),firstVideoFrameReceived(false),firstAudioFrameReceived(false),NumberOfTracks(0)
	{
		memset(videoRectangle, '\0', VIDEO_COORDINATES_SIZE);
#ifdef INTELCE
                strcpy(videoRectangle, "0,0,0,0");
#else
                /* DELIA-45366-default video scaling should take into account actual graphics
                 * resolution instead of assuming 1280x720.
                 * By default we where setting the resolution has 0,0,1280,720.
                 * For Full HD this default resolution will not scale to full size.
                 * So, we no need to set any default rectangle size here,
                 * since the video will display full screen, if a gstreamer pipeline is started
                 * using the westerossink connected using westeros compositor.
                 */
                strcpy(videoRectangle, "");
#endif
		for(int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			protectionEvent[i] = NULL;
		}
	}
	
};

static const char* GstPluginNamePR = "aampplayreadydecryptor";
static const char* GstPluginNameWV = "aampwidevinedecryptor";
static const char* GstPluginNameCK = "aampclearkeydecryptor";
static const char* GstPluginNameVMX = "aampverimatrixdecryptor";

/**
 * @brief Called from the mainloop when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval FALSE if the event source should be removed.
 */
static gboolean bus_message(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this);

/**
 * @brief Invoked synchronously when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval FALSE if the event source should be removed.
 */
static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this);

/**
 * @brief g_timeout callback to wait for buffering to change
 *        pipeline from paused->playing
 */
static gboolean buffering_timeout (gpointer data);

/** 
 * @brief check if elemement is instance (BCOM-3563)
 */
static void type_check_instance( const char * str, GstElement * elem);

/**
 * @brief wraps gst_element_set_state and adds log messages where applicable
 * @param[in] element the GstElement whose state is to be changed
 * @param[in] targetState the GstState to apply to element
 * @retval Result of the state change (from inner gst_element_set_state())
 */
static GstStateChangeReturn SetStateWithWarnings(GstElement *element, GstState targetState);

#define PLUGINS_TO_LOWER_RANK_MAX    2
const char *plugins_to_lower_rank[PLUGINS_TO_LOWER_RANK_MAX] = {
	"aacparse",
	"ac3parse",
};

/**
 * @brief AAMPGstPlayer Constructor
 */
AAMPGstPlayer::AAMPGstPlayer(AampLogManager *logObj, PrivateInstanceAAMP *aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
        , std::function< void(uint8_t *, int, int, int) > exportFrames
#endif
	) : mLogObj(logObj), aamp(NULL) , privateContext(NULL), mBufferingLock(), mProtectionLock(), PipelineSetToReady(false), trickTeardown(false)
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	, cbExportYUVFrame(NULL)
#endif
{
	FN_TRACE( __FUNCTION__ );
	privateContext = new AAMPGstPlayerPriv();
	if(privateContext)
	{
		this->aamp = aamp;

		pthread_mutex_init(&mBufferingLock, NULL);
		pthread_mutex_init(&mProtectionLock, NULL);
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
			pthread_mutex_init(&privateContext->stream[i].sourceLock, NULL);

#ifdef RENDER_FRAMES_IN_APP_CONTEXT
		this->cbExportYUVFrame = exportFrames;
#endif
		CreatePipeline();
	}
	else
	{
		AAMPLOG_WARN("privateContext  is null");  //CID:85372 - Null Returns
	}
}

/**
 * @brief AAMPGstPlayer Destructor
 */
AAMPGstPlayer::~AAMPGstPlayer()
{
	FN_TRACE( __FUNCTION__ );
	DestroyPipeline();
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		pthread_mutex_destroy(&privateContext->stream[i].sourceLock);
	SAFE_DELETE(privateContext);
	pthread_mutex_destroy(&mBufferingLock);
	pthread_mutex_destroy(&mProtectionLock);
}

/**
 *  @brief IdleTaskAdd - add an async/idle task in a thread safe manner, assuming it is not queued
 */
bool AAMPGstPlayer::IdleTaskAdd(TaskControlData& taskDetails, BackgroundTask funcPtr)
{
	FN_TRACE( __FUNCTION__ );
	bool ret = false;
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);

	if (0 == taskDetails.taskID)
	{
		taskDetails.taskIsPending = false;
		taskDetails.taskID = aamp->ScheduleAsyncTask(funcPtr, (void *)this);
		// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
		if(0 != taskDetails.taskID)
		{
			taskDetails.taskIsPending = true;
			ret = true;
			AAMPLOG_INFO("Task '%.50s' was added with ID = %d.", taskDetails.taskName.c_str(), taskDetails.taskID);
		}
		else
		{
			AAMPLOG_INFO("Task '%.50s' was not added or already ran.", taskDetails.taskName.c_str());
		}
	}
	else
	{
		AAMPLOG_WARN("Task '%.50s' was already pending.", taskDetails.taskName.c_str());
	}
	return ret;
}

/**
 *  @brief IdleTaskRemove - remove an async task in a thread safe manner, if it is queued
 */
bool AAMPGstPlayer::IdleTaskRemove(TaskControlData& taskDetails)
{
	FN_TRACE( __FUNCTION__ );
	bool ret = false;
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);

	if (0 != taskDetails.taskID)
	{
		AAMPLOG_INFO("AAMPGstPlayer: Remove task <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
		aamp->RemoveAsyncTask(taskDetails.taskID);
		taskDetails.taskID = 0;
		ret = true;
	}
	else
	{
		AAMPLOG_TRACE("AAMPGstPlayer: Task already removed <%.50s>, with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	taskDetails.taskIsPending = false;
	return ret;
}

/**
 * @brief IdleTaskClearFlags - clear async task id and pending flag in a thread safe manner
 *                             e.g. called when the task executes
 */
void AAMPGstPlayer::IdleTaskClearFlags(TaskControlData& taskDetails)
{
	FN_TRACE( __FUNCTION__ );
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);
	if ( 0 != taskDetails.taskID )
	{
		AAMPLOG_INFO("AAMPGstPlayer: Clear task control flags <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	else
	{
		AAMPLOG_TRACE("AAMPGstPlayer: Task control flags were already cleared <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	taskDetails.taskIsPending = false;
	taskDetails.taskID = 0;
}

/**
 *  @brief TimerAdd - add a new glib timer in thread safe manner
 */
void AAMPGstPlayer::TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName)
{
	FN_TRACE( __FUNCTION__ );
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);
	if (funcPtr && user_data)
	{
		if (0 == taskId)
		{
			taskId = g_timeout_add(repeatTimeout, funcPtr, user_data);
			AAMPLOG_INFO("AAMPGstPlayer: Added timer '%.50s', %d", (nullptr!=timerName) ? timerName : "unknown" , taskId);
		}
		else
		{
			AAMPLOG_INFO("AAMPGstPlayer: Timer '%.50s' already added, taskId=%d", (nullptr!=timerName) ? timerName : "unknown", taskId);
		}
	}
	else
	{
		AAMPLOG_ERR("Bad pointer. funcPtr = %p, user_data=%p");
	}
}

/**
 *  @brief TimerRemove - remove a glib timer in thread safe manner, if it exists
 */
void AAMPGstPlayer::TimerRemove(guint& taskId, const char* timerName)
{
	FN_TRACE( __FUNCTION__ );
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);
	if ( 0 != taskId )
	{
		AAMPLOG_INFO("AAMPGstPlayer: Remove timer '%.50s', %d", (nullptr!=timerName) ? timerName : "unknown", taskId);
		g_source_remove(taskId);
		taskId = 0;
	}
	else
	{
		AAMPLOG_TRACE("Timer '%.50s' with taskId = %d already removed.", (nullptr!=timerName) ? timerName : "unknown", taskId);
	}
}

/**
 *  @brief TimerIsRunning - Check whether timer is currently running
 */
bool AAMPGstPlayer::TimerIsRunning(guint& taskId)
{
	FN_TRACE( __FUNCTION__ );
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);

	return !(0 == privateContext->periodicProgressCallbackIdleTaskId);
}

/**
 * @brief Analyze stream info from the GstPipeline
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void analyze_streams(AAMPGstPlayer *_this)
{
	FN_TRACE( __FUNCTION__ );
#ifdef SUPPORT_MULTI_AUDIO
	GstElement *sinkbin = _this->privateContext->stream[eMEDIATYPE_VIDEO].sinkbin;

	g_object_get(sinkbin, "n-audio", &_this->privateContext->n_audio, NULL);
	g_print("audio:\n");
	for (gint i = 0; i < _this->privateContext->n_audio; i++)
	{
		GstTagList *tags = NULL;
		g_signal_emit_by_name(sinkbin, "get-audio-tags", i, &tags);
		if (tags)
		{
			gchar *str;
			guint rate;

			g_print("audio stream %d:\n", i);
			if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &str)) {
				g_print("  codec: %s\n", str);
				g_free(str);
			}
			if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
				g_print("  language: %s\n", str);
				g_free(str);
			}
			if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate)) {
				g_print("  bitrate: %d\n", rate);
			}
			gst_tag_list_free(tags);
		}
	}
	g_object_get(sinkbin, "current-audio", &_this->privateContext->current_audio, NULL);
#endif
}


/**
 * @brief Callback for appsrc "need-data" signal
 * @param[in] source pointer to appsrc instance triggering "need-data" signal
 * @param[in] size size of data required
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static void need_data(GstElement *source, guint size, AAMPGstPlayer * _this)
{
 	if (source == _this->privateContext->stream[eMEDIATYPE_SUBTITLE].source)
	{
		_this->aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE); // signal fragment downloader thread
	}
	else if (source == _this->privateContext->stream[eMEDIATYPE_AUDIO].source)
	{
		_this->aamp->ResumeTrackDownloads(eMEDIATYPE_AUDIO); // signal fragment downloader thread
	}
	else if (source == _this->privateContext->stream[eMEDIATYPE_AUX_AUDIO].source)
	{
		_this->aamp->ResumeTrackDownloads(eMEDIATYPE_AUX_AUDIO); // signal fragment downloader thread
	}
        else
	{
		_this->aamp->ResumeTrackDownloads(eMEDIATYPE_VIDEO); // signal fragment downloader thread
	}
}


/**
 * @brief Callback for appsrc "enough-data" signal
 * @param[in] source pointer to appsrc instance triggering "enough-data" signal
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static void enough_data(GstElement *source, AAMPGstPlayer * _this)
{
	if (_this->aamp->DownloadsAreEnabled()) // avoid processing enough data if the downloads are already disabled.
	{
		if (source == _this->privateContext->stream[eMEDIATYPE_SUBTITLE].source)
		{
			_this->aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE); // signal fragment downloader thread
		}
		else if (source == _this->privateContext->stream[eMEDIATYPE_AUDIO].source)
		{
			_this->aamp->StopTrackDownloads(eMEDIATYPE_AUDIO); // signal fragment downloader thread
		}
		else if (source == _this->privateContext->stream[eMEDIATYPE_AUX_AUDIO].source)
		{
			_this->aamp->StopTrackDownloads(eMEDIATYPE_AUX_AUDIO); // signal fragment downloader thread
		}
		else
		{
			_this->aamp->StopTrackDownloads(eMEDIATYPE_VIDEO); // signal fragment downloader thread
		}
	}
}


/**
 * @brief Callback for appsrc "seek-data" signal
 * @param[in] src pointer to appsrc instance triggering "seek-data" signal 
 * @param[in] offset seek position offset
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static gboolean appsrc_seek(GstAppSrc *src, guint64 offset, AAMPGstPlayer * _this)
{
#ifdef TRACE
	AAMPLOG_WARN("appsrc %p seek-signal - offset %" G_GUINT64_FORMAT, src, offset);
#endif
	return TRUE;
}


/**
 * @brief Initialize properties/callback of appsrc
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 * @param[in] source pointer to appsrc instance to be initialized
 * @param[in] mediaType stream type
 */
static void InitializeSource(AAMPGstPlayer *_this, GObject *source, MediaType mediaType = eMEDIATYPE_VIDEO)
{
	media_stream *stream = &_this->privateContext->stream[mediaType];
	GstCaps * caps = NULL;
	g_signal_connect(source, "need-data", G_CALLBACK(need_data), _this);
	g_signal_connect(source, "enough-data", G_CALLBACK(enough_data), _this);
	g_signal_connect(source, "seek-data", G_CALLBACK(appsrc_seek), _this);
	gst_app_src_set_stream_type(GST_APP_SRC(source), GST_APP_STREAM_TYPE_SEEKABLE);
	if (eMEDIATYPE_VIDEO == mediaType )
	{
		int MaxGstVideoBufBytes = 0;
		_this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstVideoBufBytes,MaxGstVideoBufBytes);
		AAMPLOG_INFO("Setting gst Video buffer max bytes to %d", MaxGstVideoBufBytes);
		g_object_set(source, "max-bytes", MaxGstVideoBufBytes, NULL);
	}
	else if (eMEDIATYPE_AUDIO == mediaType || eMEDIATYPE_AUX_AUDIO == mediaType)
	{
		int MaxGstAudioBufBytes = 0;
                _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstAudioBufBytes,MaxGstAudioBufBytes);
		AAMPLOG_INFO("Setting gst Audio buffer max bytes to %d", MaxGstAudioBufBytes);
		g_object_set(source, "max-bytes", MaxGstAudioBufBytes, NULL);
	}
	g_object_set(source, "min-percent", 50, NULL);
	g_object_set(source, "format", GST_FORMAT_TIME, NULL);

	caps = GetGstCaps(stream->format);
	if (caps != NULL)
	{
		gst_app_src_set_caps(GST_APP_SRC(source), caps);
		gst_caps_unref(caps);
	}
	else
	{
		g_object_set(source, "typefind", TRUE, NULL);
	}
	stream->sourceConfigured = true;
}


/**
 * @brief Callback when source is added by playbin
 * @param[in] object a GstObject
 * @param[in] orig the object that originated the signal
 * @param[in] pspec the property that changed
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static void found_source(GObject * object, GObject * orig, GParamSpec * pspec, AAMPGstPlayer * _this )
{
	MediaType mediaType;
	media_stream *stream;
	if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_VIDEO].sinkbin))
	{
		AAMPLOG_WARN("Found source for video");
		mediaType = eMEDIATYPE_VIDEO;
	}
	else if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_AUDIO].sinkbin))
	{
		AAMPLOG_WARN("Found source for audio");
		mediaType = eMEDIATYPE_AUDIO;
	}
	else if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_AUX_AUDIO].sinkbin))
	{
		AAMPLOG_WARN("Found source for auxiliary audio");
		mediaType = eMEDIATYPE_AUX_AUDIO;
	}
	else if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_SUBTITLE].sinkbin))
	{
		AAMPLOG_WARN("Found source for subtitle");
		mediaType = eMEDIATYPE_SUBTITLE;
	}
	else
	{
		AAMPLOG_WARN("found_source didn't find a valid source");
	}

	stream = &_this->privateContext->stream[mediaType];
	g_object_get(orig, pspec->name, &stream->source, NULL);
	InitializeSource(_this, G_OBJECT(stream->source), mediaType);
}

static void httpsoup_source_setup (GstElement * element, GstElement * source, gpointer data)
{
	AAMPGstPlayer * _this = (AAMPGstPlayer *)data;

	if (!strcmp(GST_ELEMENT_NAME(source), "source"))
	{
		std::string networkProxyValue = _this->aamp->GetNetworkProxy();
		if(!networkProxyValue.empty())
		{
			g_object_set(source, "proxy", networkProxyValue.c_str(), NULL);
			AAMPLOG_WARN("httpsoup -> Set network proxy '%s'", networkProxyValue.c_str());
		}
	}
}


/**
 * @brief Idle callback to notify first frame rendered event
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallbackOnFirstFrame(gpointer user_data)
{
        AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		_this->aamp->NotifyFirstFrameReceived();
		_this->privateContext->firstFrameCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
		_this->privateContext->firstFrameCallbackIdleTaskPending = false;
	}
        return G_SOURCE_REMOVE;
}


/**
 * @brief Idle callback to notify end-of-stream event
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallbackOnEOS(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		AAMPLOG_WARN("eosCallbackIdleTaskId %d", _this->privateContext->eosCallbackIdleTaskId);
		_this->aamp->NotifyEOSReached();
		_this->privateContext->eosCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
		_this->privateContext->eosCallbackIdleTaskPending = false;
	}
	return G_SOURCE_REMOVE;
}



/**
 * @brief Timer's callback to notify playback progress event
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean ProgressCallbackOnTimeout(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		_this->aamp->ReportProgress();
		AAMPLOG_TRACE("current %d, stored %d ", g_source_get_id(g_main_current_source()), _this->privateContext->periodicProgressCallbackIdleTaskId);
	}
	return G_SOURCE_CONTINUE;
}


/**
 * @brief Idle callback to start progress notifier timer
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallback(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		// mAsyncTuneEnabled passed, because this could be called from Scheduler or main loop
		_this->aamp->ReportProgress();
		_this->IdleTaskClearFlags(_this->privateContext->firstProgressCallbackIdleTask);

		if ( !(_this->TimerIsRunning(_this->privateContext->periodicProgressCallbackIdleTaskId)) )
		{
			double  reportProgressInterval;
			_this->aamp->mConfig->GetConfigValue(eAAMPConfig_ReportProgressInterval,reportProgressInterval);
			reportProgressInterval *= 1000; //convert s to ms

			GSourceFunc timerFunc = ProgressCallbackOnTimeout;
			_this->TimerAdd(timerFunc, (int)reportProgressInterval, _this->privateContext->periodicProgressCallbackIdleTaskId, user_data, "periodicProgressCallbackIdleTask");
			AAMPLOG_WARN("current %d, periodicProgressCallbackIdleTaskId %d", g_source_get_id(g_main_current_source()), _this->privateContext->periodicProgressCallbackIdleTaskId);
		}
		else
		{
			AAMPLOG_INFO("Progress callback already available: periodicProgressCallbackIdleTaskId %d", _this->privateContext->periodicProgressCallbackIdleTaskId);
		}
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Idle callback to notify first video frame was displayed
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallbackFirstVideoFrameDisplayed(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		_this->aamp->NotifyFirstVideoFrameDisplayed();
		_this->privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending = false;
		_this->privateContext->firstVideoFrameDisplayedCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	return G_SOURCE_REMOVE;
}

/**
 *  @brief Notify first Audio and Video frame through an idle function to make the playersinkbin halding same as normal(playbin) playback.
 */
void AAMPGstPlayer::NotifyFirstFrame(MediaType type)
{
	FN_TRACE( __FUNCTION__ );
	// RDK-34481 :LogTuneComplete will be noticed after getting video first frame.
	// incase of audio or video only playback NumberofTracks =1, so in that case also LogTuneCompleted needs to captured when either audio/video frame received.
	if (!privateContext->firstFrameReceived && (privateContext->firstVideoFrameReceived
			|| (1 == privateContext->NumberOfTracks && (privateContext->firstAudioFrameReceived || privateContext->firstVideoFrameReceived))))
	{
		privateContext->firstFrameReceived = true;
		aamp->LogFirstFrame();
		aamp->LogTuneComplete();
		aamp->NotifyFirstBufferProcessed();
	}

	if (eMEDIATYPE_VIDEO == type)
	{
		AAMPLOG_WARN("AAMPGstPlayer_OnFirstVideoFrameCallback. got First Video Frame");

		// DELIA-42262: No additional checks added here, since the NotifyFirstFrame will be invoked only once
		// in westerossink disabled case until BCOM fixes it. Also aware of NotifyFirstBufferProcessed called
		// twice in this function, since it updates timestamp for calculating time elapsed, its trivial
		aamp->NotifyFirstBufferProcessed();

		if (!privateContext->decoderHandleNotified)
		{
			privateContext->decoderHandleNotified = true;
			privateContext->firstFrameCallbackIdleTaskPending = false;
			privateContext->firstFrameCallbackIdleTaskId = aamp->ScheduleAsyncTask(IdleCallbackOnFirstFrame, (void *)this, "FirstFrameCallback");
			// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true 
			if(privateContext->firstFrameCallbackIdleTaskId != AAMP_TASK_ID_INVALID)
			{
				privateContext->firstFrameCallbackIdleTaskPending = true;
			}
		}
		else if (PipelineSetToReady)
		{
			//If pipeline is set to ready forcefully due to change in track_id, then re-initialize CC 
			aamp->InitializeCC();
		}

		IdleTaskAdd(privateContext->firstProgressCallbackIdleTask, IdleCallback);

		if ( (!privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending)
				&& (aamp->IsFirstVideoFrameDisplayedRequired()) )
		{
			privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending = false;
			privateContext->firstVideoFrameDisplayedCallbackIdleTaskId =
				aamp->ScheduleAsyncTask(IdleCallbackFirstVideoFrameDisplayed, (void *)this, "FirstVideoFrameDisplayedCallback");
			if(privateContext->firstVideoFrameDisplayedCallbackIdleTaskId != AAMP_TASK_ID_INVALID)
			{
				privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending = true;
			}
		}
		PipelineSetToReady = false;
	}
	else if (eMEDIATYPE_AUDIO == type)
	{
		AAMPLOG_WARN("AAMPGstPlayer_OnAudioFirstFrameAudDecoder. got First Audio Frame");
		if (aamp->mAudioOnlyPb)
		{
			if (!privateContext->decoderHandleNotified)
			{
				privateContext->decoderHandleNotified = true;
				privateContext->firstFrameCallbackIdleTaskPending = false;
				privateContext->firstFrameCallbackIdleTaskId = aamp->ScheduleAsyncTask(IdleCallbackOnFirstFrame, (void *)this, "FirstFrameCallback");
				// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true 
				if(privateContext->firstFrameCallbackIdleTaskId != AAMP_TASK_ID_INVALID)
				{
					privateContext->firstFrameCallbackIdleTaskPending = true;
				}
			}
			IdleTaskAdd(privateContext->firstProgressCallbackIdleTask, IdleCallback);
		}
	}

}

/**
 * @brief Callback invoked after first video frame decoded
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnFirstVideoFrameCallback(GstElement* object, guint arg0, gpointer arg1,
	AAMPGstPlayer * _this)

{
	_this->privateContext->firstVideoFrameReceived = true;
	_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);

}

/**
 * @brief Callback invoked after receiving the SEI Time Code information
 * @param[in] object pointer to element raising the callback
 * @param[in] hours Hour value of the SEI Timecode
 * @param[in] minutes Minute value of the SEI Timecode
 * @param[in] seconds Second value of the SEI Timecode
 * @param[in] user_data pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_redButtonCallback(GstElement* object, guint hours, guint minutes, guint seconds, gpointer user_data)
{
       AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
       if (_this)
       {
               char buffer[16];
               snprintf(buffer,16,"%d:%d:%d",hours,minutes,seconds);
               _this->aamp->seiTimecode.assign(buffer);
       }
}

/**
 * @brief Callback invoked after first audio buffer decoded
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnAudioFirstFrameAudDecoder(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	_this->privateContext->firstAudioFrameReceived = true;
	_this->NotifyFirstFrame(eMEDIATYPE_AUDIO);
}

/**
 * @brief Check if gstreamer element is video decoder
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of the decoder
 */
bool AAMPGstPlayer_isVideoDecoder(const char* name, AAMPGstPlayer * _this)
{
#if defined (REALTEKCE)
	return (aamp_StartsWith(name, "omxwmvdec") || aamp_StartsWith(name, "omxh26")
				|| aamp_StartsWith(name, "omxav1dec") || aamp_StartsWith(name, "omxvp") || aamp_StartsWith(name, "omxmpeg"));
#else
	return (_this->privateContext->using_westerossink ? aamp_StartsWith(name, "westerossink"): aamp_StartsWith(name, "brcmvideodecoder"));
#endif
}

/**
 * @brief Check if gstreamer element is video sink
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of video sink
 */
bool AAMPGstPlayer_isVideoSink(const char* name, AAMPGstPlayer * _this)
{
#if defined (REALTEKCE)
	return (aamp_StartsWith(name, "westerossink") || aamp_StartsWith(name, "rtkv1sink"));
#else
	return	(!_this->privateContext->using_westerossink && aamp_StartsWith(name, "brcmvideosink") == true) || // brcmvideosink0, brcmvideosink1, ...
			( _this->privateContext->using_westerossink && aamp_StartsWith(name, "westerossink") == true);
#endif
}

bool AAMPGstPlayer_isAudioSinkOrAudioDecoder(const char* name, AAMPGstPlayer * _this)
{
#if defined (REALTEKCE)
	return (aamp_StartsWith(name, "rtkaudiosink")
						|| aamp_StartsWith(name, "alsasink")
						|| aamp_StartsWith(name, "fakesink"));
#else
	return (aamp_StartsWith(name, "brcmaudiodecoder") || aamp_StartsWith(name, "amlhalasink"));
#endif
}

/**
 * @brief Check if gstreamer element is audio decoder
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of audio or video decoder
 */
bool AAMPGstPlayer_isVideoOrAudioDecoder(const char* name, AAMPGstPlayer * _this)
{
	// The idea is to identify video or audio decoder plugin created at runtime by playbin and register to its first-frame/pts-error callbacks
	// This support is available in BCOM plugins in RDK builds and hence checking only for such plugin instances here
	// While using playersinkbin, these callbacks are supported via "event-callback" signal and hence not requried to do explicitly
	// For platforms that doesnt support callback, we use GST_STATE_PLAYING state change of playbin to notify first frame to app
	bool isAudioOrVideoDecoder = false;
	if (!_this->privateContext->stream[eMEDIATYPE_VIDEO].using_playersinkbin &&
	    !_this->privateContext->using_westerossink && aamp_StartsWith(name, "brcmvideodecoder"))
	{
		isAudioOrVideoDecoder = true;
	}
#if defined (REALTEKCE)
	else if (aamp_StartsWith(name, "omx"))
	{
		isAudioOrVideoDecoder = true;
	}
#else
	else if (_this->privateContext->using_westerossink && aamp_StartsWith(name, "westerossink"))
	{
		isAudioOrVideoDecoder = true;
	}
#endif
	else if (aamp_StartsWith(name, "brcmaudiodecoder"))
	{
		isAudioOrVideoDecoder = true;
	}


	return isAudioOrVideoDecoder;
}

/**
 * @brief Notifies EOS if video decoder pts is stalled
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean VideoDecoderPtsCheckerForEOS(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *) user_data;
	AAMPGstPlayerPriv *privateContext = _this->privateContext;
#ifndef INTELCE
	gint64 currentPTS = _this->GetVideoPTS();

	if (currentPTS == privateContext->lastKnownPTS)
	{
		AAMPLOG_WARN("PTS not changed");
		_this->NotifyEOS();
	}
	else
	{
		AAMPLOG_WARN("Video PTS still moving lastKnownPTS %" G_GUINT64_FORMAT " currentPTS %" G_GUINT64_FORMAT " ##", privateContext->lastKnownPTS, currentPTS);
	}
#endif
	privateContext->ptsCheckForEosOnUnderflowIdleTaskId = AAMP_TASK_ID_INVALID;
	return G_SOURCE_REMOVE;
}

#ifdef RENDER_FRAMES_IN_APP_CONTEXT

/**
 *  @brief Callback function to get video frames
 */
GstFlowReturn AAMPGstPlayer::AAMPGstPlayer_OnVideoSample(GstElement* object, AAMPGstPlayer * _this)
{
	FN_TRACE( __FUNCTION__ );
	using ::mLogObj; //To use global log object
	GstSample *sample;
	GstBuffer *buffer;
	GstMapInfo map;

	if(_this && _this->cbExportYUVFrame)
	{
		sample = gst_app_sink_pull_sample (GST_APP_SINK (object));
		if (sample)
		{
			int width, height;
			GstCaps *caps = gst_sample_get_caps(sample);
			GstStructure *capsStruct = gst_caps_get_structure(caps,0);
			gst_structure_get_int(capsStruct,"width",&width);
			gst_structure_get_int(capsStruct,"height",&height);
			//AAMPLOG_WARN("StrCAPS=%s\n", gst_caps_to_string(caps));
			buffer = gst_sample_get_buffer (sample);
			if (buffer)
			{
				if (gst_buffer_map(buffer, &map, GST_MAP_READ))
				{
					_this->cbExportYUVFrame(map.data, map.size, width, height);

					gst_buffer_unmap(buffer, &map);
				}
				else
				{
					AAMPLOG_WARN("buffer map failed\n");
				}
			}
			else
			{
				AAMPLOG_WARN("buffer NULL\n");
			}
			gst_sample_unref (sample);
		}
		else
		{
			AAMPLOG_WARN("sample NULL\n");
		}
	}
	return GST_FLOW_OK;
}
#endif

/**
 * @brief Callback invoked when facing an underflow
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnGstBufferUnderflowCb(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_DisableUnderflow))
	{ // optioonally ignore underflow
		AAMPLOG_WARN("##  [WARN] Ignored underflow from %s, disableUnderflow config enabled ##", GST_ELEMENT_NAME(object));
	}
	else
	{
		//TODO - Handle underflow
		MediaType type = eMEDIATYPE_DEFAULT;  //CID:89173 - Resolve Uninit
		AAMPGstPlayerPriv *privateContext = _this->privateContext;
#ifdef REALTEKCE
		if (AAMPGstPlayer_isVideoSink(GST_ELEMENT_NAME(object), _this))
#else
		if (AAMPGstPlayer_isVideoDecoder(GST_ELEMENT_NAME(object), _this))
#endif
		{
			type = eMEDIATYPE_VIDEO;
		}
		else if (AAMPGstPlayer_isAudioSinkOrAudioDecoder(GST_ELEMENT_NAME(object), _this))
		{
			type = eMEDIATYPE_AUDIO;
		}
		else
		{
			AAMPLOG_WARN("## WARNING!! Underflow message from %s not handled, unmapped underflow!", GST_ELEMENT_NAME(object));
			return;
		}

		AAMPLOG_WARN("## Got Underflow message from %s type %d ##", GST_ELEMENT_NAME(object), type);

		_this->privateContext->stream[type].bufferUnderrun = true;

		if ((_this->privateContext->stream[type].eosReached) && (_this->privateContext->rate > 0))
		{
			if (!privateContext->ptsCheckForEosOnUnderflowIdleTaskId)
			{
				privateContext->lastKnownPTS =_this->GetVideoPTS();
				privateContext->ptsUpdatedTimeMS = NOW_STEADY_TS_MS;
				privateContext->ptsCheckForEosOnUnderflowIdleTaskId = g_timeout_add(AAMP_DELAY_BETWEEN_PTS_CHECK_FOR_EOS_ON_UNDERFLOW, VideoDecoderPtsCheckerForEOS, _this);
			}
			else
			{
				AAMPLOG_WARN("ptsCheckForEosOnUnderflowIdleTask ID %d already running, ignore underflow", (int)privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
			}
		}
		else
		{
			AAMPLOG_WARN("Mediatype %d underrun, when eosReached is %d", type, _this->privateContext->stream[type].eosReached);
			_this->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, type);
		}
	}
}

/**
 * @brief Callback invoked a PTS error is encountered
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnGstPtsErrorCb(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	AAMPLOG_WARN("## Got PTS error message from %s ##", GST_ELEMENT_NAME(object));
#ifdef REALTEKCE
	if (AAMPGstPlayer_isVideoSink(GST_ELEMENT_NAME(object), _this))
#else
	if (AAMPGstPlayer_isVideoDecoder(GST_ELEMENT_NAME(object), _this))
#endif
	{
		_this->aamp->ScheduleRetune(eGST_ERROR_PTS, eMEDIATYPE_VIDEO);
	}
	else if (AAMPGstPlayer_isAudioSinkOrAudioDecoder(GST_ELEMENT_NAME(object), _this))
	{
		_this->aamp->ScheduleRetune(eGST_ERROR_PTS, eMEDIATYPE_AUDIO);
	}
}

/**
 * @brief Callback invoked a Decode error is encountered
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnGstDecodeErrorCb(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	long long deltaMS = NOW_STEADY_TS_MS - _this->privateContext->decodeErrorMsgTimeMS;
	_this->privateContext->decodeErrorCBCount += 1;
	if (deltaMS >= AAMP_MIN_DECODE_ERROR_INTERVAL)
	{
		_this->aamp->SendAnomalyEvent(ANOMALY_WARNING, "Decode Error Message Callback=%d time=%d",_this->privateContext->decodeErrorCBCount, AAMP_MIN_DECODE_ERROR_INTERVAL);
		_this->privateContext->decodeErrorMsgTimeMS = NOW_STEADY_TS_MS;
		AAMPLOG_WARN("## Got Decode Error message from %s ## total_cb=%d timeMs=%d", GST_ELEMENT_NAME(object),  _this->privateContext->decodeErrorCBCount, AAMP_MIN_DECODE_ERROR_INTERVAL);
		_this->privateContext->decodeErrorCBCount = 0;
	}
}

static gboolean buffering_timeout (gpointer data)
{
	AAMPGstPlayer * _this = (AAMPGstPlayer *) data;
	if (_this && _this->privateContext)
	{
		AAMPGstPlayerPriv * privateContext = _this->privateContext;
		if (_this->privateContext->buffering_in_progress)
		{
			int frames = -1;
			if (_this->privateContext->video_dec)
			{
				g_object_get(_this->privateContext->video_dec,"queued_frames",(uint*)&frames,NULL);				
				AAMPLOG_TRACE("queued_frames: %i", frames);
			}
			MediaFormat mediaFormatRet;
			mediaFormatRet = _this->aamp->GetMediaFormatTypeEnum();
			/* DELIA-34654: Disable re-tune on buffering timeout for DASH as unlike HLS,
			DRM key acquisition can end after injection, and buffering is not expected
			to be completed by the 1 second timeout
			*/
			if (G_UNLIKELY(((mediaFormatRet != eMEDIAFORMAT_DASH) && (mediaFormatRet != eMEDIAFORMAT_PROGRESSIVE) && (mediaFormatRet != eMEDIAFORMAT_HLS_MP4)) && (privateContext->buffering_timeout_cnt == 0) && _this->aamp->mConfig->IsConfigSet(eAAMPConfig_ReTuneOnBufferingTimeout) && (privateContext->numberOfVideoBuffersSent > 0)))
			{
				AAMPLOG_WARN("Schedule retune. numberOfVideoBuffersSent %d frames %i", privateContext->numberOfVideoBuffersSent, frames);
				privateContext->buffering_in_progress = false;
				_this->DumpDiagnostics();
				_this->aamp->ScheduleRetune(eGST_ERROR_VIDEO_BUFFERING, eMEDIATYPE_VIDEO);
			}

#if !defined(__APPLE__)
			else if (frames == -1 || frames > DEFAULT_BUFFERING_QUEUED_FRAMES_MIN || privateContext->buffering_timeout_cnt-- == 0)
#endif
			{
				AAMPLOG_WARN("Set pipeline state to %s - buffering_timeout_cnt %u  frames %i", gst_element_state_get_name(_this->privateContext->buffering_target_state), (_this->privateContext->buffering_timeout_cnt+1), frames);
				SetStateWithWarnings (_this->privateContext->pipeline, _this->privateContext->buffering_target_state);
				_this->privateContext->buffering_in_progress = false;
				if(!_this->aamp->mConfig->IsConfigSet(eAAMPConfig_GstSubtecEnabled))
				{
					_this->aamp->UpdateSubtitleTimestamp();
				}
			}
		}
		if (!_this->privateContext->buffering_in_progress)
		{
			//reset timer id after buffering operation is completed
			_this->privateContext->bufferingTimeoutTimerId = AAMP_TASK_ID_INVALID;
		}
		return _this->privateContext->buffering_in_progress;
	}
	else
	{
		AAMPLOG_WARN("in buffering_timeout got invalid or NULL handle ! _this =  %p   _this->privateContext = %p ",
		_this, (_this? _this->privateContext: NULL) );
		return false;
	}

}

/**
 * @brief Called from the mainloop when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval FALSE if the event source should be removed.
 */
static gboolean bus_message(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this)
{
	GError *error;
	gchar *dbg_info;
	bool isPlaybinStateChangeEvent;

	switch (GST_MESSAGE_TYPE(msg))
	{ // see https://developer.gnome.org/gstreamer/stable/gstreamer-GstMessage.html#GstMessage
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &error, &dbg_info);
		g_printerr("GST_MESSAGE_ERROR %s: %s\n", GST_OBJECT_NAME(msg->src), error->message);
		char errorDesc[MAX_ERROR_DESCRIPTION_LENGTH];
		memset(errorDesc, '\0', MAX_ERROR_DESCRIPTION_LENGTH);
		strncpy(errorDesc, "GstPipeline Error:", 18);
		strncat(errorDesc, error->message, MAX_ERROR_DESCRIPTION_LENGTH - 18 - 1);
		if (strstr(error->message, "video decode error") != NULL)
		{
			_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, errorDesc, false);
		}
		else if(strstr(error->message, "HDCP Compliance Check Failure") != NULL)
		{
			// Trying to play a 4K content on a non-4K TV .Report error to XRE with no retune
			_this->aamp->SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR, errorDesc, false);
		}
		else if (strstr(error->message, "Internal data stream error") && _this->aamp->mConfig->IsConfigSet(eAAMPConfig_RetuneForGSTError))
		{
			// This can be executed only for Peacock when it hits Internal data stream error.
			AAMPLOG_WARN("Schedule retune for GstPipeline Error");
			_this->aamp->ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO);
		}
		else
		{
			_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, errorDesc);
		}
		g_printerr("Debug Info: %s\n", (dbg_info) ? dbg_info : "none");
		g_clear_error(&error);
		g_free(dbg_info);
		break;

	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &error, &dbg_info);
		g_printerr("GST_MESSAGE_WARNING %s: %s\n", GST_OBJECT_NAME(msg->src), error->message);
		if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_DecoderUnavailableStrict)  && strstr(error->message, "No decoder available") != NULL)
		{
			char warnDesc[MAX_ERROR_DESCRIPTION_LENGTH];
			snprintf( warnDesc, MAX_ERROR_DESCRIPTION_LENGTH, "GstPipeline Error:%s", error->message );
			// decoding failures due to unsupported codecs are received as warnings, i.e.
			// "No decoder available for type 'video/x-gst-fourcc-av01"
			_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, warnDesc, false);
		}
		g_printerr("Debug Info: %s\n", (dbg_info) ? dbg_info : "none");
		g_clear_error(&error);
		g_free(dbg_info);
		break;
		
	case GST_MESSAGE_EOS:
		/**
		 * pipeline event: end-of-stream reached
		 * application may perform flushing seek to resume playback
		 */
		AAMPLOG_WARN("GST_MESSAGE_EOS");
		_this->NotifyEOS();
		break;

	case GST_MESSAGE_STATE_CHANGED:
		GstState old_state, new_state, pending_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

		isPlaybinStateChangeEvent = (GST_MESSAGE_SRC(msg) == GST_OBJECT(_this->privateContext->pipeline));

		if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_GSTLogging) || isPlaybinStateChangeEvent)
		{
			AAMPLOG_WARN("%s %s -> %s (pending %s)",
				GST_OBJECT_NAME(msg->src),
				gst_element_state_get_name(old_state),
				gst_element_state_get_name(new_state),
				gst_element_state_get_name(pending_state));

			if (isPlaybinStateChangeEvent && new_state == GST_STATE_PLAYING)
			{
				// progressive ff case, notify to update trickStartUTCMS
				if (_this->aamp->mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
				{
					_this->aamp->NotifyFirstBufferProcessed();
					_this->IdleTaskAdd(_this->privateContext->firstProgressCallbackIdleTask, IdleCallback);
				}
#if defined(REALTEKCE)
				// DELIA-33640: For Realtekce build and westeros-sink disabled
				// prevent calling NotifyFirstFrame after first tune, ie when upausing
				// pipeline during flush
				if(_this->privateContext->firstTuneWithWesterosSinkOff)
				{
					_this->privateContext->firstTuneWithWesterosSinkOff = false;
					_this->privateContext->firstVideoFrameReceived = true;
					_this->privateContext->firstAudioFrameReceived = true;
					_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);
				}
#endif
#if (defined(INTELCE) || defined(RPI) || defined(__APPLE__) || defined(UBUNTU))
				if(!_this->privateContext->firstFrameReceived)
				{
					_this->privateContext->firstFrameReceived = true;
					_this->aamp->LogFirstFrame();
					_this->aamp->LogTuneComplete();
				}
				_this->aamp->NotifyFirstFrameReceived();
				//Note: Progress event should be sent after the decoderAvailable event only.
				//BRCM platform sends progress event after AAMPGstPlayer_OnFirstVideoFrameCallback.
				_this->IdleTaskAdd(_this->privateContext->firstProgressCallbackIdleTask, IdleCallback);
#endif
				analyze_streams(_this);

				if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_GSTLogging))
				{
					GST_DEBUG_BIN_TO_DOT_FILE((GstBin *)_this->privateContext->pipeline, GST_DEBUG_GRAPH_SHOW_ALL, "myplayer");
					// output graph to .dot format which can be visualized with Graphviz tool if:
					// gstreamer is configured with --gst-enable-gst-debug
					// and "gst" is enabled in aamp.cfg
					// and environment variable GST_DEBUG_DUMP_DOT_DIR is set to a basepath(e.g. /opt).
				}

				// First Video Frame Displayed callback for westeros-sink is initialized
				// via OnFirstVideoFrameCallback()->NotifyFirstFrame() which is more accurate
				if( (!_this->privateContext->using_westerossink)
						&& (!_this->privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending)
						&& (_this->aamp->IsFirstVideoFrameDisplayedRequired()) )
				{
					_this->privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending = true;
					_this->privateContext->firstVideoFrameDisplayedCallbackIdleTaskId =
							_this->aamp->ScheduleAsyncTask(IdleCallbackFirstVideoFrameDisplayed, (void *)_this,"FirstVideoFrameDisplayedCallback");
				}
			}
		}
		if ((NULL != msg->src) && AAMPGstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), _this))
		{
#ifdef AAMP_MPD_DRM
			// This is the video decoder, send this to the output protection module
			// so it can get the source width/height
			if (AAMPGstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), _this))
			{
				if(AampOutputProtection::IsAampOutputProcectionInstanceActive())
				{
					AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();
					pInstance->setGstElement((GstElement *)(msg->src));
					pInstance->Release();
				}
			}
#endif
		}

        if ((NULL != msg->src) &&
#if defined(REALTEKCE)
            AAMPGstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), _this)
#else
            AAMPGstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), _this)
#endif
            )
	{
            if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
			{
				g_signal_connect(msg->src, "buffer-underflow-callback",
					G_CALLBACK(AAMPGstPlayer_OnGstBufferUnderflowCb), _this);
				g_signal_connect(msg->src, "pts-error-callback",
					G_CALLBACK(AAMPGstPlayer_OnGstPtsErrorCb), _this);
#if !defined(REALTEKCE)
				// To register decode-error-callback for video decoder source alone
				if (AAMPGstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), _this))
				{
					g_signal_connect(msg->src, "decode-error-callback",
						G_CALLBACK(AAMPGstPlayer_OnGstDecodeErrorCb), _this);
				}
#endif
			}
	}
		break;

	case GST_MESSAGE_TAG:
		break;

	case GST_MESSAGE_QOS:
	{
		gboolean live;
		guint64 running_time;
		guint64 stream_time;
		guint64 timestamp;
		guint64 duration;
		gst_message_parse_qos(msg, &live, &running_time, &stream_time, &timestamp, &duration);
		break;
	}

	case GST_MESSAGE_CLOCK_LOST:
		AAMPLOG_WARN("GST_MESSAGE_CLOCK_LOST");
		// get new clock - needed?
		SetStateWithWarnings(_this->privateContext->pipeline, GST_STATE_PAUSED);
		SetStateWithWarnings(_this->privateContext->pipeline, GST_STATE_PLAYING);
		break;

#ifdef TRACE
	case GST_MESSAGE_RESET_TIME:
		GstClockTime running_time;
		gst_message_parse_reset_time (msg, &running_time);
		printf("GST_MESSAGE_RESET_TIME %llu\n", (unsigned long long)running_time);
		break;
#endif

	case GST_MESSAGE_STREAM_STATUS:
	case GST_MESSAGE_ELEMENT: // can be used to collect pts, dts, pid
	case GST_MESSAGE_DURATION:
	case GST_MESSAGE_LATENCY:
	case GST_MESSAGE_NEW_CLOCK:
	case GST_MESSAGE_RESET_TIME:
		break;
	case GST_MESSAGE_APPLICATION:
		const GstStructure *msgS;
		msgS = gst_message_get_structure (msg);
		if (gst_structure_has_name (msgS, "HDCPProtectionFailure")) {
			AAMPLOG_WARN("Received HDCPProtectionFailure event.Schedule Retune ");
			_this->Flush(0, AAMP_NORMAL_PLAY_RATE, true);
			_this->aamp->ScheduleRetune(eGST_ERROR_OUTPUT_PROTECTION_ERROR,eMEDIATYPE_VIDEO);
		}
		break;
	case GST_MESSAGE_NEED_CONTEXT:
		AAMPLOG_WARN("Received GST_MESSAGE_NEED_CONTEXT (probably after a discontinuity between periods having different track_ids)");

		const gchar* contextType;
		gst_message_parse_context_type(msg, &contextType);
		if (!g_strcmp0(contextType, "drm-preferred-decryption-system-id"))
		{
			AAMPLOG_WARN("Setting %s as preferred drm",GetDrmSystemName(_this->aamp->GetPreferredDRM()));
			GstContext* context = gst_context_new("drm-preferred-decryption-system-id", FALSE);
			GstStructure* contextStructure = gst_context_writable_structure(context);
			gst_structure_set(contextStructure, "decryption-system-id", G_TYPE_STRING, GetDrmSystemID(_this->aamp->GetPreferredDRM()),  NULL);
			gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
		}
		else
		{
			AAMPLOG_ERR("unknown context type - %s ", contextType);
		}
		break;

	default:
		AAMPLOG_WARN("msg type: %s", gst_message_type_get_name(msg->type));
		break;
	}
	return TRUE;
}


/**
 * @brief Invoked synchronously when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval FALSE if the event source should be removed.
 */
static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this)
{
	switch(GST_MESSAGE_TYPE(msg))
	{
	case GST_MESSAGE_STATE_CHANGED:
		GstState old_state, new_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);

		if (GST_MESSAGE_SRC(msg) == GST_OBJECT(_this->privateContext->pipeline))
		{
			_this->privateContext->pipelineState = new_state;
		}

		/* Moved the below code block from bus_message() async handler to bus_sync_handler()
		 * to avoid a timing case crash when accessing wrong video_sink element after it got deleted during pipeline reconfigure on codec change in mid of playback.
		 */
		if (!_this->privateContext->stream[eMEDIATYPE_VIDEO].using_playersinkbin)
		{
#ifndef INTELCE
			if (new_state == GST_STATE_PAUSED && old_state == GST_STATE_READY)
			{
				if (AAMPGstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), _this))
				{ // video scaling patch
					/*
					brcmvideosink doesn't sets the rectangle property correct by default
					gst-inspect-1.0 brcmvideosink
					g_object_get(_this->privateContext->pipeline, "video-sink", &videoSink, NULL); - reports NULL
					note: alternate "window-set" works as well
					*/
					_this->privateContext->video_sink = (GstElement *) msg->src;
					if (_this->privateContext->using_westerossink && !_this->aamp->mConfig->IsConfigSet(eAAMPConfig_EnableRectPropertyCfg))
					{
						AAMPLOG_WARN("AAMPGstPlayer - using westerossink, setting cached video mute and zoom");
						g_object_set(msg->src, "zoom-mode", VIDEO_ZOOM_FULL == _this->privateContext->zoom ? 0 : 1, NULL);
						g_object_set(msg->src, "show-video-window", !_this->privateContext->videoMuted, NULL);
					}
					else
					{
						AAMPLOG_WARN("AAMPGstPlayer setting cached rectangle, video mute and zoom");
						g_object_set(msg->src, "rectangle", _this->privateContext->videoRectangle, NULL);
						g_object_set(msg->src, "zoom-mode", VIDEO_ZOOM_FULL == _this->privateContext->zoom ? 0 : 1, NULL);
						g_object_set(msg->src, "show-video-window", !_this->privateContext->videoMuted, NULL);
					}
				}
				else if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "brcmaudiosink") == true)
				{
					_this->privateContext->audio_sink = (GstElement *) msg->src;

					_this->setVolumeOrMuteUnMute();
				}
				else if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "amlhalasink") == true)
				{
					_this->privateContext->audio_sink = (GstElement *) msg->src;
					
					g_object_set(_this->privateContext->audio_sink, "disable-xrun", TRUE, NULL);
					// Apply audio settings that may have been set before pipeline was ready
					_this->setVolumeOrMuteUnMute();
				}
				else if (strstr(GST_OBJECT_NAME(msg->src), "brcmaudiodecoder"))
				{
					// this reduces amount of data in the fifo, which is flushed/lost when transition from expert to normal modes
					g_object_set(msg->src, "limit_buffering_ms", 1500, NULL);   /* default 500ms was a bit low.. try 1500ms */
					g_object_set(msg->src, "limit_buffering", 1, NULL);
					AAMPLOG_WARN("Found audiodecoder, limiting audio decoder buffering");

					/* if aamp->mAudioDecoderStreamSync==false, tell decoder not to look for 2nd/next frame sync, decode if it finds a single frame sync */
					g_object_set(msg->src, "stream_sync_mode", (_this->aamp->mAudioDecoderStreamSync)? 1 : 0, NULL);
					AAMPLOG_WARN("For audiodecoder set 'stream_sync_mode': %d", _this->aamp->mAudioDecoderStreamSync);
				}
#if defined (REALTEKCE)
				else if ( aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rtkaudiosink")
						|| aamp_StartsWith(GST_OBJECT_NAME(msg->src), "alsasink")
						|| aamp_StartsWith(GST_OBJECT_NAME(msg->src), "fakesink") )
				{
					_this->privateContext->audio_sink = (GstElement *) msg->src;
					// Apply audio settings that may have been set before pipeline was ready
					_this->setVolumeOrMuteUnMute();
				}
#endif

				StreamOutputFormat audFormat = _this->privateContext->stream[eMEDIATYPE_AUDIO].format;
			}
#endif
		}
		if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
		{
#ifndef INTELCE
			if ((NULL != msg->src) && AAMPGstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), _this))
			{
				if (AAMPGstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), _this))
				{
					_this->privateContext->video_dec = (GstElement *) msg->src;
					type_check_instance("bus_sync_handle: video_dec ", _this->privateContext->video_dec);
					g_signal_connect(_this->privateContext->video_dec, "first-video-frame-callback",
									G_CALLBACK(AAMPGstPlayer_OnFirstVideoFrameCallback), _this);
#if !defined(REALTEKCE)                               
                                        g_object_set(msg->src, "report_decode_errors", TRUE, NULL);
#endif

				}
				else
				{
					_this->privateContext->audio_dec = (GstElement *) msg->src;
					type_check_instance("bus_sync_handle: audio_dec ", _this->privateContext->audio_dec);
#if !defined(REALTEKCE)
					g_signal_connect(msg->src, "first-audio-frame-callback",
									G_CALLBACK(AAMPGstPlayer_OnAudioFirstFrameAudDecoder), _this);
#endif				
					int trackId = _this->privateContext->stream[eMEDIATYPE_AUDIO].trackId;
					if (trackId >= 0) /** AC4 track selected **/
					{
#if !defined(BRCM) /** AC4 support added for non Broadcom platforms */				
						AAMPLOG_INFO("Selecting AC4 Track Id : %d", trackId);
						g_object_set(msg->src, "ac4-presentation-group-index", trackId, NULL);
#else
						AAMPLOG_WARN("AC4 support has not done for this platform - track Id: %d", trackId);
#endif
					}
				}
			}
			if ((NULL != msg->src) && AAMPGstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), _this))
			{
				if(_this->privateContext->enableSEITimeCode)
				{
					g_object_set(msg->src, "enable-timecode", 1, NULL);
					g_signal_connect(msg->src, "timecode-callback",
									G_CALLBACK(AAMPGstPlayer_redButtonCallback), _this);
				}
#if !defined(REALTEKCE)
			}
#else
				g_object_set(msg->src, "freerun-threshold", DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS, NULL);
			}

			if ((NULL != msg->src) && aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rtkaudiosink"))
				g_signal_connect(msg->src, "first-audio-frame",
					G_CALLBACK(AAMPGstPlayer_OnAudioFirstFrameAudDecoder), _this);
#endif
#else
			if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "ismdgstaudiosink") == true)
			{
				_this->privateContext->audio_sink = (GstElement *) msg->src;

				AAMPLOG_WARN("AAMPGstPlayer setting audio-sync");
				g_object_set(msg->src, "sync", TRUE, NULL);

				_this->setVolumeOrMuteUnMute();
			}
			else
			{
#ifndef INTELCE_USE_VIDRENDSINK
				if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "ismdgstvidsink") == true)
#else
				if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "ismdgstvidrendsink") == true)
#endif
				{
					AAMPGstPlayerPriv *privateContext = _this->privateContext;
					privateContext->video_sink = (GstElement *) msg->src;
					AAMPLOG_WARN("AAMPGstPlayer setting stop-keep-frame %d", (int)(privateContext->keepLastFrame));
					g_object_set(msg->src, "stop-keep-frame", privateContext->keepLastFrame, NULL);
#if defined(INTELCE) && !defined(INTELCE_USE_VIDRENDSINK)
					AAMPLOG_WARN("AAMPGstPlayer setting rectangle %s", privateContext->videoRectangle);
					g_object_set(msg->src, "rectangle", privateContext->videoRectangle, NULL);
					AAMPLOG_WARN("AAMPGstPlayer setting zoom %s", (VIDEO_ZOOM_FULL == privateContext->zoom) ? "FULL" : "NONE");
					g_object_set(msg->src, "scale-mode", (VIDEO_ZOOM_FULL == privateContext->zoom) ? 0 : 3, NULL);
					AAMPLOG_WARN("AAMPGstPlayer setting crop-lines to FALSE");
					g_object_set(msg->src, "crop-lines", FALSE, NULL);
#endif
					AAMPLOG_WARN("AAMPGstPlayer setting video mute %d", privateContext->videoMuted);
					g_object_set(msg->src, "mute", privateContext->videoMuted, NULL);
				}
				else if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "ismdgsth264viddec") == true)
				{
					_this->privateContext->video_dec = (GstElement *) msg->src;
				}
#ifdef INTELCE_USE_VIDRENDSINK
				else if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "ismdgstvidpproc") == true)
				{
					_this->privateContext->video_pproc = (GstElement *) msg->src;
					AAMPLOG_WARN("AAMPGstPlayer setting rectangle %s", _this->privateContext->videoRectangle);
					g_object_set(msg->src, "rectangle", _this->privateContext->videoRectangle, NULL);
					AAMPLOG_WARN("AAMPGstPlayer setting zoom %d", _this->privateContext->zoom);
					g_object_set(msg->src, "scale-mode", (VIDEO_ZOOM_FULL == _this->privateContext->zoom) ? 0 : 3, NULL);
				}
#endif
			}
#endif
			/*This block is added to share the PrivateInstanceAAMP object
			  with PlayReadyDecryptor Plugin, for tune time profiling

			  AAMP is added as a property of playready plugin
			*/
			if(aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNamePR) == true ||
			   aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameWV) == true ||
			   aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameCK) == true ||
			   aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameVMX) == true) 
			{
				AAMPLOG_WARN("AAMPGstPlayer setting aamp instance for %s decryptor", GST_OBJECT_NAME(msg->src));
				GValue val = { 0, };
				g_value_init(&val, G_TYPE_POINTER);
				g_value_set_pointer(&val, (gpointer) _this->aamp);
				g_object_set_property(G_OBJECT(msg->src), "aamp",&val);
			}
		}
		break;
	case GST_MESSAGE_NEED_CONTEXT:
		
		/*
		 * Code to avoid logs flooding with NEED-CONTEXT message for DRM systems
		 */
		const gchar* contextType;
		gst_message_parse_context_type(msg, &contextType);
		if (!g_strcmp0(contextType, "drm-preferred-decryption-system-id"))
		{
			AAMPLOG_WARN("Setting %s as preferred drm",GetDrmSystemName(_this->aamp->GetPreferredDRM()));
			GstContext* context = gst_context_new("drm-preferred-decryption-system-id", FALSE);
			GstStructure* contextStructure = gst_context_writable_structure(context);
			gst_structure_set(contextStructure, "decryption-system-id", G_TYPE_STRING, GetDrmSystemID(_this->aamp->GetPreferredDRM()),  NULL);
			gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
/* TODO: Fix this once preferred DRM is correct 			
			_this->aamp->setCurrentDrm(_this->aamp->GetPreferredDRM());
 */
		}

		break;
#ifdef __APPLE__
    case GST_MESSAGE_ELEMENT:
                if (
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
		(nullptr == _this->cbExportYUVFrame) &&
#endif
			gCbgetWindowContentView && gst_is_video_overlay_prepare_window_handle_message(msg))
		{
			AAMPLOG_WARN("Received prepare-window-handle. Attaching video to window handle=%llu",(*gCbgetWindowContentView)());
			gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (msg)), (*gCbgetWindowContentView)());
		}
		break;
#endif
	case GST_MESSAGE_ASYNC_DONE:
		AAMPLOG_INFO("Received GST_MESSAGE_ASYNC_DONE message");
		if (_this->privateContext->buffering_in_progress)
		{
			_this->privateContext->bufferingTimeoutTimerId = g_timeout_add_full(BUFFERING_TIMEOUT_PRIORITY, DEFAULT_BUFFERING_TO_MS, buffering_timeout, _this, NULL);
		}

		break;

	default:
		break;
	}

	return GST_BUS_PASS;
}

/**
 *  @brief Create a new Gstreamer pipeline
 */
bool AAMPGstPlayer::CreatePipeline()
{
	FN_TRACE( __FUNCTION__ );
	bool ret = false;

	if (privateContext->pipeline || privateContext->bus)
	{
		DestroyPipeline();
	}

	/*DELIA-56028 - Each "Creating gstreamer pipeline" should be paired with one, subsequent "Destroying gstreamer pipeline" log entry.
	"Creating gstreamer pipeline" is intentionally placed after the DestroyPipeline() call above to maintain this sequence*/
	AAMPLOG_WARN("Creating gstreamer pipeline");
	privateContext->pipeline = gst_pipeline_new("AAMPGstPlayerPipeline");
	if (privateContext->pipeline)
	{
		privateContext->bus = gst_pipeline_get_bus(GST_PIPELINE(privateContext->pipeline));
		if (privateContext->bus)
		{
			privateContext->busWatchId = gst_bus_add_watch(privateContext->bus, (GstBusFunc) bus_message, this);
			gst_bus_set_sync_handler(privateContext->bus, (GstBusSyncHandler) bus_sync_handler, this, NULL);
			privateContext->buffering_enabled = ISCONFIGSET(eAAMPConfig_GStreamerBufferingBeforePlay);
			privateContext->buffering_in_progress = false;
			privateContext->buffering_timeout_cnt = DEFAULT_BUFFERING_MAX_CNT;
			privateContext->buffering_target_state = GST_STATE_NULL;
#ifdef INTELCE
			privateContext->buffering_enabled = false;
			AAMPLOG_WARN("%s buffering_enabled forced 0, INTELCE", GST_ELEMENT_NAME(privateContext->pipeline));
#else
			AAMPLOG_WARN("%s buffering_enabled %u", GST_ELEMENT_NAME(privateContext->pipeline), privateContext->buffering_enabled);
#endif
			if (privateContext->positionQuery == NULL)
			{
				privateContext->positionQuery = gst_query_new_position(GST_FORMAT_TIME);
			}
			privateContext->enableSEITimeCode = ISCONFIGSET(eAAMPConfig_SEITimeCode);
			ret = true;
		}
		else
		{
			AAMPLOG_WARN("AAMPGstPlayer - gst_pipeline_get_bus failed");
		}
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer - gst_pipeline_new failed");
	}

	return ret;
}

/**
 *  @brief Cleanup an existing Gstreamer pipeline and associated resources
 */
void AAMPGstPlayer::DestroyPipeline()
{
	FN_TRACE( __FUNCTION__ );
	if (privateContext->pipeline)
	{
		/*DELIA-56028 - "Destroying gstreamer pipeline" should only be logged when there is a pipeline to destroy
		  and each "Destroying gstreamer pipeline" log entry should have one, prior "Creating gstreamer pipeline" log entry*/
		AAMPLOG_WARN("Destroying gstreamer pipeline");
		gst_object_unref(privateContext->pipeline);
		privateContext->pipeline = NULL;
	}
	if (privateContext->busWatchId != 0)
	{
		g_source_remove(privateContext->busWatchId);
		privateContext->busWatchId = 0;
	}
	if (privateContext->bus)
	{
		gst_object_unref(privateContext->bus);
		privateContext->bus = NULL;
	}

	if (privateContext->positionQuery)
	{
		gst_query_unref(privateContext->positionQuery);
		privateContext->positionQuery = NULL;
	}

	//video decoder handle will change with new pipeline
	privateContext->decoderHandleNotified = false;
	privateContext->NumberOfTracks = 0;
}

/**
 *  @brief Retrieve the video decoder handle from pipeline
 */
unsigned long AAMPGstPlayer::getCCDecoderHandle()
{
	FN_TRACE( __FUNCTION__ );
	gpointer dec_handle = NULL;
	if (this->privateContext->stream[eMEDIATYPE_VIDEO].using_playersinkbin && this->privateContext->stream[eMEDIATYPE_VIDEO].sinkbin != NULL)
	{
		AAMPLOG_WARN("Querying playersinkbin for handle");
		g_object_get(this->privateContext->stream[eMEDIATYPE_VIDEO].sinkbin, "video-decode-handle", &dec_handle, NULL);
	}
	else if(this->privateContext->video_dec != NULL)
	{
		AAMPLOG_WARN("Querying video decoder for handle");
#ifndef INTELCE
#if defined (REALTEKCE)
		dec_handle = this->privateContext->video_dec;
#else
		g_object_get(this->privateContext->video_dec, "videodecoder", &dec_handle, NULL);
#endif
#else
		g_object_get(privateContext->video_dec, "decode-handle", &dec_handle, NULL);
#endif
	}
	AAMPLOG_WARN("video decoder handle received %p for video_dec %p", dec_handle, privateContext->video_dec);
	return (unsigned long)dec_handle;
}

/**
 *  @brief Generate a protection event
 */
void AAMPGstPlayer::QueueProtectionEvent(const char *protSystemId, const void *initData, size_t initDataSize, MediaType type)
{
	FN_TRACE( __FUNCTION__ );
#ifdef AAMP_MPD_DRM
	/* There is a possibility that only single protection event is queued for multiple type since they are encrypted using same id.
	 * Don't worry if you see only one protection event queued here.
	 */
	pthread_mutex_lock(&mProtectionLock);
	if (privateContext->protectionEvent[type] != NULL)
	{
		AAMPLOG_WARN("Previously cached protection event is present for type(%d), clearing!", type);
		gst_event_unref(privateContext->protectionEvent[type]);
		privateContext->protectionEvent[type] = NULL;
	}
	pthread_mutex_unlock(&mProtectionLock); 

	AAMPLOG_WARN("Queueing protection event for type(%d) keysystem(%s) initData(%p) initDataSize(%d)", type, protSystemId, initData, initDataSize);

	/* Giving invalid initData into ProtectionEvent causing "GStreamer-CRITICAL" assertion error. So if the initData is valid then its good to call the ProtectionEvent further. */
	if (initData && initDataSize)
	{
		GstBuffer *pssi;

		pssi = gst_buffer_new_wrapped(g_memdup (initData, initDataSize), initDataSize);
		pthread_mutex_lock(&mProtectionLock);
		if (this->aamp->IsDashAsset())
		{
			privateContext->protectionEvent[type] = gst_event_new_protection (protSystemId, pssi, "dash/mpd");
		}
		else
		{
			privateContext->protectionEvent[type] = gst_event_new_protection (protSystemId, pssi, "hls/m3u8");
		}
		pthread_mutex_unlock(&mProtectionLock);

		gst_buffer_unref (pssi);
	}
#endif
}

/**
 *  @brief Cleanup generated protection event
 */
void AAMPGstPlayer::ClearProtectionEvent()
{
	FN_TRACE( __FUNCTION__ );
	pthread_mutex_lock(&mProtectionLock);
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		if(privateContext->protectionEvent[i])
		{
			AAMPLOG_WARN("removing protection event! ");
			gst_event_unref (privateContext->protectionEvent[i]);
			privateContext->protectionEvent[i] = NULL;
		}
	}
	pthread_mutex_unlock(&mProtectionLock);
}

/**
 * @brief Callback for receiving playersinkbin gstreamer events
 * @param[in] playersinkbin instance of playersinkbin
 * @param[in] status event name
 * @param[in] arg user data (pointer to AAMPGstPlayer instance)
 */
static void AAMPGstPlayer_PlayersinkbinCB(GstElement * playersinkbin, gint status,  void* arg)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)arg;
	switch (status)
	{
		case GSTPLAYERSINKBIN_EVENT_HAVE_VIDEO:
			GST_INFO("got Video PES.\n");
			break;
		case GSTPLAYERSINKBIN_EVENT_HAVE_AUDIO:
			GST_INFO("got Audio PES\n");
			break;
		case GSTPLAYERSINKBIN_EVENT_FIRST_VIDEO_FRAME:
			GST_INFO("got First Video Frame\n");
			_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);
			break;
		case GSTPLAYERSINKBIN_EVENT_FIRST_AUDIO_FRAME:
			GST_INFO("got First Audio Sample\n");
			_this->NotifyFirstFrame(eMEDIATYPE_AUDIO);
			break;
		case GSTPLAYERSINKBIN_EVENT_ERROR_VIDEO_UNDERFLOW:
			//TODO - Handle underflow
			AAMPLOG_WARN("## Got Underflow message from video pipeline ##");
			break;
		case GSTPLAYERSINKBIN_EVENT_ERROR_AUDIO_UNDERFLOW:
			//TODO - Handle underflow
			AAMPLOG_WARN("## Got Underflow message from audio pipeline ##");
			break;
		case GSTPLAYERSINKBIN_EVENT_ERROR_VIDEO_PTS:
			//TODO - Handle PTS error
			AAMPLOG_WARN("## Got PTS error message from video pipeline ##");
			break;
		case GSTPLAYERSINKBIN_EVENT_ERROR_AUDIO_PTS:
			//TODO - Handle PTS error
			AAMPLOG_WARN("## Got PTS error message from audio pipeline ##");
			break;
		default:
			GST_INFO("%s status = 0x%x (Unknown)\n", __FUNCTION__, status);
			break;
	}
}


/**
 * @brief Create an appsrc element for a particular format
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @param[in] mediaType media type
 * @retval pointer to appsrc instance
 */
static GstElement* AAMPGstPlayer_GetAppSrc(AAMPGstPlayer *_this, MediaType mediaType)
{
	GstElement *source;
	source = gst_element_factory_make("appsrc", NULL);
	if (NULL == source)
	{
		AAMPLOG_WARN("AAMPGstPlayer_GetAppSrc Cannot create source");
		return NULL;
	}
	InitializeSource(_this, G_OBJECT(source), mediaType);

	if (eMEDIATYPE_SUBTITLE == mediaType)
	{
		auto stream_format = _this->privateContext->stream[eMEDIATYPE_SUBTITLE].format;

		if (stream_format == FORMAT_SUBTITLE_MP4)
		{
			AAMPLOG_INFO("Subtitle seeking first PTS %02f", _this->aamp->GetFirstPTS());
			gst_element_seek_simple(GST_ELEMENT(source), GST_FORMAT_TIME, GST_SEEK_FLAG_NONE, _this->aamp->GetFirstPTS() * GST_SECOND);
		}
	}
	return source;
}

/**
 *  @brief Cleanup resources and flags for a particular stream type
 */
void AAMPGstPlayer::TearDownStream(MediaType mediaType)
{
	FN_TRACE( __FUNCTION__ );
	media_stream* stream = &privateContext->stream[mediaType];
	stream->bufferUnderrun = false;
	stream->eosReached = false;
	stream->flush = false;
	if (stream->format != FORMAT_INVALID)
	{
		pthread_mutex_lock(&stream->sourceLock);
		if (privateContext->pipeline)
		{
			privateContext->buffering_in_progress = false;   /* stopping pipeline, don't want to change state if GST_MESSAGE_ASYNC_DONE message comes in */
			/* set the playbin state to NULL before detach it */
			if (stream->sinkbin)
			{
				if (GST_STATE_CHANGE_FAILURE == SetStateWithWarnings(GST_ELEMENT(stream->sinkbin), GST_STATE_NULL))
				{
					AAMPLOG_WARN("AAMPGstPlayer::TearDownStream: Failed to set NULL state for sinkbin");
				}
				if (!gst_bin_remove(GST_BIN(privateContext->pipeline), GST_ELEMENT(stream->sinkbin)))
				{
					AAMPLOG_WARN("AAMPGstPlayer::TearDownStream:  Unable to remove sinkbin from pipeline");
				}
			}
			else
			{
				AAMPLOG_WARN("AAMPGstPlayer::TearDownStream:  sinkbin = NULL, skip remove sinkbin from pipeline");
			}

			if (stream->using_playersinkbin && stream->source)
			{
				if (GST_STATE_CHANGE_FAILURE == SetStateWithWarnings(GST_ELEMENT(stream->source), GST_STATE_NULL))
				{
					AAMPLOG_WARN("AAMPGstPlayer::TearDownStream: Failed to set NULL state for source");
				}
				if (!gst_bin_remove(GST_BIN(privateContext->pipeline), GST_ELEMENT(stream->source)))
				{
					AAMPLOG_WARN("AAMPGstPlayer::TearDownStream:  Unable to remove source from pipeline");
				}
			}
		}
		//After sinkbin is removed from pipeline, a new decoder handle may be generated
		if (mediaType == eMEDIATYPE_VIDEO)
		{
			privateContext->decoderHandleNotified = false;
		}
		stream->format = FORMAT_INVALID;
		stream->sinkbin = NULL;
		stream->source = NULL;
		stream->sourceConfigured = false;
		pthread_mutex_unlock(&stream->sourceLock);
	}
	if (mediaType == eMEDIATYPE_VIDEO)
	{
		privateContext->video_dec = NULL;
#if !defined(INTELCE) || defined(INTELCE_USE_VIDRENDSINK)
		privateContext->video_sink = NULL;
#endif

#ifdef INTELCE_USE_VIDRENDSINK
		privateContext->video_pproc = NULL;
#endif
	}
	else if (mediaType == eMEDIATYPE_AUDIO)
	{
		privateContext->audio_dec = NULL;
		privateContext->audio_sink = NULL;
	}
	else if (mediaType == eMEDIATYPE_SUBTITLE)
	{
		privateContext->subtitle_sink = NULL;
	}
	AAMPLOG_WARN("AAMPGstPlayer::TearDownStream: exit mediaType = %d", mediaType);
}

#define NO_PLAYBIN 1
/**
 * @brief Setup pipeline for a particular stream type
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @param[in] streamId stream type
 * @retval 0, if setup successfully. -1, for failure
 */
static int AAMPGstPlayer_SetupStream(AAMPGstPlayer *_this, MediaType streamId)
{
	media_stream* stream = &_this->privateContext->stream[streamId];

	if (!stream->using_playersinkbin)
	{
		if (eMEDIATYPE_SUBTITLE == streamId)
		{
			if(_this->aamp->mConfig->IsConfigSet(eAAMPConfig_GstSubtecEnabled))
			{
#ifdef NO_PLAYBIN
				_this->aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
				AAMPLOG_INFO("AAMPGstPlayer_SetupStream - subs using subtecbin");
				stream->sinkbin = gst_element_factory_make("subtecbin", NULL);
				if (!stream->sinkbin)
				{
					AAMPLOG_WARN("Cannot set up subtitle subtecbin");
					return -1;
				}
				g_object_set(G_OBJECT(stream->sinkbin), "sync", FALSE, NULL);

				stream->source = AAMPGstPlayer_GetAppSrc(_this, eMEDIATYPE_SUBTITLE);
				gst_bin_add_many(GST_BIN(_this->privateContext->pipeline), stream->source, stream->sinkbin, NULL);

				if (!gst_element_link_many(stream->source, stream->sinkbin, NULL))
				{
					AAMPLOG_WARN("Failed to link subtitle elements");
					return -1;
				}

				gst_element_sync_state_with_parent(stream->source);
				gst_element_sync_state_with_parent(stream->sinkbin);
				_this->privateContext->subtitle_sink = stream->sinkbin;
				g_object_set(stream->sinkbin, "mute", _this->privateContext->subtitleMuted ? TRUE : FALSE, NULL);

				return 0;
#else
				AAMPLOG_INFO("AAMPGstPlayer_SetupStream - subs using playbin");
				stream->sinkbin = gst_element_factory_make("playbin", NULL);
				auto vipertransform = gst_element_factory_make("vipertransform", NULL);
				auto textsink = gst_element_factory_make("subtecsink", NULL);
				auto subtitlebin = gst_bin_new("subtitlebin");
				gst_bin_add_many(GST_BIN(subtitlebin), vipertransform, textsink, NULL);
				gst_element_link(vipertransform, textsink);
				gst_element_add_pad(subtitlebin, gst_ghost_pad_new("sink", gst_element_get_static_pad(vipertransform, "sink")));

				g_object_set(stream->sinkbin, "text-sink", subtitlebin, NULL);
#endif
			}
		}
		else
		{
			AAMPLOG_INFO("AAMPGstPlayer_SetupStream - using playbin");
			stream->sinkbin = gst_element_factory_make("playbin", NULL);
			if (_this->privateContext->using_westerossink && eMEDIATYPE_VIDEO == streamId)
			{
				AAMPLOG_INFO("AAMPGstPlayer_SetupStream - using westerossink");
				GstElement* vidsink = gst_element_factory_make("westerossink", NULL);
#if defined(BRCM) && defined(CONTENT_4K_SUPPORTED)
				g_object_set(vidsink, "secure-video", TRUE, NULL);
#endif
				g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);
			}
			else if (!_this->privateContext->using_westerossink && eMEDIATYPE_VIDEO == streamId)
			{
				GstElement* vidsink = gst_element_factory_make("brcmvideosink", NULL);
#if defined(BRCM) && defined(CONTENT_4K_SUPPORTED)
				g_object_set(vidsink, "secure-video", TRUE, NULL);
#endif
				g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);
			}
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
			//else if(_this->cbExportYUVFrame)
			{
				if (eMEDIATYPE_VIDEO == streamId)
				{
					AAMPLOG_WARN("AAMPGstPlayer_SetupStream - using appsink\n");
					GstElement* appsink = gst_element_factory_make("appsink", NULL);
					assert(appsink);
					GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", NULL);
					gst_app_sink_set_caps (GST_APP_SINK(appsink), caps);
					g_object_set (G_OBJECT (appsink), "emit-signals", TRUE, "sync", TRUE, NULL);
					g_signal_connect (appsink, "new-sample", G_CALLBACK (AAMPGstPlayer::AAMPGstPlayer_OnVideoSample), _this);
					g_object_set(stream->sinkbin, "video-sink", appsink, NULL);
					_this->privateContext->video_sink = appsink;
				}
			}
#endif
			if (eMEDIATYPE_AUX_AUDIO == streamId)
			{
				// We need to route audio through audsrvsink
				GstElement *audiosink = gst_element_factory_make("audsrvsink", NULL);
				g_object_set(audiosink, "session-type", 2, NULL );
				g_object_set(audiosink, "session-name", "btSAP", NULL );
				g_object_set(audiosink, "session-private", TRUE, NULL );
	
				g_object_set(stream->sinkbin, "audio-sink", audiosink, NULL);
				AAMPLOG_WARN("AAMPGstPlayer_SetupStream - using audsrvsink");
			}
		}
#if defined(INTELCE) && !defined(INTELCE_USE_VIDRENDSINK)
		if (eMEDIATYPE_VIDEO == streamId)
		{
			AAMPLOG_WARN("using ismd_vidsink");
			GstElement* vidsink = _this->privateContext->video_sink;
			if(NULL == vidsink)
			{
				vidsink = gst_element_factory_make("ismd_vidsink", NULL);
				if(!vidsink)
				{
					AAMPLOG_WARN("Could not create ismd_vidsink element");
				}
				else
				{
					_this->privateContext->video_sink = GST_ELEMENT(gst_object_ref( vidsink));
				}
			}
			else
			{
				AAMPLOG_WARN("Reusing existing vidsink element");
			}
			AAMPLOG_WARN("Set video-sink %p to playbin %p", vidsink, stream->sinkbin);
			g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);
		}
#endif
		gst_bin_add(GST_BIN(_this->privateContext->pipeline), stream->sinkbin);
		gint flags;
		g_object_get(stream->sinkbin, "flags", &flags, NULL);
		AAMPLOG_WARN("playbin flags1: 0x%x", flags); // 0x617 on settop
#if (defined(__APPLE__) || defined(NO_NATIVE_AV)) 
		flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_SOFT_VOLUME;;
#elif defined (REALTEKCE)
		flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO |  GST_PLAY_FLAG_NATIVE_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO | GST_PLAY_FLAG_SOFT_VOLUME;
#else
		flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO;
#endif
		if (eMEDIATYPE_SUBTITLE == streamId) flags = GST_PLAY_FLAG_TEXT;
		g_object_set(stream->sinkbin, "flags", flags, NULL); // needed?
		MediaFormat mediaFormat = _this->aamp->GetMediaFormatTypeEnum();
		if((mediaFormat != eMEDIAFORMAT_PROGRESSIVE) ||  _this->aamp->mConfig->IsConfigSet(eAAMPConfig_UseAppSrcForProgressivePlayback))
		{
			g_object_set(stream->sinkbin, "uri", "appsrc://", NULL);
			g_signal_connect(stream->sinkbin, "deep-notify::source", G_CALLBACK(found_source), _this);
		}
		else
		{
			g_object_set(stream->sinkbin, "uri", _this->aamp->GetManifestUrl().c_str(), NULL);
			g_signal_connect (stream->sinkbin, "source-setup", G_CALLBACK (httpsoup_source_setup), _this);
		}

#if defined(REALTEKCE)
		if (eMEDIATYPE_VIDEO == streamId && (mediaFormat==eMEDIAFORMAT_DASH || mediaFormat==eMEDIAFORMAT_HLS_MP4) )
		{ // enable multiqueue (Refer : XIONE-6138)
	                int MaxGstVideoBufBytes = 0;
			_this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstVideoBufBytes,MaxGstVideoBufBytes);
			AAMPLOG_INFO("Setting gst Video buffer size bytes to %d", MaxGstVideoBufBytes);
			g_object_set(stream->sinkbin, "buffer-size", (guint64)MaxGstVideoBufBytes, NULL);
			g_object_set(stream->sinkbin, "buffer-duration", 3000000000, NULL); //3000000000(ns), 3s
		}
#endif
		gst_element_sync_state_with_parent(stream->sinkbin);
	}
	else
	{
		//TODO: For auxiliary audio playback, when using playersinbin, we might have to set some additional
		// properties, need to check
		stream->source = AAMPGstPlayer_GetAppSrc(_this, streamId);
		gst_bin_add(GST_BIN(_this->privateContext->pipeline), stream->source);
		gst_element_sync_state_with_parent(stream->source);
		stream->sinkbin = gst_element_factory_make("playersinkbin", NULL);
		if (NULL == stream->sinkbin)
		{
			AAMPLOG_WARN("AAMPGstPlayer_SetupStream Cannot create sink");
			return -1;
		}
		g_signal_connect(stream->sinkbin, "event-callback", G_CALLBACK(AAMPGstPlayer_PlayersinkbinCB), _this);
		gst_bin_add(GST_BIN(_this->privateContext->pipeline), stream->sinkbin);
		gst_element_link(stream->source, stream->sinkbin);
		if(!gst_element_link(stream->source, stream->sinkbin))
		{
			AAMPLOG_WARN("gst_element_link  is error");  //CID:90331- checked return
		}
		gst_element_sync_state_with_parent(stream->sinkbin);

		AAMPLOG_WARN("AAMPGstPlayer_SetupStream:  Created playersinkbin. Setting rectangle");
		g_object_set(stream->sinkbin, "rectangle",  _this->privateContext->videoRectangle, NULL);
		g_object_set(stream->sinkbin, "zoom", _this->privateContext->zoom, NULL);
		g_object_set(stream->sinkbin, "video-mute", _this->privateContext->videoMuted, NULL);
		g_object_set(stream->sinkbin, "volume", _this->privateContext->audioVolume, NULL);
	}
	return 0;
}

/**
 *  @brief Send any pending/cached events to pipeline
 */
void AAMPGstPlayer::SendGstEvents(MediaType mediaType, GstClockTime pts)
{
	media_stream* stream = &privateContext->stream[mediaType];
	gboolean enableOverride = FALSE;
	GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
	if(stream->flush)
	{
		AAMPLOG_WARN("flush pipeline");
		gboolean ret = gst_pad_push_event(sourceEleSrcPad, gst_event_new_flush_start());
		if (!ret) AAMPLOG_WARN("flush start error");
		GstEvent* event = gst_event_new_flush_stop(FALSE);
		ret = gst_pad_push_event(sourceEleSrcPad, event);
		if (!ret) AAMPLOG_WARN("flush stop error");
		stream->flush = false;
	}

	if (stream->format == FORMAT_ISO_BMFF && mediaType != eMEDIATYPE_SUBTITLE)
	{
#ifdef ENABLE_AAMP_QTDEMUX_OVERRIDE
		enableOverride = TRUE;
#else
		enableOverride = (privateContext->rate != AAMP_NORMAL_PLAY_RATE);
#endif
		GstStructure * eventStruct = gst_structure_new("aamp_override", "enable", G_TYPE_BOOLEAN, enableOverride, "rate", G_TYPE_FLOAT, (float)privateContext->rate, "aampplayer", G_TYPE_BOOLEAN, TRUE, NULL);
#ifdef ENABLE_AAMP_QTDEMUX_OVERRIDE
		if ( privateContext->rate == AAMP_NORMAL_PLAY_RATE )
		{
			guint64 basePTS = aamp->GetFirstPTS() * GST_SECOND;
			AAMPLOG_WARN("Set override event's basePTS [ %" G_GUINT64_FORMAT "]", basePTS);
			gst_structure_set (eventStruct, "basePTS", G_TYPE_UINT64, basePTS, NULL);
		}
#endif
		if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
		{
			AAMPLOG_WARN("Error on sending rate override event");
		}
	}

	if (mediaType == eMEDIATYPE_VIDEO)
	{
		// DELIA-39530 - Westerossink gives position as an absolute value from segment.start. In AAMP's GStreamer pipeline
		// appsrc's base class - basesrc sends an additional segment event since we performed a flushing seek.
		// To figure out the new segment.start, we need to send a segment query which will be replied
		// by basesrc to get the updated segment event values.
		// When override is enabled qtdemux internally restamps and sends segment.start = 0 which is part of
		// AAMP's change in qtdemux so we don't need to query segment.start
		// Enabling position query based progress reporting for non-westerossink configurations
		if (ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery) && enableOverride == FALSE)
		{
			privateContext->segmentStart = -1;
		}
		else
		{
			privateContext->segmentStart = 0;
		}
	}

	if (stream->format == FORMAT_ISO_BMFF)
	{
		// There is a possibility that only single protection event is queued for multiple type
		// since they are encrypted using same id. Hence check if proection event is queued for
		// other types
		GstEvent* event = privateContext->protectionEvent[mediaType];
		if (event == NULL)
		{
			// Check protection event for other types
			for (int i = 0; i < AAMP_TRACK_COUNT; i++)
			{
				if (i != mediaType && privateContext->protectionEvent[i] != NULL)
				{
					event = privateContext->protectionEvent[i];
					break;
				}
			}
		}
		if(event)
		{
			AAMPLOG_WARN("pushing protection event! mediatype: %d", mediaType);
			if (!gst_pad_push_event(sourceEleSrcPad, gst_event_ref(event)))
			{
				AAMPLOG_WARN("push protection event failed!");
			}
		}
	}
#ifdef INTELCE
	if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, gst_structure_new("discard-segment-event-with-zero-start", "enable", G_TYPE_BOOLEAN, TRUE, NULL))))
	{
		AAMPLOG_WARN("Error on sending discard-segment-event-with-zero-start custom event");
	}
#endif

	gst_object_unref(sourceEleSrcPad);
	stream->resetPosition = false;
	stream->flush = false;
}


/**
 * @fn hasId3Header
 * @brief Check if segment starts with an ID3 section
 * @param[in] data pointer to segment buffer
 * @param[in] length length of segment buffer
 * @retval true if segment has an ID3 section
 */
bool hasId3Header(MediaType mediaType, const uint8_t* data, int32_t length)
{
	if ((mediaType == eMEDIATYPE_AUDIO || mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_DSM_CC)
		&& length >= 3)
	{
		/* Check file identifier ("ID3" = ID3v2) and major revision matches (>= ID3v2.2.x). */
		if (*data++ == 'I' && *data++ == 'D' && *data++ == '3' && *data++ >= 2)
		{
			return true;
		}
	}

	return false;
}

#define ID3_HEADER_SIZE 10

/**
 * @fn getId3TagSize
 * @brief Get the size of the ID3v2 tag.
 * @param[in] data buffer pointer
 */
uint32_t getId3TagSize(const uint8_t *data)
{
	uint32_t bufferSize = 0;
	uint8_t tagSize[4];

	memcpy(tagSize, data+6, 4);

	// bufferSize is encoded as a syncsafe integer - this means that bit 7 is always zeroed
	// Check for any 1s in bit 7
	if (tagSize[0] > 0x7f || tagSize[1] > 0x7f || tagSize[2] > 0x7f || tagSize[3] > 0x7f)
	{
		AAMPLOG_WARN("Bad header format");
		return 0;
	}

	bufferSize = tagSize[0] << 21;
	bufferSize += tagSize[1] << 14;
	bufferSize += tagSize[2] << 7;
	bufferSize += tagSize[3];
	bufferSize += ID3_HEADER_SIZE;

	return bufferSize;
}

/**
 *  @brief Send new segment event to pipeline
 */
void AAMPGstPlayer::SendNewSegmentEvent(MediaType mediaType, GstClockTime startPts ,GstClockTime stopPts)
{
        FN_TRACE( __FUNCTION__ );
        media_stream* stream = &privateContext->stream[mediaType];
        GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
        if (stream->format == FORMAT_ISO_BMFF)
        {
                GstSegment segment;
                gst_segment_init(&segment, GST_FORMAT_TIME);

                segment.start = startPts;
                segment.position = 0;
                segment.rate = AAMP_NORMAL_PLAY_RATE;
                segment.applied_rate = AAMP_NORMAL_PLAY_RATE;
                if(stopPts) segment.stop = stopPts;

#if defined(AMLOGIC)
                //AMLOGIC-2143 notify westerossink of rate to run in Vmaster mode
                if (mediaType == eMEDIATYPE_VIDEO)
                        segment.applied_rate = privateContext->rate;
#endif

                AAMPLOG_INFO("Sending segment event for mediaType[%d]. start %" G_GUINT64_FORMAT " stop %" G_GUINT64_FORMAT" rate %f applied_rate %f", mediaType, segment.start, segment.stop, segment.rate, segment.applied_rate);
                GstEvent* event = gst_event_new_segment (&segment);
                if (!gst_pad_push_event(sourceEleSrcPad, event))
                {
                        AAMPLOG_ERR("gst_pad_push_event segment error");
                }
        }
}

/**
 *  @brief Inject stream buffer to gstreamer pipeline
 */
bool AAMPGstPlayer::SendHelper(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration, bool copy, bool initFragment)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		if( privateContext->numberOfVideoBuffersSent == 0 )
		{ // required in order for subtitle harvesting/processing to work
			privateContext->numberOfVideoBuffersSent++;
			aamp->UpdateSubtitleTimestamp();
		}
		return false;
	}
	FN_TRACE( __FUNCTION__ );
	GstClockTime pts = (GstClockTime)(fpts * GST_SECOND);
	GstClockTime dts = (GstClockTime)(fdts * GST_SECOND);
	GstClockTime duration = (GstClockTime)(fDuration * 1000000000LL);

	if (aamp->IsEventListenerAvailable(AAMP_EVENT_ID3_METADATA) &&
		hasId3Header(mediaType, static_cast<const uint8_t*>(ptr), len))
	{
		uint32_t len = getId3TagSize(static_cast<const uint8_t*>(ptr));
		if (len && (len != aamp->lastId3DataLen[mediaType] ||
					!aamp->lastId3Data[mediaType] ||
					(memcmp(ptr, aamp->lastId3Data[mediaType], aamp->lastId3DataLen[mediaType]) != 0)))
		{
			AAMPLOG_INFO("AAMPGstPlayer: Found new ID3 frame");
			aamp->ReportID3Metadata(mediaType, static_cast<const uint8_t*>(ptr), len);
		}
	}

	// Ignore eMEDIATYPE_DSM_CC packets
	if(mediaType == eMEDIATYPE_DSM_CC)
	{
		return false;
	}

	media_stream *stream = &privateContext->stream[mediaType];
	bool isFirstBuffer = stream->resetPosition;
	
	// Make sure source element is present before data is injected
	// If format is FORMAT_INVALID, we don't know what we are doing here
	pthread_mutex_lock(&stream->sourceLock);

	if (!stream->sourceConfigured && stream->format != FORMAT_INVALID)
	{
		bool status = WaitForSourceSetup(mediaType);

		if (!aamp->DownloadsAreEnabled() || !status)
		{
			pthread_mutex_unlock(&stream->sourceLock);
			return false;
		}
	}
	if (isFirstBuffer)
	{
		//Send Gst Event when first buffer received after new tune, seek or period change
		SendGstEvents(mediaType, pts);

		if (mediaType == eMEDIATYPE_AUDIO && ForwardAudioBuffersToAux())
		{
			SendGstEvents(eMEDIATYPE_AUX_AUDIO, pts);
		}

#if defined(AMLOGIC)
		// AMLOGIC-3130: included to fix av sync / trickmode speed issues in LLAMA-4291
		// LLAMA-6788 - Also add check for trick-play on 1st frame.
		if(!aamp->mbNewSegmentEvtSent[mediaType] || (mediaType == eMEDIATYPE_VIDEO && aamp->rate != AAMP_NORMAL_PLAY_RATE))
		{
			SendNewSegmentEvent(mediaType, pts ,0);
			aamp->mbNewSegmentEvtSent[mediaType]=true;
		}
#endif

		AAMPLOG_TRACE("mediaType[%d] SendGstEvents - first buffer received !!! initFragment: %d", mediaType, initFragment);
	}


	if( aamp->DownloadsAreEnabled())
	{
		GstBuffer *buffer;
		bool bPushBuffer = true;

		if(copy)
		{
			buffer = gst_buffer_new_and_alloc((guint)len);

			if (buffer)
			{
				GstMapInfo map;
				gst_buffer_map(buffer, &map, GST_MAP_WRITE);
				memcpy(map.data, ptr, len);
				gst_buffer_unmap(buffer, &map);
				GST_BUFFER_PTS(buffer) = pts;
				GST_BUFFER_DTS(buffer) = dts;
				GST_BUFFER_DURATION(buffer) = duration;
				AAMPLOG_TRACE("Sending segment for mediaType[%d]. pts %" G_GUINT64_FORMAT " dts %" G_GUINT64_FORMAT" ", mediaType, pts, dts);
			}
			else
			{
				bPushBuffer = false;
			}
		}
		else
		{ // transfer
			buffer = gst_buffer_new_wrapped((gpointer)ptr,(gsize)len);

			if (buffer)
			{
				GST_BUFFER_PTS(buffer) = pts;
				GST_BUFFER_DTS(buffer) = dts;
				GST_BUFFER_DURATION(buffer) = duration;
				AAMPLOG_TRACE("Sending segment for mediaType[%d]. pts %" G_GUINT64_FORMAT " dts %" G_GUINT64_FORMAT" ", mediaType, pts, dts);
			}
			else
			{
				bPushBuffer = false;
			}
		}

		if (bPushBuffer)
		{
			if (mediaType == eMEDIATYPE_AUDIO && ForwardAudioBuffersToAux())
			{
				ForwardBuffersToAuxPipeline(buffer);
			}

			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(stream->source), buffer);

			if (ret != GST_FLOW_OK)
			{
				AAMPLOG_WARN("gst_app_src_push_buffer error: %d[%s] mediaType %d", ret, gst_flow_get_name (ret), (int)mediaType);
				assert(false);
			}
			else if (stream->bufferUnderrun)
			{
				stream->bufferUnderrun = false;
			}

			// PROFILE_BUCKET_FIRST_BUFFER after successfull push of first gst buffer
			if (isFirstBuffer == true && ret == GST_FLOW_OK)
				this->aamp->profiler.ProfilePerformed(PROFILE_BUCKET_FIRST_BUFFER);
		}
	}

	pthread_mutex_unlock(&stream->sourceLock);

	if (eMEDIATYPE_VIDEO == mediaType)
	{	
		// HACK!
		// DELIA-42262: For westerossink, it will send first-video-frame-callback signal after each flush
		// So we can move NotifyFirstBufferProcessed to the more accurate signal callback
		if (isFirstBuffer)
		{
			if (!privateContext->using_westerossink)
			{
				aamp->NotifyFirstBufferProcessed();
			}

#ifdef REALTEKCE // HACK: Have this hack until reakteck Westeros fixes missing first frame call back missing during trick play.
			aamp->ResetTrickStartUTCTime();
#endif
		}

		privateContext->numberOfVideoBuffersSent++;

		StopBuffering(false);
	}

	return true;
}

/**
 *  @brief inject HLS/ts elementary stream buffer to gstreamer pipeline
 */
void AAMPGstPlayer::SendCopy(MediaType mediaType, const void *ptr, size_t len0, double fpts, double fdts, double fDuration)
{
	FN_TRACE( __FUNCTION__ );
	SendHelper( mediaType, ptr, len0, fpts, fdts, fDuration, true /*copy*/ );
}

/**
 *  @brief inject mp4 segment to gstreamer pipeline
 */
void AAMPGstPlayer::SendTransfer(MediaType mediaType, GrowableBuffer* pBuffer, double fpts, double fdts, double fDuration, bool initFragment)
{
	FN_TRACE( __FUNCTION__ );
	if( !SendHelper( mediaType, pBuffer->ptr, pBuffer->len, fpts, fdts, fDuration, false /*transfer*/, initFragment) )
	{ // unable to transfer - free up the buffer we were passed.
		aamp_Free(pBuffer);
	}
	memset(pBuffer, 0x00, sizeof(GrowableBuffer));
}

/**
 * @brief To start playback
 */
void AAMPGstPlayer::Stream()
{
	FN_TRACE( __FUNCTION__ );
}


/**
 * @brief Configure pipeline based on A/V formats
 */
void AAMPGstPlayer::Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation)
{
	FN_TRACE( __FUNCTION__ );
	AAMPLOG_WARN("videoFormat %d audioFormat %d auxFormat %d subFormat %d",format, audioFormat, auxFormat, subFormat);
	StreamOutputFormat newFormat[AAMP_TRACK_COUNT];
	newFormat[eMEDIATYPE_VIDEO] = format;
	newFormat[eMEDIATYPE_AUDIO] = audioFormat;

	if(ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
	{
		newFormat[eMEDIATYPE_SUBTITLE] = subFormat;
		AAMPLOG_WARN("Gstreamer subs enabled");
	}
	else
	{
		newFormat[eMEDIATYPE_SUBTITLE]=FORMAT_INVALID;
		AAMPLOG_WARN("Gstreamer subs disabled");
	}

	if (forwardAudioToAux)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Override auxFormat %d -> %d", auxFormat, audioFormat);
		privateContext->forwardAudioBuffers = true;
		newFormat[eMEDIATYPE_AUX_AUDIO] = audioFormat;
	}
	else
	{
		privateContext->forwardAudioBuffers = false;
		newFormat[eMEDIATYPE_AUX_AUDIO] = auxFormat;
	}

	if (!ISCONFIGSET(eAAMPConfig_UseWesterosSink))
	{
		privateContext->using_westerossink = false;
#if defined(REALTEKCE)
		privateContext->firstTuneWithWesterosSinkOff = true;
#endif
	}
	else
	{
		privateContext->using_westerossink = true;
	}

#ifdef AAMP_STOP_SINK_ON_SEEK
	privateContext->rate = aamp->rate;
#endif

	if (privateContext->pipeline == NULL || privateContext->bus == NULL)
	{
		CreatePipeline();
	}

	if (setReadyAfterPipelineCreation)
	{
		if(SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_ERR("AAMPGstPlayer_Configure GST_STATE_READY failed on forceful set");
		}
		else
		{
			AAMPLOG_INFO("Forcefully set pipeline to ready state due to track_id change");
			PipelineSetToReady = true;
		}
	}

	bool configureStream[AAMP_TRACK_COUNT];
	memset(configureStream, 0, sizeof(configureStream));

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		media_stream *stream = &privateContext->stream[i];
		if (stream->format != newFormat[i])
		{
			if (newFormat[i] != FORMAT_INVALID)
			{
				AAMPLOG_WARN("Closing stream %d old format = %d, new format = %d",
								i, stream->format, newFormat[i]);
				configureStream[i] = true;
				privateContext->NumberOfTracks++;
			}
		}

		/* Force configure the bin for mid stream audio type change */
		if (!configureStream[i] && bESChangeStatus && (eMEDIATYPE_AUDIO == i))
		{
			AAMPLOG_WARN("AudioType Changed. Force configure pipeline");
			configureStream[i] = true;
		}

		stream->resetPosition = true;
		stream->eosReached = false;
	}

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		media_stream *stream = &privateContext->stream[i];
		
		if ((configureStream[i] && (newFormat[i] != FORMAT_INVALID)) || 
			/* Allow to create audio pipeline along with video pipeline if trickplay initiated before the pipeline going to play/paused state to fix unthrottled trickplay */
			(trickTeardown && (eMEDIATYPE_AUDIO == i)))
		{
			trickTeardown = false;
			TearDownStream((MediaType) i);
			stream->format = newFormat[i];
			stream->trackId = aamp->GetCurrentAudioTrackId();
	#ifdef USE_PLAYERSINKBIN
			if (FORMAT_MPEGTS == stream->format )
			{
				AAMPLOG_WARN("using playersinkbin, track = %d", i);
				stream->using_playersinkbin = TRUE;
			}
			else
	#endif
			{
				stream->using_playersinkbin = FALSE;
			}
			if (0 != AAMPGstPlayer_SetupStream(this, (MediaType)i))
			{
				AAMPLOG_WARN("AAMPGstPlayer: track %d failed", i);
				//Don't kill the tune for subtitles
				if (eMEDIATYPE_SUBTITLE != (MediaType)i)
				{
					return;
				}
			}
		}
	}

	if (this->privateContext->buffering_enabled && format != FORMAT_INVALID && AAMP_NORMAL_PLAY_RATE == privateContext->rate)
	{
		this->privateContext->buffering_target_state = GST_STATE_PLAYING;
		this->privateContext->buffering_in_progress = true;
		this->privateContext->buffering_timeout_cnt = DEFAULT_BUFFERING_MAX_CNT;
		if (SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_WARN("AAMPGstPlayer_Configure GST_STATE_PLAUSED failed");
		}
		privateContext->pendingPlayState = false;
	}
	else
	{
		if (SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_WARN("AAMPGstPlayer: GST_STATE_PLAYING failed");
		}
		privateContext->pendingPlayState = false;
	}
	privateContext->eosSignalled = false;
	privateContext->numberOfVideoBuffersSent = 0;
	privateContext->paused = false;
	privateContext->decodeErrorMsgTimeMS = 0;
	privateContext->decodeErrorCBCount = 0;
#ifdef TRACE
	AAMPLOG_WARN("exiting AAMPGstPlayer");
#endif
}


/**
 * @fn AAMPGstPlayer_SignalEOS
 * @brief To signal EOS to a particular appsrc instance
 * @param[in] source pointer to appsrc instance
 */
static void AAMPGstPlayer_SignalEOS(GstElement *source )
{
	if (source )
	{
		GstFlowReturn ret;
		g_signal_emit_by_name(source, "end-of-stream", &ret);
		if (ret != GST_FLOW_OK)
		{
			AAMPLOG_WARN("gst_app_src_push_buffer  error: %d", ret);
		}
	}
}

/**
 *  @brief Starts processing EOS for a particular stream type
 */
void AAMPGstPlayer::EndOfStreamReached(MediaType type)
{
	FN_TRACE( __FUNCTION__ );
	AAMPLOG_WARN("entering AAMPGstPlayer_EndOfStreamReached type %d", (int)type);

	media_stream *stream = &privateContext->stream[type];
	stream->eosReached = true;
	if ((stream->format != FORMAT_INVALID) && stream->resetPosition == true)
	{
		AAMPLOG_WARN("EOS received as first buffer ");
		NotifyEOS();
	}
	else
	{
		NotifyFragmentCachingComplete();
		AAMPGstPlayer_SignalEOS(stream->source);
		/*For trickmodes, give EOS to audio source*/
		if (AAMP_NORMAL_PLAY_RATE != privateContext->rate)
		{
			AAMPGstPlayer_SignalEOS(privateContext->stream[eMEDIATYPE_AUDIO].source);
			if (privateContext->stream[eMEDIATYPE_SUBTITLE].source)
			{
				AAMPGstPlayer_SignalEOS(privateContext->stream[eMEDIATYPE_SUBTITLE].source);
			}
		}
		// We are in buffering, but we received end of stream, un-pause pipeline
		StopBuffering(true);
	}
}

/**
 *  @brief Stop playback and any idle handlers active at the time
 */
void AAMPGstPlayer::Stop(bool keepLastFrame)
{
	FN_TRACE( __FUNCTION__ );
	AAMPLOG_WARN("entering AAMPGstPlayer_Stop keepLastFrame %d", keepLastFrame);

	//XIONE-8595 - make the execution of this function more deterministic and reduce scope for potential pipeline lockups
	gst_bus_remove_watch(privateContext->bus);

#ifdef INTELCE
	if (privateContext->video_sink)
	{
		privateContext->keepLastFrame = keepLastFrame;
		g_object_set(privateContext->video_sink,  "stop-keep-frame", keepLastFrame, NULL);
#if !defined(INTELCE_USE_VIDRENDSINK)
		if  (!keepLastFrame)
		{
			gst_object_unref(privateContext->video_sink);
			privateContext->video_sink = NULL;
		}
		else
		{
			g_object_set(privateContext->video_sink,  "reuse-vidrend", keepLastFrame, NULL);
		}
#endif
	}
#endif
	if(!keepLastFrame)
	{
		privateContext->firstFrameReceived = false;
		privateContext->firstVideoFrameReceived = false;
		privateContext->firstAudioFrameReceived = false ;
	}

	this->IdleTaskRemove(privateContext->firstProgressCallbackIdleTask);

	this->TimerRemove(this->privateContext->periodicProgressCallbackIdleTaskId, "periodicProgressCallbackIdleTaskId");

	if (this->privateContext->bufferingTimeoutTimerId)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove bufferingTimeoutTimerId %d", privateContext->bufferingTimeoutTimerId);
		g_source_remove(privateContext->bufferingTimeoutTimerId);
		privateContext->bufferingTimeoutTimerId = AAMP_TASK_ID_INVALID;
	}
	if (privateContext->ptsCheckForEosOnUnderflowIdleTaskId)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove ptsCheckForEosCallbackIdleTaskId %d", privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		g_source_remove(privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		privateContext->ptsCheckForEosOnUnderflowIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	if (this->privateContext->eosCallbackIdleTaskPending)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove eosCallbackIdleTaskId %d", privateContext->eosCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->eosCallbackIdleTaskId);
		privateContext->eosCallbackIdleTaskPending = false;
		privateContext->eosCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	if (this->privateContext->firstFrameCallbackIdleTaskPending)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove firstFrameCallbackIdleTaskId %d", privateContext->firstFrameCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->firstFrameCallbackIdleTaskId);
		privateContext->firstFrameCallbackIdleTaskPending = false;
		privateContext->firstFrameCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	if (this->privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove firstVideoFrameDisplayedCallbackIdleTaskId %d", privateContext->firstVideoFrameDisplayedCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->firstVideoFrameDisplayedCallbackIdleTaskId);
		privateContext->firstVideoFrameDisplayedCallbackIdleTaskPending = false;
		privateContext->firstVideoFrameDisplayedCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	if (this->privateContext->pipeline)
	{
		privateContext->buffering_in_progress = false;   /* stopping pipeline, don't want to change state if GST_MESSAGE_ASYNC_DONE message comes in */
		SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_NULL);
		AAMPLOG_WARN("AAMPGstPlayer: Pipeline state set to null");
	}
#ifdef AAMP_MPD_DRM
	if(AampOutputProtection::IsAampOutputProcectionInstanceActive())
	{
		AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();
		pInstance->setGstElement((GstElement *)(NULL));
		pInstance->Release();
	}
#endif
	TearDownStream(eMEDIATYPE_VIDEO);
	TearDownStream(eMEDIATYPE_AUDIO);
	TearDownStream(eMEDIATYPE_SUBTITLE);
	TearDownStream(eMEDIATYPE_AUX_AUDIO);
	DestroyPipeline();
	privateContext->rate = AAMP_NORMAL_PLAY_RATE;
	privateContext->lastKnownPTS = 0;
	privateContext->segmentStart = 0;
	privateContext->paused = false;
	privateContext->pipelineState = GST_STATE_NULL;
	AAMPLOG_WARN("exiting AAMPGstPlayer_Stop");
}

/**
 * @brief Generates a state description for gst target, next and pending state i.e. **not current state**.
 * @param[in] state  - the state of the current element
 * @param[in] start - a  char to place before the state text e.g. on open bracket
 * @param[in] end  - a char to place after the state text e.g. a close bracket
 * @param[in] currentState - the current state from the same element as 'state'
 * @param[in] parentState - the state of the parent, if there is one
 * @return  - "" unless state is 'interesting' otherwise *start* *state description* *end* e.g. {GST_STATE_READY}
 */
static std::string StateText(GstState state, char start, char end, GstState currentState, GstState parentState = GST_STATE_VOID_PENDING)
{
	if((state == GST_STATE_VOID_PENDING) || ((state == currentState) && ((state == parentState) || (parentState == GST_STATE_VOID_PENDING))))
	{
		return "";
	}
	else
	{
		std::string returnStringBuilder(1, start);
		returnStringBuilder += gst_element_state_get_name(state);
		returnStringBuilder += end;
		return returnStringBuilder;
	}
}

/**
 * @brief wraps gst_element_get_name handling unnamed elements and resource freeing
 * @param[in] element a GstElement
 * @retval The name of element or "unnamed element" as a std::string
 */
static std::string SafeName(GstElement *element)
{
	std::string name;
	auto elementName = gst_element_get_name(element);
	if(elementName)
	{
		name = elementName;
		g_free(elementName);
	}
	else
	{
		name = "unnamed element";
	}
	return name;
}

/**
 * @brief - returns a string describing pElementOrBin and its children (if any).
 * The top level elements name:state are shown along with any child elements in () separated by ,
 * State information is displayed as GST_STATE[GST_STATE_TARGET]{GST_STATE_NEXT}<GST_STATE_PENDING>
 * Target state, next state and pending state are not always shown.
 * Where GST_STATE_CHANGE for the element is not GST_STATE_CHANGE_SUCCESS an additional character is appended to the element name:
	GST_STATE_CHANGE_FAILURE: "!", GST_STATE_CHANGE_ASYNC:"~", GST_STATE_CHANGE_NO_PREROLL:"*"
 * @param[in] pElementOrBin - pointer to a gst element or bin
 * @param[in] pParent - parent (optional)
 * @param recursionCount - variable shared with self calls to limit recursion depth
 * @return - description string
 */
static std::string GetStatus(gpointer pElementOrBin, int& recursionCount, gpointer pParent = nullptr)
{
	recursionCount++;
	constexpr int RECURSION_LIMIT = 10;
	if(RECURSION_LIMIT < recursionCount)
	{
		return "recursion limit exceeded";
	}

	std::string returnStringBuilder("");
	if(nullptr !=pElementOrBin)
	{
		if(GST_IS_ELEMENT(pElementOrBin))
		{
			auto pElement = reinterpret_cast<_GstElement*>(pElementOrBin);

			bool validParent = (pParent != nullptr) && GST_IS_ELEMENT(pParent);

			returnStringBuilder += SafeName(pElement);
			GstState state;
			GstState statePending;
			auto changeStatus = gst_element_get_state(pElement, &state, &statePending, 0);
			switch(changeStatus)
			{
				case  GST_STATE_CHANGE_FAILURE:
					returnStringBuilder +="!";
				break;

				case  GST_STATE_CHANGE_SUCCESS:
					//no annnotation
				break;

				case  GST_STATE_CHANGE_ASYNC:
					returnStringBuilder +="~";
					break;

				case  GST_STATE_CHANGE_NO_PREROLL:
					returnStringBuilder +="*";
					break;

				default:
					returnStringBuilder +="?";
					break;
			}

			returnStringBuilder += ":";

			returnStringBuilder += gst_element_state_get_name(state);
			auto parentState = validParent?GST_STATE(pParent):GST_STATE_VOID_PENDING;

			returnStringBuilder += StateText(statePending, '<', '>', state,
									 validParent?GST_STATE_PENDING(pParent):GST_STATE_VOID_PENDING);
			returnStringBuilder += StateText(GST_STATE_TARGET(pElement), '[', ']', state,
									 validParent?GST_STATE_TARGET(pParent):GST_STATE_VOID_PENDING);
			returnStringBuilder += StateText(GST_STATE_NEXT(pElement), '{', '}', state,
									 validParent?GST_STATE_NEXT(pParent):GST_STATE_VOID_PENDING);
		}

		//note bin inherits from element so name bin name is also printed above, with state info where applicable
		if(GST_IS_BIN(pElementOrBin))
		{
			returnStringBuilder += " (";

			auto pBin = reinterpret_cast<_GstElement*>(pElementOrBin);
			bool first = true;
			for (auto currentListItem = GST_BIN_CHILDREN(pBin);
			currentListItem;
			currentListItem = currentListItem->next)
			{
				if(first)
				{
					first = false;
				}
				else
				{
					returnStringBuilder += ", ";
				}

				auto currentChildElement = currentListItem->data;
				if (nullptr != currentChildElement)
				{
					returnStringBuilder += GetStatus(currentChildElement, recursionCount, pBin);
				}
			}
			returnStringBuilder += ")";
		}
	}
	recursionCount--;
	return returnStringBuilder;
}

static void LogStatus(GstElement* pElementOrBin)
{
	int recursionCount = 0;
	AAMPLOG_WARN("AAMPGstPlayer: %s Status: %s",SafeName(pElementOrBin).c_str(), GetStatus(pElementOrBin, recursionCount).c_str());
}

/**
 *  @brief Log the various info related to playback
 */
void AAMPGstPlayer::DumpStatus(void)
{
	FN_TRACE( __FUNCTION__ );
	GstElement *source = this->privateContext->stream[eMEDIATYPE_VIDEO].source;
	gboolean rcBool;
	guint64 rcUint64;
	gint64 rcInt64;
	GstFormat rcFormat;

	//https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-plugins/html/gst-plugins-base-plugins-appsrc.html
	
	rcBool = 0;
	g_object_get(source, "block", &rcBool, NULL);
	AAMPLOG_WARN("\tblock=%d", (int)rcBool); // 0

	rcBool = 0;
	g_object_get(source, "emit-signals", &rcBool, NULL);
	AAMPLOG_WARN("\temit-signals=%d", (int)rcBool); // 1

	rcFormat = (GstFormat)0;
	g_object_get(source, "format", &rcFormat, NULL);
	AAMPLOG_WARN("\tformat=%d", (int)rcFormat); // 2
	
	rcBool = 0;
	g_object_get(source, "is-live", &rcBool, NULL);
	AAMPLOG_WARN("\tis-live=%d", (int)rcBool); // 0
	
	rcUint64 = 0;
	g_object_get(source, "max-bytes", &rcUint64, NULL);
	AAMPLOG_WARN("\tmax-bytes=%d", (int)rcUint64); // 200000
	
	rcInt64 = 0;
	g_object_get(source, "max-latency", &rcInt64, NULL);
	AAMPLOG_WARN("\tmax-latency=%d", (int)rcInt64); // -1

	rcInt64 = 0;
	g_object_get(source, "min-latency", &rcInt64, NULL);
	AAMPLOG_WARN("\tmin-latency=%d", (int)rcInt64); // -1

	rcInt64 = 0;
	g_object_get(source, "size", &rcInt64, NULL);
	AAMPLOG_WARN("\tsize=%d", (int)rcInt64); // -1

	gint64 pos, len;
	GstFormat format = GST_FORMAT_TIME;
	if (gst_element_query_position(privateContext->pipeline, format, &pos) &&
		gst_element_query_duration(privateContext->pipeline, format, &len))
	{
		AAMPLOG_WARN("Position: %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
			GST_TIME_ARGS(pos), GST_TIME_ARGS(len));
	}

	LogStatus(privateContext->pipeline);
}


/**
 * @brief Validate pipeline state transition within a max timeout
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @param[in] stateToValidate state to be validated
 * @param[in] msTimeOut max timeout in MS
 * @retval Current pipeline state
 */
static GstState validateStateWithMsTimeout( AAMPGstPlayer *_this, GstState stateToValidate, guint msTimeOut)
{
	GstState gst_current;
	GstState gst_pending;
	float timeout = 100.0;
	gint gstGetStateCnt = GST_ELEMENT_GET_STATE_RETRY_CNT_MAX;

	do
	{
		if ((GST_STATE_CHANGE_SUCCESS
				== gst_element_get_state(_this->privateContext->pipeline, &gst_current, &gst_pending, timeout * GST_MSECOND))
				&& (gst_current == stateToValidate))
		{
			GST_WARNING(
					"validateStateWithMsTimeout - PIPELINE gst_element_get_state - SUCCESS : State = %d, Pending = %d",
					gst_current, gst_pending);
			return gst_current;
		}
		g_usleep (msTimeOut * 1000); // Let pipeline safely transition to required state
	}
	while ((gst_current != stateToValidate) && (gstGetStateCnt-- != 0));

	AAMPLOG_WARN("validateStateWithMsTimeout - PIPELINE gst_element_get_state - FAILURE : State = %d, Pending = %d",
			gst_current, gst_pending);
	return gst_current;
}


static GstStateChangeReturn SetStateWithWarnings(GstElement *element, GstState targetState)
{
    GstStateChangeReturn rc = GST_STATE_CHANGE_FAILURE;
	if(element)
	{
		//XIONE-8595 - in a synchronous only transition gst_element_set_state can lockup if there are pipeline errors
		bool syncOnlyTransition = (targetState==GST_STATE_NULL)||(targetState==GST_STATE_READY);

		GstState current;
		GstState pending;
		auto stateChangeReturn = gst_element_get_state(element, &current, &pending, 0);
		switch(stateChangeReturn)
		{
			case GST_STATE_CHANGE_FAILURE:
				AAMPLOG_WARN("AAMPGstPlayer: %s is in FAILURE state : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
				LogStatus(element);
				break;
			case GST_STATE_CHANGE_SUCCESS:
				break;
			case GST_STATE_CHANGE_ASYNC:
				if(syncOnlyTransition)
				{
					AAMPLOG_WARN("AAMPGstPlayer: %s state is changing asynchronously : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
					LogStatus(element);
				}
				break;
			default:
				AAMPLOG_ERR("AAMPGstPlayer: %s is in an unknown state", SafeName(element).c_str());
				break;
		}

		if(syncOnlyTransition)
		{
			AAMPLOG_WARN("AAMPGstPlayer: Attempting to set %s state to %s", SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		rc = gst_element_set_state(element, targetState);
		if(syncOnlyTransition)
		{
			AAMPLOG_WARN("AAMPGstPlayer: %s state set to %s",  SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
	}
	else
	{
		AAMPLOG_ERR("AAMPGstPlayer: Attempted to set the state of a null pointer");
	}
    return rc;
}


/**
 * @brief Flush the buffers in pipeline
 */
void AAMPGstPlayer::Flush(void)
{
	FN_TRACE( __FUNCTION__ );
	if (privateContext->pipeline)
	{
		PauseAndFlush(false);
	}
}

/**
 *  @brief PauseAndFlush pipeline and flush 
 */
void AAMPGstPlayer::PauseAndFlush(bool playAfterFlush)
{
	FN_TRACE( __FUNCTION__ );
	aamp->SyncBegin();
	AAMPLOG_WARN("Entering AAMPGstPlayer::PauseAndFlush() pipeline state %s",
			gst_element_state_get_name(GST_STATE(privateContext->pipeline)));
	GstStateChangeReturn rc;
	GstState stateBeforeFlush = GST_STATE_PAUSED;
#ifndef USE_PLAYERSINKBIN
	/*On pc, tsdemux requires null transition*/
	stateBeforeFlush = GST_STATE_NULL;
#endif
	rc = SetStateWithWarnings(this->privateContext->pipeline, stateBeforeFlush);
	if (GST_STATE_CHANGE_ASYNC == rc)
	{
		if (GST_STATE_PAUSED != validateStateWithMsTimeout(this,GST_STATE_PAUSED, 50))
		{
			AAMPLOG_WARN("AAMPGstPlayer_Flush - validateStateWithMsTimeout - FAILED GstState %d", GST_STATE_PAUSED);
		}
	}
	else if (GST_STATE_CHANGE_SUCCESS != rc)
	{
		AAMPLOG_WARN("AAMPGstPlayer_Flush - gst_element_set_state - FAILED rc %d", rc);
	}
	gboolean ret = gst_element_send_event( GST_ELEMENT(privateContext->pipeline), gst_event_new_flush_start());
	if (!ret) AAMPLOG_WARN("AAMPGstPlayer_Flush: flush start error");
	ret = gst_element_send_event(GST_ELEMENT(privateContext->pipeline), gst_event_new_flush_stop(TRUE));
	if (!ret) AAMPLOG_WARN("AAMPGstPlayer_Flush: flush stop error");
	if (playAfterFlush)
	{
		rc = SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_PLAYING);

		if (GST_STATE_CHANGE_ASYNC == rc)
		{
#ifdef AAMP_WAIT_FOR_PLAYING_STATE
			if (GST_STATE_PLAYING != validateStateWithMsTimeout( GST_STATE_PLAYING, 50))
			{
				AAMPLOG_WARN("AAMPGstPlayer_Flush - validateStateWithMsTimeout - FAILED GstState %d",
						GST_STATE_PLAYING);
			}
#endif
		}
		else if (GST_STATE_CHANGE_SUCCESS != rc)
		{
			AAMPLOG_WARN("AAMPGstPlayer_Flush - gst_element_set_state - FAILED rc %d", rc);
		}
	}
	this->privateContext->total_bytes = 0;
	privateContext->pendingPlayState = false;
	//privateContext->total_duration = 0;
	AAMPLOG_WARN("exiting AAMPGstPlayer_FlushEvent");
	aamp->SyncEnd();
}

/**
 *  @brief Get playback duration in MS
 */
long AAMPGstPlayer::GetDurationMilliseconds(void)
{
	FN_TRACE( __FUNCTION__ );
	long rc = 0;
	if( privateContext->pipeline )
	{
		if( privateContext->pipelineState == GST_STATE_PLAYING || // playing
		    (privateContext->pipelineState == GST_STATE_PAUSED && privateContext->paused) ) // paused by user
		{
			privateContext->durationQuery = gst_query_new_duration(GST_FORMAT_TIME);
			if( privateContext->durationQuery )
			{
				gboolean res = gst_element_query(privateContext->pipeline,privateContext->durationQuery);
				if( res )
				{
					gint64 duration;
					gst_query_parse_duration(privateContext->durationQuery, NULL, &duration);
					rc = GST_TIME_AS_MSECONDS(duration);
				}
				else
				{
					AAMPLOG_WARN("Duration query failed");
				}
				gst_query_unref(privateContext->durationQuery);
			}
			else
			{
				AAMPLOG_WARN("Duration query is NULL");
			}
		}
		else
		{
			AAMPLOG_WARN("Pipeline is in %s state", gst_element_state_get_name(privateContext->pipelineState) );
		}
	}
	else
	{
		AAMPLOG_WARN("Pipeline is null");
	}
	return rc;
}

/**
 *  @brief Get playback position in MS
 */
long AAMPGstPlayer::GetPositionMilliseconds(void)
{
	FN_TRACE( __FUNCTION__ );
	long rc = 0;
	if (privateContext->pipeline == NULL)
	{
		AAMPLOG_ERR("Pipeline is NULL");
		return rc;
	}

	if (privateContext->positionQuery == NULL)
	{
		AAMPLOG_ERR("Position query is NULL");
		return rc;
	}

	// Perform gstreamer query and related operation only when pipeline is playing or if deliberately put in paused
	if (privateContext->pipelineState != GST_STATE_PLAYING &&
		!(privateContext->pipelineState == GST_STATE_PAUSED && privateContext->paused) &&
		// XIONE-8379 - The player should be (and probably soon will be) in the playing state so don't exit early.
		GST_STATE_TARGET(privateContext->pipeline) != GST_STATE_PLAYING)
	{
		AAMPLOG_INFO("Pipeline is in %s state, returning position as %ld", gst_element_state_get_name(privateContext->pipelineState), rc);
		return rc;
	}

	media_stream* video = &privateContext->stream[eMEDIATYPE_VIDEO];

	// segment.start needs to be queried
	if (privateContext->segmentStart == -1)
	{
		GstQuery *segmentQuery = gst_query_new_segment(GST_FORMAT_TIME);
		// DELIA-39530 - send query to video playbin in pipeline.
		// Special case include trickplay, where only video playbin is active
		if (gst_element_query(video->source, segmentQuery) == TRUE)
		{
			gint64 start;
			gst_query_parse_segment(segmentQuery, NULL, NULL, &start, NULL);
			privateContext->segmentStart = GST_TIME_AS_MSECONDS(start);
			AAMPLOG_WARN("AAMPGstPlayer: Segment start: %" G_GINT64_FORMAT, privateContext->segmentStart);
		}
		else
		{
			AAMPLOG_ERR("AAMPGstPlayer: segment query failed");
		}
		gst_query_unref(segmentQuery);
	}

	if (gst_element_query(video->sinkbin, privateContext->positionQuery) == TRUE)
	{
		gint64 pos = 0;
		int rate = privateContext->rate;
		gst_query_parse_position(privateContext->positionQuery, NULL, &pos);
		if (aamp->mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
		{
			rate = 1; // MP4 position query alaways return absolute value
		}

#if !defined(REALTEKCE)	// Pos always start from "0" in Realtek
		if (privateContext->segmentStart > 0)
		{
			// DELIA-39530 - Deduct segment.start to find the actual time of media that's played.
			rc = (GST_TIME_AS_MSECONDS(pos) - privateContext->segmentStart) * rate;
		}
		else
#endif
		{
			rc = GST_TIME_AS_MSECONDS(pos) * rate;
		}
		//AAMPLOG_WARN("AAMPGstPlayer: pos - %" G_GINT64_FORMAT " rc - %ld", GST_TIME_AS_MSECONDS(pos), rc);

		//positionQuery is not unref-ed here, because it could be reused for future position queries
	}
	return rc;
}

/**
 *  @brief To pause/play pipeline
 */
bool AAMPGstPlayer::Pause( bool pause, bool forceStopGstreamerPreBuffering )
{
	FN_TRACE( __FUNCTION__ );
	bool retValue = true;

	aamp->SyncBegin();

	AAMPLOG_WARN("entering AAMPGstPlayer_Pause - pause(%d) stop-pre-buffering(%d)", pause, forceStopGstreamerPreBuffering);

	if (privateContext->pipeline != NULL)
	{
		GstState nextState = pause ? GST_STATE_PAUSED : GST_STATE_PLAYING;

		if (GST_STATE_PAUSED == nextState && forceStopGstreamerPreBuffering)
		{
			/* maybe in a timing case during the playback start,
			 * gstreamer pre buffering and underflow buffering runs simultaneously and 
			 * it will end up pausing the pipeline due to buffering_target_state has the value as GST_STATE_PAUSED.
			 * To avoid this case, stopping the gstreamer pre buffering logic by setting the buffering_in_progress to false
			 * and the resume play will be handled from StopBuffering once after getting enough buffer/frames.
			 */
			privateContext->buffering_in_progress = false;
		}

		GstStateChangeReturn rc = SetStateWithWarnings(this->privateContext->pipeline, nextState);
		if (GST_STATE_CHANGE_ASYNC == rc)
		{
			/* wait a bit longer for the state change to conclude */
			if (nextState != validateStateWithMsTimeout(this,nextState, 100))
			{
				AAMPLOG_WARN("AAMPGstPlayer_Pause - validateStateWithMsTimeout - FAILED GstState %d", nextState);
			}
		}
		else if (GST_STATE_CHANGE_SUCCESS != rc)
		{
			AAMPLOG_WARN("AAMPGstPlayer_Pause - gst_element_set_state - FAILED rc %d", rc);
		}
		privateContext->buffering_target_state = nextState;
		privateContext->paused = pause;
		privateContext->pendingPlayState = false;
		if(!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
			aamp->PauseSubtitleParser(pause);
	}
	else
	{
		AAMPLOG_WARN("Pipeline is NULL");
		retValue = false;
	}

#if 0
	GstStateChangeReturn rc;
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		media_stream *stream = &privateContext->stream[iTrack];
		if (stream->source)
		{
			rc = SetStateWithWarnings(privateContext->stream->sinkbin, GST_STATE_PAUSED);
		}
	}
#endif

	aamp->SyncEnd();

	return retValue;
}

/**
 *  @brief Set video display rectangle co-ordinates
 */
void AAMPGstPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
	FN_TRACE( __FUNCTION__ );
	int currentX = 0, currentY = 0, currentW = 0, currentH = 0;

#ifdef INTELCE
	if (strcmp(privateContext->videoRectangle, "0,0,0,0") != 0)
#else
	if (strcmp(privateContext->videoRectangle, "") != 0)
#endif
		sscanf(privateContext->videoRectangle,"%d,%d,%d,%d",&currentX,&currentY,&currentW,&currentH);

	//check the existing VideoRectangle co-ordinates
	if ((currentX == x) && (currentY == y) && (currentW == w) && (currentH == h))
	{
		AAMPLOG_TRACE("Ignoring new co-ordinates, same as current Rect (x:%d, y:%d, w:%d, h:%d)", currentX, currentY, currentW, currentH);
		//ignore setting same rectangle co-ordinates and return
		return;
	}

	media_stream *stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	sprintf(privateContext->videoRectangle, "%d,%d,%d,%d", x,y,w,h);
	AAMPLOG_WARN("Rect %s, using_playersinkbin = %d, video_sink =%p",
			privateContext->videoRectangle, stream->using_playersinkbin, privateContext->video_sink);
	if (ISCONFIGSET(eAAMPConfig_EnableRectPropertyCfg)) //As part of DELIA-37804
	{
		if (stream->using_playersinkbin)
		{
			g_object_set(stream->sinkbin, "rectangle", privateContext->videoRectangle, NULL);
		}
#ifndef INTELCE
		else if (privateContext->video_sink)
		{
			g_object_set(privateContext->video_sink, "rectangle", privateContext->videoRectangle, NULL);
		}
#else
#if defined(INTELCE_USE_VIDRENDSINK)
		else if (privateContext->video_pproc)
		{
			g_object_set(privateContext->video_pproc, "rectangle", privateContext->videoRectangle, NULL);
		}
#else
		else if (privateContext->video_sink)
		{
			g_object_set(privateContext->video_sink, "rectangle", privateContext->videoRectangle, NULL);
		}
#endif
#endif	
		else
		{
			AAMPLOG_WARN("Scaling not possible at this time");
		}
	}
	else
	{
		AAMPLOG_WARN("New co-ordinates ignored since westerossink is used");
	}
}

/**
 *  @brief Set video zoom
 */
void AAMPGstPlayer::SetVideoZoom(VideoZoomMode zoom)
{
	FN_TRACE( __FUNCTION__ );
	media_stream *stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	AAMPLOG_INFO("SetVideoZoom :: ZoomMode %d, using_playersinkbin = %d, video_sink =%p",
			zoom, stream->using_playersinkbin, privateContext->video_sink);

	privateContext->zoom = zoom;
	if (stream->using_playersinkbin && stream->sinkbin)
	{
		g_object_set(stream->sinkbin, "zoom", zoom, NULL);
	}
#ifndef INTELCE
	else if (privateContext->video_sink)
	{
		g_object_set(privateContext->video_sink, "zoom-mode", VIDEO_ZOOM_FULL == zoom ? 0 : 1, NULL);
	}
#elif defined(INTELCE_USE_VIDRENDSINK)
	else if (privateContext->video_pproc)
	{
		g_object_set(privateContext->video_pproc, "scale-mode", VIDEO_ZOOM_FULL == zoom ? 0 : 3, NULL);
	}
#else
	else if (privateContext->video_sink)
	{
		g_object_set(privateContext->video_sink, "scale-mode", VIDEO_ZOOM_FULL == zoom ? 0 : 3, NULL);
	}
#endif
}

void AAMPGstPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	FN_TRACE( __FUNCTION__ );

	if (privateContext->subtitle_sink)
	{
		AAMPLOG_INFO("pts_offset %" G_GUINT64_FORMAT ", seek_pos_seconds %2f, subtitle_sink =%p", pts_offset, aamp->seek_pos_seconds, privateContext->subtitle_sink);
		//We use seek_pos_seconds as an offset durinig seek, so we subtract that here to get an offset from zero position
		g_object_set(privateContext->subtitle_sink, "pts-offset", static_cast<std::uint64_t>(pts_offset), NULL);
	}
	else
		AAMPLOG_INFO("subtitle_sink is NULL");
}

void AAMPGstPlayer::SetSubtitleMute(bool muted)
{
	FN_TRACE( __FUNCTION__ );
	media_stream *stream = &privateContext->stream[eMEDIATYPE_SUBTITLE];
	privateContext->subtitleMuted = muted;

	if (privateContext->subtitle_sink)
	{
		AAMPLOG_INFO("muted %d, subtitle_sink =%p", muted, privateContext->subtitle_sink);

		g_object_set(privateContext->subtitle_sink, "mute", privateContext->subtitleMuted ? TRUE : FALSE, NULL);
	}
	else
		AAMPLOG_INFO("subtitle_sink is NULL");
}

/**
 * @brief Set video mute
 */
void AAMPGstPlayer::SetVideoMute(bool muted)
{
	FN_TRACE( __FUNCTION__ );
	
	//AAMPLOG_WARN(" mute == %s", muted?"true":"false");
	
	media_stream *stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	AAMPLOG_INFO("using_playersinkbin = %d, video_sink =%p", stream->using_playersinkbin, privateContext->video_sink);

	privateContext->videoMuted = muted;
	if (stream->using_playersinkbin && stream->sinkbin)
	{
		g_object_set(stream->sinkbin, "video-mute", privateContext->videoMuted, NULL);
	}
	else if (privateContext->video_sink)
	{
#ifndef INTELCE
		g_object_set(privateContext->video_sink, "show-video-window", !privateContext->videoMuted, NULL);
#else
		g_object_set(privateContext->video_sink, "mute", privateContext->videoMuted, NULL);
#endif
	}
}

/**
 *  @brief Set audio volume
 */
void AAMPGstPlayer::SetAudioVolume(int volume)
{
	FN_TRACE( __FUNCTION__ );

	privateContext->audioVolume = volume / 100.0;
	setVolumeOrMuteUnMute();

}

/**
 *  @brief Set audio volume or mute
 */
void AAMPGstPlayer::setVolumeOrMuteUnMute(void)
{
	FN_TRACE( __FUNCTION__ );
	GstElement *gSource = NULL;
	char *propertyName = NULL;
	media_stream *stream = &privateContext->stream[eMEDIATYPE_AUDIO];
	
	AAMPLOG_WARN(" volume == %lf muted == %s", privateContext->audioVolume, privateContext->audioMuted?"true":"false");

	AAMPLOG_INFO("AAMPGstPlayer: using_playersinkbin = %d, audio_sink = %p",
				 stream->using_playersinkbin, privateContext->audio_sink);

	if (stream->using_playersinkbin && stream->sinkbin)
	{
		gSource = stream->sinkbin;
		propertyName = (char*)"audio-mute";
	}
#if (defined(__APPLE__) || defined(REALTEKCE))
	else if (stream->sinkbin)
	{
		gSource = stream->sinkbin;
		propertyName = (char*)"mute";
	}
#endif
	else if (privateContext->audio_sink)
	{
		gSource = privateContext->audio_sink;
		propertyName = (char*)"mute";
	}
	else
	{
		return; /* Return here if the sinkbin or audio_sink is not valid, no need to proceed further */
	}

#ifdef AMLOGIC /*For AMLOGIC platform*/
	/*Using "stream-volume" property of audio-sink for setting volume and mute for AMLOGIC platform*/
	AAMPLOG_WARN("AAMPGstPlayer: Setting Volume %f using stream-volume property of audio-sink", privateContext->audioVolume);
	g_object_set(gSource, "stream-volume", privateContext->audioVolume, NULL);

	/* Avoid mute property setting for AMLOGIC as use of "mute" property on pipeline is impacting all other players*/
#else
	/* Muting the audio decoder in general to avoid audio passthrough in expert mode for locked channel */
	if (0 == privateContext->audioVolume)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Audio Muted");
#ifdef INTELCE
		if (!stream->using_playersinkbin)
		{
			AAMPLOG_WARN("AAMPGstPlayer: Setting input-gain to %f", INPUT_GAIN_DB_MUTE);
			g_object_set(privateContext->audio_sink, "input-gain", INPUT_GAIN_DB_MUTE, NULL);
		}
		else
#endif
		{
			g_object_set(gSource, propertyName, true, NULL);
		}
		privateContext->audioMuted = true;
	}
	else
	{
		if (privateContext->audioMuted)
		{
			AAMPLOG_WARN("AAMPGstPlayer: Audio Unmuted after a Mute");
#ifdef INTELCE
			if (!stream->using_playersinkbin)
			{
				AAMPLOG_WARN("AAMPGstPlayer: Setting input-gain to %f", INPUT_GAIN_DB_UNMUTE);
				g_object_set(privateContext->audio_sink, "input-gain", INPUT_GAIN_DB_UNMUTE, NULL);
			}
			else
#endif
			{
				g_object_set(gSource, propertyName, false, NULL);
			}
			privateContext->audioMuted = false;
		}
		
		AAMPLOG_WARN("AAMPGstPlayer: Setting Volume %f", privateContext->audioVolume);
		g_object_set(gSource, "volume", privateContext->audioVolume, NULL);
	}
#endif
}

/**
 *  @brief Flush cached GstBuffers and set seek position & rate
 */
void AAMPGstPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		return;
	}
	FN_TRACE( __FUNCTION__ );
	media_stream *stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	privateContext->rate = rate;
	//TODO: Need to decide if required for AUX_AUDIO
	privateContext->stream[eMEDIATYPE_VIDEO].bufferUnderrun = false;
	privateContext->stream[eMEDIATYPE_AUDIO].bufferUnderrun = false;

	if (privateContext->eosCallbackIdleTaskPending)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove eosCallbackIdleTaskId %d", privateContext->eosCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->eosCallbackIdleTaskId);
		privateContext->eosCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
		privateContext->eosCallbackIdleTaskPending = false;
	}

	if (privateContext->ptsCheckForEosOnUnderflowIdleTaskId)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove ptsCheckForEosCallbackIdleTaskId %d", privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		g_source_remove(privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		privateContext->ptsCheckForEosOnUnderflowIdleTaskId = AAMP_TASK_ID_INVALID;
	}

	if (privateContext->bufferingTimeoutTimerId)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Remove bufferingTimeoutTimerId %d", privateContext->bufferingTimeoutTimerId);
		g_source_remove(privateContext->bufferingTimeoutTimerId);
		privateContext->bufferingTimeoutTimerId = AAMP_TASK_ID_INVALID;
	}

	if (stream->using_playersinkbin)
	{
		Flush();
	}
	else
	{
		if (privateContext->pipeline == NULL)
		{
			AAMPLOG_WARN("AAMPGstPlayer: Pipeline is NULL");
			return;
		}
#if defined (REALTEKCE)
		bool bAsyncModify = FALSE;
		if (privateContext->audio_sink)
		{
			PrivAAMPState state = eSTATE_IDLE;
			aamp->GetState(state);
			if (privateContext->audio_sink)
			{
				if (privateContext->rate > 1 || privateContext->rate < 0 || state == eSTATE_SEEKING)
				{
					//aamp won't feed audio bitstreame to gstreamer at trickplay.
					//It needs to disable async of audio base sink to prevent audio sink never sends ASYNC_DONE to pipeline.
					AAMPLOG_WARN("Disable async for audio stream at trickplay");
					if(gst_base_sink_is_async_enabled(GST_BASE_SINK(privateContext->audio_sink)) == TRUE)
					{
						gst_base_sink_set_async_enabled(GST_BASE_SINK(privateContext->audio_sink), FALSE);
						bAsyncModify = TRUE;
					}
				}
			}
		}
#endif
		//Check if pipeline is in playing/paused state. If not flush doesn't work
		GstState current, pending;
		GstStateChangeReturn ret;
		ret = gst_element_get_state(privateContext->pipeline, &current, &pending, 100 * GST_MSECOND);
		if ((current != GST_STATE_PLAYING && current != GST_STATE_PAUSED) || ret == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_WARN("AAMPGstPlayer: Pipeline state %s, ret %u", gst_element_state_get_name(current), ret);
			if (shouldTearDown)
			{
				AAMPLOG_WARN("AAMPGstPlayer: Pipeline is not in playing/paused state, hence resetting it");
				if(rate > AAMP_NORMAL_PLAY_RATE)
				{
					trickTeardown = true;
				}
				Stop(true);
			}
			return;
		}
		else
		{
			/* BCOM-3563, pipeline may enter paused state even when audio decoder is not ready, check again */
			if (privateContext->audio_dec)
			{
				GstState aud_current, aud_pending;
				ret = gst_element_get_state(privateContext->audio_dec, &aud_current, &aud_pending, 0);
				if ((aud_current != GST_STATE_PLAYING && aud_current != GST_STATE_PAUSED) || ret == GST_STATE_CHANGE_FAILURE)
				{
					if (shouldTearDown)
					{
						AAMPLOG_WARN("AAMPGstPlayer: Pipeline is in playing/paused state, but audio_dec is in %s state, resetting it ret %u\n",
							 gst_element_state_get_name(aud_current), ret);
						Stop(true);
						return;
					}
				}
			}
			AAMPLOG_WARN("AAMPGstPlayer: Pipeline is in %s state position %f ret %d\n", gst_element_state_get_name(current), position, ret);
		}
		/* Disabling the flush flag as part of DELIA-42607 to avoid */
		/* flush call again (which may cause freeze sometimes)      */
		/* from SendGstEvents() API.              */
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			privateContext->stream[i].resetPosition = true;
			privateContext->stream[i].flush = false;
			privateContext->stream[i].eosReached = false;
		}

		AAMPLOG_INFO("AAMPGstPlayer: Pipeline flush seek - start = %f rate = %d", position, rate);
		double playRate = 1.0;
		if (eMEDIAFORMAT_PROGRESSIVE == aamp->mMediaFormat)
		{
			playRate = rate;
		}
#if defined (REALTEKCE)
		/* XIONE-9423
		 * A work around for a timing issue that can occur around scrubbing.
		 * This causes trick play issues if applied to rates >1.
		 */
		if(abs(rate)<=1)
		{
			GstState targetState, currentState;
			gst_element_get_state(privateContext->pipeline, &currentState, &targetState, 100 * GST_MSECOND);

			if((targetState != GST_STATE_PAUSED) &&
			((targetState == GST_STATE_PLAYING) || (currentState == GST_STATE_PLAYING)))
			{
				AAMPLOG_WARN("AAMPGstPlayer: Pause before seek, Setting Pipeline to GST_STATE_PAUSED.");
				SetStateWithWarnings(privateContext->pipeline, GST_STATE_PAUSED);
			}
			else
			{
				AAMPLOG_WARN("AAMPGstPlayer: Pause before seek, not required (targetState=%s, currentState=%s)",
				gst_element_state_get_name(targetState),
				gst_element_state_get_name(currentState));
			}
		}
		else
		{
			AAMPLOG_INFO("AAMPGstPlayer: Pause before seek, not required (rate = %d).",rate);
		}
#endif

		if (!gst_element_seek(privateContext->pipeline, playRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
			position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
        {
			AAMPLOG_WARN("Seek failed");
		}
#if defined (REALTEKCE)
		if(bAsyncModify == TRUE)
		{
			gst_base_sink_set_async_enabled(GST_BASE_SINK(privateContext->audio_sink), TRUE);
		}
#endif
	}
	privateContext->eosSignalled = false;
	privateContext->numberOfVideoBuffersSent = 0;
}

/**
 *  @brief Process discontinuity for a stream type
 */
bool AAMPGstPlayer::Discontinuity(MediaType type)
{
	FN_TRACE( __FUNCTION__ );
	bool ret = false;
	media_stream *stream = &privateContext->stream[type];
	AAMPLOG_WARN("Entering AAMPGstPlayer: type(%d) format(%d) resetPosition(%d)", (int)type, stream->format, stream->resetPosition);
	/*Handle discontinuity only if atleast one buffer is pushed*/
	if (stream->format != FORMAT_INVALID && stream->resetPosition == true)
	{
		AAMPLOG_WARN("Discontinuity received before first buffer - ignoring");
	}
	else
	{
		AAMPLOG_TRACE("stream->format %d, stream->resetPosition %d, stream->flush %d", stream->format , stream->resetPosition, stream->flush);
		AAMPGstPlayer_SignalEOS(stream->source);
		// We are in buffering, but we received discontinuity, un-pause pipeline
		StopBuffering(true);
		ret = true;
	}
	return ret;
}

/**
 *  @brief Check if PTS is changing
 *  @retval true if PTS changed from lastKnown PTS or timeout hasn't expired, will optimistically return true^M^M
 *                         if video-pts attribute is not available from decoder
 */
bool AAMPGstPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	FN_TRACE( __FUNCTION__ );
	bool ret = true;
#ifndef INTELCE
	gint64 currentPTS = GetVideoPTS();
	if (currentPTS != 0)
	{
		if (currentPTS != privateContext->lastKnownPTS)
		{
			AAMPLOG_WARN("AAMPGstPlayer: There is an update in PTS prevPTS:%" G_GINT64_FORMAT " newPTS: %" G_GINT64_FORMAT "\n",
							privateContext->lastKnownPTS, currentPTS);
			privateContext->ptsUpdatedTimeMS = NOW_STEADY_TS_MS;
			privateContext->lastKnownPTS = currentPTS;
		}
		else
		{
			long diff = NOW_STEADY_TS_MS - privateContext->ptsUpdatedTimeMS;
			if (diff > timeout)
			{
				AAMPLOG_WARN("AAMPGstPlayer: Video PTS hasn't been updated for %ld ms and timeout - %ld ms", diff, timeout);
				ret = false;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer: video-pts parsed is: %" G_GINT64_FORMAT "\n",
			currentPTS);
	}
#endif
	return ret;
}

/**
 *  @brief Gets Video PTS
 */
long long AAMPGstPlayer::GetVideoPTS(void)
{
	FN_TRACE( __FUNCTION__ );
	gint64 currentPTS = 0;
	GstElement *element;
#if defined (REALTEKCE)
	element = privateContext->video_sink;
#else
	element = privateContext->video_dec;
#endif
	if( element )
	{
		g_object_get(element, "video-pts", &currentPTS, NULL);
		
#ifndef REALTEKCE
		//Westeros sink sync returns PTS in 90Khz format where as BCM returns in 45 KHz,
		// hence converting to 90Khz for BCM
		if(!privateContext->using_westerossink)
		{
			currentPTS = currentPTS * 2; // convert from 45 KHz to 90 Khz PTS
		}
#endif
	}
	return (long long) currentPTS;
}

/**
 *  @brief Reset EOS SignalledFlag
 */
void AAMPGstPlayer::ResetEOSSignalledFlag()
{
	privateContext->eosSignalled = false;
}

/**
 *  @brief Check if cache empty for a media type
 */
bool AAMPGstPlayer::IsCacheEmpty(MediaType mediaType)
{
	FN_TRACE( __FUNCTION__ );
	bool ret = true;
	media_stream *stream = &privateContext->stream[mediaType];
	if (stream->source)
	{
		guint64 cacheLevel = gst_app_src_get_current_level_bytes (GST_APP_SRC(stream->source));
		if(0 != cacheLevel)
		{
			AAMPLOG_TRACE("AAMPGstPlayer::Cache level  %" G_GUINT64_FORMAT "", cacheLevel);
			ret = false;
		}
		else
		{
			// Changed from logprintf to AAMPLOG_TRACE, to avoid log flooding (seen on xi3 and xid).
			// We're seeing this logged frequently during live linear playback, despite no user-facing problem.
			AAMPLOG_TRACE("AAMPGstPlayer::Cache level empty");
			if (privateContext->stream[eMEDIATYPE_VIDEO].bufferUnderrun == true ||
					privateContext->stream[eMEDIATYPE_AUDIO].bufferUnderrun == true)
			{
				AAMPLOG_WARN("AAMPGstPlayer::Received buffer underrun signal for video(%d) or audio(%d) previously",privateContext->stream[eMEDIATYPE_VIDEO].bufferUnderrun,
					privateContext->stream[eMEDIATYPE_AUDIO].bufferUnderrun);
			}
#ifndef INTELCE
			else
			{
				bool ptsChanged = CheckForPTSChangeWithTimeout(AAMP_MIN_PTS_UPDATE_INTERVAL);
				if(!ptsChanged)
				{
					//PTS hasn't changed for the timeout value
					AAMPLOG_WARN("AAMPGstPlayer: Appsrc cache is empty and PTS hasn't been updated for more than %lldms and ret(%d)",
									AAMP_MIN_PTS_UPDATE_INTERVAL, ret);
				}
				else
				{
					ret = false;
				}
			}
#endif
		}
	}
	return ret;
}

/**
 *  @brief Set pipeline to PLAYING state once fragment caching is complete
 */
void AAMPGstPlayer::NotifyFragmentCachingComplete()
{
	FN_TRACE( __FUNCTION__ );
	if(privateContext->pendingPlayState)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Setting pipeline to PLAYING state ");
		if (SetStateWithWarnings(privateContext->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_WARN("AAMPGstPlayer_Configure GST_STATE_PLAYING failed");
		}
		privateContext->pendingPlayState = false;
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer: No pending PLAYING state");
	}
}

/**
 *  @brief Set pipeline to PAUSED state to wait on NotifyFragmentCachingComplete()
 */
void AAMPGstPlayer::NotifyFragmentCachingOngoing()
{
	FN_TRACE( __FUNCTION__ );
	if(!privateContext->paused)
	{
		Pause(true, true);
	}
	privateContext->pendingPlayState = true;
}

/**
 *  @brief Get video display's width and height
 */
void AAMPGstPlayer::GetVideoSize(int &width, int &height)
{
	FN_TRACE( __FUNCTION__ );
	int x, y, w, h;
	sscanf(privateContext->videoRectangle, "%d,%d,%d,%d", &x, &y, &w, &h);
	if (w > 0 && h > 0)
	{
		width = w;
		height = h;
	}
}

/***
 * @fn  IsCodecSupported
 * 
 * @brief Check whether Gstreamer platform has support of the given codec or not. 
 *        codec to component mapping done in gstreamer side.
 * @param codecName - Name of codec to be checked
 * @return True if platform has the support else false
 */

bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	FN_TRACE( __FUNCTION__ );
	bool retValue = false;
	GstRegistry* registry = gst_registry_get(); 
	for (std::string &componentName: gmapDecoderLoookUptable[codecName])
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, componentName.c_str());
		if (pluginFeature != NULL)
		{
			retValue = true;
			break;
		}
	}

	return retValue;
}

/**
 *  @brief Increase the rank of AAMP decryptor plugins
 */
void AAMPGstPlayer::InitializeAAMPGstreamerPlugins(AampLogManager *mLogObj)
{
	FN_TRACE( __FUNCTION__ );
#ifdef AAMP_MPD_DRM
	GstRegistry* registry = gst_registry_get();

	GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, GstPluginNamePR);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available; reloading aamp plugin", GstPluginNamePR);
		GstPlugin * plugin = gst_plugin_load_by_name ("aamp");
		if(plugin)
		{
			gst_object_unref(plugin);
		}
		pluginFeature = gst_registry_lookup_feature(registry, GstPluginNamePR);
		if(pluginFeature == NULL)
			AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available", GstPluginNamePR);
	}
	if(pluginFeature)
	{
		gst_registry_remove_feature (registry, pluginFeature);//Added as a work around to handle DELIA-31716
		gst_registry_add_feature (registry, pluginFeature);


		AAMPLOG_WARN("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNamePR);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameWV);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available", GstPluginNameWV);
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameWV);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameCK);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available", GstPluginNameCK);
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameCK);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameVMX);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer::%s():%d %s plugin feature not available", __FUNCTION__, __LINE__, GstPluginNameVMX);
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer::%s():%d %s plugin priority set to GST_RANK_PRIMARY + 111", __FUNCTION__, __LINE__, GstPluginNameVMX);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}
#ifndef INTELCE
	for (int i=0; i<PLUGINS_TO_LOWER_RANK_MAX; i++) {
		pluginFeature = gst_registry_lookup_feature(registry, plugins_to_lower_rank[i]);
		if(pluginFeature) {
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY - 1);
			gst_object_unref(pluginFeature);
			AAMPLOG_WARN("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY  - 1\n", plugins_to_lower_rank[i]);
		}
	}
#endif
#endif
}

/**
 * @brief Notify EOS to core aamp asynchronously if required.
 * @note Used internally by AAMPGstPlayer
 */
void AAMPGstPlayer::NotifyEOS()
{
	FN_TRACE( __FUNCTION__ );
	if (!privateContext->eosSignalled)
	{
		if (!privateContext->eosCallbackIdleTaskPending)
		{
			privateContext->eosCallbackIdleTaskId = aamp->ScheduleAsyncTask(IdleCallbackOnEOS, (void *)this, "IdleCallbackOnEOS");
			if (privateContext->eosCallbackIdleTaskId != AAMP_TASK_ID_INVALID)
			{
				privateContext->eosCallbackIdleTaskPending = true;
			}
			else
			{
				AAMPLOG_WARN("eosCallbackIdleTask scheduled, eosCallbackIdleTaskId %d", privateContext->eosCallbackIdleTaskId);
			}
		}
		else
		{
			AAMPLOG_WARN("IdleCallbackOnEOS already registered previously, hence skip!");
		}
		privateContext->eosSignalled = true;
	}
	else
	{
		AAMPLOG_WARN("EOS already signaled, hence skip!");
	}
}

/**
 *  @brief Dump a file to log
 */
static void DumpFile(const char* fileName)
{
	int c;
	FILE *fp = fopen(fileName, "r");
	if (fp)
	{
		printf("\n************************Dump %s **************************\n", fileName);
		c = getc(fp);
		while (c != EOF)
		{
			printf("%c", c);
			c = getc(fp);
		}
		fclose(fp);
		printf("\n**********************Dump %s end *************************\n", fileName);
	}
	else
	{
		AAMPLOG_WARN("Could not open %s", fileName);
	}
}

/**
 *  @brief Dump diagnostic information
 *
 */
void AAMPGstPlayer::DumpDiagnostics()
{
	FN_TRACE( __FUNCTION__ );
	AAMPLOG_WARN("video_dec %p audio_dec %p video_sink %p audio_sink %p numberOfVideoBuffersSent %d",
			privateContext->video_dec, privateContext->audio_dec, privateContext->video_sink,
			privateContext->audio_sink, privateContext->numberOfVideoBuffersSent);
#ifndef INTELCE
	DumpFile("/proc/brcm/transport");
	DumpFile("/proc/brcm/video_decoder");
	DumpFile("/proc/brcm/audio");
#endif
}

/**
 *  @brief Signal trick mode discontinuity to gstreamer pipeline
 */
void AAMPGstPlayer::SignalTrickModeDiscontinuity()
{
	FN_TRACE( __FUNCTION__ );
	media_stream* stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	if (stream && (privateContext->rate != AAMP_NORMAL_PLAY_RATE) )
	{
		GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
		int  vodTrickplayFPS;
		GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS); 
		GstStructure * eventStruct = gst_structure_new("aamp-tm-disc", "fps", G_TYPE_UINT, (guint)vodTrickplayFPS, NULL);
		if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
		{
			AAMPLOG_WARN("Error on sending aamp-tm-disc");
		}
		else
		{
			AAMPLOG_WARN("Sent aamp-tm-disc event");
		}
		gst_object_unref(sourceEleSrcPad);
	}
}

/**
 *  @brief Flush the data in case of a new tune pipeline
 */
void AAMPGstPlayer::SeekStreamSink(double position, double rate)
{
	FN_TRACE( __FUNCTION__ );
	// shouldTearDown is set to false, because in case of a new tune pipeline
	// might not be in a playing/paused state which causes Flush() to destroy
	// pipeline. This has to be avoided.
	Flush(position, rate, false);

}

/**
 *  @brief Get the video rectangle co-ordinates
 */
std::string AAMPGstPlayer::GetVideoRectangle()
{
	FN_TRACE( __FUNCTION__ );
	return std::string(privateContext->videoRectangle);
}

/**
 *  @brief Un-pause pipeline and notify buffer end event to player
 */
void AAMPGstPlayer::StopBuffering(bool forceStop)
{
	FN_TRACE( __FUNCTION__ );
	pthread_mutex_lock(&mBufferingLock);
	//Check if we are in buffering
	if (ISCONFIGSET(eAAMPConfig_ReportBufferEvent) && privateContext->video_dec && aamp->GetBufUnderFlowStatus())
	{
		bool stopBuffering = forceStop;
#if ( !defined(INTELCE) && !defined(RPI) && !defined(__APPLE__) )
		int frames = -1;
	        g_object_get(privateContext->video_dec,"queued_frames",(uint*)&frames,NULL);
	        if (frames != -1)
	        {
			stopBuffering = stopBuffering || (frames > DEFAULT_BUFFERING_QUEUED_FRAMES_MIN); //TODO: the minimum frame values should be configurable from aamp.cfg
	        }
	        else
#endif
			stopBuffering = true;

		if (stopBuffering)
		{
#if ( !defined(INTELCE) && !defined(RPI) && !defined(__APPLE__) )
			AAMPLOG_WARN("Enough data available to stop buffering, frames %d !", frames);
#endif
			GstState current, pending;
			bool sendEndEvent = false;

			if(GST_STATE_CHANGE_FAILURE != gst_element_get_state(privateContext->pipeline, &current, &pending, 0 * GST_MSECOND))
			{
				if (current == GST_STATE_PLAYING)
				{
					sendEndEvent = true;
				}
				else
				{
					sendEndEvent = aamp->PausePipeline(false, false);
					aamp->UpdateSubtitleTimestamp();
				}
			}
			else
			{
				sendEndEvent = false;
			}

			if( !sendEndEvent )
			{
				AAMPLOG_ERR("Failed to un-pause pipeline for stop buffering!");
			}
			else
			{
				aamp->SendBufferChangeEvent();
			}
	        }
		else
		{
			static int bufferLogCount = 0;
			if (0 == (bufferLogCount++ % 10) )
			{
#if ( !defined(INTELCE) && !defined(RPI) && !defined(__APPLE__) )
				AAMPLOG_WARN("Not enough data available to stop buffering, frames %d !", frames);
#endif
			}
		}
	}
	pthread_mutex_unlock(&mBufferingLock);
}


void type_check_instance(const char * str, GstElement * elem)
{
	AAMPLOG_WARN("%s %p type_check %d", str, elem, G_TYPE_CHECK_INSTANCE (elem));
}

/**
 * @brief Wait for source element to be configured.
 */
bool AAMPGstPlayer::WaitForSourceSetup(MediaType mediaType)
{
	bool ret = false;
	int timeRemaining = -1;
	GETCONFIGVALUE(eAAMPConfig_SourceSetupTimeout, timeRemaining);
	media_stream *stream = &privateContext->stream[mediaType];
	
	int waitInterval = 100; //ms

	AAMPLOG_WARN("Source element[%p] for track[%d] not configured, wait for setup to complete!", stream->source, mediaType);
	while(timeRemaining >= 0)
	{
		aamp->InterruptableMsSleep(waitInterval);
		if (aamp->DownloadsAreEnabled())
		{
			if (stream->sourceConfigured)
			{
				AAMPLOG_WARN("Source element[%p] for track[%d] setup completed!", stream->source, mediaType);
				ret = true;
				break;
			}
		}
		else
		{
			//Playback stopped by application
			break;
		}
		timeRemaining -= waitInterval;
	}

	if (!ret)
	{
		AAMPLOG_WARN("Wait for source element setup for track[%d] exited/timedout!", mediaType);
	}
	return ret;
}

/**
 *  @brief Forward buffer to aux pipeline
 */
void AAMPGstPlayer::ForwardBuffersToAuxPipeline(GstBuffer *buffer)
{
	media_stream *stream = &privateContext->stream[eMEDIATYPE_AUX_AUDIO];
	if (!stream->sourceConfigured && stream->format != FORMAT_INVALID)
	{
		bool status = WaitForSourceSetup(eMEDIATYPE_AUX_AUDIO);
		if (!aamp->DownloadsAreEnabled() || !status)
		{
			// Buffer is not owned by us, no need to free
			return;
		}
	}

	GstBuffer *fwdBuffer = gst_buffer_new();
	if (fwdBuffer != NULL)
	{
		if (FALSE == gst_buffer_copy_into(fwdBuffer, buffer, GST_BUFFER_COPY_ALL, 0, -1))
		{
			AAMPLOG_ERR("Error while copying audio buffer to auxiliary buffer!!");
			gst_buffer_unref(fwdBuffer);
			return;
		}
		//AAMPLOG_TRACE("Forward audio buffer to auxiliary pipeline!!");
		GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(stream->source), fwdBuffer);
		if (ret != GST_FLOW_OK)
		{
			AAMPLOG_ERR("gst_app_src_push_buffer error: %d[%s] mediaType %d", ret, gst_flow_get_name (ret), (int)eMEDIATYPE_AUX_AUDIO);
			assert(false);
		}
	}
}

/**
 *  @brief Check if audio buffers to be forwarded or not
 */
bool AAMPGstPlayer::ForwardAudioBuffersToAux()
{
	return (privateContext->forwardAudioBuffers && privateContext->stream[eMEDIATYPE_AUX_AUDIO].format != FORMAT_INVALID);
}

/**
 * @}
 */

/**
 * @brief  adjust playback rate
 */
bool AAMPGstPlayer::AdjustPlayBackRate(double position, double rate)
{
	FN_TRACE( __FUNCTION__ );
	bool ErrSuccess = false;
	if (privateContext->pipeline == NULL)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Pipeline is NULL");
	}
	else
	{
        PrivAAMPState state = eSTATE_IDLE;
        aamp->GetState(state);
        if( ( rate != privateContext->playbackrate ) && ( state == eSTATE_PLAYING ) )
		{
			gint64 position1=0;
			/* Obtain the current position, needed for the seek event */
			if (!gst_element_query_position (privateContext->pipeline, GST_FORMAT_TIME, &position1))
			{
				AAMPLOG_WARN("AAMPGstPlayer: Unable to query gst element position");
			}
			else
			{
				if (!gst_element_seek(privateContext->pipeline, rate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
					GST_SEEK_TYPE_SET, position1, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
				{
					AAMPLOG_WARN("AAMPGstPlayer: playrate adjustment  failed");
				}
				else
				{
					AAMPLOG_INFO("AAMPGstPlayer: playrate adjustment  success");
					privateContext->playbackrate = rate;
					ErrSuccess = true;
				}
			}
		}
		else
		{
			AAMPLOG_TRACE("AAMPGstPlayer: rate is already in %lf rate",rate);
			ErrSuccess = true;
		}
	}
	return ErrSuccess;
}
