#!/bin/sh
#
# frameDup.sh
# ndt: n-dimensional tracer
#
# Copyright (c) 2020 Bryan Franklin. All rights reserved.

# This script is intended to be used when creating an animation that
# ping-pongs, loops, or otherwise reuses sequences of frames.
# example:
# frameDup.sh 1139 241 -1 1381 images/maze-cube/3d/1920x1080/maze-cube_1920x1080_XXXXXX.png
# or
# frameDup.sh 939 481 -1 1420 images/maze-cube/4d/1920x1080/maze-cube_1920x1080_XXXXXX.png

if [ -z "$5" ]; then
    echo "Usage:"
    echo "  $0 startFrame endFrame step outStart pattern"
    echo ""
    echo "Pattern should have 6 X (i.e. XXXXXX) where number will be filled in."
    echo ""
    exit 1
fi

STARTF=$1
ENDF=$2
STEP=`echo "$3" | sed 's/-/_/'`  # dc requires this for negative numbers
OUTF=$4
PATTERN=$5

echo "Looping from ${STARTF} to ${ENDF} by ${STEP}."
echo "Output starts at ${OUTF} with pattern ${PATTERN}."

IFRAME=${STARTF}
OFRAME=${OUTF}
while true; do
    # pad frame numbers with zeros
    PIF=`echo ${IFRAME} | awk '{printf "%04i", $1}'`
    POF=`echo ${OFRAME} | awk '{printf "%04i", $1}'`

    # constuct file names from pattern
    SRC=`echo ${PATTERN} | sed 's/XXXXXX/'${PIF}'/g'`
    DST=`echo ${PATTERN} | sed 's/XXXXXX/'${POF}'/g'`

    # duplicate file accordingly
    #cp -v "${SRC}" "${DST}" || exit 1
    ln "${SRC}" "${DST}" || exit 1

    # check exit condition here, so final copy happens
    if [ ${IFRAME} -eq ${ENDF} ]; then
        exit 0
    fi

    # update frame counters
    IFRAME=`echo "0 ${IFRAME} ${STEP} + p" | dc`
    OFRAME=`echo "0 ${OFRAME} 1 + p" | dc`
done
