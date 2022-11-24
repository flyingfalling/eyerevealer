#!/bin/python3

#REV: ghetto ROC computer given a saliency video (uint8)
#REV: needs to "gather" saliency within a given DVA...but fuck it just use target pixel for now?



import sys
import numpy as np
import pandas as pd
import cv2


#REV: read in

#REV: timestamps sec (of frames)
#REV: gaze (timestamp sec and x/y normalized)
#REV: video itself (mkv frames uint8)

#REV: color/depth vid for overlay?


salvidfn = sys.argv[1];
ts = sys.argv[2]; #REV: text, FIDX SEC
gaze = sys.argv[3];
outrocfn = sys.argv[4];

#salvidfn=sys.argv[4];

scale=4.0;

def draw_circles( img, ptlist, col=(255,255,0), radpx=20, linepx=3 ):
    for pt in ptlist:
        cv2.circle(img,(int(pt[0]), int(pt[1])),radpx,col,linepx);
    return;

tsf = open( ts, "r" );
head=tsf.readline();

tsdict={};
for line in tsf.readlines():
    splt = line.split(' ');
    idx = int(splt[0]);
    tsec = float(splt[1]);
    tsdict[idx] = tsec;


salcap = cv2.VideoCapture( salvidfn );

#REV: get timepoints
gazedf = pd.read_csv(gaze, sep=' ');

#REV: filter out illegal positions...?
gazedf = gazedf[ (gazedf.X >= 0) & (gazedf.X <= 1.0) & (gazedf.Y >= 0) & (gazedf.Y <= 1.0) ]


nnulls = 50;
nullgazedf = gazedf.sample(n=nnulls, replace=False);
#nullxys = [(x,y) for x,y in zip( nullgazedf['X'], nullgazedf['Y'] ) ];
#print(nullxys);



time=tsdict[0]; #REV: I just know zeroindex...
idx=0;
ret, frame = salcap.read();
frame = cv2.resize( frame, (int(frame.shape[1]*scale), int(frame.shape[0]*scale)) );
while( tsdict[idx] < time ):
    ret, frame = salcap.read();
    if( ret ):
        idx += 1;
        frame = cv2.resize( frame, (int(frame.shape[1]*scale), int(frame.shape[0]*scale)) );
    else:
        print("Vid (Sal) File ended!");
        exit(1);


dtsec=0.020;

gazedsec = 0.040;

#cv2.namedWindow('SAL');

pctles=[];
pctts=[];

while(ret):
    if( 0 == idx % 300 ):
        print( "Time (sec): [{:>7.3f}]".format( time ) );
    
    
    
    #REV: draw points of gaze!
    
    #REV: get rstime and tobii2time corresponding points ;)
    #ss = gazedf[ (gazedf.TSEC > time-gazedsec ) & (gazedf.TSEC < time+gazedsec) ];
    ss = gazedf[ (gazedf.TSEC > time ) & (gazedf.TSEC < time+gazedsec) ];
    
    
    
    #NEGS
    nullxs = nullgazedf['X'] * frame.shape[1];
    nullys = nullgazedf['Y'] * frame.shape[0];
    nullpts = [ (int(x),int(y)) for x, y in zip(nullxs, nullys) ];
    
    
    negvals = [ frame[y][x][2]/255.0 for (x,y) in nullpts ];
    #print("Negatives", negvals);
    #print("NEG Mean: {}".format( np.mean(negvals)) );
    
    #POS (at end)
    xs = ss.X * frame.shape[1];
    ys = ss.Y * frame.shape[0];
    pts = [ (int(x),int(y)) for x, y in zip(xs, ys) ];
    
    posvals = [ frame[y][x][2]/255.0 for (x,y) in pts ];
    #print("Positives", posvals);
    #print("POS Mean: {}".format( np.mean(posvals)) );

    #REV: calculate percentile (among only sampled null and positive pixels).
    #REV: percentile is just number of items < me, divided by number of items (+1?) * 100
    
    for posval in posvals:
        pctle = ((negvals < posval).sum()) / float(len(negvals)+1)
        #print(pctle);
        pctles.append(pctle);
        pctts.append(time);
    #print(pctles);


    #draw_circles( frame, nullpts, col=(0,0,255), radpx=10, linepx=2 );
    #draw_circles( frame, pts, col=(255,255,0), radpx=15, linepx=3 );
    #cv2.imshow( 'SAL', frame );
    #key = cv2.waitKey(0);
    
    if( False ): #and key == ord('q') ):
        print("Q pressed, quitting...")
        break;
    
    else:
        #REV: advance both forward (but by dtsec!?) -- when I "flip"
        time += dtsec;
                
        while( tsdict[idx] < time ):
            #print("Advancing (Time={}, IDX={}, IDXTIME={})!".format(time, idx, tsdict[idx]));
            ret, frame = salcap.read();
            if( ret ):
                idx += 1;
                frame = cv2.resize( frame, (int(frame.shape[1]*scale), int(frame.shape[0]*scale)) );
            else:
                print("SAL ended!");
                break;

#outdf = pd.DataFrame( columns=["TSEC", "X", "Y", "SALPCT"] );
outdf = pd.DataFrame( columns=["TSEC", "SALPCT"] );
outdf["TSEC"] = pctts;
outdf["SALPCT"] = pctles;

outdf.to_csv( outrocfn, sep=' ');

print( np.mean(pctles) );
exit(0);

