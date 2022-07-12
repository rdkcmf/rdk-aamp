// configuration
var gUseAampIfAvailable = true; // change false to force use of a given MSE/EME player even on RDK
var gPreferredMseEmeEngine = "dashjs"; // MSE/EME video engine to use in other environments

const uveStatesEnum = { "idle":0, "initializing":1, "initialized":2, "playing":8, "paused":6, "seeking":7 };

function uve_prepare( engine, useAampIfAvailable )
{
	console.log( "uve_prepare " + engine );
	gUseAampIfAvailable = useAampIfAvailable;
	gPreferredMseEmeEngine = engine;
	if(engine == "dashjs")
	{
		var script = document.createElement('script');
		script.src = "http://cdn.dashjs.org/latest/dash.all.min.js";
		document.head.appendChild(script);
	}
	else if(engine == "hlsjs") {
		var script = document.createElement('script');
		script.src = "https://cdn.jsdelivr.net/npm/hls.js@latest";
		document.head.appendChild(script);
	}
	else if(engine == "shaka") {
		var script = document.createElement('script');
		script.src = "https://cdnjs.cloudflare.com/ajax/libs/shaka-player/3.2.0/shaka-player.compiled.js";
		document.head.appendChild(script);
		script.src = "https://google.github.io/eme-encryption-scheme-polyfill/dist/eme-encryption-scheme-polyfill.js";
		document.head.appendChild(script);
	}
	else if(engine == "videotag" || engine=="aamp" )
	{
	}
	else
	{
		console.log( "unsupported video engine" );
	}
}

class UVEPlayer {
	constructor(appName) {
		if( gUseAampIfAvailable )
		{
			try
			{
				var rc = new AAMPMediaPlayer(appName);
				console.log( "AAMP Video Engine is available" );
				return rc;
			}
			catch( e )
			{
				console.log( "AAMP Video Engine not supported by this browser" );
			}
		}
		this.playerState = uveStatesEnum.idle;
		if(gPreferredMseEmeEngine == "dashjs") return new AAMPDashJSPlayer(appName);
		if(gPreferredMseEmeEngine == "hlsjs") return new AAMPHlsJSPlayer(appName);
		if(gPreferredMseEmeEngine == "shaka") return new AAMPShakaPlayer(appName);
		if(gPreferredMseEmeEngine == "videotag") return new AAMPVideoPlayer(appName);
	}
}

// DASH JS PLAYER MAPPER
class AAMPDashJSPlayer {

    constructor(appName) { 
        console.log( appName + " using dashjs");
        // create dash player instance
        this.player = new dashjs.MediaPlayer().create();
        // drm server data
        this.drmData = {
            'com.widevine.alpha': {
                'serverURL': '',
                'priority': 3,
                'httpRequestHeaders': {}
            },
            'com.microsoft.playready': {
                'serverURL': '',
                'priority': 3,
                'httpRequestHeaders': {}
            }
        };
    }

    load(url, foreground) {
        console.log("Url received: " + url);
        // initialize player instance with videotag object
        this.player.initialize(document.querySelector("video"), url, true);
        console.log("Loading " + url + " using DASH.JS");
        this.player.play();
    }

    play() {
        console.log("Play started.");
        this.player.play();
    }

    pause() {
        console.log("Playback paused.");
        this.player.pause();
    }

    seek(timeSec) {
        console.log("Invoked seek with: " + timeSec);
        this.player.seek(timeSec);
    }

    getCurrentPosition() {
        return this.player.time();
    }

    stop() {
        console.log("Invoked stop");
        this.player.destroy();
    }

    release() {
        console.log("Invoked release");
    }

    setClosedCaptionStatus(flag) {
        console.log("Invoked setClosedCaptionStatus");
    }

    getPlaybackRate() {
        return this.player.getPlaybackRate();
    }

    setPlaybackRate(rate, overshoot) {
        this.player.setPlaybackRate(rate);
    }

    initConfig(config) {
        console.log("Invoked initConfig::  received object: " + JSON.stringify(config));
    }

    setDRMConfig(config) {
        if(config.hasOwnProperty('com.microsoft.playready')) {
            this.drmData['com.microsoft.playready']['serverURL'] = config['com.microsoft.playready'];
            this.drmData['com.microsoft.playready']['priority'] = 1;
        }
        
        if(config.hasOwnProperty('com.widevine.alpha')) {
            this.drmData['com.widevine.alpha']['serverURL'] = config['com.widevine.alpha'];
            this.drmData['com.widevine.alpha']['priority'] = 1;
        }

        // Set preferred Key system into priority
        if(config.hasOwnProperty('preferredKeysystem')) {
            if(config['preferredKeysystem'] == 'com.microsoft.playready') {
                this.drmData['com.microsoft.playready']['priority'] = 1;
                this.drmData['com.widevine.alpha']['priority'] = 2;
            } else if(config['preferredKeysystem'] == 'com.widevine.alpha') {
                this.drmData['com.widevine.alpha']['priority'] = 1;
                this.drmData['com.microsoft.playready']['priority'] = 2;
            }
        }
        this.player.setProtectionData(this.drmData);
    }

    addCustomHTTPHeader(headerName, headerValue, isLicense) {
        this.drmData['com.microsoft.playready']['httpRequestHeaders'][headerName] = headerValue;
        this.drmData['com.widevine.alpha']['httpRequestHeaders'][headerName] = headerValue;
        this.player.setProtectionData(this.drmData);
    }

    addEventListener(eventName, eventCallback) {
        if (eventName === "playbackStarted") {
            this.player.on(dashjs.MediaPlayer.events["PLAYBACK_STARTED"], function() {
                var eventObj = {};
                eventCallback(eventObj);
            }.bind(this));

        } else if (eventName === "playbackFailed") {
            this.player.on(dashjs.MediaPlayer.events["ERROR"], function(evt) {
                var eventObj = {};
                eventObj.shouldRetry = false;
                eventObj.code = evt.error.code;
                eventObj.description = evt.error.message;
                eventCallback(eventObj);
            }.bind(this));

        } else if (eventName === "playbackSpeedChanged") {
            this.player.on(dashjs.MediaPlayer.events["PLAYBACK_RATE_CHANGED"], function(evt) {
                var eventObj = {};
                eventObj.speed = evt.playbackRate;
                eventObj.reason = evt.type;                
                eventCallback(eventObj);
            }.bind(this));

        } else if (eventName === "playbackCompleted") {
            this.player.on(dashjs.MediaPlayer.events["PLAYBACK_ENDED"], function() {
                var eventObj = {};
                eventCallback(eventObj);
            }.bind(this));

        } else if(eventName === "playlistIndexed") {
            console.log("playbackStateChanged Event");

        } else if (eventName === "playbackProgressUpdate") {
            this.player.on(dashjs.MediaPlayer.events["PLAYBACK_TIME_UPDATED"], function(evt) {
                var eventObj = {};
                eventObj.durationMiliseconds = this.player.duration() * 1000;
                eventObj.positionMiliseconds = evt.time * 1000; 
                eventObj.playbackSpeed = this.player.getPlaybackRate();
                eventObj.startMiliseconds = 0; 
                eventObj.endMiliseconds = 0;
                eventObj.currentPTS = 0;
                eventObj.videoBufferedMiliseconds = 0; 
                eventCallback(eventObj);
            }.bind(this));

        } else if (eventName === "decoderAvailable") {
            console.log("decoderAvailable Event");

        } else if (eventName === "jsEvent") {
            console.log("jsEvent Event");  

        } else if (eventName === "mediaMetadata") {
            this.player.on(dashjs.MediaPlayer.events["PLAYBACK_METADATA_LOADED"], function() {
                var eventObj = {};
                eventCallback(eventObj);
            }.bind(this));

        } else if (eventName === "enteringLive") {
            console.log("enteringLive Event");

        } else if (eventName === "bitrateChanged") {
            console.log("bitrateChanged Event");

        } else if (eventName === "timedMetadata") {
            console.log("timedMetadata Event");  
          
        } else if (eventName === "bulkTimedMetadata") {
            console.log("bulkTimedMetadata Event");

        } else if (eventName === "playbackStateChanged") {
            console.log("playbackStateChanged Event");

        } else if (eventName === "speedsChanged") {
            console.log("speedsChanged Event");

        } else if (eventName === "seeked") {
            this.player.on(dashjs.MediaPlayer.events["PLAYBACK_SEEKED"], function() {
                var eventObj = {};
                eventObj.position = this.player.time();
                eventCallback(eventObj);
            }.bind(this));

        } else if (eventName === "tuneProfiling") {
            console.log("tuneProfiling Event");
        
        } else if (eventName === "bufferingChanged") {
            this.player.on(dashjs.MediaPlayer.events["BUFFER_LEVEL_STATE_CHANGED"], function(evt) {
                console.log(JSON.stringify(evt));
                var eventObj = {};
                eventObj.buffering = evt.state === "bufferLoaded"? true: false;
                eventCallback(eventObj);
            }.bind(this));            

        } else if (eventName === "durationChanged") {
            console.log("durationChanged Event");

        } else if (eventName === "audioTracksChanged") {
            console.log("audioTracksChanged Event");

        } else if (eventName === "textTracksChanged") {
            console.log("textTracksChanged Event");

        } else if (eventName === "contentBreaksChanged") {
            console.log("contentBreaksChanged Event");

        } else if (eventName === "contentStarted") {
            console.log("contentStarted Event");

        } else if (eventName === "contentCompleted") {
            console.log("contentCompleted Event");

        } else if (eventName === "drmMetadata") {
            console.log("drmMetadata Event");

        } else if (eventName === "anomalyReport") {
            console.log("anomalyReport Event");

        } else if (eventName === "vttCueDataListener") {
            console.log("vttCueDataListener Event");

        } else if (eventName === "adResolved") {
            console.log("adResolved Event");            

        } else if (eventName === "reservationStart") {
            console.log("reservationStart Event");

        } else if (eventName === "reservationEnd") {
            console.log("reservationEnd Event");

        } else if (eventName === "placementStart") {
            console.log("placementStart Event");

        } else if (eventName === "placementEnd") {
            console.log("placementEnd Event");

        } else if (eventName === "placementError") {
            console.log("placementError Event");

        } else if (eventName === "placementProgress") {
            console.log("placementProgress Event");

        } else if (eventName === "metricsData") {
            console.log("metricsData Event");

        } else if (eventName === "id3Metadata") {
            console.log("id3Metadata Event");

        } else if (eventName === "drmMessage") {
            console.log("drmMessage Event");

        } else if (eventName === "blocked") {
            console.log("blocked Event");

        } else if (eventName === "contentGap") {
            console.log("contentGap Event");

        } else if (eventName === "httpResponseHeader") {
            console.log("httpResponseHeader Event");

        } else if (eventName === "watermarkSessionUpdate") {
            console.log("watermarkSessionUpdate Event");
        }
    }
}

// HLS JS PLAYER MAPPER
class AAMPHlsJSPlayer {
    constructor(appName) { 
        console.log( appName + " using hlsjs");
        this.video = document.querySelector("video");
        this.hlsConfig = {
			"debug": true,
			emeEnabled: true,
			widevineLicenseUrl: "https://cwip-shaka-proxy.appspot.com/no_auth"
		}
        if (Hls.isSupported()) {
            // create hls player instance
            this.player = new Hls(this.hlsConfig);
        }
    }

    load(url, foreground) {
        console.log("Url received: " + url);
        if (Hls.isSupported()) {
            console.log("Loading " + url + " using HLS.JS");
            this.player.loadSource(url);
            this.player.attachMedia(this.video);
            this.player.on(Hls.Events.MEDIA_ATTACHED, function () {
            this.video.play();
            }.bind(this));
        }
    }

    play() {
        console.log("Play started.");
        this.video.play();
    }

    pause() {
        console.log("Playback paused.");
        this.video.pause();
    }

    seek(timeSec) {
        console.log("Invoked seek with: " + timeSec);
        this.video.currentTime = timeSec;
    }

    getCurrentPosition() {
        this.video.currentTime;
    }

    stop() {
        console.log("Invoked stop");
        this.video.pause();
        this.video.currentTime = 0;
    }

    getPlaybackRate() {
        return  this.video.playbackRate;
    }

    setPlaybackRate(rate, overshoot) {
        this.video.playbackRate = rate;
    }

    initConfig(config) {
        console.log("Invoked initConfig::  received object: " + JSON.stringify(config));
    }

    setDRMConfig(config) {
        // Hlsjs only support widevine
        if(config.hasOwnProperty('com.widevine.alpha')) {
            this.hlsConfig['widevineLicenseUrl'] = config['com.widevine.alpha'];
            // recreate hls config with drm details
            this.player = new Hls(this.hlsConfig);  
        }   
    }

    addCustomHTTPHeader(headerName, headerValue, isLicense) {
        this.hlsConfig['licenseXhrSetup'] = function(xhr, url) {
            xhr.setRequestHeader(headerName , headerValue);
        }
       // this.player = new Hls(this.hlsConfig); 
    }    

    addEventListener(eventName, eventCallback) {
        mapVideotagEvents(this.player, "hlsjs", eventName, eventCallback);
    }
}

// SHAKA PLAYER MAPPER
class AAMPShakaPlayer {
    constructor(appName) { 
        console.log( appName + " using shaka");
        shaka.polyfill.installAll();
        this.video = document.querySelector("video");
        // create shaka player instance
        this.player = new shaka.Player(this.video);
        // drm server data
        this.drmData = { 
            drm: {
                servers: {
                    'com.widevine.alpha': '',
                    'com.microsoft.playready': ''
                }
            }
        }
    }

    load(url, foreground) {
        console.log("Url received: " + url);
        console.log("Loading " + url + " using SHAKA");
        this.player.load(url);
    }

    play() {
        console.log("Play started.");
        this.video.play();
    }

    pause() {
        console.log("Playback paused.");
        this.video.pause();
    }

    seek(timeSec) {
        console.log("Invoked seek with: " + timeSec);
        this.video.currentTime = timeSec;
    }

    getCurrentPosition() {
        return this.video.currentTime;
    }

    stop() {
        console.log("Invoked stop");
        this.video.pause();
        this.video.currentTime = 0;
    }

    getPlaybackRate() {
        return this.video.playbackRate;
    }

    setPlaybackRate(rate, overshoot) {
        this.video.playbackRate = rate;
    }

    initConfig(config) {
        console.log("Invoked initConfig::  received object: " + JSON.stringify(config));
    }

    setDRMConfig(config) {
        if(config.hasOwnProperty('com.microsoft.playready')) {
            this.drmData['drm']['servers']['com.microsoft.playready'] = config['com.microsoft.playready'];
        }
        
        if(config.hasOwnProperty('com.widevine.alpha')) {
            this.drmData['drm']['servers']['com.widevine.alpha'] = config['com.widevine.alpha'];
        }
        this.player.configure(this.drmData);      
    }

    addCustomHTTPHeader(headerName, headerValue, isLicense) {
        this.player.getNetworkingEngine().registerRequestFilter(function(type, request) {
            if (type == shaka.net.NetworkingEngine.RequestType.LICENSE) {
              request.headers[headerName] = headerValue;
            }
        });
    }

    addEventListener(eventName, eventCallback) {
        mapVideotagEvents(null, "shaka", eventName, eventCallback);
    }
}

// VIDEO TAG PLAYER MAPPER
class AAMPVideoPlayer {
    constructor(appName) { 
        console.log( appName + " using videotag");
        this.video = document.querySelector("video");
    }

    load(url, foreground) {
		if( foreground==undefined ) foreground = true;
        console.log("Url received: " + url);
        this.video.src = url;
        this.video.load();
		if(foreground) { // autoplay
            this.video.play();
            this.playerState = uveStatesEnum.playing;
        }
    }

    initConfig(IConfig) {
        console.log("Invoked initConfig with :" + JSON.stringify(IConfig));
    }

    play() {
        console.log("Play started.");
        this.video.play();
		this.playerState = uveStatesEnum.playing;
    }

    pause() {
        console.log("Playback paused.");
        this.video.pause();
		this.playerState = uveStatesEnum.paused;
    }

    seek(timeSec) {
        console.log("Invoked seek with: " + timeSec);
        this.video.currentTime = timeSec;
    }

    getCurrentPosition() {
        return this.video.currentTime;
    }

    stop() {
        console.log("Invoked stop");
        this.video.pause();
        this.video.currentTime = 0;
    }

    release() {
        console.log("Invoked release");
    }

    getCurrentState() {
		return playerSate;
        //console.log("Invoked getCurrentState");
    }

    getPlaybackRate() {
        return this.video.playbackRate;
    }

    setPlaybackRate(rate, overshoot) {
        this.video.playbackRate = rate;
    }

    addEventListener(eventName, eventCallback) {
        mapVideotagEvents(null, "videotag", eventName, eventCallback);
    }
}

// this function will map uve event listeners with corresponding videotag events
function mapVideotagEvents(player, playerName, eventName, eventCallback) {
    this.video = document.querySelector("video");
    if (eventName === "playbackStarted") {
        this.video.addEventListener("playing", function() {
            var eventObj = {};
            eventCallback(eventObj);
        }.bind(this));

    } else if (eventName === "playbackFailed") {
        if(playerName === "hlsjs") {
            player.on(Hls.Events.ERROR, function () {
                var eventObj = {};
                eventObj.code = "404";
                eventObj.description = "Playback Failed";
                eventCallback(eventObj);
            }.bind(this));
        } else {
            this.video.addEventListener("error", function(evt) {
                var eventObj = {};
                alert(JSON.stringify(evt));
                eventObj.shouldRetry = false;
                eventObj.code = "null";
                eventObj.description = "Playback Failed";
                eventCallback(eventObj);
            }.bind(this));
        }

    } else if (eventName === "playbackSpeedChanged") {
        this.video.addEventListener("ratechange", function() {
            var eventObj = {};
            eventObj.speed = this.video.playbackRate;
            eventObj.reason = "speedChange";
            eventCallback(eventObj);
        }.bind(this));

    } else if (eventName === "playbackCompleted") {
        this.video.addEventListener("ended", function() {
            var eventObj = {};
            eventCallback(eventObj);
        }.bind(this));
        
    } else if(eventName === "playlistIndexed") {
        console.log("playbackStateChanged Event");

    } else if (eventName === "playbackProgressUpdate") {
        this.video.addEventListener("timeupdate", function() {
            var eventObj = {};
            eventObj.durationMiliseconds = this.video.duration * 1000;
            eventObj.positionMiliseconds = this.video.currentTime * 1000; 
            eventObj.playbackSpeed = this.video.playbackRate;
            eventObj.startMiliseconds = 0; 
            eventObj.endMiliseconds = 0;
            eventObj.currentPTS = 0;
            eventObj.videoBufferedMiliseconds = 0; 
            eventCallback(eventObj);
        }.bind(this));

    } else if (eventName === "decoderAvailable") {
        console.log("decoderAvailable Event");

    } else if (eventName === "jsEvent") {
        console.log("jsEvent Event");  

    } else if (eventName === "mediaMetadata") {
        console.log("mediaMetadata Event");

    } else if (eventName === "enteringLive") {
        console.log("enteringLive Event");

    } else if (eventName === "bitrateChanged") {
        console.log("bitrateChanged Event");

    } else if (eventName === "timedMetadata") {
        console.log("timedMetadata Event");  
      
    } else if (eventName === "bulkTimedMetadata") {
        console.log("bulkTimedMetadata Event");

    } else if (eventName === "playbackStateChanged") {
        console.log("playbackStateChanged Event");

    } else if (eventName === "speedsChanged") {
        console.log("speedsChanged Event");

    } else if (eventName === "seeked") {
        this.video.addEventListener("seeked", function() {
            var eventObj = {};
            eventObj.position = this.video.currentTime ;
            eventCallback(eventObj);
        }.bind(this));

    } else if (eventName === "tuneProfiling") {
        console.log("tuneProfiling Event");
    
    } else if (eventName === "bufferingChanged") {        
        console.log("bufferingChanged Event");

    } else if (eventName === "durationChanged") {
        console.log("durationChanged Event");

    } else if (eventName === "audioTracksChanged") {
        console.log("audioTracksChanged Event");

    } else if (eventName === "textTracksChanged") {
        console.log("textTracksChanged Event");

    } else if (eventName === "contentBreaksChanged") {
        console.log("contentBreaksChanged Event");

    } else if (eventName === "contentStarted") {
        console.log("contentStarted Event");

    } else if (eventName === "contentCompleted") {
        console.log("contentCompleted Event");

    } else if (eventName === "drmMetadata") {
        console.log("drmMetadata Event");

    } else if (eventName === "anomalyReport") {
        console.log("anomalyReport Event");

    } else if (eventName === "vttCueDataListener") {
        console.log("vttCueDataListener Event");

    } else if (eventName === "adResolved") {
        console.log("adResolved Event");            

    } else if (eventName === "reservationStart") {
        console.log("reservationStart Event");

    } else if (eventName === "reservationEnd") {
        console.log("reservationEnd Event");

    } else if (eventName === "placementStart") {
        console.log("placementStart Event");

    } else if (eventName === "placementEnd") {
        console.log("placementEnd Event");

    } else if (eventName === "placementError") {
        console.log("placementError Event");

    } else if (eventName === "placementProgress") {
        console.log("placementProgress Event");

    } else if (eventName === "metricsData") {
        console.log("metricsData Event");

    } else if (eventName === "id3Metadata") {
        console.log("id3Metadata Event");

    } else if (eventName === "drmMessage") {
        console.log("drmMessage Event");

    } else if (eventName === "blocked") {
        console.log("blocked Event");

    } else if (eventName === "contentGap") {
        console.log("contentGap Event");

    } else if (eventName === "httpResponseHeader") {
        console.log("httpResponseHeader Event");

    } else if (eventName === "watermarkSessionUpdate") {
        console.log("watermarkSessionUpdate Event");
    }
}
