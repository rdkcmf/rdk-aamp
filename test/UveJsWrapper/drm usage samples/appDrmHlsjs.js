// select hlsjs as preferred engine
uve_prepare("hlsjs" , true);

window.onload = function() {
    // create UVEPlayer instance
    var player = new UVEPlayer("sampleApp");
    player.addEventListener("playbackStarted", mediaPlaybackStarted);
    var DrmConfig = { 'com.widevine.alpha':'https://cwip-shaka-proxy.appspot.com/no_auth', 'preferredKeysystem':'com.widevine.alpha'};
    var locator = "https://storage.googleapis.com/shaka-demo-assets/angel-one-widevine-hls/hls.m3u8"; //DRM Encrypted content
    player.setDRMConfig(DrmConfig);
    player.load(locator);
}

function mediaPlaybackStarted() {
    console.log("Playback Event Received");
}
