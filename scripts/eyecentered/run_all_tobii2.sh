#!/bin/bash

DIR=$1
LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    echo $f
    bash tobii2example.sh $f &
done
