marker_main = [{
	"pattern":"operation is not allowed when player in eSTATE_ERROR state",
	"label":"NOP-Error-State",
},
	{
	"pattern":"operation is not allowed when player in %% state",
	"label":"NOP-%0-State",
},
	{
	"pattern":"operation is not allowed in %% state",
	"label":"NOP-%0-State",
},
	{
	"pattern":"Ignoring trickplay. No iframe tracks in stream",
	"label":"Ignore-Trickplay",
},
	{
	"pattern":"PLAYER[%%] Player BACKGROUND=>FOREGROUND",
	"label":"(%0)BG=>FG",
},
	{
	"pattern":"Already at logical live point, hence skipping operation",
	"label":"Skip-Op",
},
	{
	"pattern":"Already running at playback rate",
	"label":"Skip-SetRate",
},
	{
	"pattern":"Already at live point",
	"label":"Skip-Op",
},
	{
	"pattern":"SetRate ignored!! Invalid rate",
	"label":"Invalid-Rate",
},
	{
	"pattern":"outside the range supported",
	"label":"Aud-OutOfRange",
},
	{
	"pattern":"Enable async tune operation",
	"label":"Enable-Async",
},
	{
	"pattern":"Disable async tune operation",
	"label":"Disable-Async",
},
	{
	"pattern":"PlayerInstanceAAMP: device::Manager initialization failed",
	"label":"Device::Manager-Init-Failed",
},
	{
	"pattern":"PlayerInstanceAAMP:: invalid drm type",
	"label":"Invalid-Drm",
},
	{
	"pattern":"Auxiliary audio language is not supported in this platform",
	"label":"Aux-Not-Supported",
},
	{
	"pattern":"Sending error ",
	"label":"Playback-Error",
},
	{
	"pattern":"NotifyBitRateChangeEvent :: bitrate",
	"label":"Notify-Bitrate-Change",
    "style":"#d35811"
}];
