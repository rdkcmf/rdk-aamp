marker_general = [
	{ "pattern": "Period(%% - %%/%%) Offset[%%] IsLive(%%) IsCdvr(%%)",
		"label":"Period->%0(%3)"
	},
	{
		"pattern": "FOREGROUND PLAYER[%%] APP: %% aamp_tune: attempt: %% format: %% URL:",
		"label":"Aamp-Tune-FG(%0)(%1)(%2)(%3)"
	},
	{
		"pattern": "BACKGROUND PLAYER[%%] APP: %% aamp_tune: attempt: %% format: %% URL:",
		"label":"Aamp-Tune-BG(%0)(%1)(%2)(%3)"
	},
	{
		"pattern": "aamp_tune: attempt: %% format: %% URL:",
		"label":"Aamp-Tune(%1)",
	},
	{
		"pattern": "UveAAMPPlayer: setBlock: %% ",
		"label":"SetBlock(%0)",
	},
	{
		"pattern":"[ThunderAccessAAMP] %% plugin Activated. Response : %%",
		"label":"Plgin-Activated"
	},
	{
		"pattern":"[ThunderAccessAAMP] %% : invoke failed with error status %%",
		"label":"Plgin-InvokeFailed"
	},
	{
		"pattern":"aamp_stop PlayerState=%%",
		"label":"Aamp-Stop" // playerStates[parseInt(param[0])]
	},
	{
		"pattern":"aamp_Seek(%%) and seekToLive(%%)",
		"label":"Seek(%0)",
	},
	{
		"pattern":"Got Underflow message from %% type %%",
		"label":"%1 Underflow"
	},
	{
		"pattern":"aamp pos: [%%..%%..%%..]",
		"label":"Pos(%1)",
	},
	{
		"pattern":"processKey: Key Usable",
		"label":"KeyUsable",
	},
	{
		"pattern":"TuneHelper : mVideoFormat %%, mAudioFormat %% mAuxFormat %%",
		"label":"TuneHelper:V(%0)A(%1)AX(%2)",
	},
	{
		"pattern":"Updated seek_pos_seconds %% culledSeconds %%",
		"label":"TuneHelper:SeekPosUpdate(%0)",
	},
	{
		"pattern":"AAMPGstPlayerPipeline buffering_enabled %%",
		"label":"BufferingEnabled(%0)",
	},
	{
		"pattern":"fragment injector started. track %%",
		"label":"Inject-Start(%0)",
	},
	{
		"pattern":"fragment injector done. track %%",
		"label":"Inject-Stop(%0)",
	},
	{
		"pattern":"[SendEventSync][%%](type=%%)(state=%%)",
		"label":"Event(%1::%2)",
	},
	{
		"pattern":"Ignored underflow from %%, disableUnderflow config enabled",
		"label":"Ignored-Underflow",
	},
	{
		"pattern":"restore prev-pos as current-pos",
		"label":"Progress Report Problem"
	},
	{
		"pattern":"RDKBROWSER_RENDER_PROCESS_CRASHED",
		"label":"Crashed!!!",
        "style":"#ff0000"
	},
	{
		"pattern":"WebProcess is unresponsive",
		"label":"WebProcess unresponsive",
        "style":"#ff3d3d"
	},
	{
		"pattern":"Critical load average for",
		"label":"High load average",
        "style":"#f59f00"
	}];
