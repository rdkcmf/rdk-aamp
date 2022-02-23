// 127.0.0.1:8080/autotriage.html
const ROWHEIGHT = 24;
const BITRATE_MARGIN = 112;
const TOP_MARGIN = 64;
const COLOR_GOLD = "#FFD700";
const alllabels = ["MANIFEST","DRM","VIDEO","AUDIO","IFRAME","AUX_AUDIO","SUBTITLE"];

var markerPicker;
var markerCfg = [];
var userDefinedMarkers = [];
var windowHandle;
var gFirstLine;
var gPendingImport;
var gAllLogLines;
var gTuneStartLine;
var gAllIpAampTuneTimes;
var gBoxDownload;
var gBoxMarker;
var timestamp_min;
var timestamp_max;
var gLineNumber;
var gTimeStampUTC;

function myclickhandler(e)
{
	if( gBoxDownload )
	{
		const rect = canvas.getBoundingClientRect()
		var x = e.clientX - rect.left + translateX;
		var y = e.clientY - rect.top;
		for (var i = 0; i < gBoxDownload.length; i++) {
			if (x >= gBoxDownload[i].x && x < gBoxDownload[i].x + gBoxDownload[i].w &&
				y >= gBoxDownload[i].y && y < gBoxDownload[i].y + gBoxDownload[i].h)
			{
				var msg =
				"type:" + mediaTypes[gBoxDownload[i].type] + "; " +
				"response: " + gBoxDownload[i].error + "; " +
				"ulSz: " + gBoxDownload[i].ulSz + " bytes; " +
				"download time:" + gBoxDownload[i].durationms + "ms; " +
				"dlSz: " + gBoxDownload[i].bytes + " bytes; " +
				"URL: " + gBoxDownload[i].url;

				// populate fragment data table
				document.getElementById("line").innerHTML = gBoxDownload[i].line;
				document.getElementById("response").innerHTML = gBoxDownload[i].error;
				document.getElementById("type").innerHTML = mediaTypes[gBoxDownload[i].type];
				document.getElementById("ulSz").innerHTML = gBoxDownload[i].ulSz + " bytes";
				document.getElementById("downloadTime").innerHTML = gBoxDownload[i].durationms + " ms";
				document.getElementById("dlSz").innerHTML = gBoxDownload[i].bytes + " bytes";
				document.getElementById("url").innerHTML = gBoxDownload[i].url;
				// display the fragment table on click
				document.getElementById("fragTable").style.display = "block";

				if( e.ctrlKey || e.metaKey )
				{ // control/command click to pop up details for this download activity indicator
					// note: with newlines included, alert text isn't selectable in chrome
					alert( msg );
				}
				else
				{ // pan within log view window
					scrollTo( gBoxDownload[i].line );
				}
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
					"Type:" + mediaTypes[gBoxDownload[i].type] + "<br>" +
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
	return TOP_MARGIN+track*ROWHEIGHT;
}

function scrollTo( line )
{
	if( windowHandle )
	{
		line -= gFirstLine; // global to local
		console.log( "sending scrollTo " + line );
		windowHandle.postMessage( { "command":"scrollTo", "line":line }, TARGET_ORIGIN );
	}
}

function paint()
{
	dx = -translateX;
	console.log( "paint " + dx );
	
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

			console.log(obj.label);
			
			if( pass==0 )
			{ // vertical lines
				
				if(String(obj.label) === "Tuned") {
					// Show tuned line in green color
					ctx.strokeStyle = "#058840";
				} else if(String(obj.label) === "Notify-Bitrate-Change") {
					// Show bitrate changed line in orange color
					ctx.strokeStyle = "#d35811";		
				} else if(String(obj.label) === "Crashed!!!") {
					// Show bitrate changed line in orange color
					ctx.strokeStyle = "#00ff00";		
				} else if(String(obj.label) === "WebProcess unresponsive") {
					// Show bitrate changed line in orange color
					ctx.strokeStyle = "#ff3d3d";		
				} else if(String(obj.label) === "High load average") {
					// Show bitrate changed line in orange color
					ctx.strokeStyle = "#f59f00";		
				} else if(isUserDefinedMarker(obj.label)) {
					// If user defined marker show in distinctive color
					ctx.strokeStyle = "#b768eb";	
				} else {
					ctx.strokeStyle = COLOR_GOLD;
				}
				ctx.beginPath();
				ctx.moveTo(obj.x+dx, timeline_y0-20);
				ctx.lineTo(obj.x+dx, obj.y);
				ctx.stroke();
			}
			else
			{ // framed marker boxes
				ctx.strokeStyle = COLOR_GOLD;
				ctx.strokeRect(obj.x+dx,obj.y,obj.w,obj.h);

				if(String(obj.label) === "Tuned") {
					// Show tuned in green color
					ctx.fillStyle = "#058840";
				} else if(String(obj.label) === "Notify-Bitrate-Change") {
					// Show bitrate changed in orange color
					ctx.fillStyle = "#d35811";		
				} else if(String(obj.label) === "Crashed!!!") {
					// Show bitrate changed line in orange color
					ctx.fillStyle = "#ff0000";		
				} else if(String(obj.label) === "WebProcess unresponsive") {
					// Show bitrate changed line in orange color
					ctx.fillStyle = "#ff3d3d";		
				} else if(String(obj.label) === "High load average") {
					// Show bitrate changed line in orange color
					ctx.fillStyle = "#f59f00";		
				} else if(isUserDefinedMarker(obj.label)) {
					// If user defined marker fill in distinctive color
					ctx.fillStyle = "#6d4194";	
				} else {
					ctx.fillStyle = '#ffffe0';
				}

				ctx.fillRect(obj.x+dx,obj.y,obj.w,obj.h);
				
				if((String(obj.label) !== "Tuned")
						&& (String(obj.label) !== "Notify-Bitrate-Change")
						&& (String(obj.label) !== "Crashed!!!")
						&& (String(obj.label) !== "WebProcess unresponsive")
						&& (String(obj.label) !== "High load average")
						&& !(isUserDefinedMarker(obj.label))) {
					ctx.fillStyle = "#000000";	
				} else {
					ctx.fillStyle = '#ffffff';
				}
				
				ctx.fillText(obj.label, obj.x+4+dx, obj.y+16 );
			}
		}
	}

	ctx.strokeStyle = '#dddddd';
	for (var i = 0; i < alllabels.length; i++) {
		var label = alllabels[i];
		var y = track2y(i);
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
	
	// draw y-axis legend
	ctx.textAlign = "right";
	ctx.fillStyle = "#ffffff";
	ctx.fillRect(0,0,BITRATE_MARGIN,track2y(alllabels.length) );
	ctx.fillStyle = '#000000';
	for (var i = 0; i < alllabels.length; i++) {
		var label = alllabels[i];
		var y = track2y(i);
		ctx.fillText(label, BITRATE_MARGIN, y + ROWHEIGHT / 2 - 8);
	}
}

// function to check if the marker is user defined one
function isUserDefinedMarker(label) {
	for (var markerIndex = 0; markerIndex < userDefinedMarkers.length; ++markerIndex)
	{
		if(label === userDefinedMarkers[markerIndex].label) {
			console.log(userDefinedMarkers + "is present");
			// user defined marker is present
			return true;
		}
	}
	return false;
}

window.onload = function() {
	gBoxDownload = [];
	gBoxMarker = [];
	
	canvas = document.getElementById("myCanvas");
	canvas.height = alllabels.length*ROWHEIGHT + 480 + 256;
	ctx = canvas.getContext("2d");
    // Show fragment details on canvas hover
	canvas.onmousemove = hoverDataVisualizer;
	
	AdjustSize();
	
	draggger_Init();
	
	OpenLogWindow();
	
	function AdjustTimeline( gTimeStampUTC )
	{
		if (timestamp_min == null || gTimeStampUTC < timestamp_min) timestamp_min = gTimeStampUTC;
		if (timestamp_max == null || gTimeStampUTC > timestamp_max) timestamp_max = gTimeStampUTC;
	}
	
	function AddMarker( label )
	{
		var obj = {
			"timestamp":gTimeStampUTC,
			"line":gLineNumber,
			"label":label
		};
		gBoxMarker.push(obj);
		
		AdjustTimeline( gTimeStampUTC );
		
		var option = document.createElement("option");
		option.text = "LineNo. " + Number(gLineNumber + 1) + ". " + label;
		// Show user defined markers in a distinctive color in drop down 
		if(isUserDefinedMarker(label)) {
			option.style.color = "#b768eb";
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
			IndexRawData();
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
			
			sessionClickedID = 0;
			sessionClicked = false;
			
			//var tStopBegin = null;
			gAllIpAampTuneTimes = ["version,build,tuneStartBaseUTCMS,ManifestDLStartTime,ManifestDLTotalTime,ManifestDLFailCount,VideoPlaylistDLStartTime,VideoPlaylistDLTotalTime,VideoPlaylistDLFailCount,AudioPlaylistDLStartTime,AudioPlaylistDLTotalTime,AudioPlaylistDLFailCount,VideoInitDLStartTime,VideoInitDLTotalTime,VideoInitDLFailCount,AudioInitDLStartTime,AudioInitDLTotalTime,AudioInitDLFailCount,VideoFragmentDLStartTime,VideoFragmentDLTotalTime,VideoFragmentDLFailCount,VideoBitRate,AudioFragmentDLStartTime,AudioFragmentDLTotalTime,AudioFragmentDLFailCount,AudioBitRate,drmLicenseAcqStartTime,drmLicenseAcqTotalTime,drmFailErrorCode,LicenseAcqPreProcessingDuration,LicenseAcqNetworkDuration,LicenseAcqPostProcDuration,VideoFragmentDecryptDuration,AudioFragmentDecryptDuration,gstPlayStartTime,gstFirstFrameTime,contentType,streamType,firstTune,Prebuffered,PreBufferedTime,durationSeconds,interfaceWifi,TuneAttempts,TuneSuccess,FailureReason,Appname,Numbers of TimedMetadata(Ads),StartTime to Report TimedEvent,Time taken to ReportTimedMetadata,TSBEnabled"];
			var prefix = "IP_AAMP_TUNETIME:";
			for(var iLine=0; iLine<gAllLogLines.length; iLine++ )
			{
				var line = gAllLogLines[iLine];
				/*
				// following code snippet measured stop overhead
				 var tm = ParseReceiverLogTimestamp(line);
				if( line.indexOf("aamp_stop PlayerState")>=0)
				{
					tStopBegin = tm;
				}
				else if( line.indexOf("exiting AAMPGstPlayer_Stop")>=0 )
				{
					if( tStopBegin )
					{
						var dt = tm - tStopBegin;
						console.log( "stop time = " + dt );
						tStopBegin = null;
					}
				}
				 else
				*/
				if( line.indexOf("aamp_tune:")>=0 )
				{
					gTuneStartLine.push(iLine);
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
				}
			} // next line
			
			// reset pulldown state
			var currentSession = document.getElementById("session");
			while (currentSession.firstChild) {
				currentSession.removeChild(currentSession.firstChild);
			}
			// populate pulldown to pick tune session of interest
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
						option.text += "(" + temp.split(",")[_gstFirstFrameTime]+"ms)"
					}
					option.value = (iter+1);
					currentSession.add(option);
				}
			}
			
			mySessionFilter();
		}
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

					console.log("QQQQQQQQQQQQQQQQQQQ");
					console.log(MarkerToOptionText(entry.label,param));
					AddMarker( MarkerToOptionText(entry.label,param) );

					if(String(entry.label) === "Tuned") {
						// Show tuned in green color
						color = "#058840";
					} else if(String(entry.label) === "Notify-Bitrate-Change") {
						// Show bitrate changed in orange color
						color = "#d35811";		
					} else if(String(entry.label) === "Crashed!!!") {
						// Show bitrate changed line in orange color
						color = "#ff0000";		
					} else if(String(entry.label) === "WebProcess unresponsive") {
						// Show bitrate changed line in orange color
						color = "#ff3d3d";		
					} else if(String(entry.label) === "High load average") {
						// Show bitrate changed line in orange color
						color = "#f59f00";		
					} else if(isUserDefinedMarker(entry.label)) {
						// If user defined marker fill in distinctive color
						color = "#6d4194";	
					} else {
						color= COLOR_GOLD;
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
				obj.ulSz = httpRequestEnd.ulSz;//.times.ulSz;
				obj.bytes = httpRequestEnd.dlSz;//.times.dlSz;
				obj.url = httpRequestEnd.url;
				var doneUtc = ParseReceiverLogTimestamp(line);
				obj.utcstart = doneUtc-obj.durationms;
				AdjustTimeline(obj.utcstart);
				AdjustTimeline(doneUtc);
				
				{
					var label = mediaTypes[type];
					var offs = label.indexOf("_");
					if( offs>0 )
					{
						label = label.substr(offs+1);
					}
					obj.track = alllabels.indexOf(label);
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
			}
			
			var param=line.indexOf("IP_AAMP_TUNETIME:");
			if( param >= 0 )
			{
				AddMarker( "Tuned" );
				
				var attrs = line.substr(param+17).split(",");
				var baseUTC = gTimeStampUTC - parseInt(attrs[_gstFirstFrameTime]);
				var startTime = baseUTC + parseInt(attrs[_drmLicenseAcqStartTime]);
				
				// use IP_AAMP_TUNETIME log to define license and pre/post drm overhead in timeline
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
					obj.track = 1;//"DRM";
					obj.fillStyle = MapMediaColor(-eMEDIATYPE_LICENSE);
					gBoxDownload.push(obj);
					startTime += obj.durationms;
				}
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
					obj.track = 1;//"DRM";
					obj.fillStyle = MapMediaColor(eMEDIATYPE_LICENSE);
					gBoxDownload.push(obj);
					startTime += obj.durationms;
					color = obj.fillStyle[0];
				}
				
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
					obj.track = 1;//"DRM";
					obj.fillStyle = MapMediaColor(-eMEDIATYPE_LICENSE);
					gBoxDownload.push(obj);
				}
			} // IP_AAMP_TUNETIME
		}
		return color;
	} // ProcessLine
	
	/*
	function isAampLog( line )
	{
		if( line.indexOf("[AAMP-PLAYER]")>=0 ) return true;
		if( line.indexOf("[ABRManager]")>=0 ) return true;
		return false;
	}
	 */
	
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
			var logGoo = [];
			for( gLineNumber = gFirstLine; gLineNumber<iFin; gLineNumber++ )
			{
				var line = gAllLogLines[gLineNumber];
				//if( includeNonAAMPLogs || isAampLog(line) )
				{
					var color = ProcessLine( line );
					logGoo.push( [color,line] );
				}
			}
			windowHandle.postMessage( { "command":"initialize", "text":logGoo }, TARGET_ORIGIN );
		}
		
		// update onscreen duration estimate for IP Video Session
		document.getElementById("durationID").innerHTML = FormatTime(timestamp_max - timestamp_min) + " Seconds";
		
		UpdateLayout();
		
		paint();
	}
	
	function UpdateLayout()
	{
		timeline_y0 = TOP_MARGIN;
		timeline_y1 = TOP_MARGIN+alllabels.length*ROWHEIGHT;
		
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
				if (f.type == "application/json")
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
		marker_tsprocessor];

	for (var fileIndex = 0; fileIndex < myFileArray.length; ++fileIndex)
	{
		for (var markerIndex = 0; markerIndex < myFileArray[fileIndex].length; ++markerIndex)
		{
			var obj = {};
			obj.pattern = myFileArray[fileIndex][markerIndex].pattern;
			obj.label = myFileArray[fileIndex][markerIndex].label;
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
			console.log( "received scrollTo " + line );
			line += gFirstLine;
			dragTo( (ParseReceiverLogTimestamp( gAllLogLines[line] )  - timestamp_min)*0.1, translateY );
			paint();
		}
	}, false);
	
	paint();
}
