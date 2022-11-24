#!/bin/bash



#REV: loop through directory, for each directory raw-DATE, inside that is
#REV: directory device-TOBII2_*

#REV: inside that we have tobii2_mpegts.raw and tobii2_json.raw

#./tobii2_file_vidgaze_decoder.exe ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-TOBII2_TG02B-080107043241_fe80__2c20_6fff_fe38_6801%wlp59s0_/tobii2_mpegts.raw ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-TOBII2_TG02B-080107043241_fe80__2c20_6fff_fe38_6801%wlp59s0_/tobii2_json.raw vid.mkv vid.ts vid.gaze


DIR=$1
OUTDIR=$2

for d in $DIR/raw-*/; do
    echo $d
    BN=$(basename $d)
    echo $BN
    MYDIR=$OUTDIR/$BN
    mkdir -p $MYDIR
    for d2 in $d/device-RS_*; do
	RSVID=$d2/rs_color.mkv
	RSVIDTS=$d2/rs_color.mkv.ts
	RSDEP=$d2/rs_depth.h5
	RSDEPTS=$d2/rs_depth.h5.ts
	cp $RSVID $MYDIR/rs_color.mkv
	cp $RSVIDTS $MYDIR/rs_color.mkv.ts
	cp $RSDEP $MYDIR/rs_depth.h5
	cp $RSDEPTS $MYDIR/rs_depth.h5.ts
    done
    
    for d2 in $d/device-TOBII2_*; do
	#echo $d2
	#ls -lah $d2/tobii2_mpegts.raw
	#ls -lah $d2/tobii2_json.raw
	MPEGTS=$d2/tobii2_mpegts.raw
	JSON=$d2/tobii2_json.raw
	./tobii2_file_vidgaze_decoder.exe $MPEGTS $JSON $MYDIR/tobii2_color.mkv $MYDIR/tobii2_color.mkv.ts $MYDIR/tobii2_color.mkv.gaze
    done
done

	

