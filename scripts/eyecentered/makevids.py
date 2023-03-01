#!python3
#REV: take input from saliency...

#REV: note, takes input as output from saliency_pct.py (t, x, y, xsamp, ysamp, etc.)

#REV: note, takes y-up values, and x-right values (i.e. not tobii format). Flips Y.

#REV: this runs it for the specified video...including drawing circles etc.
#REV: however, I also want to open for all the different SALMAP guys? Just do separately lol...
#REV: salmap guys are already u8c3?
#REV: better to combine here if possible...
#REV: would really like to "draw" a thing below it...for each video/measurement, of guy...
#REV: I only need one "original"? Draw a separate video for each salmap? And each radius? Nah...yea?
#REV: Just draw one (for example).
#REV: Draw "histogram" and output to another video...

#REV: I can do that afterwards, for now main thing is (1) make video and (2) calculate values. I can compute (total) ROC after...
#REV: note ROC is calculated on thresholded values AFTER blur/filtering? Meh..that's fine I guess.

#REV: roughtly 1000 pts for each eye sample, eye samples are 100 hz, times (circ+gaus) times salmap num 3 = 6*2*1000*100 per sec

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import math
import cv2
import time
import sqlite3

fname = sys.argv[1];


sep = sys.argv[2];
if( sep[0] != '[' or sep[2] != ']' ):
    print("Error, expect [] with sep");
    exit(1);
    pass;
else:
    sep = sep[1];
    pass;

rawvidfname = sys.argv[3]; #REV: this is RAW vid name

outvidfname = sys.argv[4]; #REV: full path I guess?

viddvawid = float(sys.argv[5]);
viddvahei = float(sys.argv[6]);

centuncent = sys.argv[7];

outdfname = sys.argv[8];

#REV: sal vids...
#REV: for now, all vids in that dir with rawvidfname _ TAG.mkv?
salfnamedict = {};
for i in range(9, len(sys.argv), 2):
    key = sys.argv[i] ;
    if key in salfnamedict:
        print("ERROR; key {} already exists -- you must use unique names for sal inputs".format(key));
        exit(1);
        pass;
    
    salfnamedict[ key ] = sys.argv[ i+1 ];
    pass;

print(salfnamedict);

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
        #exit(1);
        pass;
    
    return width, height, fps, frame_count, durationsec;

#10 px / 640 means it is drawing 2.5625 dva radius circles..
def drawcircs( img, xydf, col, radpx=10, thickpx=2, xcol="xsamppx", ycol="ysamppx" ):
    #for tup in xys:
    for idx, row in xydf.iterrows():
        #print(row);
        _ = cv2.circle(img, (int(row[xcol]), int(row[ycol])), radpx, col, thickness=thickpx);
    return;

#REV: box/circular convolution... (REV: what will happen on edges?)
def sampsal( img, xydf, xcol="xsamppx", ycol="ysamppx"):
    retdf = xydf.copy();
    #ys = xydf[ycol].to_list();
    #print( np.min(ys), np.max(ys) );
    
    #xs = xydf[xcol].to_list();
    #print( np.min(xs), np.max(xs) );
    #yxs = [ yx for yx in zip(ys, xs) ];
    #idxs=np.array(yxs).astype(int);
    #print(idxs.shape);
    #vals = img[np.array(xydf[ycol]).astype(int), np.array(xydf[xcol]).astype(int)];
    #print(img.shape);
    retdf["sal"] = img[np.array(xydf[ycol]).astype(int), np.array(xydf[xcol]).astype(int)];
    
    '''
    for idx, row in xydf.iterrows():
        #_ = cv2.circle(img, (int(row[xcol]), int(row[ycol])), radpx, col, thickness=thickpx);
        xpos=int(row[xcol]);
        ypos=int(row[ycol]);
        if( ypos >= 0 and ypos < img.shape[0] and xpos >=0 and xpos < img.shape[1] ):
            sal = img[ypos, xpos];
            newrow = row;
            row["sal"] = sal;
            retdf.loc[ len(retdf.index) ] = newrow;
            pass;
        else:
            #REV: couldn't do it bc outside of frame... error/warn?
            pass;
        pass;
    '''
    return retdf;


#REV: box/circular convolution... (REV: what will happen on edges?)
#REV: note, we need to make the size odd...fuck.
def blurcirc( img, radpx ):
    sz=int(radpx*2+1); #REV: or -1 better?
    mask=np.zeros([sz,sz]);
    mask=cv2.circle(mask, (int(radpx+1), int(radpx+1)), int(radpx), (1,1,1), thickness=-1);
    mask /= np.sum(mask);
    res = cv2.filter2D( src=img, ddepth=-1, kernel=mask );
    return res;

def blurgauss( img, radpx ):
    res = cv2.GaussianBlur( src=img, ksize=(0,0), sigmaX=radpx );
    return res;


df = pd.read_csv( fname, sep=sep );
#df = df2.rename(columns={tcol:"t", xcol:"x", ycol:"y"}); #inplace=True                                                     


cap = cv2.VideoCapture( rawvidfname );

w, h, fps, nframes, dursec = get_cap_info(cap);

cwriter = cv2.VideoWriter();
fourcc = cv2.VideoWriter_fourcc(*"H264");
isColor = True;
ext=".mkv";

if( len(outvidfname) >= 4 and outvidfname[-4:] == ext ):
    outvidfname = outvidfname[:-4];
    pass;

circvidfname = outvidfname;

#if( len(outvidfname.split('.')) > 2 and outvidfname.split('.')[-1] != 'mkv'):
if( len(circvidfname) >= 4 and circvidfname[-4:] != ext ):
    print("Adding {} to out fname".format(ext));
    circvidfname = outvidfname + ext;
    pass;

print("Outputting to file: [{}]".format(circvidfname));

cwriter.open( circvidfname, fourcc=fourcc, fps=fps, frameSize=(w, h), isColor=isColor );


swriterdict={};
scapdict={};

#REV: need to do the same for the saliency videos...
for key in salfnamedict:
    salinfname = salfnamedict[key];
    saloutfname = outvidfname + "_" + key + ext;
    print("Saliency map: {}  (file: {}) output will be: {}".format(key, salinfname, saloutfname));
    scapdict[key] = cv2.VideoCapture( salinfname );

    #REV: sal output is same size as raw input
    swriterdict[key] = cv2.VideoWriter();
    swriterdict[key].open( saloutfname, fourcc=fourcc, fps=fps, frameSize=(w, h), isColor=isColor );
    pass;

iscentered= (centuncent=="centered");
df = df[ (df.centered == iscentered ) ];

wpxperdva = w/viddvawid;
hpxperdva = h/viddvahei;

#REV: do the y ones not work because of Y-pixel size being different? I.e. not square pixels...not isotropic.

#REV: convert all to image space...
xcent=int(w/2);
ycent=int(h/2);

#REV: x will be actually the um, xsamp! Fuck...
df["xsamppx"] = df.sampx * wpxperdva + xcent;

#REV: norm x from left
df["xsamppx01"] = (df.sampx * wpxperdva + xcent)/w;

#REV: y input is positive up, positive right. Need to make positive down.
df["ysamppx"] = -(df.sampy * hpxperdva) + ycent;

#REV: norm y from top
df["ysamppx01"] = (-(df.sampy * hpxperdva) + ycent)/h;

print("Min/Max X: {}/{}    Y: {}/{}".format(df.xsamppx.min(), df.xsamppx.max(), df.ysamppx.min(), df.ysamppx.max()));

#df = df.reset_index(); #REV: uhhh...so that at least rows are unique.
print(df.head(10));

#REV: radii of circles or std of gaussians (in dva) to sample saliency. Will just blur image using filter and draw central pixels.
#sal_circ_rads_dva = np.arange(1, 8.1, 1.0); #1,1.5,...4.5,5 i.e. 9 points...
sal_circ_rads_dva = [2.5, 5, 10];
sal_circ_rads_px = np.array(sal_circ_rads_dva) * hpxperdva;
radsdict={};
for dva, px in zip(sal_circ_rads_dva, sal_circ_rads_px):
    radsdict[dva] = px;
    pass;

#REV: some global sizes for drawing pretty circles..
whiterdva=5;
whiterpx=int(whiterdva * hpxperdva + 0.5);
whitethickpx=int(whiterpx/5 + 0.5);

redbluerdva=2.5;
redbluerpx=int(redbluerdva * hpxperdva + 0.5);
redbluethickpx=int(redbluerpx/5 + 0.5);

if(whiterpx < 5):
    whiterpx = 5;
    pass;
if(whitethickpx<1):
    whitethickpx=1;
    pass;
        
if(redbluerpx < 3):
    redbluerpx=3;
    pass;
if(redbluethickpx<1):
    redbluethickpx=1;
    pass;

outdflist = [];
outdf = pd.DataFrame();



ifps = 1/fps;
fidx=0;
maxt = df.timeidx.max();

#REV: saveall makes it save all data to databases (raw fp etc.)
#REV: otherwise it just save percentiles...and ROC?
SAVEALL=True;
dbconn=sqlite3.connect(outdfname, timeout=6000000);
newcsv=True;

#df = df[['t','tpfpdelta','x','y','timeidx','sampx','sampy']];

def calc_pctl(posval, negvals):
    pctle = ((negvals < posval).sum()) / float(len(negvals)+1);
    return pctle;



while(True):
    timer1=time.time();
    ret, frame = cap.read();
    if( False == ret ):
        print("Raw RET is false!");
        pass;
    
    sframes = {};
    circsframes = {};
    for key in scapdict:
        ret2, sframes[key] = scapdict[key].read();
        if( False == ret2 ):
            print("Salmap RET2 was false!!!");
            ret = False;
            pass;
        else:
            dim=(int(w),int(h));
            resized = cv2.resize(sframes[key], dsize=dim, interpolation = cv2.INTER_AREA);
            sframes[key] = resized[:,:,1]; #REV: fuck it is 3channel lol?
            circsframes[key] = resized.copy(); #cv2.merge((resized, resized, resized));
            #circframes[key] = cv2.cvtColor( resized, cv2.COLOR_GRAY2BGR );
            #print(sframes[key].shape);
            #print(circsframes[key].shape);
            #print(frame.shape);
            pass;
        pass;
            
    stt = fidx * ifps;
    ent = stt+ifps;
    print("Doing for t=[{}]".format(stt));
    if( False == ret or stt > maxt ):
        print("Breaking!!! (ret was false or > max desired time point)");
        break;
    
    
    mydf = df[ (df.t >= stt) & (df.t < ent) & (df.tpfpdelta == "d")];
    #REV: should return only a few, for each of those, get the ones that matter...

    circframe = frame.copy();

    framedflist=[];

    circsaldvadict = {};
    gausssaldvadict = {};
    for salkey in sframes:
        for dva in radsdict:
            circfilt = blurcirc( sframes[salkey], radsdict[dva] );
            circsaldvadict[(salkey,dva)] = circfilt;
            
            gaussfilt = blurgauss( sframes[salkey], radsdict[dva] );
            gausssaldvadict[(salkey,dva)] = gaussfilt;
            pass;
        pass;
    
    for idx, row in mydf.iterrows():
        idx = row.timeidx;
        df2 = df[ (df.timeidx == idx) ];
        tps = df2[ (df2.tpfpdelta == "t") ];
        nulls = df2[ (df2.tpfpdelta == "f") ];
        offsets = df2[ (df2.tpfpdelta == "d") ];

        
        #REV: sample the salmaps...
        for salkey in sframes:
            saldflist = [];# = pd.DataFrame();
            for dva in radsdict:
                #print("Doing for sal {}  DVA {}".format(salkey, dva));
                radpx = radsdict[dva];
                #circfilt = blurcirc( sframes[salkey], radsdict[dva] );
                circfilt = circsaldvadict[(salkey,dva)];

                circresdfnulls = sampsal( circfilt, xydf=nulls );
                circresdftps = sampsal( circfilt, xydf=tps );
                circresdfoffs = sampsal( circfilt, xydf=offsets );
                if( len(circresdftps.index) > 1 ):
                    print("REV: wtf more than one TP?");
                    exit(1);
                    pass;
                #REV: stats here
                circpctle = calc_pctl( circresdftps.iloc[0].sal, circresdfnulls.sal );
                
                if( SAVEALL ):
                    circdf = pd.concat( [circresdfnulls, circresdftps, circresdfoffs] );
                    pass;
                else:
                    circdf = circresdftps;
                    pass;
                
                circdf["pct"] = circpctle;
                circdf["blur"] = "c";
                circdf["blurdva"] = dva;
                circdf["blurpx"] = radpx;
                
                

                
                
                #gaussfilt = blurgauss( sframes[salkey], radsdict[dva] );
                gaussfilt = gausssaldvadict[(salkey,dva)];
                
                gaussresdfnulls = sampsal( gaussfilt, xydf=nulls );
                gaussresdftps = sampsal( gaussfilt, xydf=tps );
                gaussresdfoffs = sampsal( gaussfilt, xydf=offsets );
                gausspctle = calc_pctl( circresdftps.iloc[0].sal, circresdfnulls.sal );

                if( SAVEALL ):
                    gaussdf = pd.concat( [gaussresdfnulls, gaussresdftps, gaussresdfoffs] );
                    pass;
                else:
                    gaussdf = gaussresdftps;
                    pass;
                
                gaussdf["pct"] = gausspctle;
                gaussdf["blur"] = "g";
                gaussdf["blurdva"] = dva;
                gaussdf["blurpx"] = radpx;
                
                saldflist.append(circdf);
                saldflist.append(gaussdf);
                
                pass;
            
            saldf = pd.concat( saldflist );
            saldf["saltype"] = salkey;
            framedflist.append(saldf);
            pass;
        
        #REV: draw for each salmap...
        #REV: note, need to convert them into color, 3 channels
        for salkey in circsframes:
            salimg = circsframes[salkey];
            drawcircs( img=salimg, xydf=nulls.head(30), col=(30,30,150), radpx=redbluerpx, thickpx=redbluethickpx );
            drawcircs( img=salimg, xydf=offsets, col=(40,200,140), radpx=whiterpx, thickpx=whitethickpx );
            drawcircs( img=salimg, xydf=tps, col=(200,30,30), radpx=redbluerpx, thickpx=redbluethickpx );
            pass;
                
        #REV: draw on the circframe (copy if raw input frame itself)
        drawcircs( img=circframe, xydf=nulls.head(30), col=(30,30,150), radpx=redbluerpx, thickpx=redbluethickpx );
        drawcircs( img=circframe, xydf=offsets, col=(40,200,140), radpx=whiterpx, thickpx=whitethickpx );
        drawcircs( img=circframe, xydf=tps, col=(200,30,30), radpx=redbluerpx, thickpx=redbluethickpx );
                
        pass;

    if( len(framedflist) > 0 ):
        framedf = pd.concat( framedflist );
        framedf["frameidx"] = fidx;
        #framedf["framesec"] = stt;
        outdflist.append( framedf  );
        DUMP_CUTOFF=0;
        if( len(outdflist) > DUMP_CUTOFF ):
            outdf = pd.concat( outdflist );
            outdflist = [];
            print("DUMPING {} rows to {}".format(len(outdf.index), outdfname));
            if( SAVEALL ):
                outdf = outdf[['tpfpdelta','blur','blurdva','saltype','frameidx','sal','x','y','timeidx','pct']];
                pass;
            
            if( newcsv ): #0 == len(outdflist) ):
                #outdf.to_csv(outdfname, mode='w', header=True, index=False );
                outdf.to_sql('data', dbconn, if_exists='replace', index=False );
                newcsv=False;
                pass;
            else:
                outdf.to_sql('data', dbconn, if_exists='append', index=False );
                #outdf.to_csv(outdfname, mode='a', header=False, index=False );
                pass;
            pass;
        pass;
    
    cwriter.write(circframe);
    
    for skey in swriterdict:
        swriterdict[skey].write(circsframes[skey]);
        pass;
    
    fidx+=1;
    timer2=time.time();
    print("Elapsed: {:4.1f} msec".format((timer2-timer1)*1e3));
    pass;

print("FINISHED");

'''
print("Appending outdf list...");
outdf = pd.concat( outdflist );
print("FINISHED, writing csv to {}!".format(outdfname));
outdf.to_csv(outdfname, index=False);
print("Done writing to {}".format(outdfname));
'''
