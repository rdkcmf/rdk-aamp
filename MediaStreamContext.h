#ifndef MEDIASTREAMCONTEXT_H
#define MEDIASTREAMCONTEXT_H

#include "StreamAbstractionAAMP.h"
#include "fragmentcollector_mpd.h"
#include "isobmff/isobmffbuffer.h"
/**
 * @class MediaStreamContext
 * @brief MPD media track
 */
class MediaStreamContext : public MediaTrack
{
public:
    /**
     * @brief MediaStreamContext Constructor
     * @param type Type of track
     * @param context  MPD collector context
     * @param aamp Pointer to associated aamp instance
     * @param name Name of the track
     */
    MediaStreamContext(AampLogManager *logObj, TrackType type, StreamAbstractionAAMP_MPD* ctx, PrivateInstanceAAMP* aamp, const char* name) :
            MediaTrack(logObj, type, aamp, name),
            mediaType((MediaType)type), adaptationSet(NULL), representation(NULL),
            fragmentIndex(0), timeLineIndex(0), fragmentRepeatCount(0), fragmentOffset(0),
            eos(false), fragmentTime(0), periodStartOffset(0), timeStampOffset(0), index_ptr(NULL), index_len(0),
            lastSegmentTime(0), lastSegmentNumber(0), lastSegmentDuration(0), adaptationSetIdx(0), representationIndex(0), profileChanged(true),
            adaptationSetId(0), fragmentDescriptor(), context(ctx), initialization(""),
            mDownloadedFragment(), discontinuity(false), mSkipSegmentOnError(true),
            downloadedDuration(0)
	   , scaledPTO(0)
	   , startNumberOffset(0)
    {
        memset(&mDownloadedFragment, 0, sizeof(GrowableBuffer));
        fragmentDescriptor.bUseMatchingBaseUrl = ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
    }

    /**
     * @brief MediaStreamContext Destructor
     */
    ~MediaStreamContext()
    {
        aamp_Free(&mDownloadedFragment);
    }

    /**
     * @brief MediaStreamContext Copy Constructor
     */
     MediaStreamContext(const MediaStreamContext&) = delete;

    /**
     * @brief MediaStreamContext Assignment operator overloading
     */
     MediaStreamContext& operator=(const MediaStreamContext&) = delete;


    /**
     * @brief Get the context of media track. To be implemented by subclasses
     * @retval Context of track.
     */
    StreamAbstractionAAMP* GetContext()
    {
        return context;
    }

    /**
     * @brief Receives cached fragment and injects to sink.
     *
     * @param[in] cachedFragment - contains fragment to be processed and injected
     * @param[out] fragmentDiscarded - true if fragment is discarded.
     */
    void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded);

    /**
     * @brief Fetch and cache a fragment
     * @param fragmentUrl url of fragment
     * @param curlInstance curl instance to be used to fetch
     * @param position position of fragment in seconds
     * @param duration duration of fragment in seconds
     * @param range byte range
     * @param initSegment true if fragment is init fragment
     * @param discontinuity true if fragment is discontinuous
     * @param  playingAd flag if playing Ad
     * @param pto unscaled pto value from mpd
     * @param scale timeScale value from mpd
     * @retval true on success
     */
    bool CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range = NULL, bool initSegment = false, bool discontinuity = false, bool playingAd = false, double pto = 0, uint32_t scale = 0);

    /**
     * @brief Cache Fragment Chunk
     * @param MediaType type of cached media
     * @param char* CURL provided chunk data
     * @param size_t CURL provided chunk data size
     * @param fragmentUrl url of fragment
     */
    bool CacheFragmentChunk(MediaType actualType, char *ptr, size_t size, std::string remoteUrl);

    /**
     * @brief Listener to ABR profile change
     */
    void ABRProfileChanged(void);

    double GetBufferedDuration();

    /**
     * @brief Notify discontinuity during trick-mode as PTS re-stamping is done in sink
     */
    void SignalTrickModeDiscontinuity();

    /**
     * @brief Returns if the end of track reached.
     */
    bool IsAtEndOfTrack();

    MediaType mediaType;
    struct FragmentDescriptor fragmentDescriptor;
    const IAdaptationSet *adaptationSet;
    const IRepresentation *representation;
    int fragmentIndex;
    int timeLineIndex;
    int fragmentRepeatCount;
    uint64_t fragmentOffset;
    bool eos;
    bool profileChanged;
    bool discontinuity;
    GrowableBuffer mDownloadedFragment;

    double fragmentTime;
    double downloadedDuration;
    double periodStartOffset;
    uint64_t timeStampOffset;
    char *index_ptr;
    size_t index_len;
    uint64_t lastSegmentTime;
    uint64_t lastSegmentNumber;
    uint64_t startNumberOffset;
    uint64_t lastSegmentDuration;
    int adaptationSetIdx;
    int representationIndex;
    StreamAbstractionAAMP_MPD* context;
    std::string initialization;
    uint32_t adaptationSetId;
    bool mSkipSegmentOnError;
    double scaledPTO;
}; // MediaStreamContext

#endif /* MEDIASTREAMCONTEXT_H */

