const BUCKET_SIZE_MS = 250;
var gTuneBaseTimeUTC;
var gOutput;
var gStatistics;
var gTuneStatistics;
var gTimeStamp;

function ExportFile( text, fileName )
{
	let a = document.createElement('a');
	a.href = "data:application/octet-stream,"+encodeURIComponent(text);
	a.download = fileName;
	a.click();
}

function popUp( html ){
	var windowHandle = window.open("","","width=300,height=300,scrollbars=1,resizable=1");
	if( windowHandle===null )
	{
		alert( "unable to open window" );
	}
	else
	{
		windowHandle.document.open();
		windowHandle.document.write(html);
		windowHandle.document.close();
	}
}

function ExportStatsFormatTime( utc )
{
	if( !gTuneBaseTimeUTC )
	{
		gTuneBaseTimeUTC = utc;
	}
	
	// convert elapsed milliseconds to fixed-width <minutes>:<seconds>.<ms>
	utc -= gTuneBaseTimeUTC; // make relative
	var ms = utc%1000;
	var s = Math.floor(utc/1000);
	var minute = Math.floor( s/60 );
	var second = s%60; // 0..59
	if( second<10 ) second = "0" + second;
	if( ms<10 ) ms = "00" + ms; else if( ms<100 ) ms = "0" + ms;
	return Pad(minute) + ":" + second + "." + ms;
}

function Output( text, color )
{
	var string = /*Pad(lineNumber+1)+":" +*/ ExportStatsFormatTime(gTimeStamp) + " " + text;
	if( color )
	{
		string = '<font color="'+color+'">' + string + '</font>';
	}
	gOutput.push( string );
}

function Fixed( value )
{
	var f = parseFloat(value);
	return Math.round(f*1000)/1000.0;
}

function UpdateTuneStatistics( ms )
{
	var err = "Success";
	var type = "Tune Time";
	if( !gTuneStatistics[type] )
	{
		gTuneStatistics[type] = [];
	}
	if( !gTuneStatistics[type][err] )
	{
		gTuneStatistics[type][err] = 0;
	}
	gTuneStatistics[type][err]++;
	for( var bucket=0; bucket<10000; bucket+=BUCKET_SIZE_MS )
	{
		if( ms<bucket )
		{
			var key = bucket;
			if( !gTuneStatistics[type][key] )
			{
				gTuneStatistics[type][key] = 0;
			}
			gTuneStatistics[type][key]++;
			break;
		}
	}
}
function UpdateDownloadStats( err, type, url, ms )
{
	if( url.indexOf(".fwmrm.")>=0 )
	{
		type += " AD";
	}
	if( !gStatistics[type] )
	{
		gStatistics[type] = [];
	}
	if( !gStatistics[type][err] )
	{
		gStatistics[type][err] = 0;
	}
	gStatistics[type][err]++;
	if( err=="HTTP200(OK)" )
	{ // populate histogram buckets, to allow view of download time distribution
		// we might want to split by file type; playlists and video fragments likely to have very
		// different download characteristics.
		if( ms<0 || ms>10000 ) ms = 10000;
		for( var bucket=0; bucket<10000; bucket+=BUCKET_SIZE_MS )
		{
			if( ms<bucket )
			{
				var key = bucket;
				if( !gStatistics[type][key] )
				{
					gStatistics[type][key] = 0;
				}
				gStatistics[type][key]++;
				break;
			}
		}
	}
}

function ExportStatistics( s, name )
{
	var extra = "";
	var types = Object.keys(s);
	for( var t=0; t<types.length; t++ )
	{
		var type = types[t];
		var goodDownloadCount = s[type][name];
		var keys = Object.keys(s[type]);
		extra += "<br/>"+type + ":";
		
		for( var i=0; i<keys.length; i++ )
		{
			var attr = keys[i];
			var isNumericAttr = /^\d+$/.test(attr);
			var value = s[type][keys[i]];
			if( isNumericAttr )
			{ // histogram bucket
				var pct = Math.round(100*(value/goodDownloadCount));
				if( pct<10 ) pct = "  " + pct; else if( pct<100 ) pct = " " + pct;
				value = value + " "+ pct + "%";
				attr = (attr-BUCKET_SIZE_MS) + ".." + attr;
			}
			else
			{
				value = value.toString();
				value += "     ";
			}
			while( value.length < 10 )
			{
				value = " " + value;
			}
			extra += "<br/>"+ value + " : " + attr;
		}
	}
	return extra;
}

function ExportLogs()
{
	var extra1 = ExportStatistics(gStatistics,"HTTP200(OK)" );
	var extra2 = ExportStatistics(gTuneStatistics,"Success" );
	
	var tunedist = ["range(ms),count"];
	for( var i=0; i<10000; i+=BUCKET_SIZE_MS )
	{
		var count = gTuneStatistics["Tune Time"][i];
		if( !count ) count = 0;
		var range = (i)+".."+(i+250)+"ms";
		tunedist.push( range+","+count);
	}
	ExportFile( tunedist.join("\r\n"), "tune-distribution.csv" );
	
	popUp( "<html><head><title>" + this.filename + "</title></head><body><pre>" +
		  gOutput.join("<br/>") +
		  "<hr/>" + extra1 +"<hr/>" + extra2 +
		  "</pre></body></html>" );
}


function ExportFile( text, fileName )
{
	let a = document.createElement('a');
	a.href = "data:application/octet-stream,"+encodeURIComponent(text);
	a.download = fileName;
	a.click();
}

// input: gAllIpExTuneTimes (X1-only)
// input: gAllIpAampTuneTimes
// input: gAllLogLines
function ExportHelper()
{
	ExportFile( gAllIpExTuneTimes.join("\r\n"), "IP_EX_TUNETIME.csv" );
	ExportFile( gAllIpAampTuneTimes.join("\r\n"), "IP_AAMP_TUNETIME.csv" );
	gOutput = [];
	gStatistics = [];
	gTuneStatistics = [];
	gTuneBaseTimeUTC = null;
	var err,type,code,url;
	
	var aampPlayerPrefix = "[AAMP-PLAYER]";
	for( var lineNumber=0; lineNumber<gAllLogLines.length; lineNumber++ )
	{
		var line = gAllLogLines[lineNumber];
		gTimeStamp = ParseReceiverLogTimestamp(line);
		
		var offs = line.indexOf("AAMPMediaPlayerJS_initConfig(): ");
		if( offs>=0 )
		{
			Output( line.substr(offs+32), "blue" );
			continue;
		}
		
		var aampLogOffset = line.indexOf(aampPlayerPrefix);
		if( aampLogOffset >=0 )
		{
			var payload = line.substr(aampLogOffset+aampPlayerPrefix.length);
			var param;
			var httpRequestEnd;
			
			if( payload.indexOf("aamp_tune:")>=0 )
			{
				Output( "--> New Tune "+new Date(gTimeStamp), "green" );
			}
			else if( (param = sscanf(payload,"AAMPLogNetworkError error='%% error %%' type='%%' location='%%' symptom='%%' url='%%'")) )
			{
				err = mapError(param[1]);
				type = param[2];
				if( type == "main manifest" )
				{
					type = "MANIFEST";
				}
				else if( type == "sub manifest" )
				{
					type = "PLAYLIST";
				}
				else if( type == "video segment" )
				{
					type = "VIDEO";
				}
				else if( type == "audio segment" )
				{
					type = "AUDIO";
				}
				else if( type == "iframe" )
				{
					type = "IFRAME";
				}
				url = param[5];
				//Output( "neterr " + err + ", " + type + ", " + url );
			}
			else if( (httpRequestEnd=ParseHttpRequestEnd(line)) )
			{
				err = mapError(httpRequestEnd.responseCode);
				var ms = 1000*httpRequestEnd.curlTime;
				url = httpRequestEnd.url;
				type = httpRequestEnd.type;
				if( type!==undefined ) type = mediaTypes[type]; else type = "";
				
				var special = null;
				// for codes other than 2xx success
				if( !err.match("^HTTP2[0-9]+.*") )
				{
					special = "red";
				}
				else if( ms>=2000 )
				{
					special = "orange";
				}
				Output( "curl" +
					   Pad(ms) + "ms, ulSz=" +
					   Pad(httpRequestEnd.ulSz) + ", dlSz=" +
					   Pad(httpRequestEnd.dlSz) + ", " +
					   err + ", " + type + ", " + url,
					   special );
				UpdateDownloadStats( err, type, url, ms );
			}
			else if( payload.indexOf("IP_AAMP_TUNETIME:")>=0 )
			{
				param = payload.substr(17).split(",");
				var ms = param[35];
				Output( "IP_AAMP_TUNETIME "+ms, "green" );
				UpdateTuneStatistics(ms);
			}
		}
	}
	var extra1 = ExportStatistics(gStatistics,"HTTP200(OK)");
	var extra2 = ExportStatistics(gTuneStatistics,"Success");

	popUp( "<html><head><title>Statistics</title></head><body><pre>" +
			gOutput.join("<br/>") +
		  "<hr/>" + extra1 +"<hr/>" + extra2 +
			"</pre></body></html>" );
}

