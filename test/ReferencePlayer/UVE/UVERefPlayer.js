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

var playbackSpeeds = [-64, -32, -16, -4, 1, 4, 16, 32, 64];

//Comcast DRM config for AAMP
var DrmConfig = {'com.microsoft.playready':'mds.ccp.xcal.tv', 'com.widevine.alpha':'mds.ccp.xcal.tv', 'preferredKeysystem':'com.widevine.alpha'};

//DRM config for Sintel asset
var SintelDrmConfig = {'com.microsoft.playready':'https://amssamples.keydelivery.mediaservices.windows.net/PlayReady/', 'com.widevine.alpha':'https://amssamples.keydelivery.mediaservices.windows.net/Widevine/?KID=f9dbca11-a1e2-45c8-891f-fb71063cbfdb', 'preferredKeysystem':'com.microsoft.playready'};
//AAMP initConfig is used to pass certain predefined config params to AAMP
//Commented out values are not implemented for now
//Values assigned are default values of each config param
//All properties are optional.
var defaultInitConfig = {
    /**
     * max initial bitrate (bps)
     */
    initialBitrate: 2500000,

    /**
     * max initial bitrate for 4K assets (bps)
     */
    //initialBitrate4K: number,

    /**
     * min amount of buffer needed before playback (seconds)
     */
    //initialBuffer: number;

    /**
     * max amount of buffer during playback (seconds)
     */
    //playbackBuffer: number;

    /**
     * start position for playback (seconds)
     */
    offset: 15,

    /**
     * network request timeout (seconds)
     */
    networkTimeout: 10,

    /**
     * manifest request timeout (seconds)
     */
    manifestTimeout: 0.5,
    /**
     * manifest request timeout (seconds)
     */
    playlistTimeout: 10,

    /**
     * max number of fragments to keep as playback buffer (number)
     * e.x:
     *   with a downloadBuffer of 3, there will be 3 fragments of
     *   video or audio cached as buffers during playback.
     *   If each fragment is 2 second long, total download buffer
     *   size of this playback is 3 * 2 = 6 secs
     */
    //downloadBuffer: number;

    /**
     * min amount of bitrate (bps)
     */
    //minBitrate: number;

    /**
     * max amount of bitrate (bps)
     */
    //maxBitrate: number;

    /**
     * preferred audio language
     */
    preferredAudioLanguage: "en",

    /**
     * TSB length in seconds, value of 0 means it is disabled
     */
    //timeShiftBufferLength: number;

    /**
     * offset from live point for live assets (seconds)
     */
    liveOffset: 15,

    /**
     * preferred subtitle language
     */
    preferredSubtitleLanguage: "en",

    /**
     *  network proxy to use (Format <SCHEME>://<PROXY IP:PROXY PORT>)
     */
    //networkProxy: string;

    /**
     *  network proxy to use for license requests (Format same as network proxy)
     */
    //licenseProxy: string;

    /**
     *  time to deem a download with partial bytes as stalled prematurely. (seconds)
     *  optimization to avoid long waits when networkTimeout configured is a high value
     */
    //downloadStallTimeout: number;

    /**
     *  time to deem a download with no bytes received as stalled. (seconds)
     *  optimization to avoid long waits when networkTimeout configured is a high value
     */
    //downloadStartTimeout: number;

    /**
     * drmConfig for the playback
     */
    drmConfig: DrmConfig, //For sample structure DrmConfig

    /**
     * stereo-only for the playback
     */
    //stereoOnly: true,
    /**
     * bulk TimedMetadata reporting
     */
    //bulkTimedMetadata: true,

    /**
     * enable audio video playlist parallel download optimization (only for HLS)
     */
    //parallelPlaylistDownload: true,
	/**
	 * disable audio video playlist parallel download for linear (only for HLS)
	 */
	//parallelPlaylistRefresh: false,

    /**
     * enable async tune method for player
     */
    //asyncTune: true,

    /**
     * use westeros sink based video decoding
     */
    //useWesterosSink: true,

    /**
     * use Average bandwidth for ABR switching
     */
    //useAverageBandwidth: true,
    /**
     * Enable PreCaching of Playlist and TimeWindow for Cache(minutes)
     */
    //preCachePlaylistTime: 5
    /**
     * Progress Report Interval (in seconds)
     */
    //progressReportingInterval: 2,
    /**
     * enable unpaired discontinuity retune config
     */
    //useRetuneForUnpairedDiscontinuity: true
    
};

var playerState = playerStatesEnum.idle;
var playbackRateIndex = playbackSpeeds.indexOf(1);
var urlIndex = 0;
var mutedStatus = false;
var playerObj = null;

window.onload = function() {
    initPlayerControls();
    resetPlayer();
    resetUIOnNewAsset();

    //loadUrl(urls[urlIndex]);
}

function playbackStateChanged(event) {
    console.log("Playback state changed event: " + JSON.stringify(event));
    switch (event.state) {
        case playerStatesEnum.idle:
            playerState = playerStatesEnum.idle;
            break;
        case playerStatesEnum.initializing:
            playerState = playerStatesEnum.initializing;
            break;
        case playerStatesEnum.initialized:
            playerState = playerStatesEnum.initialized;
            console.log("Available audio tracks: " + playerObj.getAvailableAudioTracks());
            console.log("Available text tracks: " + playerObj.getAvailableTextTracks());

            // Remove exsisting options in list
            if(ccTracks.options.length) {
                for(itemIndex = ccTracks.options.length; itemIndex >= 0; itemIndex--) {
                    ccTracks.remove(itemIndex);
                }
            }

            var textTrackList = JSON.parse((playerObj.getAvailableTextTracks()));
            // Parse only the closed captioning tracks
            var closedCaptioningList = [];
            for(track=0; track<textTrackList.length;track++) {
                if(textTrackList[track].type === "CLOSED-CAPTIONS") {
                    closedCaptioningList.push(textTrackList[track].name);
                }
            }

            // Iteratively adding all the options to ccTracks
            for (var trackNo = 1; trackNo <= closedCaptioningList.length; trackNo++) {
                var option = document.createElement("option");
                option.value = trackNo;
                option.text = closedCaptioningList[trackNo-1];
                ccTracks.add(option);
            }

            break;
        case playerStatesEnum.playing:
            playerState = playerStatesEnum.playing;
            break;
        case playerStatesEnum.paused:
            playerState = playerStatesEnum.paused;
            break;
        case playerStatesEnum.seeking:
            playerState = playerStatesEnum.seeking;
            break;
        default:
            console.log("State not expected");
            break;
    }
    console.log("Player state is: " + playerState);
}

function mediaEndReached() {
    console.log("Media end reached event!");
    loadNextAsset();
}

function mediaSpeedChanged(event) {
    console.log("Media speed changed event: " + JSON.stringify(event));
    var currentRate = event.speed;
    console.log("Speed Changed [" + playbackSpeeds[playbackRateIndex] + "] -> [" + currentRate + "]");
    if (currentRate != undefined) {
        if (currentRate != 0) {
            playbackRateIndex = playbackSpeeds.indexOf(currentRate);
        } else {
                playbackRateIndex = playbackSpeeds.indexOf(1);
        }
        if (currentRate != 0 && currentRate != 1){
                showTrickmodeOverlay(currentRate);
        }

        if (currentRate === 1) {
            document.getElementById("playOrPauseIcon").src = "../icons/pause.png";
        } else {
            document.getElementById("playOrPauseIcon").src = "../icons/play.png";
        }
    }
}

function bitrateChanged(event) {
    console.log("bitrate changed event: " + JSON.stringify(event));
}

function mediaPlaybackFailed(event) {
    console.log("Media failed event: " + JSON.stringify(event));
    //Uncomment below line to auto play next asset on playback failure
    //loadNextAsset();
}

function mediaMetadataParsed(event) {
    console.log("Media metadata event: " + JSON.stringify(event));
}

function subscribedTagNotifier(event) {
    console.log("Subscribed tag notifier event: " + JSON.stringify(event));
}

function mediaProgressUpdate(event) {
    //console.log("Media progress update event: " + JSON.stringify(event));
    var duration = event.durationMiliseconds;
    var position = event.positionMiliseconds;
    var value = ( position / duration ) * 100;
	var seekBar = document.getElementById("seekBar");
    document.getElementById("totalDuration").innerHTML = convertSStoHr(duration / 1000.0);
    document.getElementById("currentDuration").innerHTML = convertSStoHr(position / 1000.0);
    console.log("Media progress update event: value=" + value);
    // Update the slider value
    if(isFinite(value)) {
        seekBar.value = value;
        seekBar.style.width = value+"%";
        seekBar.style.backgroundColor = "red";
    }
}

function mediaPlaybackStarted() {
    document.getElementById("playOrPauseIcon").src = "../icons/pause.png";

    var availableVBitrates = playerObj.getVideoBitrates();
    if (availableVBitrates !== undefined) {
        createBitrateList(availableVBitrates);
    }
}

function mediaPlaybackBuffering(event) {
    //Show buffering animation here, fired when buffers run dry mid-playback
    console.log("Buffers running empty - " + event.buffering);
    if (event.buffering === true) {
        //bufferingDisplay(true);
    } else {
        //bufferingDisplay(false);
    }
}

function mediaDurationChanged(event) {
    console.log("Duration changed!");
}

function decoderHandleAvailable(event) {
    console.log("decoderHandleAvailable " + event.decoderHandle);
    XREReceiver.onEvent("onDecoderAvailable", { decoderHandle: event.decoderHandle });
}

function anomalyEventHandler(event) {
    if (event.severity === anomalySeverityEnum.error || event.severity === anomalySeverityEnum.warning) {
        console.log("anomalyEventHandler got msg: " + event.description);
    }
}


function bufferingChangedHandler(event) {
        console.log("bufferingChangedHandler got msg: " + event.buffering);
	if(event.buffering === false){
		document.getElementById('buffModal').style.display = "block";
	}else{
		document.getElementById('buffModal').style.display = "none";
	}
}

function playbackSeeked(event) {
    console.log("Play Seeked " + JSON.stringify(event));
}

function bulkMetadataHandler(event) {
	console.log("Bulk TimedMetadata : " + JSON.stringify(event));
}

// helper functions
function resetPlayer() {
    if (playerState !== playerStatesEnum.idle) {
        playerObj.stop();
    }
    if (playerObj !== null) {
        playerObj.destroy();
        playerObj = null;
    }

    playerObj = new AAMPPlayer("UVE-Ref-Player");
    playerObj.addEventListener("playbackStateChanged", playbackStateChanged);
    playerObj.addEventListener("playbackCompleted", mediaEndReached);
    playerObj.addEventListener("playbackSpeedChanged", mediaSpeedChanged);
    playerObj.addEventListener("bitrateChanged", bitrateChanged);
    playerObj.addEventListener("playbackFailed", mediaPlaybackFailed);
    playerObj.addEventListener("mediaMetadata", mediaMetadataParsed);
    playerObj.addEventListener("timedMetadata", subscribedTagNotifier);
    playerObj.addEventListener("playbackProgressUpdate", mediaProgressUpdate);
    playerObj.addEventListener("playbackStarted", mediaPlaybackStarted);
    //playerObj.addEventListener("bufferingChanged", mediaPlaybackBuffering);
    playerObj.addEventListener("durationChanged", mediaDurationChanged);
    playerObj.addEventListener("decoderAvailable", decoderHandleAvailable);
    playerObj.addEventListener("anomalyReport", anomalyEventHandler);
    playerObj.addEventListener("bufferingChanged", bufferingChangedHandler);
    playerObj.addEventListener("seeked", playbackSeeked);
    //newPlayer.addEventListener("bulkTimedMetadata", bulkMetadataHandler);
    //Can add generic callback for ad resolved event or assign unique through setAlternateContent
    //playerObj.addEventListener("adResolved", adResolvedCallback);
    playerState = playerStatesEnum.idle;
    mutedStatus = false;
}

function loadUrl(urlObject) {
    console.log("UrlObject received: " + urlObject);
    //set custom HTTP headers for HTTP manifest/fragment/license requests. Example provided below
    //For manifest/fragment request - playerObj.addCustomHTTPHeader("Authentication-Token:", "12345");
    //For license request - playerObj.addCustomHTTPHeader("Content-Type:", "application/octet-stream", true);
    if (urlObject.name.includes("Sintel") == true) {
        var initConfiguration = defaultInitConfig;
        initConfiguration.drmConfig = SintelDrmConfig;
        playerObj.initConfig(initConfiguration);
        playerObj.load(urlObject.url);
    } else if (urlObject.useDefaultDrmConfig === true) {
        playerObj.initConfig(defaultInitConfig);
        playerObj.load(urlObject.url);
    } else {
        var initConfiguration = defaultInitConfig;
        initConfiguration.drmConfig = null;
        playerObj.load(urlObject.url);
    }
}
