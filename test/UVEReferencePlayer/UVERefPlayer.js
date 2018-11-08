/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

window.onload = function() {

    var playerStatesEnum = { "idle":0, "initializing":1, "playing":8, "paused":6, "seeking":7 };
    Object.freeze(playerStatesEnum);
    var playbackSpeeds = [-64, -32, -16, -4, 1, 4, 16, 32, 64];
    /*var urls = [
        "http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8",
        "http://cilhlsvod.akamaized.net/i/506629/MP4/demo3/abcd123/,cea708test,.mp4.csmil/master.m3u8",
        "http://playertest.longtailvideo.com/adaptive/wowzaid3/playlist.m3u8",
        "http://amssamples.streaming.mediaservices.windows.net/683f7e47-bd83-4427-b0a3-26a6c4547782/BigBuckBunny.ism/manifest(format=mpd-time-csf)",
        "http://cilhlsvod.akamaized.net/i/506629/MP4/Production/083fd/OR_CholosTryS2_ep208_BountyHunting_Final_16x93710_68f8d(175650_R21MP4,5000),4000),3000),2000),1500),1000),500),350),64),.mp4.csmil/master.m3u8?hdnea=st=1503082666~exp=1503082726~acl=/i/506629/MP4/Production/083fd/OR_CholosTryS2_ep208_BountyHunting_Final_16x93710_68f8d(175650_R21MP4,5000),4000),3000),2000),1500),1000)"
    ];*/

    var urls = [
        "http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8",
        "http://playertest.longtailvideo.com/adaptive/wowzaid3/playlist.m3u8",
        "http://amssamples.streaming.mediaservices.windows.net/683f7e47-bd83-4427-b0a3-26a6c4547782/BigBuckBunny.ism/manifest(format=mpd-time-csf)",
    ];

    function createBitrateList(availableBitrates) {
        for (var iter = 0; iter < availableBitrates.length; iter++) {
            var option = document.createElement("option");
            option.value = availableBitrates[iter];
            option.text = Math.round(availableBitrates[iter]/1000) + "kbps";
            settingButton.add(option);
        }
    }

    function showTrickmodeOverlay(speed) {
        document.getElementById('ffSpeed').innerHTML = Math.abs(speed)+ "x";
        if (speed > 0) {
            document.getElementById('ffModal').style["-webkit-transform"]= "scaleX(1)";
        } else {
            document.getElementById('ffModal').style["-webkit-transform"]= "scaleX(-1)";
        }

        //Display Fast Forward modal
        document.getElementById('ffModal').style.display = "block";
        document.getElementById('ffSpeed').style.display = "block";

        //Set timeout to hide
        setTimeout(function() {
            document.getElementById('ffModal').style.display = "none";
            document.getElementById('ffSpeed').style.display = "none";
        }, 2000);
    }

    function bufferingDisplay(buffering) {
        // Get the buffer div
        var bufferShow = document.getElementById("bufferWidget");
        if (buffering === true) {
            bufferShow.style.display = "block";
        } else {
            bufferShow.style.display = "none";
        }
    };

    /*var AAMPMediaPlayer = {
        addEventListener: function() {
            console.log("Invoked addEventListener");
        },
        load: function(url) {
            console.log("Invoked load"+ url);
        },
        stop: function() {
            console.log("Invoked stop");
        },
        play: function() {
            console.log("Invoked play");
        },
        setPlaybackRate: function() {
            console.log("Invoked setPlaybackRate");
        },
        setVideoMute: function() {
            console.log("Invoked setVideoMute");
        },
        seek: function() {
            console.log("Invoked seek");
        },
        getDurationSec: function() {
            console.log("Invoked getDurationSec");
        },
        getCurrentPosition: function() {
            console.log("Invoked getCurrentPosition");
        },
        pause: function() {
            console.log("Invoked pause");
        },
        setVolume: function() {
            console.log("Invoked setVolume");
        },
        getVideoBitrates: function() {
            console.log("getVideoBitrates");
        },
        setVideoBitrate: function() {
            console.log("setVideoBitrate");
        }
    };*/

    var mediaPlayer = null;
    var playerState = playerStatesEnum.idle;
    var playbackRateIndex = playbackSpeeds.indexOf(1);
    var urlIndex = 0;
    var mutedStatus = false;

    // Buttons
    var playButton = document.getElementById("playOrPauseButton");
    var muteButton = document.getElementById("muteVideoButton");
    var rwdButton = document.getElementById("rewindButton");
    var skipBwdButton = document.getElementById("skipBackwardButton");
    var skipFwdButton = document.getElementById("skipForwardButton");
    var fwdButton = document.getElementById("fastForwardButton");
    var helpContentButton = document.getElementById('helpButton');
    var rwdButton = document.getElementById("rewindButton");
    var skipBwdButton = document.getElementById("skipBackwardButton");
    var skipFwdButton = document.getElementById("skipForwardButton");
    var fwdButton = document.getElementById("fastForwardButton");

    // Sliders
    var seekBar = document.getElementById("seekBar");

    var loadFile = document.getElementById("loadButton");
    var deleteFile = document.getElementById("deleteButton");

    //to show the navBar initially
    var navBar = document.getElementById('controlDiv');
    navBar.style.display = "block";
    var navBarNext = document.getElementById('controlDivNext');
    navBarNext.style.display = "block";

    //var availableBitrates = null;
    resetAAMPMediaPlayer();
    resetUIOnNewAsset();
    mediaPlayer.load(urls[urlIndex]);

    //to refreash URL select field on every reload
    var storedURLs = JSON.parse(sessionStorage.getItem("URLItems"));
    if(storedURLs) {
        //to remove all current options of videoURLs
        while (videoURLs.firstChild) {
            videoURLs.removeChild(videoURLs.firstChild);
        }
        //iteratively adding all the options in session storage to videoURLs
        var option = document.createElement("option");
        option.disabled = true;
        option.value = "";
        option.text = "Select video URL:";
        videoURLs.add(option);

        for (var iter = 0; iter < storedURLs.length; iter++) {
            var option = document.createElement("option");
            option.value = storedURLs[iter];
            option.text = storedURLs[iter];
            videoURLs.add(option);
        }
    }

    function playbackStateChanged(event) {
        console.log("Playback state changed event: " + JSON.stringify(event));
        switch (event.state) {
            case playerStatesEnum.idle:
                playerState = playerStatesEnum.idle;
                break;
            case playerStatesEnum.initializing:
                playerState = playerStatesEnum.initializing;
                break;
            case playerStatesEnum.playing:
                playerState = playerStatesEnum.playing;
                break;
            case playerStatesEnum.paused:
                playerState = playerStatesEnum.paused;
                break;
            case playerStatesEnum.seeking:
                playerState = playerStatesEnum.seeking;
                break;
            default:
                console.log("State not expected");
                break;
        }
        console.log("Player state is: " + playerState);
    }

    function mediaEndReached() {
        console.log("Media end reached event!");
        loadNextAsset();
    }

    function mediaSpeedChanged(event) {
        console.log("Media speed changed event: " + JSON.stringify(event));
        var currentRate = event.speed;
        console.log("Speed Changed [" + playbackSpeeds[playbackRateIndex] + "] -> [" + currentRate + "]");
        if (currentRate != undefined) {
            if (currentRate != 0) {
                playbackRateIndex = playbackSpeeds.indexOf(currentRate);
            } else {
                playbackRateIndex = playbackSpeeds.indexOf(1);
            }
            if (currentRate != 0 && currentRate != 1){
                showTrickmodeOverlay(currentRate);
            }

            if (currentRate === 1) {
                document.getElementById("playOrPauseIcon").src = "icons/pause.png";
                document.getElementById("playState").innerHTML = "PLAYING";
            } else {
                document.getElementById("playOrPauseIcon").src = "icons/play.png";
                if (currentRate === 0) {
                    document.getElementById("playState").innerHTML = "PAUSED";
                } else if (currentRate > 1) {
                    document.getElementById("playState").innerHTML = "FAST FORWARD";
                } else if (currentRate < 0) {
                    document.getElementById("playState").innerHTML = "FAST REWIND";
                }
            }
            document.getElementById("speedX").innerHTML = currentRate;
        }
    }

    function mediaPlaybackFailed(event) {
        console.log("Media failed event: " + JSON.stringify(event));
        loadNextAsset();
    }

    function mediaMetadataParsed(event) {
        console.log("Media metadata event: " + JSON.stringify(event));
    }

    function subscribedTagNotifier(event) {
        console.log("Subscribed tag notifier event: " + JSON.stringify(event));
    }

    function mediaProgressUpdate(event) {
        //console.log("Media progress update event: " + JSON.stringify(event));
        var duration = event.durationMiliseconds;
        var position = event.positionMiliseconds;
        var value = ( position / duration ) * 100;
        document.getElementById("totalDuration").innerHTML = convertSStoHr(duration / 1000.0);
        document.getElementById("currentDuration").innerHTML = convertSStoHr(position / 1000.0);

        // Update the slider value
        if(isFinite(value)) {
            seekBar.value = value;
            seekBar.style.width = value+"%";
            seekBar.style.backgroundColor = "red";
        }
    }

    function mediaPlaybackStarted() {
        bufferingDisplay(false);
        document.getElementById("playOrPauseIcon").src = "icons/pause.png";
        document.getElementById("playState").innerHTML = "PLAYING";

        var availableVBitrates = mediaPlayer.getVideoBitrates();
        if (availableVBitrates !== undefined) {
            createBitrateList(availableVBitrates);
        }
    }

    function mediaPlaybackBuffering(event) {
        if (event.buffering === true){
            bufferingDisplay(true);
        } else {
            bufferingDisplay(false);
        }
    }

    function mediaDurationChanged(event) {
        console.log("Duration changed!");
    }

    // helper functions
    function resetAAMPMediaPlayer() {
        if (playerState !== playerStatesEnum.idle) {
            mediaPlayer.stop();
        }
        mediaPlayer = null;
        //mediaPlayer = AAMPMediaPlayer;
        mediaPlayer = new AAMPMediaPlayer();
        mediaPlayer.addEventListener("playbackStateChanged", playbackStateChanged, null);
        mediaPlayer.addEventListener("playbackCompleted", mediaEndReached, null);
        mediaPlayer.addEventListener("playbackSpeedChanged", mediaSpeedChanged, null);
        mediaPlayer.addEventListener("playbackFailed", mediaPlaybackFailed, null);
        mediaPlayer.addEventListener("mediaMetadata", mediaMetadataParsed, null);
        mediaPlayer.addEventListener("timedMetadata", subscribedTagNotifier, null);
        mediaPlayer.addEventListener("playbackProgressUpdate", mediaProgressUpdate, null);
        mediaPlayer.addEventListener("playbackStarted", mediaPlaybackStarted, null);
        mediaPlayer.addEventListener("bufferingChanged", mediaPlaybackBuffering, null);
        mediaPlayer.addEventListener("durationChanged", mediaDurationChanged, null);
        playerState = playerStatesEnum.idle;
        mutedStatus = false;
    }

    function playPause() {
        console.log("playPause");
        // If it was a trick play operation
        if ( playbackSpeeds[playbackRateIndex] != 1 ) {
            // Change to normal speed
            mediaPlayer.play();
        } else {
            if (playerState === playerStatesEnum.paused) {
                // Play the video
                mediaPlayer.play();
            } else { // Pause the video
                mediaPlayer.pause();
            }
        }
        playbackRateIndex = playbackSpeeds.indexOf(1);
    };

    function mutePlayer() {
        if (mutedStatus === false) {
            // Mute
            mediaPlayer.setVolume(0);
            mutedStatus = true;
            document.getElementById("muteIcon").src = "icons/mute.png";
        } else {
            // Unmute
            mediaPlayer.setVolume(100);
            mutedStatus = false;
            document.getElementById("muteIcon").src = "icons/unMute.png";
        }
    };

    function skipTime(tValue) {
        //if no video is loaded, this throws an exception
        try {
            var position = mediaPlayer.getCurrentPosition();
            if (!isNaN(position)) {
                mediaPlayer.seek(position + tValue);
            }
        } catch (err) {
            // errMessage(err) // show exception
            errMessage("Video content might not be loaded: " + err);
        }
    }

    // Event listener for the play/pause button
    playButton.addEventListener("click", function() {
        playPause();
    });

    // show listener for the
    helpContentButton.addEventListener("click", function() {
        showhide();
    });

    // Event listener for the mute button
    muteButton.addEventListener("click", function() {
        mutePlayer();
    });

    function showhide() {
        // Get the modal
        var modal = document.getElementById('myModal');
        modal.style.display = "block";
        setTimeout(function() {
            modal.style.display = "none";
        }, 5000);
    };

    function skipBackward() {
        skipTime(-10);
    };

    function skipForward() {
        skipTime(10);
    };

    function fastrwd() {
        var newSpeedIndex = playbackRateIndex - 1;
        if (newSpeedIndex < 0) {
            newSpeedIndex = 0;
        }
        if (newSpeedIndex !== playbackRateIndex) {
            console.log("Change speed from [" + playbackSpeeds[playbackRateIndex] + "] -> [" + playbackSpeeds[newSpeedIndex] + "]");
            mediaPlayer.setPlaybackRate(playbackSpeeds[newSpeedIndex]);
        }
    };

    function fastfwd() {
        var newSpeedIndex = playbackRateIndex + 1;
        if (newSpeedIndex >= playbackSpeeds.length) {
            newSpeedIndex = playbackSpeeds.length - 1;
        }
        if (newSpeedIndex !== playbackRateIndex) {
            console.log("Change speed from [" + playbackSpeeds[playbackRateIndex] + "] -> [" + playbackSpeeds[newSpeedIndex] + "]");
            mediaPlayer.setPlaybackRate(playbackSpeeds[newSpeedIndex]);
        }
    };

    function overlayDisplay() {
        // Get the modal
        var modal = document.getElementById('overlayModal');
        //modal.style.display = "block";
        if(modal.style.display == "block") {
            modal.style.display = "none";
        } else {
            modal.style.display = "block";
        }
    };

    function overlayController() {
        // Get the modal
        if(navBar.style.display == "block") {
            navBar.style.display = "none";
        } else {
            navBar.style.display = "block";
        }
        if(navBarNext.style.display == "block") {
            navBarNext.style.display = "none";
        } else {
            navBarNext.style.display = "block";
        }
    };

    // Event listener for the rewind button
    rwdButton.addEventListener("click", function() {
        fastrwd();
    });

    // Event listener for the skip Backward button
    skipBwdButton.addEventListener("click", function() {
        skipBackward();
    });

    // Event listener for the skip Forward button
    skipFwdButton.addEventListener("click", function() {
        skipForward();
    });

    // Event listener for the fast Forward button
    fwdButton.addEventListener("click", function() {
        fastfwd();
    });

    // Event listener for the seek bar
    seekBar.addEventListener("change", function() {
        // Calculate the new time
        var duration = mediaPlayer.getDurationSec();
        var time = duration * (seekBar.value / 100);
        console.log("seek cursor time: " + time);
        mediaPlayer.seek(time);
    });

    // Pause the video when the seek handle is being dragged
    seekBar.addEventListener("keydown", function() {
        mediaPlayer.pause();
    });

    // Play the video when the seek handle is dropped
    seekBar.addEventListener("keyup", function() {
        mediaPlayer.play();
    });

    //  set src == latest video file URL
    document.getElementById("loadButton").addEventListener("click", getVideo, false);

    //  load video file from select field
    function getVideo() {
        var fileURLContent = document.getElementById("videoURLs").value; // get select field
        if (fileURLContent != "") {
            var newFileURLContent = fileURLContent;
            //video.src = newFileURLContent;
            //get the selected index of the URL List
            var selectedURL = document.getElementById("videoURLs");
            var optionIndex = selectedURL.selectedIndex;
            //set the index to the selected field
            document.getElementById("videoURLs").selectedIndex = optionIndex;

            console.log("Loading:" + newFileURLContent);
            resetAAMPMediaPlayer();
            resetUIOnNewAsset();
            mediaPlayer.load(newFileURLContent);

            console.log("After Load:" + newFileURLContent);
        } else {
            errMessage("Enter a valid video URL"); // fail silently
        }
    }

    function deleteVideo() {
        var fileURL = document.getElementById("videoURLs").value; // get select field
        if (fileURL != "") {
            var delURL = document.getElementById("videoURLs");
            var selectPos = delURL.selectedIndex;
            var selectOption = delURL.options;
            //"Index: " + selectOption[selectPos].index + " is " + selectOption[selectPos].text
            var storedURLs = JSON.parse(sessionStorage.getItem("URLItems"));
            storedURLs.splice(selectPos,1);
            sessionStorage.setItem("URLItems", JSON.stringify(storedURLs));
            delURL.remove(delURL.selectedIndex);
            //Display Delete modal
            var delModal = document.getElementById('deleteModal');
            document.getElementById("deleteContent").innerHTML = fileURL;
            delModal.style.display = "block";
            setTimeout(function() {
                delModal.style.display = "none";
            }, 3000);
        }
    }

    function loadNextAsset() {
        resetAAMPMediaPlayer();
        resetUIOnNewAsset();
        urlIndex++;
        if (urlIndex >= urls.length) {
            urlIndex = 0;
        }
        mediaPlayer.load(urls[urlIndex]);
    }

    function loadPrevAsset() {
        resetAAMPMediaPlayer();
        resetUIOnNewAsset();
        urlIndex--;
        if (urlIndex < 0) {
            urlIndex = urls.length - 1;
        }
        mediaPlayer.load(urls[urlIndex]);
    }

    function resetUIOnNewAsset(){
        document.getElementById("playOrPauseIcon").src = "icons/play.png";
        document.getElementById("playState").innerHTML = "PAUSED";
        document.getElementById("muteIcon").src = "icons/unMute.png";
        document.getElementById("currentDuration").innerHTML = "00:00:00";
        document.getElementById("totalDuration").innerHTML = "00:00:00";
        document.getElementById('ffSpeed').innerHTML = "";
        document.getElementById('ffModal').style.display = "none";
        document.getElementById('ffSpeed').style.display = "none";

        var value = 0;
        seekBar.value = value;
        seekBar.style.width = value+"%";
        seekBar.style.backgroundColor = "red";

        //Reset bitrate list
        var bitrateList = document.getElementById("settingButton");
        bitrateList.innerHTML = "";
        var option2 = document.createElement("option");
        option2.value = "0";
        option2.text = "Auto";
        option2.selected = true;
        bitrateList.add(option2);

        bufferingDisplay(true);
    }

    var HTML5Player = function() {
        var that = this;
        this.init = function() {
            this.video = document.getElementById("video");

            // Buttons
            this.playButton = document.getElementById("playOrPauseButton");
            this.rwdButton = document.getElementById("rewindButton");
            this.skipBwdButton = document.getElementById("skipBackwardButton");
            this.skipFwdButton = document.getElementById("skipForwardButton");
            this.fwdButton = document.getElementById("fastForwardButton");
            this.muteButton = document.getElementById("muteVideoButton");
            this.setButton = document.getElementById("settingButton");

            // Sliders
            this.seekBar = document.getElementById("seekBar");
            this.videoFileList = document.getElementById("videoURLs");
            this.loadVideoButton = document.getElementById("loadButton");
            this.deleteVideoButton = document.getElementById("deleteButton");
            this.helpContentButton = document.getElementById('helpButton');

            this.currentObj = this.playButton;
            this.components = [this.playButton, this.rwdButton, this.skipBwdButton, this.skipFwdButton, this.fwdButton, this.muteButton, this.setButton, this.videoFileList, this.loadVideoButton, this.deleteVideoButton, this.helpContentButton];
            this.currentPos = 0;
            this.dropDownListVisible = false;
            this.dropDownBitrateListVisible = false;
            this.selectListIndex = 0;
            this.selectBitrateListIndex = 0;
            this.prevObj = null;
            this.addFocus();
        };

        this.reset = function() {
        };

        this.keyLeft = function() {
            this.gotoPrevious();
        };

        this.keyRight = function() {
            this.gotoNext();
        };

        this.keyUp = function() {
            if (this.dropDownListVisible) {
                this.prevVideoSelect();
            } else if (this.dropDownBitrateListVisible) {
                this.prevBitrateSelect();
            } else if ((this.components[this.currentPos] == this.playButton) || (this.components[this.currentPos] == this.rwdButton) || (this.components[this.currentPos] == this.skipBwdButton) || (this.components[this.currentPos] == this.skipFwdButton) || (this.components[this.currentPos] == this.fwdButton) || (this.components[this.currentPos] == this.muteButton)) {
                //when a keyUp is received from the buttons in the bottom navigation bar
                this.removeFocus();
                this.currentObj = this.setButton;
                //move focus to the first element in the top navigation bar
                this.currentPos = this.components.indexOf(this.setButton);
                this.addFocus();
            }
        };

        this.keyDown = function() {
            if (this.dropDownListVisible) {
                this.nextVideoSelect();
            } else if (this.dropDownBitrateListVisible) {
                this.nextBitrateSelect();
            } else if ((this.components[this.currentPos] == this.setButton) || (this.components[this.currentPos] == this.videoFileList) || (this.components[this.currentPos] == this.loadVideoButton) || (this.components[this.currentPos] == this.deleteVideoButton) || (this.components[this.currentPos] == this.helpContentButton)) {
                //when a keyDown is received from the buttons in the top navigation bar
                this.removeFocus();
                this.currentObj = this.playButton;
                //move focus to the first element in the bottom navigation bar
                this.currentPos = 0;
                this.addFocus();
            }
        };

        this.prevVideoSelect = function() {
            if (this.selectListIndex > 0) {
                this.selectListIndex--;
            } else {
                this.selectListIndex = this.videoFileList.options.length - 1;
            }
            this.videoFileList.options[this.selectListIndex].selected = true;
        };

        this.nextVideoSelect = function() {
            if (this.selectListIndex < this.videoFileList.options.length - 1) {
                this.selectListIndex++;
            } else {
                this.selectListIndex = 0;
            }
            this.videoFileList.options[this.selectListIndex].selected = true;
        };

        this.prevBitrateSelect = function() {
            if (this.selectBitrateListIndex > 0) {
                this.selectBitrateListIndex--;
            } else {
                this.selectBitrateListIndex = this.setButton.options.length - 1;
            }
            this.setButton.options[this.selectBitrateListIndex].selected = true;
        };

        this.nextBitrateSelect = function() {
            if (this.selectBitrateListIndex < this.setButton.options.length - 1) {
                this.selectBitrateListIndex++;
            } else {
                this.selectBitrateListIndex = 0;
            }
            this.setButton.options[this.selectBitrateListIndex].selected = true;
        };

        this.showDropDown = function() {
            this.dropDownListVisible = true;
            var n = this.videoFileList.options.length;
            this.videoFileList.size = n;
        };

        this.hideDropDown = function() {
            this.dropDownListVisible = false;
            this.videoFileList.size = 1;
        };

        this.showBitrateDropDown = function() {
            this.dropDownBitrateListVisible = true;
            var n = this.setButton.options.length;
            this.setButton.size = n;
        };

        this.hideBitrateDropDown = function() {
            this.dropDownBitrateListVisible = false;
            this.setButton.size = 1;
            var newBitrate = document.getElementById("settingButton").value;
            if (newBitrate != "") {
                console.log("bitrate: "  + newBitrate)
                mediaPlayer.setVideoBitrate(newBitrate);
            }
        };

        this.ok = function() {
            switch (this.currentPos) {
                case 0:
                        playPause();
                        break;
                case 1:
                        fastrwd();
                        break;
                case 2:
                        skipBackward();
                        break;
                case 3:
                        skipForward();
                        break;
                case 4:
                        fastfwd();
                        break;
                case 5:
                        mutePlayer();
                        break;
                case 6:
                        if (this.dropDownBitrateListVisible == false) {
                            this.showBitrateDropDown();
                        } else {
                            this.hideBitrateDropDown();
                        }
                        break;
                case 7:
                        if (this.dropDownListVisible == false) {
                            this.showDropDown();
                        } else {
                            this.hideDropDown();
                        }
                        break;
                case 8:
                        getVideo();
                        break;
                case 9:
                        deleteVideo();
                        break;
                case 10:
                        showhide();
                        break;
            };
        };

        this.gotoNext = function() {
            this.removeFocus();
            if (this.currentPos < this.components.length - 1) {
                this.currentPos++;
            } else {
                this.currentPos = 0;
            }
            this.currentObj = this.components[this.currentPos];
            this.addFocus();
        };

        this.gotoPrevious = function() {
            this.removeFocus();
            if (this.currentPos > 0) {
                this.currentPos--;
            } else {
                this.currentPos = this.components.length - 1;
            }
            this.currentObj = this.components[this.currentPos];
            this.addFocus();
        };

        this.addFocus = function() {
            if (this.currentObj) {
                this.currentObj.classList.add("focus");
            } else {
                this.currentObj.focus();
            }
        };

        this.removeFocus = function() {
            if (this.currentObj) {
                this.currentObj.classList.remove("focus");
            } else {
                this.currentObj.blur();
            }
        };

        this.keyEventHandler = function(e, type) {
            var keyCode = e.which || e.keyCode;
            console.log(keyCode);
            e.preventDefault();
            if (type == "keydown") {
                switch (keyCode) {
                    case 37: // Left Arrow
                            this.keyLeft();
                            break;
                    case 38: // Up Arrow
                            this.keyUp();
                            break;
                    case 39: // Right Arrow
                            this.keyRight();
                            break;
                    case 40: // Down Arrow
                            this.keyDown();
                            break;
                    case 13: // Enter
                            this.ok();
                            break;
                    case 88: // X
                            skipBackward();
                            break;
                    case 90: // Z
                            skipForward();
                            break;
                    case 32:
                            this.ok();
                            break;
                    case 80: // P
                            playPause();
                            break;
                    case 113: // F2
                            mutePlayer();
                            break;
                    case 114: // F3
                            showhide();
                            break;
                    case 82: // R
                            fastrwd();
                            break;
                    case 70: // F
                            fastfwd();
                            break;
                    case 117: // F6
                            overlayDisplay();
                            break;
                    case 118: // F7
                            overlayController();
                            break;
                    case 85: // U
                            loadNextAsset();
                            break;
                    case 68: // D
                            loadPrevAsset();
                            break;
                    default:
                            break;
                }
            }
            return false;
        }
    };

    var playerObj = new HTML5Player();
    playerObj.init();
    if (document.addEventListener) {
        document.addEventListener("keydown", function(e) {
            return playerObj.keyEventHandler(e, "keydown");
        });
    }
}

function convertSStoHr(videoTime) {
    var hhTime = Math.floor(videoTime / 3600);
    var mmTime = Math.floor((videoTime - (hhTime * 3600)) / 60);
    var ssTime = videoTime - (hhTime * 3600) - (mmTime * 60);
    ssTime = Math.round(ssTime);

    var timeFormat = (hhTime < 10 ? "0" + hhTime : hhTime);
        timeFormat += ":" + (mmTime < 10 ? "0" + mmTime : mmTime);
        timeFormat += ":" + (ssTime  < 10 ? "0" + ssTime : ssTime);

    return timeFormat;
}
