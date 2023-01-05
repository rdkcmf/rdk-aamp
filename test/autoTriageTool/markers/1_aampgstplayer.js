marker_aampgstplayer = [{
	"pattern":"got First Video Frame",
	"label":"Got-First-Video-Frame",
},
	{
	"pattern":"got First Audio Frame",
	"label":"Got-First-Audio-Frame",
},
	{
	"pattern":"Set pipeline state to %% - buffering_timeout_cnt ",
	"label":"Pipeline->Playing-Buffering-Timeout",
},
	{
	"pattern":"Schedule retune for GstPipeline Error",
	"label":"Retune-GST",
},
	{
	"pattern":"GST_MESSAGE_EOS",
	"label":"_EOS_",
},
	{
	"pattern":"AAMPGstPlayerPipeline %% -> %% (",
	"label":"Pipeline(%0->%1)",
},
	{
	"pattern":"flush start error",
	"label":"Flush-Start-Error",
},
	{
	"pattern":"flush stop error",
	"label":"Flush-Stop-Error",
},
	{
	"pattern":"push protection event failed",
	"label":"Push-Protection-Evt=Failed",
},
	{
	"pattern":"AudioType Changed. Force configure pipeline",
	"label":"Audio-Changed-Reconf-Pipeline",
},
	{
	"pattern":"Pipeline is in FAILURE state",
	"label":"Pipeline-Failed-State",
},
	{
	"pattern":"state is changing asynchronously",
	"label":"Pipeline-State-Change-Async",
},
	{
	"pattern":"in an unknown state",
	"label":"Pipeline is in unknown state",
},
	{
	"pattern":"Attempting to set %% state to %%",
	"label":"%0 set to %1",
},
	{
	"pattern":"%% state set to %%",
	"label":"%0 state set to %1",
},
	{
	"pattern":"Attempted to set the state of a null pointer",
	"label":"Pipeline state tried for Null Element",
},
	{
	"pattern":"SetVideoRectangle :: ",
	"label":"Video-Rect",
},
	{
	"pattern":"AAMPGstPlayer: Audio Muted",
	"label":"Audio-Muted",
},
	{
	"pattern":"Audio Unmuted after a Mute",
	"label":"Audio-Unmuted",
},
	{
	"pattern":"Seek failed",
	"label":"Flush-Seek-Failed",
},
	{
	"pattern":"Pipeline flush seek - start =",
	"label":"Flush-Called",
},
	{
	"pattern":"Discontinuity received before first buffer - ignoring",
	"label":"Disco-Received-B4-First-Buf",
},
	{
	"pattern":"Entering AAMPGstPlayer: type(%%) format",
	"label":"Process-EOS-For-Disco(%0)",
},
	{
	"pattern":"EOS already signaled, hence skip",
	"label":"Skip-EOS",
},
	{
	"pattern":"Enough data available to stop buffering",
	"label":"Stop-buffering",
},
	{
	"pattern":"Not enough data available to stop buffering",
	"label":"No-data-to-stop-buffering",
},
	{
	"pattern":"Sending Buffer Change event status (Buffering): %%",
	"label":"%0-Buffering",
}];
