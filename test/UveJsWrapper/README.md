# UVE JS WRAPPER
AAMP UVE JS WRAPPER is a single file uve.js which can be added to any html app and can be used to play CLEAR, Encrypted contents in desktop browsers using UVE APIs by making use of different mse/eme players. This bundle includes other sample files on how to use uve.js wrapper.

## Overview:


    1. Using uvejs wrapper an application can be run on desktop browsers using UVE APIs. The uvejs wrapper will be using other mse/eme players under the hood for playing. 

    2. Have support to play both CLEAR and Encrypted contents.

    3. The same app can be run on both Set-top-boxes and desktop browsers.

    3. uve.js wrapper supports usage of basic UVE APIs described in AAMP-UVE-API.pdf like load(), play(), pause(), seek(), setPlaybackRate(), setDRMConfig() etc. This also includes all aamp player events supported by videtag player.

    4. User have the provision to select the engine for playback from AAMP, DASHJS, HLSJS, SHAKA and VIDEOTAG.


## Usage:     
    1. In index.html
        import uve.js wrapper via a script tag.
    2. Write a file similar to app.js ( or other appDrm files).
    3. In index.html
        import app.js (or other appDrm files) via script tag.
    4. In app.js
        i) Select the preferred engine for playback before onloading.
            uve_prepare(preferredEngine, useAAMPInSTB)
            eg: uve_prepare("dashjs",true);
        ii) create a UVEPlayer instance
            eg: var player = new UVEPlayer("appName");
        iii) Use UVE APIs for playback.
    5. Launch the app as per next section.


## General setup:

    To Run in Desktop Browsers:
        To play CLEAR contents, app can be directly launched using the browser. To play Encrypted content, app needs to be hosted to avoid browsers blocking the request.
        To play encrypted content,
            a. Host the app into a server or
            b. Run the app in localhost
                i) To run a local python server in the PC 
                ii) If Python version is above is 3.X
                        python3 -m http.server
                    If Python version is above is 2.X
                        python -m SimpleHTTPServer
                iii)  App can be access in browser using http://localhost:8000/

    To Run in RDK Devices:

         a. Only RDK devices have AAMPMediaPlayer object. Hence AAMP engine can be only used in RDK devices and not in Desktop.

         b. Host this player in a web server or in /opt/www/ folder in device
            Folders in /opt/www/ can be accessed using http://localhost:50050/<folder name>

         c. According to the platform launch the player in box using IBIS, rdkshell, rdkbroswer2, etc.

## Features

    1. Clear and encrypted DASH contents supported.
    2. Player events supported.
    3. Basic UVE APIs supported.
    4. provision to select the playback engine from from AAMP, DASHJS, HLSJS, SHAKA and VIDEOTAG.


## Folder structure

    -index.html                 // Main html page to launch the app via browser.
    -uve.js                     // UVEJS wrapper which maps UVE APIs and events into MSE/EME player APIs and events.
    -sampleApp.js                     // Sample app file to play a CLEAR DASH stream using UVE APIs via DASHJS engine.
    -drm usage samples
        -appDrmDashjs.js            // Sample app file to play an Encrypted DASH stream using UVE APIs via DASHJS engine.
        -appDrmShaka.js            // Sample app file to play an Encrypted DASH stream stream using UVE APIs via SHAKA engine.
        -appDrmHlsjs.js            // Sample app file to play an Encrypted HLS stream using UVE APIs via HLSJS engine.
