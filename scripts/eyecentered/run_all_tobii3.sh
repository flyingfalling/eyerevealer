#!/bin/bash

DIR=$1
LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    DEVDIR=`find $f -maxdepth 1 -type d -name "device-Tobii3*"`
    for df in $DEVDIR;
    do
	echo $df
	bash tobii3example.sh $df &
    done
done
