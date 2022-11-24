#!/bin/bash

d=$1


python3 tools/visualize_sal_and_gaze.py $d/tobii2_color.mkv $d/tobii2_color.mkv.ts $d/tobii2_color.mkv.gaze $d/rs_color.mkv $d/rs_color.mkv.ts.sec $d/rs_color.mkv.gaze $d/offset $d/RSDEPTH_fin.mkv
