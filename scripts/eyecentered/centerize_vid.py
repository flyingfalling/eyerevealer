##REV: centerize video based on eye position (i.e. make retinotopic, and apply peripheral blur, etc.)

import os;
import sys;
import cv2;
import numpy as np;
import pandas as pd;
import time;

def embed_edge_gauss_blur_create( inimg255, cxpx, cypx, bgwid, bghei, bgrgb255, blurlimpx, blursigmult=1/3.0 ):
    bgimg = np.full( (bghei, bgwid, 3), bgrgb255, dtype=np.uint8 );
    return embed_edge_gauss_blur( inimg255, cxpx, cypx, bgimg, blurlimpx, blursigmult );
    
def embed_edge_gauss_blur( inimg255, cxpx, cypx, bgimg255, blurlimpx, blursigmult=1/3.0 ):
    sigmax=blurlimpx*blursigmult;

    mask = np.zeros( inimg255.shape, dtype=np.uint8 );
    mask[ int(blurlimpx):int(mask.shape[0]-blurlimpx), int(blurlimpx):int(mask.shape[1]-blurlimpx), : ] = 255;
    
    mask_blurred  = cv2.GaussianBlur( mask, ksize=(0,0), sigmaX=sigmax, sigmaY=0, borderType=cv2.BORDER_CONSTANT );
    mask_blurred_3chan = mask_blurred/255.0; #cv2.cvtColor(mask_blurred, cv2.COLOR_GRAY2BGR).astype('float') / 255.0;

    #cv2.imshow("Small Mask", mask_blurred_3chan );
    #cv2.waitKey(0);
    
    res = bgimg255.copy();
    
    cxpx = int(cxpx);
    cypx = int(cypx);
    
    if( inimg255.shape[1] % 2 == 1 ):
        left = cxpx - int(inimg255.shape[1]/2); #e.g. if center is 10, and it is 11, we will get 10-5 to 10+5 = 5 to 15, total 11
        right = cxpx + int(inimg255.shape[1]/2 + 1);
        pass;
    else:
        left = cxpx - int(inimg255.shape[1]/2 - 1); #e.g. if center is 10, and it is 10, we will get 10-4 to 10+5 = 6 to 15, i.e. 10
        right = cxpx + int(inimg255.shape[1]/2 + 1);
        pass;
    
    if( inimg255.shape[0] % 2 == 1 ):
        top = cypx - int(inimg255.shape[0]/2); #e.g. if center is 10, and it is 11, we will get 10-5 to 10+5 = 5 to 15, total 11
        bot = cypx + int(inimg255.shape[0]/2 + 1);
        pass;
    else:
        top = cypx - int(inimg255.shape[0]/2 - 1); #e.g. if center is 10, and it is 10, we will get 10-4 to 10+5 = 6 to 15, i.e. 10
        bot = cypx + int(inimg255.shape[0]/2 + 1);
        pass;

    #REV: insert is totally outside image...
    if( left >= res.shape[1] or right < 0 or bot < 0 or top >= res.shape[0] ):
        return res;
    
    
    itop=0;
    ileft=0;
    ibot=inimg255.shape[0];
    iright=inimg255.shape[1];
    if( left < 0 ):
        ileft = -left;
        left=0;
        pass;
    if( right > res.shape[1] ):
        iright = inimg255.shape[1] - (right - res.shape[1]);
        right=res.shape[1];
        pass;
    if( top < 0 ):
        itop = -top;
        top=0;
        pass;
    if( bot > res.shape[0] ):
        ibot = inimg255.shape[0] - (bot - res.shape[0]);
        bot=res.shape[0];
        pass;
        
    res = res.astype(np.float32) / 255.0;
    inimg = inimg255.astype(np.float32) / 255.0;

    if( bot-top != inimg.shape[0] ):
        #print("WARN: Bot-Top: {}-{} = {}  !=  Img Hei: {}".format(bot, top, bot-top, inimg.shape[0]));
        #exit(1);
        pass;
    
    if( right-left != inimg.shape[1] ):
        #print("WARN: Right-Left: {}-{} = {}  !=  Img Wid: {}".format(right, left, right-left, inimg.shape[1]));
        #exit(1);
        pass;

    #print( "(Res: {}    T:{} B:{} L:{} R:{}    Hei: {}    Wid: {}     img: {}  mask: {}".format(res.shape, top, bot, left, right, bot-top, right-left, inimg.shape, mask_blurred_3chan.shape));
    
    #print(res[ top:bot, left:right, : ].shape);
    res[ top:bot, left:right, : ] = (inimg[itop:ibot,ileft:iright,:] * mask_blurred_3chan[itop:ibot,ileft:iright,:])  + (res[ top:bot, left:right, : ] * (1.0 - mask_blurred_3chan[itop:ibot,ileft:iright,:]));
    return res;


#REV: centerize video
#REV: argument 1: video file (will open/decode using cv2 opencv VideoCapture)
#REV: argument 2: video-timed eye positions file (one per frame)
#     note: these should be *CLEANED AND SMOOTHED*
#     note: while smooth pursuits and e.g. VOR should be fine, eye velocities over some threshold should be filtered out
#           specifically, vision during saccadic eye movements should be suppressed.
#           this should be true also for saliency analysis (although counter rotation with eye should be OK)
#           maximum head rotation speed will be on the order of...
# https://www.researchgate.net/figure/Average-duration-and-speed-of-head-movements-as-a-function-of-gaze-shifts-A-Head_fig2_277580571
#           Note easily achieves 100 deg/sec (maybe 200 deg/sec). Note saccades usually over 1500 deg/sec.
#           Should also note acceleration of eyes over some threshold...

#     Input eye positions:
# frame#   timesec   xpx   ypx    velocitypxpsec     accelpxpsecpsec     dvappx
# REV: that way, we can do dvappx * px = dva.
# REV: read it all into memory first? Or line-by-line?

# REV: note when I calculate saccades, need to get it in degrees, i.e. base it on dva (angle of field of view) of camera.

# REV: why not smooth it all in real time? :) Note VOR etc. is *EXTREMELY* low latency...can't do more than 1 frame delay...

#REV: bgsize is just 2x size of input...for both wid/hei, so total 4x size (wow).
#REV: may need to downsize ;)

def get_cap_info( cap ):
    if not cap.isOpened():
        print("Cap not opened!");
        exit(1);
        pass;

    width  = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH));  # float
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)); # float
    fps = cap.get(cv2.CAP_PROP_FPS);
    frame_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT));
    
    if( frame_count > 0 ):
        durationsec = frame_count/float(fps);
        pass;
    else:
        print("Frame count is zero?! Need to estimate manually...?");
        durationsec=None;
        exit(1);
        pass;
    
    return width, height, fps, frame_count, durationsec;


#REV: calculate many gaussian blurs (note high levels will take LOTS of time?)
#REV: start with "furthest", and go in "circles", masking out the shape of that iso-blur shape. Then paste "inside" that the next
#REV: blur level.

#REV: send level of "nyquist" and from that compute blah?
#REV: just send levels of blur?
#REV: or function which, given a DVA eccentricity (or pix?), outputs a blur (and a direction?)
import math

def sigma_from_sigmaf( sigmaf ):
    return 1 / (2*math.pi*sigmaf);

def sigma_from_freq_modul( targf, targmodul ):
    retsigf = np.sqrt( -(targf*targf) / (2*np.log(targmodul) ) ); #//log is log_e
    retsig = sigma_from_sigmaf( retsigf );
    return retsig, retsigf;

#REV: could do differnet for the 4 quadrants... (mirror too?) but...
def get_sigma_blur( ecc_dva ):
    fovea_0_sf = 20.0;
    base=0.833;
    mult = base ** ecc_dva; #REV: this is proportion of the max I get. I could convert to stuff, but I'll just do directly...
    sf = mult * fovea_0_sf;
    freqmod=0.5;
    dvasig, _ = sigma_from_freq_modul( sf, freqmod );
    return dvasig;


def euclid_2d_from0(x,y):
    res = np.sqrt( x**2 + y**2 );
    return res;

def euclid_2d(x1,y1,x2,y2):
    res = np.sqrt( (x1-x2)**2 + (y1-y2)**2 );
    return res;

#REV: can also calculate angle and eccintricity this way too haha.
def pixel_dists_from( img, x, y ):
    idxs=np.indices(img.shape[0:2]);
    #idxs[0] -= y;
    #idxs[1] -= x;
    dists = euclid_2d( idxs[1], idxs[0], x, y ); #REV: note swapped 1 and 0 for matrix form!
    return dists;



def sample_gaussian( pos, img, sigmapxs, sigpxcutoff=0.5 ):
    x=int(pos[1]);
    y=int(pos[0]);
    #if( x % 30 == 0 and y % 30 == 0 ):
    #    print("Doing x, y: {} {} / {}".format(x,y,img.shape));
    sigmapx = sigmapxs[y,x];
    
    #ksize = int(((sigmapx-0.8)/0.3 + 1)/0.5 + 1); #sigma = 0.3*((ksize-1)*0.5 - 1) + 0.8 = 0.15x + 0.35. I.e. (sigma - 0.35)/0.15 = ksize
    ksize = math.ceil((sigmapx-0.35)/0.15);
    if( ksize % 2 != 1 ):
        ksize+=1;
        pass;
            
    w=img.shape[1];
    h=img.shape[0];
    
    if( x < 0 or x >= w or y < 0 or y >= h ):
        print("ERROR requesting X Y outside of img");
        exit(1);
        pass;

    #REV: don't bother blurring...
    if( sigmapx < sigpxcutoff ):
        return img[y,x];

   
    samplecirc = True;
    if( samplecirc ):
        if( sigmapx < 3 ):
            return img[y,x];
        
        ksize = math.ceil(sigmapx);
        if( ksize % 2 == 0 ):
            ksize+=1;
            pass;

        pass;

    halfk = int(ksize/2);
    kernL = int(min(halfk, x));
    kernR = int(min(halfk, w-x-1)); #i.e. if x is 999 and w is 1000, w-x is 1, so there is zero on that side.
    kernT = int(min(halfk, y));
    kernB = int(min(halfk, h-y-1));
    
    if( samplecirc ):    
        #REV: just try square rofl.
        return np.mean( img[ (y-kernT):(y+kernB+1), (x-kernL):(x+kernR+1), :], axis=(0,1) );
    
    kern = cv2.getGaussianKernel(ksize, sigmapx); #REV: will give me correct size with cutoff? Better to just sep kernel it with mask? Yea...let's do that LOL
        
    #REV: if kern size is 15, halfk is 7... 0 1 2 3 4 5 6 (7) 8 9 10 11 12 13 14 OK
            
    kern2d = np.outer(kern, kern);
    
    
    weightsum = np.sum(kern2d[ (halfk-kernT):(halfk+kernB+1), (halfk-kernL):(halfk+kernR+1)]);
    kern2d3 = np.stack((kern2d,)*3, axis=-1);
    res = kern2d3[ (halfk-kernT):(halfk+kernB+1), (halfk-kernL):(halfk+kernR+1)] * img[ (y-kernT):(y+kernB+1), (x-kernL):(x+kernR+1), :];
    return np.sum(res) / weightsum;
    

#REV: fuck, I have done this before, I swear, but where?
#REV: y from top...
def sample_gaussian_wblur( img, x, y, sigmapx ):
    #ksize=sigmapx*3; #1.4 sig is 7, i.e. about 5x? 15 is 14/2-1 * 0.3 + 0.8 = 2.6, about 6x!?   sig = 0.3*((ksize-1)*0.5-1) + 0.8. SO:  ((sig-0.8)/0.3 + 1)/0.5 + 1 = ksize
    ksize = ((sigmapx-0.8)/0.3 + 1)/0.5 + 1;
    if( ksize < sigmapx*3 ):
        print("WTF? ksize = {} for sigma {}".format(ksize, sigmapx));
        exit(1);
        pass;
    
    kern = cv2.getGaussianKernel(ksize, sigmapx); #REV: will give me correct size with cutoff? Better to just sep kernel it with mask? Yea...let's do that LOL

    x = int(x);
    y = int(y);
    #5, let us say size is 3. Then it goes 5-3, 5+3, i.e. 2 to 8. Center is 5. 2, 3, 4 (5), 6 7 8 Note, it must INCLUDE the higher one!
    halfk = int((ksize-1)/2); #REV: make it one bigger for safety? This won't deal with edges nicely :( For integrals.
    ret = cv2.GaussianBlur( img[(y-halfk):(y+halfk+1), (x-halfk):(x+halfk+1), :], ksize=(0,0), sigmaX=sigmapx, sigmaY=0 );
    
    return ret[halfk, halfk, :]; #REV: should be a tuple of b, g, r values...


from functools import partial
from itertools import repeat
from multiprocessing import Pool, freeze_support
    
def blur_concentric_bypixel(img, dva_per_px ):
    #res = img.copy();

    time1=time.time();
    cx = img.shape[1]/2;
    cy = img.shape[0]/2;
    distspx = pixel_dists_from( img, cx, cy );
    time2=time.time();
    print(1e3*(time2-time1));
    distsdva = distspx * dva_per_px;
    sigsdva = get_sigma_blur( distsdva );
    #print(sigsdva);
    #sigspx = sigsdva / dva_per_px;
    #print(sigspx);
    
    #REV: note, we have the nyquist limit here, which is sf/2
    #REV: right now, I can sample at most 1/dva_per_pix (e.g. 80 pix / dva) * 1/2 = 40 cycles per degree.
    #REV: Actually, not bad. So, now I will filter anything above that.
    nyqfreqdva = 0.5 * 1/dva_per_px;
    nyqsigmadva = sigma_from_sigmaf( nyqfreqdva );
    #REV: order is intentionally interleaved, because max sigma will be the min freq...
    minsigma = np.min(sigsdva);
    maxfreq = sigma_from_sigmaf( minsigma );
        
    maxsigma = np.max(sigsdva);
    minfreq = sigma_from_sigmaf( maxsigma );
    
    #print("(Pixels: Nyq: {:4.3f} (sig={:4.3f})   MinSig: {:4.3f} (={:4.3f})    MaxSig: {:4.3f} (={:4.3f})".format(nyqfreqpx, nyqsigmapx,  minsigma, maxfreq, maxsigma, minfreq));
    
    print("(DVA: NyqSig: {:4.3f} (={:4.3f})   MinSig: {:4.3f} (={:4.3f})    MaxSig: {:4.3f} (={:4.3f})".format( nyqsigmadva, nyqfreqdva,  minsigma, maxfreq, maxsigma, minfreq));

    
    nyqsigmapx = nyqsigmadva/dva_per_px;  # dva / dva/pix = pix * dva/dva
    minsigmapx = minsigma/dva_per_px;
    maxsigmapx = maxsigma/dva_per_px;
    
    nyqfreqpx = nyqfreqdva*dva_per_px; #REV: freq = 1/dva * dva/px  = 1/px
    minfreqpx = minfreq*dva_per_px;
    maxfreqpx = maxfreq*dva_per_px;
    print("(PIX: NyqSig: {:4.3f} (={:4.3f})   MinSig: {:4.3f} (={:4.3f})    MaxSig: {:4.3f} (={:4.3f})".format( nyqsigmapx, nyqfreqpx,  minsigmapx, maxfreqpx, maxsigmapx, minfreqpx));

    #REV: I will just go pixel-by-pixel, compute gaussian, and sample based on that gaussian weight (i.e. mask just around the correct areas down to a cutoff, shift the kernel, and convolve for that pixel
    #REV: then sum. I can do memory-adjacent, or kernel-adjacent, i.e. should I just go pixel by pixel and re-compute the kernel each time, or go by kernel sizes... ah well. optimize later. What to do about
    #portions off the side? 
    #REV: for efficiency, don't blur under the nyquist, or over e.g. 30 deg?
    #REV: I should also sample logrithmically....

    #REV: or, use spatial frequency space, and do by 0.1 jumps there for example?
    #REV: or do distance "rings" with logrithmic size?
    # min: sig=0.063 dva
    #      e^? = 0.063 -> log(0.063
    #REV: base is e...
    
    maxsigcutoff = 14;
    sigsdva[ (sigsdva>maxsigcutoff) ] = maxsigcutoff;
    sigspx = sigsdva/dva_per_px;
    w=img.shape[1];
    h=img.shape[0];

    #REV: indices are in x,y order...
    indices = list( zip(np.indices(img.shape)[0].flatten(), np.indices(img.shape)[1].flatten())); #REV: generator...but OK?
    indices = indices[0::3];
    print(len(indices))
    with Pool(12) as mypool:
        res = np.array(mypool.map( partial( sample_gaussian, img=img, sigmapxs=sigspx ), indices ));
        #print(res.shape)
        #print(res); #REV: this is a list of 3x tuples now...
        res = res.reshape(img.shape);

    
    '''
    #REV: would need to pass x:y matrix unwrapped as arguments...
    for y in range(w):
        for x in range(h):
            #print("Sampling {} {}".format(x,y));
            sigpx = sigspx[ y, x ];
            res[y,x] = sample_gaussian( pos=(x,y), img, sigpx );
            pass;
        pass;
    '''
    return res;
    
    
#REV: problem is if it is non-isotropic, we need to blur using some "weird shaped" masks.
#REV: i.e. we provide iso-blur masks?
#REV: other way around -> Assign to each pixel a level of blur. Then, go through and "threshold" "level of blur" pixels within narrow
#REV: bands, blur, and apply those pixels only as mask to add to the final image...
def blur_concentric( img, dva_per_px ):
    res = img.copy();

    time1=time.time();
    cx = img.shape[1]/2;
    cy = img.shape[0]/2;
    distspx = pixel_dists_from( img, cx, cy );
    time2=time.time();
    print(1e3*(time2-time1));
    distsdva = distspx * dva_per_px;
    #print(distsdva);
    sigsdva = get_sigma_blur( distsdva );
    #print(sigsdva);
    #sigspx = sigsdva / dva_per_px;
    #print(sigspx);
    
    #REV: note, we have the nyquist limit here, which is sf/2
    #REV: right now, I can sample at most 1/dva_per_pix (e.g. 80 pix / dva) * 1/2 = 40 cycles per degree.
    #REV: Actually, not bad. So, now I will filter anything above that.
    
    nyqfreqdva = 0.5 * 1/dva_per_px;
    nyqsigmadva = sigma_from_sigmaf( nyqfreqdva );

    #nyqsigmadva = dva_per_px * nyqsigmapx;
    #nyqfreqdva = nyqfreqpx/dva_per_px;

    #REV: order is intentionally interleaved, because max sigma will be the min freq...
    minsigma = np.min(sigsdva);
    maxfreq = sigma_from_sigmaf( minsigma );
        
    maxsigma = np.max(sigsdva);
    minfreq = sigma_from_sigmaf( maxsigma );
    
    #print("(Pixels: Nyq: {:4.3f} (sig={:4.3f})   MinSig: {:4.3f} (={:4.3f})    MaxSig: {:4.3f} (={:4.3f})".format(nyqfreqpx, nyqsigmapx,  minsigma, maxfreq, maxsigma, minfreq));
    
    print("(DVA: NyqSig: {:4.3f} (={:4.3f})   MinSig: {:4.3f} (={:4.3f})    MaxSig: {:4.3f} (={:4.3f})".format( nyqsigmadva, nyqfreqdva,  minsigma, maxfreq, maxsigma, minfreq));

    
    nyqsigmapx = nyqsigmadva/dva_per_px;  # dva / dva/pix = pix * dva/dva
    minsigmapx = minsigma/dva_per_px;
    maxsigmapx = maxsigma/dva_per_px;
    
    nyqfreqpx = nyqfreqdva*dva_per_px; #REV: freq = 1/dva * dva/px  = 1/px
    minfreqpx = minfreq*dva_per_px;
    maxfreqpx = maxfreq*dva_per_px;
    print("(PIX: NyqSig: {:4.3f} (={:4.3f})   MinSig: {:4.3f} (={:4.3f})    MaxSig: {:4.3f} (={:4.3f})".format( nyqsigmapx, nyqfreqpx,  minsigmapx, maxfreqpx, maxsigmapx, minfreqpx));
    

    #REV: for efficiency, don't blur under the nyquist, or over e.g. 30 deg?
    #REV: I should also sample logrithmically....

    #REV: or, use spatial frequency space, and do by 0.1 jumps there for example?
    #REV: or do distance "rings" with logrithmic size?
    # min: sig=0.063 dva
    #      e^? = 0.063 -> log(0.063
    #REV: base is e...
    
    maxsigcutoff = 8;
    sigsdva[ (sigsdva>maxsigcutoff) ] = maxsigcutoff;
    mybase=2;
    start = math.log(nyqsigmadva,mybase);
    end = math.log(maxsigcutoff,mybase);
    # max: sig=20 dva
    
    nslices=150;
    slices = np.logspace( start, end, num=nslices, base=mybase );
    slices[-1] += 1.0; #REV: just to make sure I caputre everything...
    #print(slices);

    #REV: DVA is sigsdva, not distances dva!!!!
    
    for i, val in enumerate(slices[:-1]):
        #mask = sigsdva[ (sigsdva >= slices[i]) & (sigsdva < slices[i+1]) ];
        imask = ((sigsdva>=slices[i]) & (sigsdva<slices[i+1]));
        mask = imask.astype(float);
        mask = np.stack((mask,)*3, axis=-1)
        sigpx = slices[i]/dva_per_px;
        started=time.time();
        newmethod=False;
        #REV: this doesn't work because I copy the matrices first? I need to blur them in place? :(
        if( newmethod ):

            tmpimg = img.copy();
            
            #REV: find min/max pixel dist (i.e. inner outer radius).
            mindistpx = np.min(distspx[ imask ]);
            maxdistpx = np.max(distspx[ imask ]);
            print("Min {}  Max {}".format(mindistpx, maxdistpx));
            
            inr = mindistpx - 2.0;
            outr = maxdistpx + 2.0;
            if( inr < 0 ):
                inr = 0;
                pass;
            
            #REV: need to make outer circle inner square as well, i.e. four corners.
            insqrdiag = inr / math.sqrt(2);
            outsqrdiag = outr / math.sqrt(2); 
            outsqr = outr; 

            imgh = img.shape[0];
            imgw = img.shape[1];
                             
            #top
            topy = [cy - outsqr, cy - insqrdiag];
            topx = [cx - outsqrdiag, cx + outsqrdiag];

            skip=False;
            if( topy[0] < 0 ):
                topy[0] = 0;
                pass;
            if( topy[1] > imgh ):
                topy[1] = imgh;
                pass
            
            if( topy[0] >= imgh ):
                #REV: empty, pass
                skip=True;
                pass;
            if( topy[1] < 0 ):
                #REV: empty
                skip=True
                pass;

            
            if( topx[0] < 0 ):
                topx[0] = 0;
                pass;
            if( topx[1] > imgw ):
                topx[1] = imgw;
                pass
            
            if( topx[0] >= imgw ):
                #REV: empty, pass
                skip=True
                pass;
            if( topx[1] < 0 ):
                #REV: empty
                skip=True
                pass;
            
            if( not skip ):
                tblur = cv2.GaussianBlur( tmpimg[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ], ksize=(0,0), sigmaX=sigpx, sigmaY=0 );
                tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] = res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                #bmask = (tblur[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * tmask);
                bmask = (tblur * tmask);
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                
                #timg = img[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ];
                #tblur = cv2.GaussianBlur( timg, ksize=(0,0), sigmaX=sigpx, sigmaY=0 ); #REV: this had better use the proper matrix stuff outside of it, right?!
                #bmask = (tblur*tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                pass;
            
            #bot
            topy = [cy + insqrdiag, cy + outsqr];
            topx = [cx - outsqrdiag, cx + outsqrdiag];

            skip=False;
            if( topy[0] < 0 ):
                topy[0] = 0;
                pass;
            if( topy[1] > imgh ):
                topy[1] = imgh;
                pass
            
            if( topy[0] >= imgh ):
                #REV: empty, pass
                skip=True;
                pass;
            if( topy[1] < 0 ):
                #REV: empty
                skip=True
                pass;

            
            if( topx[0] < 0 ):
                topx[0] = 0;
                pass;
            if( topx[1] > imgw ):
                topx[1] = imgw;
                pass
            
            if( topx[0] >= imgw ):
                #REV: empty, pass
                skip=True
                pass;
            if( topx[1] < 0 ):
                #REV: empty
                skip=True
                pass;
            
            if( not skip ):
                tblur = cv2.GaussianBlur( tmpimg[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ], ksize=(0,0), sigmaX=sigpx, sigmaY=0 );
                tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] = res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                bmask = (tblur * tmask);
                #bmask = (tblur[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * tmask);
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                
                #tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                #timg = img[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ];
                #tblur = cv2.GaussianBlur( timg, ksize=(0,0), sigmaX=sigpx, sigmaY=0 ); #REV: this had better use the proper matrix stuff outside of it, right?!
                #bmask = (tblur*tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                pass;

            
            #left
            topx = [cx - outsqr, cx - insqrdiag];
            topy = [cy - outsqrdiag, cy + outsqrdiag]

            skip=False;
            if( topy[0] < 0 ):
                topy[0] = 0;
                pass;
            if( topy[1] > imgh ):
                topy[1] = imgh;
                pass
            
            if( topy[0] >= imgh ):
                #REV: empty, pass
                skip=True;
                pass;
            if( topy[1] < 0 ):
                #REV: empty
                skip=True
                pass;

            
            if( topx[0] < 0 ):
                topx[0] = 0;
                pass;
            if( topx[1] > imgw ):
                topx[1] = imgw;
                pass
            
            if( topx[0] >= imgw ):
                #REV: empty, pass
                skip=True
                pass;
            if( topx[1] < 0 ):
                #REV: empty
                skip=True
                pass;
            
            if( not skip ):
                tblur = cv2.GaussianBlur( tmpimg[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ], ksize=(0,0), sigmaX=sigpx, sigmaY=0 );
                tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] = res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                bmask = (tblur * tmask);
                #bmask = (tblur[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * tmask);
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                
                #tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                #timg = img[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ];
                #tblur = cv2.GaussianBlur( timg, ksize=(0,0), sigmaX=sigpx, sigmaY=0 ); #REV: this had better use the proper matrix stuff outside of it, right?!
                #bmask = (tblur*tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                pass;
            
            #right
            topx = [cx + insqrdiag, cx + outsqr];
            topy = [cy - outsqrdiag, cy + outsqrdiag]

            skip=False;
            if( topy[0] < 0 ):
                topy[0] = 0;
                pass;
            if( topy[1] > imgh ):
                topy[1] = imgh;
                pass
            
            if( topy[0] >= imgh ):
                #REV: empty, pass
                skip=True;
                pass;
            if( topy[1] < 0 ):
                #REV: empty
                skip=True
                pass;

            
            if( topx[0] < 0 ):
                topx[0] = 0;
                pass;
            if( topx[1] > imgw ):
                topx[1] = imgw;
                pass
            
            if( topx[0] >= imgw ):
                #REV: empty, pass
                skip=True
                pass;
            if( topx[1] < 0 ):
                #REV: empty
                skip=True
                pass;
            
            if( not skip ):
                tblur = cv2.GaussianBlur( tmpimg[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ], ksize=(0,0), sigmaX=sigpx, sigmaY=0 );
                tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] = res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                bmask = (tblur * tmask);
                #bmask = (tblur[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * tmask);
                res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                
                #tmask = mask[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :];
                #timg = img[ int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), : ];
                #tblur = cv2.GaussianBlur( timg, ksize=(0,0), sigmaX=sigpx, sigmaY=0 ); #REV: this had better use the proper matrix stuff outside of it, right?!
                #bmask = (tblur*tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] * (1-tmask);
                #res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] =  res[int(topy[0]):int(topy[1]), int(topx[0]):int(topx[1]), :] + bmask;
                pass;
            pass;
        else:
            #REV: note, eventually I may want to do this with non-circular...so just prep it now? Cant use dist from middle then.
            blurred  = cv2.GaussianBlur( img, ksize=(0,0), sigmaX=sigpx, sigmaY=0 );
            bmask = (blurred*mask);
            
            res = res * (1-mask);
            
            #REV: add new pixels (blurred, and >0 only on the mask locations).
            res = res + bmask;
            pass;

        slwid=slices[i+1]-slices[i];
        slwidpx = slwid/dva_per_px; # dva/dva/px = px
        
        nmsec=time.time()-started;
        print("{:4.1f} msec  MASK: {} - {} DVA (span: {}) (PX: {})".format(nmsec*1e3, slices[i], slices[i+1], slwid, slwidpx));

        
        #print("Blurring between {:4.3}-{:4.3} dva with gauss sigma={:4.3} px  ({}/{} pixels)".format( slices[i], slices[i+1], sigpx, np.count_nonzero(mask), mask.size));
        #print(mask);
            
        
            
            
        
        show=False #True;
        if( show ):
            cv2.imshow("Mask", mask);
            cv2.imshow("Concentric", bmask);
            cv2.imshow("Blurred C", blurred);
            cv2.waitKey(0);
            pass;

        #REV: use numpy where?
        #np.where(mask!=0,imageAllBlurred,image)
        
        
        #REV: delete old pixels (leave everything that is 0 in the mask)
        
        pass;
    
    return res;


#Tobii3 glasses :
#   view (horizontal and vertical) 	95 deg. horizontal / 63 deg. vertical 

#REV: img received as float32, [0,1]
def blur_it( img, blursig, blurpxrad, scaledown ):
    
    #print(img.shape); #960*2*2...
    #blurimg = blur_concentric( img, scaledown*(95.0/(1920)) );
    blurimg = blur_concentric_bypixel( img, scaledown*(95.0/(1920)) );
    #print(np.indices(mask.shape[0:2])); #REV: [0] of this will contain for each pixel the y idx (row#), [1] will contain x idx...
    #print(pixel_dists_from(mask, mask.shape[1]/2, mask.shape[0]/2));
    #mask = cv2.circle( mask, [int(img.shape[1]/2), int(img.shape[0]/2)], blurpxrad, [0,0,0], -1 );
    #maskinv = 1-mask;
    #cv2.imshow( "CMask", mask );
    #cv2.imshow( "IMask", maskinv );
    #cv2.waitKey(0);
    #blurred  = cv2.GaussianBlur( img, ksize=(0,0), sigmaX=blursig, sigmaY=0 );
    #mask_3chan = mask.astype(float)/255.0;
    #blurimg = img.astype(float) * (1-mask) + blurred * mask;
    ##two_linear_blur(0,0);
    return blurimg;



if( __name__ == "__main__" ):
    invidfile = sys.argv[1];
    ingazefile = sys.argv[2];

    #REV: use mean luminance in the image? Of each channel?
    #bgpxval=60;
    #REV: use mean frame luminance?
    
    blursizepx=300; #REV: max size of blur in px, sigma is 1/3 of this.
    
    #if( len(sys.argv) > 3 ):
    #    bgpxval = sys.argv[3];
    #    pass;
   # 
   # bgcol255 = [bgpxval];
    
    cap = cv2.VideoCapture( invidfile );
    if( not cap.isOpened() ):
        print("REV: video file [{}] not open?".format(invidfile));
        exit(1);
        pass;
    
    wid, hei, fps, nframes, dursec = get_cap_info( cap );
    
    bgwid = wid*2;
    bghei = hei*2;
    bgpxval=[0,0,0];
    blackbgimg = np.full( (bghei,bgwid,3), bgpxval, dtype=np.uint8 );
    
    
    #gazefh = open( ingazefile, "r" ); #REV: not binary
    #if not gazefh:
    #    print("REV: error no gaze file [{}]".format(ingazefile));
    #    exit(1);
    #    pass;
    
    ret, frame = cap.read();

    fridx=0;
    tsec=0.0;
    tdelta = 1.0/fps;
    
    gazedf = pd.read_csv( ingazefile );

    blurwriter = cv2.VideoWriter();
    centwriter = cv2.VideoWriter();
    uncentwriter = cv2.VideoWriter();
    origwriter = cv2.VideoWriter();

    odir = os.path.dirname(invidfile);
    
    centeyeposdf = pd.DataFrame( columns=["FRIDX", "CX", "CY"] );
    eyeposoutfname = os.path.join( odir, "cent_eyepos.csv");
    
    while( ret ):
        #line = gazefh.readline();
        #if not line:
        #    print("REV: have frame, but no new eye position (EOF eye positions file {})?!".format(ingazefile));
        #    break;
        #
        #dat = line.split(); #REV: default split by whitespace
        #fr=int(dat[0]); #index
        #t=float(dat[1]); #sec
        #gzx=int(dat[2]); #px
        #gzy=int(dat[3]); #px
        tsec = fridx * tdelta;
        tsec2 = tsec + tdelta;
        
        df = gazedf[ (gazedf.Tsec >= tsec) & (gazedf.Tsec < tsec2 ) ];
        #print( df.gaze2d_0 );
        #print( df.gaze2d_1 );
        #gzx = np.mean( df.gaze2d_0[ ~np.isnan[df.gaze2d_0] ] );
        #gzy = np.mean( df.gaze2d_1[ ~np.isnan[df.gaze2d_1] ] );
        gzx = np.mean( df.gaze2d_0[ df.gaze2d_0.notna() ] );
        gzy = np.mean( df.gaze2d_1[ df.gaze2d_1.notna() ] );
        #REV: now, get average gaze location etc. from here I guess...
        
        gzx *= frame.shape[1];
        gzy *= frame.shape[0];
        
        nframe = frame.copy();
        
        
        #REV: tobii3 gaze2 references is from BOTTOM LEFT of frame. We need to "offset" frame by gaze amount inversed.
        #gzx = None;
        #gzy = None;
        if( np.isnan(gzx) or np.isnan(gzy) ):
            res = blackbgimg.copy() / 255;
            uncentres = res;
            pass;
        else:
            cgzx = gzx - frame.shape[1]/2; #REV: centered.
            cgzy = gzy - frame.shape[0]/2; #REV: centered. (note positive is up)
            
            #We must offset gaze by inverse of this number.
            bg_gzx = bgwid/2 + (-1 * cgzx);
            bg_gzy = bghei/2 + (-1 * cgzy); #this is from BOTTOM
            bg_gzy = bghei - bg_gzy; #REV: this is from TOP
            
            toadd = [ fridx, bg_gzx/bgwid, bg_gzy/bghei]; #REV: now from TOP, note!
            print(toadd)
            centeyeposdf.loc[ len(centeyeposdf.index) ] = toadd;
            imgmeanpx=np.mean(np.mean(nframe, axis=0), axis=0); #REV: compress along x then y, just leave 3 channels means.
            print(imgmeanpx);
            
            bgimg = np.full( (bghei,bgwid,3), imgmeanpx, dtype=np.uint8 );
            res = embed_edge_gauss_blur( frame, bg_gzx, bg_gzy, bgimg, blursizepx );
            
            cx = bgimg.shape[1]/2;
            cy = bgimg.shape[0]/2;
            uncentres = embed_edge_gauss_blur( frame, cx, cy, bgimg, blursizepx );
            
            #REV: draw circle...
            mygzx = int(gzx);
            mygzy = int(frame.shape[0]-gzy);
            nframe = cv2.circle( nframe, [mygzx, mygzy], 60, [220,50,50], 15 );
            pass;
        
        print("Gaze ({},{})".format(gzx,gzy));
        
        scaledown=4;
        
        if( scaledown != 1 ):
            suncentres = cv2.resize( uncentres, (int(res.shape[1]/scaledown), int(res.shape[0]/scaledown)) );
            sres = cv2.resize( res, (int(res.shape[1]/scaledown), int(res.shape[0]/scaledown)) );
            snframe = cv2.resize( nframe, (int(nframe.shape[1]/scaledown), int(nframe.shape[0]/scaledown)) );
            pass;
        else:
            suncentres = uncentres;
            sres = res;
            snframe = nframe;
            pass;

        startt = time.time();
        #REV: this includes the scaledown!
        sresb = blur_it( sres, 50, 100, scaledown );
        elap = time.time()-startt;
        print("Blur took {:4.1f} msec".format(1000*elap));
        
        #REV: draw circle and show "normal" too
        showhere=False;
        if(showhere):
            cv2.imshow("Result", sres);
            cv2.imshow("Orig", snframe);
            cv2.imshow("Blur", sresb);
            cv2.imshow("Uncentered", suncentres);
            cv2.waitKey(1);
            print("Processing frame {}".format(fridx));
        
        #REV: save each frame
        if( not blurwriter.isOpened() ):
            #fps=30; same as input vid
            fourcc = cv2.VideoWriter_fourcc(*"MP4V");
            isColor = True;
            
            
            blurname = os.path.join(odir, "blur.mp4");
            centname = os.path.join(odir, "cent.mp4");
            uncentname = os.path.join(odir, "uncent.mp4");
            origname = os.path.join(odir, "orig.mp4");
            
            blurwriter.open( blurname, fourcc=fourcc, fps=fps, frameSize=(sresb.shape[1], sresb.shape[0]), isColor=isColor );
            centwriter.open( centname, fourcc=fourcc, fps=fps, frameSize=(sres.shape[1], sres.shape[0]), isColor=isColor );
            uncentwriter.open( uncentname, fourcc=fourcc, fps=fps, frameSize=(suncentres.shape[1], suncentres.shape[0]), isColor=isColor );
            origwriter.open( origname, fourcc=fourcc, fps=fps, frameSize=(snframe.shape[1], snframe.shape[0]), isColor=isColor );
            pass;

        if( not blurwriter.isOpened() ):
            print("Error");
            exit(1);
            pass;
        
        blurwriter.write( (sresb*255).astype(np.uint8) );
        centwriter.write( (sres*255).astype(np.uint8) );
        uncentwriter.write( (suncentres*255).astype(np.uint8) );
        origwriter.write( (snframe*255).astype(np.uint8) );
                
        ret, frame = cap.read();
        fridx+=1;
        pass;

    centeyeposdf.to_csv(eyeposoutfname, index=False );
    
    #REV: end __main__
    pass;
