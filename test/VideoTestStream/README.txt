Video Test Stream
=================

In this directory the script generate-hls-dash.sh can be used to create a video
test stream in HLS and DASH format. The script startserver.sh can be used to run
a web server (server.py) so that AAMP can play the video test stream.

The following streams are available:

main.mpd

  DASH.

main.m3u8

  HLS TS format.

main_mp4.m3u8

  HLS MP4 format.

Multiple video profiles (for ABR) and audio languages are supported.


generate-hls-dash.h
===================

To generate the video test stream on Ubuntu for example:

$ cd aamp/tests/VideoTestStream
$ ./generate-hls-dash.h

Known Limitations
-----------------

Currently no subtitles are available in the video test stream.

Multiplexed HLS test streams are generated but there is no master playlist.

The DASH manifest main.mpd and HLS master playlists main.m3u8 and main_mp4.m3u8
are not generated. If you change the generated video test stream parameters in
generate-hls-dash.h you may need to edit these files.


startserver.sh
==============

The script startserver.sh can be used to run a web server (server.py) so that
AAMP can play the the generated video test stream. Options can be passed to the
server to enable the emulation of an ongoing event (i.e. content growing over
time) or, for HLS, a live event (i.e. a content window).

After generating the video test stream you can play the HLS video test stream on
Ubuntu for example:

$ cd aamp/tests/VideoTestStream
$ ./startserver.sh

In another terminal window:

$ cd aamp
$ LD_LIBRARY_PATH=./Linux/lib ./Linux/bin/aamp-cli http://127.0.0.1:8080/main.m3u8

The following URIs are supported:

http://127.0.0.1:8080/main.mpd

  DASH stream.

http://127.0.0.1:8080/main.m3u8

  HLS TS format stream.

http://127.0.0.1:8080/main_mp4.m3u8

  HLS MP4 format stream.

Press Ctrl-C to terminate the server.

Server Options
--------------

A number of server options are available including some which modify files on
the fly as they are downloaded by AAMP. These include:

--event

  Emulate an ongoing event where the available content grows over time.

--live

  Emulate a live event where a moving window of content is available. HLS only.

The --help option lists all of the server options. For example:

$ ./startserver.sh --help
usage: server.py [-h] [--vod | --event | --live] [--time] [--discontinuity]
                 [--port PORT] [--mintime MINTIME] [--livewindow LIVEWINDOW]
                 [--all]

AAMP video test stream HTTP server

optional arguments:
  -h, --help            show this help message and exit
  --vod                 VOD test stream (default)
  --event               emulate an event test stream
  --live                emulate a live test stream (HLS only)
  --time                add EXT-X-PROGRAM-DATE-TIME tags to HLS (or live)
                        event playlists (enabled for live)
  --discontinuity       add EXT-X-DISCONTINUITY tags to HLS event playlists
  --port PORT           HTTP server port number
  --mintime MINTIME     starting event (or live) duration in seconds (default
                        10)
  --livewindow LIVEWINDOW
                        live window in seconds (default 30)
  --all                 enable GET of all files. By default, only files with
                        expected extensions will be served

Known Limitations
-----------------

Only run the server in a trusted development environment.

DASH dynamic presentation emulation is only partially implemented.

HTTPS is not supported.

The web server server.py makes certain assumptions about the format of video
test stream files. If these files are changed then the results may be
unexpected when using event, live emulation or other options which modify on the
fly the files downloaded by AAMP.

