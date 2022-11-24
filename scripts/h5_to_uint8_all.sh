#!/bin/bash

#python3 scripts/readh5py.py build/results/raw-2022-02-23-16-48-50/rs_depth.h5 test.mkv build/results/raw-2022-02-23-16-48-50/rs_color.mkv

DIR=$1

for d in $DIR/raw-*/; do
    echo $d
    python3 scripts/readh5py.py $d/rs_depth.h5 $d/rs_depth.mkv $d/rs_color.mkv
done

	

