import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

df = pd.read_csv('raw-2022-12-19-12-38-06_blur.mkv.csv.csv')
df2 = pd.read_csv('raw-2022-12-19-12-38-06_uncent.mkv.csv.csv')
df3 = pd.read_csv('raw-2022-12-19-12-38-06_cent.mkv.csv')
df3 = pd.read_csv('raw-2022-12-19-12-38-06_cent.csv')
df["myt"] = 'blur'
df2["myt"] = 'uncent'
df3["myt"] = 'cent'
df = pd.concat([df, df2, df3])

#g = sns.FacetGrid(df, row="blurdva", column="saltype", 
meandf = df.groupby(['blurdva','blur','saltype','myt'], as_index=False).mean()
g = sns.catplot( x="blur", y="pctle", hue="myt", col="blurdva", row="saltype", data=meandf , kind="bar", height=4, aspect=.7);
plt.show()


#import readline
#for i in range(readline.get_current_history_length()):
#    print (readline.get_history_item(i + 1))
