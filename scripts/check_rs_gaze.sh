#!/bin/bash

#python3 ../tools/visualize_tobii2_rs_gaze.py goodata/vid.mkv goodata/vid.ts goodata/vid.gaze ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-RS_Intel_RealSense_D435I_#140122076342/rs_color.mkv goodata/rsvid.ts goodata/rs.gaze 0.51969287


d=$1

echo $d

python3 tools/visualize_tobii2_rs_gaze.py $d/tobii2_color.mkv $d/tobii2_color.mkv.ts $d/tobii2_color.mkv.gaze $d/rs_color.mkv $d/rs_color.mkv.ts.sec $d/rs_color.mkv.gaze $d/offset

	

