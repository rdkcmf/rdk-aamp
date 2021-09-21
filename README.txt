Advanced Adaptive Micro Player (AAMP)
=================================================================================================================

Index 
----------------------------
1. AAMP Source Overview
2. AAMP Configuration
3. Channel Override Settings
4. Westeros Settings
5. AAMP Tunetime 
6. AAMP MicroEvents 
7. VideoEnd (Session Statistics) Event 
----------------------------


---------------------------------------------------------------------------------------
1. AAMP Source Overview:

aampcli.cpp
- entry point for command line test app

aampgstplayer.cpp
- gstreamer abstraction - allows playback of unencrypted video fragments

base16, _base64
- utility functions

fragmentcollector_hls
- hls playlist parsing and fragment collection

fragmentcollector_mpd
- dash manifest parsing and fragment collection

fragmentcollector_progressive
- raw mp4 playback support

drm
- digital rights management support and plugins

=================================================================================================================
2. AAMP Configuration

AAMP Configuration can be set with different method . Below is the list (from 
lowest priority to highest priority override ownership).
	a) AAMP Default Settings within Code 
	b) AAMP Configuration from Operator ( RFC / ENV variables )
	c) AAMP Settings from Stream 
	d) AAMP Settings from Application settings 
	e) AAMP Settings from Dev configuration ( /opt/aamp.cfg - text format  , /opt/aampcfg.json - JSON format input)

Configuration Field						Description	
===============================================================================
On / OFF Switches : All Enable/Disable configuration needs true/false input .
Example : abr=false -> to disable ABR 
===============================================================================
abr				Enable/Disable adaptive bitrate logic.Default true
fog				Enable/Disable Fog .Default true
parallelPlaylistDownload	Enable parallel fetching of audio & video playlists for HLS during Tune.Default false
parallelPlaylistRefresh		Enable parallel fetching of audio & video playlists for HLS during refresh .Default true
preFetchIframePlaylist		Enable prefetching of I-Frame playlist.Default false
preservePipeline		Flush instead of teardown.Default false
demuxHlsAudioTrack		Demux Audio track from HLS transport stream.Default true
demuxHlsVideoTrack		Demux Video track from HLS transport stream.Default true
demuxHlsVideoTrackTrickMode	Demux Video track from HLS transport stream during TrickMode.Defaut true
throttle			Regulate output data flow,used with restamping. Default false
demuxAudioBeforeVideo		Demux video track from HLS transport stream track mode.Default false
stereoOnly			Enable selection of stereo only audio.It Overrides forceEC3/disableEC3/disableATMOS.Default false
forceEC3			Forcefully enable DDPlus.Default false
disableEC3			Disable DDPlus.Default false
disableATMOS			Disable Dolby ATMOS.Default false
disable4K			Disable 4K Profile playback in 4K Supported device (default false)
disablePlaylistIndexEvent	Disables generation of playlist indexed event by AAMP on tune/trickplay/seek.Default true
enableSubscribedTags		Enable subscribed tags.Default true
dashIgnoreBaseUrlIfSlash	Ignore the constructed URI of DASH.Default false
licenseAnonymousRequest		Acquire license without token.Default false
hlsAVTrackSyncUsingPDT		Use EXT-X-PROGRAM-DATE to synchronize audio and video playlists. Default false
mpdDiscontinuityHandling	Discontinuity handling during MPD period transition.Default true
mpdDiscontinuityHandlingCdvr	Discontinuity handling during MPD period transition for cDvr.Default true
forceHttp			Allow forcing of HTTP protocol for HTTPS URLs . Default false
internalRetune			Internal reTune logic on underflows/ pts errors.Default true
audioOnlyPlayback		Audio only Playback . Default false
gstBufferAndPlay		Pre-buffering which ensures minimum buffering is done before pipeline play.Default true
bulkTimedMetadata		Report timed Metadata as single bulk event.Default false
asyncTune			Asynchronous API / Event handling for UI.Default false
useWesterosSink			Enable/Disable westeros sink based video decoding. 
useNewABR			Enable/Disable New buffer based hybrid ABR . Default true (enables useNewAdBreaker & PDT based A/V sync)
useNewAdBreaker			Enable/Disable New discontinuity processing based on PDT.Default false
useAverageBandwidth		Enable/Disable use of average bandwidth in manifest for ABR instead of Bandwidth attribute . Default false
useRetuneForUnpairedDiscontinuity	Enable/Disable internal retune on unpaired discontinuity .Default true
useMatchingBaseUrl		Enable/Disable use of matching base url, whenever there are multiple base urls are available.Default false
nativeCCRendering		Enable/Disable Native CC rendering in AAMP Player.Default false
enableVideoRectangle		Enable/Disable Setting of rectangle property for sink element.Default true
useRetuneForGstInternalError	Enable/Disable Retune on GST Error.Default true
enableSeekableRange		Enable/Disable Seekable range reporting in progress event for non-fog content.Default false
reportVideoPTS 			Enable/Disable video pts reporting in progress events.Default false
propagateUriParameters		Enable/Disable top-level manifest URI parameters while downloading fragments.  Default true
sslVerifyPeer			Enable/Disable SSL Peer verification for curl connection . Default false
setLicenseCaching		Enable/Disable license caching in WV . Default true
persistBitrateOverSeek		Enable/Disable ABR profile persistence during Seek/Trickplay/Audio switching. Default false
fragmp4LicensePrefetch		Enable/Disable fragment mp4 license prefetching.Default true
gstPositionQueryEnable		GStreamer position query will be used for progress report events.Default true for non-Intel platforms
playreadyOutputProtection  	Enable/Disable HDCP output protection for DASH-PlayReady playback. Default false
enableVideoEndEvent		Enable/Disable Video End event generation; Default true
enableTuneProfiling		Enable/Disable Video End event generation; Default false
playreadyOutputProtection	Enable/Disable output protection for PlayReady DRM.Default false
descriptiveAudioTrack   	Enable/Disable role in audio track selection.syntax <langcode>-<role> instead of just <langcode>.Default false
decoderUnavailableStrict	Reports decoder unavailable GST Warning as aamp error. Default false
retuneOnBufferingTimeout 	Enable/Disable internal re-tune on buffering time-out.Default true
client-dai			Enable/Disable Client-DAI.Default false
cdnAdsOnly			Enable/Disable picking Ads from Fog or CDN . Default false
appSrcForProgressivePlayback 	Enables appsrc for playing progressive AV type.Default false
seekMidFragment			Enable/Disable Mid-Fragment seek .Default false
wifiCurlHeader			Enable/Disble wifi custom curl header inclusion.Default true
removeAVEDRMPersistent		Enable/Disable code in ave drm to avoid crash when majorerror 3321, 3328 occurs.Default false.
reportBufferEvent		Enables Buffer event reporting.Default is true.
info            		Enable/Disable logging of requested urls.Default is false
gst             		Enable/Disable gstreamer logging including pipeline dump.Default is false
progress        		Enable/Disable periodic logging of position.Default is false
trace           		Enable/Disable dumps of manifests.Default is false
curl            		Enable/Disable verbose curl logging for manifest/playlist/segment downloads.Default is false
curlLicense     		Enable/Disable verbose curl logging for license request (non-secclient).Default is false
debug           		Enable/Disable debug level logs.Default is false
logMetadata     		Enable/Disable timed metadata logging.Default is false
useLinearSimulator		Enable/Disable linear simulator for testing purpose, simulate VOD asset as a "virtual linear" stream.Default is false
dashParallelFragDownload	Enable/Disable dash fragment parallel download.Default is true
enableAccessAttributes		Enable/Disable Usage of Access Attributes in VSS.Default is true 
subtecSubtitle			Enable/Disable subtec-based subtitles.Default is false
webVttNative			Enable/Disable Native WebVTT processing.Default is false
failover			Enable/Disable failover logging.Default is false
curlHeader			Enable/Disable curl header response logging on curl errors.Default is false
stream				Enable/Disable HLS Playlist content logging.Default is false
isPreferredDRMConfigured	Check whether preferred DRM has set.Default is false
limitResolution			Check if display resolution based profile selection to be done.Default is false
disableUnderflow		Enable/Disable Underflow processing.Default is false
useAbsoluteTimeline		Enable Report Progress report position based on Availability Start Time.Default is false
id3				Enable/Disable ID3 tag.Default is false
repairIframes		Enable/Disable iframe fragment repair (stripping and box adjustment) for HLS mp4 when whole file is received for ranged request. Default is false

// Integer inputs
ptsErrorThreshold		aamp maximum number of back-to-back pts errors to be considered for triggering a retune
waitTimeBeforeRetryHttp5xx 	Specify the wait time before retry for 5xx http errors. Default wait time is 1s.
harvestCountLimit		Specify the limit of number of files to be harvested
harvestConfig			*Specify the value to indicate the type of file to be harvested. Refer table below for masking table 
bufferHealthMonitorDelay 	Override for buffer health monitor start delay after tune/ seek (in secs)
bufferHealthMonitorInterval	Override for buffer health monitor interval(in secs)
abrCacheLife 			Lifetime value for abr cache  for network bandwidth calculation(in msecs.default 5000 msec)
abrCacheLength  		Length of abr cache for network bandwidth calculation (# of segments . default 3)
abrCacheOutlier 		Outlier difference which will be ignored from network bandwidth calculation(default 5MB.in bytes)
abrNwConsistency		Number of checks before profile incr/decr by 1.This is to avoid frequenct profile switching with network change(default 2)
abrSkipDuration			Minimum duration of fragment to be downloaded before triggering abr (in secs.default 6 sec).
progressReportingInterval	Interval of progress reporting(in msecs.default is 1000 msec)
licenseRetryWaitTime		License retry wait interval(in msecs.default is 500msec)
liveOffset    			live offset time in seconds, aamp starts live playback this much time before the live point.Default 15sec
cdvrLiveOffset    		live offset time in seconds for cdvr, aamp starts live playback this much time before the live point for inprogress cdvr.Default 30 sec
liveTuneEvent 			Send streamplaying for live when 
							0 -	playlist acquired 
							1 - first fragment decrypted
							2 - first frame visible (default)

vodTuneEvent 			Send streamplaying for vod when 
							0 -	playlist acquired 
							1 - first fragment decrypted
							2 - first frame visible (default)
preferredDrm			Preferred DRM for playback  
							0 - No DRM 
							1 - Widevine
							2 - PlayReady ( Default)
							3 - Consec
							4 - AdobeAccess
							5 - Vanilla AES
							6 - ClearKey
ceaFormat				Preferred CEA option for CC. Default stream based . Override value 
							0 - CEA 608
							1 - CEA 708
							
maxPlaylistCacheSize            Max Size of Cache to store the VOD Manifest/playlist . Size in KBytes.Default is 3072.
initRampdownLimit		Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time.Default is 0 (disabled).
downloadBuffer                  Fragment cache length (defaults 3 fragments)
vodTrickPlayFps		        Specify the framerate for VOD trickplay (defaults to 4)
linearTrickPlayFps      	Specify the framerate for Linear trickplay (defaults to 8)
fragmentRetryLimit		Set fragment rampdown/retry limit for video fragment failure (default is -1).
initRampdownLimit		Maximum number of rampdown/retries for initial playlist retrieval at tune/seek time. Default is 0 (disabled).
initFragmentRetryCount	    	Max retry attempts for init frag curl timeout failures, default count is 1 (which internally means 1 download attempt and "1 retry attempt after failure").
langCodePreference		prefered format for normalizing language code.Default is 0.
initialBuffer			cached duration before playback start, in seconds. Default is 0.
maxTimeoutForSourceSetup	Timeout value wait for GStreamer appsource setup to complete.default is 1000.
drmDecryptFailThreshold		Retry count on drm decryption failure, default is 10.
segmentInjectFailThreshold	Retry count for segment injection discard/failue, default is 10.
preCachePlaylistTime		Max time to complete PreCaching default is 0 in minutes
thresholdSizeABR		ABR threshold size. Default 6000.
stallTimeout			Stall detection timeout. Default is  10sec
stallErrorCode			Stall error code.Default is  7600
minABRBufferRampdown		Mininum ABR Buffer for Rampdown.Default is 10sec
maxABRBufferRampup		Maximum ABR Buffer for Rampup.Default is 15sec
preplayBuffercount		Count of segments to be downloaded until play state.Default is 2
downloadDelay			Delay for downloads to simulate network latency.Default is 0
onTuneRate			Tune rate.Default is INT_MAX
dashMaxDrmSessions		Max drm sessions that can be cached by AampDRMSessionManager.Default is 3
log				New Configuration to overide info/debug/trace.Default is 0
livePauseBehavior               Player paused state behavior.Default is 0(ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE)

// String inputs
licenseServerUrl		URL to be used for license requests for encrypted(PR/WV) assets
mapMPD				<domain / host to map> Remap HLS playback url to DASH url for matching domain/host string (.m3u8 to .mpd)
mapM3U8				<domain / host to map> Remap DASH MPD playback url to HLS m3u8 url for matching domain/host string (.mpd to .m3u8)
harvestPath			Specify the path where fragments has to be harvested,check folder permissions specifying the path
networkProxy			proxy address to set for all file downloads. Default None  
licenseProxy			proxy address to set for licese fetch . Default None
sessionToken			SessionToken string to override from Application . Default None
userAgent			Curl user-agent string.Default is {Mozilla/5.0 (Linux; x86_64 GNU/Linux) AppleWebKit/601.1 (KHTML, like Gecko) Version/8.0 Safari/601.1 WPE}
customHeader			custom header data to be appended to curl request. Default None
uriParameter			uri parameter data to be appended on download-url during curl request. Default None
preferredSubtitleLanguage	User preferred subtitle language.Default is None
ckLicenseServerUrl		ClearKey License server URL.Default is None
prLicenseServerUrl		PlayReady License server URL.Default is None
wvLicenseServerUrl		Widevine License server URL.Default is None
customHeaderLicense             custom header data to be appended to curl License request. Default None

// Long inputs
minBitrate			Set minimum bitrate filter for playback profiles, default is 0.
maxBitrate			Set maximum bitrate filter for playback profiles, default is LONG_MAX.
downloadStallTimeout		Timeout value for detection curl download stall in second,default is 0.
downloadStartTimeout		Timeout value for curl download to start after connect in seconds,default is 0.
discontinuityTimeout		Value in MS after which AAMP will try recovery for discontinuity stall, after detecting empty buffer, 0 will disable the feature, default 3000.
defaultBitrate			Default bitrate.Default is 2500000
defaultBitrate4K		Default 4K bitrateDefault is 13000000
iframeDefaultBitrate		Default bitrate for iframe track selection for non-4K assets.Default is 0
iframeDefaultBitrate4K		Default bitrate for iframe track selection for 4K assets.Default is 0


// Double inputs
networkTimeout			Specify download time out in seconds, default is 10 seconds.
manifestTimeout			Specify manifest download time out in seconds, default is 10 seconds.
playlistTimeout			Playlist download time out in sec.Default is 10 seconds.

*File Harvest Config :
    By default aamp will dump all the type of data, set 0 for desabling harvest
	0x00000001 (1)      - Enable Harvest Video fragments - set 1st bit 
	0x00000002 (2)      - Enable Harvest audio - set 2nd bit 
	0x00000004 (4)      - Enable Harvest subtitle - set 3rd bit 
	0x00000008 (8)      - Enable Harvest auxiliary audio - set 4th bit 
	0x00000010 (16)     - Enable Harvest manifest - set 5th bit 
	0x00000020 (32)     - Enable Harvest license - set 6th bit , TODO: not yet supported license dumping
	0x00000040 (64)     - Enable Harvest iframe - set 7th bit 
	0x00000080 (128)    - Enable Harvest video init fragment - set 8th bit 
	0x00000100 (256)    - Enable Harvest audio init fragment - set 9th bit 
	0x00000200 (512)    - Enable Harvest subtitle init fragment - set 10th bit 
	0x00000400 (1024)   - Enable Harvest auxiliary audio init fragment - set 11th bit 
	0x00000800 (2048)   - Enable Harvest video playlist - set 12th bit 
	0x00001000 (4096)   - Enable Harvest audio playlist - set 13th bit 
	0x00002000 (8192)   - Enable Harvest subtitle playlist - set 14th bit 
	0x00004000 (16384)  - Enable Harvest auxiliary audio playlist - set 15th bit 
	0x00008000 (32768)  - Enable Harvest Iframe playlist - set 16th bit 
	0x00010000 (65536)  - Enable Harvest IFRAME init fragment - set 17th bit  
	example :- if you want harvest only manifest and vide0 fragments , set value like 0x00000001 + 0x00000010 = 0x00000011 = 17
	harvest-config=17
=================================================================================================================
3. Channel Override Settings

Overriding channels in aamp.cfg
aamp.cfg allows to map channnels to custom urls as follows

*<Token> <Custom url>
This will make aamp tune to the <Custom url> when ever aamp gets tune request to any url with <Token> in it.

Example adding the following in aamp.cfg will make tune to the given url (Spring_4Ktest) on tuning to url with USAHD in it
This can be done for n number of channels.

*USAHD https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd
*FXHD http://demo.unified-streaming.com/video/tears-of-steel/tears-of-steel-dash-playready.ism/.mpd

=================================================================================================================
4. Westeros Settings

To enable Westeros
-------------------

Currently, use of Westeros is default-disabled, and can be enabled via RFC.  To apply, Developers can add below
flag in SetEnv.sh under /opt, then restart the receiver process:

	export AAMP_ENABLE_WESTEROS_SINK=true

Note: Above is now used as a common FLAG by AAMP and Receiver module to configure Westeros direct rendering
instead of going through browser rendering.  This allows for smoother video zoom animations
(Refer DELIA-38429/RDK-26261)

However, note that with this optimization applied, the AAMP Diagnostics overlays cannot be made visible.
As a temporary workaround, the following flag can be used  by developers which will make diagnostic overlay
again visible at expense of zoom smoothness:

	export DISABLE_NONCOMPOSITED_WEBGL_FOR_IPVIDEO=1

=================================================================================================================
4. AAMP-CLI Commands

CLI-specific commands:
<enter>		dump currently available profiles
help		show usage notes
http://...	tune to specified URL
<number>	tune to specified channel (based on canned aamp channel map)
next        tune to next virtual channel
prev        tune to previous virtual channel
seek <sec>	time-based seek within current content (stub)
ff32		set desired trick speed to 32x
ff16		set desired trick speed to 16x
ff		set desired trick speed to 4x
flush		flush player buffers
stop		stop streaming
status		dump gstreamer state
rect		Set video rectangle. eg. rect 0 0 640 360
zoom <val>	Set video zoom mode. mode "none" if val is zero, else mode "full"
pause       Pause playback
play        Resume playback
rw<val>     Rewind with speed <val>
live        Seek to live point
exit        Gracefully exit application
sap <lang>  Select alternate audio language track.
bps <val>   Set video bitrate in bps

To add channelmap for CLI, enter channel entries in below format in /opt/aampcli.cfg
*<Channel Number> <Channel Name> <Channel URL>

or

To add channelmap for CLI, enter channel entries in below format in /opt/aampcli.csv
<Channel Number>,<Channel Name>,<Channel URL>
=================================================================================================================
5. AAMP Tunetime 

Following line can be added as a header while making CSV with profiler data.

version#4
version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime,contentType,streamType,firstTune,Prebuffered,PreBufferedTime

=================================================================================================================

6. AAMP MicroEvents 
=====================
Common:
ct = Content Type
it = Initiation Time of Playback in epoch format (on Receiver side)
tt = Total Tune Time/latency
pi = Playback Index
ts = Tune Status
va = Vector of tune Attempts

Individual Tune Attempts:
s = Start Time in epoch format
td = Tune Duration
st = Stream Type
u = URL
r = Result (1:Success, 0:Failure)
v = Vector of Events happened

Events:
i = Id
	0: main manifest download
	1: video playlist download
	2: audio playlist download
	3: subtitle playlist download
	4: video initialization fragment download
	5: audio initialization fragment download
	6: subtitle initialization fragment download
	7: video fragment download
	8: audio fragment download
	9: subtitle fragment download
	10: video decryption
	11: audio decryption
	12: subtitle decryption
	13: license acquisition total
	14: license acquisition pre-processing
	15: license acquisition network
	16: license acquisition post-processing
b = Beginning time of the event, relative to 's'
d = Duration till the completion of event
o = Output of Event (200:Success, Non 200:Error Code)

=================================================================================================================
7. VideoEnd (Session Statistics) Event 
==========================
vr = version of video end event (currently "2.0")
tt = time to reach top profile first time after tune. Provided initial tune bandwidth is not a top bandwidth
ta = time at top profile. This includes all the fragments which are downloaded/injected at top profile for total duration of playback. 
d = duration - estimate of total playback duration.  Note that this is based on fragments downloaded/injected - user may interrupt buffered playback with seek/stop, causing estimates to skew higher in edge cases.
dn = Download step-downs due to bad Network bandwidth
de = Download step-downs due to Error handling ramp-down/retry logic
w = Display Width :  value > 0 = Valid Width.. value -1 means HDMI display resolution could NOT be read successfully. Only for HDMI Display else wont be available.
h = Display Height : value > 0 = Valid Height,  value -1 means HDMI display resolution could NOT be read successfully. Only for HDMI Display else wont be available.
t = indicates that FOG time shift buffer (TSB) was used for playback
m =  main manifest
v = video Profile
i = Iframe Profile
a1 = audio track 1
a2 = audio track 2
a3 = audio track 3
...
u = Unknown Profile or track type

l = supported languages,
	In version 2.0, same tag is reused to represent download latency report if it appears under 'n' or 'i' or 'ms'(See their representations below).
p = profile-specific metrics encapsulation
w = profile frame width
h = profile frame height
ls = license statistics

ms = manifest statistics
fs = fragment statistics

r = total license rotations / stream switches
e = encrypted to clear switches
c = clear to encrypted switches

in version 1.0,
	4 = HTTP-4XX error count
	5 = HTTP-5XX error count
	t = CURL timeout error count
	c = CURL error count (other)
	s = successful download count
in version 2.0
	S = Session summary
	200 = http success
	18(0) - Curl 18 occured, network connectivity is down
	18(1) = Curl 18 occured, network connectivity is up
	28(0) - Curl 28 occured, network connectivity is down
	28(1) = Curl 28 occured, network connectivity is up
	404, 42, 7, etc.. = http/curl error code occured during download.
		Example : "S":{"200":341,"404":6} - 341 success attemps and 4 attempts with 404
			  "S":{"200":116,"28(1)":1,"404":114} - 115 success attempts, 114 attempts with 404 and 1 attempt with curl-28
								where network connection is up.
	T0
	T1
	T2
	...
	Ty = Latency report in a specific time window, where y represents window number (comes under 'l').
		T0 - is 0ms - 250ms window (Window calculations: start = (250ms x y), end = (start + 250ms))
		For example : T13 represents window 3250ms - 3500ms (13x250ms = 3250ms).

u = URL of most recent (last) failed download
n = normal fragment statistics
i = "init" fragment statistics (used in case of DASH and fragmented mp4)

=================================================================================================================

