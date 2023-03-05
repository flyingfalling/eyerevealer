

import pandas as pd
import numpy as np
import sys


infname=sys.argv[1];
if( len(sys.argv) > 2 ):
    resamplerate=sys.argv[2];
    pass;
else:
    resamplerate=100;
    pass;

outfname=infname + "_flat.csv";

#REV: read in, and then melt/merge
#REV: note sep is space!
#TSEC IDX VAR ELEM VAL

#Create a column for each VAR_ELEM (within a given IDX)
#Create a row for each TSEC
sep=' ';
df = pd.read_csv( infname, sep=sep );
print(df);

mydict={};
columns=[];
for grp, gdf in df.groupby(['VAR', 'ELEM']):
    columns.append( str(grp[0]) + "_" + str(grp[1]) );
    if grp[0] in mydict:
        mydict[grp[0]].append(grp[1]);
        pass;
    else:
        mydict[grp[0]] = [grp[1]];
        pass;
    pass;

print(columns);

newcols=[];
for key in mydict:
    if( len(mydict[key]) < 2 ):
        newcols.append(key);
        pass;
    else:
        for i in mydict[key]:
            newcols.append( key + "_" + str(i) );
            pass;
        pass;
    pass;

print(newcols);

#REV: now, for those with categorical values (eye?) figure out what to do
#REV: note "status" is already only those with 0 error...i.e. no "s"

#REV: get all those which have eye in their thing, and append to every single name.

#REV: right now it is fine...it will just have separate line for some? No...not good...same time point, different eye...

#REV: goal: create blah_left and blah_right for only those "groups" which also contain an "eye". So, group by IDX where IDX subset contains an "eye" line.

#REV: need to get 

#df=df[ (df.TSEC >= 0) & (df.TSEC < 240 ) ];

df.drop_duplicates(inplace=True);

#REV: if it contains "eye" in VAR, append that to each guy :)
typedict={};
for grp, gdf in df.groupby('IDX'):

    if( grp % 1000 == 0 ):
        print("IDX: {}".format(grp))
        pass;
    
    #print(gdf.VAR.tolist());
    name = tuple(gdf.VAR.tolist());
    if name not in typedict:
        typedict[name] = [gdf];
        pass;
    typedict[name].append(gdf);
    pass;

dflist=[]
idx=0;
for key in typedict:
    print("For KEY {} in typdict".format(key));
    typedict[key] = pd.concat( typedict[key] );
    typedict[key]["typeidx"] = idx;
    dflist.append(typedict[key]);
    idx+=1;
    pass;
#REV: now, typeidx represents the type of each...now, within a timepoint, get the values for that...if they have different values we have an issue?
#REV: if gazeidx is different for example? It should not be? Fuck is there gazeidx for acc? No there is not...

#REV: if typeidx ELEM unique contains EYE, make it into another typeidx?

#REV: fuck, sampling of acc and gyro etc. may be OFFSET from gaze in some way? So the whole point of "resampling" was to give some new sampling rate,
#REV: in which I got the "closest" value of gyro etc...problem is if I "copy" it is may make problems with algorightms etc.?
#REV: worry about that afterwards...

#REV: do it on same row or else everything will get fucked up. Or specify "binoc" or not.
#REV: OK...

#REV: for each timepoint, check the unique guys available... note I need to already have those "named".
#REV: fuck it just do it slowly lol

#REV: what items are unique to only one typeidx?

finaldf = pd.concat(dflist);
print(finaldf);

before=len(finaldf.index);
print(len(finaldf.index));
finaldf.drop_duplicates(inplace=True);
print(len(finaldf.index));
after= len(finaldf.index);
if( before != after ):
    print("finaldf changed size: {} {}".format( before, after));

uniquesbytype={};

for grp, gdf in finaldf.groupby('typeidx'):
    myvals = gdf.VAR.unique();
    
    for v in myvals:
        ti = finaldf[ finaldf.VAR == v ].typeidx.unique();
        if( len( ti ) == 1 ):
            print("VAR: {} is UNIQUE for TYPE IDX {}  ({})".format(v, grp, myvals));
            if( 'eye' in myvals ):
                #print("Should rename {} to {}".format(v, v+"_"+gdf.eye
                if grp not in uniquesbytype:
                    uniquesbytype[grp] = [];
                    pass;
                uniquesbytype[grp].append( v );
                pass;
            pass;
        else:
            print("VAR: {} is SHARED by {} TYPEIDXS ({})".format(v, len(ti), ti));
            pass;
        pass;
    pass;

print(uniquesbytype);

#REV: now to go slowly and rename for each timepoint...getting from appropriate guy?

#REV: for each timepoint, create a dict? Add to it (if exists check they are same!)
biglist=[];
for grp, gdf in finaldf.groupby('TSEC'):
    thisdict={'TSEC': grp};
    
    for tgrp, tgdf in gdf.groupby('typeidx'):
        #REV: convert each one
        
        for mygrp, mydf in tgdf.groupby('IDX'):
            ev='';
            #print(mydf.VAR);
            if 'eye' in mydf.VAR.tolist():
                ev = mydf[ mydf.VAR=='eye' ].VAL.iloc[0];
                #print("GOT EYE {}".format(ev));
                pass;
            else:
                #print("NO EYE IN VAR");
                pass;

            #REV: remove it...
            mydf = mydf[ mydf.VAR != 'eye' ];
            
            #print(mydf);
            
            for g, d in mydf.groupby(['VAR', 'ELEM']):
                elempost = '';
                grpn=g[0];
                                    
                if( len(mydf[ (mydf.VAR==grpn) ].index) > 1 ):
                    if( grpn == 'l' or grpn == 'gidx' ):
                        print(mydf[ (mydf.VAR==grpn) ]);
                    
                    elempost = "_" + str(g[1]);
                    pass;
                
                
                #print("For G={}".format(grpn));
                if tgrp in uniquesbytype and grpn in uniquesbytype[tgrp]:
                    #print("{} was UNIQUE".format(grpn));
                    myname = str(grpn) + "_" + ev + elempost;
                    pass;
                else:
                    #print("PASSED OVER {} i.e. not UNIQUE".format(g));
                    myname = str(grpn) + elempost;
                    pass;
                
                if( len(d.index) > 1 ):
                    #print(d);
                    #print("REV: error >1 len");
                    #exit(1);
                    pass;
                val = d.VAL.iloc[0];
                if myname in thisdict:
                    #print("NAME {} already in dict! (old val: {}) (new val: {})".format(myname, thisdict[myname], val));
                    if( val != thisdict[myname] ):
                        print("Do not match! {} was in dict, {} is new".format(thisdict[myname], val));
                        print(d);
                        print(thisdict);
                        exit(1);
                    pass;
                else:
                    #print("SETTING NAME {} in dict! (new val: {})".format(myname, val));
                    thisdict[myname] = val;
                    pass;
                pass;
            pass;
    biglist.append(thisdict);
    pass;

finaldf2 = pd.DataFrame( biglist );
print(finaldf2.columns);
print(finaldf2);


#REV: resample at hz. (for every element, not some are only every 10 hz etc.? Should I "stretch" on a per-column basis?). Nah just smooth after.
dt = 1/resamplerate;
tsecs = np.arange(finaldf2.TSEC.min(), finaldf2.TSEC.max(), dt);
newdflist=[];

cols = finaldf2.columns;
finaldf2[cols] = finaldf2[cols].apply(pd.to_numeric, errors='coerce');
print(finaldf2);
for t in tsecs:
    startt=t;
    endt=t+dt;
    tmp=finaldf2[ (finaldf2.TSEC >= startt) & (finaldf2.TSEC < endt ) ];
    res = tmp.mean(axis=0);
    #pd.DataFrame(res));
    #print( type(res) );
    #print(tmp.mean(axis=0));
    newdflist.append( res.to_frame().T ); #REV: will auto-exclude nans...
    pass;

resampdf = pd.concat(newdflist);
print(resampdf);

#import matplotlib.pyplot as plt
#plotdf = resampdf[ (~resampdf.gp_0.isnull()) & (~resampdf.gp_1.isnull())];
#plt.hist2d(plotdf.gp_0, plotdf.gp_1, bins=(50,50), cmap=plt.cm.jet);
#plt.show();

print("Writing to CSV {}".format(outfname));
resampdf.to_csv(outfname, index=False);
print("FINISHED");

exit(0);
