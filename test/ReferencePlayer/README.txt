
A. Overview:

    AAMP Reference Player is a bundle of reference players showcasing various AAMP interfaces. The different reference players are -

    1. HTML Reference Player - As the name suggests, this reference player showcases AAMP interfaces that are leveraged using
       standard HTML5 <video> tag. To select AAMP, the URI protocol must be changes to aamp:// or aamps:// instead of http:// or https:// respectively
       This forces webkit's MediaPlayer to pick gstaampsrc and gstaamp gstreamer plugins. The gstaamp gstreamer plugin houses the
       AAMP player.

    2. UVE Reference Player - Unified Video Engine (UVE) is a set of JavaScript binding APIs which allows its user to interact with
       AAMP player. The bindings are made available in JavaScript with the help of injectedbundle once DOM elements are loaded by webkit.
       The specification document for UVE is available at
       current repo AAMP-UVE-API.pdf (./../../AAMP-UVE-API.pdf)

B. Target audience:

    This README is targeted to OTT app vendors and HTML5 developers who are interested in adopting AAMP for their media player
    applications in STB.

C. General setup:

    C.1. Setup in RDK devices:

         a. Host ReferencePlayer folder in a web server or in /opt/www/ folder in device
            Folders in /opt/www/ can be accessed using http://localhost:50050/<folder name>

         b. If hosted in /opt/www folder, need the following override config files in /opt folder since
            XRE rejects Fling requests with URLs with domain localhost/127.0.0.1. Contents for the override files are provided in
            Appendix section.

            b.1. /opt/webfilterpatterns.conf

         c. To get input fields rendered correctly, add the following line in /opt/SetEnv.sh
           c.1 export RFC_WEBKIT_NICOSIA_PAINTING_THREADS=0

         d. To launch ReferencePlayer, use IBIS App (https://ibis.comcast.com)
           d.1 Login to IBIS
           d.2 Select "Manage Devices" and select Pair the device using either XRE Easy pair or using IBIS pair
           d.3 Select "Launch HTML App" tab
           d.4 Selct the device
           d.5 fill in URL to launch (i.e. http://localhost:50050/ReferencePlayer/index.html) and any App name
           d.6 Click "Launch" button

D. Features

    1. Clear and encrypted(Access/AES-128) HLS supported
    2. Clear and encrypted(Playready/Widevine) DASH supported
    3. Supports inband closedcaptions
    4. Audio language selection is supported
    5. Trickplay and Audio/Video bitrate selection is supported

E. Folder structure

   -icons                            // UI elements of reference players and homepage
   -UVE
     -index.html                     // Homepage of UVE reference player
     -UVEMediaPlayer.js              // Includes the "AAMPPlayer" JS class which wraps UVE binding object AAMPMediaPlayer.
     -UVEPlayerUI.js                 // JS code for the UI elements and their functionality
     -UVERefPlayer.js                // Main JS file
     -UVERefPlayerStyle.js           // CSS for reference player and its UI
   -VIDEOTAG
     -AAMPReferencePlayer.js         // JS code for HTML reference player
     -AAMPReferencePlayerStyle.js    // CSS for reference player and its UI
     -index.html                     // Homepage of HTML reference player
   -index.html                       // Homepage of reference player
   -ReferencePlayer.js               // JS  code for Homepage and redirection to respective reference players
   -URLs.js                          // JS code to give URLs
   -ReferencePlayerStyle.css         // CSS for Homepage and its UI

F. Appendix

a. /opt/webfilterpatterns.conf
{"webfilter":[
  {"host":"localhost", "block":"0"},
  {"host":"127.0.0.1", "block":"1"},
  {"host":"169.254.*", "block":"1"},
  {"host":"192.168.*", "block":"1"},
  {"host":"[::1]", "block":"1"},
  {"scheme":"file", "block":"1"},
  {"scheme":"ftp", "block":"1"},
  {"scheme":"*", "block":"0"},
  {"block":"1"}
]}
