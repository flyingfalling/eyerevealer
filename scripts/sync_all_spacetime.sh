#!/bin/bash

#python3 ../tools/time_space_sync.py goodata/vid.mkv goodata/vid.ts ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-RS_Intel_RealSense_D435I_#140122076342/rs_color.mkv goodata/rsvid.ts goodata/sync goodata/offset


DIR=$1

for d in $DIR/raw-*/; do
    echo $d
    python3 tools/time_space_sync.py $d/tobii2_color.mkv $d/tobii2_color.mkv.ts $d/rs_color.mkv $d/rs_color.mkv.ts.sec $d/sync $d/offset
done

	

