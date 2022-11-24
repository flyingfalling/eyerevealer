

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys
import seaborn as sns

condfn = sys.argv[1];
dir = sys.argv[2];

conddf = pd.read_csv(condfn, sep=' ');

#DIR, SUBJ, POS, TIME

saldf = pd.DataFrame( columns=['SUBJ', 'POS', 'TIME', 'SALTYPE', 'SALPCT', 'COLOR'] );

def fileexists( fn ):
    val=True;
    try:
        f = open(fn);
        val=True;
        f.close();
    except IOError:
        print("File {} not accessible".format(fn));
        val=False;
    return val;


def get_mean_roc( fn ):
    if( fileexists(fn) ):
        df = pd.read_csv( fn, sep=' ' );
        return np.mean( df['SALPCT'] );
    else:
        return None;

for i in range(len(conddf)):
    row = conddf.iloc[i];
    #print(row);
    #print( row['DIR'], row['SUBJ'], row['POS'], row['TIME'] );
    mydir=row['DIR'];

    tobiirocf=dir+"/"+mydir+"/TOBII_fin.mkv.roc";
    rsdeprocf=dir+"/"+mydir+"/RSDEPTH_fin.mkv.roc";
    rsnodeprocf=dir+"/"+mydir+"/RSNODEPTH_fin.mkv.roc";

    tobiiroc = get_mean_roc(tobiirocf);
    rsdeproc = get_mean_roc(rsdeprocf);
    rsnodeproc = get_mean_roc(rsnodeprocf);
    

    #saldf.loc[len(saldf)] = [ row['SUBJ'], row['POS'], row['TIME'], 'TOBII', tobiiroc ];
    saldf.loc[len(saldf)] = [ row['SUBJ'], row['POS'], row['TIME'], 0, tobiiroc, (0,0,0,0) ];
    if( rsdeproc ):
        #saldf.loc[len(saldf)] = [ row['SUBJ'], row['POS'], row['TIME'], 'RSDEPTH', rsdeproc ];
        saldf.loc[len(saldf)] = [ row['SUBJ'], row['POS'], row['TIME'], 1, rsdeproc, (1,0,0,0) ];

    #saldf.loc[len(saldf)] = [ row['SUBJ'], row['POS'], row['TIME'], 'RSNODEPTH', rsnodeproc ];
    saldf.loc[len(saldf)] = [ row['SUBJ'], row['POS'], row['TIME'], 2, rsnodeproc, (1,0,1,0) ];


print( saldf );



#REV: make some plots... diff subjects = different colors

#fig = plt.figure(); #constrained_layout=True, figsize=(8, 5))


#REV: model type is color (black, red, or purple) for tobii, rs-dep, rs+dep

#REV: subjects are...shades? Linetypes!


#Plot by day/night and driving/passenger (4 plots total?)
#collist=['black', 'red', 'purple'] * 4;
#print(collist);

#g = sns.FacetGrid(data=saldf, row="POS", col="TIME", margin_titles=True, hue="SALTYPE", palette=['black','red','purple'])
g = sns.FacetGrid(data=saldf, row="POS", col="TIME", margin_titles=True, hue="SUBJ", palette=['orange','green','blue','gray'])
#g.map(sns.regplot, "SALTYPE", "SALPCT", fit_reg=False, x_jitter=.1) #color="0.3"
g.map(sns.regplot, "SALTYPE", "SALPCT", fit_reg=False, x_jitter=.1) #color="0.3"
#g.map(sns.catplot, "SALTYPE", "SALPCT", x_jitter=.1) #color="0.3"

#sns.catplot(y=saldf["SALPCT"], x=saldf["SALTYPE"], row=saldf["POS"], col=saldf["TIME"]);

#rowvars=['SALPCT']  #saldf.POS.unique();
#colvars=saldf.TIME.unique();

#g=sns.PairGrid(data=saldf, x_vars=colvars, y_vars=colvars);
#g.map(sns.violinplot);

g.set_axis_labels("Device/Model Type", "Saliency Percentile")
g.set(xticks=[0, 1, 2]);
g.set(ylim=[0,1]);
g.set(yticks=[0, 0.25, 0.5, 0.75, 1.0]);
g.set(xticklabels=['Tobii', 'RS-depth', 'RS+depth']);

for axr in g.axes:
    for axc in axr:
        axc.axhline(0.5, ls='--');


plt.show();




#REV: gaze data I guess?
