
#REV: take input from saliency...


import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import math
import cv2

fname = sys.argv[1];


sep = sys.argv[2];
if( sep[0] != '[' or sep[2] != ']' ):
    print("Error, expect [] with sep");
    exit(1);
    pass;

sep = sep[1];

vidfname = sys.argv[3];

outvidfname = sys.argv[4];

viddvawid = float(sys.argv[5]);

centuncent = sys.argv[6];

#REV: for each video frame...get points around it.

#REV: for centered and uncentered, draw eye position "now", "next" eye position (TP) and nulls (FP)

def get_cap_info( cap ):
    if not cap.isOpened():
        print("Cap not opened!");
        exit(1);
    width  = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH));  # float
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)); # float
    fps = cap.get(cv2.CAP_PROP_FPS);
    frame_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT));
    
    if( frame_count > 0 ):
        durationsec = frame_count/float(fps);
    else:
        print("Frame count is zero?! Need to estimate manually...?");
        durationsec=None;
        exit(1);
        pass;
    
    return width, height, fps, frame_count, durationsec;


def drawcircs( img, xydf, col, radpx=10, thickpx=2, xcol="xsamppx", ycol="ysamppx" ):
    #for tup in xys:
    for idx, row in xydf.iterrows():
        #print(row);
        _ = cv2.circle(img, (int(row[xcol]), int(row[ycol])), radpx, col, thickness=thickpx);
    return;





df = pd.read_csv( fname, sep=sep );
#df = df2.rename(columns={tcol:"t", xcol:"x", ycol:"y"}); #inplace=True                                                     


cap = cv2.VideoCapture( vidfname );

w, h, fps, nframes, dursec = get_cap_info(cap);

writer = cv2.VideoWriter();
fourcc = cv2.VideoWriter_fourcc(*"H264");
isColor = True;
ext=".mkv";

writer.open( outvidfname, fourcc=fourcc, fps=fps, frameSize=(w, h), isColor=isColor );

iscentered= (centuncent=="centered");
df = df[ (df.centered == iscentered ) ];

pxperdva = w/viddvawid;

#REV: convert all to image space...
xcent=int(w/2);
ycent=int(h/2);

#REV: x will be actually the um, xsamp! Fuck...
df["xsamppx"] = df.sampx * pxperdva + xcent;

#REV: y input is positive up, positive right. Need to make positive down.
df["ysamppx"] = -(df.sampy * pxperdva) + ycent;

ifps = 1/fps;
fidx=0;
maxt = df.timeidx.max();
while(True):
    ret, frame = cap.read();
    
    stt = fidx * ifps;
    ent = stt+ifps;
    print("Doing for t=[{}]".format(stt));
    if( False == ret or stt > maxt ):
        break;
    
    
    mydf = df[ (df.t >= stt) & (df.t < ent) & (df.tpfpdelta == "delta")];
    #REV: should return only a few, for each of those, get the ones that matter...
    for idx, row in mydf.iterrows():
        idx = row.timeidx;
        df2 = df[ (df.timeidx == idx) ];
        tps = df2[ (df2.tpfpdelta == "tp") ];
        fps = df2[ (df2.tpfpdelta == "fp") ];
        offsets = df2[ (df2.tpfpdelta == "delta") ];

        drawcircs( img=frame, xydf=fps.head(50), col=(30,30,150) );
        drawcircs( img=frame, xydf=offsets, col=(180,180,180), radpx=20, thickpx=4 );
        drawcircs( img=frame, xydf=tps, col=(200,30,30) );
        pass;

    writer.write(frame);
    
    fidx+=1;
    pass;
    
