
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
input2=sys.argv[2]

output=sys.argv[3]

f1 = open(input)
f2 = open(input2)

lines = f1.read().splitlines()
lines2 = f2.read().splitlines()


tx_no = lines[1].split(":")[-1]
utxo_list = []
times_list = []
times_list2 = []


cache_size_list = []
info_list = []

print("tx_no: %s" % tx_no)




for i in range(2, len(lines) - 3, 2):
    utxo_no = lines[i].split(":")
    cpu_cycles = lines[i+1].split(":")
    cpu_cycles2 = lines2[i+1].split(":")

    if len(utxo_no) >= 1:
	utxo_no = utxo_no[-1]
    if len(cpu_cycles) >= 1:
#	cpu_cycles = cpu_cycles[-4]
	cpu_cycles = cpu_cycles[-1]

    if len(cpu_cycles2) >= 1:
#	cpu_cycles2 = cpu_cycles2[-4].split(" ")[0]
	cpu_cycles2 = cpu_cycles2[-1].split(" ")[0]

    #print("utxo_no: %s, cpu_cycles: %s" % (utxo_no, cpu_cycles))
    utxo_list.append(int(utxo_no))
    times_list.append(float(cpu_cycles))
    times_list2.append(float(cpu_cycles2))

print("END")


avg_time1=reduce(lambda x, y: x + y, times_list) / float(len(times_list))
avg_time2=reduce(lambda x, y: x + y, times_list2) / float(len(times_list2))
print(avg_time1)
print(avg_time2)

print(times_list)
fig = plt.figure(figsize=(15,15))
ax = plt.subplot(111)

#ax.set_ylim(0, 50)


ax.plot(utxo_list,times_list, label='LevelDb. Avg time: '+ `avg_time1`)
ax.scatter(utxo_list,times_list)

ax.plot(utxo_list, times_list2, label='Sqlite with Map. Avg time: '+ `avg_time2`)
ax.scatter(utxo_list, times_list2)
plt.legend(loc='upper left', prop={'size': 20});

plt.xlabel("Number of transaction")
plt.ylabel("Elapsed time")
plt.title("Write Operation")

fig.savefig(output)


