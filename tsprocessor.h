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
* @file tsprocessor.h
* @brief Header file for play context
*/

#ifndef _TSPROCESSOR_H
#define _TSPROCESSOR_H

#include "mediaprocessor.h"
#include "uint33_t.h"
#include <stdio.h>
#include <pthread.h>

#include <vector>

#define MAX_PIDS (8) //PMT Parsing

/**
* @struct RecordingComponent
* @brief Stores information of a audio/video component.
*/
struct RecordingComponent
{
	int siType; 			/**< Service information type such as PAT, PMT, NIT, SDT, BAT, etc */
	int elemStreamType; 		/**< Elementary stream type of Audio or Video, such as 1B=H.264, 0x01=MPEG1, 0x10=MPEG4, 0x03=MPEG1 Audio, 0x04=MPEG2 Audio, etc */
	int pid; 			/**< PID associated to the audio, video or data component */
	char *associatedLanguage; 	/**< Language such as eng, hin, etc */
	unsigned int descriptorTags; 	/**< Descriptor tags, each byte value will represent one descriptor tag up to max MAX_DESCRIPTOR (4)*/
};


// Maximum number of bytes needed to examine in a start code
#define SCAN_REMAINDER_SIZE_MPEG2 (7)
#define SCAN_REMAINDER_SIZE_H264 (29)
#if (SCAN_REMAINDER_SIZE_MPEG2 > SCAN_REMAINDER_SIZE_H264)
#define MAX_SCAN_REMAINDER_SIZE SCAN_REMAINDER_SIZE_MPEG2
#else
#define MAX_SCAN_REMAINDER_SIZE SCAN_REMAINDER_SIZE_H264
#endif

class Demuxer;

/**
 * @enum StreamOperation
 * @brief Operation done by TSProcessor
 */
typedef enum
{
	eStreamOp_NONE, 		/**< Normal operation when no demuxing is required. Used with playersinkbin */
	eStreamOp_DEMUX_AUDIO, 		/**< Demux and inject audio only*/
	eStreamOp_DEMUX_VIDEO, 		/**< Demux and inject video only*/
	eStreamOp_DEMUX_ALL, 		/**< Demux and inject audio and video*/
	eStreamOp_QUEUE_AUDIO, 		/**< When video contains PAT/PMT, audio needs to be queued until video is processed
					     used with playersinkbin*/
	eStreamOp_SEND_VIDEO_AND_QUEUED_AUDIO, /**< Send queued audio after video*/
	eStreamOp_DEMUX_AUX, 		/**< Demux and inject auxiliary audio only*/
	eStreamOp_DEMUX_VIDEO_AND_AUX	/**< Demux and inject auxiliary audio and video*/
	
} StreamOperation;

/**
 * @enum TrackToDemux
 * @brief Track to demux
 */
typedef enum
{
	ePC_Track_Video,	/**< Demux and send video only*/
	ePC_Track_Audio,	/**< Demux and send audio only*/
	ePC_Track_Both 		/**< Demux and send audio and video*/
}TrackToDemux;

/**
* @class TSProcessor
* @brief MPEG TS Processor. Supports software Demuxer/ PTS re-stamping for trickmode.
*/
class TSProcessor : public MediaProcessor
{
   public:
      /**
       * @fn TSProcessor
       * @param[in] aamp Pointer to aamp associated with this TSProcessor
       * @param[in] streamOperation Operation to be done on injected data.
       * @param[in] track MediaType to be operated on. Not relavent for demux operation
       * @param[in] peerTSProcessor Peer TSProcessor used along with this in case of separate audio/video playlists
       */
      TSProcessor(AampLogManager *logObj, class PrivateInstanceAAMP *aamp, StreamOperation streamOperation, int track = 0, TSProcessor* peerTSProcessor = NULL, TSProcessor* auxTSProcessor = NULL);
      /**
       * @brief Copy constructor disabled
       *
       */
      TSProcessor(const TSProcessor&) = delete;
      /**
       * @brief assignment operator disabled
       *
       */
      TSProcessor& operator=(const TSProcessor&) = delete;
      /**
       * @fn ~TSProcessor
       */
      ~TSProcessor();
      /**
       * @fn sendSegment
       * @param[in] segment Buffer containing the data segment
       * @param[in] size Specifies size of the segment in bytes.
       * @param[in] position Position of the segment in seconds
       * @param[in] duration Duration of the segment in seconds
       * @param[in] discontinuous true if fragment is discontinuous
       * @param[out] true on PTS error
       */
      bool sendSegment( char *segment, size_t& size, double position, double duration, bool discontinuous, bool &ptsError);
      /**
       * @fn setRate
       *
       * @param[in] rate play rate could be 1.0=Normal Playback, 0.0=Pause, etc
       * @param[in] mode play mode such as PlayMode_normal, PlayMode_retimestamp_Ionly,
       * PlayMode_retimestamp_IPB, PlayMode_retimestamp_IandP or PlayMode_reverse_GOP.
       *
       */
      void setRate(double rate, PlayMode mode);    
      /**
       * @fn setThrottleEnable 
       * @param[in] enable true to enable throttle, false to disable
       */
      void setThrottleEnable(bool enable);
      /**
       * @brief Set frame rate for trick mode
       * @param[in] frameRate rate per second
       */
      void setFrameRateForTM( int frameRate)
      {
         m_apparentFrameRate = frameRate;
      }
      /**
       * @fn abort
       */
      void abort();
      /**
       * @fn reset
       */
      void reset();
      /**
       * @fn flush
       */
      void flush();

   protected:
      /**
       * @fn getAudioComponents
       * @param[out] audioComponentsPtr pointer to audio component array
       * @param[out] count Number of audio components
       */
      void getAudioComponents(const RecordingComponent** audioComponentsPtr, int &count);  
      /**
       * @fn sendQueuedSegment
       * @param[in] basepts new base pts to be set. Valid only for eStreamOp_DEMUX_AUDIO.
       * @param[in] updatedStartPositon New start position of queued segment.
       */
      void sendQueuedSegment(long long basepts = 0, double updatedStartPositon = -1);
      /**
       * @fn setBasePTS
       * @param[in] position start position of fragment
       * @param[in] pts base pts for demux operations.
       */
      void setBasePTS(double position, long long pts);

   private:
      class PrivateInstanceAAMP *aamp;
      /**
       * @fn setPlayMode
       *
       * @param[in] mode play mode such as PlayMode_normal, PlayMode_retimestamp_Ionly,
       * PlayMode_retimestamp_IPB, PlayMode_retimestamp_IandP or PlayMode_reverse_GOP.
       */
      void setPlayMode( PlayMode mode );
      /**
       * @fn processPMTSection
       * @param[in] section character buffer containing PMT section
       * @param[in] sectionLength length of PMT section
       */
      void processPMTSection( unsigned char* section, int sectionLength );
      /**
       * @fn reTimestamp
       * @param[in,out] packet TS data to re-stamp
       * @param[in] length[in] TS data size
       */
      void reTimestamp( unsigned char *&packet, int length );
      /**
       * @fn insertPatPmt
       *
       * @param[out] buffer PAT and PMT is copied to this buffer
       * @param[in] trick true on trick mode, false on normal playback
       * @param[in] bufferSize size of buffer
       */
      int insertPatPmt( unsigned char *buffer, bool trick, int bufferSize );
      /**
       * @fn insertPCR
       * @param[in] packet[in,out] buffer to which PCR to be inserted
       * @param[in] pid[in] pcr pid
       */
      void insertPCR( unsigned char *packet, int pid );
      /**
       * @fn generatePATandPMT
       * @param[in] trick true on trickmode
       * @param[out] buff PAT and PMT copied to this buffer
       * @param[out] buflen Length of buff
       * @param[in] bHandleMCTrick true if audio pid is same as PCR pid
       */
      bool generatePATandPMT( bool trick, unsigned char **buff, int *bufflen, bool bHandleMCTrick = false);
      /**
       * @fn putPmtByte
       * @param[in,out] pmt buffer in which PMT is being constructed
       * @param[in,out] index current index of PMT construction.
       * @param[in] byte byte to be written at index
       * @param[in] pmtPid PID of PMT
       */
      void putPmtByte( unsigned char* &pmt, int& index, unsigned char byte, int pmtPid );
      /**
       * @fn processStartCode
       * @param[in] buffer buffer containing start code
       * @param[in] keepScanning true to keep on scanning
       * @param[in] length size of the buffer
       * @param[in] base Not used
       */
      bool processStartCode( unsigned char *buffer, bool& keepScanning, int length, int base );
      /**
       * @fn checkIfInterlaced
       * @param[in] packet buffer containing TS packet
       * @param[in] length length of buffer
       */
      void checkIfInterlaced( unsigned char *packet, int length );
      /**
       * @fn readTimeStamp
       * @param[in] p buffer position containing time-stamp
       * @param[out] TS time-stamp
       */
      bool readTimeStamp( unsigned char *p, long long &value );
      /**
       * @fn writeTimeStamp
       * @param[out] p buffer to which TS to be written
       * @param[in] prefix of time-stamp
       * @param[in] TS time-stamp
       */
      void writeTimeStamp( unsigned char *p, int prefix, long long TS );
      /**
       * @fn readPCR
       * @param[in] p start of PCR data
       */
      long long readPCR( unsigned char *p );
      /**
       * @fn writePCR
       * @param[out] p buffer to write PCR
       * @param[in] PCR timestamp to be written
       * @param[in] clearExtension clear PCR extension
       */
      void writePCR( unsigned char *p, long long PCR, bool clearExtension );
      /**
       * @fn createNullPFrame
       * @param[in] width width of P frame to be constructed
       * @param[in] height height of P frame
       * @param[out] nullPFrameLen length of constructed p frame
       */
      unsigned char* createNullPFrame( int width, int height, int *nullPFrameLen );
      /**
       * @fn processSeqParameterSet
       * @param[in] p pointer containing SPS
       * @param[in] length size of SPS
       */
      bool processSeqParameterSet( unsigned char *p, int length );
      /**
       * @fn processPictureParameterSet
       * @param[in] p buffer containing PPS
       * @param[in] length size of PPS
       */
      void processPictureParameterSet( unsigned char *p, int length );
      /**
       * @fn processScalingList
       * @param[in] p buffer containing scaling list
       * @param[in] mask mask
       * @param[in] size lenght of scaling list
       */
      void processScalingList( unsigned char *& p, int& mask, int size );
      /**
       * @fn getBits
       * @param[in,out] p pointer being processed, updated internally
       * @param[in,out] mask mask to be applied
       * @param[in] bitCount Number of bits to be processed.
       */
      unsigned int getBits( unsigned char *& p, int& mask, int bitCount );
      /**
       * @fn putBits 
       * @param[in,out] p reference of buffer to which bits to be put
       * @param[in,out] mask mask to be applied
       * @param[in] bitCount count of bits to be put
       * @param[in] value bits to be put
       */
      void putBits( unsigned char *& p, int& mask, int bitCount, unsigned int value );
      /**
       * @fn getUExpGolomb
       * @param[in,out] p buffer
       * @param[in,out] mask bitmask
       * @retval Unsigned EXP Golomb
       */
      unsigned int getUExpGolomb( unsigned char *& p, int& mask );
      /**
       * @fn getSExpGolomb
       * @param[in,out] p buffer
       * @param[in,out] bit mask
       * @retval signed EXP Golomb
       */
      int getSExpGolomb( unsigned char *& p, int& mask );
      /**
       * @fn updatePATPMT 
       */ 
      void updatePATPMT();
      /**
       * @fn abortUnlocked
       */
      void abortUnlocked();

      bool m_needDiscontinuity;
      long long m_currStreamOffset;
      long long m_currPTS;
      long long m_currTimeStamp;
      int m_currFrameNumber;
      int m_currFrameLength;
      long long m_currFrameOffset;

      bool m_trickExcludeAudio;
      int m_PatPmtLen;
      unsigned char *m_PatPmt;
      int m_PatPmtTrickLen;
      unsigned char *m_PatPmtTrick;
      int m_PatPmtPcrLen;
      unsigned char *m_PatPmtPcr;
      int m_patCounter;
      int m_pmtCounter;

      PlayMode m_playMode;
      PlayMode m_playModeNext;
      double m_playRate;
      double m_absPlayRate;
      double m_playRateNext;
      double m_apparentFrameRate;
      int m_packetSize;
      int m_ttsSize;
      int m_pcrPid;
      int m_videoPid;
      bool m_haveBaseTime;
      bool m_haveEmittedIFrame;
      bool m_haveUpdatedFirstPTS;
      int m_pcrPerPTSCount;
      long long m_baseTime;
      long long m_segmentBaseTime;
      long long m_basePCR;
      long long m_prevRateAdjustedPCR;
      long long m_currRateAdjustedPCR;
      long long m_currRateAdjustedPTS;
      unsigned char m_continuityCounters[8192];
      unsigned char m_pidFilter[8192];
      unsigned char m_pidFilterTrick[8192];

      unsigned char *m_nullPFrame;
      int m_nullPFrameLength;
      int m_nullPFrameNextCount;
      int m_nullPFrameOffset;
      int m_nullPFrameWidth;
      int m_nullPFrameHeight;
      int m_frameWidth;
      int m_frameHeight;
      bool m_scanForFrameSize;
      int m_scanRemainderSize;
      int m_scanRemainderLimit;
      unsigned char m_scanRemainder[MAX_SCAN_REMAINDER_SIZE*3];
      bool m_isH264;
      bool m_isMCChannel;
      bool m_isInterlaced;
      bool m_isInterlacedKnown;
      bool m_throttle;
      bool m_haveThrottleBase;
      long long m_lastThrottleContentTime;
      long long m_lastThrottleRealTime;
      long long m_baseThrottleContentTime;
      long long m_baseThrottleRealTime;
      uint33_t m_throttlePTS;
      bool m_insertPCR;
      int m_emulationPreventionCapacity;
      int m_emulationPreventionOffset;
      unsigned char * m_emulationPrevention;
      bool m_scanSkipPacketsEnabled;

      /**
       * @struct _H264SPS
       * @brief Holds SPS parameters
       */
      typedef struct _H264SPS
      {
         int picOrderCountType;
         int maxPicOrderCount;
         int log2MaxFrameNumMinus4;
         int log2MaxPicOrderCntLsbMinus4;
         int separateColorPlaneFlag;
         int frameMBSOnlyFlag;
      } H264SPS;

      /**
       * @struct _H264PPS
       * @brief Holds PPS parameters
       */
      typedef struct _H264PPS
      {
         int spsId;
      } H264PPS;

      H264SPS m_SPS[32];
      H264PPS m_PPS[256];
      int m_currSPSId;
      int m_picOrderCount;
      bool m_updatePicOrderCount;
      /**
       * @fn processBuffer
       * @param[in] buffer contains TS data
       * @param[in] size lenght of the buffer
       * @param[out] insPatPmt indicates if PAT and PMT needs to inserted
       */
      bool processBuffer(unsigned char *buffer, int size, bool &insPatPmt);
      /**
       * @fn getCurrentTime
       */
      long long getCurrentTime();
      /**
       * @fn throttle
       */
      bool throttle();
      /**
       * @fn sendDiscontinuity
       * @param[in] position position in seconds
       */
      void sendDiscontinuity(double position);
      /**
       * @fn setupThrottle
       * @param[in] segmentDurationMsSigned Duration of segment
       */
      void setupThrottle(int segmentDurationMs);
      /**
       * @fn demuxAndSend
       * @param[in] ptr buffer containing TS data
       * @param[in] len lenght of buffer
       * @param[in] position position of segment in seconds
       * @param[in] duration duration of segment in seconds
       * @param[in] discontinuous true if segment is discontinous
       * @param[in] trackToDemux media track to do the operation
       */
      bool demuxAndSend(const void *ptr, size_t len, double fTimestamp, double fDuration, bool discontinuous, TrackToDemux trackToDemux = ePC_Track_Both);
      /**
       * @fn msleep
       * @param[in] throttleDiff time in milliseconds
       * @retval true on abort
       */
      bool msleep(long long throttleDiff);

      bool m_havePAT; 			//!< Set to 1 when PAT buffer examined and loaded all program specific information
      int m_versionPAT; 		//!< Pat Version number
      int m_program; 			//!< Program number in the corresponding program map table
      int m_pmtPid; 			//!< For which PID the program information is available such as, audio pid, video pid, stream types, etc

      bool m_havePMT; 			//!< When PMT buffer examined the value is set to 1
      int m_versionPMT; 		//!< Version number for PMT which is being examined

      bool m_indexAudio; 		//!< If PCR Pid matches with any Audio PIDs associated for a recording, the value will be set to 1
      bool m_haveAspect; 		//!< Set to 1 when it found aspect ratio of current video
      bool m_haveFirstPTS; 		//!< The value is set to 1 if first PTS found from a recording after examining few KB of initial data
      uint33_t m_currentPTS; 		//!< Store the current PTS value of a recording
      int m_pmtCollectorNextContinuity; //!< Keeps next continuity counter for PMT packet at the time of examine the TS Buffer
      int m_pmtCollectorSectionLength; 	//!< Update section length while examining PMT table
      int m_pmtCollectorOffset; 	//!< If it is set, process subsequent parts of multi-packet PMT
      unsigned char *m_pmtCollector; 	//!< A buffer pointer to hold PMT data at the time of examining TS buffer
      bool m_scrambledWarningIssued;
      bool m_checkContinuity;
      int videoComponentCount, audioComponentCount;
      RecordingComponent videoComponents[MAX_PIDS], audioComponents[MAX_PIDS];
      bool m_dsmccComponentFound; 	//!< True if DSMCC found
      RecordingComponent m_dsmccComponent; //!< Digital storage media command and control (DSM-CC) Component

      uint33_t m_actualStartPTS;

      int m_throttleMaxDelayMs;
      int m_throttleMaxDiffSegments;
      int m_throttleDelayIgnoredMs;
      int m_throttleDelayForDiscontinuityMs;
      pthread_cond_t m_throttleCond;
      pthread_cond_t m_basePTSCond;
      pthread_mutex_t m_mutex;
      bool m_enabled;
      bool m_processing;
      int m_framesProcessedInSegment;
      long long m_lastPTSOfSegment;
      StreamOperation m_streamOperation;
      Demuxer* m_vidDemuxer;
      Demuxer* m_audDemuxer;
      Demuxer* m_dsmccDemuxer;
      bool m_demux;
      TSProcessor* m_peerTSProcessor;
      int m_packetStartAfterFirstPTS;
      unsigned char * m_queuedSegment;
      double m_queuedSegmentPos;
      double m_queuedSegmentDuration;
      size_t m_queuedSegmentLen;
      bool m_queuedSegmentDiscontinuous;
      double m_startPosition;
      int m_track;
      long long m_last_frame_time;
      bool m_demuxInitialized;
      long long m_basePTSFromPeer;
      unsigned char m_AudioTrackIndexToPlay;
      TSProcessor* m_auxTSProcessor;
      bool m_auxiliaryAudio;
      AampLogManager *mLogObj;
};

#endif

