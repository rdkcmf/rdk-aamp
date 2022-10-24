##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2022 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

import time
import re
import os
import argparse
from datetime import datetime
from enum import Enum
from http.server import BaseHTTPRequestHandler, HTTPServer

class StreamType(Enum):
    VOD = 1
    EVENT = 2
    LIVE = 3

class TestServer(BaseHTTPRequestHandler):
    startTime = time.time()
    minTime = 10.0
    liveWindow = 30.0
    streamType = StreamType.VOD
    addPDTTags = False
    addDiscontinuities = False
    getAll = False

    def getEventHLSPlaylist(self, path):
        """Get a video test stream HLS playlist modified to emulate an ongoing
        event or live.

        Event media playlists are truncated based on the time since the server
        was started. Live media playlists contain a window of segments based on
        the time since the server was started. Master playlists are unmodified.

        self -- Instance
        path -- Playlist file path
        """

        currentTime = time.time()
        currentPlayTime = (currentTime - self.startTime) + self.minTime
        segmentTime = 0.0
        extXTargetDurationPattern = re.compile(r"^#EXT-X-TARGETDURATION:([\d\.]+).*")
        extXPlaylistTypePattern = re.compile(r"^#EXT-X-PLAYLIST-TYPE:.*")
        extinfPattern = re.compile(r"^#EXTINF:([\d\.]+),.*")
        extXMediaSequencePattern = re.compile(r"^#EXT-X-MEDIA-SEQUENCE:.*")
        mediaUrlPattern = re.compile(r"^[^#\s]")
        skipSegment = False
        targetDuration = 0.0
        totalDuration = 0.0
        firstSegment = True
        sequenceNumber = 0

        # Get the target and total durations
        with open(path, "r") as f:
            for line in f:
                # "^#EXTINF:([\d\.]+),.*"
                m = extinfPattern.match(line)
                if m:
                    # Extract the segment duration.
                    totalDuration += float(m.group(1))
                    continue

                # "^#EXT-X-TARGETDURATION:([\d\.]+).*"
                m = extXTargetDurationPattern.match(line)
                if m:
                    # Extract the target duration.
                    targetDuration = float(m.group(1))
                    continue

        # Get a modified version of the playlist
        with open(path, "r") as f:
            self.send_response(200)
            self.end_headers()

            for line in f:
                # "^#EXTINF:([\d\.]+),.*"
                m = extinfPattern.match(line)
                if m:
                    # Extract the segment duration.
                    segmentDuration = float(m.group(1))

                    # Truncate the playlist if the next segment ends after the
                    # current playlist time.
                    if currentPlayTime < (segmentTime + segmentDuration):
                        break

                    # In live emulation, skip segments outside the live window
                    # unless the total duration would be less than three times
                    # the target duration.
                    totalDuration -= segmentDuration
                    if ((self.streamType == StreamType.LIVE) and
                       (currentPlayTime >= (segmentTime + segmentDuration + self.liveWindow)) and
                       (totalDuration >= (targetDuration*3.0))):
                        skipSegment = True
                        segmentTime += segmentDuration
                        continue
                    else:
                        skipSegment = False

                    # If this is the first segment to be emitted, then emit the
                    # EXT-X-MEDIA-SEQUENCE and EXT-X-DISCONTINUITY-SEQUENCE
                    # tags.
                    if firstSegment:
                        firstSegment = False
                        self.wfile.write(bytes("#EXT-X-MEDIA-SEQUENCE:%d\n" % (sequenceNumber), "utf-8"))

                        if self.addDiscontinuities:
                            # Segment 0 has no EXT-X-DISCONTINUITY flag. If
                            # segment 0 is removed from the playlist, don't
                            # increment the EXT-X-DISCONTINUITY-SEQUENCE value.
                            if sequenceNumber == 0:
                                self.wfile.write(bytes("#EXT-X-DISCONTINUITY-SEQUENCE:0\n", "utf-8"))
                            else:
                                self.wfile.write(bytes("#EXT-X-DISCONTINUITY-SEQUENCE:%d\n" % (sequenceNumber - 1), "utf-8"))

                    if self.addDiscontinuities and sequenceNumber > 0:
                        # Segment 0 has no EXT-X-DISCONTINUITY flag.
                        self.wfile.write(bytes("#EXT-X-DISCONTINUITY\n", "utf-8"))

                    if self.addPDTTags:
                        # Add program date time tag
                        timestring = datetime.fromtimestamp(self.startTime + segmentTime).astimezone().isoformat(timespec='milliseconds')
                        self.wfile.write(bytes("#EXT-X-PROGRAM-DATE-TIME:" + timestring + "\n", "utf-8"))

                    segmentTime += segmentDuration
                # "^#EXT-X-PLAYLIST-TYPE:.*"
                elif extXPlaylistTypePattern.match(line):
                    # Skip or replace the playlist type tag
                    if self.streamType == StreamType.EVENT:
                        line = "#EXT-X-PLAYLIST-TYPE:EVENT\n"
                    else:
                        continue
                # "^#EXT-X-MEDIA-SEQUENCE:.*"
                elif extXMediaSequencePattern.match(line):
                    # Delay emitting the EXT-X-MEDIA-SEQUENCE tag until the
                    # first segment is about to be emitted when we know what the
                    # first sequence number is.
                    continue
                # "^[^#\s]"
                elif mediaUrlPattern.match(line):
                    # Media segment URI.
                    sequenceNumber += 1
                    if skipSegment:
                        skipSegment = False
                        continue

                self.wfile.write(bytes(line, "utf-8"))

    def getEventDASHManifest(self, path):
        """Get a video test stream DASH manifest modified to emulate an
        ongoing event.

        Later segments are removed from the manifest based on the time since the
        server was started.

        self -- Instance
        path -- Manifest file path
        """

        currentTime = time.time()
        currentPlayTime = (currentTime - self.startTime) + self.minTime
        segmentTime = 0.0
        timescale = 10000.0
        mpdTypePattern = re.compile(r"type=\"static\"")
        mpdMediaPresentationDurationPattern = re.compile(r"mediaPresentationDuration=\"PT((\d+H)?)((\d+M)?)(\d+\.?\d+S)\"")
        mpdTimescalePattern = re.compile(r"timescale=\"([\d]+)\"")
        mpdSegmentPattern = re.compile(r"<S((\s+\w+=\"\d+\")+)\s*/>")
        mpdSegmentDurationPattern = re.compile(r"d=\"(\d+)\"")
        mpdSegmentRepeatPattern = re.compile(r"r=\"(\d+)\"")
        mpdSegmentTimePattern = re.compile(r"t=\"(\d+)\"")

        # Get a modified version of the playlist
        with open(path, "r") as f:
            self.send_response(200)
            self.end_headers()

            for line in f:
                # "type=\"static\""
                m = mpdTypePattern.search(line)
                if m:
                    # Change the manifest type
                    line = line.replace("\"static\"", "\"dynamic\"")

                # "mediaPresentationDuration=\"PT((\d+H)?)((\d+M)?)(\d+\.?\d+S)\""
                m = mpdMediaPresentationDurationPattern.search(line)
                if m:
                    # Remove mediaPresentationDuration for dynamic manifests
                    line = mpdMediaPresentationDurationPattern.sub("", line)

                # "timescale=\"([\d]+)\""
                m = mpdTimescalePattern.search(line)
                if m:
                    # Extract the segment timescale
                    timescale = float(m.group(1))
                    segmentTime = 0.0

                # "<S((\s+\w+=\"\d+\")+)\s*/>"
                m = mpdSegmentPattern.search(line)
                if m:
                    # Segment attributes
                    attributes = m.group(1)

                    # Extract the segment duration attribute
                    # "d=\"(\d+)\""
                    m = mpdSegmentDurationPattern.search(attributes)
                    if m:
                        d = float(m.group(1))

                        # Extract the segment repeat attribute (plus one)
                        # "r=\"(\d+)\""
                        m = mpdSegmentRepeatPattern.search(attributes)
                        if m:
                            rPlusOne = float(m.group(1)) + 1.0
                        else:
                            rPlusOne = 1.0

                        # Extract the time at the start of the segments
                        # "t=\"(\d+)\""
                        m = mpdSegmentTimePattern.search(attributes)
                        if m:
                            t = float(m.group(1))
                            startSegmentTime = t/timescale
                        else:
                            t = None
                            startSegmentTime = segmentTime

                        # Truncate the number of segments if required
                        segments = (currentPlayTime - startSegmentTime)/(d/timescale)
                        segmentTime = startSegmentTime + ((rPlusOne*d)/timescale)
                        if segments < 1.0:
                            # No complete segment
                            continue
                        elif (segments <= rPlusOne):
                            # Truncate the segments
                            rPlusOne = segments

                        # Rewrite the manifest
                        if t is not None:
                            if rPlusOne >= 2.0:
                                line = mpdSegmentPattern.sub("<S t=\"%d\" d=\"%d\" r=\"%d\" />" % (int(t), int(d), int(rPlusOne - 1.0)), line);
                            else:
                                line = mpdSegmentPattern.sub("<S t=\"%d\" d=\"%d\" />" % (int(t), int(d)), line);
                        else:
                            if rPlusOne >= 2.0:
                                line = mpdSegmentPattern.sub("<S d=\"%d\" r=\"%d\" />" % (int(d), int(rPlusOne - 1.0)), line);
                            else:
                                line = mpdSegmentPattern.sub("<S d=\"%d\" />" % (int(d)), line);

                self.wfile.write(bytes(line, "utf-8"))

    def getFile(self, path):
        """Get a file.

        self -- Instance
        path -- File path
        """

        with open(path, "rb") as f:
            contents = f.read()
            self.send_response(200)
            self.end_headers()
            self.wfile.write(contents)

    def do_GET(self):
        """Get request.

        self -- Instance
        """

        # Extract the relative path and file extension
        path = self.path[1:]
        filename, extension = os.path.splitext(path)

        try:
            if extension == ".m3u8":
                # HLS playlist
                if self.streamType == StreamType.VOD:
                    self.getFile(path)
                else:
                    self.getEventHLSPlaylist(path)
            elif extension == ".mpd":
                # DASH manifest
                if self.streamType == StreamType.EVENT:
                    self.getEventDASHManifest(path)
                else:
                    self.getFile(path)
            elif self.getAll:
                # Get all files
                self.getFile(path)
            elif extension == ".m4s":
                # fMP4 segment
                self.getFile(path)
            elif extension == ".ts":
                # MPEG TS segment
                self.getFile(path)
            elif extension == ".mp3":
                # MP3 audio
                self.getFile(path)
            else:
                self.send_response(404)
                self.end_headers()

        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()

if __name__ == "__main__":
    hostName = "localhost"
    serverPort = 8080

    # Parse the command line arguments.
    parser = argparse.ArgumentParser(description="AAMP video test stream HTTP server")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--vod", action="store_true", help="VOD test stream (default)")
    group.add_argument("--event", action="store_true", help="emulate an event test stream")
    group.add_argument("--live", action="store_true", help="emulate a live test stream (HLS only)")
    parser.add_argument("--time", action="store_true", help="add EXT-X-PROGRAM-DATE-TIME tags to HLS (or live) event playlists (enabled for live)")
    parser.add_argument("--discontinuity", action="store_true", help="add EXT-X-DISCONTINUITY tags to HLS event playlists")
    parser.add_argument("--port", type=int, help="HTTP server port number")
    parser.add_argument("--mintime", type=float, help="starting event (or live) duration in seconds (default %d)" % (TestServer.minTime))
    parser.add_argument("--livewindow", type=float, help="live window in seconds (default %d)" % (TestServer.liveWindow))
    parser.add_argument("--all", action="store_true", help="enable GET of all files. By default, only files with expected extensions will be served")
    args = parser.parse_args()

    if args.event:
        TestServer.streamType = StreamType.EVENT

    if args.live:
        TestServer.streamType = StreamType.LIVE
        TestServer.addPDTTags = True

    if args.time:
        TestServer.addPDTTags = True

    if args.discontinuity:
        TestServer.addDiscontinuities = True

    if args.port:
        serverPort = args.port

    if args.mintime:
        TestServer.minTime = args.mintime

    if args.livewindow:
        TestServer.liveWindow = args.livewindow

    if args.all:
        TestServer.getAll = True

    # Create and run the HTTP server.
    testServer = HTTPServer((hostName, serverPort), TestServer)
    print("Server started http://%s:%s" % (hostName, serverPort))

    try:
        testServer.serve_forever()
    except KeyboardInterrupt:
        pass

    testServer.server_close()
    print("Server stopped.")

