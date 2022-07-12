// select dashjs as preferred engine
uve_prepare("dashjs", true);

window.onload = function() {
    // create UVEPlayer instance
    var player = new UVEPlayer("sampleApp");
    player.addEventListener("playbackStarted", mediaPlaybackStarted);
    var DrmConfig = { 'com.widevine.alpha':'https://drm-widevine-licensing.axtest.net/AcquireLicense', 'preferredKeysystem':'com.widevine.alpha'};
    var locator = "https://media.axprod.net/TestVectors/v7-MultiDRM-SingleKey/Manifest_1080p.mpd"; //DRM Encrypted content
    player.setDRMConfig(DrmConfig);
    player.addCustomHTTPHeader("X-AxDRM-Message", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ2ZXJzaW9uIjoxLCJjb21fa2V5X2lkIjoiYjMzNjRlYjUtNTFmNi00YWUzLThjOTgtMzNjZWQ1ZTMxYzc4IiwibWVzc2FnZSI6eyJ0eXBlIjoiZW50aXRsZW1lbnRfbWVzc2FnZSIsImZpcnN0X3BsYXlfZXhwaXJhdGlvbiI6NjAsInBsYXlyZWFkeSI6eyJyZWFsX3RpbWVfZXhwaXJhdGlvbiI6dHJ1ZX0sImtleXMiOlt7ImlkIjoiOWViNDA1MGQtZTQ0Yi00ODAyLTkzMmUtMjdkNzUwODNlMjY2IiwiZW5jcnlwdGVkX2tleSI6ImxLM09qSExZVzI0Y3Iya3RSNzRmbnc9PSJ9XX19.FAbIiPxX8BHi9RwfzD7Yn-wugU19ghrkBFKsaCPrZmU", true);
    player.load(locator);
}

function mediaPlaybackStarted() {
    console.log("Playback Event Received");
}
