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
//AAMP initConfig is used to pass certain predefined config params to AAMP
//Commented out values are not implemented for now
//Values assigned are default values of each config param
//All properties are optional.
var defaultInitConfig = {
    /**
     * start position for playback (seconds)
     */
    offset: 0,

    /**
     * drmConfig for the playback
     */
    drmConfig: DrmConfig //For sample structure DrmConfig
};

var playerState = playerStatesEnum.idle;
var playbackRateIndex = playbackSpeeds.indexOf(1);
var urlIndex = 0;
var playerObj = null;
var bgPlayerObj = null;
var alreadyLoaded = false;
var currentPlaybackRate = 1;
var totalPlaylistDuration = 0;
var currentPlaylistDuration = 0;
var prevPositionInPlaylist = 0;
var prevValue = 0;
var transitionPoints = [];

window.onload = function() {
    initPlayerControls();
    resetUIOnNewAsset();

    // calculate playlist duration
    for(var urlNo = 0; urlNo  < urls.length; urlNo++) {
        totalPlaylistDuration += (urls[urlNo].end - urls[urlNo].start);
        if(urlNo != urls.length-1) {
            // if not the last element push the content transition point to the transitionPoints list
            transitionPoints.push(totalPlaylistDuration);  
        }
    }

    // create content boundaries
    for(var transitionBoundary = 0; transitionBoundary  < transitionPoints.length; transitionBoundary++) {
        var contentPadding = (urls[transitionBoundary].end - urls[transitionBoundary].start);
        var value = ( contentPadding / totalPlaylistDuration ) * 100;
        //create a transition line
        let div = document.createElement('div');
        div.style.marginLeft = value-1+"%"; // value -1 to adjust the padding due to width of line
        div.style.backgroundColor = "yellow";
        div.style.width = "4px";
        div.style.height = "25px";
        document.getElementById("contentBoundaries").appendChild(div);
    }  
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
//  loadNextAsset();
        playerObj.stop();
        resetUIOnNewAsset();
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
            document.getElementById("playOrPauseIcon").src = "icons/pause.png";
        } else {
            document.getElementById("playOrPauseIcon").src = "icons/play.png";
        }
    }
}

function mediaProgressUpdate(event) {
    console.log("MMM Media progress update event: " + JSON.stringify(event));
    var position = event.positionMiliseconds;
    var positionInPlaylist = currentPlaylistDuration + position - Number(urls[urlIndex].start);
    if(positionInPlaylist < 0) {
        positionInPlaylist = 0;
    }

    // if trickplay exceeds total duration
    if(positionInPlaylist > totalPlaylistDuration) {
        positionInPlaylist = totalPlaylistDuration;
    }

    var value = ( positionInPlaylist / totalPlaylistDuration ) * 100;
    var seekBar = document.getElementById("seekBar");
    document.getElementById("totalDuration").innerHTML = convertSStoHr(totalPlaylistDuration / 1000.0);
    if(positionInPlaylist > prevPositionInPlaylist) {
        document.getElementById("currentDuration").innerHTML = convertSStoHr(positionInPlaylist / 1000.0);
        prevPositionInPlaylist = positionInPlaylist;
    }

    if(value > prevValue) {
        // Update the slider value
        seekBar.value = value;
        seekBar.style.width = value+"%";
        seekBar.style.backgroundColor = "red";
        prevValue = value;
    }

    if(!alreadyLoaded) {
        if(position+10000 >= urls[urlIndex].end) {
            bgPlayerObj = createAAMPPlayer();
            let initConfigObject = Object.assign({}, defaultInitConfig);
            if (urls[urlIndex].useDefaultDrmConfig === false) {
                initConfigObject.drmConfig = null;  
            }
            if(urlIndex != urls.length-1)  {
            // start caching from the specified start position
            initConfigObject.offset = Number(urls[urlIndex+1].start)/1000;
            bgPlayerObj.initConfig(initConfigObject);
            bgPlayerObj.load(urls[urlIndex+1].url, false);
            }
            alreadyLoaded = true;
        }
    }

    if(position >= urls[urlIndex].end) {
        if(urlIndex == urls.length-1)  {
            playerObj.stop();
        }
        currentPlaylistDuration += Number(urls[urlIndex].end - urls[urlIndex].start);
        urlIndex++;
        playerObj.detach();
        playerObj = bgPlayerObj;
        playerObj.play();
        alreadyLoaded = false;
    }

}

function mediaPlaybackStarted() {
    console.log("Media Playback Started");
    if(currentPlaybackRate == 1) {
        document.getElementById("playOrPauseIcon").src = "icons/pause.png";
    } else {
        // if currently doing trickplay operation,
        // continue trickplay in new content also
        playerObj.setPlaybackRate(currentPlaybackRate);
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

function playbackSeeked(event) {
    console.log("Play Seeked " + JSON.stringify(event));
}

function tuneProfiling(event) {
    console.log("Tune Profiling Data: " + event.microData);
}

function bulkMetadataHandler(event) {
    console.log("Bulk TimedMetadata : " + JSON.stringify(event));
}

function createAAMPPlayer(){
    var newPlayer = new AAMPPlayer("UVE-Ref-Player");
    newPlayer.addEventListener("playbackStateChanged", playbackStateChanged);
    newPlayer.addEventListener("playbackCompleted", mediaEndReached);
    newPlayer.addEventListener("playbackSpeedChanged", mediaSpeedChanged);
    newPlayer.addEventListener("playbackProgressUpdate", mediaProgressUpdate);
    newPlayer.addEventListener("playbackStarted", mediaPlaybackStarted);
    newPlayer.addEventListener("durationChanged", mediaDurationChanged);
    newPlayer.addEventListener("decoderAvailable", decoderHandleAvailable);
    newPlayer.addEventListener("seeked", playbackSeeked);
    newPlayer.addEventListener("tuneProfiling", tuneProfiling);
    return newPlayer;
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
    playerObj = createAAMPPlayer();
    playerState = playerStatesEnum.idle;
}

function loadUrl(urlObject) {
    console.log("loadUrl: UrlObject received: " + urlObject);
    
    let initConfigObject = Object.assign({}, defaultInitConfig);
    if (urlObject.useDefaultDrmConfig === false) {
        initConfigObject.drmConfig = null;  
    }
    // start caching from the specified start position
    initConfigObject.offset = Number(urls[urlIndex].start)/1000;
    playerObj.initConfig(initConfigObject);
    playerObj.load(urlObject.url, true);
}
