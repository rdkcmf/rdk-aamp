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
		// if log is in unix timestamp format from osx simulator
		// "<sec>:<ms> : " format used in simulator
		let unixTime = String(line.match("^[0-9 ]+:[0-9 ]+"));
		if(unixTime != "null") {
			let uList = unixTime.split(':');
			// add milliseconds to unix time
			parsedUTCTime = parseInt(uList[0]) * 1000 + parseInt(uList[1]);
			return parsedUTCTime;
		} else {
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
	}
	catch( e )
	{
		console.log( "ParseReceiverLogTimestamp error with: <LINE>" + line + "</LINE>" );
		return 0;
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
	201:"Created",
	202:"Accepted",
	203:"Non-Authoritative Information",
	204:"No Content",
	205:"Reset Content",
	206:"Partial Content",
	207:"Multi-Status",
	208:"Already Reported",
	226:"IM Used",
	250:"Low On Storage Space",
    302:"Temporary Redirect",
    304:"Not Modified",
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

const eMEDIATYPE_VIDEO = 0;
const eMEDIATYPE_AUDIO = 1;
const eMEDIATYPE_SUBTITLE = 2;
const eMEDIATYPE_AUX_AUDIO = 3;
const eMEDIATYPE_MANIFEST = 4;
const eMEDIATYPE_LICENSE = 5;
const eMEDIATYPE_IFRAME = 6;
const eMEDIATYPE_INIT_VIDEO = 7;
const eMEDIATYPE_INIT_AUDIO = 8;
const eMEDIATYPE_INIT_SUBTITLE = 9;
const eMEDIATYPE_INIT_AUX_AUDIO = 10;
const eMEDIATYPE_PLAYLIST_VIDEO = 11;
const eMEDIATYPE_PLAYLIST_AUDIO = 12;
const eMEDIATYPE_PLAYLIST_SUBTITLE = 13;
const eMEDIATYPE_PLAYLIST_AUX_AUDIO = 14;
const eMEDIATYPE_PLAYLIST_IFRAME = 15;
const eMEDIATYPE_INIT_IFRAME = 16;
const eMEDIATYPE_DSM_CC = 17;
const eMEDIATYPE_IMAGE = 18;

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

/*
 // hack! - needed if processing logs from stable2 today
{ // older build
	mediaTypes =
		[
		"VIDEO",
		"AUDIO",
		"SUBTITLE",
		//"AUX_AUDIO",
		"MANIFEST",
		"LICENCE",
		"IFRAME",
		"INIT_VIDEO",
		"INIT_AUDIO",
		"INIT_SUBTITLE",
		//"INIT_AUX_AUDIO",
		"PLAYLIST_VIDEO",
		"PLAYLIST_AUDIO",
		"PLAYLIST_SUBTITLE",
		//"PLAYLIST_AUX_AUDIO",
		"PLAYLIST_IFRAME",
		"PLAYLIST_INIT_IFRAME",
		"DSM_CC",
		"IMAGE" ];
	eMEDIATYPE_AUX_AUDIO = null;
	eMEDIATYPE_MANIFEST = 3;
	eMEDIATYPE_LICENSE = 4;
	eMEDIATYPE_IFRAME = 5;
	eMEDIATYPE_INIT_VIDEO = 6;
	eMEDIATYPE_INIT_AUDIO = 7;
	eMEDIATYPE_INIT_SUBTITLE = 8;
	eMEDIATYPE_INIT_AUX_AUDIO = null;
	eMEDIATYPE_PLAYLIST_VIDEO = 9;
	eMEDIATYPE_PLAYLIST_AUDIO = 10;
	eMEDIATYPE_PLAYLIST_SUBTITLE = 11;
	eMEDIATYPE_PLAYLIST_AUX_AUDIO = null;
	eMEDIATYPE_PLAYLIST_IFRAME = 12;
	eMEDIATYPE_INIT_IFRAME = 13;
	eMEDIATYPE_DSM_CC = 14;
	eMEDIATYPE_IMAGE = 15;
}
*/

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
			var fields = "mediaType,type,responseCode,curlTime,total,connect,startTransfer,resolve,appConnect,preTransfer,redirect,dlSz,ulSz,url";
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

/*
[startFragmentDownloadInternal]: TotalTime: 2.2008, ConnectTime: 0.0001, APPConnectTime: 0.0001, PreTransferTime: 0.0005, StartTransferTime: 0.0870, RedirectTime: 0.0000, URL: https://lin203-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/Sky_SDR_Spddd_UD_SU_SKYUK_7221_0_6147665276772982163/track-video-repid-root_video0-tc-0-frag-815568476.mp4
*/
function ParseFogDownload( line )
{
	var prefix, offs;
	
	prefix = "abrs_fragment#";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var payload = line.substr(offs+prefix.length);
		var temp = sscanf( payload, "t:%%,s:%%,d:%%,sz:%%,r:%%,cerr:%%,hcode:%%,n:%%,estr:%%,url:%%");
		var httpRequestEnd = {};
		httpRequestEnd.type = temp[0];
		httpRequestEnd.curlTime = parseFloat(temp[2])/1000.0;
		httpRequestEnd.dlSz = parseInt(temp[3]);
		httpRequestEnd.responseCode = parseInt(temp[5])||parseInt(temp[6]);
		httpRequestEnd.url = temp[9];
		httpRequestEnd.ulSz = 0;
		return httpRequestEnd;
	}
	
	prefix = "On-demand download of ";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var payload = line.substr(offs+prefix.length);
		var temp = sscanf( payload, ":%% state:%% r:%% from:%%");
		var httpRequestEnd = {};
		httpRequestEnd.curlTime = 1.0; // hack
		httpRequestEnd.dlSz = 1; // hack
		httpRequestEnd.responseCode = 200; // hack
		httpRequestEnd.url = temp[3];
		httpRequestEnd.ulSz = 0;
		if( httpRequestEnd.url.indexOf("video")>=0 )
		{
			httpRequestEnd.type = "VIDEO";
		}
		else if( httpRequestEnd.url.indexOf("audio")>=0 )
		{
			httpRequestEnd.type = "AUDIO";
		}
		else
		{
			httpRequestEnd.type = "?";
		}
		if( httpRequestEnd.url.indexOf("-init")>=0 )
		{
			httpRequestEnd.type = "INIT_" + httpRequestEnd.type;
		}
		return httpRequestEnd;
	}
	
	//2021 Sep 27 20:07:36.279702 fogcli[6915]: Requesting GET /manifests/eba75a7c-5ade-4c05-b5da-f94a79dfc35a/abr/815558146-2/Bandwidth-6052400-init.seg
	
	return null;
	
	var prefix = "[startFragmentDownloadInternal]: ";
	var offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var payload = line.substr(offs+prefix.length);
		var temp = sscanf( payload, "TotalTime: %%, ConnectTime: %%, APPConnectTime:%%, PreTransferTime: %%, StartTransferTime: %%, RedirectTime: %%, URL: %%" );
		var httpRequestEnd = {};
		httpRequestEnd.responseCode = 200;
		httpRequestEnd.curlTime = parseFloat(temp[0]);
		httpRequestEnd.url = temp[6];
		httpRequestEnd.type = 0;
		httpRequestEnd.ulSz = 0;
		httpRequestEnd.dlSz = 0;
		return httpRequestEnd;
	}
	return null;
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
