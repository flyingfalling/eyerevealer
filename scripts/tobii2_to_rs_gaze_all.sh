#!/bin/bash


#python3 ../tools/tobii2_to_rs_gaze.py goodata/vid.gaze goodata/rs.gaze

DIR=$1

for d in $DIR/raw-*/; do
    echo $d
    python3 tools/tobii2_to_rs_gaze.py $d/tobii2_color.mkv.gaze $d/rs_color.mkv.gaze $d/offset
done

	

