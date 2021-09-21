
A. Overview:

  AAMP CDAI Test App make use of AAMP player switching by maintaining two aamp player instances; one for playback and one for caching for  swtiching between main and Ad contents. 

B. Configuration:
  - The virtualPlaylist.js contains a list of urls which will act as a virtual playlist. Once configured and clicked play the contents will be played according to the time range specified in it from first url to last url in order once.
  - parameters to be specified for each content are,
    a) name  - name of the content
    b) url   - url of the content
    c) start - playback start position of content(in milliseconds)
    d) end   - playback end position and switching to next content position(in milliseconds)

C. General setup:

    C.1. Setup in RDK devices:

         a. Host AAMPCDAITestApp folder in a web server or in /opt/www/ folder in device
            Folders in /opt/www/ can be accessed using http://localhost:50050/<folder name>

         b. If hosted in /opt/www folder, need the following override config files in /opt folder since
            XRE rejects Fling requests with URLs with domain localhost/127.0.0.1. Contents for the override files are provided in
            Appendix section.

            b.1. /opt/webfilterpatterns.conf
            b.2. /opt/xrehtmlappswhitelist.conf


         c. To launch CDAI Test App, use IBIS App (https://ibis.comcast.com/app-dev-tool/send-html-app)
           c.1 Selct the device
           c.2 fill in URL to launch (i.e. http://localhost:50050/AAMPCDAITestApp/index.html) and App name
           c.3 Click "Launch" button


D. Folder structure

   -icons                            // UI elements of reference players and homepage
   -index.html                       // Homepage of CDAI Test App
   -UVEMediaPlayer.js                // Includes the "AAMPPlayer" JS class which wraps UVE binding object AAMPMediaPlayer.
   -appUI.js                         // JS code for the UI elements and their functionality
   -appPlayer.js                     // Main JS file
   -appStyle.css                     // CSS for CDAI Test App and its UI
   -virtualPlaylist.js               // To specify contents for making virtual playlist

E. Appendix

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

b. /opt/xrehtmlappswhitelist.conf
{"htmlappswhitelist":
  [
    {
      "name" : "About Blank Page",
      "url" : "about:blank"
    },
    {
      "name" : "Google",
      "url" : "www.google.com"
    },
    {
      "name" : "AAMP CDAI Player",
      "url" : "localhost:50050/AAMPCDAITestApp/index.html"
    }
  ]
}
