marker_dash = [{
		"pattern":"bad fragment duration",
		"label":"Bad frag"
},
	{
		"pattern":"Wrong size in ParseSegmentIndexBox",
		"label":"SegIndx wrong size"
},
	{
		"pattern":"Wrong type in ParseSegmentIndexBox",
		"label":"SegIndx wrong type",
},
	{
		"pattern":"failed. fragmentUrl ",
		"label":"Frag fetch failed"
},
	{
		"pattern":"Ad fragment not available. Playback failed.",
		"label":"AD frag NA"
},
	{
		"pattern":"Setting startTime to %%",
		"label":"StartTime:%0"
},
	{
		"pattern":"Skipping Fetchfragment, Number(%%) fragment beyond duration",
		"label":"Skip fetch frag"
},
	{
		"pattern":"ScheduleRetune to handle start-time reset ",
		"label":"Retune start time reset"
},
	{
		"pattern":"segmentTimeline not available",
		"label":"SegTimeline NA"
},
	{
		"pattern":"START-TIME RESET in TSB period",
		"label":"Start time reset TSB"
},
	{
		"pattern":"SegmentUrl is empty",
		"label":"SegmentUrl empty"
},
	{
		"pattern":"base64_Decode of pssh resulted in 0 length",
		"label":"PSSH len 0"
},
	{
		"pattern":"Failed to locate DRM helper",
		"label":"Fail locate DRM helper"
},
	{
		"pattern":"Failed to Parse PSSH ",
		"label":"Fail PSSH parse"
},
	{
		"pattern":"No PSSH data available from the stream",
		"label":"No PSSH"
},
	{
		"pattern":"Skipping creation of session for duplicate helper",
		"label":"Skip session creation"
},
	{
		"pattern":"MPD DRM not enabled",
		"label":"MPD DRM not enabled"
},
	{
		"pattern":"required to calculate period duration",
		"label":"Data req to cal period duration"
},
	{
		"pattern":"Could not get valid period duration to calculate end time",
		"label":"No valid period duration"
},
	{
		"pattern":"Period startTime is not present in MPD, so calculating start time with previous period durations",
		"label":"Period startTime not in MPD"
},
	{
		"pattern":"mediaPresentationDuration missing in period",
		"label":"Duration missed in period"
},
	{
		"pattern":"Invalid period duration periodStartTime",
		"label":"Invalid-Duration"
},
	{
		"pattern":"Next period startTime missing periodIndex",
		"label":"Next-Period-Start-Missed"
},
	{
		"pattern":"Start time and duration missing in period",
		"label":"Start-Time-Duration-Missed"
},
	{
		"pattern":"GAP betwen period found",
		"label":"GAP-In-Period"
},
	{
		"pattern":"Duration of VOD content is 0",
		"label":"VOD Duration 0"
},
	{
		"pattern":"seek target out of range, mark EOS.",
		"label":"Seek out of range"
},
	{
		"pattern":"No adaptation sets could be selected",
		"label":"No adaptation set selected"
},
	{
		"pattern":"Pushing EncryptedHeaders",
		"label":"Push Enc headers"
},
	{
		"pattern":"Pushing encrypted header",
		"label":"Push Enc headers"
},
	{
		"pattern":"is null",
		"label":"Null Data!!"
},
	{
		"pattern":"manifest retrieved from cache",
		"label":"Manifest from cache"
},
	{
		"pattern":"Ignore curl timeout",
		"label":"Ignore curl timeout"
},
	{
		"pattern":"manifest download failed",
		"label":"Manifest download failed"
},
	{
		"pattern":"Error while processing MPD",
		"label":"Error processing MPD"
},
	{
		"pattern":"error on manifest fetch",
		"label":"Manifest fetch error"
},
	{
		"pattern":"Invalid Playlist URL",
		"label":"Invalid playlist URL"
},
	{
		"pattern":"Invalid Playlist DATA",
		"label":"Invalid playlist Data"
},
	{
		"pattern":"node is null",
		"label":"Null Node!!"
},
	{
		"pattern":"Found CDAI events for period",
		"label":"CDAI evets found"
},
	{
		"pattern":"Unable to get audioAdaptationSet",
		"label":"Unable to get audio adapt"
},
	{
		"pattern":"Text Track not found",
		"label":"Text not found"
},
	{
		"pattern":"Detected encrypted iframe track",
		"label":"Detected enc iframe"
},
	{
		"pattern":"Selected Audio Track codec is unknown",
		"label":"Audio codec unknown"
},
	{
		"pattern":"AudioType Changed %% -> %%",
		"label":"AudioType %0 -> %1",
},
	{
		"pattern":"No valid adaptation set found for Media[%%]",
		"label":"No valid adapt(%0)"
},
	{
		"pattern":"Audio only period",
		"label":"Audio only period"
},
	{
		"pattern":"No profiles found",
		"label":"No profile found"
},
	{
		"pattern":"Adbreak ended early",
		"label":"AD ends early"
},
	{
		"pattern":"Failed to get keyID for vss common key EAP",
		"label":"Failed to get VSS kep"
},
	{
		"pattern":"Discontinuity process is yet to complete",
		"label":"Yet to compl discontinuity"
},
	{
		"pattern":"Period ID changed from '%%' to '%%'",
		"label":"Period ID changed %0 -> %1"
},
	{
		"pattern":"playing period %%",
		"label":"Playing period %0"
},
	{
		"pattern":"Period ID not changed from",
		"label":"Period ID not changed"
},
	{
		"pattern":"Change in AdaptationSet count",
		"label":"Change in adapt count"
},
	{
		"pattern":"AdaptationSet index changed; updating stream selection",
		"label":"Change in adapt index"
},
	{
		"pattern":"Audio or Video track missing in period, ignoring discontinuity",
		"label":"AV track missing in period"
},
	{
		"pattern":"discontinuity detected",
		"label":"Discontinuity detected"
},
	{
		"pattern":"No discontinuity detected",
		"label":"No discontinuity detected"
},
	{
		"pattern":"MPD Fragment Collector detected reset in Period",
		"label":"Reset in period"
},
	{
		"pattern":"Exiting. Thumbnail track not configured",
		"label":"thumbnail track not configured"
},
	{
		"pattern":"Found a scte35:Binary in manifest with empty binary data",
		"label":"CDAI: Manifest with empty bin data"
},
	{
		"pattern":"Found a scte35:Signal in manifest without scte35:Binary",
		"label":"CDAI: Manifest without bin data"
},
	{
		"pattern":"STARTING ADBREAK",
		"label":"CDAI: Started AD"
},
	{
		"pattern":"AdIdx[%%] in the AdBreak[%%] is invalid",
		"label":"CDAI: AD invalid"
},
	{
		"pattern":"BasePeriodId in Adbreak. But Ad not available",
		"label":"CDAI: AD not available"
},
	{
		"pattern":"AdIdx[%%] Found at Period",
		"label":"CDAI: AD found"
},
	{
		"pattern":"ADBREAK[%%] ENDED.",
		"label":"CDAI: AD ended"
},
	{
		"pattern":"Ad finished at Period. Waiting to catchup the base offset",
		"label":"CDAI: AD finished, waiting for base offset"
},
	{
		"pattern":"Ad Playback failed. Going to the base period",
		"label":"CDAI: AD failed, going to base period"
},
	{
		"pattern":"BUG! BUG!! BUG!!! We should not come here",
		"label":"Bug!!! - Check this"
},
	{
		"pattern":"Current Ad placement Completed. Ready to play next Ad.",
		"label":"CDAI: Ready to play next AD"
},
	{
		"pattern":"AdIdx is invalid. Skipping.",
		"label":"CDAI: AD Index invalid"
},
	{
		"pattern":"Next AdIdx[%%] Found at Period",
		"label":"CDAI: Next AD found"
},
	{
		"pattern":"All Ads in the ADBREAK",
		"label":"CDAI: All ADs in ADBreak"
},
	{
		"pattern":"Ad playback failed. Not able to download Ad manifest from FOG",
		"label":"CDAI: AD failed, unable to download from FOG"
},
	{
		"pattern":"State changed from [%%] => [%%]",
		"label":"CDAI: State %0 -> %1"
},
	{
		"pattern":"not supported mimeType",
		"label":"Not supported mimetype"
},
	{
		"pattern":"Empty timeScale ",
		"label":"Empty timescale"
},
	{
		"pattern":"Out of Range error",
		"label":"Out of range error"
},
	{
		"pattern":"latencyStatus = LATENCY_STATUS_UNKNOWN",
		"label":"LLD: Latency unknown"
},
	{
		"pattern":"AdjustPlayBackRate: failed",
		"label":"LLD: AdjustPlayBackRate failed"
},
	{
		"pattern":"schemeIdUri attribute not available",
		"label":"LLD: schemeIdUri NA"
},
	{
		"pattern":"schemeIdUri Value not proper",
		"label":"LLD: schemeIdUri no proper value"
},
	{
		"pattern":"UTCTiming did not Match",
		"label":"LLD: UTCTiming not match"
},
	{
		"pattern":"Scope element not available",
		"label":"LLD: Scope ele NA"
},
	{
		"pattern":"Latency element not available",
		"label":"LLD: Latency ele NA"
},
	{
		"pattern":"Latency target attribute not available",
		"label":"LLD: Latency target attr NA"
},
	{
		"pattern":"Latency max attribute not available",
		"label":"LLD: Latency max attr NA"
},
	{
		"pattern":"Latency min attribute not available",
		"label":"LLD: Latency min attr NA"
},
	{
		"pattern":"Play Rate element not available",
		"label":"LLD: Play Rate ele NA"
},
	{
		"pattern":"UTCTiming element not available",
		"label":"LLD: UTCTiming ele NA"
},
	{
		"pattern":"Preffered DRM Failed to locate with UUID",
		"label":"DRM failed to locate UUID"
},
	{
		"pattern":"No PSSH data available for Preffered DRM",
		"label":"No PSSH in DRM"
},
	{
		"pattern":"Failed to create DRM helper",
		"label":"Fail DRM helper"
},
	{
		"pattern":"not-yet-supported mpd format",
		"label":"Not suported MPD format"
},
	{
		"pattern":"No playable profiles found",
		"label":"No profile found"
},
	{
		"pattern":"corrupt/invalid manifest",
		"label":"Corrupted manifest"
},
	{
		"pattern":"Failed due to Invalid Arguments",
		"label":"Failed due to invalid arg"
}];
