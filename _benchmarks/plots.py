
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

tx_no = lines[1].split(":")[-1]
utxo_list = []
times_list = []
cache_size_list = []
info_list = []

print("tx_no: %s" % tx_no)


for i in range(2, len(lines) - 3, 4):
    #print(lines[i].split(":"))
    utxo_no = lines[i].split(":")[-1]
    cpu_cycles = lines[i+1].split(",")[-1]
    cache_size = lines[i+2].split(":")[-1]
    #print("utxo_no: %s, cpu_cycles: %s, cache_size: %s" % (utxo_no, cpu_cycles, cache_size))
    utxo_list.append(int(utxo_no))
    times_list.append(int(cpu_cycles))
    cache_size_list.append(cache_size)
    info_list.append(utxo_no + " tx\n" + cache_size + " B")


print("END")

# line1 = ax1.plot(utxo_list, 'o-')
# # ylabel("-dbcache=300 (default)")
#
# # now, the second axes that shares the x-axis with the ax1
# ax2 = fig1.add_subplot(111, sharex=ax1, frameon=False)
# line2 = ax2.plot(cache_size_list, 'xr-')
# ax2.yaxis.tick_right()
# ax2.yaxis.set_label_position("right")
# # ylabel("-dbcache=5")
#
# # y_line =
#
# # for the legend, remember that we used two different axes so, we need
# # to build the legend manually
# legend((line1, line2), ("1", "2"))
# show()
#


x = times_list
# x1 = cache_size_list[1:5]
y = utxo_list
print (x)
print (y)
print (info_list)

# fig = plt.figure()
# fig,ax = plt.subplots()

fig = plt.figure(figsize=(15,15))
ax = plt.subplot(111)


#x=x[1:800]
#y=y[1:800]

ax.plot(x,y)
ax.scatter(x,y)

info_indices = np.arange(0, len(info_list), 10)
print(info_indices)

ax.set_yticks(info_indices) # choose which x locations to have ticks
ax.set_yticklabels(info_list) #

#plt.yticks(y, info_list)


#print(len(y))
#print(len(info_list))
#info_list = np.arange(0, len(info_list), 30)


#print(info_list)


#ax.set_yticks(info_list)

#plt.yticks(info_list)


plt.xlabel("CPU cycles")
plt.ylabel("tx number\ncache size")
plt.title("NO Cache Flush")
# plt.legend((x, y), ("cpu cycles", "UTXO number\ncache size"))

# ax1.set_ylabel('UTXO number\nCache size')

fig.savefig(output)
# plt.show()

#
# ax2 = ax1.twinx()
# ax2.plot(x2, y, 'r-')
# ax2.set_ylabel('x2', color='r')
# for tl in ax2.get_yticklabels():
#     tl.set_color('r')
#
