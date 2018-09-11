Advanced Adaptive Micro Player (AAMP)

https://code.rdkcentral.com/r/rdk/components/generic/aamp

git clone https://code.rdkcentral.com/r/rdk/components/generic/aamp && (cd aamp && gitdir=$(git rev-parse --git-dir); curl -o ${gitdir}/hooks/commit-msg https://code.rdkcentral.com/r/tools/hooks/commit-msg ; chmod +x ${gitdir}/hooks/commit-msg)
git push origin HEAD:refs/for/master

========================================================================================

Win32 build:
prerequisite - install
docs.gstreamer.com (0.10); installs to C:\gstreamer-sdk\0.10\x86_64\bin
	GStreamer SDK 2013.6 (Congo) for Windows 64 bits (Runtime)
		gstreamer-sdk-x86_64-2013.6.msi

	GStreamer SDK 2013.6 (Congo) for Windows 64 bits (Development Files)
		gstreamer-sdk-devel-x86_64-2013.6.msi

	GStreamer SDK 2013.6 (Congo) for Windows 64 bits (Merge Modules) (not needed)
		gstreamer-sdk-x86_64-2013.6-merge-modules.zip (not needed)

	open aamp\vs2010\tutorials.sln in Visual Studio 2013 (or 2010?)


	if libcurl.dll error,
	copy libcurl.dll from AAMP\vs2010\curl-7.46.0-win64\dlls to C:\gstreamer-sdk\0.10\x86_64\bin

	if VCRUNTIME140.dll error, install runtime (required by gstreamer)
	https://www.microsoft.com/en-us/download/details.aspx?id=48145

========================================================================================

RDK Build notes

1. complete and install build
	bitbake comcast-mediaclient-vbn-image // builds everything
	bitbake -v -C compile -f  xre-receiver-default // rebuild just receiver
	bitbake -v -C compile -f  aamp // rebuilt just aamp lib

2. Enable receiver use of AAMP Video Engine.

2.a) Via RFC rule 'AAMP' in prod or DEV RFC xconf - add eSTB Mac
- After adding the box mac in the list, reboot the box twice, one for downloading RFC config and next for getting AAMP configuration

- or -

2.b) Locally configure device to use AAMP:
Create /opt/SetEnv.sh with following entries
# cat /opt/SetEnv.sh
export XRE_DEBUG_ENABLE_OVERRIDE=1
export ENABLE_AAMP=TRUE
Create /opt/xre_rt.conf with following entry and reboot the box
# cat /opt/xre_rt.conf
enableAAMP=true

3. AAMP will now be used by default instead of Adobe player; inspect logs to confirm:
tail /opt/logs/receiver.log | grep aamp

**********************************************************************************************

CLI AAMP is generated as part of build, but not automatically copied to settop image

**********************************************************************************************
Source Overview:

main.cpp
- entry point for command line test app and options

aampgstplayer.cpp
- gstreamer setup - allows playback of unencrypted video fragments

base16, base64
- fast utility functions

fragmentcollector_hls
- hls parsing and fragment collection

fragmentcollector_mpd
- dash fragment collection (using libdash)

drm.cpp
- abstraction for AVE DRM
- input: encrypted fragment & encryption (currently Adobe Access) metadata
- performs decryption in-place, yielding content that can be presented by gstreamer

================================================================================================

/opt/aamp.cfg
This optional file supports changes to default logging/behavior and channel remappings to alternate content.
sap		enable presentation of secondary audio (prelim)
info		enable logging of requested urls
gst		enable gstreamer logging including pipeline dump
progress	enable periodic logging of position
trace		enable dumps of manifests
curl		enable verbose curl logging
debug		enable debul level logs
abr		disable abr mode (defaults on)
default-bitrate	specify initial bitrate while tuning, or target bitrate while abr disabled (defaults to 2500000)
default-bitrate-4k	specify initial bitrate while tuning 4K contents, or target bitrate while abr disabled for 4K contents (defaults to 13000000)
display-offset	default is -1; if set, configures delay before display, gstreamer is-live, and display-offset parameters
throttle	used with restamping (default=1)
flush		if zero, preserve pipeline during channel changes (default=1)
<url1> <url2>	redirects requests to tune to url1 to url2
demux-hls-audio-track=1 // use software demux for audio
demux-hls-video-track=1 // use software demux for video
demux-hls-video-track-tm=1 // use software demux for trickmodes
live-tune-event-playlist-indexed=1 // to report tuning once playlist acquired for live
live-tune-event-first-fragment-decrypted=1 // to report tuning once first fragment decrypted for live
vod-tune-event-playlist-indexed=1 // // to report tuning once playlist acquired for vod
vod-tune-event-first-fragment-decrypted=1 // to report tuning once first fragment decrypted for vod
demuxed-audio-before-video=1 // send audio es before video in case of s/w demux
forceEC3=1 // inserts "-eac3" before .m3u8 in main manifest url. Useful in comcast live environment to test Dolby track.
disableEC3=1 // removes "-eac3" before .m3u8 in main manifest url. Useful in comcast live environment to disable Dolby track.
			 //In case of MPEG DASH playback this flag makes AAC preferred over ATMOS and DD+
			 //Default priority of audio selction in DASH is ATMOS, DD+ then AAC
disableATMOS=1 //For DASH playback makes DD+ or AAC preferred over ATMOS (EC+3)

live-offset    live offset time in seconds, aamp starts live playback this much time before the live point
disablePlaylistIndexEvent=1    disables generation of playlist indexed event by AAMP on tune/trickplay/seek
enableSubscribedTags=1    Specifies if subscribedTags[] and timeMetadata events are enabled during HLS parsing, default value: 1 (true)
map-mpd -	remap production linear/vod content to corresponding dash lanes
    map-mpd=1 //Just m3u8 to mpd substitution, base URL remains same
    map-mpd=2 //Old style COAM re-mapping
    map-mpd=3 //Replace all national channels' hostnames with `ctv-nat-slivel4lb-vip.cmc.co.ndcwest.comcast.net`
dash-ignore-base-url-if-slash If present, disables dash BaseUrl value if it is / . Sample - http://assets.player.xcal.tv/super8sapcc/index.mpd
ad-position	position at which ad to be inserted, use together with ad-url
ad-url		url of ad content to be inserted, use together with ad-position
fog-dash=1	Implies fog has support for dash, so no "defogging" when map-mpd is set.
min-vod-cache	Vod duration to be cached before playing in seconds.
fragmentDLTimeout=<download time out> Specify curl fragment download time out in seconds, default is 5 seconds
license-anonymous-request If set, makes PlayReady/WideVine license request without access token
abr-cache-life=<x in sec> lifetime value for abr cache  for network bandwidth calculation(default 5 sec)
abr-cache-length=<x>  length of abr cache for network bandwidth calculation (default 3)
abr-cache-outlier=<x in bytes> Outlier difference which will be ignored from network bandwidth calculation(default 5MB)
abr-nw-consistency=<x> Number of checks before profile incr/decr by 1.This is to avoid frequenct profile switching with network change(default 2)
abr-skip-duration=<x> minimum duration of fragment to be downloaded before triggering abr (default 6 sec).
hls-av-sync-use-start-time=1 Use EXT-X-PROGRAM-DATE to synchronize audio and video playlists. Disabled in default configuration.
playlists-parallel-fetch=1 Fetch audio and video playlists in parallel. Disabled in default configuration.
pre-fetch-iframe-playlist=1 Pre-fetch iframe playlist for VOD. Enabled by default.
license-server-url=<serverUrl> URL to be used for license requests for encrypted(PR/WV) assets.
vod-trickplay-fps=<x> Specify the framerate for VOD trickplay (defaults to 4)
linear-trickplay-fps=<x> Specify the framerate for Linear trickplay (defaults to 8)
http-proxy=<HTTP PROXY IP:HTTP PROXY PORT> Specify the HTTP Proxy
http-proxy=<USERNAME:PASSWORD>@<HTTP PROXY IP:HTTP PROXY PORT> Specify the HTTP Proxy with Proxy Authentication Credentials. Make sure to encode special characters if present in username or password (URL Encoding)
mpd-discontinuity-handling=0	Disable discontinuity handling during MPD period transition.
mpd-discontinuity-handling-cdvr=0	Disable discontinuity handling during MPD period transition for cDvr.
force-http Allow forcing of HTTP protocol for HTTPS URLs

CLI-specific commands:
<enter>		dump currently available profiles
help		show usage notes
http://...	tune to specified URL
<number>	tune to specified channel (based on canned aamp channel map)
seek <sec>	time-based seek within current content (stub)
ff32		set desired trick speed to 32x
ff16		set desired trick speed to 16x
ff		set desired trick speed to 4x
flush		flush player buffers
stop		stop streaming
status		dump gstreamer state
rect		Set video rectangle. eg. rect 0 0 640 360

To add channelmap for CLI, enter channel entries in below format
*<Channel Number> <Channel Name> <Channel URL>

================================================================================================
Following line can be added as a header while making CSV with profiler data.

version#2
version,build,tuneStartBaseUTCMS,ManifestDownloadStartTime,ManifestDownloadTotalTime,ManifestDownloadFailCount,PlaylistDownloadStartTime,PlaylistDownloadTotalTime,PlaylistDownloadFailCount,VideoInit1DownloadStartTime,VideoInit1DownloadTotalTime,VideoInit1FailCount,VideoFragment1DownloadStartTime,VideoFragment1DownloadTotalTime,VideoFragment1DownloadFailCount,VideoFragment1Bandwidth,VideoFragment1DecryptTime,AudioInit1DownloadStartTime,AudioInit1DownloadTotalTime,AudioInit1FailCount,AudioFragment1DownloadStartTime,AudioFragment1DownloadTotalTime,AudioFragment1DownloadFailCount,AudioFragment1DecryptTime,drmLicenseRequestStart,drmLicenseRequestTotalTime,drmFailErrorCode,gstStart,gstReady,gstPlaying,gstFirstFrame

version#3
version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime

version#4
version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime,contentType,streamType,firstTune

