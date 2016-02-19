#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: Eloi Garrido
# @Date:   2016-02-12
__author__ = 'egarrido'
import sys
import matplotlib.pyplot as plt

'''
Log Converter
convert Cooja results into statistical data and graphs
'''
general_path = "/home/egarrido/contiki/tools/cooja/build/"
file_path = "/home/egarrido/staffetta_sensys2015/eh_staffetta/"
class LogConverter(object):

    def __init__(self, filename, number_of_nodes):
        # try:
            # content = open(filename).read().split(',')
            # print ('>> Reading file {}'.format(filename))
            #
            # self.output_filename = '{}.log'.format(filename)
            # self.content = content[1:]
        self.output = []

        self.nodes = []
        self.number_of_nodes = number_of_nodes

        # Add here function calls to output data
        self.create_structure()
        # print self.nodes
        print len(self.nodes)
        self.read_file(filename)



        self.output_pkt_seq("origSeq.txt")

        self.generateGraphs()
        # except Exception as e:
        #     print ('>> Error on LogConverter: ',e)



    def create_structure(self):
        '''
        Create data structure,  array of dictionaries containing all node information
        '''
        for i in range (0, self.number_of_nodes):
            self.nodes.append({'id':i, 'time2':[], 'time3':[], 'time4':[], 'time6':[], 'time_on': [], 'time_off': [], 'pkt':[], 'num_wakeups':[], 'on_time': [], 'avg_edc':[], 'seq':[], 'node_energy_state':[], 'remaining_energy':[], 'harvesting_rate':[]})

#--------------------------- Output Functions ---------------------------#
#TODO Create a function to output each type of file data
    def output_energy_values(self, filename):
        txt_name = str(filename)

        with open(txt_name, 'w') as fp:
            fp.write('\n'.join(self.output))

    def output_pkt_seq(self, filename):
        print '>> Writing packet sequence file...'
        txt_name = file_path + str(filename)

        with open(txt_name, 'w') as fp:
            for i in range (0, self.number_of_nodes-1):
                fp.write(str(self.nodes[i]['seq'])+"\n")
        fp.close()

    def output_sink_file(self, filename):
        print '>> Writing sink file...'
        txt_name = file_path + str(filename)
        with open(txt_name, 'w') as fp:
            # TODO fix this output for the sink
            fp.write(str(self.nodes[0]['seq'])+"\n")
        fp.close()
#--------------------------- Read and Store Functions ---------------------------#
    def read_file(self, filename):
        file_name = general_path + filename

        # try:
        print ('>> Reading file: ' + file_name + '...')
        f = open(file_name,'r')
        for line in f:
            self.parse(line)
        f.close()

        # except Exception as e:
        #     print ('>> Error, No file with that name: ',e)

        print ('>> Reading done')

    def format_seq(self, msg):
        result = ""
        for i in range (3,len(msg)):
            result += str(msg[i])
            result += ","
        return result


    def store_information(self,id,time,msg_type,msg):
        # print id
        # print msg
        if msg_type == 2:
            self.nodes[id-1]['num_wakeups'].append(msg[3])
            self.nodes[id-1]['time2'].append(time)
        elif msg_type == 3:
            self.nodes[id-1]['on_time'].append(msg[3])
            self.nodes[id-1]['avg_edc'].append(msg[4])
            self.nodes[id-1]['time3'].append(time)
        elif msg_type == 4:
            self.nodes[id-1]['seq'].append(self.format_seq(msg))
            self.nodes[id-1]['time4'].append(time)
        elif msg_type == 6:
            self.nodes[id-1]['node_energy_state'].append(msg[3])
            self.nodes[id-1]['remaining_energy'].append(msg[4])
            self.nodes[id-1]['harvesting_rate'].append(msg[5])
            self.nodes[id-1]['time6'].append(time)
        elif msg_type == 7: #Packet path (Sink)
            self.nodes[id-1]['pkt'].append(msg[3] + msg[4] + msg[5])
        elif msg_type == 8: #Packet path (Node)
            self.nodes[id-1]['pkt'].append(msg[3] + msg[4] + msg[5])
        elif msg_type == 9: #Node goes ON
            self.nodes[id-1]['time_off'].append(time)
        elif msg_type == 10:#Node goes OFF
            self.nodes[id-1]['time_on'].append(time)

    def parse(self, line):
        '''
        Parse each line
        '''
        if len(line) == 0:
            return False
        else:
            msg = line.split('|')
            if msg[0].isdigit() == False or msg[2].isdigit() == False:
                return False
            else:
                print msg
                id = int(msg[0])
                time = int(msg[1])
                msg_type = int(msg[2])
                self.store_information(id,time,msg_type,msg)
                return True

#--------------------------- Printing Functions ---------------------------#
    def format_figure(self,title, xlab, ylab, filename):
        plt.title(title)
        plt.xlabel(xlab)
        plt.ylabel(ylab)
        plt.draw()
        plt.savefig(filename)

    def printEnergyLevels(self):
        print ('>> Printing energy levels...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['remaining_energy'])

        self.format_figure('Node Energy Levels','Time', 'Energy', 'node_energy')
        return

    def printNodeState(self):
        print ('>> Printing node state...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['node_energy_state'])

        self.format_figure('Node State','Time', 'State', 'node_state')
        return

    def printHarvestingRate(self):
        print ('>> Printing harvesting rate...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['harvesting_rate'])

        self.format_figure('Harvesting Rate','Time', 'HR', 'node_harv')
        return

    def printAvgEdc(self):
        print ('>> Printing average EDC...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['avg_edc'])

        self.format_figure('Node Avg EDC','Time', 'Metric', 'avg_edc')
        return

    def printWakeups(self):
        print ('>> Printing number of wake-ups...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['num_wakeups'])

        self.format_figure('Node Number of Wake-ups','Time', 'Wake-ups', 'wakeups')
        return

    def printOnTime(self):
        print ('>> Printing ON time...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['on_time'])
        self.format_figure('Node ON Time','Time', 'ON Time', 'on_time')
        return

    def generateGraphs(self):
        print ('>> Generating graphics...')
        self.printAvgEdc()
        self.printEnergyLevels()
        self.printHarvestingRate()
        self.printNodeState()
        self.printOnTime()
        self.printWakeups()
        plt.show()
        return

#--------------------------- Main Function ---------------------------#
if __name__ == '__main__':
    '''
    Main function call, pass file name + number of nodes.
    '''
    if len(sys.argv) < 2:
        print('Usage: python log_converter.py <LOG_FILENAME>')
        print len(sys.argv)
        exit(1)
    adapter = LogConverter(sys.argv[1], int(sys.argv[2]))
