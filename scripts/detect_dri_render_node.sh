#!/bin/bash

MYNODE="";
for NODE in `ls /dev/dri/renderD*`; do
    #echo "TESTING: " + $NODE
    RESULT=`vainfo --display drm --device $NODE 2>&1 | grep error`
    #echo "MY RESULT: " $RESULT
    if [ -z "$RESULT" ]
    then
	MYNODE=$NODE
    fi
done;
echo $MYNODE
