#!/bin/bash


#python3 scripts/print_timestamps.py ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-RS_Intel_RealSense_D435I_#140122076342/rs_color.mkv.ts 1000 build/goodata/rsvid.ts

DIR=$1

for d in $DIR/raw-*/; do
    echo $d
    python3 scripts/print_timestamps.py $d/rs_color.mkv.ts 1000 $d/rs_color.mkv.ts.sec
    python3 scripts/print_timestamps.py $d/rs_depth.h5.ts 1000 $d/rs_depth.h5.ts.sec
done

	

