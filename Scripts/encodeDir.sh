#!/bin/sh
#
# encodeDir.sh
# ndt: n-dimensional tracer
#
# Copyright (c) 2014-2021 Bryan Franklin. All rights reserved.

if [ -z "$1" ]; then
    echo "Usage:"
    echo "  $0 directory [fps] [options]"
    exit 1
fi

FPS=24
if [ ! -z $2 ]; then
    FPS=$2
fi

DIR=`echo "$1" | sed 's/[\/.]*$//'`
PARENT=$(basename $(dirname $DIR))
PARENT2=$(basename $(dirname $(dirname $DIR)))
NAME=`basename $DIR`
if [ "$PARENT2" != "images" ]; then
    VIDEO=videos/${PARENT2}_${PARENT}_${NAME}_${FPS}fps.avi
elif [ "$PARENT" != "images" ]; then
    VIDEO=videos/${PARENT}_${NAME}_${FPS}fps.avi
else
    VIDEO=videos/${NAME}_${FPS}fps.avi
fi

# remove processed command-line options
shift 2
if [ ! -z "$*" ]; then
    echo "ffmpeg options: $*"
fi

echo $VIDEO

mkdir -p videos
rm $VIDEO
#mencoder "mf://$DIR/*" -mf fps=$FPS -o $VIDEO -ovc lavc -lavcopts vcodec=libx264
#mencoder "mf://$DIR/*.png" -mf fps=$FPS -o $VIDEO -ovc lavc -lavcopts vcodec=mpeg4:mbd=2:trell=yes:v4mv=yes
# see: http://www.mplayerhq.hu/DOCS/HTML/en/menc-feat-x264.html
#mencoder "mf://$DIR/*.png" -mf fps=$FPS -o $VIDEO -ovc lavc -x264encopts slow_firstpass:pass=2:subq=6:partitions=all:8x8dct:me=umh:frameref=5:bframes=3:b_pyramid=normal:weight_b:vbv_maxrate=40000
#mplayer $VIDEO

# see: https://wiki.archlinux.org/index.php/MEncoder#Two-pass_x264_(very_high-quality)
mencoder "mf://$DIR/*.png" -mf fps=$FPS -oac copy -ovc x264 -x264encopts pass=1:preset=veryslow:fast_pskip=0:tune=film:frameref=15:bitrate=3000:threads=auto -o /dev/null
mencoder "mf://$DIR/*.png" -mf fps=$FPS -oac copy -ovc x264 -x264encopts pass=2:preset=veryslow:fast_pskip=0:tune=film:frameref=15:bitrate=3000:threads=auto -o $VIDEO

MP4=`echo $VIDEO | sed 's/.avi$/.mp4/'`
test -f "$MP4" && rm -f "$MP4"
echo "ffmpeg -i $VIDEO $* $MP4"
ffmpeg -i "$VIDEO" $* "$MP4"

ls -l $MP4
