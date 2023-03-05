#!/bin/bash

EXE=$1
INDIR=$2

rawdirs=`find $INDIR -maxdepth 1 -name "*raw-*" ;`

for f in $rawdirs;
do
    echo $f
    INDIRS=`find $f -maxdepth 1 -name "*device-TOBII2*" ;`
    for d in $INDIRS
    do
	echo $d
	#$EXE $d"/tobii2_mpegts.raw" \
	#     $d"/tobii2_json.raw" \
	#     $d"/tobii2_scene.mkv" \
	#     $d"/tobii2_scene.mkv.ts" \
	#     $d"/tobii2_scene.gaze.csv" > $d"/RESAMPLE2.OUT" 2>&1
	
	# convert to _flat and resample
	echo "FLATTENING for $d"
	resamprate=100
	python3 tobii2_resample_valgaze.py $d/tobii2_scene.gaze.csv $resamprate 1>$d"/FLATTEN.OUT" 2>&1
    done
done
