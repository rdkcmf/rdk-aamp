WIDTH=(640 842 1280 1920)
HEIGHT=(360 480 720 1080)
KBPS=(800 1400 2800 5000)
MAXKBPS=(856 1498 2996 5350)
# recreation of main DASH manifest is not supported yet
# do it manually by merging all *.mpd files into one main.mpd
VIDEO_LENGTH_SEC=900
FPS=25 
VIDEO_SEGMENT_SEC=2
IFRAME_CADENCE_SEC=1

HLS_OUT="hls"
DASH_OUT="dash"

mkdir $HLS_OUT
mkdir $DASH_OUT

# convert single image to video; "t" is target duration in seconds
FILE_LOOP=loop.mp4
if [ -f "$FILE_LOOP" ]; then
	echo "$FILE_LOOP exists"
else
	ffmpeg -loop 1 -i testpat.jpg -c:v libx264 -t $VIDEO_LENGTH_SEC -pix_fmt yuv420p -vf scale=1920:1080 -r $FPS $FILE_LOOP
fi

# generate profiles
for I in {0..3} 
do
# add resolution and current-time as overlay
FILE_OVERLAY=overlay${HEIGHT[$I]}.mp4
if [ -f "$FILE_OVERLAY" ]; then
	echo "$FILE_OVERLAY exists"
else
ffmpeg -i loop.mp4 -vf \
drawtext="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2",\
drawtext="fontfile=/path/to/font.ttf: text='${HEIGHT[$I]}p': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2" \
-codec:a copy $FILE_OVERLAY
fi
ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}.mp4 -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease  -c:a aac -ar 48000 -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -b:a 96k -min_seg_duration $((VIDEO_SEGMENT_SEC*1000000)) -use_timeline 1 -use_template 1 -init_seg_name $DASH_OUT/${HEIGHT[$I]}p_init.m4s -media_seg_name $DASH_OUT/${HEIGHT[$I]}p_'$Number%03d$.m4s'  -f dash ${HEIGHT[$I]}p.mpd
# generate fragmented HLS
ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}.mp4 -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease -c:a aac -ar 48000 -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -hls_time ${VIDEO_SEGMENT_SEC} -hls_playlist_type vod  -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -b:a 96k -hls_segment_filename $HLS_OUT/${HEIGHT[$I]}p_%03d.ts $HLS_OUT/${HEIGHT[$I]}p.m3u8
done

# TODO: merge all .mpd manifests into one main.mpd
# TODO: merge all .m3u8 manifests into one main.m3u8
​
# "IFRAME" text overlay for iframe track
I=0
FILE_OVERLAY=overlayiframe.mp4
ffmpeg -i loop.mp4 -vf \
drawtext="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2",\
drawtext="fontfile=/path/to/font.ttf: text='IFRAME ${HEIGHT[$I]}p': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2" \
-codec:a copy -g $((FPS*IFRAME_CADENCE_SEC)) $FILE_OVERLAY
# single segment duration: IFRAME_CADENCE_SEC
# single segment FPS: 1/IFRAME_CADENCE_SEC
# what gives one I-Frame per segment and nothing else
# generate iframe fragments and playlist for hls
ffmpeg -hide_banner -y -i overlayiframe.mp4 -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:a aac -ar 48000 -c:v h264 -profile:v main -r $((1/IFRAME_CADENCE_SEC)) -crf 20 -sc_threshold 0 -g 1 -hls_time $IFRAME_CADENCE_SEC -hls_playlist_type vod  -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -b:a 96k -hls_segment_filename $HLS_OUT/iframe_%03d.ts  $HLS_OUT/iframe.m3u8
# generate iframe fragments and playlist for dash
ffmpeg -hide_banner -y -i overlayiframe.mp4 -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:a aac -ar 48000 -c:v h264 -profile:v main -r $((1/IFRAME_CADENCE_SEC)) -crf 20 -sc_threshold 0 -g 1 -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -b:a 96k -min_seg_duration $((IFRAME_CADENCE_SEC*1000000)) -use_timeline 1 -use_template 1 -init_seg_name $DASH_OUT/iframe_init.m4s -media_seg_name $DASH_OUT/iframe_'$Number%03d$.m4s'  -f dash iframe.mpd






