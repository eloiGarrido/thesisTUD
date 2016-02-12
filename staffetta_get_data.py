#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: Eloi Garrido
# @Date:   2016-02-12
__author__ = 'egarrido'
import sys

'''
Log Converter
convert Cooja results into statistical data and graphs
'''
general_path = "~/thesisTUDelft/eh_staffetta/tests/"

class LogConverter(object):

    def __init__(self, filename, number_of_nodes):
        try:
            content = open(filename).read().split(',')
            print ('>> Reading file {}'.format(filename))

            self.output_filename = '{}.log'.format(filename)
            self.content = content[1:]
            self.output = []

            self.nodes = []
            self.number_of_nodes = number_of_nodes

            # Add here function calls to output data
            self.create_structure()
            self.parse()
        except Exception as e:
            print ('>> Open file error: ',e)



    def create_structure(self):
        '''
        Create data structure,  array of dictionaries containing all node information
        '''
        for i in range (0, self.number_of_nodes):
            self.nodes.append({'id':i, 'time':[], 'strobe_ack':[], 'num_wakeups':[], 'on_time': [], 'avg_edc':[], 'seq':[], 'node_energy_state':[], 'remaining_energy':[], 'harvesting_rate':[]})

#TODO Create a function to output each type of file data
    def output_energy_values(self, filename):
        txt_name = str(filename)

        with open(txt_name, 'w') as fp:
            fp.write('\n'.join(self.output))



    def parse(self):
        '''
        Parses each line
        '''
        for line in self.content:
            if len(line) == 0:
                continue



if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python log_converter.py <LOG_FILENAME>')
        exit(1)
    adapter = LogConverter(sys.argv[1], sys.argv[2])