// load and initialize needed video engines

 var locator = "http://amssamples.streaming.mediaservices.windows.net/683f7e47-bd83-4427-b0a3-26a6c4547782/BigBuckBunny.ism/manifest(format=mpd-time-csf)"; // DASH
// var locator = "https://test-streams.mux.dev/x36xhzz/x36xhzz.m3u8"; // HLS
// var locator = "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/Sintel.mp4"; // mp4

uve_prepare("dashjs",true);
// uve_prepare("shaka",true);
// uve_prepare("hlsjs",true);
// uve_prepare("videotag",true);

var player;

function app_load()
{
	player = new UVEPlayer("UVETEST");
	player.addEventListener("playbackStarted", mediaPlaybackStarted);
	player.load(locator,false); // no autoplay
}

function app_play()
{
	player.play();
}

function app_pause()
{
	player.pause();
}

function app_seek(sec)
{
	player.seek(sec);
}

window.onload = function() {
	// key navigation support; needed for settops (no mouse/keyboard)
	document.addEventListener("keydown", keyEventHandler);
	loadButton  = document.getElementById("loadButton");
	playButton  = document.getElementById("playButton");
	pauseButton = document.getElementById("pauseButton");
	seekButton0 = document.getElementById("seekButton0");
	seekButton0 = document.getElementById("seekButton0");
	seekButton0 = document.getElementById("seekButton0");
	seekButton0 = document.getElementById("seekButton0");
	currentObj =  loadButton;
	components = [ loadButton,  playButton,  pauseButton,  seekButton0,  seekButton1,  seekButton2,  seekButton3];
	currentPos = 0;
	addFocus();
}

function mediaPlaybackStarted() {
    console.log("playbackStarted Event Received");
}

keyLeft = function() {
	//goto Previous button
	removeFocus();
   if ( currentPos > 0) {
		currentPos--;
   } else {
		currentPos =  components.length - 1;
   }
	currentObj =  components[ currentPos];
	addFocus();
};

keyRight = function() {
	//goto Next button
	removeFocus();
	if ( currentPos <  components.length - 1) {
		 currentPos++;
	} else {
		 currentPos = 0;
	}
	 currentObj =  components[ currentPos];
	 addFocus();
};

ok = function() {
   switch ( currentPos) {
	   case 0:
			   app_load();
			   break;
	   case 1:
			   app_play();
			   break;
	   case 2:
			   app_pause();
			   break;
	   case 3:
			   app_seek(0*60);
			   break;
	   case 4:
			   app_seek(1*60);
			   break;
	   case 5:
			   app_seek(2*60);
			   break;
	   case 6:
			   app_seek(3*60);
			   break;
   }
}

addFocus = function() {
   if ( currentObj) {
		currentObj.classList.add("focus");
   } else {
		currentObj.focus();
   }
};

removeFocus = function() {
   if ( currentObj) {
		currentObj.classList.remove("focus");
   } else {
		currentObj.blur();
   }
};

keyEventHandler = function(e) {
   var keyCode = e.which || e.keyCode;
   e.preventDefault();
	switch (keyCode) {
		case 37: // Left Arrow
				keyLeft();
				break;
		case 39: // Right Arrow
				keyRight();
				break;
		case 13: // Enter
		case 32:
				ok();
				break;
		default:
				break;
	   }
   return false;
}
