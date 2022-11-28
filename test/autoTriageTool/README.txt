Note: tool is currently leveraging HTML postMessage command to send messagages between main visualization window and log viewer window.
In order for this to work (behind web security), the following steps can be used:

1. start local web server using startserver.sh
2. launch (in Chrome or Safari) using http://127.0.0.1:8080/autotriage.html

Note that above won't be needed when/if tool is eventually hosted online.

3. Click "Choose Files" button, then pick one or more log files, i.e. a collection of sky-messages-log.#.log

Visualization and log viewing window will be populated.
Visualization includes timeline with download activity broken down by track type, and below set of 'markers' based on configurations matchcing patterns to logs of interest.

4. drag up/down in log window, or left/right in visualization window to browse.

5. The "Session" pulldown can be used to browser to different logical tunes.

6. The "Marker" pulldown can be used to jump to a specific marker.

7. click on a log window row to auto-scroll visualization window.

8. click on a visualization window download box or marker to auto-scroll log window

9. control or command click to get details for download or marker in an alert.  This is useful when one wants to copy-paste details.

10. Optional "Export Statistics" button extracts all IP_AAMP_TUNETIME logging to an .csv file, along with column headers and tune url.  For tunes to linear through FOG, the locator is unpacked from recordedUrl uri parameter.

Implementation notes & open areas for further improvement:
- uses HTML canvas for fast rendering; draggger.js implements dragging momentum and animation support, common to both windows.  Includes parameters that could be tuned for better drag momentum if desired.
- nice if we can make two-finger touch pan work on OSX, not just plain mouse dow/drag.
- currently all markers are defined in a collection of .js files under "markers" subdirectory.
- Better to have markers driven by log level? Anything logged with "WARN" or "ERROR" level logically could/should be marker-worthy
- "LOG_WARN" sounds bad, but we use it for critical normal logworthy activities, too.  Worth distinguishing between "CRITICAL" "WARN" and "ERROR" log levels, all of would would implicitly result in markers, but with distinct coloring?
- some log entries use numeric enums (i.e. SendEventSync), that could be expanded to human readable text (or logged as such)
- could use better distinction between init and normal fragments in timeline (color differencec may be too subtle)
- could use better indication of bitrate changes; note that having separate rows for video by bitrate wastes sceen real estate, and can lead to confusion due to download activity line not mentioning bitrate (needs to be inferred from preceeding lines)
- need to explore implications from background tunes.
- could be useful to (efficiently!) reintroducec support for filtering log view to aamp-only logs
- could be useful to support checkbox options to hide/show subset off markers
- as visualization and log window is rendered using canvas, doesn't support control+F / find commands from browser.  This could be done in software without much fuss, but would need UI.
- all CSS temporarily disabled; can be reintroduced in cosmetic pass
