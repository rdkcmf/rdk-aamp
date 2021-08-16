window.onload = function() {
	var MEDIA_TIMELINE_WIDTH = 2000;
    var ROWHEIGHT = 24;
    var BITRATE_MARGIN = 112;
    var data;
    var sessionData = [];
	var sessionFirstLine = [];
    var sessionClicked = false;
    var sessionClickedID = 0;
	var topMargin = 24;
    var timeOffset;
	var timestamp_min;
	var timestamp_max;
	var bitrate_max;
	var allbitrates;
	var currentBitrate;
	var marker;
	var timestamp;
	var raw = [];
	var canvas = document.getElementById("myCanvas");
	canvas.onclick = myclickhandler;

	function myclickhandler(event)
	{
		if( data )
		{ // clicking on bar pops up details
			var x = event.pageX;
			var y = event.pageY - ROWHEIGHT;
			for (var i = 0; i < data.length; i++) {
				if (x >= data[i].x && x < data[i].x + data[i].w &&
					y >= data[i].y && y < data[i].y + ROWHEIGHT) {
					alert("line:"+data[i].line + "\nresponse:" + data[i].error +
						  "\ntype:" + mediaTypes[data[i].type] +
						  "\nulSz: " + data[i].ulSz +
						  " bytes\ndownload time:" + data[i].durationms +
						  "ms\ndlSz:" + data[i].bytes +
						  " bytes\nURL: " + data[i].url);
					// todo: text in alerts can't be copy-pasted
					return;
				}
			}
			if( y<bitrate2y(0) )
			{ // for other clicks, show the GMT time, suitable to look up in original rdk log
				var utc = x2time(x);
				var date = new Date(utc);
				var rdkdate = date.getUTCFullYear() + " " + months[date.getUTCMonth()] + " " + Pad2(date.getUTCDate()) + " " + Pad2(date.getUTCHours())+":"+Pad2(date.getUTCMinutes())+":"+Pad2(date.getUTCSeconds())+"."+Pad3(utc%1000);
				alert( "GMT: " + rdkdate + "\n" + "Local: " + date.toLocaleTimeString() );
			}
		}
	}

	function x2time( x )
	{
 		return (x - BITRATE_MARGIN - 32)*10.0 + timeOffset + timestamp_min;
	}

    function time2x(t) {
        return BITRATE_MARGIN + 32 + (t - timestamp_min - timeOffset) * 0.1;
    }

    function bitrate2y(bitrate) {
        for( var i=0; i<allbitrates.length; i++ )
        {
            if( allbitrates[i] == bitrate ) return i*ROWHEIGHT*2 + 40+topMargin;
        }
        return 40+topMargin;
    }
                         
    function AddBitrate( newBitrate )
    {
        currentBitrate = newBitrate;
        for( var i=0; i<allbitrates.length; i++ )
        {
            if( allbitrates[i]==currentBitrate ) return;
        }
        allbitrates.push( currentBitrate );
    }
	
	function AdjustTimeline( timestamp )
	{
		if (timestamp_min == null || timestamp < timestamp_min) timestamp_min = timestamp;
		if (timestamp_max == null || timestamp > timestamp_max) timestamp_max = timestamp;
	}
						 
    function AddMarker( label, mediatime, type )
    {
		marker.push( [timestamp, label, mediatime, type] );
		AdjustTimeline( timestamp );
    }
						 
	function Fixed( value )
	{
		var f = parseFloat(value);
		return Math.round(f*10)/10.0;
	}
	
	function myLoadHandler(e) {
		console.log( "myLoadHandler " + e.target.result.length + "bytes" );
		raw.push(e.target.result);
	}

	function IndexRawData()
	{
		if( raw.length>0 )
		{
			console.log( "indexing " + raw.length + " log files" );

			// combine multiple logs, orderd by timestamp
			raw.sort( function(a,b){return ParseReceiverLogTimestamp(b)-ParseReceiverLogTimestamp(a);} );
			raw = raw.join("\n");
			
			console.log( "size = " + raw.length + " bytes" );
			sessionData = [];
			sessionFirstLine = [];
			sessionClickedID = 0;
			sessionClicked = false;
		
			// cut out single sessions
			var sessions = raw.split("aamp_tune:");
			raw = [];
			console.log( "sessions: " + sessions.length );
			
			// reset pulldown state
			var currentSession = document.getElementById("session");
			while (currentSession.firstChild) {
				currentSession.removeChild(currentSession.firstChild);
			}
			
			var totalLines = 0;
			for (iter = 0; iter < sessions.length; iter++) {
				var sessionDataItem = sessions[iter].split("\n");
				var numLines = sessionDataItem.length;
				totalLines += numLines;
				sessionFirstLine.push( totalLines-iter ); // compensate for double split
				
				if( iter>0 )
				{
					var utc = ParseReceiverLogTimestamp(sessionDataItem[1]);
					var date = new Date(utc);
					var dateString = date.toLocaleTimeString();
					var option = document.createElement("option");
					option.text = iter + ": " + dateString;
					option.value = iter;

					sessionData.push(sessionDataItem);
					currentSession.add(option);
				}
			}
			
			newSession(); // new
		}
		else
		{
			alert( "load files before clicking Index button" );
		}
	}
	
	function mySessionFilter(s) {
		console.log( "entering mySessionFilter" );
        timeOffset = 0;
        var iframe_seqstart = null;
        var av_seqstart = null;
        var bandwidth_samples = [];

        // initialize state
        timestamp_min = null;
        timestamp_max = null;
        bitrate_max = null;
        allbitrates = [0];
        marker = [];
		mediaTimeline = [[],[]];
		var sessionIndex = sessionClickedID - 1;
		var samples = sessionData[sessionIndex];

        data = [];
        currentBitrate = 1;
		var isVOD = false;

        var aampLogPrefix = "[AAMP-PLAYER]";
        var abrLogPrefix = "[ABRManager] ";

        for (var lineNumber = 0; lineNumber < samples.length; lineNumber++) {
            var line = samples[lineNumber];
            var payload;
            var logOffset;
            var param;
            var httpRequestEnd;
            logOffset = line.indexOf( aampLogPrefix );
            if( logOffset>=0 )
            {
                payload = line.substr(logOffset+aampLogPrefix.length);
            }
            else
            {
                logOffset = line.indexOf( abrLogPrefix );
                if( logOffset>=0 )
                {
                    payload = line.substr(logOffset+abrLogPrefix.length);
                }
                else
                {
                    continue;
                }
            }
            
            timestamp = ParseReceiverLogTimestamp(line);
						 
			if( param=sscanf(payload,"aamp pos: [%%..%%..%%..-1]") )
			{
				var startpos = parseInt(param[0]);
				var position = parseInt(param[1]);
				var endpos = parseInt(param[2]);
				//var label = "("+(position-startpos)+")"+position+"("+(endpos-position)+")";
				//var label = position + "("+(endpos-startpos)+")";
				var label = position; // for now, just show progress - above variations can be useful to show manifest growth/culling
				AddMarker( label, position, -1 );
			}
			else if( param=sscanf(payload,"aamp_stop PlayerState=%%") )
			{
				var state = parseInt(param[0]);
				AddMarker( "aamp_stop("+playerStates[state]+")" );
			}
			if( param = sscanf(payload,"GetNextFragmentUriFromPlaylist:%% #EXT-X-DISCONTINUITY in track[%%] playTarget %% total mCulledSeconds %%") )
			{ // HLS-specific
				var position = parseFloat(param[2]);
				var culled = parseFloat(param[3]);
				var trackIndex = parseInt(param[1]);
				var track = trackNames[trackIndex];
				var label = track + " discontinuity@"+Fixed(position)+"("+Fixed(culled)+")";
				if( track=="video" )
				{
					AddMarker( label, position, 0 );
				}
				else
				{
					AddMarker( label, position, 1 );
				}
			}
			else if( param = sscanf(payload,"AAMPLogNetworkError error='%% error %%' type='%%' location='%%' symptom='%%' url='%%'") )
			{
				AddMarker( mapError(param[1]+"-"+param[2]), undefined, "exception" );
			}
            else if( param = sscanf(payload,"aamp_Seek(%%) and seekToLive(%%)" ) )
            {
				var pos = parseFloat(param[0]);
                AddMarker( "seek"+ pos.toFixed(2)+"s", pos );
            }
            else if( param = sscanf(payload,"PrivateInstanceAAMP::ScheduleRetune:%%: numPtsErrors %%, ptsErrorThreshold %%") )
            {
                AddMarker( "retune" );
            }
			else if( param = sscanf(payload,"Received getfile Bitrate : %%") )
			{ // aamp+fog
				AddBitrate(parseInt(param[0]));
			}
            else if( param = sscanf(payload,"getInitialProfileIndex:%% Get initial profile index = %%, bitrate = %% and defaultBitrate = %%") )
            { // aamp-only
                AddBitrate(parseInt(param[2]));
                AddMarker( "initial"+":"+Math.round(currentBitrate/1000)+"kbps" );
            }
			else if( payload.startsWith("## AAMPGstPlayer_OnGstBufferUnderflowCb") )
			{ // note: could be audio underflow, too
				AddMarker( "Video UnderFlow",undefined,"exception" );
			}
			else if( param=sscanf(payload,"[AAMP_JS] SendEventSync(type=%%)(state=%%)") )
			{
				var eName = eventNames[param[0]];
				if( eName == "statusChanged" )
				{ // for statusChanged, expand state as name
					AddMarker( playerStates[parseInt(param[1])] );
				}
				else
				{
					AddMarker( eName );
				}
			}
			else if( param=sscanf(payload,"BitrateChanged:%%)") )
			{
				AddMarker( bitrateChangedReason[param[0]] );
			}
			else if( param=sscanf(payload,"[AAMP_JS] SendEventSync(type=%%)") )
			{
				var eName = eventNames[param[0]];
				if( eName != "bitrateChanged" )
				{ // redundant and less expressive than BitrateChanged logging
					AddMarker( eName );
				}
			}
            else if( httpRequestEnd = ParseHttpRequestEnd(line) )
            {
                var type = httpRequestEnd.type;
                var obj = {};
				obj.line = lineNumber+sessionFirstLine[sessionIndex];
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
                if( type==eMEDIATYPE_PLAYLIST_VIDEO || type==eMEDIATYPE_VIDEO || type==eMEDIATYPE_INIT_VIDEO )
                {
                    obj.bitrate = currentBitrate;
                }
                else
                {
                    obj.bitrate = 0;
                }
                data.push(obj);
            }
            else if( line.indexOf("GST_MESSAGE_EOS")>=0 )
            {
                AddMarker( "GST_EOS" );
            }
			else if( line.indexOf("EXT-X-PLAYLIST-TYPE - VOD")>=0 )
			{
				isVOD = true;
			}
            else if( line.indexOf("IP_AAMP_TUNETIME")>=0 )
            {
                AddMarker( "Tuned" );
            }
            else if( param = sscanf(payload,"AAMPLogABRInfo : switching to '%%' profile '%% -> %%' currentBandwidth[%%]->desiredBandwidth[%%]") )
            {
                var reason = param[0];
                AddBitrate(param[4]);
                AddMarker( reason+":"+Math.round(currentBitrate/1000)+"kbps", undefined, "exception" );
            }
        } // next line
        allbitrates.sort( function(a,b){return b-a;} );
        Plot();
}

function media2x( m )
{
	var dur = (timestamp_max - timestamp_min)/1000;
	return MEDIA_TIMELINE_WIDTH*m/dur;
}

function DrawMediaTimeline()
{
		var canvas = document.getElementById("myCanvas");
		var ctx = canvas.getContext("2d");
		var y = [canvas.height-64, canvas.height-32];

		for( var track=0; track<2; track++ )
		{
			ctx.fillStyle = '#f7f7f7';
			ctx.fillRect(0, y[track], MEDIA_TIMELINE_WIDTH, 30 );
		}
												 
		for( var i=0; i<marker.length; i++ )
		{
			var mediatime = marker[i][2];
			var type = marker[i][3];
			if( type==0 )
			{ // video
				ctx.fillStyle = '#00cc00';
				ctx.fillRect( media2x(mediatime),y[type],1,30 );
			}
			else if( type==1 )
			{ // audio
				ctx.fillStyle = '#0000cc';
				ctx.fillRect( media2x(mediatime),y[type],1,30 );
			}
		}
}
												 
function Plot()
{
        var canvas = document.getElementById("myCanvas");
        canvas.width = 32767;//Math.min(10000, time2x(timestamp_max) + 16); // cap max width to avoid canvas limitation
        canvas.height = allbitrates.length*ROWHEIGHT*2 + 480 + 256;

        var ctx = canvas.getContext("2d");
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.font = "18px Arial";

        // timeline backdrop
        ctx.textAlign = "center";
        var timeline_y0 = bitrate2y(0)+ROWHEIGHT;
        var timeline_y1 = bitrate2y(bitrate_max)-ROWHEIGHT;
        var shade = true;
        										 
        for (var t0 = timestamp_min; t0 < timestamp_max; t0 += 1000) {
            var x0 = time2x(t0);
            var x1 = time2x(t0 + 1000);
            if (shade) { // light grey band for every other second
                ctx.fillStyle = '#f3f3f3';
                ctx.fillRect(x0, timeline_y0, x1 - x0, timeline_y1 - timeline_y0);
            }
            shade = !shade;

            ctx.strokeStyle = '#dddddd';
            ctx.strokeRect(x0, topMargin, 1, timeline_y1-topMargin);

            ctx.fillStyle = '#000000';
			ctx.fillText( FormatTime(t0-timestamp_min), x0, topMargin );
        }
		document.getElementById("durationID").innerHTML = "Max Duration: " + FormatTime(timestamp_max - timestamp_min);
        ctx.textAlign = "center";
		var extent = null;
        var row = 0;
		for( var i=0; i<marker.length; i++ )
        {
			var tm = marker[i][0];
			var x = time2x(tm);
			var label = marker[i][1];
			var mediatime = marker[i][2];
			var type = marker[i][3];

			var mt = ctx.measureText(label).width/2;
			if( extent==null || x-mt > extent )
			{
			   row = 0;
			}
			else
			{
			   row++;
			}
			extent = x+mt;
												 
            var y = row*24 + topMargin+timeline_y0+64;

			if( type==0 )
			{ // video
				ctx.strokeStyle = '#00cc00';
				ctx.beginPath();
				ctx.moveTo(media2x(mediatime), canvas.height-64);
				ctx.lineTo(x, y);
				ctx.stroke();
			}
			else if( type==1 )
			{ // audio
				ctx.strokeStyle = '#0000cc';
				ctx.beginPath();
				ctx.moveTo(media2x(mediatime), canvas.height-32);
				ctx.lineTo(x, y);
				ctx.stroke();
			}
			else if( type==-1 && timeOffset == tm-timestamp_min )
			{
				ctx.strokeStyle = '#000000';
				ctx.beginPath();
				ctx.moveTo(media2x(mediatime), canvas.height-64);
				ctx.lineTo(x, y);
				ctx.stroke();
			}
			
			{
				ctx.strokeStyle = '#cc0000';
				ctx.beginPath();
				ctx.moveTo(x, topMargin);
				ctx.lineTo(x, y);
				ctx.stroke();
			}
			
            ctx.fillStyle = '#000000';
            ctx.fillText(label, x, y+16 );
        }


        ctx.strokeStyle = '#dddddd';
        for (var i = 0; i < allbitrates.length; i++) {
                var bitrate = allbitrates[i];
                var y = bitrate2y(bitrate);
                ctx.strokeRect(BITRATE_MARGIN + 2, y, canvas.width, 1);
        }
        ctx.textAlign = "left";

        // compute default positions of bars
        for (var i = 0; i < data.length; i++) {
            var t0 = data[i].utcstart;
            var t1 = data[i].durationms + t0;
            data[i].x = time2x(t0);
            data[i].w = time2x(t1) - data[i].x;
            var bitrate = data[i].bitrate;
            data[i].y = bitrate2y(bitrate);
        }

        // adjust bar placement to avoid overlap w/ parallel downloads
        for (;;) {
            var bump = false;
            var pad = 0; // +16 used to include labels poking out past bars as consideration for overlap
            for (var i = 0; i < data.length; i++) {
                for (var j = i + 1; j < data.length; j++) {
                    if (
                        data[i].x + data[i].w + pad > data[j].x &&
                        data[j].x + data[j].w + pad > data[i].x &&
                        data[i].y + ROWHEIGHT > data[j].y &&
                        data[j].y + ROWHEIGHT > data[i].y) {
                        data[j].y += ROWHEIGHT;
                        bump = true;
                    }
                }
            }
            if (!bump) {
                break;
            }
        }

        for (var i = 0; i < data.length; i++) {
            // map colors based on download type and success/failure
            if (data[i].error != "HTTP200(OK)" ) {
                ctx.fillStyle = '#ff0000';
            }
            else
            {
                switch( data[i].type )
                {
                case eMEDIATYPE_INIT_VIDEO:
                case eMEDIATYPE_VIDEO:
                    ctx.fillStyle = '#ccffcc'; // light green
                    break;
                         
                case eMEDIATYPE_AUDIO:
                case eMEDIATYPE_INIT_AUDIO:
                    ctx.fillStyle = '#ccccff'; // light blue
                    break;

                case eMEDIATYPE_MANIFEST:
                case eMEDIATYPE_PLAYLIST_VIDEO:
                    ctx.fillStyle = '#00cc00'; // dark green
                    break;
                         
                case eMEDIATYPE_PLAYLIST_AUDIO:
                    ctx.fillStyle = '#0000cc'; // dark blue
                    break;

                case eMEDIATYPE_PLAYLIST_SUBTITLE:
                case eMEDIATYPE_PLAYLIST_IFRAME:
                    ctx.fillStyle = '#cccc00'; // dark yellow
                    break;

                case eMEDIATYPE_INIT_IFRAME:
                case eMEDIATYPE_IFRAME:
                    ctx.fillStyle = '#ffffcc'; // light yellow
                    break;
                         
                case eMEDIATYPE_LICENSE:
                case eMEDIATYPE_INIT_SUBTITLE:
                case eMEDIATYPE_SUBTITLE:
                default:
                    ctx.fillStyle = '#ffaaff';
                    ctx.strokeStyle = '#000000';
                    break;
                }
            }
                         
			data[i].y -= ROWHEIGHT / 2;
			
            ctx.fillRect(data[i].x,
						 data[i].y,//data[i].y - ROWHEIGHT / 2,
						 data[i].w, ROWHEIGHT - 1);
      

            ctx.strokeStyle = '#999999';
            ctx.strokeRect(data[i].x,
						   data[i].y,//data[i].y - ROWHEIGHT / 2,
						   data[i].w, ROWHEIGHT - 1);
        }
                           

        // draw y-axis labels
        ctx.textAlign = "right";
        ctx.strokeStyle = '#dddddd';
        ctx.fillStyle = "#ffffff";
        ctx.fillRect(0,0,BITRATE_MARGIN,bitrate2y(0)+24 );
        for (var i = 0; i < allbitrates.length; i++) {
            var bitrate = allbitrates[i];
            var y = bitrate2y(bitrate);
            ctx.fillStyle = '#000000';
            var label = (bitrate==0)?"audio/other":bitrate;
            ctx.fillText(label, BITRATE_MARGIN, y + ROWHEIGHT / 2 - 3);
        }
						 
		var date = new Date(timeOffset + timestamp_min);
		dateString = date.toLocaleTimeString();
		ctx.fillStyle = '#000000';
		ctx.fillText( dateString, BITRATE_MARGIN, topMargin );

		DrawMediaTimeline();
    }
                           
    function TimePlus()
    {
		for( var i=0; i<marker.length; i++ )
		{
			if( marker[i][3]=="exception" )
			{
				var dt = marker[i][0]-timestamp_min;
				if( dt > timeOffset )
				{
					timeOffset = dt;
					Plot();
					return;
				}
			}
		}
    }

    function TimeMinus()
    {
		for( var i=marker.length-1; i>=0; i-- )
		{
			if( marker[i][3]=="exception" )
			{
				var dt = marker[i][0]-timestamp_min;
				if( dt < timeOffset )
				{
					timeOffset = dt;
					Plot();
					return;
				}
			}
		}
		timeOffset = 0;
		Plot();
    }

	function showJumpHelp()
	{
		alert(
			"GMT Time: <YYYY> <MON> <DAY> <HH>:<MM>:<SS>\n" +
			"Example: 2020 Apr 11 23:17:20\n" +
			"-or-\n" +
			"Session Offset: <hh>:<mm>:<ss> or <mm>:<ss> or <ss>\n" +
			"Example: 1:30\n"+
			"-or-\n"+
			"Browser Offset: +<time> or -<time>\n" +
			"Example: +30" );
	}
    // function to jump to a particular position in timeline
    function jumpToPosition() {
		var value = document.getElementById("jumpPosition").value;
        if( value) {
			if( value.indexOf(" ")>=0 )
			{
				var tm = ParseReceiverLogTimestamp(value+ ": [AAMP-PLAYER]");
				if(tm)
				{
					timeOffset = tm - timestamp_min;
				}
			}
			else
			{
				var plusMinus = value.substr(0,1);
				var base;
				if( plusMinus=='-' || plusMinus=='+' )
				{
					relTimeBase = timeOffset;
					value = value.substr(1); // strip first character
				}
				
				var part = value.split(":");
				switch( part.length )
				{
				case 1:
					timeOffset = part[0]*1000;
					break;
				case 2:
					timeOffset = part[1]*1000+part[0]*60*1000;
					break;
				case 3:
					timeOffset = part[2]*1000+part[1]*60*1000+part[0]*60*60*1000;
					break;
				default:
					break;
				}
				if( plusMinus=='+' )
				{
					timeOffset = relTimeBase + timeOffset;
				}
				else if( plusMinus=='-' )
				{
					timeOffset = relTimeBase - timeOffset;
				}
			}
			Plot();
        }
    }

    function handleFileSelect(evt) {
		console.log( "entering handleFileSelect\n" );
        var files = evt.target.files;
        for (var fileIndex = 0; fileIndex < files.length; fileIndex++) {
            var f = files[fileIndex];
            if (f.type = "text/plain")
			{
				var filename = f.name;
				console.log( "processing: " + filename );
				var reader = new FileReader();
                reader.onload = myLoadHandler;
                reader.readAsText(f);
            }
        }
    }

    // For a new Tune Session
    function newSession(evt) {
        var currentSession = document.getElementById("session");
        sessionClickedID = currentSession.options[currentSession.selectedIndex].value;
        sessionClicked = true;
        mySessionFilter();
    }

    // For a new checkbox change request
    function newCheckBoxChange(evt) {
        if(sessionClicked) {
            myLoadHandler();
        } else {
            var event = new Event('change');
            // Dispatch the change event
            document.getElementById('files').dispatchEvent(event);
        }
    }

    document.getElementById('files').addEventListener('change', handleFileSelect, false);
    document.getElementById('session').addEventListener('change', newSession, false);
    document.getElementById('timeplus').addEventListener('click', TimePlus );
    document.getElementById('timeminus').addEventListener('click', TimeMinus );
    document.getElementById('jumpButton').addEventListener('click', jumpToPosition );
    document.getElementById('jumpHelpButton').addEventListener('click', showJumpHelp );
	document.getElementById('indexButton').addEventListener('click', IndexRawData );
}
