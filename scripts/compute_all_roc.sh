#!/bin/bash



DIR=$1
TAG=$2
TSNAME=$3 #rs_color.mkv.ts.sec  or #tobii2_color.mkv.ts
GAZENAME=$4 #rs_color.mkv.gaze   or #tobii2_color.mkv.gaze

for d in $DIR/raw-*/; do
    echo $d
    salfn=$d/"$TAG""_fin.mkv"
    outfn="$salfn"".roc"
    stdo=$d/"TAG""_stdROC"
    python3 tools/compute_roc.py $salfn $d/"$TSNAME" $d/"$GAZENAME" $outfn  &> $stdo &
done

	

