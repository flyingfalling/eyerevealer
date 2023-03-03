#!/bin/bash

DIR=$1
LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    echo $f
    #newdir=`basename $f`
    DEVDIR=`find $f -maxdepth 1 -type d -name "device-Tobii3*"`
    
    for df in $DEVDIR;
    do
	csvfile=$df"/resampled.csv_resampled.csv"
	bash single_tpfp_sample.sh $csvfile $df 1>"$df""/TPFP.OUT" 2>&1 &
    done
done
