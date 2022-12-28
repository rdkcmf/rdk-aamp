const currentVersion = "2.1";
const ROWHEIGHT = 24;
const BITRATE_MARGIN = 112;
const TOP_MARGIN = 64;
const COLOR_TUNED = "#058840";
const COLOR_TUNE_FAILED = "#FF0000";
const COLOR_GOLD = "#FFD700";
const COLOR_LIGHT_GOLD = "#ffffcc";
var alllabels = [];

var markerPicker;
var markerCfg = [];
var userDefinedMarkers = [];
var windowHandle;
var gFirstLine;
var gPendingImport;
var gAllLogLines;
var gTuneStartLine;
var gAllIpExTuneTimes;
var gAllIpAampTuneTimes;
var gBoxDownload;
var gChunk;
var gBoxMarker;
var timestamp_min;
var timestamp_max;
var gLineNumber;
var gTimeStampUTC;
var gContentType;

function myclickhandler(e)
{
	if( gBoxDownload )
	{
		const rect = canvas.getBoundingClientRect()
		var x = e.clientX - rect.left + translateX;
		var y = e.clientY - rect.top;
		for (var i = 0; i < gBoxDownload.length; i++) {
			const box = gBoxDownload[i];
			if (x >= box.x && x < box.x + box.w &&
				y >= box.y && y < box.y + box.h)
			{
				// populate fragment data table
				document.getElementById("line").innerHTML = box.line;
				document.getElementById("response").innerHTML = box.error;
				document.getElementById("type").innerHTML = mediaTypes[box.type];
				document.getElementById("ulSz").innerHTML = box.ulSz + " bytes";
				document.getElementById("downloadTime").innerHTML = box.durationms + " ms";
				document.getElementById("dlSz").innerHTML = box.bytes + " bytes";
				document.getElementById("url").innerHTML = box.url;
				// display the fragment table on click
				document.getElementById("fragTable").style.display = "block";
                
				// pan within log view window
				scrollTo( box.line );
				return;
			}
		}
		for( var i=0; i<gBoxMarker.length; i++ )
		{
			if (x >= gBoxMarker[i].x && x < gBoxMarker[i].x + gBoxMarker[i].w &&
				y >= gBoxMarker[i].y && y < gBoxMarker[i].y + gBoxMarker[i].h )
			{
				if( e.ctrlKey || e.metaKey )
				{ // control/command click to pop up corresponding log line associated with marker
					alert( gAllLogLines[gBoxMarker[i].line] );
				}
				else
				{ // pan within log view window
					scrollTo( gBoxMarker[i].line );
				}
				return;
			}
		}
	}
}

function hoverDataVisualizer(event)
{
	if( gBoxDownload )
	{
		const rect = canvas.getBoundingClientRect()
		var x = event.clientX - rect.left + translateX;
		var y = event.clientY - rect.top;
		for (var i = 0; i < gBoxDownload.length; i++) {
			if (x >= gBoxDownload[i].x && x < gBoxDownload[i].x + gBoxDownload[i].w &&
				y >= gBoxDownload[i].y && y < gBoxDownload[i].y + gBoxDownload[i].h)
			{
				document.getElementById("fragmentContent").innerHTML =
					"Type: " + mediaTypes[gBoxDownload[i].type] + "<br>" +
					"Response: " + gBoxDownload[i].error + "<br>" +
					"Download time: " + gBoxDownload[i].durationms + "ms" + "<br>" +
					"Download Size: " + gBoxDownload[i].bytes + " bytes " + "<br>" +
					"URL: " + gBoxDownload[i].url;
				document.getElementById("fragmentModal").style.display = "block";
				// add padding to show fragment modal on top of  mouse pointer
				document.getElementById("fragmentModal").style.left = event.pageX+30;
				document.getElementById("fragmentModal").style.top = event.pageY-90;
				return;
			} else {
				document.getElementById("fragmentModal").style.display = "none";
			}
		}
	}
}

function MapMediaColor(mediaType)
{ // first color for box interior; second color for outline
	switch( mediaType )
	{
		case eMEDIATYPE_MANIFEST:
			return ['#00cccc','#006666']; // cyan;
		case eMEDIATYPE_PLAYLIST_VIDEO:
			return ['#00cc00','#006600']; // dark green
		case eMEDIATYPE_INIT_VIDEO:
			return ['#7fff7f','#3f7f3f']; // medium green
		case eMEDIATYPE_VIDEO:
			return ['#ccffcc','#667f66']; // light green
		case eMEDIATYPE_PLAYLIST_IFRAME:
			return ['#00cc00','#006600']; // dark green
		case eMEDIATYPE_INIT_IFRAME:
			return ['#7fff7f','#3f7f3f']; // medium green
		case eMEDIATYPE_IFRAME:
			return ['#ccffcc','#667f66']; // light green
		case eMEDIATYPE_PLAYLIST_AUDIO:
			return ['#0000cc','#000066']; // dark blue
		case eMEDIATYPE_INIT_AUDIO:
			return ['#7f7fff','#3f3f7f']; // medium blue
		case eMEDIATYPE_AUDIO:
			return ['#ccccff','#66667f']; // light blue
		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			return ['#cccc00','#666600']; // dark yellow
		case eMEDIATYPE_INIT_SUBTITLE:
			return ['#ffff7f','#7f7f3f']; // medium yellow
		case eMEDIATYPE_SUBTITLE:
			return ['#ffffcc','#7f7f66']; // light yellow
		case eMEDIATYPE_LICENSE:
			return ['#ff7fff','#7f3f7f']; // medium magenta
		case -eMEDIATYPE_LICENSE: // pre/post overhead
			return ['#ffccff','#7f667f'];
		default: // error
			return ['#ff2020','#7f3f3f'];
	}
}

function AdjustSize()
{
	canvas.width = window.innerWidth - 32;
}

function AdjustSizeAndRepaint()
{
	AdjustSize();
	paint();
}

function time2x(t) {
	return BITRATE_MARGIN + 32 + (t - timestamp_min) * 0.1;
}

function track2y(track)
{
	for( var i=0; i<alllabels.length; i++ )
	{
		if( alllabels[i] == track ) return i*ROWHEIGHT + 64;
	}
	return 0;
}

function GetTrackBottomY()
{
	return alllabels.length*ROWHEIGHT+64;
}

function AddBitrate( newBitrate )
{
	for( var i=0; i<alllabels.length; i++ )
	{
		if( alllabels[i]==newBitrate ) return;
	}
	//alert( newBitrate );
	alllabels.push( newBitrate );
}

function scrollTo( line )
{
	if( windowHandle )
	{
		line -= gFirstLine; // global to local
		//console.log( "sending scrollTo " + line );
		windowHandle.postMessage( { "command":"scrollTo", "line":line }, TARGET_ORIGIN );
	}
}

function fileUploadClick() {
	document.getElementById("fileUploadResponse").style.display='none';
	document.getElementById("tuneResponse").style.paddingTop="3%";
}

function paint()
{
	dx = -translateX;
	//console.log( "paint " + dx );
	
	ctx.clearRect(0, 0, canvas.width, canvas.height);
	ctx.font = "12px Arial";

	// timeline backdrop
	ctx.textAlign = "center";
	
	var shade = true;

	for (var t0 = timestamp_min; t0 < timestamp_max; t0 += 1000)
	{
		var x0 = dx+time2x(t0);
		if (shade)
		{ // light grey band for every other second
			var x1 = dx+time2x(t0 + 1000);
			ctx.fillStyle = '#f3f3f3';
			ctx.fillRect(x0, timeline_y0 - ROWHEIGHT/2, x1 - x0, timeline_y1 - timeline_y0);
			shade = false;
		}
		else
		{
			shade = true;
		}
		ctx.fillStyle = '#000000';
		ctx.fillText( FormatTime(t0-timestamp_min), x0, TOP_MARGIN-20 );
	}
	ctx.textAlign = "left";
	for( var pass=0; pass<2; pass++ )
	{
		var extent = [];
		for( var i=0; i<gBoxMarker.length; i++ )
		{
			var obj = gBoxMarker[i];
//			console.log(obj.label);
			if( pass==0 )
			{ // vertical lines
                if( obj.style )
                {
                    ctx.strokeStyle = obj.style;
                }
                else
                {
					ctx.strokeStyle = COLOR_GOLD;
                }
                ctx.beginPath();
				ctx.moveTo(obj.x+dx, timeline_y1 );
				ctx.lineTo(obj.x+dx, obj.y);
				ctx.stroke();
			}
			else
			{ // framed marker boxes
                if( obj.style )
                {
                    ctx.fillStyle = obj.style;
                }
                else
                {
					ctx.fillStyle = COLOR_LIGHT_GOLD;
                }
                ctx.strokeRect(obj.x+dx,obj.y,obj.w,obj.h);
                ctx.fillRect(obj.x+dx,obj.y,obj.w,obj.h);
				if( obj.style )
                {
                    ctx.fillStyle = '#ffffff';
                }
                else
                {
                    ctx.fillStyle = "#000000";
                }
				ctx.fillText(obj.label, obj.x+4+dx, obj.y+16 );
			}
		}
	}

	ctx.strokeStyle = '#dddddd';
	for (var i = 0; i < alllabels.length; i++) {
		var label = alllabels[i];
		var y = track2y(label);
		ctx.strokeRect(BITRATE_MARGIN + 2, y, canvas.width, 1);
	}
	
	for (var i = 0; i < gBoxDownload.length; i++) {
		ctx.fillStyle = gBoxDownload[i].fillStyle[0];
		ctx.fillRect(gBoxDownload[i].x+dx,
					 gBoxDownload[i].y,
					 gBoxDownload[i].w,
					 gBoxDownload[i].h );
		
		ctx.strokeStyle = gBoxDownload[i].fillStyle[1];
		ctx.strokeRect(gBoxDownload[i].x+dx,
					   gBoxDownload[i].y,
					   gBoxDownload[i].w,
					   gBoxDownload[i].h );
	}

	for (var i = 0; i < gChunk.length; i++) {
		// draw vertical dotted lines to show chunk injection
		ctx.beginPath();
		ctx.lineWidth = "2";
		ctx.strokeStyle = "#000000";
		ctx.setLineDash([3, 2]); /*dashes are 3px and spaces are 2px*/

		// find chunk position using bitrate
		ypos = track2y(gChunk[i].bitrate);
		var xpos = time2x(gChunk[i].injectTime);
		ctx.moveTo(xpos + dx , ypos - (ROWHEIGHT/2)); // starting point of line
		ctx.lineTo(xpos + dx , ypos + (ROWHEIGHT/2)); // end point of line
		ctx.stroke();
		ctx.closePath();
	}

	// Reset dotted line setting
	if (ctx.setLineDash) {
		ctx.setLineDash([]);
	}
	
	// draw y-axis legend
	ctx.textAlign = "right";
	ctx.fillStyle = "#ffffff";
	ctx.fillRect(0,0,BITRATE_MARGIN,GetTrackBottomY() );
	ctx.fillStyle = '#000000';
	
	for (var i = 0; i < alllabels.length; i++) {
		var label = alllabels[i];
		var y = track2y(label);
		ctx.fillText(label, BITRATE_MARGIN, y + ROWHEIGHT / 2 - 8);
	}
}

// function to check if the marker is user defined one
function isUserDefinedMarker(label) {
	for (var markerIndex = 0; markerIndex < userDefinedMarkers.length; ++markerIndex)
	{
		if(label === userDefinedMarkers[markerIndex].label) {
			//console.log(userDefinedMarkers + "is present");
			// user defined marker is present
			return true;
		}
	}
	return false;
}

window.onload = function() {
	gBoxDownload = [];
	gChunk = [];
	gBoxMarker = [];
	
	canvas = document.getElementById("myCanvas");
	canvas.height = 2048;
	ctx = canvas.getContext("2d");
	
    // show fragment details on canvas hover
	canvas.onmousemove = hoverDataVisualizer;

	// Display current auto triage Tool version
	document.getElementById("versionText").innerHTML = currentVersion;
	
	AdjustSize();
	
	draggger_Init(true);
	
	OpenLogWindow();
	
	function AdjustTimeline( gTimeStampUTC )
	{
		if (timestamp_min == null || gTimeStampUTC < timestamp_min) timestamp_min = gTimeStampUTC;
		if (timestamp_max == null || gTimeStampUTC > timestamp_max) timestamp_max = gTimeStampUTC;
	}
	
	function AddMarker( label, style )
	{
		var obj = {
			"timestamp":gTimeStampUTC,
			"line":gLineNumber,
			"label":label,
            "style":style
		};
		gBoxMarker.push(obj);
		
		AdjustTimeline( gTimeStampUTC );
		
		var option = document.createElement("option");
		option.text = "LineNo. " + Number(gLineNumber + 1) + ". " + label;
		// Show user defined markers in a distinctive color in drop down 
		if(isUserDefinedMarker(label)) {
			option.style.color = obj.style;
		}
		option.value = gLineNumber;
		markerPicker.add(option);
	}
	
	function myLoadHandler(e) {
		console.log( "myLoadHandler " + e.target.result.length + "bytes" );
		gAllLogLines.push(e.target.result);
		gPendingImport--;
		if( gPendingImport==0 )
		{
			// start busy cursor animation
			document.getElementById("loader").style.display = "block";
			
			// async, to allow busy cursor animation to begin
			setTimeout( IndexRawData, 1 );
		}
	}

	function myLoadUserDefMarkerHandler(e) {
		console.log( "myLoadUserDefMarkerHandler " + e.target.result.length + "bytes" );
		var userDefMarker = JSON.parse(e.target.result);
		for (var markerIndex = 0; markerIndex < userDefMarker.length; ++markerIndex)
		{
			var obj = {};
			obj.pattern = userDefMarker[markerIndex].pattern;
			obj.label = userDefMarker[markerIndex].label;
            obj.style = userDefMarker[markerIndex].style;
			markerCfg.push(obj);
			// push to user defined marker list also
			userDefinedMarkers.push(obj);
		}
	}
	
	function IndexRawData()
	{
		var locator = "";
		if( gAllLogLines.length>0 )
		{
			console.log( "indexing " + gAllLogLines.length + " log files" );
			
			// sort n log files using timestamp of first line
			gAllLogLines.sort( function(a,b){return ParseReceiverLogTimestamp(a)-ParseReceiverLogTimestamp(b);} );
			gAllLogLines = gAllLogLines.join("\n");
			gAllLogLines = gAllLogLines.split("\n");
			
			gTuneStartLine = [];
			var hasSkyTune = false;
			
			sessionClickedID = 0;
			sessionClicked = false;
			
			gAllIpExTuneTimes =
			["contentType,totalTime,networkTime,loadBucketTime,prepareToPlayBucketTime,playBucketTime,drmReadyBucketTime,decodedStreamingBucketTime,playingbackToXREBucketTime,firstManifestTIme,firstProfileTime,firstFragmentTime,firstLicenseTime,maniCount,manifestTotal,profCount,profilesTotal,fragCount,fragmentTotal,isLive,streamType,abrSwitch,isFOGEnabled,isDDPlus,isDemuxed,assetDuration,success,failRetryBucketTime,playbackCount,tuneRetries,tuneCompleteTime"
			 ];
			var prefix2 = "IP_EX_TUNETIME:";
			
			gAllIpAampTuneTimes = ["version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime,contentType,streamType,firstTune,Prebuffered,PreBufferedTime,durationSeconds,interfaceWifi,TuneAttempts,TuneSuccess,FailureReason,Appname,Numbers of TimedMetadata(Ads),StartTime to Report TimedEvent,Time taken to ReportTimedMetadata,TSBEnabled,TotalTime"];
			var prefix = "IP_AAMP_TUNETIME:";
            var epgTuneTime = [];
			for(var iLine=0; iLine<gAllLogLines.length; iLine++ )
			{
				var line = gAllLogLines[iLine];
				
				if( line.indexOf("EPG Request to play VIPER stream")>=0 ) // SKY
				{
					hasSkyTune = true;
					gTuneStartLine.push(iLine);
				}
                
                var epgTuneTimePrefix = "EPG Total tune time (ms) :  ";
                var epgTuneTimeDelim = line.indexOf(epgTuneTimePrefix);
                if( epgTuneTimeDelim>=0 )
                {
                    var epgTuneTimeDelimStart = epgTuneTimeDelim + epgTuneTimePrefix.length;
                    var epgTuneTimeDelimFin = line.indexOf(" ",epgTuneTimeDelimStart);
                    var tuneTimeMs = parseInt(line.substr(epgTuneTimeDelimStart,epgTuneTimeDelimFin-epgTuneTimeDelimStart));
                    
                    epgTuneTime[gTuneStartLine.length-1] = tuneTimeMs;
                }
                
				if( line.indexOf("aamp_tune:")>=0 )
				{
					if( !hasSkyTune )
					{ // fall back to plotting individual aamp tunes if no sky epg tune log preceeding
						gTuneStartLine.push(iLine);
					}
					var offs = line.indexOf("URL: ");
					if( offs>=0 )
					{
						locator = line.substr(offs+5);
						var uriParam = "&recordedUrl=";
						var fog = locator.indexOf(uriParam);
						if( fog>=0 )
						{
							locator = locator.substr(fog+uriParam.length);
							var fin = locator.indexOf("&");
							if( fin>=0 )
							{
								locator = locator.substr(0,fin);
							}
							locator = decodeURIComponent(locator);
						}
					}
				}
				else
				{
					var idx = line.indexOf(prefix);
					if( idx>=0 )
					{
						gAllIpAampTuneTimes[gTuneStartLine.length] = line.substr(idx+prefix.length)+","+locator;
					}

					idx = line.indexOf(prefix2);
					if( idx>=0 )
					{
						gAllIpExTuneTimes.push( line.substr(idx+prefix2.length)+","+locator );
					}
				}
			} // next line
			
			// reset pulldown state
			var currentSession = document.getElementById("session");
			while (currentSession.firstChild) {
				currentSession.removeChild(currentSession.firstChild);
			}
			// populate pulldown to pick tune session of interest

			if( gTuneStartLine.length==0 )
			 { // fragment
				var iLine = 0;
				var utc = ParseReceiverLogTimestamp( gAllLogLines[iLine] );
				var date = new Date(utc);
				var dateString = date.toLocaleTimeString();
				var option = document.createElement("option");
				option.text = dateString;
				option.value = 1;
				currentSession.add(option);
				gTuneStartLine[0] = iLine;
			}
			else
			for( var iter=0; iter<gTuneStartLine.length; iter++ )
			{
				var iLine = gTuneStartLine[iter];
				var utc = ParseReceiverLogTimestamp( gAllLogLines[iLine] );
				var date = new Date(utc);
				var dateString = date.toLocaleTimeString();
				var option = document.createElement("option");
				option.text = (iter+1) + ": " + dateString;
				if( iter<gAllIpAampTuneTimes.length )
				{
					var temp = gAllIpAampTuneTimes[iter+1];
					if( temp )
					{
						tempData = temp.split(",");
						var type = contentTypeString[parseInt(tempData[_contentType])];
						if( hasSkyTune )
						{
							if(epgTuneTime[iter]) {
								option.text += "("+epgTuneTime[iter]+")"; // SKY
							} else {
								option.text += "(fail)"; // Failed Tune
								option.classList.add("failed-tune");
							}
						}
						else
						{
							var tempSplit = temp.split(",");
							var tuneTime = tempSplit[_gstFirstFrameTime];
							if( tempSplit[_tuneSuccess] != 1 ) // _tuneSuccess = 44
							{
								tuneTime = "fail";
								option.classList.add("failed-tune");
							} 
							option.text += "(" + tuneTime +")"
						}

						option.text += " " + type;
					}
					option.value = (iter+1);
					currentSession.add(option);
				}
			}
			mySessionFilter();
		}
		document.getElementById("loader").style.display = "none";
	} // IndexRawData
	
	function MarkerToOptionText( string, param )
	{
		for(;;)
		{
			var offs = string.indexOf("%");
			if( offs<0 ) return string;
			var idx = parseInt(string.substr(offs+1,1));
			string = string.substr(0,offs) + param[idx] + string.substr(offs+2);
		}
	}
	
	function ProcessLine( line )
	{
		var color = undefined;

		gTimeStampUTC = ParseReceiverLogTimestamp(line);
		if(gTimeStampUTC)
		{
			for( var iCfg=0; iCfg<markerCfg.length; iCfg++ )
			{
				var entry = markerCfg[iCfg];
				var param = sscanf(line,entry.pattern);
				if( param )
				{
					gTimeStampUTC = ParseReceiverLogTimestamp(line);
					//console.log(MarkerToOptionText(entry.label,param));
                    color = entry.style;
					AddMarker( MarkerToOptionText(entry.label,param),color );
                    if( !color )
                    {
                        color = COLOR_LIGHT_GOLD;
                    }
					break;
				}
			}
			
			var httpRequestEnd = ParseHttpRequestEnd(line);
			if( httpRequestEnd )
			{
				var type = httpRequestEnd.type;
				var obj = {};
				obj.line = gLineNumber;
				obj.error = mapError(httpRequestEnd.responseCode);
				obj.durationms = 1000*httpRequestEnd.curlTime;
				obj.type = type;
				obj.ulSz = httpRequestEnd.ulSz;
				obj.bytes = httpRequestEnd.dlSz;
				obj.url = httpRequestEnd.url;
				var doneUtc = ParseReceiverLogTimestamp(line);
				obj.utcstart = doneUtc-obj.durationms;
				AdjustTimeline(obj.utcstart);
				AdjustTimeline(doneUtc);
				
				if( obj.type==eMEDIATYPE_LICENSE )
				{
					obj.track = "drm"; // AES-128
				}
				else if( obj.type==eMEDIATYPE_PLAYLIST_VIDEO )
				{
					obj.track = "vid-playlist";
				}
				else if( obj.type==eMEDIATYPE_VIDEO || obj.type == eMEDIATYPE_INIT_VIDEO )
				{
					obj.track = httpRequestEnd.br;
					if( !obj.track )
					{
						console.log( "unk video segment bitrate" );
						obj.track = 0;
					}
					AddBitrate( parseInt(obj.track) );
				}
				else if( obj.type == eMEDIATYPE_IFRAME || obj.type == eMEDIATYPE_INIT_IFRAME )
				{
					obj.track = "iframe";
				}
				else if( obj.type == eMEDIATYPE_AUDIO || obj.type == eMEDIATYPE_INIT_AUDIO )
				{
					obj.track = "audio";
				}
				else if( obj.type == eMEDIATYPE_SUBTITLE || obj.type == eMEDIATYPE_INIT_SUBTITLE )
				{
					obj.track = "subtitle";
				}
				else if( obj.type == eMEDIATYPE_MANIFEST )
				{
					obj.track = "manifest";
				}
				else if( obj.type == eMEDIATYPE_PLAYLIST_AUDIO )
				{
					obj.track = "aud-playlist";
				}
				else
				{
					console.log( "UNK TYPE!" );
				}
				// map UI color based on download type and success/failure
				if( obj.error != "HTTP200(OK)" && obj.error != "HTTP206" ) {
					obj.fillStyle = MapMediaColor(-1);
				}
				else
				{
					obj.fillStyle = MapMediaColor(type);
				}
				gBoxDownload.push(obj);
				color = obj.fillStyle[0];
			} // httpRequestEnd
			else {
				// parse chunk data
				var chunkDetails = ParseFragmentChunk(line);
				if( chunkDetails )
				{
					chunkDetails.injectTime = ParseReceiverLogTimestamp(line);
					gChunk.push(chunkDetails);
				}
			}
			
			var param=line.indexOf("IP_AAMP_TUNETIME:");
			if( param >= 0 )
			{
				var attrs = line.substr(param+17).split(",");
				// check tuneSucess parameter for checking if tune is success/failure
				if(attrs[_tuneSuccess] == '1') {
					AddMarker( "Tuned", COLOR_TUNED );
					color = COLOR_TUNED;

					var baseUTC = gTimeStampUTC - parseInt(attrs[_gstFirstFrameTime]);
					var startTime = baseUTC + parseInt(attrs[_drmLicenseAcqStartTime]);
					// use IP_AAMP_TUNETIME log to define license and pre/post drm overhead in timeline
					if( true )
					{
						var obj = {};
						obj.line = gLineNumber;
						obj.error = "HTTP200(OK)";
						obj.durationms = parseInt(attrs[_LicenseAcqPreProcessingDuration]);
						obj.type = eMEDIATYPE_LICENSE;
						obj.ulSz = 0;
						obj.bytes = 0;
						obj.url = "LicenseAcqPreProcessingDuration";
						obj.utcstart = startTime;
						obj.track = "drm";
						obj.fillStyle = MapMediaColor(-eMEDIATYPE_LICENSE);
						if( obj.durationms>0 )
						{
							gBoxDownload.push(obj);
							startTime += obj.durationms;
						}
					}
					if( true )
					{
						var obj = {};
						obj.line = gLineNumber;
						obj.error = "HTTP200(OK)";
						obj.durationms = parseInt(attrs[_LicenseAcqNetworkDuration]);
						obj.type = eMEDIATYPE_LICENSE;
						obj.ulSz = 0;
						obj.bytes = 0;
						obj.url = "LicenseAcqNetworkDuration";
						obj.utcstart = startTime;
						obj.track = "drm";
						obj.fillStyle = MapMediaColor(eMEDIATYPE_LICENSE);
						if( obj.durationms>0 )
						{
							gBoxDownload.push(obj);
							startTime += obj.durationms;
						}
					}
					if( true )
					{
						var obj = {};
						obj.line = gLineNumber;
						obj.error = "HTTP200(OK)";
						obj.durationms = parseInt(attrs[_LicenseAcqPostProcDuration]);
						obj.type = eMEDIATYPE_LICENSE;
						obj.ulSz = 0;
						obj.bytes = 0;
						obj.url = "LicenseAcqPostProcDuration";
						obj.utcstart = startTime;
						obj.track = "drm";
						obj.fillStyle = MapMediaColor(-eMEDIATYPE_LICENSE);
						if( obj.durationms>0 )
						{
							gBoxDownload.push(obj);
						}
					}
				} else {
					AddMarker( "Tune Failed", COLOR_TUNE_FAILED );
					color = COLOR_TUNE_FAILED;
				}
				gContentType = parseInt(attrs[_contentType]);
			} // IP_AAMP_TUNETIME
		}
		return color;
	} // ProcessLine
	
	function myMarkerFilter() {
		if (markerPicker.selectedIndex>=0)
		{ // todo: use line number or time offset directly
			var line = markerPicker.options[markerPicker.selectedIndex].value;
			dragTo( (ParseReceiverLogTimestamp( gAllLogLines[line] )  - timestamp_min)*0.1, translateY );
			paint();
		}
	}
	
	// process subset of logs for a given tune/session
	function mySessionFilter()
	{
		var currentSession = document.getElementById("session");
		console.log( "entering mySessionFilter " + currentSession.selectedIndex );
		if( currentSession.selectedIndex<0 )
		{
			return;
		}
		var sessionIndex = currentSession.options[currentSession.selectedIndex].value - 1;
		var title = currentSession.options[currentSession.selectedIndex].text;
		gFirstLine = gTuneStartLine[sessionIndex];
		
		translateX = 0;
		timestamp_min = null;
		timestamp_max = null;
		gBoxMarker = [];
		gBoxDownload = [];
		
		var indexedLog = [];
		var iFin;
		if( sessionIndex == gTuneStartLine.length-1 )
		{
			iFin = gAllLogLines.length;
		}
		else
		{
			iFin = gTuneStartLine[sessionIndex+1];
		}
		//var includeNonAAMPLogs = document.getElementById("filter-player").checked;
		
		if( windowHandle )
		{ // pass log subset to log window for display/browsing
			alllabels = ["drm","iframe","audio","subtitle","manifest","aud-playlist","vid-playlist"];
			var logGoo = [];
			for( gLineNumber = gFirstLine; gLineNumber<iFin; gLineNumber++ )
			{
				var line = gAllLogLines[gLineNumber];
				var color = ProcessLine( line );
				logGoo.push( [color,line] );
			}
			alllabels.sort( function(a,b){return b-a;} );
			windowHandle.postMessage( { "command":"initialize", "text":logGoo }, TARGET_ORIGIN );
		}
		
		document.getElementById("tuneMsg").innerHTML =
			contentTypeString[gContentType] + " (" + FormatTime(timestamp_max - timestamp_min) + "s)";
		document.getElementById("tuneResponse").style.display = "block";

		UpdateLayout();
		
		paint();
	}
	
	function UpdateLayout()
	{
		timeline_y0 = TOP_MARGIN;
		timeline_y1 = GetTrackBottomY();//TOP_MARGIN+alllabels.length*ROWHEIGHT;
		
		var extent = []; // right edge of markers so far for each row
		for( var i=0; i<gBoxMarker.length; i++ )
		{
			var obj = gBoxMarker[i];
			obj.x = time2x(obj.timestamp);
			obj.w = ctx.measureText(obj.label).width + 8;
			obj.h = 22;
			
			// find optimal vertical placement, avoiding marker overlap
			var row = 0;
			for(;;)
			{
				if( extent[row]==null || obj.x > extent[row]+8/*pad*/ )
				{
					extent[row] = obj.x + obj.w;
					obj.y = (row+1)*ROWHEIGHT + timeline_y1;
					break;
				}
				row++;
			}
		}
		
		for (var i = 0; i < gBoxDownload.length; i++) {
			var t0 = gBoxDownload[i].utcstart;
			var t1 = gBoxDownload[i].durationms + t0;
			gBoxDownload[i].x = time2x(t0)-1;
			gBoxDownload[i].w = time2x(t1) - gBoxDownload[i].x+2;
			gBoxDownload[i].y = track2y(gBoxDownload[i].track)-ROWHEIGHT/2+2;
			gBoxDownload[i].h = ROWHEIGHT-4;
		}
	}
	
	function OpenLogWindow()
	{
		console.log( "OpenLogWindow" );
		windowHandle = window.open("log-viewer.html?", "_blank", "width=640,height=480");
	}
	
	function handleFileSelect(evt) {
		console.log( "entering handleFileSelect\n" );
		gPendingImport = 0;
		gAllLogLines = [];
		var files = evt.target.files;

		// Reset tune response text to default height if closed before
		document.getElementById("tuneResponse").style.paddingTop="1%";

		try{
			for (var fileIndex = 0; fileIndex < files.length; fileIndex++) {
				var f = files[fileIndex];
				if (f.type = "text/plain")
				{
					var filename = f.name; // sky-messages.log.#, receiver.log.#
					console.log( "processing: " + filename );
					gPendingImport++;
					var reader = new FileReader();
					reader.onload = myLoadHandler;
					reader.readAsText(f);
				}
			}
			if(filename) {

				document.getElementById("fileUploadResponse").style.backgroundColor = "#08814f";
				document.getElementById("fileMsg").innerHTML = filename + " uploaded successfully.";
				document.getElementById("fileUploadResponse").style.display = "block";

			}
		} catch(err) {

			document.getElementById("fileUploadResponse").style.backgroundColor = "#e73e32";
			document.getElementById("fileMsg").innerHTML = filename + " upload failed. " + err.message;
			document.getElementById("fileUploadResponse").style.display = "block";

		}
	}

	function handleUserDefMarkerSelect(evt) {
		console.log( "entering handleUserDefMarkerSelect\n" );
		var files = evt.target.files;
		try{
			for (var fileIndex = 0; fileIndex < files.length; fileIndex++) {
				var f = files[fileIndex];
				if (f.type == "text/javascript")
				{
					var filename = f.name;
					console.log( "processing: " + filename );
					var reader = new FileReader();
					reader.onload = myLoadUserDefMarkerHandler;
					reader.readAsText(f);
				}
			}
			if(filename) {
				document.getElementById("fileUploadResponse").style.backgroundColor = "#08814f";
				document.getElementById("fileMsg").innerHTML = filename + " uploaded successfully.";
				document.getElementById("fileUploadResponse").style.display = "block";
			}
		} catch(err) {
			document.getElementById("fileUploadResponse").style.backgroundColor = "#e73e32";
			document.getElementById("fileMsg").innerHTML = filename + " upload failed. " + err.message;
			document.getElementById("fileUploadResponse").style.display = "block";
		}
	}
	
	// combine marker definitions
	var myFileArray = [
		marker_general,
		marker_aampgstplayer,
		marker_main,
		marker_priv_aamp,
		marker_hls,
		marker_dash,
		marker_tsprocessor,
        marker_sky ];

	for (var fileIndex = 0; fileIndex < myFileArray.length; ++fileIndex)
	{
		for (var markerIndex = 0; markerIndex < myFileArray[fileIndex].length; ++markerIndex)
		{
			var obj = {};
			obj.pattern = myFileArray[fileIndex][markerIndex].pattern;
            obj.label = myFileArray[fileIndex][markerIndex].label;
            obj.style = myFileArray[fileIndex][markerIndex].style;
            if( obj.style )
            {
            //    console.log( obj.label + "->" + obj.style );
            }
			markerCfg.push(obj);
		}
	}
	
	document.getElementById('files').addEventListener('change', handleFileSelect, false);
	document.getElementById('userdefmarkers').addEventListener('change', handleUserDefMarkerSelect, false);
	document.getElementById('session').addEventListener('change', mySessionFilter, false);
	markerPicker = document.getElementById("markers");
	markerPicker.addEventListener('change', myMarkerFilter, false);
	
	document.getElementById('exportButton').addEventListener('click', ExportHelper );
	
	window.addEventListener( "message", (event) => {
		// handle click from log viewer
		windowHandle = event.source;
		if( event.data.command == "scrollTo" )
		{
			var line = event.data.line;
			//console.log( "received scrollTo " + line );
			line += gFirstLine;
			dragTo( (ParseReceiverLogTimestamp( gAllLogLines[line] )  - timestamp_min)*0.1, translateY );
			paint();
		}
	}, false);
	
	paint();
}
