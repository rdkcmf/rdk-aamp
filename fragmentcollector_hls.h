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
 *  @file  fragmentcollector_hls.cpp
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

#define MAX_LICENSE_ACQ_WAIT_TIME 12000  /*!< 12 secs Increase from 10 to 12 sec(DELIA-33528) */
#define MAX_SEQ_NUMBER_LAG_COUNT 50 /*!< Configured sequence number max count to avoid continuous looping for an edge case scenario, which leads crash due to hung */
#define MAX_SEQ_NUMBER_DIFF_FOR_SEQ_NUM_BASED_SYNC 2 /*!< Maximum difference in sequence number to sync tracks using sequence number.*/
#define MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_EVENT 5 /*!< Maximum playlist refresh count for discontinuity check for TSB/cDvr*/
#define MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_LIVE 3 /*!< Maximum playlist refresh count for discontinuity check for live without TSB*/


/**
* \struct	HlsStreamInfo
* \brief	HlsStreamInfo structure for stream related information 
*/
typedef struct HlsProtectionInfo
{ 
	struct DrmSessionParams* drmData;	/**< Session data */
	HlsProtectionInfo *next; /** < pointer to access next element of Queue **/
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
	long averageBandwidth;	/**< Average Bandwidth */
	const char *closedCaptions;	/**< CC if present */
	const char *subtitles;	/**< Subtitles */
	StreamOutputFormat audioFormat; /**< Audio codec format*/
} HlsStreamInfo;

/**
* \struct	MediaInfo
* \brief	MediaInfo structure for Media related information 
*/
typedef struct MediaInfo
{ // #EXT-X-MEDIA
	MediaType type;			/**< Media Type */
	const char *group_id;	/**< Group ID */
	const char *name;		/**< Name of Media */
	const char *language;	/**< Language */
	bool autoselect;		/**< AutoSelect */
	bool isDefault;			/**< IsDefault */
	const char *uri;		/**< URI Information */
	StreamOutputFormat audioFormat; /**< Audio codec format*/
	// rarely present
	int channels;			/**< Channel */
	const char *instreamID;	/**< StreamID */
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
	const char *pFragmentInfo;				/**< Fragment Information pointer */
	int drmMetadataIdx;						/**< DRM Index for Fragment */
	const char *initFragmentPtr;			/**< Fragmented MP4 specific pointer to associated (preceding) initialization fragment */
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
	std::string mShaID;		/**< ShaID of Key tag */
	double mKeyStartDuration;		/**< duration in playlist where Keytag starts */
	std::string mKeyTagStr;			/**< String to store key tag,needed for trickplay */
};

/**
*	\struct	DiscontinuityIndexNode
* 	\brief	Index Node structure for Discontinuity Index
*/
struct DiscontinuityIndexNode
{
	int fragmentIdx;	         /**< Idx of fragment in index table*/
	double position;	         /**< Time of index from start */
	double fragmentDuration;	/**< Fragment duration of current discontinuity index */
	const char* programDateTime; /**Program Date time */
};

/**
*	\enum DrmKeyMethod
* 	\brief	Enum for various EXT-X-KEY:METHOD= values
*/
typedef enum
{
	eDRM_KEY_METHOD_NONE,
	eDRM_KEY_METHOD_AES_128,
	eDRM_KEY_METHOD_SAMPLE_AES,
	eDRM_KEY_METHOD_SAMPLE_AES_CTR,
	eDRM_KEY_METHOD_UNKNOWN
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
	/// Constructor
	TrackState(TrackType type, class StreamAbstractionAAMP_HLS* parent, PrivateInstanceAAMP* aamp, const char* name);
	/// Copy Constructor
	TrackState(const TrackState&) = delete;
	/// Destructor
	~TrackState();
	/// Assignment operator Overloading
	TrackState& operator=(const TrackState&) = delete;
	/// Start Fragment downloader and Injector thread  
	void Start();
	/// Reset and Stop Collector and Injector thread 
	void Stop(bool clearDRM = false);
	/// Fragment Collector thread execution function
	void RunFetchLoop();
	/// Function to parse playlist file and update data structures 
	void IndexPlaylist(bool IsRefresh, double &culledSec);
	/// Function to handle Profile change after ABR  
	void ABRProfileChanged(void);
	/// Function to get next fragment URI for download 
	char *GetNextFragmentUriFromPlaylist(bool ignoreDiscontinuity=false);
	/// Function to update IV value from DRM information 
	void UpdateDrmIV(const char *ptr);
	/// Function to update SHA1 ID from DRM information
	void UpdateDrmCMSha1Hash(const char *ptr);
	/// Function to decrypt the fragment data 
	DrmReturn DrmDecrypt(CachedFragment* cachedFragment, ProfilerBucketType bucketType);
	/// Function to fetch the Playlist file
	void FetchPlaylist();
	/**
	 * @brief Get period information of next fragment
	 *
	 * @param[out] periodIdx Index of the period in which next fragment belongs
	 * @param[out] offsetFromPeriodStart Offset from start position of the period
	 */
	void GetNextFragmentPeriodInfo(int &periodIdx, double &offsetFromPeriodStart, int &fragmentIdx);

	/**
	 * @brief Get start position of the period corresponding to the index.
	 *
	 * @param[in] periodIdx Index of period
	 * @return Start position of the period
	 */
	double GetPeriodStartPosition(int periodIdx);

	/**
	 * @brief Get total number of periods in playlist based on discontinuity
	 *
	 * @return Number of periods in playlist
	 */
	int GetNumberOfPeriods();

	/// Check if discontinuity present around given position
	bool HasDiscontinuityAroundPosition(double position, bool useStartTime, double &diffBetweenDiscontinuities, double playPosition,double inputCulledSec,double inputProgramDateTime);

	/**
	 * @brief Start fragment injection
	 */
	void StartInjection();

	/**
	 * @brief Stop fragment injection
	 */
	void StopInjection();

	/// Stop wait for playlist refresh
	void StopWaitForPlaylistRefresh();

	/**
	 * @brief Cancel DRM operations
	 */
	void CancelDrmOperation(bool clearDRM);

	/**
	 * @brief Restore DRM state
	 */
	void RestoreDrmState();
	/// Function to check the IsLive status of track. Kept Public as its called from StreamAbstraction
	bool IsLive()  { return (ePLAYLISTTYPE_VOD != mPlaylistType);}
	/**
	 * @brief Function to search playlist for subscribed tags
	 */
	void FindTimedMetadata(bool reportbulk=false, bool bInitCall = false);
	// Function to set XStart Time Offset Value 
	void SetXStartTimeOffset(double offset) { mXStartTimeOFfset = offset; }
	// Function to retune XStart Time Offset
	double GetXStartTimeOffset() { return mXStartTimeOFfset;}
	double GetBufferedDuration();
private:
	/// Function to get fragment URI based on Index 
	char *GetFragmentUriFromIndex(bool &bSegmentRepeated);
	/// Function to flush all the downloads done 
	void FlushIndex();
	/// Function to Fetch the fragment and inject for playback 
	void FetchFragment();
	/// Helper function fetch the fragments 
	bool FetchFragmentHelper(long &http_error, bool &decryption_error, bool & bKeyChanged, int * fogError, double &downloadTime);
	/// Function to redownload playlist after refresh interval .
	void RefreshPlaylist(void);
	/// Function to get Context pointer
	StreamAbstractionAAMP* GetContext();
	/// Function to inject fragment decrypted fragment
	void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded);
	/// Function to find the media sequence after refresh for continuity
	char *FindMediaForSequenceNumber();
	/// Fetch and inject init fragment
	void FetchInitFragment();
	/// Helper function fetch the init fragments
	bool FetchInitFragmentHelper(long &http_code, bool forcePushEncryptedHeader = false);
	/// Process Drm Metadata after indexing
	void ProcessDrmMetadata();
	/// Function to check for deferred licensing
	void ComputeDeferredKeyRequestTime();
	/// Function to get DRM License key
	void InitiateDRMKeyAcquisition(int indexPosn=-1);
	/// Function to set the DRM Metadata into Adobe DRM Layer for decryption
	void SetDrmContext();
	// Flushes all old fragments and switches playlist without restarting pipeline
	void SwitchSubtitleTrack();

public:
	std::string mEffectiveUrl; 		/**< uri associated with downloaded playlist (takes into account 302 redirect) */
	std::string mPlaylistUrl; 		/**< uri associated with downloaded playlist */
	GrowableBuffer playlist; 				/**< downloaded playlist contents */
	
	double mProgramDateTime;
	GrowableBuffer index; 			/**< packed IndexNode records for associated playlist */
	int indexCount; 				/**< number of indexed fragments in currently indexed playlist */
	int currentIdx; 				/**< index for currently-presenting fragment used during FF/REW (-1 if undefined) */
	std::string mFragmentURIFromIndex; /**< storage for uri generated by GetFragmentUriFromIndex */
	long long indexFirstMediaSequenceNumber; /**< first media sequence number from indexed manifest */

	char *fragmentURI; /**< pointer (into playlist) to URI of current fragment-of-interest */
	long long lastPlaylistDownloadTimeMS; /**< UTC time at which playlist was downloaded */
	size_t byteRangeLength; /**< state for \#EXT-X-BYTERANGE fragments */
	size_t byteRangeOffset; /**< state for \#EXT-X-BYTERANGE fragments */

	long long nextMediaSequenceNumber; /**< media sequence number following current fragment-of-interest */
	double playlistPosition; /**< playlist-relative time of most recent fragment-of-interest; -1 if undefined */
	double playTarget; /**< initially relative seek time (seconds) based on playlist window, but updated as a play_target */
	double playTargetBufferCalc;
	double lastDownloadedIFrameTarget;	/**< stores last downloaded iframe segment target value for comparison */ 
	double targetDurationSeconds; /**< copy of \#EXT-X-TARGETDURATION to manage playlist refresh frequency */
	int mDeferredDrmKeyMaxTime;	 /**< copy of \#EXT-X-X1-LIN DRM refresh randomization Max time interval */
	StreamOutputFormat streamOutputFormat; /**< type of data encoded in each fragment */
	MediaProcessor* playContext; /**< state for s/w demuxer / pts/pcr restamper module */
	double startTimeForPlaylistSync; /**< used for time-based track synchronization when switching between playlists */
	double playTargetOffset; /**< For correcting timestamps of streams with audio and video tracks */
	bool discontinuity; /**< Set when discontinuity is found in track*/
	StreamAbstractionAAMP_HLS* context; /**< To get  settings common across tracks*/
	bool fragmentEncrypted; /**< In DAI, ad fragments can be clear. Set if current fragment is encrypted*/
	bool mKeyTagChanged;	/**< Flag to indicate Key tag got changed for decryption context setting */
	int mLastKeyTagIdx ;     /**< Variable to hold the last keyTag index,to check if key tag changed */
	struct DrmInfo mDrmInfo;	/**< Structure variable to hold Drm Information */
	char* mCMSha1Hash;	/**< variable to store ShaID*/
	long long mDrmTimeStamp;	/**< variable to store Drm Time Stamp */
	int mDrmMetaDataIndexPosition;	/**< Variable to store Drm Meta data Index position*/
	GrowableBuffer mDrmMetaDataIndex;  /**< DrmMetadata records for associated playlist */
	int mDrmMetaDataIndexCount; /**< number of DrmMetadata records in currently indexed playlist */
	int mDrmKeyTagCount;  /**< number of EXT-X-KEY tags present in playlist */
	bool mIndexingInProgress;  /**< indicates if indexing is in progress*/
	GrowableBuffer mDiscontinuityIndex;  /**< discontinuity start position mapping of associated playlist */
	int mDiscontinuityIndexCount; /**< number of records in discontinuity position index */
	bool mDiscontinuityCheckingOn;
	double mDuration;  /** Duration of the track*/
	typedef std::vector<KeyTagStruct> KeyHashTable;
	typedef std::vector<KeyTagStruct>::iterator KeyHashTableIter;
	KeyHashTable mKeyHashTable;
	bool mCheckForInitialFragEnc;  /**< Flag that denotes if we should check for encrypted init header and push it to GStreamer*/
	DrmKeyMethod mDrmMethod;  /**< denotes the X-KEY method for the fragment of interest */

private:
	bool refreshPlaylist;	/**< bool flag to indicate if playlist refresh required or not */
	pthread_t fragmentCollectorThreadID;	/**< Thread Id for Fragment  collector Thread */
	bool fragmentCollectorThreadStarted;	/**< Flag indicating if fragment collector thread started or not*/
	int manifestDLFailCount;				/**< Manifest Download fail count for retry*/
	bool firstIndexDone;                    /**< Indicates if first indexing is done*/
	std::shared_ptr<HlsDrmBase> mDrm;       /**< DRM decrypt context*/
	bool mDrmLicenseRequestPending;         /**< Indicates if DRM License Request is Pending*/
	bool mInjectInitFragment;               /**< Indicates if init fragment injection is required*/
	const char* mInitFragmentInfo;          /**< Holds init fragment Information index*/
	bool mForceProcessDrmMetadata;          /**< Indicates if processing drm metadata to be forced on indexing*/
	pthread_mutex_t mPlaylistMutex;         /**< protect playlist update */
	pthread_cond_t mPlaylistIndexed;        /**< Notifies after a playlist indexing operation */
	pthread_mutex_t mTrackDrmMutex;         /**< protect DRM Interactions for the track */
	double mLastMatchedDiscontPosition;     /**< Holds discontinuity position last matched  by other track */
	double mCulledSeconds;                  /**< Total culled duration in this streamer instance*/
	double mCulledSecondsOld;                  /**< Total culled duration in this streamer instance*/
	bool mSyncAfterDiscontinuityInProgress; /**< Indicates if a synchronization after discontinuity tag is in progress*/
	PlaylistType mPlaylistType;		/**< Playlist Type */
	bool mReachedEndListTag;		/**< Flag indicating if End list tag reached in parser */
	bool mByteOffsetCalculation;            /**< Flag used to calculte byte offset from byte length for fragmented streams */
	bool mSkipAbr;                          /**< Flag that denotes if previous cached fragment is init fragment or not */
	const char* mFirstEncInitFragmentInfo;  /**< Holds first encrypted init fragment Information index*/
	double mXStartTimeOFfset;		/**< Holds value of time offset from X-Start tag */
	double mCulledSecondsAtStart;		/**< Total culled duration with this asset prior to streamer instantiation*/
	bool mSkipSegmentOnError;				/**< Flag used to enable segment skip on fetch error */
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
	/// Function to to handle parse and indexing of individual tracks 
	void IndexPlaylist(TrackState *trackState);
	/// Constructor 
	StreamAbstractionAAMP_HLS(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
	/// Copy Constructor
	StreamAbstractionAAMP_HLS(const StreamAbstractionAAMP_HLS&) = delete;
	/// Destructor 
	~StreamAbstractionAAMP_HLS();
	/// Assignment operator overloading
	StreamAbstractionAAMP_HLS& operator=(const StreamAbstractionAAMP_HLS&) = delete;
	/// Function to log all video/audio profiles 
	void DumpProfiles(void);
	//void SetRate(float rate, double seek_pos );
	/// Function to start processing of all tracks with stream 
	void Start();
	/// Function to handle stop processing of all tracks within stream
	void Stop(bool clearChannelData);
	/// Function to initialize member variables,download main manifest and parse
	AAMPStatusType Init(TuneType tuneType);
	/// Function to get stream format 
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat);
	/// Function to return current playing position of stream 
	double GetStreamPosition() { return seekPosition; }
	/// Function to return first PTS 
	double GetFirstPTS();
	/// Function to return start time of first PTS
	double GetStartTimeOfFirstPTS() { return 0.0; }
	/// Function to return the MediaTrack instance for the media type input 
	MediaTrack* GetMediaTrack(TrackType type);
	/// Function to return Bandwidth index for the bitrate value 
	int GetBWIndex(long bitrate);
	/// Function to get available video bitrates.
	std::vector<long> GetVideoBitrates(void);
	/// Function to get available audio bitrates.
	std::vector<long> GetAudioBitrates(void);
	/// Function to get the Media count 
	int GetMediaCount(void) { return mMediaCount;}	
	/// Function to filter the audio codec based on the configuration
	bool FilterAudioCodecBasedOnConfig(StreamOutputFormat audioFormat);
	// Function to update seek position
	void SeekPosUpdate(double secondsRelativeToTuneTime);
	/// Function to initiate precaching of playlist
	void PreCachePlaylist();	
	double GetBufferedDuration();
	/// Function to get the language code
	std::string GetLanguageCode( int iMedia );
	int GetBestAudioTrackByLanguage( void );
	/// Function to get the thumbnail rates.
	std::vector<StreamInfo*> GetAvailableThumbnailTracks(void);
	// Function to set the thumbnail resolution.
	bool SetThumbnailTrack(int);
	// Function to get the Thumbnail Information.
	std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*);
	// Function to parse the Thumbnail Manifest and extract Tile information.
	std::map<std::string,double> GetImageRangeString(double*, std::string, TileInfo*, double);
	GrowableBuffer thumbnailManifest;	/**< Thumbnail manifest buffer holder */
	std::vector<TileInfo> indexedTileInfo;	/**< Indexed Thumbnail information */
	
	void NotifyPlaybackPaused(bool pause) override;

	// Function to get the total number of profiles
	int GetTotalProfileCount() { return mProfileCount;}
//private:
	// TODO: following really should be private, but need to be accessible from callbacks
	
	TrackState* trackState[AAMP_TRACK_COUNT];		/**< array to store all tracks of a stream */
	float rate;										/**< Rate of playback  */
	float maxIntervalBtwPlaylistUpdateMs;			/**< Interval between playlist update */
	GrowableBuffer mainManifest;					/**< Main manifest buffer holder */
	bool allowsCache;								/**< Flag indicating if playlist needs to be cached or not */
	HlsStreamInfo streamInfo[MAX_PROFILE];			/**< Array to store multiple stream information */
	MediaInfo mediaInfo[MAX_PROFILE];				/**< Array to store multiple media within stream */

	double seekPosition;							/**< Seek position for playback */
	double midSeekPtsOffset;							/**< PTS offset for Mid Fragment seek  */
	int mTrickPlayFPS;								/**< Trick play frames per stream */
	bool enableThrottle;							/**< Flag indicating throttle enable/disable */
	bool firstFragmentDecrypted;					/**< Flag indicating if first fragment is decrypted for stream */
	bool mStartTimestampZero;						/**< Flag indicating if timestamp to start is zero or not (No audio stream) */
	int mNumberOfTracks;							/**< Number of media tracks.*/
	/// Function to parse Main manifest 
	AAMPStatusType ParseMainManifest();
	/// Function to get playlist URI for the track type 
	const char *GetPlaylistURI(TrackType trackType, StreamOutputFormat* format = NULL);
	int lastSelectedProfileIndex; 	/**< Variable  to restore in case of playlist download failure */

	/// Stop injection of fragments.
	void StopInjection(void);
	/// Start injection of fragments.
	void StartInjection(void);
	/// Function to check for live status comparing both playlist ( audio & video).Kept public as its called from outside StreamAbstraction class
	bool IsLive();

	/// Function to notify first video pts value from tsprocessor/demux. Kept public as its called from outside StreamAbstraction class
	void NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale);

	// Signals subtitle renderer to begin presentation
	void StartSubtitleParser() override;
	
	/// Function to get matching mediaInfo index for a language and track type
	int GetMediaIndexForLanguage(std::string lang, TrackType type);

	/// Function to get output format for track type
	StreamOutputFormat GetStreamOutputFormatForTrack(TrackType type);

protected:
	/// Function to get streamInfo for the profileIndex
	StreamInfo* GetStreamInfo(int idx);
private:
	/// Function to Synchronize timing of Audio /Video for live streams 
	AAMPStatusType SyncTracks(void);
	/// Function to update play target based on audio video exact discontinuity positions.
	void CheckDiscontinuityAroundPlaytarget(void);
	/// Function to Synchronize timing of Audio/ Video for streams with discontinuities and uneven track length.
	AAMPStatusType SyncTracksForDiscontinuity();
	/// Populate audio and text track info structures
	void PopulateAudioAndTextTracks();

	/// Function to select the audio track and update AudioProfileIndex
	void ConfigureAudioTrack();
	/// Function to select the best match video profiles based on audio and filters
	void ConfigureVideoProfiles();
	/// Function to select the text track and update TextTrackProfileIndex
	void ConfigureTextTrack();

	int segDLFailCount;						/**< Segment Download fail count */
	int segDrmDecryptFailCount;				/**< Segment Decrypt fail count */
	int mMediaCount;						/**< Number of media in the stream */
	int mProfileCount;						/**< Number of Video/Iframe in the stream */
	bool mIframeAvailable;					/**< True if iframe available in the stream */
	std::set<std::string> mLangList; /**< Available language list */
	double mFirstPTS; /**< First video PTS in seconds */
};

#endif // FRAGMENTCOLLECTOR_HLS_H
