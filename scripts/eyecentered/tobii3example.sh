#!/bin/bash

DIR=$1

VIDFILE=$DIR"/tobii3_scene.mp4"
GAZEFILE=$DIR"/resampled.csv"
GAZEOUT=$GAZEFILE"_resampled.csv"

#REV: something.gaze
python3 saccade_detection.py $GAZEFILE Tsec 1 gaze2d_0 95 gaze2d_1 -63 0.5 0.5 [,] &> $DIR"/SACC_STD3.OUT"

python3 centerize_vid.py $VIDFILE $GAZEOUT tobii3 &> $DIR"/CENT_STD3.OUT"
