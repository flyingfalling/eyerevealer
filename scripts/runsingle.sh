#REV: extract tobii2 video and gaze positions from the mpegts and json streams
./tobii2_file_vidgaze_decoder.exe ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-TOBII2_TG02B-080107043241_fe80__2c20_6fff_fe38_6801%wlp59s0_/tobii2_mpegts.raw ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-TOBII2_TG02B-080107043241_fe80__2c20_6fff_fe38_6801%wlp59s0_/tobii2_json.raw vid.mkv vid.ts vid.gaze

#REV: print timestamps of RS from binary
python3 scripts/print_timestamps.py ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-RS_Intel_RealSense_D435I_#140122076342/rs_color.mkv.ts 1000 build/goodata/rsvid.ts

#import pandas as pd
#df = pd.read_csv( "conditions.csv", delim_whitespace=True)
#df.to_csv( "~/gitsoft/rteye2/build/results/conditions.csv", sep=' ');

#REV: synchronize it (requires manual intervention...)
python3 ../tools/time_space_sync.py goodata/vid.mkv goodata/vid.ts ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-RS_Intel_RealSense_D435I_#140122076342/rs_color.mkv goodata/rsvid.ts goodata/sync goodata/offset

#REV: build the RS gaze from the sync values and the tobii2 gaze
python3 ../tools/tobii2_to_rs_gaze.py goodata/vid.gaze goodata/rs.gaze


#REV: visualize them to make sure :)
python3 ../tools/visualize_tobii2_rs_gaze.py goodata/vid.mkv goodata/vid.ts goodata/vid.gaze ~/mazda_driving_data/raw-2022-02-23-14-48-52/device-RS_Intel_RealSense_D435I_#140122076342/rs_color.mkv goodata/rsvid.ts goodata/rs.gaze 0.51969287


