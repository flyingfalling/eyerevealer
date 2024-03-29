#REV: this simply does the sampling of TP and FP and time offsets.
#REV: will take (1) eye positions (and times)

#REV: for centered, we need to sample at the offset value of position (negatively). I.e. where was the CURRENT eye position
#     100 msec ago (and sample saliency there).
#REV: for uncentered, we need to simply sample the specified location

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import math
import cv2
import sqlite3

fname = sys.argv[1];

tcol = sys.argv[2];
tu = float(sys.argv[3]);

xcol = sys.argv[4];
xu = float(sys.argv[5]);

ycol = sys.argv[6];
yu = float(sys.argv[7]);

x0 = float(sys.argv[8]);
y0 = float(sys.argv[9]);

sep = sys.argv[10];
if( sep[0] != '[' or sep[2] != ']' ):
    print("Error, expect [] with sep");
    exit(1);
    pass;

sep = sep[1];

outfname = sys.argv[11]; 

print("Using sep: [{}] (Xcol: [{}], Ycol: [{}])".format(sep, xcol, ycol));
df2 = pd.read_csv( fname, sep=sep );

df2 = df2[[tcol, xcol, ycol]]; #REV: only subset...

#REV: oh it may overwrite shit?
df = df2.rename(columns={tcol:"t", xcol:"x", ycol:"y"}); #inplace=True                                                     

df.x -= x0;
df.y -= y0;

df.x *= xu;
df.y *= yu; #REV: will include a bottom-top flip if needed (i.e. I expect df.y to be in bottom-neg, up-positive after this)

df.t *= tu;

#REV: need something to access "target" in sal video...

#REV: null model is all eye positions... (relative to current position, not as possible saccades...right?)
#REV: better: conditional probability from current position...oh well

#REV: get frame at time? We need frame "indices" which I didn't use before...

#REV: sample from saliency VIDEO at time X, is it centered, etc.
#REV: simultaneously sample the null positions as well.
#REV: draw circles if we like? Note passing VideoCapture will cause that cv2 python memory leak? :(

#REV: note this "trial" only (or this subj, this etc.)
#REV: note: want to make a separate "video" of histograms for each time point (?) against the thing...
#REV: all "other" I guess? Just do histogram of values (0 to 1) of the null, draw them blue, then draw the "true" value histo on top (as 1).
#REV: same number of bins I guess... whatever, do later...
#REV: do for ROC as well? For each time point, run at thresholds 0 to 1, at each, take TP vs FP rate. Threshold, all below 0, all above 1.
#REV: mark the x, y positions (of the image) and the frame number? For each "sample"
#REV: OK...let's just record it in a big DF, and do it later...
#REV: sample (i.e. time), null sample time, null sample idx, etc., etc.

#REV: gazedf x, y are in top-left 0,0, and in VIDEO (salmap?) NORMALIZED COORDINATES...
#REV: for centered... gaze is always at 0.5,0.5 I.e. "original coordinates" must be offset from that.
#REV: also get e.g. distribution of X, Y saliency? Won't really work for centered :(
#samprowsdf = notviddf.sample( n=NNEGSAMPS, replace=True );
#        negxyns = [ (x,y) for x, y in zip(samprowsdf.x, samprowsdf.y) ];

#REV: should I use original xfilt etc.?
#REV: take mean of 100 msec before? Or -150 to -100 before? Or...most recent? What if they are all NaN?
#REV: "integrated evidence" over last period, which determined the current eye position I guess?
#REV: note, near end of fixation...I'll keep looking! Great ;) I could do for different delta's I guess? By accident one may be better though :(
#REV: 

#REV: I will make proper databases

#1) actual gaze data. This will stay constant. I.e. gaze2d_1, etc., etc.
#   This will have "indices" (i.e. each gaze sample? will be an independent key
#REV: should I make a single db for all trials? raw_blah? Or separate for each? For now, separate for each...

#2) Smoothing algorithm. I will also "smooth" or resample? And also create e.g. saccade and fixation databases... those will also be separate?

#3) Sampled gaze data (by...trial or some shit?). I need to get for CENTERED and UNCENTERED? Actually, the samples are the same...right?
#   
#2) 


NNEGSAMPS=500;
POSTGRACESEC=0.050;
PREGRACESEC=0.025;
def sample_tpfp(deltasec, gazedf, timesec, centered=False, TCOL="t", XCOL="x", YCOL="y"):
    print("Processing: [{}]".format(timesec));
    tprow = gazedf[ (gazedf[TCOL] == timesec) & (False == gazedf[XCOL].isna()) ].copy();
    if( 0 == len(tprow.index)  ):
        print("Null time [{}]".format(timesec));
        #REV: nothing to do
        return pd.DataFrame();
    if( len(tprow.index) > 1 ):
        print("REV: wtf more than one gaze pos for tp [{timesec}]?");
        print(tprow);
        return pd.DataFrame();
    
    fprows = gazedf[ (gazedf[TCOL] != timesec) & (False == gazedf[XCOL].isna()) ].sample( n=NNEGSAMPS, replace=False ).copy();
    
    tprow["tpfpdelta"] = "t";
    fprows["tpfpdelta"] = "f";
    #REV: iloc is row index from 0, loc is index by index!
    timeidx = tprow.iloc[0][TCOL] #REV: time index is the actual time I sampled from (i.e. sample from timesec, to do that I get all values with that timepoint that are not NAN,
    #REV: if timeidx is not timesec, error?
    tprow["timeidx"] = timeidx;
    fprows["timeidx"] =timeidx;
    
    #REV: timeidx is the UNOFFSET time (i.e. the time point of which the X, Y of the TP is drawn). Then, I sample saliency from e.g. -100msec, which is actual e.g. t where I am drawn from.
    #REV: for TP, t should be same as timeidx...
    
    #REV: These represent the "True" locations, however, I must modify it to get "closest"
    offsettime = timeidx + deltasec; #REV: deltasec is probably negative...
    
    offsetdf = gazedf[ (gazedf[TCOL] >= (offsettime-PREGRACESEC)) &
                       (gazedf[TCOL] <= (offsettime+POSTGRACESEC)) &
                       (False == gazedf[XCOL].isna()) ];
    
    
    #df.iloc[(df['points']-101).abs().argsort()[:1]]
    offsetdf = offsetdf.iloc[ (offsetdf[TCOL]-offsettime).abs().argsort().head(1) ];
    if( 0 == len(offsetdf.index) ):
        print("No valid -delta timepoints for [{}]".format(timesec));
        return pd.DataFrame(); #REV: no value for -delta...
    
    #tprow[TCOL] = offsetdf.iloc[0][TCOL];
    offsetdf["timeidx"] = timeidx;
    offsetdf["tpfpdelta"] = "d";
    
    retdf = pd.concat( [tprow, fprows, offsetdf] );
    
    offx = offsetdf.iloc[0][XCOL];
    offy = offsetdf.iloc[0][YCOL];
    
    if( False == centered ):
        offx = 0; #I will not center at all, i.e. use same raw value from "now" to sample from.
        offy = 0;
        pass;
    #REV: If centered==False, I want to sample naively from tprow[XCOL] and tprow[YCOL] at time offsetdf[TCOL]. I.e. what did *I* look like delta time ago.
    #REV: If centered==True, I want to sample from current position, relative to position delta time ago. Since we know that at any time point,
    #REV:    the center is me, I need to basically subtract the eye position at -delta time from all the others...

    
    #REV: I.e. tp is 0.6, 0.6, and I was looking at 0.4, 0.4, This will give me 0.6-0.4, i.e. 0.2. From 0 center, I should sample 0.2...
    #REV: I.e. this is value from "center". Otherwise, I should always just offset from 0,0?
    #REV: Centered assumes offset from 0,0.
    retdf["sampx"] = retdf[XCOL] - offx;
    retdf["sampy"] = retdf[YCOL] - offy;
    
    #REV: note, these are still in whatever format from before. They must be converted to "image space"
    #REV: however, they are in same "global format". For uncentered ones, they represent some relationship to image (relative to fixed image).
    #REV: e.g. 0,0 is straight ahead, i.e. center of image. So, e.g. +10, +10 will sample down-right
    
    #REV: for centered, 0,0 is not straight ahead. Rather, the image's 0,0 is equal to offsetdf.X and Y. However, this is fine, as it means "next"
    #REV: will be correct, +10, +10 being down right (and it means I'm looking up-left). OK...
    
    return retdf;


#df_centeredlist = []; #pd.DataFrame();
#df_uncenteredlist = []; #pd.DataFrame();

#REV: write to sqlite3 db...

dbconn=sqlite3.connect(outfname, timeout=6000000);
newcsv=True;

DELTASEC=-0.100;
for timesec in df.t.unique():
    #REV: only difference between centered and uncentered is that OFFSET
    resdf_centered = sample_tpfp(deltasec=DELTASEC, gazedf=df, timesec=timesec, centered=True);
    resdf_uncentered = sample_tpfp(deltasec=DELTASEC, gazedf=df, timesec=timesec, centered=False);
    
    #df_centeredlist.append( resdf_centered );
    #df_uncenteredlist.append( resdf_uncentered );
    resdf_centered["centered"] = True;
    resdf_uncentered["centered"] = False;

    outdf = pd.concat([resdf_centered, resdf_uncentered]);

    if( len(outdf.index) > 0 ):
        if( newcsv ):
            #dbconn.execute('DROP TABLE data');
            print("Attempting to save to {}".format(outfname));
            outdf.to_sql('data', dbconn, if_exists='replace', index=False );
            newcsv=False;
            pass;
        else:
            outdf.to_sql('data', dbconn, if_exists='append', index=False );
            pass;
        pass;
    pass;

print("FINISHED, {}".format(outfname));

#df_centered = pd.concat(df_centeredlist);
#df_uncentered = pd.concat(df_uncenteredlist);

#df_centered["centered"] = True;
#df_uncentered["centered"] = False;



#dffinal = pd.concat([df_centered, df_uncentered]);

#print("Will write to CSV file {}".format(outfname));
#dffinal.to_csv(outfname, index=False);

#print("Wrote to CSV file {}".format(outfname));

#REV: fuck when I make the video it's going to be nasty...need to do for "every" timepoint... (every FRAME?!) Sample saliency from gaussian around?
#REV: i.e. not just from instant, but from salmaps for e.g. one or two frames, with varying values... LPF faster...

#REV: I can then "offset" everything? So, what I want to do, is sample, where was the X, Y at the time beforehand. Depends on where time is "then".
#REV: I.e...
