#!/bin/bash

DIR=$1

VIDFILE=$DIR"/tobii2_color.mkv"
GAZEFILE=$DIR"/tobii2_color.mkv.gaze"
GAZEOUT=$GAZEFILE"_resampled.csv"

#REV: something.gaze
python3 saccade_detection.py $GAZEFILE TSEC 1 X 82 Y -52 0.5 0.5 &> $DIR"/SACC_STD.OUT"

python3 centerize_vid.py $VIDFILE $GAZEOUT tobii2 &> $DIR"/CENT_STD.OUT"
