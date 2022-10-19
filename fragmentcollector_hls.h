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
 *  @file  fragmentcollector_hls.h
 *  @brief This file handles HLS Streaming functionality for AAMP player	
 *
 *  @section DESCRIPTION
 *  
 *  This file handles HLS Streaming functionality for AAMP player. Class/structures 
 *	required for hls fragment collector is defined here.  
 *  Major functionalities include 
 *	a) Manifest / fragment collector and trick play handling
 *	b) DRM Initialization / Key acquisition
 *  c) Decrypt and inject fragments for playback
 *  d) Synchronize audio/video tracks .
 *
 */
#ifndef FRAGMENTCOLLECTOR_HLS_H
#define FRAGMENTCOLLECTOR_HLS_H

#include <memory>
#include "StreamAbstractionAAMP.h"
#include "mediaprocessor.h"
#include "drm.h"
#include <sys/time.h>
#include <atomic>

#define MAX_PROFILE 128 // TODO: remove limitation
#define FOG_FRAG_BW_IDENTIFIER "bandwidth-"
#define FOG_FRAG_BW_IDENTIFIER_LEN 10
#define FOG_FRAG_BW_DELIMITER "-"
#define CHAR_CR 0x0d // '\r'
#define CHAR_LF 0x0a // '\n'
#define BOOLSTR(boolValue) (boolValue?"true":"false")
#define PLAYLIST_TIME_DIFF_THRESHOLD_SECONDS (0.1f)
#define MAX_MANIFEST_DOWNLOAD_RETRY 3
#define MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS (6*1000)
#define MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS (500) // 500mSec
#define DRM_IV_LEN 16

#define MAX_SEQ_NUMBER_LAG_COUNT 50 				/*!< Configured sequence number max count to avoid continuous looping for an edge case scenario, which leads crash due to hung */
#define MAX_SEQ_NUMBER_DIFF_FOR_SEQ_NUM_BASED_SYNC 2 		/*!< Maximum difference in sequence number to sync tracks using sequence number.*/
#define MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_EVENT 5 	/*!< Maximum playlist refresh count for discontinuity check for TSB/cDvr*/
#define MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_LIVE 3 	/*!< Maximum playlist refresh count for discontinuity check for live without TSB*/
#define MAX_PDT_DISCONTINUITIY_DELTA_LIMIT 1.0f  		/*!< maximum audio/video track PDT delta to determine discontiuity using PDT*/



/**
* \struct	HlsProtectionInfo
* \brief	HlsStreamInfo structure for stream related information 
*/
typedef struct HlsProtectionInfo
{ 
	struct DrmSessionParams* drmData; /**< Session data */
	HlsProtectionInfo *next;          /**< pointer to access next element of Queue */
} HlsProtectionInfo;

/**
* \struct	HlsStreamInfo
* \brief	HlsStreamInfo structure for stream related information 
*/
typedef struct HlsStreamInfo: public StreamInfo
{ // #EXT-X-STREAM-INFs
	long program_id;	/**< Program Id */
	const char *audio;	/**< Audio */
	const char *codecs;	/**< Codec String */
	const char *uri;	/**< URI Information */

	// rarely present
	long averageBandwidth;	        /**< Average Bandwidth */
	const char *closedCaptions;     /**< CC if present */
	const char *subtitles;          /**< Subtitles */
	StreamOutputFormat audioFormat; /**< Audio codec format*/
} HlsStreamInfo;

/**
* \struct	MediaInfo
* \brief	MediaInfo structure for Media related information 
*/
typedef struct MediaInfo
{ // #EXT-X-MEDIA
	MediaType type;			/**< Media Type */
	const char *group_id;		/**< Group ID */
	const char *name;		/**< Name of Media */
	const char *language;		/**< Language */
	bool autoselect;		/**< AutoSelect */
	bool isDefault;			/**< IsDefault */
	const char *uri;		/**< URI Information */
	StreamOutputFormat audioFormat; /**< Audio codec format*/
	// rarely present
	int channels;			/**< Channel */
	const char *instreamID;		/**< StreamID */
	bool forced;			/**< Forced Flag */
	const char *characteristics;	/**< Characteristics */
	bool isCC;			/**< True if the text track is closed-captions */
} MediaInfo;

/**
*	\struct	IndexNode
* 	\brief	IndexNode structure for Node/DRM Index
*/
struct IndexNode
{
	double completionTimeSecondsFromStart;	/**< Time of index from start */
	const char *pFragmentInfo;		/**< Fragment Information pointer */
	int drmMetadataIdx;			/**< DRM Index for Fragment */
	const char *initFragmentPtr;		/**< Fragmented MP4 specific pointer to associated (preceding) initialization fragment */
};

/**
*	\struct	KeyTagStruct
* 	\brief	KeyTagStruct structure to store all Keytags with Hash
*/
struct KeyTagStruct
{
	KeyTagStruct() : mShaID(""), mKeyStartDuration(0), mKeyTagStr("")
	{
	}
	std::string mShaID;	   /**< ShaID of Key tag */
	double mKeyStartDuration;  /**< duration in playlist where Keytag starts */
	std::string mKeyTagStr;	   /**< String to store key tag,needed for trickplay */
};

/**
*	\struct	DiscontinuityIndexNode
* 	\brief	Index Node structure for Discontinuity Index
*/
struct DiscontinuityIndexNode
{
	int fragmentIdx;	         /**< Idx of fragment in index table*/
	double position;	         /**< Time of index from start */
	double fragmentDuration;	 /**< Fragment duration of current discontinuity index */
	double discontinuityPDT;	 /**< Program Date time value */
};

/**
*	\enum DrmKeyMethod
* 	\brief	Enum for various EXT-X-KEY:METHOD= values
*/
typedef enum
{
	eDRM_KEY_METHOD_NONE,		/**< DRM key is none */
	eDRM_KEY_METHOD_AES_128, 	/**< DRM key is AES 128 Method */
	eDRM_KEY_METHOD_SAMPLE_AES,	/**< DRM key is Sample AES method */
	eDRM_KEY_METHOD_SAMPLE_AES_CTR,	/**< DRM key is Sample AES CTR method */
	eDRM_KEY_METHOD_UNKNOWN 	/**< DRM key is unkown method */
} DrmKeyMethod;

/**
 * @}
 */

/**
 * \class TrackState
 * \brief State Machine for each Media Track
 *
 * This class is meant to handle each media track of stream
 */
class TrackState : public MediaTrack
{
public:
	/***************************************************************************
     	 * @fn TrackState
    	 *
     	 * @param[in] type Type of the track
     	 * @param[in] parent StreamAbstractionAAMP_HLS instance
     	 * @param[in] aamp PrivateInstanceAAMP pointer
     	 * @param[in] name Name of the track
     	 * @return void
     	 ***************************************************************************/
	TrackState(AampLogManager *logObj, TrackType type, class StreamAbstractionAAMP_HLS* parent, PrivateInstanceAAMP* aamp, const char* name);
	/***************************************************************************
     	 * @fn ~TrackState
     	 * @brief copy constructor function 
    	 *
     	 * @return void
     	***************************************************************************/
	TrackState(const TrackState&) = delete;
	/***************************************************************************
     	 * @fn ~TrackState
     	 *
     	 * @return void
     	 ***************************************************************************/
	~TrackState();
	/*************************************************************************
	 * @brief  Assignment operator Overloading
	 *************************************************************************/
	TrackState& operator=(const TrackState&) = delete;
	/***************************************************************************
     	 * @fn Start
    	 *
     	 * @return void
     	 **************************************************************************/  
	void Start();
	/***************************************************************************
     	 * @fn Stop
     	 *
     	 * @return void
     	 ***************************************************************************/
	void Stop(bool clearDRM = false);
	/***************************************************************************
     	 * @fn RunFetchLoop
    	 *
     	 * @return void
     	 ***************************************************************************/
	void RunFetchLoop();
	/***************************************************************************
     	 * @fn IndexPlaylist
     	 * @brief Function to parse playlist 
     	 *
     	 * @return double total duration from playlist
     	***************************************************************************/
	void IndexPlaylist(bool IsRefresh, double &culledSec);
	/***************************************************************************
     	 * @fn ABRProfileChanged
     	 *
     	 * @return void
     	 ***************************************************************************/
	void ABRProfileChanged(void);
	/***************************************************************************
     	 * @fn GetNextFragmentUriFromPlaylist
     	 * @param ignoreDiscontinuity Ignore discontinuity
	 * @param init flag to identify called from Init call
     	 * @return string fragment URI pointer
     	 ***************************************************************************/
	char *GetNextFragmentUriFromPlaylist(bool ignoreDiscontinuity=false, bool init=false);
	/***************************************************************************
     	 * @fn UpdateDrmIV
     	 *
     	 * @param[in] ptr IV string from DRM attribute
     	 * @return void
     	 ***************************************************************************/
	void UpdateDrmIV(const char *ptr);
	/***************************************************************************
     	 * @fn UpdateDrmCMSha1Hash
    	 *
     	 * @param[in] ptr ShaID string from DRM attribute
     	 * @return void
     	 ***************************************************************************/
	void UpdateDrmCMSha1Hash(const char *ptr);
	/***************************************************************************
     	 * @fn DrmDecrypt
    	 *
     	 * @param[in] cachedFragment CachedFragment struction pointer
     	 * @param[in] bucketTypeFragmentDecrypt ProfilerBucketType enum
     	 * @return bool true if successfully decrypted
     	 ***************************************************************************/
	DrmReturn DrmDecrypt(CachedFragment* cachedFragment, ProfilerBucketType bucketType);
	/***************************************************************************
     	 * @fn CreateInitVectorByMediaSeqNo
    	 *
     	 * @param[in] ui32Seqno Current fragment's sequence number
     	 * @return bool true if successfully created, false otherwise.
     	***************************************************************************/
	bool CreateInitVectorByMediaSeqNo( unsigned int ui32Seqno );
	/***************************************************************************
     	 * @fn FetchPlaylist
     	 *
     	 * @return void
     	***************************************************************************/
	void FetchPlaylist();
	/****************************************************************************
	 * @fn GetNextFragmentPeriodInfo
	 *
	 * @param[out] periodIdx Index of the period in which next fragment belongs
	 * @param[out] offsetFromPeriodStart Offset from start position of the period
	 * @param[out] fragmentIdx Fragment index
	 ****************************************************************************/
	void GetNextFragmentPeriodInfo(int &periodIdx, double &offsetFromPeriodStart, int &fragmentIdx);

	/***************************************************************************
     	 * @fn GetPeriodStartPosition
     	 * @param[in] periodIdx Period Index
     	 * @return void
     	 ***************************************************************************/
	double GetPeriodStartPosition(int periodIdx);

	/***************************************************************************
     	 * @fn GetNumberOfPeriods
      	 *
     	 * @return int number of periods
      	 ***************************************************************************/
	int GetNumberOfPeriods();

	/***************************************************************************
    	 * @fn HasDiscontinuityAroundPosition
     	 * @param[in] position Position to check for discontinuity
		 * @param[in] useStartTime starting time to search discontinuity
		 * @param[out] diffBetweenDiscontinuities discontinuity position minus input position
		 * @param[in] playPosition playback position 
		 * @param[in] inputCulledSec culled seconds
		 * @param [in] inputProgramDateTime prorgram date and time in epoc format
		 * @param [out] isDiffChkReq indicates is diffBetweenDiscontinuities check required 
		 * @return true if discontinuity present around given position
     	 ***************************************************************************/
	bool HasDiscontinuityAroundPosition(double position, bool useStartTime, double &diffBetweenDiscontinuities, double playPosition,double inputCulledSec,double inputProgramDateTime,bool &isDiffChkReq);

	/***************************************************************************
     	 * @fn StartInjection
     	 *
     	 * @return void
      	 ***************************************************************************/
	void StartInjection();

	/**
	 * @fn StopInjection
	 * @return void
	 */
	void StopInjection();

	/***************************************************************************
     	 * @fn StopWaitForPlaylistRefresh
    	 *
     	 * @return void
     	 ***************************************************************************/
	void StopWaitForPlaylistRefresh();

	/***************************************************************************
     	 * @fn CancelDrmOperation
    	 *
     	 * @param[in] clearDRM flag indicating if DRM resources to be freed or not
     	 * @return void
     	 ***************************************************************************/
	void CancelDrmOperation(bool clearDRM);

	/***************************************************************************
	 * @fn StopDiscontinuityCheck
	 *
	 * @return void
	***************************************************************************/
	void StopDiscontinuityCheckWait();
	
	/***************************************************************************
     	 * @fn RestoreDrmState
    	 *
     	 * @return void
     	 ***************************************************************************/
	void RestoreDrmState();
    	/***************************************************************************
     	 * @fn IsLive
     	 * @brief Function to check the IsLive status of track. Kept Public as its called from StreamAbstraction
    	 *
     	 * @return True if both or any track in live mode
     	 ***************************************************************************/
	bool IsLive()  { return (ePLAYLISTTYPE_VOD != mPlaylistType);}
	/***************************************************************************
     	 * @fn FindTimedMetadata
    	 *
     	 * @return void
     	 ***************************************************************************/
	void FindTimedMetadata(bool reportbulk=false, bool bInitCall = false);
	/***************************************************************************
	 * @fn SetXStartTimeOffset
     	 * @brief Function to set XStart Time Offset Value 
     	 *
         * @return void
     	 ***************************************************************************/
	void SetXStartTimeOffset(double offset) { mXStartTimeOFfset = offset; }
    	/***************************************************************************
     	 * @fn SetXStartTimeOffset
     	 * @brief Function to retune XStart Time Offset
    	 *
     	 * @return Start time
     	***************************************************************************/
	double GetXStartTimeOffset() { return mXStartTimeOFfset;}
    	/***************************************************************************
     	 * @fn GetBufferedDuration
     	 *
     	 * @return Buffer Duration
     	 ***************************************************************************/
	double GetBufferedDuration();

	/***************************************************************************
	 * @fn GetPlaylistUrl
	 *
	 * @return string - playlist URL
	 ***************************************************************************/
	std::string& GetPlaylistUrl() { return mPlaylistUrl; }
	/***************************************************************************
	 * @fn GetEffectivePlaylistUrl
	 *
	 * @return string - original playlist URL(redirected)
	 ***************************************************************************/
	std::string& GetEffectivePlaylistUrl() { return mEffectiveUrl; }
	/***************************************************************************
	 * @fn SetEffectivePlaylistUrl
	 *
	 * @return none
	 ***************************************************************************/
	void SetEffectivePlaylistUrl(std::string url) { mEffectiveUrl = url; }
	/***************************************************************************
	 * @fn GetLastPlaylistDownloadTime
	 *
	 * @return lastPlaylistDownloadTime
	 ****************************************************************************/
	long long GetLastPlaylistDownloadTime() { return lastPlaylistDownloadTimeMS; }
	/****************************************************************************
	 * @fn SetLastPlaylistDownloadTime
	 *
	 * @return void
	 ****************************************************************************/
	void SetLastPlaylistDownloadTime(long long time) { lastPlaylistDownloadTimeMS = time; }
	/****************************************************************************
	 * @fn GetMinUpdateDuration
	 *
	 * @return minimumUpdateDuration
	 ****************************************************************************/
	long GetMinUpdateDuration();
	/****************************************************************************
	 * fn GetDefaultDurationBetweenPlaylistUpdates
	 *
	 * @return maxIntervalBtwPlaylistUpdateMs
	 ****************************************************************************/
	int GetDefaultDurationBetweenPlaylistUpdates();

	/****************************************************************************
	 * @fn ProcessPlaylist
	 *
	 * @return none
	 ****************************************************************************/
	void ProcessPlaylist(GrowableBuffer& newPlaylist, long http_error);
private:
	/***************************************************************************
     	 * @fn GetFragmentUriFromIndex
     	 *
     	 * @return string fragment URI pointer
     	 ***************************************************************************/
	char *GetFragmentUriFromIndex(bool &bSegmentRepeated);
	/***************************************************************************
     	 * @fn FlushIndex
     	 *
     	 * @return void
     	 ***************************************************************************/
	void FlushIndex();
	/***************************************************************************
     	 * @fn FetchFragment
     	 *
     	 * @return void
     	 ***************************************************************************/
	void FetchFragment();
	/***************************************************************************
     	 * @fn FetchFragmentHelper
     	 *
     	 * @param[out] http_error http error string
     	 * @param[out] decryption_error decryption error
     	 * @return bool true on success else false
     	 ***************************************************************************/
	bool FetchFragmentHelper(long &http_error, bool &decryption_error, bool & bKeyChanged, int * fogError, double &downloadTime);
	/***************************************************************************
     	 * @fn RefreshPlaylist
    	 *
     	 * @return void
     	 ***************************************************************************/
	void RefreshPlaylist(void);
	/***************************************************************************
     	 * @fn GetContext
    	 *
     	 * @return StreamAbstractionAAMP instance
    	 ***************************************************************************/
	StreamAbstractionAAMP* GetContext();
	/***************************************************************************
     	 * @fn InjectFragmentInternal
      	 *
     	 * @param[in] cachedFragment CachedFragment structure
     	 * @param[out] fragmentDiscarded bool to indicate fragment successfully injected
     	 * @return void
     	 ***************************************************************************/
	void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded);
	/***************************************************************************
     	 * @fn FindMediaForSequenceNumber
     	 * @return string fragment tag line pointer
     	 ***************************************************************************/
	char *FindMediaForSequenceNumber();
	/***************************************************************************
     	 * @fn FetchInitFragment
    	 *
     	 * @return void
     	 ***************************************************************************/
	void FetchInitFragment();
	/***************************************************************************
     	 * @fn FetchInitFragmentHelper
     	 * @return true if success
     	 ***************************************************************************/
	bool FetchInitFragmentHelper(long &http_code, bool forcePushEncryptedHeader = false);
	/***************************************************************************
	 * @fn ProcessDrmMetadata
     	 ***************************************************************************/
	void ProcessDrmMetadata();
	/***************************************************************************
     	 * @fn ComputeDeferredKeyRequestTime
     	 ***************************************************************************/
	void ComputeDeferredKeyRequestTime();
	/***************************************************************************
     	 * @fn InitiateDRMKeyAcquisition
     	 ***************************************************************************/
	void InitiateDRMKeyAcquisition(int indexPosn=-1);
	/***************************************************************************
     	 * @fn SetDrmContext
     	 * @return None
     	 ***************************************************************************/
	void SetDrmContext();
	/***************************************************************************
     	 * @fn SwitchSubtitleTrack
     	 *
     	 * @return void
     	 ***************************************************************************/
	void SwitchSubtitleTrack();

public:
	std::string mEffectiveUrl; 		 /**< uri associated with downloaded playlist (takes into account 302 redirect) */
	std::string mPlaylistUrl; 		 /**< uri associated with downloaded playlist */
	GrowableBuffer playlist; 		 /**< downloaded playlist contents */
	
	double mProgramDateTime;
	GrowableBuffer index; 			 /**< packed IndexNode records for associated playlist */
	int indexCount; 			 /**< number of indexed fragments in currently indexed playlist */
	int currentIdx; 			 /**< index for currently-presenting fragment used during FF/REW (-1 if undefined) */
	std::string mFragmentURIFromIndex;       /**< storage for uri generated by GetFragmentUriFromIndex */
	long long indexFirstMediaSequenceNumber; /**< first media sequence number from indexed manifest */

	char *fragmentURI;                       /**< pointer (into playlist) to URI of current fragment-of-interest */
	long long lastPlaylistDownloadTimeMS;    /**< UTC time at which playlist was downloaded */
	size_t byteRangeLength;                  /**< state for \#EXT-X-BYTERANGE fragments */
	size_t byteRangeOffset;                  /**< state for \#EXT-X-BYTERANGE fragments */

	long long nextMediaSequenceNumber;       /**< media sequence number following current fragment-of-interest */
	double playlistPosition;                 /**< playlist-relative time of most recent fragment-of-interest; -1 if undefined */
	double playTarget;                       /**< initially relative seek time (seconds) based on playlist window, but updated as a play_target */
	double playTargetBufferCalc;
	double lastDownloadedIFrameTarget;	 /**< stores last downloaded iframe segment target value for comparison */ 
	double targetDurationSeconds;            /**< copy of \#EXT-X-TARGETDURATION to manage playlist refresh frequency */
	int mDeferredDrmKeyMaxTime;	         /**< copy of \#EXT-X-X1-LIN DRM refresh randomization Max time interval */
	StreamOutputFormat streamOutputFormat;   /**< type of data encoded in each fragment */
	MediaProcessor* playContext;             /**< state for s/w demuxer / pts/pcr restamper module */
	double startTimeForPlaylistSync;         /**< used for time-based track synchronization when switching between playlists */
	double playTargetOffset;                 /**< For correcting timestamps of streams with audio and video tracks */
	bool discontinuity;                      /**< Set when discontinuity is found in track*/
	StreamAbstractionAAMP_HLS* context;      /**< To get  settings common across tracks*/
	bool fragmentEncrypted;                  /**< In DAI, ad fragments can be clear. Set if current fragment is encrypted*/
	bool mKeyTagChanged;	                 /**< Flag to indicate Key tag got changed for decryption context setting */
	int mLastKeyTagIdx ;                     /**< Variable to hold the last keyTag index,to check if key tag changed */
	struct DrmInfo mDrmInfo;	         /**< Structure variable to hold Drm Information */
	char* mCMSha1Hash;	                 /**< variable to store ShaID*/
	long long mDrmTimeStamp;	         /**< variable to store Drm Time Stamp */
	int mDrmMetaDataIndexPosition;	         /**< Variable to store Drm Meta data Index position*/
	GrowableBuffer mDrmMetaDataIndex;        /**< DrmMetadata records for associated playlist */
	int mDrmMetaDataIndexCount;              /**< number of DrmMetadata records in currently indexed playlist */
	int mDrmKeyTagCount;                     /**< number of EXT-X-KEY tags present in playlist */
	bool mIndexingInProgress;                /**< indicates if indexing is in progress*/
	GrowableBuffer mDiscontinuityIndex;      /**< discontinuity start position mapping of associated playlist */
	int mDiscontinuityIndexCount;            /**< number of records in discontinuity position index */
	std::atomic<bool> mDiscontinuityCheckingOn;
	double mDuration;                        /** Duration of the track*/
	typedef std::vector<KeyTagStruct> KeyHashTable;
	typedef std::vector<KeyTagStruct>::iterator KeyHashTableIter;
	KeyHashTable mKeyHashTable;
	bool mCheckForInitialFragEnc;           /**< Flag that denotes if we should check for encrypted init header and push it to GStreamer*/
	DrmKeyMethod mDrmMethod;                /**< denotes the X-KEY method for the fragment of interest */
	CMCDHeaders *pCMCDMetrics;		/**<pointer object to class CMCDHeaders*/

private:
	bool refreshPlaylist;	                /**< bool flag to indicate if playlist refresh required or not */
	pthread_t fragmentCollectorThreadID;	/**< Thread Id for Fragment  collector Thread */
	bool fragmentCollectorThreadStarted;	/**< Flag indicating if fragment collector thread started or not*/
	int manifestDLFailCount;		/**< Manifest Download fail count for retry*/
	bool firstIndexDone;                    /**< Indicates if first indexing is done*/
	std::shared_ptr<HlsDrmBase> mDrm;       /**< DRM decrypt context*/
	bool mDrmLicenseRequestPending;         /**< Indicates if DRM License Request is Pending*/
	bool mInjectInitFragment;               /**< Indicates if init fragment injection is required*/
	const char* mInitFragmentInfo;          /**< Holds init fragment Information index*/
	bool mForceProcessDrmMetadata;          /**< Indicates if processing drm metadata to be forced on indexing*/
	pthread_mutex_t mPlaylistMutex;         /**< protect playlist update */
	pthread_cond_t mPlaylistIndexed;        /**< Notifies after a playlist indexing operation */
	pthread_mutex_t mTrackDrmMutex;         /**< protect DRM Interactions for the track */
	pthread_mutex_t mDiscoCheckMutex;         	/**< protect playlist discontinuity check */
	pthread_cond_t mDiscoCheckComplete;     /**< Notifies after a discontinuity check */
	double mLastMatchedDiscontPosition;     /**< Holds discontinuity position last matched  by other track */
	double mCulledSeconds;                  /**< Total culled duration in this streamer instance*/
	double mCulledSecondsOld;               /**< Total culled duration in this streamer instance*/
	bool mSyncAfterDiscontinuityInProgress; /**< Indicates if a synchronization after discontinuity tag is in progress*/
	PlaylistType mPlaylistType;		/**< Playlist Type */
	bool mReachedEndListTag;		/**< Flag indicating if End list tag reached in parser */
	bool mByteOffsetCalculation;            /**< Flag used to calculte byte offset from byte length for fragmented streams */
	bool mSkipAbr;                          /**< Flag that denotes if previous cached fragment is init fragment or not */
	const char* mFirstEncInitFragmentInfo;  /**< Holds first encrypted init fragment Information index*/
	double mXStartTimeOFfset;		/**< Holds value of time offset from X-Start tag */
	double mCulledSecondsAtStart;		/**< Total culled duration with this asset prior to streamer instantiation*/
	bool mSkipSegmentOnError;		/**< Flag used to enable segment skip on fetch error */
	MediaType playlistMediaType;		/**< Media type of playlist of this track */
};

class StreamAbstractionAAMP_HLS;
class PrivateInstanceAAMP;
/**
 * \class StreamAbstractionAAMP_HLS
 *
 * \brief HLS Stream handler class 
 *
 * This class is meant to handle download of HLS manifest and interface play controls
 */
class StreamAbstractionAAMP_HLS : public StreamAbstractionAAMP
{
public:
	/**************************************************************************
        * @fn IndexPlaylist
        * @return void
        *************************************************************************/
	void IndexPlaylist(TrackState *trackState);
	/***************************************************************************
         * @fn StreamAbstractionAAMP_HLS
         *
         * @param[in] aamp PrivateInstanceAAMP pointer
         * @param[in] seekpos Seek position
         * @param[in] rate Rate of playback
         * @return void
         ***************************************************************************/
	StreamAbstractionAAMP_HLS(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
	/*************************************************************************
         * @brief Copy constructor disabled
         *
         *************************************************************************/
	StreamAbstractionAAMP_HLS(const StreamAbstractionAAMP_HLS&) = delete;
	/***************************************************************************
         * @fn ~StreamAbstractionAAMP_HLS
         *
         * @return void
         ***************************************************************************/ 
	~StreamAbstractionAAMP_HLS();
	/*****************************************************************************
         * @brief assignment operator disabled
         *
         ****************************************************************************/
	StreamAbstractionAAMP_HLS& operator=(const StreamAbstractionAAMP_HLS&) = delete;
	/***************************************************************************
         * @fn DumpProfiles
         *
         * @return void
         ***************************************************************************/
	void DumpProfiles(void);
	/***************************************************************************
         * @fn Start
         *
         * @return void
         ***************************************************************************/
	void Start();
	/***************************************************************************
         * @fn Stop
         * @param[in] clearChannelData flag indicating to full stop or temporary stop
         * @return void
         ***************************************************************************/
	void Stop(bool clearChannelData);
	/***************************************************************************
         * @fn Init
         *
         * @param[in] tuneType Tune type
         * @return bool true on success
         ***************************************************************************/
	AAMPStatusType Init(TuneType tuneType);
	/***************************************************************************
         * @fn GetStreamFormat
         *
         * @param[out] primaryOutputFormat video format
         * @param[out] audioOutputFormat audio format
         * @param[out] auxOutputFormat auxiliary audio format
         * @param[out] subFormat subtitle format
         * @return void
         ***************************************************************************/
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subOutputFormat) override;
	/***************************************************************************
         * @fn GetStreamPosition
         * @brief Function to return current playing position of stream
         *
         * @return seek position
         ***************************************************************************/
	double GetStreamPosition() { return seekPosition; }
	/***************************************************************************
         * @fn GetFirstPTS
         *
         * @return double PTS value
         ***************************************************************************/
	double GetFirstPTS();
	/***************************************************************************
         * @fn GetStartTimeOfFirstPTS
         * @brief Function to return start time of first PTS
         *
         * @return double start time of first PTS value
         ***************************************************************************/
	double GetStartTimeOfFirstPTS() { return 0.0; }
	/***************************************************************************
         * @fn GetMediaTrack
         *
         * @param[in] type TrackType input
         * @return MediaTrack structure pointer
         ***************************************************************************/
	MediaTrack* GetMediaTrack(TrackType type);
	/***************************************************************************
         * @fn GetBWIndex
         *
         * @param bitrate Bitrate in bits per second
         * @return bandwidth index
         ***************************************************************************/
	int GetBWIndex(long bitrate);
	/***************************************************************************
         * @fn GetVideoBitrates
         *
         * @return available video bitrates
         ***************************************************************************/
	std::vector<long> GetVideoBitrates(void);
	/***************************************************************************
         * @fn GetAudioBitrates
         *
         * @return available audio bitrates
         ***************************************************************************/
	std::vector<long> GetAudioBitrates(void);
	/***************************************************************************
         * @fn GetMediaCount
         * @brief Function to get the Media count
         *
         * @return Number of media count
        ***************************************************************************/
	int GetMediaCount(void) { return mMediaCount;}	
	/***************************************************************************
         * @fn FilterAudioCodecBasedOnConfig
         *
         * @param[in] audioFormat Audio codec type
         * @return bool false if the audio codec type is allowed to process
         ***************************************************************************/
	bool FilterAudioCodecBasedOnConfig(StreamOutputFormat audioFormat);
	/***************************************************************************
         * @fn SeekPosUpdate
         *
         * @param[in] secondsRelativeToTuneTime seek position time
         ***************************************************************************/
	void SeekPosUpdate(double secondsRelativeToTuneTime);
	/***************************************************************************
         * @fn PreCachePlaylist
         *
         * @return none
         ***************************************************************************/
	void PreCachePlaylist();	
	/***************************************************************************
         *   @fn GetBufferedDuration
         *
         *   @return buffer value
         **************************************************************************/
	double GetBufferedDuration();
	/***************************************************************************
         * @fn GetLanguageCode
         *
         * @return Language code in string format
         ***************************************************************************/
	std::string GetLanguageCode( int iMedia );
	/***************************************************************************
         * @fn GetBestAudioTrackByLanguage
         *
         * @return int index of the audio track selected
         ***************************************************************************/
	int GetBestAudioTrackByLanguage( void );
	/***************************************************************************
         * @fn GetAvailableThumbnailTracks
         *
         * @return vector of available thumbnail tracks.
         ***************************************************************************/
	std::vector<StreamInfo*> GetAvailableThumbnailTracks(void);
	/***************************************************************************
         * @fn SetThumbnailTrack
         *
         * @param thumbIndex thumbnail index value indicating the track to select
         * @return bool true on success.
        ***************************************************************************/
	bool SetThumbnailTrack(int);
	/***************************************************************************
         * @fn GetThumbnailRangeData
         *
         * @param tStart start duration of thumbnail data.
         * @param tEnd end duration of thumbnail data.
         * @param baseurl base url of thumbnail images.
         * @param raw_w absolute width of the thumbnail spritesheet.
         * @param raw_h absolute height of the thumbnail spritesheet.
         * @param width width of each thumbnail tile.
         * @param height height of each thumbnail tile.
         * @return Updated vector of available thumbnail data.
         ***************************************************************************/
	std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*);
	/***************************************************************************
         * @brief Function to parse the Thumbnail Manifest and extract Tile information
         *
         *************************************************************************/
	std::map<std::string,double> GetImageRangeString(double*, std::string, TileInfo*, double);
	GrowableBuffer thumbnailManifest;	/**< Thumbnail manifest buffer holder */
	std::vector<TileInfo> indexedTileInfo;	/**< Indexed Thumbnail information */
	/***************************************************************************
         * @brief Function to get the total number of profiles
         *
         ***************************************************************************/
	int GetTotalProfileCount() { return mProfileCount;}
	/***************************************************************************
         * @fn GetAvailableVideoTracks
         * @return list of available video tracks
         *
         **************************************************************************/
	std::vector<StreamInfo*> GetAvailableVideoTracks(void);

	/**
	 * @fn Is4KStream
	 * @brief check if current stream have 4K content
	 * @param height - resolution of 4K stream if found
	 * @param bandwidth - bandwidth of 4K stream if foudd
	 * @return true on success 
	 */
	virtual bool Is4KStream(int &height, long &bandwidth) override;

//private:
	// TODO: following really should be private, but need to be accessible from callbacks
	
	TrackState* trackState[AAMP_TRACK_COUNT];		/**< array to store all tracks of a stream */
	float rate;						/**< Rate of playback  */
	float maxIntervalBtwPlaylistUpdateMs;			/**< Interval between playlist update */
	GrowableBuffer mainManifest;				/**< Main manifest buffer holder */
	bool allowsCache;					/**< Flag indicating if playlist needs to be cached or not */
	HlsStreamInfo streamInfo[MAX_PROFILE];			/**< Array to store multiple stream information */
	MediaInfo mediaInfo[MAX_PROFILE];			/**< Array to store multiple media within stream */

	double seekPosition;					/**< Seek position for playback */
	double midSeekPtsOffset;				/**< PTS offset for Mid Fragment seek  */
	int mTrickPlayFPS;					/**< Trick play frames per stream */
	bool enableThrottle;					/**< Flag indicating throttle enable/disable */
	bool firstFragmentDecrypted;				/**< Flag indicating if first fragment is decrypted for stream */
	bool mStartTimestampZero;				/**< Flag indicating if timestamp to start is zero or not (No audio stream) */
	int mNumberOfTracks;					/**< Number of media tracks.*/
	CMCDHeaders *pCMCDMetrics;				/**<pointer object to class CMCDHeaders*/
	/***************************************************************************
         * @fn ParseMainManifest
         *
         * @return AAMPStatusType
         ***************************************************************************/
	AAMPStatusType ParseMainManifest();
	/***************************************************************************
         * @fn GetPlaylistURI
         *
         * @param[in] trackType Track type
         * @param[in] format stream output type
         * @return string playlist URI
         ***************************************************************************/ 
	const char *GetPlaylistURI(TrackType trackType, StreamOutputFormat* format = NULL);
	int lastSelectedProfileIndex; 	/**< Variable  to restore in case of playlist download failure */
	/***************************************************************************
         * @fn StopInjection
         *
         * @return void
         ***************************************************************************/
	void StopInjection(void);
	/***************************************************************************
         * @fn StartInjection
         *
         * @return void
         ***************************************************************************/
	void StartInjection(void);
	/***************************************************************************
         * @fn IsLive
         * @return True if both or any track in live mode
         ***************************************************************************/
	bool IsLive();
	/***************************************************************************
         * @fn NotifyFirstVideoPTS
         * @param[in] pts base pts
         * @param[in] timeScale time scale
         * @return void
         ***************************************************************************/
	void NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale);
	/**
         * @fn StartSubtitleParser
         * @return void 
         */ 
	void StartSubtitleParser() override;
	/**
         * @fn PauseSubtitleParser
         * @return void
         */
	void PauseSubtitleParser(bool pause) override;
	/***************************************************************************
         * @fn GetMediaIndexForLanguage
         *
         * @param[in] lang language
         * @param[in] type track type
         * @return int mediaInfo index of track with matching language
         ***************************************************************************/
	int GetMediaIndexForLanguage(std::string lang, TrackType type);
	/***************************************************************************
         * @fn GetStreamOutputFormatForTrack
         *
         * @param[in] type track type
         * @return StreamOutputFormat for the audio codec selected
         ***************************************************************************/
	StreamOutputFormat GetStreamOutputFormatForTrack(TrackType type);
	/***************************************************************************
         * @brief  Function to get output format for audio/aux track
         *
         *************************************************************************/
	StreamOutputFormat GetStreamOutputFormatForAudio(void);
	/****************************************************************************
	 *   @brief Change muxed audio track index
	 *
	 *   @param[in] string index
	 *   @return void
	 ****************************************************************************/
	void ChangeMuxedAudioTrackIndex(std::string& index);

protected:
	/***************************************************************************
         * @fn GetStreamInfo
         *
         * @param[in] idx profileIndex
         * @return StreamInfo for the index
         ***************************************************************************/
	StreamInfo* GetStreamInfo(int idx);
private:
	/***************************************************************************
         * @fn SyncTracks
         * @param useProgramDateTimeIfAvailable use program date time tag to sync if available
         * @return eAAMPSTATUS_OK on success
         ***************************************************************************/
	AAMPStatusType SyncTracks(void);
	/***************************************************************************
         * @fn CheckDiscontinuityAroundPlaytarget
         *
         * @return void
         ***************************************************************************/
	void CheckDiscontinuityAroundPlaytarget(void);
	/***************************************************************************
         * @fn SyncTracksForDiscontinuity
         *
         * @return eAAMPSTATUS_OK on success
         ***************************************************************************/
	AAMPStatusType SyncTracksForDiscontinuity();
	/***************************************************************************
         * @fn PopulateAudioAndTextTracks
         *
         * @return void
         ***************************************************************************/
	void PopulateAudioAndTextTracks();
	/***************************************************************************
         * @fn ConfigureAudioTrack
         *
         * @return void
         ***************************************************************************/
	void ConfigureAudioTrack();
	/***************************************************************************
         * @fn ConfigureVideoProfiles
         *
         * @return void
         ***************************************************************************/
	void ConfigureVideoProfiles();
	/***************************************************************************
         * @fn ConfigureTextTrack
         *
         * @return void
         ***************************************************************************/
	void ConfigureTextTrack();

	int segDLFailCount;		/**< Segment Download fail count */
	int segDrmDecryptFailCount;	/**< Segment Decrypt fail count */
	int mMediaCount;		/**< Number of media in the stream */
	int mProfileCount;		/**< Number of Video/Iframe in the stream */
	bool mIframeAvailable;		/**< True if iframe available in the stream */
	std::set<std::string> mLangList;/**< Available language list */
	double mFirstPTS; 		/**< First video PTS in seconds */
};

#endif // FRAGMENTCOLLECTOR_HLS_H
