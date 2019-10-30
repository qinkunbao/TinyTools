import numpy as np
import matplotlib.pyplot as plt
import statistics
import os
from collections import Counter
import csv

# Parameters specified
#24000
PTcapacity = 5
cachelineCapacity = 3




class LRUCache(object):
    """A sample class that implements LRU algorithm"""

    def __init__(self, length):
        self.length = length
        self.hash = {}
        self.item_list = []
        self.in_mem_read = {}
        self.swapped_history = {}
        self.count = 0
        self.record = {}

    def visitItem(self, item):
        """Insert new items to cache"""

        self.count += 1
        if item in self.hash:
            # Add 1 to visit number counts
            # Move the existing item to the head of item_list.
            self.hash[item] += 1
            item_index = self.item_list.index(item)
            self.item_list[:] = self.item_list[:item_index] + self.item_list[item_index+1:]
            self.item_list.insert(0, item)
        else:
            # Remove the last item if the length of cache exceeds the upper bound.
            if len(self.item_list) >= self.length:
                #print('swap out:'+self.item_list[-1])
                self.removeItem(self.item_list[-1])
            # If this is a new item, just append it to
            # the front of item_list.
            self.hash[item] = 1
            self.item_list.insert(0, item)
            if item not in self.record:
                self.record[item] = [[self.count]]
            else:
                self.record[item].append([self.count])
            #print('swap in:'+item)

    def removeItem(self, item):
        """Remove LRU items"""
        # Add the count to the history dictionary
        if item not in self.swapped_history:
        	self.swapped_history[item] = []
        self.swapped_history[item].append(self.hash[item])
        self.record[item][len(self.record[item])-1].extend((self.hash[item], self.count))
        self.hash.pop(item)
        self.item_list.remove(item)


    def showStats(self):
        for item in self.hash:
            if item not in self.swapped_history:
                self.swapped_history[item] = []
            self.swapped_history[item].append(self.hash[item])
            self.record[item][len(self.record[item])-1].extend((self.hash[item], -1))
        #print(self.record)
        f = open("dict.txt","w")
        for k, v in self.record.items():
            f.write(str(k) + ': '+ str(v) + '\n')
        f.close()
        # w = csv.writer(open("output.csv", "w"))
        # for key, val in self.record.items():
        #     w.writerow([key, val])


# read file and split
with open('tiny_trace.txt', 'r') as f0:
#trace = f0.read()
    target = [i.rstrip("\n").split(' ')[2] for i in f0]
#op = [i.rstrip("\n").split(' ')[1] for i in f0]
print('finished reading')
# get page and cache line visits by shifting the addresses
pages = [i[:-3] for i in target]
#print(pages)
#page_cum = Counter(pages)
#print(page_cum)

#lines = [bin(int(i[2:], 16))[2:-6] for i in target]
#print(lines)

# build LRU cache simulator and print result
print('page swap record:')
Pcache = LRUCache(PTcapacity)
for i in pages:
	Pcache.visitItem(i)
print('######################')
print('show access count of pages for each period in memory:')
Pcache.showStats()
# Lcache = LRUCache(cachelineCapacity)
# print('######################')
# print('cache line swap record:')
# for i in lines:
# 	Lcache.visitItem(i)
# print('######################')
# print('show access count of cachelines for each period in memory:')
# Lcache.showStats()

