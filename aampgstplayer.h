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

#define DAI_MAIN_PLAYER (1)                               /**< main vod player (see DELIA-57442) */
#define DAI_UNDEFINED_PLAYER (-1)                         /**< player uninitialized */


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
         * @fn SendHelper
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
         * @fn SendGstEvents
         * @param[in] mediaType stream type
         * @param[in] pts PTS of next buffer
         */
	void SendGstEvents(MediaType mediaType, GstClockTime pts);
	/**
         * @fn SendNewSegmentEvent
         * @param[in] mediaType stream type
         * @param[in] startPts Start Position of first buffer
         * @param[in] stopPts Stop position of last buffer
         */
	void SendNewSegmentEvent(MediaType mediaType, GstClockTime startPts ,GstClockTime stopPts = 0);

public:
	class PrivateInstanceAAMP *aamp;
	/**
         * @fn Configure
         * @param[in] format video format
         * @param[in] audioFormat audio format
         * @param[in] auxFormat aux audio format
         * @param[in] subFormat subtitle format
         * @param[in] bESChangeStatus
         * @param[in] forwardAudioToAux if audio buffers to be forwarded to aux pipeline
         * @param[in] setReadyAfterPipelineCreation True/False for pipeline is created
         */
	void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation=false);
	/**
         * @fn SendCopy
         * @param[in] mediaType stream type
         * @param[in] ptr buffer pointer
         * @param[in] len length of buffer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] duration duration of buffer (in sec)
         */
	void SendCopy(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration);
	/**
         * @fn SendTransfer
         * @param[in] mediaType stream type
         * @param[in] buffer buffer as GrowableBuffer pointer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] duration duration of buffer (in sec)
         * @param[in] initFragment flag for buffer type (init, data)
         */
	void SendTransfer(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double duration, bool initFragment);
	/**
         * @fn EndOfStreamReached
         * @param[in] type stream type
         */
	void EndOfStreamReached(MediaType type);
	/**
         * @fn Stream
         */
	void Stream(void);

	/**
         * @fn SetKeepLastFrame
         * @param[in] keepLastFrame True/False to enable or disable
         */
	void SetKeepLastFrame(bool keepLastFrame);

	/**
         * @fn Detach
         */
	void Detach(void);

	/**
         * @fn DetachComplete
         */
	bool DetachComplete(void);

	/**
         * @fn Stop
         * @param[in] keepLastFrame denotes if last video frame should be kept
         */
	void Stop(bool keepLastFrame);
	/**
         * @fn DumpStatus
         */
	void DumpStatus(void);
	/**
         * @fn Flush
         * @param[in] position playback seek position
         * @param[in] rate playback rate
         * @param[in] shouldTearDown flag indicates if pipeline should be destroyed if in invalid state
         */
	void Flush(double position, int rate, bool shouldTearDown);
	/**
         * @fn Pause
         * @param[in] pause flag to pause/play the pipeline
         * @param[in] forceStopGstreamerPreBuffering - true for disabling bufferinprogress
         * @retval true if content successfully paused
         */
	bool Pause(bool pause, bool forceStopGstreamerPreBuffering);
	/**
         * @fn GetPositionMilliseconds
         * @retval playback position in MS
         */
	long GetPositionMilliseconds(void);
        /**
         * @fn GetDurationMilliseconds 
         * @retval playback duration in MS
         */
	long GetDurationMilliseconds(void);
	/**
         * @fn getCCDecoderHandle
         * @retval the decoder handle
         */
	unsigned long getCCDecoderHandle(void);
	/**
         * @fn GetVideoPTS
         * @retval Video PTS value
         */
	virtual long long GetVideoPTS(void);
	/**
         * @fn SetVideoRectangle
         * @param[in] x x co-ordinate of display rectangle
         * @param[in] y y co-ordinate of display rectangle
         * @param[in] w width of display rectangle
         * @param[in] h height of display rectangle
         */
	void SetVideoRectangle(int x, int y, int w, int h);
	/**
         * @fn Discontinuity
         * @param mediaType Media stream type
         * @retval true if discontinuity processed
         */
	bool Discontinuity( MediaType mediaType);
	/**
         * @fn SetVideoZoom
         * @param[in] zoom zoom setting to be set
         */
	void SetVideoZoom(VideoZoomMode zoom);
	/**
         * @fn SetVideoMute
         * @param[in] muted true to mute video otherwise false
         */
	void SetVideoMute(bool muted);
	/**
         * @fn SetAudioVolume
         * @param[in] volume audio volume value (0-100)
         */
	void SetAudioVolume(int volume);
	/**
         * @fn SetSubtitleMute
         * @param[in] muted true to mute subtitle otherwise false
         */
	void SetSubtitleMute(bool mute);
	/**
         * @fn SetSubtitlePtsOffset
         * @param[in] pts_offset pts offset for subs
         */
	void SetSubtitlePtsOffset(std::uint64_t pts_offset);
	/**
         * @fn setVolumeOrMuteUnMute
         * @note set privateContext->audioVolume before calling this function
         */
	void setVolumeOrMuteUnMute(void);
	/**
         * @fn IsCacheEmpty
         * @param[in] mediaType stream type
         * @retval true if cache empty
         */
	bool IsCacheEmpty(MediaType mediaType);
	/**
         * @fn ResetEOSSignalledFlag
         */
	void ResetEOSSignalledFlag();
	/**
         * @fn CheckForPTSChangeWithTimeout
         *
         * @param[in] timeout - to check if PTS hasn't changed within a time duration
         */
	bool CheckForPTSChangeWithTimeout(long timeout);
	/**
         * @fn NotifyFragmentCachingComplete
         */
	void NotifyFragmentCachingComplete();
	/**^M
         * @fn NotifyFragmentCachingOngoing
         */
	void NotifyFragmentCachingOngoing();
	/**
         * @fn GetVideoSize
         * @param[in] w width video width
         * @param[in] h height video height
         */
	void GetVideoSize(int &w, int &h);
	/**^M
         * @fn QueueProtectionEvent
         * @param[in] protSystemId keysystem to be used
         * @param[in] ptr initData DRM initialization data
         * @param[in] len initDataSize DRM initialization data size
         * @param[in] type Media type
         */
	void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, MediaType type);
	/**
         * @fn ClearProtectionEvent
         */
	void ClearProtectionEvent();
	/**
         * @fn IdleTaskAdd
         * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
         * @param[in] funcPtr function pointer to add to the asynchronous queue task
         * @return true - if task was added
         */
	bool IdleTaskAdd(TaskControlData& taskDetails, BackgroundTask funcPtr);
	/**
         * @fn IdleTaskRemove
         * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
         * @return true - if task was removed
         */
	bool IdleTaskRemove(TaskControlData& taskDetails);
	/**
         * @fn IdleTaskClearFlags
         * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
         */
	void IdleTaskClearFlags(TaskControlData& taskDetails);
	/**
         * @fn TimerAdd
         * @param[in] funcPtr function to execute on timer expiry
         * @param[in] repeatTimeout timeout between calls in ms
         * @param[in] user_data data to pass to the timer function
         * @param[in] timerName name of the timer being removed (for debug) (opt)
         * @param[out] taskId id of the timer to be returned
         */
	void TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName = nullptr);
	/**
         * @fn TimerRemove
         * @param[in] taskId id of the timer to be removed
         * @param[in] timerName name of the timer being removed (for debug) (opt)
         */
	void TimerRemove(guint& taskId, const char* timerName = nullptr);
	/**
         * @fn TimerIsRunning
         * @param[in] taskId id of the timer to be removed
         * @return true - timer is currently running
         */
	bool TimerIsRunning(guint& taskId);
	/**
         * @fn StopBuffering
         *
         * @param[in] forceStop - true to force end buffering
         */
	void StopBuffering(bool forceStop);
	/**
         * @fn AdjustPlayBackRate
         * @param[in] position playback seek position
         * @param[in] rate playback rate
         * @return true if playrate adjusted
         */
	bool AdjustPlayBackRate(double position, double rate);

	/**
	 * @fn SetPlayBackRate
	 * @param[in] rate playback rate
	 * @return true if playrate adjusted
	 */
	bool SetPlayBackRate ( double rate );

	bool PipelineSetToReady; /**< To indicate the pipeline is set to ready forcefully */
	bool trickTeardown;
	struct AAMPGstPlayerPriv *privateContext;
	/**
         * @fn AAMPGstPlayer
         *  
         */
	AAMPGstPlayer(AampLogManager *logObj, PrivateInstanceAAMP *aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	, std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
#endif
	);
	AAMPGstPlayer(const AAMPGstPlayer&) = delete;
	AAMPGstPlayer& operator=(const AAMPGstPlayer&) = delete;
	/**
     	 * @fn ~AAMPGstPlayer
     	 */
	~AAMPGstPlayer();
	/**
     	 * @fn InitializeAAMPGstreamerPlugins
     	 */
	static void InitializeAAMPGstreamerPlugins(AampLogManager *logObj=NULL);
	/**
	 * @fn NotifyEOS
     	 */
	void NotifyEOS();
	/**
     	 * @fn NotifyFirstFrame
     	 * @param[in] type media type of the frame which is decoded, either audio or video.
     	 */
	void NotifyFirstFrame(MediaType type);
	/**
     	 * @fn DumpDiagnostics
    	 *
     	 */
	void DumpDiagnostics();
	/**
     	 *   @fn SignalTrickModeDiscontinuity
     	 *   @return void
     	 */
	void SignalTrickModeDiscontinuity();
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	std::function< void(uint8_t *, int, int, int) > cbExportYUVFrame;
	/**
     	 * @fn AAMPGstPlayer_OnVideoSample
     	 * @param[in] object - pointer to appsink instance triggering "new-sample" signal
     	 * @param[in] _this  - pointer to AAMPGstPlayer instance
     	 * @retval GST_FLOW_OK
     	 */
	static GstFlowReturn AAMPGstPlayer_OnVideoSample(GstElement* object, AAMPGstPlayer * _this);
#endif
    	/**
     	 * @fn SeekStreamSink
     	 * @param position playback seek position
	 * @param rate play rate  
     	 */
	void SeekStreamSink(double position, double rate);
	
	/**
     	 * @fn static IsCodecSupported
    	 * @param[in] codecName - name of the codec value
     	 */
	static bool IsCodecSupported(const std::string &codecName);
	
	/**
     	 *   @fn GetVideoRectangle
    	 *
     	 */
	std::string GetVideoRectangle();

	/**
	 	* @brief Get the player ID kept in the mLogObj object of the player
	 	* @fn GetLoggingPlayerID
	 	* @return Player ID recorded in mLogObj
	 */
	int getLoggingPlayerID();

	/**
	 	* @brief Get whether the log level is allowed for the players' mLogObj
	 	* @fn GetLoggingLevelAllowed()
	 	* @param[in] logLevel the logging level to query
	 	* @return True or false : whether the logging level is allowed
	 */
	bool isLoggingLevelAllowed(AAMP_LogLevel logLevel);

private:
	/**
     	 * @fn PauseAndFlush 
     	 * @param playAfterFlush denotes if it should be set to playing at the end
     	 */
	void PauseAndFlush(bool playAfterFlush);
	/**
     	 * @fn TearDownStream
     	 * @param[in] mediaType stream type
     	 */
	void TearDownStream(MediaType mediaType);
	/**
     	 * @fn CreatePipeline
     	 */
	bool CreatePipeline();
	/**
     	 * @fn DestroyPipeline
     	 */
	void DestroyPipeline();
	static bool initialized;
    	/**
     	 * @fn Flush
     	 */ 
	void Flush(void);
	/**
     	 * @brief Flush last saved ID3 metadata
     	 * @return void
     	 */
	void FlushLastId3Data();
    	/**
     	 * @fn WaitForSourceSetup
     	 *
     	 * @param[in] mediaType - source element for media type
     	 * @return bool - true if source setup completed within timeout
     	 */
	bool WaitForSourceSetup(MediaType mediaType);
	/**
     	 * @fn ForwardBuffersToAuxPipeline
    	 *
     	 * @param[in] buffer - input buffer to be forwarded
     	 */
	void ForwardBuffersToAuxPipeline(GstBuffer *buffer);
	/**
     	 * @fn ForwardAudioBuffersToAux
     	 *
     	 * @return bool - true if audio to be forwarded
     	 */
	bool ForwardAudioBuffersToAux();
	pthread_mutex_t mBufferingLock;
	pthread_mutex_t mProtectionLock;
	AampLogManager *mLogObj;
};

#endif // AAMPGSTPLAYER_H
