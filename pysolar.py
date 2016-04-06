# !/usr/bin/env python

__author__ = 'egarrido'
import sys
import matplotlib.pyplot as plt
import os
import shutil
from operator import add

def read_file():
	lines = [line.rstrip('\n') for line in open('solar_model.txt')]
	plt.figure()
	plt.plot(lines)
	plt.show()
#--------------------------- Main Function ---------------------------#
if __name__ == '__main__':
    '''
    Main function call, pass file name + number of nodes.
    '''
    # if len(sys.argv) < 2:
    #     print('Usage: python log_converter.py <LOG_FILENAME>')
    #     print len(sys.argv)
    #     exit(1)
    # adapter = LogConverter(sys.argv[1], int(sys.argv[2]))
    read_file()
