import numpy as np
import pandas as pd
import sys
import matplotlib.pyplot as plt

infile = sys.argv[1];

df = pd.read_csv(infile);

ntypes=len(df.groupby(['blurdva', 'blur', 'saltype'])); #36, i.e. 3 * 2 * 6 i.e. 12*3 i.e. 36 ok.
print(ntypes);

fig, axs = plt.subplots(ntypes, 2, figsize=(5,40));


print(df.groupby(['blurdva', 'blur', 'saltype']).mean());

print(df.groupby(['blurdva', 'blur', 'saltype']).mean().max());

print(np.sum(df.pct - df.roc));


idx=0;
for grp, gdf in df.groupby(['blurdva', 'blur', 'saltype']):
    #print(grp);
    #print( gdf[['pct','roc']].mean() );
    h1 = axs[idx,0].hist( gdf.roc, bins=20 );
    h2 = axs[idx,1].hist( gdf.pct, bins=20 );
    axs[idx,0].set_title( 'ROC {} {:3.2f}'.format(grp, gdf.roc.mean()) );
    axs[idx,1].set_title( 'PCT {} {:3.2f}'.format(grp, gdf.pct.mean()) ); #REV: this isn't "all", which mixes up all together...
    #print(h1);
    #print(h2);
    idx+=1;
    pass;

fig.tight_layout();

fig.savefig('rochist.pdf');
