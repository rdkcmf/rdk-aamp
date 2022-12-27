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
		part = line.split(" ");
		if( part[1]==":" )
		{ // simulator format - <sec>:<ms>
			part = part[0].split(":");
			var s = parseInt(part[0]);
			var ms = parseInt(part[1]);
			return s*1000+ms;
		}
		else if( part[2]==":" )
		{
			var s = parseInt(part[0]);
			var ms = parseInt(part[1]);
			return s*1000+ms;
		}

		// format used on settop (GMT time)
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
		//console.log( "ParseReceiverLogTimestamp error with: <LINE>" + line + "</LINE>" );
		return 0;
	}
}

// format includes %% for numbers and strings to be extracted, in-between other literal text
// returns array of extracted values, or null if literal delimiters don't match
function sscanf( string, format )
{
	string = string.trim();
	var first = true;
    var rc = [];
    for(;;)
    {
        var nextParam = format.indexOf("%%");
        if( nextParam<0 )
		{ // no more parameters in format string
			if( first )
			{
				if( string.indexOf(format)<0 )
				{ // must contain simple format
					rc = null;
				}
			}
			else if( !string.startsWith(format) )
			{ // must match trailing characters from format
				rc = null;
			}
			return rc;
		}
		var delim = format.substr(0,nextParam);
		if( first )
		{ // fuzzy match any leading characters
			nextParam = string.indexOf(delim);
			if( nextParam<0 )
			{
				return null;
			}
			first = false;
			string = string.substr(nextParam); // strip wildcard leading characters
			continue;
		}
        else if( string.substr(0,nextParam)!=delim )
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

const ContentType_UNKNOWN = 0;
const ContentType_CDVR = 1;
const ContentType_VOD = 2;
const ContentType_LINEAR = 3;
const ContentType_IVOD = 4;
const ContentType_EAS = 5;
const ContentType_CAMERA = 6;
const ContentType_DVR = 7;
const ContentType_MDVR = 8;
const ContentType_IPDVR = 9;
const ContentType_PPV = 10;
const ContentType_OTT = 11;
const ContentType_OTA = 12;
const ContentType_HDMIIN = 13;
const ContentType_COMPOSITEIN = 14;
const ContentType_SLE = 15;

var mediaTypes = [
"VIDEO",
"AUDIO",
"SUBTITLE",
"AUX_AUDIO",
"MANIFEST",
"LICENSE",
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
	var prefix, offs;
	
	prefix = "HttpRequestEnd: Type: ";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{ // handle older HttpRequestEnd logging as used by FOG
	  // refer DELIA-57887 "[FOG] inconsistent HttpRequestEnd logging"
		// preliminary review shared which introduces "br" (bitrate) field in log - it's present for abrs_fragment#t:VIDEO
		// but not HttpRequestEnd
		var httpRequestEnd = {};
		line = "Type: "  + line.substr(offs+prefix.length);
		line = line.split(", ");
		for( var i=0; i<line.length; i++ )
		{
			var pat = line[i].split( ": " );
			httpRequestEnd[pat[0]] = pat[1];
		}
		if( httpRequestEnd.Type == "DASH-MANIFEST" )
		{
			httpRequestEnd.type = eMEDIATYPE_MANIFEST;
		}
		else if( httpRequestEnd.Type=="IFRAME" )
		{
			httpRequestEnd.type = eMEDIATYPE_IFRAME;
		}
		else if( httpRequestEnd.Type=="VIDEO" )
		{
			httpRequestEnd.type = eMEDIATYPE_VIDEO;
		}
		else if( httpRequestEnd.Type=="AUDIO" )
		{
			httpRequestEnd.type = eMEDIATYPE_AUDIO;
		}
		else
		{
			console.log( "unk type! '" + httpRequestEnd.Type + "'" );
		}
		httpRequestEnd.ulSz = httpRequestEnd.RequestedSize;
		httpRequestEnd.dlSz = httpRequestEnd.DownloadSize;
		httpRequestEnd.curlTime = httpRequestEnd.TotalTime;
		httpRequestEnd.responseCode = parseInt(httpRequestEnd.hcode)||parseInt(httpRequestEnd.cerr);
		httpRequestEnd.url = httpRequestEnd.Url;
		
		return httpRequestEnd;
	}
	
	prefix = "HttpRequestEnd:";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{ // handle HttpRequestEnd as logged by aamp
		var json = line.substr(offs+prefix.length);
		try
		{
			rc = JSON.parse(json);
		}
		catch( err )
		{

			// Parse the logs having comma in url
			var parseUrl = json.split(",http");

			var param = parseUrl[0].split(",");

			//insert url to param list
			param.push("http" + parseUrl[1]);

			var fields = "mediaType,type,responseCode,curlTime,total,connect,startTransfer,resolve,appConnect,preTransfer,redirect,dlSz,ulSz";
			if( param[0] != parseInt(param[0]) )
			{
				// Old log format
				if( param.length == 16) {
					// use downloadbps value as br
					fields = "appName," + fields + ",br,url";
				} else if( param.length == 17) {
					fields = "appName," + fields + ",downloadbps,br,url";
				}
			} else {
				if( param.length == 15) {
					fields += ",br,url";
				} else if( param.length == 16) {
					fields += ",downloadbps,br,url";
				}
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

function ParseFragmentChunk( line )
{
	prefix = "Injecting chunk for video br=";
	offs = line.indexOf(prefix);
	var isVideo = true;
	if( offs<0 )
	{
		prefix = "Injecting chunk for audio br=";
		offs = line.indexOf(prefix);
		isVideo = false;
	}
	if( offs>=0 )
	{
		chunkData = line.substr(offs+prefix.length);
		// sample format:: 2150000,chunksize=271272 fpts=109017.909000 fduration=0.016683
		chunkData = chunkData.split(",");
		var obj = {};
		if( isVideo )
		{
			obj.bitrate = chunkData[0]; // first element is bitrate
		}
		else
		{
			obj.bitrate = "audio";
		}
		chunkData = chunkData[1].split(" ");
		obj.chunksize = chunkData[0].slice(chunkData[0].indexOf('=')+1);
		obj.fpts = chunkData[1].slice(chunkData[1].indexOf('=')+1);
		obj.fduration = chunkData[2].slice(chunkData[2].indexOf('=')+1);
		return obj;
	}
	return null;
}

function ParseABRProfileBandwidth( line )
{
    var rc = null;
    var prefix = "Added Video Profile to ABR BW= ";
    var offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var text = line.substr(offs+prefix.length);
		const textArray = text.split(" ");

		return textArray[0];
	}
	
    return rc;
}

function ParseCurrentBitrate( line )
{
    var rc = null;
    var prefix = "NotifyBitRateChangeEvent :: bitrate:";
    var offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var text = line.substr(offs+prefix.length);
		const textArray = text.split(" ");

		return textArray[0];
	}
	
    return rc;
}

var contentTypeString = [
"UNKNOWN",
"CDVR",
"VOD",
"LINEAR",
"IVOD",
"EAS",
"CAMERA",
"DVR",
"MDVR",
"IPDVR",
"PPV",
"OTT",
"OTA",
"HDMIIN",
"COMPOSITEIN",
"SLE"];

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

// various indices into IP_AAMP_TUNETIME
const _tuneStartBaseUTCMS = 2;
const _drmLicenseAcqStartTime = 26;
const _drmLicenseAcqTotalTime = 27;
const _drmFailErrorCode = 28;
const _LicenseAcqPreProcessingDuration = 29;
const _LicenseAcqNetworkDuration = 30;
const _LicenseAcqPostProcDuration = 31;
const _VideoFragmentDecryptDuration = 32;
const _AudioFragmentDecryptDuration = 33;
const _gstPlayStartTime = 34;
const _gstFirstFrameTime = 35;
const _tuneSuccess = 44;
const _contentType = 36;
