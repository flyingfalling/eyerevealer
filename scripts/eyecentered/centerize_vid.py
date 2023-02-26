##REV: centerize video based on eye position (i.e. make retinotopic, and apply peripheral blur, etc.)

#REV: note input eye positions must be in format positive goes DOWN and positive goes RIGHT in videos frame.
#REV: note input eye positions must be in range [0,1] where 0 is lowest part of image and 1 is max part of image
#REV: this is typical for 2d eye positions in tobii2 and tobii3 (REV: and pupil invis?)

#REV: note this refers to the LENS-DISTORTED IMAGE. To convert to "true" X/Y angles we need to undistort using image lens intrinsic
#     matrix...
#REV: or, better, use the 3d points (gp3? or gaze3d?), but then we need to project to image...( do they not undistort it?)

import os;
import sys;
import cv2;
import numpy as np;
import pandas as pd;
import time;

def embed_edge_gauss_blur_create( inimg255, cxpx, cypx, bgwid, bghei, bgrgb255, blurlimpx, blursigmult=1/3.0 ):
    bgimg = np.full( (bghei, bgwid, 3), bgrgb255, dtype=np.uint8 );
    return embed_edge_gauss_blur( inimg255, cxpx, cypx, bgimg, blurlimpx, blursigmult );

def create_maskblurred_3chan_embedder( inimg255, blurlimpx, blursigmult=1/3 ):
    sigmax=blurlimpx*blursigmult;
    
    mask = np.zeros( inimg255.shape, dtype=np.uint8 );
    mask[ int(blurlimpx):int(mask.shape[0]-blurlimpx), int(blurlimpx):int(mask.shape[1]-blurlimpx), : ] = 255;
    
    mask_blurred  = cv2.GaussianBlur( mask, ksize=(0,0), sigmaX=sigmax, sigmaY=0, borderType=cv2.BORDER_CONSTANT );
    mask_blurred_3chan = mask_blurred/255.0; #cv2.cvtColor(mask_blurred, cv2.COLOR_GRAY2BGR).astype('float') / 255.0;
    return mask_blurred_3chan;

def embed_edge_gauss_blur( inimg255, cxpx, cypx, bgimg255, mask_blurred_3chan ):
    etime1=time.time();
    
    print("Creating masks: {}".format(time.time()-etime1));
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

    etime2=time.time();
    print("Embedding: {} msec".format(1000*(etime2-etime1)));
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
    method='linear';
    if( method == 'linear' ):
        slope_per_dva=0.2;
        dpc = 1/fovea_0_sf + slope_per_dva * ecc_dva; #REV: this is how many cycles per degree... I want to find the sigma value in frequency space that gives this some value, i.e. 0.6? At sigma,
        cpd = 1/dpc;
        # This value will be 0.6, i.e. if sigma=cpd, it will pass 0.6 power (?) of it. I want to pass more of it...let's say 0.9
        freqmod=0.75;
        dvasig, _ = sigma_from_freq_modul( cpd, freqmod );
        print(dvasig);
        pass;
    else:
        base=0.833;
        mult = base ** ecc_dva; #REV: this is proportion of the max I get. I could convert to stuff, but I'll just do directly...
        sf = mult * fovea_0_sf;
        freqmod=0.75;
        dvasig, _ = sigma_from_freq_modul( sf, freqmod );
        pass;
    
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


#REV: do it way more efficiently, using some numpy or scikit or some shit, pytorch or etc., tensors.
#REV: we want:
#     for each pixel (which is an index, let's say it's just 1d?) -> the pixel has a list of "source indices" and their weights. The weights are based on the kernel size (I can keep those stored).
#     note many of these will be "convolutional" but most different size...

#REV: was planning to use (py)torch, and do e.g. input=1920x1080(x3) -> weights would be differently weighted projection of the input, with each "slice"

#  I1 -> W11 W12 W13 W14
#  I2 -> W21 W22 W23 W24
#  I3 -> W31 W32 W33 W34
#  I4 -> W41 W42 W43 W44
#        O1  O2  O3  O4    Where O1 = I1*W11 + I2*W21 + I3*W31 + I4*W41  (i.e. dot product), or, matrix multiply (if one is transposed appropriately) i.e. 1xm * mx1 (rows x cols)

#REV: but for 1920*1080 that is already 1920*1080*(3 channels)*(1 byte) = 24.88 mbyte per image, and we would have image-size weights per pixel, i.e. roughly square it. Still 12899.45 GIGABYTES, i.e. 13TB
#REV: just to hold the weights matrix. Haha. So, I need it sparse. Otherwise, linear layers are what I want!

#REV: even at 960x540, if I use sparse, and we use a sparse matrix of e.g. max 1/6 the size for the largest gaussian, that is still 960*540*(960/6*540/6) = 7.47 GB...fuck. This works better if I can
#REV: do the image in "chunks" based roughly on the size, remembering where each pixel goes afterwards...only the outermost will need to have such large weights. Ideally I should just subsample...
#REV: blurring is stupid...? Should I just compute infinitely many pyramid levels and use those? They are not decimated each time though... I should really blur it first so as to not lose info?
#REV: but...I will lose info. By definition. The light will...miss the receptors. Fuck it. Just subsample at some very local area, i.e. skip appropriate amounts? Should I sample with gaussian weights
#REV: or simply sample fewer further out (all with weight 1?). I.e. always sample middle pixel? Or compute it, then randomly sample indices to sample from to make it less "predictable"? Or distribute them in some type of balanced but poisson way so it is not absolutely uniform? Move with gaussian?
#REV: OK...let's just try embedding, but let us do it with some sub-sampling?

#REV: each pixel has an "index" of the kernel to use (compute just unique values). Those kernels are pre-computed and stored in memory as ksize for each (not so much space)...hopefully not many uniques.
#REV: I know that each is also centered at itself (so no issue there). The problem is that I don't want to do so many samplings/multiplications? I should be able to "sample" weights and target indices
#REV: in some way...using a mask? The problem is that each will still access each...ugh. I need to subsample in some natural way. Sample same number of receptors, just in less or more area/space?
#REV: eventually they will all be inside the same pixel (in which case, just sum their weights). We may get artifacts from pixel boundaries...but hopefully very small. Define distances not in terms of pixel offsets, but in terms of evenly space helixal structure? (hexa structure?). Or, octo structure? Just go in 8 directions, and the "amount" to go will be based on gaussian? I.e. much fewer further out...may get a "lucky pixel" though? Many falling in same area will catch...what? Doesn't matter... Let us say that we want to sample weights in the 8 directions with 20 samples in each direction, i.e. 20*8 = 160 samples.
#"distance" of those 20 should be determined by probability cumulatives, i.e. cdf of normal. So, each of the 5%iles. Note we start at center of pixel, so 0.5 is the edge of it in any direction (although...
# they are fucking square! So diagnals are fucked lol. Also, we won't get a nice circular sampling...it will only go in 8 directions. I don't like that. Should use some kind of spring thing to balance it.

def create_torch_nn():
    pass;


def mask_from_kern2d(kern):
    #kern is a list of (x, y, w)...
    kmat = np.array(kern);
    if( kmat.shape[1] != 3 ):
        print("Erro shape {}".format(kmat.shape));
        exit(1);
        pass;

    xs = kmat[:,0];
    ys = kmat[:,1];
    ws = kmat[:,2];
    
    kxmin = np.min(xs);
    kxmax = np.max(xs);
    kymin = np.min(ys);
    kymax = np.max(ys);
    
    #print(kmat);
    #print(xs);
    xspan = int(kxmax - kxmin)+1;
    yspan = int(kymax - kymin)+1;
    
    mask = np.zeros( (yspan, xspan), dtype=float );
    for i in kern:
        if( i[0] < kxmin or i[1] < kymin ):
            print("Err");
            exit(1);
            pass;
        #print("Accessing {} {} ({} {})".format(i[1], i[0], kxmin, kymin));
        mask[ int(i[1]-kymin), int(i[0]-kxmin) ] = i[2];
        pass;
    return mask;

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
    
def samples_from_2d_sym_gauss( N, sig ):
    samps = np.abs(np.random.normal( loc=0, scale=sig, size=N ));
    angles = np.pi * np.random.uniform(0, 2, size=N);
    xs = np.sqrt(samps) * np.cos(angles)
    ys = np.sqrt(samps) * np.sin(angles)
    return xs, ys

#REV: basically returns pixel index values and weights in case multiple fall inside. Note, pixels are square...
def samples_into_indices( N, sig ):
    xs, ys = samples_from_2d_sym_gauss(N, sig);
    #print(xs, np.std(xs))
    #print(ys, np.std(ys));
    #REV: compress, and int-ize.
    #REV: mult by 2 so that it is not 0.5, but 1 that we are dealing with... i.e. +1 to +3 is +1? Will get 2.9 to 2. Then when I divide again, I div 2 and add 0.5 and then int it? Or just make them
    #REV: all odd numbers! Should be 0, 2, 4. Note, up to 1.49999 it should be 1, then 1.5 should be 2.0. Mult 2 will make it 3. So, yea, just add 1? Mult 2, add 1. I.e. 0.5*2 + 1 = 2. 1.2*2 + 1 = 3.4 (3). 1.5*2+1 = 4 (4 -> 2)
    xs2 = xs*2 + 1*np.sign(xs);
    ys2 = ys*2 + 1*np.sign(ys);
    xs2 = np.fix(np.fix(xs2) / 2); #int division...
    ys2 = np.fix(np.fix(ys2) / 2); #REV: huh wtf?
    
    #print(xs2);
    #print(ys2);
    #xys = np.array(xs2, ys2);
    xys = [ (x, y) for x, y in zip(xs2,ys2) ];
    xys, ws = np.unique( xys, axis=0, return_counts=True );
    #print(xys)
    #print( ws);
    ws = ws / np.sum(ws);
    
    kern = [ (v[0], v[1], w) for v, w in zip( xys, ws ) ];
    return kern;


#REV: I could make special kernels (dispensations) for edge conditions? Need to remember "center" for each?
#REV: or just do online?
def create_gauss_mask_kerns(sigmapxs):
    kerns=[];
    uniques = np.unique(sigmapxs);
    kernidxs = np.zeros( sigmapxs.shape, dtype=np.uint );
    for i, u in enumerate(uniques):
        kernidxs[ np.where(sigmapxs == u) ] = i;
        if( u <= 0.3 ):
            k = np.ones((1,1), dtype=float);
            pass;
        else:
            sigmapx = u;
            ksize = int(((sigmapx-0.8)/0.3 + 1)/0.5 + 1);
            if( ksize % 2 != 1 ):
                ksize+=1;
                pass;
            k = cv2.getGaussianKernel(ksize, sigmapx); #REV: will give me correct size with cutoff? Better to just sep kernel it with mask? Yea...let's do that LOL
            k = np.outer(k,k);
            pass;
        k = np.stack((k,)*3, axis=-1); #REV make 3 channels...
        kerns.append(k);
        pass;
    return kernidxs, kerns;

def create_gauss_subsample_kerns(sigmapxs, samps=100):
    #sample = np.vectorize( partial( sample_into_indices, N=samps ) );
    #kerns = sample(sigmapxs);
    
    #REV: oh fuck this is the same as a gaussian convolution kernel type thing, but different for each location! note SIZE could be same, but actual spread will be larger or smaller...
    kerns=[];
    uniques = np.unique(sigmapxs);
    kernidxs = np.zeros( sigmapxs.shape, dtype=np.uint );
    #print(sigmapxs.shape);    
    for i, u in enumerate(uniques):
        kernidxs[ np.where(sigmapxs == u) ] = i;
        
        #REV; fuck this can't work because of edge conditions. Have to handle it in real time while iterating image ;( Or build it into independent kernels for each...
        k = samples_into_indices(N=samps, sig=u);
        kerns.append(k);
        pass;
    
    return kernidxs, kerns;


def apply_mask_kernel2(idx, img, kernidxs, kerns):
    x=idx[1];
    y=idx[0];
    w=img.shape[1];
    h=img.shape[0];
    
    kern2d3=kerns[ kernidxs[y,x] ];
    ksize=kern2d3.shape[0];
    
    halfk = int(ksize/2);
    kernL = int(min(halfk, x));
    kernR = int(min(halfk, w-x-1)); #i.e. if x is 999 and w is 1000, w-x is 1, so there is zero on that side.
    kernT = int(min(halfk, y));
    kernB = int(min(halfk, h-y-1));
    
    weightsum = np.sum(kern2d3[ (halfk-kernT):(halfk+kernB+1), (halfk-kernL):(halfk+kernR+1)], axis=(0,1));
    res = kern2d3[ (halfk-kernT):(halfk+kernB+1), (halfk-kernL):(halfk+kernR+1)] * img[ (y-kernT):(y+kernB+1), (x-kernL):(x+kernR+1), :];
    return np.sum(res, axis=(0,1)) / weightsum;

def apply_mask_kernel(idx, img, kernidxs, kerns):
    x=idx[1];
    y=idx[0];
    w=img.shape[1];
    h=img.shape[0];

    sigmapx = kernidxs[y,x];

    #REV: don't bother blurring < 0.3 px...
    if( sigmapx < 0.3 ):
        return img[y,x];
    
    
    ksize = int(((sigmapx-0.8)/0.3 + 1)/0.5 + 1);
    if( ksize % 2 != 1 ):
        ksize+=1;
        pass;
    
    
    halfk = int(ksize/2);
    kernL = int(min(halfk, x));
    kernR = int(min(halfk, w-x-1)); #i.e. if x is 999 and w is 1000, w-x is 1, so there is zero on that side.
    kernT = int(min(halfk, y));
    kernB = int(min(halfk, h-y-1));
    
    tmpblurred = cv2.GaussianBlur( img[ (y-kernT):(y+kernB+1), (x-kernL):(x+kernR+1), :], ksize=(0,0), sigmaX=sigmapx, sigmaY=0 );
    #weightsum = np.sum(kern2d3[ (halfk-kernT):(halfk+kernB+1), (halfk-kernL):(halfk+kernR+1)]);
    #res = kern2d3[ (halfk-kernT):(halfk+kernB+1), (halfk-kernL):(halfk+kernR+1)] * img[ (y-kernT):(y+kernB+1), (x-kernL):(x+kernR+1), :];
    res = tmpblurred[ kernT, kernL ];
    #print(res);
    return res;


def apply_kernel(idx, img, kernidxs, kerns):
    x=idx[1];
    y=idx[0];
    #print("Apply for {} {}".format(x,y));
    kern=kerns[ kernidxs[y,x] ];
    #print(kern);
    sumw=0;
    res=np.zeros(img[y,x].shape);
    #print(res);
    
    
    #print(len(kern));
    for (dx, dy, w) in kern:
        #print(dx,dy,w);
        tx = int(x+dx);
        ty = int(y+dy);
        
        if( tx < 0 or tx >= img.shape[1] or ty < 0 or ty >= img.shape[0] ):
            #print("Outside {} {}!".format(tx, ty));
            continue;
        #print(ty, tx); #REV: ok, accessing img is the issue? Is it copying it wtf?
        #print(img[ty,tx]);
        res += w * img[ty,tx];
        sumw += w;
        pass;
    
    #print("Finish");
    #print(m);
    #m = mask_from_kern2d( kern );
    #m/=np.max(m);
    #cv2.imshow("mask", m);
    #cv2.waitKey(0);

    return res / sumw;


def blur_concentric_gauss_mask(img, kernidxs, kerns):
    #REV; vectorize application? or multithread it? Fuck it?
    indices = list( zip(np.indices(img.shape)[0].flatten(), np.indices(img.shape)[1].flatten())); #REV: generator...but OK?
    indices = indices[0::3];
    #print(indices);
    #REV: implicit argument needs to be the first one.
    res=[];
    '''
    for idx in indices:
        print("For {}".format(idx));
        r = apply_mask_kernel(idx, img=img, kernidxs=kernidxs, kerns=kerns );
        #print(r);
        res.append(r);
        pass;
    '''
    nthreads=2;
    with Pool(nthreads) as mypool:
        #res = np.array(mypool.map( partial( apply_mask_kernel2, img=img, kernidxs=kernidxs, kerns=kerns), indices ) );
        res = np.array(mypool.map( partial( apply_mask_kernel, img=img, kernidxs=kernidxs, kerns=kerns), indices ) );
        pass;
    
    #res = np.map( partial( apply_kernel, img=img, kernidxs=kernidxs, kerns=kerns), indices );
    #res = np.vectorize( partial( apply_kernel, img=img, kernidxs=kernidxs, kerns=kerns) )( indices );
    
    res = res.reshape(img.shape);
    
    return res;
 
def blur_concentric_gauss_subsample(img, kernidxs, kerns):
    #REV; vectorize application? or multithread it? Fuck it?
    indices = list( zip(np.indices(img.shape)[0].flatten(), np.indices(img.shape)[1].flatten())); #REV: generator...but OK?
    indices = indices[0::3];
    #print(indices);
    #REV: implicit argument needs to be the first one.
    with Pool(12) as mypool:
        res = np.array(mypool.map( partial( apply_kernel, img=img, kernidxs=kernidxs, kerns=kerns), indices ) );
        pass;
    #res = np.map( partial( apply_kernel, img=img, kernidxs=kernidxs, kerns=kerns), indices );
    #res = np.vectorize( partial( apply_kernel, img=img, kernidxs=kernidxs, kerns=kerns) )( indices );
    
    res = res.reshape(img.shape);
    
    return res;



#REV: this does it but with full-image blurs... leave edge up to opencv...
def blur_concentric_full( img, kernidxs, kerns ):
    #REV: kerns is empty
    uniques = np.unique(kernidxs);
    res = img.copy();
    time1=time.time();
    print(len(uniques));
    for i, sigpx in enumerate(uniques):
        #REV: blur img at that level...
        blurred  = cv2.GaussianBlur( img, ksize=(0,0), sigmaX=sigpx, sigmaY=0 );
        locs = np.where(kernidxs==sigpx);
        res[ locs ] = blurred[ locs ];
        pass;
    
    time2=time.time();
    print("Real Blur took: {} sec".format(1e3*(time2-time1)));
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
    
    maxsigcutoff = 4;
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

def create_kerns( img, dva_per_px ):
    #dva_per_pix = scaledown*(95.0/(1920));
    time1=time.time();
    cx = img.shape[1]/2;
    cy = img.shape[0]/2;
    distspx = pixel_dists_from( img, cx, cy );
    time2=time.time();
    print(1e3*(time2-time1));
    distsdva = distspx * dva_per_px;

    #REV: different method...
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
    
    maxsigcutoff = 8; #REV: corresponds to... 1/(2*pi*10) (1/(2*pi*10)) = 0.0159 dva sigma (SD) of frequency response... I.e. at 1 sd, i.e. exp( -(sigma*sigma) / (2*sigma*sigma) ) = exp(-0.5) = 0.606530
    #REV: i.e. 0.0159 ... wait what. If it is 10dva width (sigma) that corresponds to...what? Frequency response of that KERNEL (filter), i.e. it will pass things of < 0.0159 dva? Let's say it
    #REV: corresponds to SPATIAL FREQUENCY. (FREQUENCY response, i.e. wavelength will be inverse of frequency...). -> 62.89 DVA PER CYCLE!
    #REV: problem, it must increase by 0.2 deg to maintain recognition in periphery. If e.g. an object is 1/20 of a degree in the middle, each degree it must get 0.2deg larger (NOT SCALE).
    #REV: i.e. up to? some minimum. Fuck it. So, if at middle it is 1/20, (0.05 deg), at 5 deg it must be 1deg large, at 10, it must be 2deg large, at 15 3 deg, at 20, 4 deg,
    #REV: so...I should drop off linearly (in terms of blur?). 1/(1/x) is just linear lol. What is "acuity" -> it is resolvable CPD. How can I remove resolvable CPD -> blur it to remove that frequency
    #REV: response. 20 in middle (1/20 deg per cycle), to 1 deg/cycle (1cyc/deg). at 45 deg it must be 0.2*45 = 9 degrees... so my blur sigma should at 45 be roughly that.
    #REV: lets just use 1/20 (at center) to 0.25 at 1 deg to 1.05 at 5 deg to etc.9.05 at 45 deg...for something 9 deg to be resolvable, (i.e. 1/9 cpd? ish? Or 1/18?). That means
    #REV; 1/9 cpd (note 20 cpd takes 1/20 of a degree). 1/9 cpd takes 9 degrees. 9 degrees means that a blur of (assuming nyquist frequency is very small for image) -> I must leave 1/9 = 0.1111 frequency
    #REV: response...is 1.432 CPD!! (note the other one I start in center at 8 CPD, then do 4 2 1 1/2 1/4...fine.
    sigsdva[ (sigsdva>maxsigcutoff) ] = maxsigcutoff;
    sigspx = sigsdva/dva_per_px;
    
    #kernidxs, kerns = create_gauss_subsample_kerns( sigspx );
    
    #kernidxs, kerns = create_gauss_mask_kerns( sigspx );
    
    #kernidxs, kerns = create_gauss_kerns( sigspx ); #REV: this just creates sigpx for each...i.e. it is literally just the
    kernidxs = sigspx;
    kerns = None;
    
    return kernidxs, kerns;
    
def blur_it_full( img, kernidxs, kerns ):
    blurimg = blur_concentric_full(img, kernidxs, kerns);
    return blurimg;

def blur_it_kern( img, kernidxs, kerns ):
    blurimg = blur_concentric_gauss_subsample(img, kernidxs, kerns);
    return blurimg;

def blur_it_kern_mask( img, kernidxs, kerns ):
    blurimg = blur_concentric_gauss_mask(img, kernidxs, kerns);
    return blurimg;

#REV: img received as float32, [0,1]
def blur_it( img, blursig, blurpxrad, scaledown ):
    
    #print(img.shape); #960*2*2...
    #blurimg = blur_concentric( img, scaledown*(95.0/(1920)) );
    blurimg = blur_concentric_bypixel( img, scaledown*(95.0/(1920)) )
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
    #orig_dva_per_pix = sys.argv[3]; #0.0494792 for tobii3  0.0427083 for tobii2 (assuming size is 1920?)
    camtype = sys.argv[3];
    
    
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

    if( wid != 1920):
        print("WARNING: input video wid is not expected raw (1920 wid) for tobii");
        pass;
    if( camtype == 'tobii2' ):
        orig_dva_per_pix = wid/82;
        pass;
    elif( camtype == 'tobii3' ):
        orig_dva_per_pix = wid/95;
        pass;
    else:
        print("Error, unrecognized cam type [{}] (should be tobii2 or tobii3 for now)".format(camtype));
        exit(1);
        pass;
        
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

    
    kerns=None;
    kernidxs=None;

    mask_3d = create_maskblurred_3chan_embedder( frame,  blursizepx );
    
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
        
        print("Frame [{}/{}]  Time: {}".format(fridx, nframes, tsec));
        df = gazedf[ (gazedf.Tsec >= tsec) & (gazedf.Tsec < tsec2 ) ];
        #print( df.gaze2d_0 );
        #print( df.gaze2d_1 );
        #gzx = np.mean( df.gaze2d_0[ ~np.isnan[df.gaze2d_0] ] );
        #gzy = np.mean( df.gaze2d_1[ ~np.isnan[df.gaze2d_1] ] );
        
        #REV: TODO 27 feb 2023
        #REV: really should remove outliers? I.e. points that diverge from mean by...? (or "jumps")?
        #REV: should do it *before* this point...
        #REV: should remove saccades entirely? Or mark them as being contained in saccades?
        gzx = np.mean( df.gaze2d_0[ df.gaze2d_0.notna() ] );
        gzy = np.mean( df.gaze2d_1[ df.gaze2d_1.notna() ] );
        #REV: now, get average gaze location etc. from here I guess...
        
        gzx *= frame.shape[1];
        gzy *= frame.shape[0];
        
        nframe = frame.copy();
        
        startt = time.time();
        
        #REV: tobii3 gaze2 references is from BOTTOM LEFT of frame. We need to "offset" frame by gaze amount inversed.
        #gzx = None;
        #gzy = None;
        if( np.isnan(gzx) or np.isnan(gzy) ):
            res = blackbgimg.copy() / 255;
            uncentres = res;
            pass;
        else:
            cgzx = gzx - frame.shape[1]/2; #REV: centered.
            cgzy = gzy - frame.shape[0]/2; #REV: centered. (note positive is DOWN)
            
            #We must offset gaze by inverse of this number.
            bg_gzx = bgwid/2 + (-1 * cgzx); #from left (positive RIGHT)
            bg_gzy = bghei/2 + (-1 * cgzy); #this is from TOP (positive is DOWN)
            #bg_gzy = bghei - bg_gzy; #REV: this is from TOP 24 feb 2023 changed (fixed)
            
            toadd = [ fridx, bg_gzx/bgwid, bg_gzy/bghei]; #REV: now from TOP, note!
            print(toadd)
            centeyeposdf.loc[ len(centeyeposdf.index) ] = toadd;
            imgmeanpx=np.mean(np.mean(nframe, axis=0), axis=0); #REV: compress along x then y, just leave 3 channels means.
            print(imgmeanpx);
            
            bgimg = np.full( (bghei,bgwid,3), imgmeanpx, dtype=np.uint8 );
            res = embed_edge_gauss_blur( frame, bg_gzx, bg_gzy, bgimg, mask_3d );
            #REV: bg_gz is center + -centeredgaze. I.e. if looking "up" (I will have lower i.e. 0.25).
            #REV: Center is lets say 0.5. I want to "offset" as if I'm looking there. I.e. so I hould offset it "down"
            #  cgz = -w/2 to +w/2
            # bg_gz is w/2 + (-cgz), i.e. if cgz is -0.25 (looking "up"), then shift will be w/2 + (-1 * -0.25), i.e. 0.75, I.e.
            # shift down. OK....?
            
            cx = bgimg.shape[1]/2;
            cy = bgimg.shape[0]/2;
            uncentres = embed_edge_gauss_blur( frame, cx, cy, bgimg, mask_3d );
            
            #REV: draw circle...
            mygzx = int(gzx);
            mygzy = int(frame.shape[0]-gzy);
            cv2.circle( nframe, [mygzx, mygzy], 60, [220,50,50], 15 );
            pass;
        
        print("Gaze ({},{})".format(gzx,gzy));
        
        scaledown=6;
        
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
        
        elap = time.time()-startt;
        print("Pre-blur took {:4.1f} msec".format(1000*elap));
        
        startt = time.time();
        
        if( kernidxs is None and kerns is None ):
            dva_per_pix = scaledown * orig_dva_per_pix; #REV: hardcoded values...for tobii3
            kernidxs, kerns = create_kerns(sres, dva_per_pix);
            pass;
        
        #REV: this includes the scaledown!
        #sresb = blur_it( sres, 50, 100, scaledown );
        sresb = blur_it_kern_mask( sres, kernidxs, kerns );
        #sresb = blur_it_full( sres, kernidxs, kerns );
        
        elap = time.time()-startt;
        print("Blur took {:4.1f} msec".format(1000*elap));
        
        #REV: draw circle and show "normal" too
        showhere=False;#True;
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
            fourcc = cv2.VideoWriter_fourcc(*"H264");
            isColor = True;
            
            ext=".mkv";
            blurname = os.path.join(odir, "blur"+ext);
            centname = os.path.join(odir, "cent"+ext);
            uncentname = os.path.join(odir, "uncent"+ext);
            origname = os.path.join(odir, "orig"+ext);
            
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
        origwriter.write( (snframe).astype(np.uint8) ); #REV: don't *255!
                
        ret, frame = cap.read();
        fridx+=1;
        pass;

    centeyeposdf.to_csv(eyeposoutfname, index=False );
    
    #REV: end __main__
    pass;
