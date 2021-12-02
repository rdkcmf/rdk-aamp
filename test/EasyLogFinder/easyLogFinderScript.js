window.onload = function()
{
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
    //    var e = windowHandle.document.getElementById('myDiv');
    //    e.scrollIntoView();
    }
	
    var tuneBaseTimeUTC;
    
    function FormatTime( utc )
    {
		if( !tuneBaseTimeUTC )
		{
			tuneBaseTimeUTC = utc;
		}

		// convert elapsed milliseconds to fixed-width <minutes>:<seconds>.<ms>
		utc -= tuneBaseTimeUTC; // make relative
		var ms = utc%1000;
		var s = Math.floor(utc/1000);
		var minute = Math.floor( s/60 );
		var second = s%60; // 0..59
		if( second<10 ) second = "0" + second;
		if( ms<10 ) ms = "00" + ms; else if( ms<100 ) ms = "0" + ms;
		return Pad(minute) + ":" + second + "." + ms;
	}
	
    var output;
    var statistics;
	var tune_statistics;
    var timestamp;
    var track;
    var lineNumber;
                                
    function Output( text, color )
    {
		var string = Pad(lineNumber+1)+":" + FormatTime(timestamp) + " " + text;
		if( color )
		{
			string = '<font color="'+color+'">' + string + '</font>';
		}
        output.push( string );
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
		if( !tune_statistics[type] )
		{
			tune_statistics[type] = [];
		}
		if( !tune_statistics[type][err] )
		{
			tune_statistics[type][err] = 0;
		}
		tune_statistics[type][err]++;
		for( var bucket=0; bucket<20000; bucket+=500 )
		{
			if( ms<bucket )
			{
				var key = bucket;
				if( !tune_statistics[type][key] )
				{
					tune_statistics[type][key] = 0;
				}
				tune_statistics[type][key]++;
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
		if( !statistics[type] )
		{
			statistics[type] = [];
		}
		if( !statistics[type][err] )
		{
			statistics[type][err] = 0;
		}
		statistics[type][err]++;
		if( err=="HTTP200(OK)" )
		{ // populate histogram buckets, to allow view of download time distribution
			// we might want to split by file type; playlists and video fragments likely to have very
			// different download characteristics.
			if( ms<0 || ms>10000 ) ms = 10000;
			for( var bucket=0; bucket<10000; bucket+=250 )
			{
				if( ms<bucket )
				{
					var key = bucket;
					if( !statistics[type][key] )
					{
						statistics[type][key] = 0;
					}
					statistics[type][key]++;
					break;
				}
			}
		}
	}
									
    function myLoadHandler( e )
    {
        var raw = e.target.result;
        if( raw )
        {
            var lines = raw.split("\n");
            tuneBaseTimeUTC = null;
            var isLive = true;
            var tuneInProgress;
            var currentBitrate = null;
            track = null;
			var position,err,type,code,url;

			var trace = false;
            
			for( lineNumber=0; lineNumber<lines.length; lineNumber++ )
            {
                var line = lines[lineNumber];
                timestamp = ParseReceiverLogTimestamp(line);
				
				var offs = line.indexOf("AAMPMediaPlayerJS_initConfig(): ");
				if( offs>=0 )
				{
					Output( line.substr(offs+32), "blue" );
					continue;
				}
					
				if( (httpRequestEnd=ParseFogDownload(line)) )
				{
					err = mapError(httpRequestEnd.responseCode);
					var ms = 1000*httpRequestEnd.curlTime;
					url = httpRequestEnd.url;
					type = httpRequestEnd.type;
					//if( type!==undefined ) type = mediaTypes[type]; else type = "";
					var special = null;
					if( err!="HTTP200(OK)" )
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
					continue;
				}
				
				var aampLogOffset = line.indexOf( "[AAMP-PLAYER]" );
                if( aampLogOffset >=0 )
				{
                    var payload = line.substr(aampLogOffset+13); // skip "[AAMP-PLAYER]"
                    var param;
                    var httpRequestEnd;

					if( payload.startsWith("SetValue:") )
					{
					//	Output(payload,"blue");
					}
					
					else if( payload.startsWith("Received X-Reason header from CDN Server:") )
					{
//						Output( "X-Reason: " + payload.substr(41), "red" );
					}
					else if( payload.indexOf("aamp_tune:")>=0 )
                    {
                        tuneInProgress = true;
						// tuneBaseTimeUTC = timestamp;
                        Output( "--> New Tune "+new Date(timestamp), "green" );
                    }
					/*
                    else if( (param = sscanf(payload,"aamp pos: [%%..%%..%%..%%]")) )
                    {
						if( trace )
                        Output( param[0] + ".." + param[1] + ".." + param[2] );
                    }
                    else if( payload.startsWith("## AAMPGstPlayer_OnGstBufferUnderflowCb") )
                    {
						Output( "***Video UnderFlow***" );
                    }
					 */
                    else if( (param = sscanf(payload,"aamp_Seek(%%) and seekToLive(%%)" )) )
                    {
						if( trace )
                        Output( "Seek "+ parseFloat(param[0])+"s" );
                    }
                    else if( (param=sscanf(payload,"aamp_stop PlayerState=%%")) )
                    {
						if( trace )
                        Output( "aamp_stop("+playerStates[parseInt(param[0])]+")" );
                    }
                    else if( (payload.indexOf("aamp: EXT-X-PLAYLIST-TYPE - VOD")>=0) )
                    {
                        isLive = false;
									if( trace )
                        Output( "Playlist Type is VOD" );
                    }
                    else if( payload.indexOf("GST_MESSAGE_EOS")>=0 )
                    {
						if( trace )
                        Output( "EOS reached" );
                    }
                    else if( (param = sscanf(payload,"AAMPLogABRInfo : switching to '%%' profile '%% -> %%' currentBandwidth[%%]->desiredBandwidth[%%] nwBandwidth[%%] reason='%%' symptom='video quality may decrease")) )
                    {
						if( trace )
                        Output( "ABR " + param[3] + "->" + param[4] );
                    }
                    else if( payload.indexOf("cdb_underflow_cb")>=0 )
                    {
						if( trace )
                        Output( "cdb_underflow_cb" );
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
						if( err!="HTTP200(OK)" )
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
                    else if( payload.indexOf("IP_AAMP_TUNETIME:")>=0 )//payload.startsWith("IP_AAMP_TUNETIME:") )
                    {
						param = payload.substr(17).split(",");
                       //if( trace )
                       //tuneBaseTimeUTC = timestamp; // rebase timestamp to track time since tune complete
//                        tuneInProgress = false;
						
						var ms = param[35];
						Output( "IP_AAMP_TUNETIME "+ms, "green" );
						UpdateTuneStatistics(ms);
                    }
                    else if( payload.startsWith("NotifyBitRateChangeEvent") )
                    {
                        var offs = payload.indexOf("bitrate:");
                        var bitrate = -1;
                        if( offs>=0 )
                        {
                            bitrate = parseInt(payload.substr(offs+8) );
                        }
                        var comment;
                        if( currentBitrate===null )
                        {
                            comment = "initial";
                        }
                        else if( bitrate > currentBitrate )
                        {
                            comment = "ramp up";
                        }
                        else if( bitrate < currentBitrate )
                        {
                            comment = "ramp down";
                        }
                        else
                        {
                            comment = "?";
                        }
                        currentBitrate = bitrate;
                        if( trace )Output( Pad(bitrate/1000) + "kbps (" + comment + ")" );
                    }
                    else if( payload.startsWith("PrivateInstanceAAMP::SendEventAsync:813 Failed to send event type") )
                    {
						code = parseInt(payload.substr(65));
											   if( trace )
                        Output( "SendEventAsync("+eventNames[code]+")" );
                    }
                    else if( payload.startsWith("[AAMP_JS] SendEventSync(type=") )
                    {
                        code = parseInt(payload.substr(29));
											   if( trace )
                        Output( "SendEventSync("+eventNames[code]+")" );
                    }
                    else if( (param = sscanf(payload,"%% Downloading Playlist Type:%% for PreCaching:%%")) )
                    {
						if( trace )
                        Output( "PRECACHE " + param[1] + ":" + param[2] );
                    }
                    else if( payload.indexOf("End of PreCachePlaylistDownloadTask")>=0 )
                    {
						if( trace )
                        Output("PRECACHE COMPLETE" );
                    }
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
					attr = (attr-250) + ".." + attr;
				}
				else
				{
					value = value.toString();
					value += "     ";
				}
				//console.log( "value='"+value+"'" + value.length );
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
        var extra1 = ExportStatistics(statistics,"HTTP200(OK)");
        var extra2 = ExportStatistics(tune_statistics,"Success");
    
        popUp( "<html><head><title>" + this.filename + "</title></head><body><pre>" +
                output.join("<br/>") +
              "<hr/>" + extra1 +"<hr/>" + extra2 +
                "</pre></body></html>" );
    }
    
    function handleFileSelect(evt) {
        var files = evt.target.files;
        statistics = [];
		tune_statistics = [];
        output = [];
        for(var fileIndex=0; fileIndex<files.length; fileIndex++ )
        {
            var f = files[fileIndex];
			//if( f.type == "text/plain" )
            {
                var filename = f.name;
                console.log( "processing: " + filename + ", type="+f.type );
                var reader = new FileReader();
                output.push( filename );
                reader.onload = myLoadHandler;
                reader.filename = filename;
                reader.readAsText(f);
            }
        }
    }
    
    document.getElementById('files').addEventListener('change', handleFileSelect, false);
    document.getElementById('AnalysisId').addEventListener('click', ExportLogs );
};

