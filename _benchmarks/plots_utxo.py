
from pylab import figure, show, legend, ylabel
import matplotlib.pyplot as plt
import numpy as np
import sys
import matplotlib.ticker as plticker


# create the general figure
# fig = figure(figsize=(10,10))

# and the first axes using subplot populated with data
# ax1 = fig.add_subplot(111)

#Benchmark,count,min,max,average,min_cycles,max_cycles,average_cycles
# number of transactions: 2
# UTXO per transaction: 2
# CCoinsCaching,180224,0.000005889713066,0.000006434565876,0.000005953029772,15265,16678,15430
# coins cache size: 4


input=sys.argv[1]
output=sys.argv[2]

f1 = open(input)
#f2 = open('tmp2_0')

lines = f1.read().splitlines()
#lines = lines[25:]


tx_no = lines[1].split(":")[-1]
utxo_list = []
times_list = []
cache_size_list = []
info_list = []

print("tx_no: %s" % tx_no)


for i in range(2, len(lines) - 3, 2):

    '''
    if not lines[i].startswith('number') and not lines[i].startswith('CCoinsCaching'):
	#print(lines[i])
	continue;
    '''

    utxo_no = lines[i].split(":")
    cpu_cycles = lines[i+1].split(",")

    if len(utxo_no) >= 1:
	utxo_no = utxo_no[-1]
    if len(cpu_cycles) >= 1:
	cpu_cycles = cpu_cycles[-1]    

    #print("utxo_no: %s, cpu_cycles: %s" % (utxo_no, cpu_cycles))
    utxo_list.append(int(utxo_no))
    times_list.append(int(cpu_cycles))

print("END")


avg_time=reduce(lambda x, y: x + y, times_list) / float(len(times_list))


x = utxo_list
y = times_list

print (x)
print (y)
print (info_list)

# fig = plt.figure()
# fig,ax = plt.subplots()

fig = plt.figure(figsize=(15,15))
ax = plt.subplot(111)

#print the average access time
ax.text(0.5, 0.9,'avg time='+ `avg_time`, fontsize=20, ha='center', va='center', transform=ax.transAxes,bbox=dict(facecolor='white', alpha=1))

#x=x[1:800]
#y=y[1:800]


#cache compare
#ax.set_ylim([10400,11500])

#db
#ax.set_ylim([10000,20000])
#ax.set_ylim([17000000,22000000])
#ax.set_ylim([100000,1300000000])
#ax.set_ylim([100000,800000000])
#ax.set_ylim([100000,39000000])
ax.set_ylim([100000,10000000])

ax.plot(x,y)
ax.scatter(x,y)

'''
info_indices = np.arange(0, max(y), 40)

ax.set_ylim(10,max(y))

ax.set_yticks(info_indices) # choose which x locations to have ticks
ax.set_yticklabels(info_list) #
'''

#the same but for the other axis
'''
info_indices = np.arange(0, max(x), 40)

ax.set_ylim(10,max(x))

ax.set_xticks(info_indices) # choose which x locations to have ticks
ax.set_xticklabels(info_list) #
'''


#ax.set_yticks(info_list)
#plt.yticks(info_list)


plt.xlabel("UTXO number")
plt.ylabel("CPU cycles")
plt.title("Sqlite Batch Write")
# plt.legend((x, y), ("cpu cycles", "UTXO number\ncache size"))

# ax1.set_ylabel('UTXO number\nCache size')

fig.savefig(output)


