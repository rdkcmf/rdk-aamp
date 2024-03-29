var months =
[
    "Jan","Feb","Mar",
    "Apr","May","Jun",
    "Jul","Aug","Sep",
    "Oct","Nov","Dec"
];

function ParseReceiverLogTimestamp( line )
{
	try
	{
    var marker = "[AAMP-PLAYER]";
    var markerOffset = line.indexOf(marker);
    if( markerOffset<0 )
    {
        marker = "[ABRManager]";
        markerOffset = line.indexOf(marker);
        if( markerOffset<0 )
        {
            return null; // ignore - not AAMP-related
        }
    }
    
    line = line.substr(0,markerOffset);
    var part = line.split(":");
    if( part.length==3 )
    { // "<sec>:<ms> : " format used in simulator
        var utc = parseInt(part[0])*1000+parseInt(part[1]);
        return utc;
    }
    
    // format used on settop (GMT time)
    part = line.split(" ");
    var idx = 0;
    var year = parseInt(part[idx++]);
    if( isNaN(year) )
    { // year missing (fog logs)
        year = new Date().getFullYear();
        idx--;
    }
    var month = months.indexOf(part[idx++]);
    var day = parseInt(part[idx++]);
    var time = part[idx++].split(":");
    var hour = parseInt(time[0]);
    var minute = parseInt(time[1]);
    var seconds = parseFloat(time[2]);
    var s = Math.floor(seconds);
    var ms = Math.floor((seconds - s)*1000);
    return Date.UTC(year, month, day, hour, minute, s, ms );
	}
	catch( e )
	{
		console.log( "ParseReceiverLogTimestamp error with: <LINE>" + line + "</LINE>" );
	}
}

// format includes %% for numbers and strings to be extracted, in-between other literal text
// returns array of extracted values, or null if literal delimiters don't match
function sscanf( string, format )
{
    var rc = [];
    for(;;)
    {
        var nextParam = format.indexOf("%%");
        if( nextParam<0 ) return rc; // done
        if( string.substr(0,nextParam)!=format.substr(0,nextParam) )
        {
            return null;
        }
        string = string.substr(nextParam); // strip leading characters
        format = format.substr(nextParam+2); // skip past %% in format string
        var charAfterParam = format.substr(0,1);
        var idx = string.indexOf(charAfterParam);
        if( idx<0 || format=="" )
        { // no final delimiter
            rc.push( string );
        }
        else
        { // extract value between delimiters
            rc.push( string.substr(0,idx) );
            string = string.substr(idx);
        }
    }
}

function Pad( num )
{ // used for formatting - convert to fixed width by padding from left with spaces
    num = Math.round(num);
    if( num<10 ) return "      "+num;
    if( num<100 ) return "     "+num;
    if( num<1000 ) return "    "+num;
    if( num<10000 ) return "   "+num;
    if( num<100000 ) return "  "+num;
    if( num<1000000 ) return " "+num;
    return num;
}

function Pad2( n )
{ // prefix 2 digit number with leading zeros
	if( n<10 ) return "0" + n;
	return n;
}

function Pad3( n )
{ // prefix 3 digit number with leading zeros
	if( n<10 ) return "00" + n;
	if( n<100 ) return "0" + n;
	return n;
}

function FormatTime( duration )
{
	var ms = duration%1000;
	duration = (duration-ms)/1000;
	var s = duration%60;
	duration = (duration-s)/60;
	var m = duration%60;
	duration = (duration-m)/60;
	var h = duration;
	var rc = [];
	if( h>0 ){ rc.push(h); rc.push(Pad2(m)); rc.push(Pad2(s)); }
	else if( m>0 ){ rc.push(m); rc.push(Pad2(s)); }
	else{ rc.push(s); }
	return rc.join(":");
}

var httpCurlErrMap =
{
    0:"OK",
    7:"Couldn't Connect",
    18:"Partial File",
    23:"Interrupted Download",
    28:"Operation Timed Out",
    42:"Aborted by callback",
    56:"Failure with receiving network data",
    200:"OK",
    302:"Temporary Redirect",
    304:"Not Modified",
    204:"No Content",
    400:"Bad Request",
    401:"Unauthorized",
    403:"Forbidden",
    404:"Not Found",
    500:"Internal Server Error",
    502:"Bad Gateway",
    503:"Service Unavailable"
};

function mapError( code )
{
	code = parseInt(code);
    var desc = httpCurlErrMap[code];
    if( desc!=null )
    {
        desc = "("+desc+")";
    }
    else
    {
        desc = "";
    }
    return ((code<100)?"CURL":"HTTP") + code + desc;
}

var eMEDIATYPE_VIDEO = 0;
var eMEDIATYPE_AUDIO = 1;
var eMEDIATYPE_SUBTITLE = 2;
var eMEDIATYPE_AUX_AUDIO = 3;
var eMEDIATYPE_MANIFEST = 4;
var eMEDIATYPE_LICENCE = 5;
var eMEDIATYPE_IFRAME = 6;
var eMEDIATYPE_INIT_VIDEO = 7;
var eMEDIATYPE_INIT_AUDIO = 8;
var eMEDIATYPE_INIT_SUBTITLE = 9;
var eMEDIATYPE_INIT_AUX_AUDIO = 10;
var eMEDIATYPE_PLAYLIST_VIDEO = 11;
var eMEDIATYPE_PLAYLIST_AUDIO = 12;
var eMEDIATYPE_PLAYLIST_SUBTITLE = 13;
var eMEDIATYPE_PLAYLIST_AUX_AUDIO = 14;
var eMEDIATYPE_PLAYLIST_IFRAME = 15;
var eMEDIATYPE_INIT_IFRAME = 16;
var eMEDIATYPE_DSM_CC = 17;
var eMEDIATYPE_IMAGE = 18;

var mediaTypes = [
"VIDEO",
"AUDIO",
"SUBTITLE",
"AUX_AUDIO",
"MANIFEST",
"LICENCE",
"IFRAME",
"INIT_VIDEO",
"INIT_AUDIO",
"INIT_SUBTITLE",
"INIT_AUX_AUDIO",
"PLAYLIST_VIDEO",
"PLAYLIST_AUDIO",
"PLAYLIST_SUBTITLE",
"PLAYLIST_AUX_AUDIO",
"PLAYLIST_IFRAME",
"PLAYLIST_INIT_IFRAME",
"DSM_CC",
"IMAGE" ];

function ParseHttpRequestEnd( line )
{
    var rc = null;
    var prefix = "HttpRequestEnd:";
    var offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var json = line.substr(offs+prefix.length);
		try
		{
			rc = JSON.parse(json);
		}
		catch( err )
		{
			var fields = "mediaType,type,responseCode,curlTime,total,connect,startTransfer,resolve,appConnect,preTransfer,redirect,dlSz,ulSz,bps,url";
			var param = json.split(",");
			if( param[0] != parseInt(param[0]) )
			{
				fields = "appName,"+fields;
			}
			fields = fields.split(",");
			var httpRequestEnd = {};
			for( var i=0; i<fields.length; i++ )
			{
				httpRequestEnd[fields[i]] = param[i];
			};
			httpRequestEnd.type = parseInt(httpRequestEnd.type);
			httpRequestEnd.responseCode = parseInt(httpRequestEnd.responseCode);
			httpRequestEnd.curlTime = parseFloat(httpRequestEnd.curlTime);
			return httpRequestEnd;
		}
	}
	
	prefix = "HttpLicenseRequestEnd:";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var json = line.substr(offs+prefix.length);
		try
		{
			rc = JSON.parse(json);
			rc.url = rc.license_url;
			rc.type = eMEDIATYPE_LICENSE;
		}
		catch( err )
		{
			console.log( "ignoring corrupt JSON from receiver log<JSON>" + json + "</JSON>" );
		}
	}
    return rc;
}

var trackNames = [ "video", "audio", "subtitle" ];

var playerStates = [ // PrivAAMPState
 "eSTATE_IDLE",
 "eSTATE_INITIALIZING",
 "eSTATE_INITIALIZED",
 "eSTATE_PREPARING",
 "eSTATE_PREPARED",
 "eSTATE_BUFFERING",
 "eSTATE_PAUSED",
 "eSTATE_SEEKING",
 "eSTATE_PLAYING",
 "eSTATE_STOPPING",
 "eSTATE_STOPPED",
 "eSTATE_COMPLETE",
 "eSTATE_ERROR",
 "eSTATE_RELEASED",
 "eSTATE_BLOCKED",
];

var eventNames = [
 null,
 "tuned",
 "tuneFailed",
 "speedChanged",
 "eos",
 "playlistIndexed",
 "progress",
 "decoderAvail", // AAMP_EVENT_CC_HANDLE_RECEIVED
 "jsEvent",
 "metadata",
 "enteringLive",
 "bitrateChanged",
 "timedMetadata",
 "bulkTimedMetadata",
 "statusChanged",
 "speedsChanged",
 "seeked",
 "tuneprofiling",
 "bufferingChanged",
 "durationChanged",
 "audioTracksChanged",
 "textTracksChanged",
 "adBreaksChanged",
 "adStarted",
 "adCompleted",
 "drmMetadata",
 "anomaly",
 "webVttCueData",
 "adResolved",
 "adReservationStart",
 "adReservationEnd",
 "adPlacementStart",
 "adPlacementEnd",
 "adPlacementError",
 "adPlacementProgress",
 "metricsData",
 "id3Mmetadata",
 "drmMessage",
 "blocked",
];

var bitrateChangedReason =
	[
	"ABR:bandwidth", // eAAMP_BITRATE_CHANGE_BY_ABR
	"Rampdown:failure", // eAAMP_BITRATE_CHANGE_BY_RAMPDOWN
	"Tune:ABR reset", // eAAMP_BITRATE_CHANGE_BY_TUNE
	"Seek:ABR reset", // eAAMP_BITRATE_CHANGE_BY_SEEK
	"Trickplay:ABR ", // eAAMP_BITRATE_CHANGE_BY_TRICKPLAY
	"Rampup - full buf", // eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL
	"Rampdown - empty buf", // eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY
	"ABR:FOG", // eAAMP_BITRATE_CHANGE_BY_FOG_ABR
	"ABR:OTA", // eAAMP_BITRATE_CHANGE_BY_OTA
	"ABR:HDMIIN", // eAAMP_BITRATE_CHANGE_BY_HDMIIN
	 ];
