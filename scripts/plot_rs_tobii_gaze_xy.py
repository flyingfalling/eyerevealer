import pandas as pd
import numpy as np
import sys
import matplotlib.pyplot as plt


tobiigzfn = sys.argv[1];
rsgazefn = sys.argv[2];


tobiidf = pd.read_csv( tobiigzfn, sep=' ');
rsdf = pd.read_csv( rsgazefn, sep=' ');

tts = tobiidf['TSEC'];
txs = tobiidf['X'];
tys = tobiidf['Y'];

OFFSETSEC=0.52
rts = rsdf['TSEC']+OFFSETSEC;
rxs = rsdf['X'];
rys = rsdf['Y'];



fig = plt.figure();

plt.plot( tts, txs, linestyle='--', color='black' );
plt.plot( tts, tys, linestyle='-', color='black');

plt.plot( rts, rxs, linestyle='--', color='red' );
plt.plot( rts, rys, linestyle='-', color='red' );

plt.legend(['Tobii X', 'Tobii Y', 'RS X', 'RS Y']);

plt.xlim([1000, 1005]);
plt.xlabel("Time (sec)");
plt.ylabel("Norm. gaze X/Y");

plt.show();
