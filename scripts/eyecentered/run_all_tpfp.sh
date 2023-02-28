#!/bin/bash

DIR=$1
LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    echo $f
    #newdir=`basename $f`
    csvfile=$f"/tobii2_color.mkv.gaze_resampled.csv"
    bash single_tpfp_sample.sh $csvfile $f 1>"$f""/TPFP.OUT" 2>&1 &
done
