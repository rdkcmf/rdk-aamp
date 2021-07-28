#include "MediaStreamContext.h"
#include "AampMemoryUtils.h"

/**
 * @brief Receives cached fragment and injects to sink.
 *
 * @param[in] cachedFragment - contains fragment to be processed and injected
 * @param[out] fragmentDiscarded - true if fragment is discarded.
 */
void MediaStreamContext::InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded)
{
    //FN_TRACE_F_MPD( __FUNCTION__ );
    if(!(aamp->GetLLDashServiceData()->lowLatencyMode  && (cachedFragment->type == eMEDIATYPE_AUDIO ||
                                                           cachedFragment->type == eMEDIATYPE_VIDEO)))
    {
        aamp->SendStreamTransfer((MediaType)type, &cachedFragment->fragment,
        cachedFragment->position, cachedFragment->position, cachedFragment->duration);
    }
    else
    {
        AAMPLOG_TRACE("%s:%d Full Fragment Send Ignored in LL Mode", __FUNCTION__, __LINE__);
    }
    fragmentDiscarded = false;
} // InjectFragmentInternal

/**
 * @brief Fetch and cache a fragment
 * @param fragmentUrl url of fragment
 * @param curlInstance curl instance to be used to fetch
 * @param position position of fragment in seconds
 * @param duration duration of fragment in seconds
 * @param range byte range
 * @param initSegment true if fragment is init fragment
 * @param discontinuity true if fragment is discontinuous
 * * @param  cache fragment Media type
 * @retval true on success
 */
bool MediaStreamContext::CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range, bool initSegment, bool discontinuity
    , bool playingAd)
{
    // FN_TRACE_F_MPD( __FUNCTION__ );
    bool ret = false;

    fragmentDurationSeconds = duration;
    ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
    CachedFragment* cachedFragment = GetFetchBuffer(true);
    long http_code = 0;
    long bitrate = 0;
    double downloadTime = 0;
    MediaType actualType = (MediaType)(initSegment?(eMEDIATYPE_INIT_VIDEO+mediaType):mediaType); //Need to revisit the logic

    cachedFragment->type = actualType;

    if(!initSegment && mDownloadedFragment.ptr)
    {
        ret = true;
        cachedFragment->fragment.ptr = mDownloadedFragment.ptr;
        cachedFragment->fragment.len = mDownloadedFragment.len;
        cachedFragment->fragment.avail = mDownloadedFragment.avail;
        memset(&mDownloadedFragment, 0, sizeof(GrowableBuffer));
    }
    else
    {
        std::string effectiveUrl;
        int iFogError = -1;
        int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
        ret = aamp->LoadFragment(bucketType, fragmentUrl,effectiveUrl, &cachedFragment->fragment, curlInstance,
                    range, actualType, &http_code, &downloadTime, &bitrate, &iFogError, fragmentDurationSeconds );

        if (iCurrentRate != AAMP_NORMAL_PLAY_RATE)
        {
            if(actualType == eMEDIATYPE_VIDEO)
            {
                actualType = eMEDIATYPE_IFRAME;
            }
            else if(actualType == eMEDIATYPE_INIT_VIDEO)
            {
                actualType = eMEDIATYPE_INIT_IFRAME;
            }
            //CID:101284 - To resolve the deadcode
        }

        //update videoend info
        aamp->UpdateVideoEndMetrics( actualType,
                                bitrate? bitrate : fragmentDescriptor.Bandwidth,
                                (iFogError > 0 ? iFogError : http_code),effectiveUrl,duration, downloadTime);
    }

    context->mCheckForRampdown = false;
    if(bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth)
    {
        AAMPLOG_INFO("%s:%d Bitrate changed from %u to %ld", __FUNCTION__, __LINE__, fragmentDescriptor.Bandwidth, bitrate);
        fragmentDescriptor.Bandwidth = bitrate;
        context->SetTsbBandwidth(bitrate);
        mDownloadedFragment.ptr = cachedFragment->fragment.ptr;
        mDownloadedFragment.avail = cachedFragment->fragment.avail;
        mDownloadedFragment.len = cachedFragment->fragment.len;
        memset(&cachedFragment->fragment, 0, sizeof(GrowableBuffer));
        ret = false;
    }
    else if (!ret)
    {
        aamp_Free(&cachedFragment->fragment);
        if( aamp->DownloadsAreEnabled())
        {
            logprintf("%s:%d %sfragment fetch failed -- fragmentUrl %s", __FUNCTION__, __LINE__, (initSegment)?"Init ":" ", fragmentUrl.c_str());
            if (mSkipSegmentOnError)
            {
                // Skip segment on error, and increse fail count
                segDLFailCount += 1;
            }
            else
            {
                // Rampdown already attempted on same segment
                // Reset flag for next fetch
                mSkipSegmentOnError = true;
            }
            if (MAX_SEG_DOWNLOAD_FAIL_COUNT <= segDLFailCount)
            {
                if(!playingAd)    //If playingAd, we are invalidating the current Ad in onAdEvent().
                {
                    if (!initSegment)
                    {
                        AAMPLOG_ERR("%s:%d Not able to download fragments; reached failure threshold sending tune failed event",__FUNCTION__, __LINE__);
                        aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, http_code);
                    }
                    else
                    {
                        // When rampdown limit is not specified, init segment will be ramped down, this wil
                        AAMPLOG_ERR("%s:%d Not able to download init fragments; reached failure threshold sending tune failed event",__FUNCTION__, __LINE__);
                        aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
                    }
                }
            }
            // DELIA-32287 - Profile RampDown check and rampdown is needed only for Video . If audio fragment download fails
            // should continue with next fragment,no retry needed .
            else if ((eTRACK_VIDEO == type) && !(context->CheckForRampDownLimitReached()))
            {
                // Attempt rampdown
                if (context->CheckForRampDownProfile(http_code))
                {
                    context->mCheckForRampdown = true;
                    if (!initSegment)
                    {
                        // Rampdown attempt success, download same segment from lower profile.
                        mSkipSegmentOnError = false;
                    }
                    AAMPLOG_WARN( "StreamAbstractionAAMP_MPD::%s:%d > Error while fetching fragment:%s, failedCount:%d. decrementing profile",
                            __FUNCTION__, __LINE__, fragmentUrl.c_str(), segDLFailCount);
                }
                else
                {
                    if(!playingAd && initSegment)
                    {
                        // Already at lowest profile, send error event for init fragment.
                        AAMPLOG_ERR("%s:%d Not able to download init fragments; reached failure threshold sending tune failed event",__FUNCTION__, __LINE__);
                        aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
                    }
                    else
                    {
                        AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s:%d Already at the lowest profile, skipping segment", __FUNCTION__,__LINE__);
                        context->mRampDownCount = 0;
                    }
                }
            }
            else if (AAMP_IS_LOG_WORTHY_ERROR(http_code))
            {
                AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s:%d > Error on fetching %s fragment. failedCount:%d",
                        __FUNCTION__, __LINE__, name, segDLFailCount);
                // For init fragment, rampdown limit is reached. Send error event.
                if(!playingAd && initSegment)
                {
                    aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
                }
            }
        }
    }
    else
    {
        cachedFragment->position = position;
        cachedFragment->duration = duration;
        cachedFragment->discontinuity = discontinuity;
#ifdef AAMP_DEBUG_INJECT
        if (discontinuity)
        {
            logprintf("%s:%d Discontinuous fragment", __FUNCTION__, __LINE__);
        }
        if ((1 << type) & AAMP_DEBUG_INJECT)
        {
            cachedFragment->uri.assign(fragmentUrl);
        }
#endif
        segDLFailCount = 0;
        if ((eTRACK_VIDEO == type) && (!initSegment))
        {
            // reset count on video fragment success
            context->mRampDownCount = 0;
        }
        UpdateTSAfterFetch();
        ret = true;
    }
    return ret;
}

bool MediaStreamContext::CacheFragmentChunk(MediaType actualType, char *ptr, size_t size, std::string remoteUrl)
{
    AAMPLOG_TRACE("%s:%d [%s] Chunk Buffer Length %d Remote URL %s", __FUNCTION__, __LINE__,name, size, remoteUrl.c_str());

    bool ret = true;
    if (WaitForCachedFragmentChunkInjected())
    {
        CachedFragmentChunk* cachedFragmentChunk = NULL;
        cachedFragmentChunk = GetFetchChunkBuffer(true);
        if(NULL == cachedFragmentChunk)
        {
                AAMPLOG_WARN("%s:%d [%s] Something Went wrong - Can't get FetchChunkBuffer",__FUNCTION__, __LINE__,name);
                return false;
        }
        cachedFragmentChunk->type = actualType;
        aamp_AppendBytes(&cachedFragmentChunk->fragmentChunk, ptr, size);

        AAMPLOG_TRACE("%s:%d [%s] cachedFragmentChunk %p ptr %p", __FUNCTION__, __LINE__,
                name, cachedFragmentChunk, cachedFragmentChunk->fragmentChunk.ptr);

        UpdateTSAfterChunkFetch();
    }
    else
    {
        logprintf("%s:%d [%s] WaitForCachedFragmentChunkInjected aborted", __FUNCTION__, __LINE__, name);
        ret = false;
    }
    return ret;
}

/**
 * @brief Listener to ABR profile change
 */
void MediaStreamContext::ABRProfileChanged(void)
{
    struct ProfileInfo profileMap = context->GetAdaptationSetAndRepresetationIndicesForProfile(context->currentProfileIndex);
    // Get AdaptationSet Index and Representation Index from the corresponding profile
    int adaptIdxFromProfile = profileMap.adaptationSetIndex;
    int reprIdxFromProfile = profileMap.representationIndex;
    if (!((adaptationSetIdx == adaptIdxFromProfile) && (representationIndex == reprIdxFromProfile)))
    {
        const IAdaptationSet *pNewAdaptationSet = context->GetAdaptationSetAtIndex(adaptIdxFromProfile);
        IRepresentation *pNewRepresentation = pNewAdaptationSet->GetRepresentation().at(reprIdxFromProfile);
        if(representation != NULL)
        {
            logprintf("StreamAbstractionAAMP_MPD::%s:%d - ABR %dx%d[%d] -> %dx%d[%d]", __FUNCTION__, __LINE__,
                    representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth(),
                    pNewRepresentation->GetWidth(), pNewRepresentation->GetHeight(), pNewRepresentation->GetBandwidth());
            adaptationSetIdx = adaptIdxFromProfile;
            adaptationSet = pNewAdaptationSet;
            adaptationSetId = adaptationSet->GetId();
            representationIndex = reprIdxFromProfile;
            representation = pNewRepresentation;
            const std::vector<IBaseUrl *>*baseUrls = &representation->GetBaseURLs();
            if (baseUrls->size() != 0)
            {
                fragmentDescriptor.SetBaseURLs(baseUrls);
            }
            fragmentDescriptor.Bandwidth = representation->GetBandwidth();
            fragmentDescriptor.RepresentationID.assign(representation->GetId());
            profileChanged = true;
        }
        else
        {
            AAMPLOG_WARN("%s:%d :  representation is null", __FUNCTION__, __LINE__);  //CID:83962 - Null Returns
        }
    }
    else
    {
        traceprintf("StreamAbstractionAAMP_MPD::%s:%d - Not switching ABR %dx%d[%d] ", __FUNCTION__, __LINE__,
                representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth());
    }

}

double MediaStreamContext::GetBufferedDuration()
{
    double position = aamp->GetPositionMs() / 1000.00;
    if(downloadedDuration >= position)
    {
        // If player faces buffering, this will be 0
        return (downloadedDuration - position);
    }
    else
    {
        // This avoids negative buffer, expecting
        // downloadedDuration never exceeds position in normal case.
        // Other case happens when contents are yet to be injected.
        downloadedDuration = 0;
        return downloadedDuration;
    }
}


/**
 * @brief Notify discontinuity during trick-mode as PTS re-stamping is done in sink
 */
void MediaStreamContext::SignalTrickModeDiscontinuity()
{
     //       FN_TRACE_F_MPD( __FUNCTION__ );
    aamp->SignalTrickModeDiscontinuity();
}

/**
 * @brief Returns if the end of track reached.
 */
bool MediaStreamContext::IsAtEndOfTrack()
{
    //        FN_TRACE_F_MPD( __FUNCTION__ );
    return eosReached;
}


