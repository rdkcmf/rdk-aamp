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
 * @file aampgstplayer.h
 * @brief Gstreamer based player for AAMP
 */

#ifndef AAMPGSTPLAYER_H
#define AAMPGSTPLAYER_H

#include <stddef.h>
#include <functional>
#include <gst/gst.h>
#include "priv_aamp.h"
#include <pthread.h>

/**
 * @struct AAMPGstPlayerPriv
 * @brief forward declaration of AAMPGstPlayerPriv
 */
struct AAMPGstPlayerPriv;

/**
 * @struct TaskControlData
 * @brief data for scheduling and handling asynchronous tasks
 */
struct TaskControlData
{
	guint       taskID;
	bool        taskIsPending;
	std::string taskName;
	TaskControlData(const char* taskIdent) : taskID(0), taskIsPending(false), taskName(taskIdent ? taskIdent : "undefined") {};
};

/**
 * @brief Function pointer for the idle task
 * @param[in] arg - Arguments
 * @return Idle task status
 */
typedef int(*BackgroundTask)(void* arg);


/**
 * @class AAMPGstPlayer
 * @brief Class declaration of Gstreamer based player
 */
class AAMPGstPlayer : public StreamSink
{
private:
	/**
         * @brief Inject stream buffer to gstreamer pipeline
         * @param[in] mediaType stream type
         * @param[in] ptr buffer pointer
         * @param[in] len length of buffer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] duration duration of buffer (in sec)
         * @param[in] copy to map or transfer the buffer
         * @param[in] initFragment flag for buffer type (init, data)
         */
	bool SendHelper(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration, bool copy, bool initFragment = 0);
	/**
         * @brief Send any pending/cached events to pipeline
         * @param[in] mediaType stream type
         * @param[in] pts PTS of next buffer
         */
	void SendGstEvents(MediaType mediaType, GstClockTime pts);
	/**
     	 * @brief Send new segment event to pipeline
     	 * @param[in] mediaType stream type
     	 * @param[in] startPts Start Position of first buffer
     	 * @param[in] stopPts Stop position of last buffer
     	 */
	void SendNewSegmentEvent(MediaType mediaType, GstClockTime startPts ,GstClockTime stopPts = 0);

public:
	class PrivateInstanceAAMP *aamp;
	/**
	 * @brief Configure pipeline based on A/V formats
     	 * @param[in] format video format
     	 * @param[in] audioFormat audio format
     	 * @param[in] auxFormat aux audio format
     	 * @param[in] bESChangeStatus
     	 * @param[in] forwardAudioToAux if audio buffers to be forwarded to aux pipeline
         * @param[in] setReadyAfterPipelineCreation True/False for pipeline is created
     	 */
	void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation);
	/**
	 * @brief inject HLS/ts elementary stream buffer to gstreamer pipeline
         * @param[in] mediaType stream type
         * @param[in] ptr buffer pointer
         * @param[in] len length of buffer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] duration duration of buffer (in sec)
         */
	void SendCopy(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration);
	/**
         * @brief inject mp4 segment to gstreamer pipeline
         * @param[in] mediaType stream type
         * @param[in] buffer buffer as GrowableBuffer pointer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] duration duration of buffer (in sec)
         * @param[in] initFragment flag for buffer type (init, data)
         */
	void SendTransfer(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double duration, bool initFragment);
	/**
         * @brief Starts processing EOS for a particular stream type
         * @param[in] type stream type
         */
	void EndOfStreamReached(MediaType type);
	/**
         * @brief To start playback
         */
	void Stream(void);
	/**
         * @brief Stop playback and any idle handlers active at the time
         * @param[in] keepLastFrame denotes if last video frame should be kept
         */
	void Stop(bool keepLastFrame);
	/**
         * @brief Log the various info related to playback
         */
	void DumpStatus(void);
	/**
         * @brief Flush cached GstBuffers and set seek position & rate
         * @param[in] position playback seek position
         * @param[in] rate playback rate
         * @param[in] shouldTearDown flag indicates if pipeline should be destroyed if in invalid state
         */
	void Flush(double position, int rate, bool shouldTearDown);
	/**
         * @brief To pause/play pipeline
         * @param[in] pause flag to pause/play the pipeline
         * @param[in] forceStopGstreamerPreBuffering - true for disabling bufferinprogress
         * @retval true if content successfully paused
         */
	bool Pause(bool pause, bool forceStopGstreamerPreBuffering);
	/**
     	 * @brief Get playback position in MS
     	 * @retval playback position in MS
     	 */
	long GetPositionMilliseconds(void);
	/**
         * @brief Get playback duration in MS
     	 * @retval playback duration in MS
     	 */
	long GetDurationMilliseconds(void);
	/**
	 * @brief Retrieve the video decoder handle from pipeline
     	 * @retval the decoder handle
     	 */
	unsigned long getCCDecoderHandle(void);
	/**
         * @brief Gets Video PTS
         * @retval Video PTS value
         */
	virtual long long GetVideoPTS(void);
	/**
         * @brief Set video display rectangle co-ordinates
         * @param[in] x x co-ordinate of display rectangle
         * @param[in] y y co-ordinate of display rectangle
         * @param[in] w width of display rectangle
         * @param[in] h height of display rectangle
         */
	void SetVideoRectangle(int x, int y, int w, int h);
	/**
         * @brief Process discontinuity for a stream type
         * @param mediaType Media stream type
         * @retval true if discontinuity processed
         */
	bool Discontinuity( MediaType mediaType);
	/**
         * @brief Set video zoom
         * @param[in] zoom zoom setting to be set
         */
	void SetVideoZoom(VideoZoomMode zoom);
	/**
         * @brief Set video mute
         * @param[in] muted true to mute video otherwise false
         */
	void SetVideoMute(bool muted);
	/**
         * @brief Set audio volume
	 * @param[in] volume audio volume value (0-100)
         */
	void SetAudioVolume(int volume);
	/**
         * @brief Set audio volume or mute
         * @note set privateContext->audioVolume before calling this function
         */
	void setVolumeOrMuteUnMute(void);
	/**
         * @brief Check if cache empty for a media type^M
         * @param[in] mediaType stream type
         * @retval true if cache empty
         */
	bool IsCacheEmpty(MediaType mediaType);
	/**
         * @brief Reset EOS SignalledFlag
         */
	void ResetEOSSignalledFlag();
	/**
	 * @brief Check if PTS is changing
     	 *
     	 * @param[in] timeout - to check if PTS hasn't changed within a time duration
     	 * @retval true if PTS changed from lastKnown PTS or timeout hasn't expired, will optimistically return true^M
     	 *                         if video-pts attribute is not available from decoder
     	 */
	bool CheckForPTSChangeWithTimeout(long timeout);
	/**
         * @brief Set pipeline to PLAYING state once fragment caching is complete
	 */
	void NotifyFragmentCachingComplete();
	/**
         * @brief Set pipeline to PAUSED state to wait on NotifyFragmentCachingComplete()
         */
	void NotifyFragmentCachingOngoing();
	/**
         * @brief Get video display's width and height
         * @param[in] w width video width
         * @param[in] h height video height
         */
	void GetVideoSize(int &w, int &h);
	/**
         * @brief Generate a protection event
         * @param[in] protSystemId keysystem to be used
         * @param[in] ptr initData DRM initialization data
         * @param[in] len initDataSize DRM initialization data size
         * @param[in] type Media type
         */
	void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, MediaType type);
	/**
         * @brief Cleanup generated protection event
         */
	void ClearProtectionEvent();
	/**
	 * @brief IdleTaskAdd - add an async/idle task in a thread safe manner, assuming it is not queued
	 * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
	 * @param[in] funcPtr function pointer to add to the asynchronous queue task
	 * @return true - if task was added
 	 */
	bool IdleTaskAdd(TaskControlData& taskDetails, BackgroundTask funcPtr);
	/**
 	 * @brief IdleTaskRemove - remove an async task in a thread safe manner, if it is queued
	 * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
	 * @return true - if task was removed
 	 */
	bool IdleTaskRemove(TaskControlData& taskDetails);
	/**
	 * @brief IdleTaskClearFlags - clear async task id and pending flag in a thread safe manner
 	 *                             e.g. called when the task executes
	 * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
 	 */
	void IdleTaskClearFlags(TaskControlData& taskDetails);
	/**
 	 * @brief TimerAdd - add a new glib timer in thread safe manner
	 * @param[in] funcPtr function to execute on timer expiry
	 * @param[in] repeatTimeout timeout between calls in ms
	 * @param[in] user_data data to pass to the timer function
	 * @param[in] timerName name of the timer being removed (for debug) (opt)
	 * @param[out] taskId id of the timer to be returned
	 */
	void TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName = nullptr);
	/**
 	 * @brief TimerRemove - remove a glib timer in thread safe manner, if it exists
 	 * @param[in] taskId id of the timer to be removed
	 * @param[in] timerName name of the timer being removed (for debug) (opt)
	 */
	void TimerRemove(guint& taskId, const char* timerName = nullptr);
	/**
 	 * @brief RemoveTimer - remove a glib timer in thread safe manner, if it exists
         * @param[in] taskId id of the timer to be removed
         * @return true - timer is currently scheduled
         */
	bool TimerIsRunning(guint& taskId);
	/**
         * @brief Un-pause pipeline and notify buffer end event to player
         *
         * @param[in] forceStop - true to force end buffering
         */
	void StopBuffering(bool forceStop);
	/**
         * @brief  adjust playback rate
         * @param[in] position playback seek position
         * @param[in] rate playback rate
         * @return true if playrate adjusted
         */
	bool AdjustPlayBackRate(double position, double rate);
	
	bool PipelineSetToReady; /**< To indicate the pipeline is set to ready forcefully */
	bool trickTeardown;
	struct AAMPGstPlayerPriv *privateContext;
	/**
         * @brief  adjust playback rate
         * @param[in] position playback seek position
         * @param[in] rate playback rate
         * @return true if playrate adjusted
         */
	AAMPGstPlayer(AampLogManager *logObj, PrivateInstanceAAMP *aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	, std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
#endif
	);
	AAMPGstPlayer(const AAMPGstPlayer&) = delete;
	AAMPGstPlayer& operator=(const AAMPGstPlayer&) = delete;
	/**
     	 * @brief AAMPGstPlayer Destructor
     	 */
	~AAMPGstPlayer();
	/**
     	 * @brief Increase the rank of AAMP decryptor plugins
     	 */
	static void InitializeAAMPGstreamerPlugins(AampLogManager *logObj=NULL);
	/**
	 * @brief Notify EOS to core aamp asynchronously if required.
     	 * @note Used internally by AAMPGstPlayer
     	 */
	void NotifyEOS();
	/**
     	 * @brief Notify first Audio and Video frame through an idle function to make the playersinkbin halding same as normal(playbin) playback.
     	 * @param[in] type media type of the frame which is decoded, either audio or video.
     	 */
	void NotifyFirstFrame(MediaType type);
	/**
     	 * @brief Dump diagnostic information
    	 *
     	 */
	void DumpDiagnostics();
	/**
     	 *   @brief Signal trick mode discontinuity to gstreamer pipeline
     	 *
     	 */
	void SignalTrickModeDiscontinuity();
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	std::function< void(uint8_t *, int, int, int) > cbExportYUVFrame;
	/**
     	 * @brief Callback function to get video frames
     	 * @param[in] object - pointer to appsink instance triggering "new-sample" signal
     	 * @param[in] _this  - pointer to AAMPGstPlayer instance
     	 * @retval GST_FLOW_OK
     	 */
	static GstFlowReturn AAMPGstPlayer_OnVideoSample(GstElement* object, AAMPGstPlayer * _this);
#endif
    	/**
     	 * @brief Flush the data in case of a new tune pipeline
     	 * @param position playback seek position
	 * @param rate play rate  
     	 */
	void SeekStreamSink(double position, double rate);
	/**
     	 *   @brief Get the video rectangle co-ordinates
    	 *
     	 */
	std::string GetVideoRectangle();
private:
	/**
     	 * @brief PauseAndFlush pipeline and flush 
     	 * @param playAfterFlush denotes if it should be set to playing at the end
     	 */
	void PauseAndFlush(bool playAfterFlush);
	/**
     	 * @brief Cleanup resources and flags for a particular stream type
     	 * @param[in] mediaType stream type
     	 */
	void TearDownStream(MediaType mediaType);
	/**
     	 * @brief Create a new Gstreamer pipeline
     	 */
	bool CreatePipeline();
	/**
     	 * @brief Cleanup an existing Gstreamer pipeline and associated resources
     	 */
	void DestroyPipeline();
	static bool initialized;
    	/**
     	 * @brief Flush the buffers in pipeline
     	 */ 
	void Flush(void);
	/**
     	 * @brief Flush last saved ID3 metadata
     	 * @return void
     	 */
	void FlushLastId3Data();
    	/**
     	 * @brief Wait for source element to be configured.
     	 *
     	 * @param[in] mediaType - source element for media type
     	 * @return bool - true if source setup completed within timeout
     	 */
	bool WaitForSourceSetup(MediaType mediaType);
	/**
     	 * @brief Forward buffer to aux pipeline
    	 *
     	 * @param[in] buffer - input buffer to be forwarded
     	 */
	void ForwardBuffersToAuxPipeline(GstBuffer *buffer);
	/**
     	 * @brief Check if audio buffers to be forwarded or not
     	 *
     	 * @return bool - true if audio to be forwarded
     	 */
	bool ForwardAudioBuffersToAux();
	pthread_mutex_t mBufferingLock;
	pthread_mutex_t mProtectionLock;
	AampLogManager *mLogObj;
};

#endif // AAMPGSTPLAYER_H
