marker_sky = [
	{
		"pattern":"Send first progress event with position",
		"label":"1st progress",
		"style":"#ff5555"
	},
	
	{
		"pattern":"EPG Request to play VIPER stream",
		"label":"EPG Tune Start",
		"style":"#55cc55"
	},
	{
		"pattern":"EPG Show notification id :  \"OTTBUFFERING\"",
		"label":"OTTBUFFERING",
		"style":"#55cc55"
	},
	{
		"pattern":"EPG Total tune time (ms)",
		"label":"EPG Tune Complete",
		"style":"#55cc55"
	},

	{
		"pattern":"XXXUnhandled event type:",
		"label":"unhandled event",
		"style":"#cc55cc"
	},
	{
		"pattern":'"type":"MediaProgress"',
		"label":"AS-MediaProgress",
		"style":"#cc55cc"
	},
	{
		"pattern":"First MediaProgress position",
		"label":"JSPP-First-MediaProgress",
		"style":"#4d7222"
	},
	{
		"pattern":"XXXNew Request from AS to JSPP",
		"label":"AS to JSPP",
		"style":"#cc55cc"
	},
	{
		"pattern":"Current playback state is now PLAYING",
		"label":"now PLAYING",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnName":"stop"}',
		"label":"stop",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnArgs":[1],"fnName":"setVolume"}',
		"label":"setVolume",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnArgs":["en"],"fnName":"setPreferredAudioLanguage"}',
		"label":"setPreferredAudioLanguage",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnArgs":[1920,1080],"fnName":"setDimensionsOfVideo"}',
		"label":"setPreferredAudioLanguage",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnName":"getAvailableAudioTracks"}',
		"label":"getAvailableAudioTracks",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnName":"getAvailableTextTracks"}',
		"label":"getAvailableTextTracks",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnArgs":[true],"fnName":"setTextTrackEnabled"}',
		"label":"setTextTrackEnabled",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnArgs":["en","SUBTITLES"],"fnName":"setTextTrack"}',
		"label":"setTextTrack",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnName":"getCurrentTextTrack"}',
		"label":"getCurrentTextTrack",
		"style":"#cc55cc"
	},
	{
		"pattern":'{"fnName":"getCurrentAudioTrack"}',
		"label":"getCurrentAudioTrack",
		"style":"#cc55cc"
	},
	
	{
        "pattern":"BACKGROUND=>FOREGROUND",
        "label":"Player Brought to Foreground",
    },

    {"style":"#5555cc", "pattern":"Application UveVODAdManager started","label":"UveVODAdManager"},
    {"style":"#5555cc", "pattern":"EngineSelector getEngine: type selected: uve_aamp","label":"getEngine"},
    {"style":"#5555cc", "pattern":"Application UveAAMPPlayer started","label":"veAAMPPlayer"},
    {"style":"#5555cc", "pattern":"UveAAMPPlayer setAsset","label":"setAsset" },

    {"style":"#5555cc", "pattern":"[AAMP_JS_EVENT] event onMediaMetadata","label":"onMediaMetadata"},
    {"style":"#5555cc", "pattern":"UveVODAdResolver Total Number of Temporal Slots to be submitted","label":"UveVODAdResolver"},
    {"style":"#5555cc", "pattern":"Context.dispatchEvent type: onRequestInitiated","label":"onRequestInitiated"},
    
    {"style":"#5555cc", "pattern":"Context.submitRequest","label":"fw send"},
    {"style":"#5555cc", "pattern":"Ad request succeeded","label":"fw rcv"},
    
    {"style":"#5555cc", "pattern":"UveVODAdController start","label":"UveVODAdController"},
    {"style":"#5555cc", "pattern":"Slot.play PREROLL","label":"PREROLL"},
    
    {"style":"#5555cc", "pattern":"Slot.playNextAdInstance  PREROLL","label":"playNextAdInstance"},
    {"style":"#5555cc", "pattern":"UveAAMPRenderer start","label":"UveAAMPRenderer"},
    
    {"style":"#5555cc", "pattern":"UveAampHandler UveAampHandler(0): load","label":"load"},
    {"style":"#5555cc", "pattern":"UveAampHandler UveAampHandler(0): play","label":"play"},
    
    {"style":"#5555cc", "pattern":"PlayerPlatformAPI: onPlayStateChanged - new state: initializing","label":"initializing"},
    {"style":"#5555cc", "pattern":"PlayerPlatformAPI: onPlayStateChanged - new state: initialized","label":"initialized"},
    {"style":"#5555cc", "pattern":"PlayerPlatformAPI: onPlayStateChanged - new state: preparing","label":"preparing"},
    {"style":"#5555cc", "pattern":"PlayerPlatformAPI: onPlayStateChanged - new state: prepared","label":"prepared"},
    {"style":"#5555cc", "pattern":"PlayerPlatformAPI: onPlayStateChanged - new state: playing","label":"playing"},
];
