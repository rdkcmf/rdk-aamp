#include "MediaStreamContext.h"
#include "AampMemoryUtils.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"

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
	aamp->ProcessID3Metadata(cachedFragment->fragment.ptr, cachedFragment->fragment.len, (MediaType) type);
	AAMPLOG_TRACE("Type[%d] cachedFragment->position: %f cachedFragment->duration: %f cachedFragment->initFragment: %d", type, cachedFragment->position,cachedFragment->duration,cachedFragment->initFragment);
        aamp->SendStreamTransfer((MediaType)type, &cachedFragment->fragment,
        cachedFragment->position, cachedFragment->position, cachedFragment->duration, cachedFragment->initFragment);
    }
    else
    {
        AAMPLOG_TRACE("Full Fragment Send Ignored in LL Mode");
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
 * @param playingAd flag if playing Ad
 * @param pto unscaled pto value from mpd
 * @param scale timeScale value from mpd
 * @retval true on success
 */
bool MediaStreamContext::CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range, bool initSegment, bool discontinuity
    , bool playingAd, double pto, uint32_t scale)
{
    // FN_TRACE_F_MPD( __FUNCTION__ );
    bool ret = false;

    AAMPLOG_TRACE("Type[%d] fragmentUrl %s fragmentTime %f discontinuity %d pto %f  scale %lu duration %f", type, fragmentUrl.c_str(), position, discontinuity, pto, scale, duration);

    fragmentDurationSeconds = duration;
    ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
    CachedFragment* cachedFragment = GetFetchBuffer(true);
    long http_code = 0;
    long bitrate = 0;
    double downloadTime = 0;
    MediaType actualType = (MediaType)(initSegment?(eMEDIATYPE_INIT_VIDEO+mediaType):mediaType); //Need to revisit the logic

    cachedFragment->type = actualType;
    cachedFragment->initFragment = initSegment;

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
        bool bReadfromcache = false;
        if(initSegment)
        {
            ret = bReadfromcache = aamp->getAampCacheHandler()->RetrieveFromInitFragCache(fragmentUrl,&cachedFragment->fragment,effectiveUrl);
        }

        if(!bReadfromcache)
        {
            ret = aamp->LoadFragment(bucketType, fragmentUrl,effectiveUrl, &cachedFragment->fragment, curlInstance,
                    range, actualType, &http_code, &downloadTime, &bitrate, &iFogError, fragmentDurationSeconds );

            if ( initSegment && ret )
            aamp->getAampCacheHandler()->InsertToInitFragCache ( fragmentUrl, &cachedFragment->fragment, effectiveUrl, actualType);
        }

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
		else
		{
			if(actualType == eMEDIATYPE_INIT_VIDEO || actualType == eMEDIATYPE_INIT_AUDIO)
			{
				//To read track_id from the init fragments to check if there any mismatch.
				//A mismatch in track_id is not handled in the gstreamer version 1.10.4
				//But is handled in the latest version (1.18.5),
				//so upon upgrade to it or introduced a patch in qtdemux,
				//this portion can be reverted
				IsoBmffBuffer buffer;
				buffer.setBuffer((uint8_t *)cachedFragment->fragment.ptr, cachedFragment->fragment.len);
				buffer.parseBuffer();
				uint32_t track_id = 0;
				buffer.getTrack_id(track_id);
				
				if(actualType == eMEDIATYPE_INIT_VIDEO)
				{
					AAMPLOG_INFO("Video track_id read from init config: %d ", track_id);
					if(aamp->mCurrentVideoTrackId != -1 && track_id != aamp->mCurrentVideoTrackId)
					{
						aamp->mIsTrackIdMismatch = true;
						AAMPLOG_WARN("TrackId mismatch detected for video, current track_id: %d, next period track_id: %d", aamp->mCurrentVideoTrackId, track_id);
					}
					aamp->mCurrentVideoTrackId = track_id;
				}
				else
				{
					AAMPLOG_INFO("Audio track_id read from init config: %d ", track_id);
					if(aamp->mCurrentAudioTrackId != -1 && track_id != aamp->mCurrentAudioTrackId)
					{
						aamp->mIsTrackIdMismatch = true;
						AAMPLOG_WARN("TrackId mismatch detected for audio, current track_id: %d, next period track_id: %d", aamp->mCurrentAudioTrackId, track_id);
					}
					aamp->mCurrentAudioTrackId = track_id;
				}
			}
		}

        if(!bReadfromcache)
        {
            //update videoend info
            aamp->UpdateVideoEndMetrics( actualType,
                                    bitrate? bitrate : fragmentDescriptor.Bandwidth,
                                    (iFogError > 0 ? iFogError : http_code),effectiveUrl,duration, downloadTime);
        }
    }

    context->mCheckForRampdown = false;
    if(bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth)
    {
        AAMPLOG_INFO("Bitrate changed from %u to %ld",fragmentDescriptor.Bandwidth, bitrate);
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
	AAMPLOG_INFO("fragment fetch failed - Free cachedFragment");
        aamp_Free(&cachedFragment->fragment);
        if( aamp->DownloadsAreEnabled())
        {
            AAMPLOG_WARN("%sfragment fetch failed -- fragmentUrl %s", (initSegment)?"Init ":" ", fragmentUrl.c_str());
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
	int FragmentDownloadFailThreshold;
	GETCONFIGVALUE(eAAMPConfig_FragmentDownloadFailThreshold,FragmentDownloadFailThreshold);
            if (FragmentDownloadFailThreshold <= segDLFailCount)
            {
                if(!playingAd)    //If playingAd, we are invalidating the current Ad in onAdEvent().
                {
                    if (!initSegment)
                    {
                        AAMPLOG_ERR("Not able to download fragments; reached failure threshold sending tune failed event");
                        aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, http_code);
                    }
                    else
                    {
                        // When rampdown limit is not specified, init segment will be ramped down, this wil
                        AAMPLOG_ERR("Not able to download init fragments; reached failure threshold sending tune failed event");
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
                    AAMPLOG_WARN( "StreamAbstractionAAMP_MPD::Error while fetching fragment:%s, failedCount:%d. decrementing profile",
                             fragmentUrl.c_str(), segDLFailCount);
                }
                else
                {
                    if(!playingAd && initSegment)
                    {
                        // Already at lowest profile, send error event for init fragment.
                        AAMPLOG_ERR("Not able to download init fragments; reached failure threshold sending tune failed event");
                        aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
                    }
                    else
                    {
                        AAMPLOG_WARN("StreamAbstractionAAMP_MPD::Already at the lowest profile, skipping segment");
                        context->mRampDownCount = 0;
                    }
                }
            }
            else if (AAMP_IS_LOG_WORTHY_ERROR(http_code))
            {
                AAMPLOG_WARN("StreamAbstractionAAMP_MPD::Error on fetching %s fragment. failedCount:%d",
                         name, segDLFailCount);
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
            AAMPLOG_WARN("Discontinuous fragment");
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
    AAMPLOG_TRACE("[%s] Chunk Buffer Length %d Remote URL %s", name, size, remoteUrl.c_str());

    bool ret = true;
    if (WaitForCachedFragmentChunkInjected())
    {
        CachedFragmentChunk* cachedFragmentChunk = NULL;
        cachedFragmentChunk = GetFetchChunkBuffer(true);
        if(NULL == cachedFragmentChunk)
        {
                AAMPLOG_WARN("[%s] Something Went wrong - Can't get FetchChunkBuffer", name);
                return false;
        }
        cachedFragmentChunk->type = actualType;
        aamp_AppendBytes(&cachedFragmentChunk->fragmentChunk, ptr, size);

        AAMPLOG_TRACE("[%s] cachedFragmentChunk %p ptr %p",
                name, cachedFragmentChunk, cachedFragmentChunk->fragmentChunk.ptr);

        UpdateTSAfterChunkFetch();
    }
    else
    {
        AAMPLOG_WARN("[%s] WaitForCachedFragmentChunkInjected aborted", name);
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
            AAMPLOG_WARN("StreamAbstractionAAMP_MPD: ABR %dx%d[%d] -> %dx%d[%d]",
                    representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth(),
                    pNewRepresentation->GetWidth(), pNewRepresentation->GetHeight(), pNewRepresentation->GetBandwidth());
            adaptationSetIdx = adaptIdxFromProfile;
            adaptationSet = pNewAdaptationSet;
            adaptationSetId = adaptationSet->GetId();
            representationIndex = reprIdxFromProfile;
            representation = pNewRepresentation;
	
			dash::mpd::IMPD *mpd = context->GetMPD();
			IPeriod *period = context->GetPeriod();
			fragmentDescriptor.ClearMatchingBaseUrl();
			fragmentDescriptor.AppendMatchingBaseUrl( &mpd->GetBaseUrls() );
			fragmentDescriptor.AppendMatchingBaseUrl( &period->GetBaseURLs() );
			fragmentDescriptor.AppendMatchingBaseUrl( &adaptationSet->GetBaseURLs() );
			fragmentDescriptor.AppendMatchingBaseUrl( &representation->GetBaseURLs() );

			fragmentDescriptor.Bandwidth = representation->GetBandwidth();
            fragmentDescriptor.RepresentationID.assign(representation->GetId());
            profileChanged = true;
        }
        else
        {
            AAMPLOG_WARN("representation is null");  //CID:83962 - Null Returns
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


