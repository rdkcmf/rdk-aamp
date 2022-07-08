# Auto Triage Tool
Auto triage tool is a web utility for log analysis. It takes set-top-box logs as input, parse it, and provide detailed analysis along with pictorial representations after breaking down tune time. It also has the provision to set user defined markers for the logs and to export IP_TUNE_TIME data.


## Basic Usage

1. Open the index.html in a browser(eg. Chrome).

2. Once launched, allow a pop-up for the site in the browser (which blocks the log parser tab)

3. Reload the webpage and a blank tab for log parser will also appear.

4. Click "Upload Markers" button to upload custom user defined marker.

5. Click on the Upload Log" button to  upload one or more log files. Visualization and log viewing window will be populated.
Visualization includes timeline with download activity broken down by track type, and below set of 'markers' based on configurations matchcing patterns to logs of interest.

6. Drag up/down in log window, or left/right in visualization window to browse.

7. The "Session" pulldown can be used to browser to different logical tunes.

8. The "Marker" pulldown can be used to jump to a specific marker.

9. Click on a log window row to auto-scroll visualization window.

10. Click on a visualization window download box or marker to auto-scroll log window.

11. Control or command click to get details for download or marker in an alert.  This is useful when one wants to copy-paste details.

11. Optional "Export Statistics" button extracts all IP_AAMP_TUNETIME logging to an .csv file, along with column headers and tune url.  For tunes to linear through FOG, the locator is unpacked from recordedUrl uri parameter.


## Implementation Details
- Uses HTML canvas for fast rendering. draggger.js implements dragging momentum and animation support, common to both windows.  Includes parameters that could be tuned for better drag momentum if desired.
- Currently all markers are defined in a collection of .js files under "markers" subdirectory.


## Notes
- Tool is currently leveraging HTML postMessage command to send messagages between main visualization window and log viewer window. In order for this to work (behind web security), pop up blocker neeeds to disabled.
- as visualization and log window is rendered using canvas, doesn't support control+F / find commands from browser.  This could be done in software without much fuss, but would need UI.


## Areas of Improvements
- Nice if we can make two-finger touch pan work on OSX, not just plain mouse down/drag.
- Better to have markers driven by log level? Anything logged with "WARN" or "ERROR" level logically could/should be marker-worthy.
- "LOG_WARN" sounds bad, but we use it for critical normal logworthy activities, too.  Worth distinguishing between "CRITICAL" "WARN" and "ERROR" log levels, all of would would implicitly result in markers, but with distinct coloring?
- Some log entries use numeric enums (i.e. SendEventSync), that could be expanded to human readable text (or logged as such)
- Could use better distinction between init and normal fragments in timeline (color difference may be too subtle)
- Need to explore implications from background tunes.
- Could be useful to (efficiently!) re-introduce support for filtering log view to aamp-only logs.
- Could be useful to support checkbox options to hide/show subset off markers.

- Need to check the progress position with timeline time.
- Provision to copy HTTP response data.
- Option to upload tar/zip file directly with our extracting it or a folder.
- Text provision to add user-defined markers.
- Add XRE receiver side markers, rash/Hang markers.
- Backtrace analysis from the tool.
- Scroll bar support in both pages.
- Sometimes the start of a session would be in one log file, split from the rest of the session. It would be useful if importing both of these would allow it to be recognised as one session (perhaps based off of the timestamps).
- It may be handy to have the breakdown seen in the logs reflected in the UI (eg: showing as a tooltip, similar to in the X1 debugger). If possible this could be a good place to show time taken for security handshake, DNS etc.
- Avoid extra overhead of allowing pop-up in browser for log viewer.
- Add provision to click on the markers.
- Fix issue with pattern matching in markers. (eg: "PLAYER[%%] Player BACKGROUND=>FOREGROUND" matching even if only "PLAYER[%%]" is present)
- It would be great to have user profiles with user defined markers saved.
- Add  line wrapping, search, and text selection from logger window.
