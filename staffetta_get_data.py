#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: Eloi Garrido
# @Date:   2016-02-12
__author__ = 'egarrido'
import sys
import matplotlib.pyplot as plt
import os
import shutil
import numpy as np
import scipy
from operator import add
'''
Log Converter
convert Cooja results into statistical data and graphs
'''
# env = 'uni'
env = 'home'
repeated = True
# simulation = 'orig'
simulation = 'eh'
model = 'solar'
# model = 'bernoulli'
# model = 'mover'
energy = 'energest'
# energy = 'noEnergest'
RV = '10k'
nodes = '11'
duration = '5min'
age = 'slow4Age'
# age = 'noAge'
simulation_name = str(simulation) + "_" + str(env) + "_" + str(model) + "_" + str(age) + "_" + str(energy) + "_" + str(RV) + "_" + str(nodes) + "_" + str(duration)





file_path = ""
if env == 'uni':
    if ( repeated == True):
        general_path = "/home/egarrido/"
    else:
        general_path = "/home/egarrido/contiki/tools/cooja/build/"

    if simulation == 'orig':
        file_path = "/home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/" + simulation_name
    elif simulation == 'eh':
        file_path = "/home/egarrido/staffetta_sensys2015/eh_staffetta/results/eh_staffetta/" + simulation_name

elif env == 'home':
    if ( repeated == True):
        general_path = "/home/jester/"
    else:
        general_path = "/home/jester/contiki/tools/cooja/build/"

    if simulation == 'orig':
        file_path = "/home/jester/thesisTUDelft/eh_staffetta/results/original/" + simulation_name
    elif simulation == 'eh':
        file_path = "/home/jester/thesisTUDelft/eh_staffetta/results/eh_staffetta/" + simulation_name

not_created = 0
idx = 0
while not_created == 0:
    if os.path.isdir(file_path):
        file_path += "_" + str(idx)
    try:
        os.mkdir(file_path)
        file_path += "/"
        not_created = 1
    except:
        file_path = file_path.rstrip('_'+ str(idx))
        idx += 1

class LogConverter(object):

    def __init__(self, filename, number_of_nodes):
        self.nodes = []
        self.number_of_nodes = number_of_nodes

        # Add here function calls to output data
        self.create_structure()
        self.read_file(filename)


        pkts = self.organize_pkts()
        paths = self.create_pkt_path(pkts)
        self.output_file(paths,"paths",0)
        self.output_file(pkts, "packets",1)

        pkt_delay, pkt_delay_raw = self.get_end_to_end_delay(pkts)
        self.output_file(pkt_delay,"delay", 0)
        self.output_file(pkt_delay_raw,"delay_raw", 0)

        self.output_pkt_seq("origSeq")

        self.print_delay(pkt_delay)

        self.print_dc()
        self.print_drop_ratio(pkt_delay)

        # for i in range (1, number_of_nodes):
        #     self.print_rendezvous(i)
        # plt.show()

        self.generate_graphs()
        try:
            # shutil.copy( general_path + "COOJA.testlog", file_path )
            print('>>>> Moving File <<<<')
            print(general_path)
            print(filename)
            print(file_path)
            shutil.copy( general_path + filename, file_path )

        except:
            print ('>> Error when moving COOJA.testlog')

    def create_structure(self):
        '''
        Create data structure,  array of dictionaries containing all node information
        '''
        for i in range (0, self.number_of_nodes):
            self.nodes.append({'id':i, 'node_state': [], 'no_energy': 0, 'time5': [], 'accum_harvest':[], 'accum_consumption' :[],'rv_time':[], 'time2':[], 'time3':[], 'time4':[], 'time6':[], 'time_on': [], 'time_off': [], 'abs_time_off': [], 'pkt':[], 'num_wakeups':[], 'on_time': [], 'avg_edc':[], 'seq':[], 'node_energy_state':[], 'remaining_energy':[], 'harvesting_rate':[]})

#--------------------------- Output Functions ---------------------------#
#TODO Create a function to output each type of file data
    def output_energy_values(self, filename):
        txt_name = str(filename)

        with open(txt_name, 'w') as fp:
            fp.write('\n'.join(self.output))



    def output_file(self, element, filename, num):
        if num == 1:
            for i in range (0, len(element)):
                txt_name = file_path + str(filename) + str(i) + ".txt"
                with open(txt_name, 'w') as fp:
                    fp.write(str(element[i])+"\n")
                fp.close()
        elif num == 0:
            txt_name = file_path + str(filename) + ".txt"
            with open(txt_name, 'w') as fp:
                for i in range (0, len(element)):
                    fp.write(str(element[i])+"\n")
            fp.close()


    def output_pkt_seq(self, filename):
        print '>> Writing packet sequence file...'
        txt_name = file_path + str(filename) + ".txt"

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
        print ('>> Reading file: ' + file_name + '...')
        f = open(file_name,'r')
        for line in f:
            self.parse(line)
        f.close()
        print ('>> Reading done')

    def format_pkt_path(self, split_packet):

        main_split = []
        for l in range (0, len(split_packet)):
            splited = []
            for idx in range (0, len(split_packet[l])):
                splited.append([])
                if split_packet[l] != []:
                    if (split_packet[l][idx] not in splited) :
                        splited[idx].append(split_packet[l][idx])
                        for j in range (0, len(split_packet[l])):
                            src = splited[idx][0][3]
                            seq = splited[idx][0][2]
                            if (split_packet[l][j] not in splited[idx]) and split_packet[l][j][3] == src and split_packet[l][j][2] == seq:
                                splited[idx].append(split_packet[l][j])

                main_split.append(splited[idx])


        return main_split

    def create_pkt_path(self,pkts):
        print ('>> Creating packet paths...')
        pkt_paths = []
        split_packets = []
        path = {'src':999, 'seq': 999, 'path': []}
        for i in range (0, len(pkts)):
            split_packets.append([])
            if pkts[i] != []:
                pkts_t = pkts[i]
                for j in range (0, len(pkts_t)):
                    pkt_split = pkts_t[j].split(',')
                    del pkt_split[-1]
                    split_packets[i].append(pkt_split)

        main_split = self.format_pkt_path(split_packets)

        return main_split

    def repeated(self, orig_t, pkt_delay):
        result = False
        for i in range(0, len(pkt_delay)):
            if (int(orig_t[3]) == pkt_delay[i]['src']) and (int(orig_t[2]) == pkt_delay[i]['seq']):
                result = True
        return result

    def find_packet_index(self, seq, src, sink_packet):
        index = -1
        for pkt_idx in range(0, len(sink_packet)):
            if sink_packet[pkt_idx] == []:
                continue
            else:
                sink_t = sink_packet[pkt_idx].split(',')
                if sink_t[3] == src and sink_t[2] == seq:
                    index = pkt_idx
                    break
        return index

    def create_pkt_delay(self,orig_packet, sink_packet):
        pkt_delay = []
        print ('>> Create packet delay...')

        for nodes in range(1, self.number_of_nodes):
            for pkt_seq in range(0, len(self.nodes[nodes]['seq'])):
                seq = (self.nodes[nodes]['seq'][pkt_seq]).split(',')
                seq = seq[0]
                node = nodes+1
                index = self.find_packet_index(seq, str(node), self.nodes[0]['pkt'])
                if index == -1:
                    pkt_delay.append({'src': node, 'seq':seq, 'delay': 'lost' })
                else:
                    sink_t = self.nodes[0]['pkt'][index].split(',')
                    delay_t = long(sink_t[4]) - long( self.nodes[nodes]['time4'][pkt_seq] )
                    if delay_t < 0:
                        continue
                    else:
                        pkt_delay.append({'src': node, 'seq':seq, 'delay': str(delay_t) })
        sorted_pkt_list = sorted(pkt_delay, key=lambda k: (k['src'], int(k['seq'])))
        return sorted_pkt_list

    def create_pkt_delay_raw(self,orig_packet, sink_packet):
        pkt_delay = []
        print ('>> Create packet delay...')
        for idx in range(0, len(orig_packet)):
            orig_t = orig_packet[idx].split(',')
            if orig_packet[idx] != []:
                if sink_packet[idx] == []:
                    pkt_delay.append({'src':orig_t[3], 'seq':orig_t[2], 'delay': 'lost' })
                else:
                    sink_t = sink_packet[idx].split(',')
                    pkt_delay.append({'src':int(orig_t[3]), 'seq':int(orig_t[2]), 'delay': (int(sink_t[4]) - int(orig_t[4]))})

        sorted_pkt_list = sorted(pkt_delay, key=lambda k: (k['src'], k['seq']))
        return sorted_pkt_list


    def get_end_to_end_delay(self, packets):
        print ('>> Getting end-to-end delay...')
        orig_packet = []
        sink_packet = []
        for i in range(1, self.number_of_nodes):
            if self.nodes[i]['pkt'] != []:
                for len_pkt in range (0, len(self.nodes[i]['pkt'])):
                    packet_t = self.nodes[i]['pkt'][len_pkt].split(',')
                    if packet_t[0] == packet_t[3]: #Is a original packet
                        orig_packet.append(  self.nodes[i]['pkt'][len_pkt] )

        # Look for received packets timestamp
        for len_orig_pkt in range( 0 , len(orig_packet)):
            sink_packet.append([])
            for sink in range (0, len(self.nodes[0]['pkt'])):
                if self.nodes[0]['pkt'] != []:
                    packet_orig_t = orig_packet[len_orig_pkt].split(',')
                    packet_sink_t = self.nodes[0]['pkt'][sink].split(',')
                    if packet_sink_t[2] == packet_orig_t[2] and packet_sink_t[3] == packet_orig_t[3]: # Same origin and sequence code
                        sink_packet[len_orig_pkt] = self.nodes[0]['pkt'][sink]
                        break

        pkt_delay = self.create_pkt_delay(orig_packet, sink_packet)
        pkt_delay_raw = self.create_pkt_delay_raw(orig_packet, sink_packet)
        return pkt_delay ,pkt_delay_raw

    def format_seq(self, msg):
        result = ""
        for i in range (3,len(msg)):
            result += str(msg[i])
            result += ","
        return result


    def store_information(self,id,time,msg_type,msg):
        if msg_type == 2:
            self.nodes[id-1]['num_wakeups'].append(msg[3])
            self.nodes[id-1]['time2'].append(time)
        elif msg_type == 3:
            self.nodes[id-1]['on_time'].append(msg[3])
            # self.nodes[id-1]['avg_edc'].append(msg[4])
            self.nodes[id-1]['time3'].append(time)
        elif msg_type == 4:
            self.nodes[id-1]['seq'].append(self.format_seq(msg))
            self.nodes[id-1]['time4'].append(time)
        elif msg_type == 5:
            self.nodes[id-1]['no_energy'] += 1
            self.nodes[id-1]['time5'].append(time)
        elif msg_type == 6:
            self.nodes[id-1]['node_energy_state'].append(msg[3])
            self.nodes[id-1]['remaining_energy'].append(int(msg[4]))
            self.nodes[id-1]['harvesting_rate'].append(msg[5])
            self.nodes[id-1]['accum_consumption'].append(float(msg[6]))
            self.nodes[id-1]['accum_harvest'].append(float(msg[7]))
            self.nodes[id-1]['time6'].append(time)
        elif msg_type == 7: #Packet path (Sink)
            self.nodes[id-1]['pkt'].append(msg[3] + ',' +  msg[4] + ',' + msg[5] + ',' + msg[6] + ',' + str(time))
        elif msg_type == 8: #Packet path (Node)
            self.nodes[id-1]['pkt'].append(msg[3] + ',' + msg[4] + ',' + msg[5] + ',' + msg[6] + ',' + str(time))
        elif msg_type == 9: #Node goes OFF
            self.nodes[id-1]['time_off'].append(float(msg[3]))
            self.nodes[id-1]['abs_time_off'].append(time)
        elif msg_type == 10:#Node goes ON
            self.nodes[id-1]['time_on'].append(float(msg[3]))
        elif msg_type == 12: #Rendezvous time
            self.nodes[id-1]['rv_time'].append(msg[3])
        elif msg_type == 13: #Node energy state
            self.nodes[id-1]['node_state'].append(int(msg[3]))
        elif msg_type == 14:
            self.nodes[id-1]['avg_edc'].append(int(msg[3]))

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
                # print msg
                id = int(msg[0])
                time = int(msg[1])
                msg_type = int(msg[2])
                self.store_information(id,time,msg_type,msg)
                return True


    def organize_pkts(self):
        print ('>> Organizing packets....')
        pkts_with_origin = []
        for i in range(0, self.number_of_nodes):
            pkts_with_origin.append([])
        for i in range(0, self.number_of_nodes):
            for j in range(0, len(self.nodes[i]['pkt'])):
                temp = self.nodes[i]['pkt'][j]
                temp_t = temp.split(',')
                idx = int ( temp_t[len(temp_t)-2] )
                if (temp in pkts_with_origin[ idx-1 ]) == False:
                    pkts_with_origin[ idx -1 ].append(temp)
        return pkts_with_origin


    def get_node_dc(self, node_id):
        node_dc = []
        counter = 0.0
        total_on = 0.0
        # TODO THis calculation is wrong, uses the radio on time instead of printf 9 and 10
        for i in range (0, len(self.nodes[node_id]['time_on'])-1):

            # period = self.nodes[node_id]['time_off'][i+1] - self.nodes[node_id]['time_off'][i]
            period = abs(self.nodes[node_id]['time_off'][i+1] - self.nodes[node_id]['time_off'][i])
            on = abs( self.nodes[node_id]['time_off'][i] - self.nodes[node_id]['time_on'][i] )
            node_dc.append( float(on) / float(period) )

            total_on += on
            # total_on = sum(self.nodes[node_id]['time_on'])
            # total_on += abs( self.nodes[node_id]['time_on'][i] - self.nodes[node_id]['time_off'][i+1] )
            counter += 1.0
        try:
            # avg_dc_t = float(600000000) / float(total_on)
            # avg_dc =   float(total_on) / float(self.nodes[node_id]['time_off'][len(self.nodes[node_id]['time_off'])-1])
            avg_dc = float(total_on) / 600000000
            # print ('avg_dc_t: '+str(avg_dc_t) + ' avg_dc:'+str(avg_dc))
        except:
            avg_dc = 0
            avg_dc_t = 0
            print ('>> ERROR: No DC data')
        # avg_dc = avg_dc / counter
        return node_dc, avg_dc


#--------------------------- Printing Functions ---------------------------#
    def print_dead_node(self):
        print ('>> Printing dead occurrences...')
        plt.figure()
        for i in range (1, self.number_of_nodes-1):
            total_count = len(self.nodes[i]['time_on']) + int(self.nodes[i]['no_energy'])
            plt.bar(i,self.nodes[i]['no_energy'], align='center')
            print(float(self.nodes[i]['no_energy']))
            print(float(total_count))
            print('------------')
            plt.annotate( str( (100.0 * float(self.nodes[i]['no_energy'])) / float(total_count) ) + '%', xy=(i, 0))
        self.format_figure('Node dead times', 'Node', 'Occurrences', 'node_dead_times')


    def print_delay(self,pkt_delay):
        print ('>> Printing delay...')
        plt.figure()
        avg_delay_node = []
        counter = []
        acum = 0.0
        for i in range(0, self.number_of_nodes-1):
            avg_delay_node.append(0.0)
            counter.append(0.0)
        for i in range(0, len(pkt_delay)):
            node = pkt_delay[i]['src']
            if pkt_delay[i]['delay'] == 'lost':
                continue
            else:
                avg_delay_node[node-2] += float(pkt_delay[i]['delay'])
                counter[node-2] += 1.0

        for i in range (0, self.number_of_nodes-1):
            try:
                avg_delay_node[i] = avg_delay_node[i] / counter[i]
                acum += long(float(avg_delay_node[i] / (self.number_of_nodes - 1) ))
            except:
                print ('>> ERROR: No delay data on node ' + str(i))
                avg_delay_node[i] = 0.0
            plt.bar(i+1,avg_delay_node[i],align='center')

        plt.axhline(int(acum), color='r')
        plt.annotate(acum, xy=(self.number_of_nodes-2,int(acum) + 0.5))


        self.format_figure('Node average delay','Node', 'Delay', 'node_delay')

    def printf_node_state(self):
        print ('>> Printf node state...')
        plt.figure()
        for i in range(1, self.number_of_nodes):
            results = map(int,self.nodes[i]['node_energy_state'])
            plt.plot(results)
        self.format_figure('Node State', 'Node', 'State', 'node_state')

    def print_packet_created(self):
        print('>> Printing created packets...')
        plt.figure()
        for i in range(1, self.number_of_nodes):
            plt.bar(i, len(self.nodes[i]['seq']), align='center')

        self.format_figure('Packets Created', 'Node', 'Packets', 'packets_created')

    def print_drop_ratio(self, pkt_delay):
        print ('>> Printing packet drop ratio...')
        plt.figure()
        drop_pkt = []
        total_pkt = []
        total_created = []
        print ('>> Total packets created')
        for i in range(0, self.number_of_nodes):
            drop_pkt.append(0.0)
            total_pkt.append(0.0)
            total_created.append( len(self.nodes[i]['seq']) )

        for i in range(0, len(pkt_delay)):
            node = int(pkt_delay[i]['src'])
            total_pkt[node-1] += 1.0
            if pkt_delay[i]['delay'] == 'lost':
                drop_pkt[node-1] += 1.0

        for i in range(0, len(total_pkt)):
            try:
                plt.bar(i+1, ( drop_pkt[i] / total_pkt[i]),align='center' )
            except:
                plt.bar(i+1, 0 ,align='center')
        plt.axhline( sum(drop_pkt) / sum(total_pkt), color='r' )
        plt.annotate(str(sum(drop_pkt) / sum(total_pkt)), xy=(self.number_of_nodes-2 , (sum(drop_pkt) / sum(total_pkt)) + 0.05))

        self.format_figure('Node packet drop ratio', 'Node', 'Packet Dropped', 'packet_drop')

    def print_rendezvous(self, node):
        print ('>> Printing rendezvous time, node ' + str(node) + '...')
        plt.figure()
        plt.plot(self.nodes[node]['rv_time'])
        plt.axhline(10000, color='r')

        self.format_figure('Rendezvous', 'Node:'+str(node), 'Rendezvous time', 'rendezvous_time_'+str(node))

    def format_figure(self,title, xlab, ylab, filename):
        plt.title(title)
        plt.xlabel(xlab)
        plt.ylabel(ylab)
        plt.draw()
        plt.savefig(file_path+filename+'.eps', format='eps')

# --------------------- ENERGY GRAPHS ---------------------------------------- #
    def print_energy_levels(self):
        print ('>> Printing energy levels...')
        plt.figure()
        avg = []
        for i in range (0, len(self.nodes[1]['remaining_energy'])):
            avg.append(0.0)

        for i in range (1, self.number_of_nodes):
            plt.plot((self.nodes[i]['remaining_energy']) )
        self.format_figure('Node Energy Overview','Time', 'Energy', 'node_energy_overview')
        return

    def print_energy_total(self):
        print ('>> Printing energy statistics...')
        # plt.figure()
        fig, ax = plt.subplots()
        index = np.arange(self.number_of_nodes-1)
        bar_width = 0.35
        opacity = 0.4
        error_config = {'ecolor': '0.3'}
        # Add all harvested energy
        node_acum_harv = []
        node_acum_cons = []
        for i in range (1, self.number_of_nodes):
            node_acum_harv.append( (sum(self.nodes[i]['accum_harvest']) ) / 1000 + 18) #Initial energy add as harvested
            node_acum_cons.append( (sum(self.nodes[i]['accum_consumption'])) / 1000)

        rects1 = plt.bar(index, node_acum_harv, bar_width,
                 alpha=opacity,
                 color='b',
                 error_kw=error_config,
                 label='Collection')
        # Add all spend energy
        rects2 = plt.bar(index + bar_width, node_acum_cons, bar_width,
                 alpha=opacity,
                 color='r',
                 error_kw=error_config,
                 label='Consumption')
        #plot

        plt.xlabel('Node')
        plt.ylabel('Energy (x / 1000)')
        plt.title('Energy Total Statistics')
        plt.xticks(index + bar_width, index+1)
        plt.legend()

        plt.tight_layout()
        plt.draw()
        plt.savefig(file_path+'node_energy_total.eps', format='eps')
        return

    def print_energy_bar(self):
        print ('>> Printing energy statistics...')
        plt.figure()
        energy_t = []
        for i in range (1, self.number_of_nodes):
            energy_t.append(self.nodes[i]['remaining_energy'])

        plt.boxplot(energy_t,0,'')
        self.format_figure('Node Energy Deviation','Time', 'Energy', 'node_energy_deviation')
        return

# --------------------- NODE STATE ---------------------------------------- #
    def print_node_state(self):
        print ('>> Printing node state...')
        plt.figure()
        avg_state = []
        avg_state_t = []
        for i in range (1, self.number_of_nodes):
            sum_t  = 0.0
            counter = 0.0
            for j in range (0, len(self.nodes[i]['node_energy_state'])):
                sum_t += float(self.nodes[i]['node_energy_state'][j])
                counter += 1.0
            avg_state.append(sum_t/counter)

            # plt.plot(self.nodes[i]['node_energy_state'])
        for i in range(0,self.number_of_nodes-1):
            result = map(float,self.nodes[i]['node_energy_state'])
            avg_state_t.append(result)
            # plt.bar(i+1, avg_state[i] ,align='center')
        plt.boxplot(avg_state_t,0,'')
        plt.ylim(-0.2, 3.2)
        avg = float(sum(avg_state)) / float(self.number_of_nodes - 1)
        plt.axhline(avg, color='r')
        plt.annotate(str(avg), xy=(self.number_of_nodes-2, 0 ))
        self.format_figure('Node State','Time', 'State', 'node_state_bar')
        return

    def print_harvesting_rate(self):
        print ('>> Printing harvesting rate...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['harvesting_rate'])

        self.format_figure('Harvesting Rate','Time', 'HR', 'node_harv')
        return

    def print_avg_edc(self):
        print ('>> Printing average EDC...')
        plt.figure()
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['avg_edc'], label='node: '+str(i))

        plt.legend(loc=4)
        plt.ylim(0,256)

        self.format_figure('Node Avg EDC','Time', 'Metric', 'avg_edc')
        return

    def print_boxplot_edc(self):
        print ('>> Printing bar EDC...')
        plt.figure()
        edc_t = []
        for i in range (1, self.number_of_nodes):
            edc_t.append(self.nodes[i]['avg_edc'])
        plt.boxplot(edc_t,0,'')
        self.format_figure('Node EDC','Time', 'Metric', 'bar_edc')
        return


    def print_wakeups(self):
        print ('>> Printing number of wake-ups...')
        plt.figure()
        avg_wup = []
        avg_count = []

        for i in range (1, self.number_of_nodes):
            avg_count.append(0.0)
            avg_wup.append(0.0)
            for j in range (0, len(self.nodes[i]['num_wakeups'])):
                avg_wup[i-1] += float(self.nodes[i]['num_wakeups'][j])
                avg_count[i-1] += 1.0
        for i in range (0, self.number_of_nodes-1):
            avg_wup[i] = float(avg_wup[i]) / avg_count[i]
            plt.bar(i+1, avg_wup[i] ,align='center')

        avg = float(sum(avg_wup)) / float(self.number_of_nodes -1)
        plt.axhline(avg, color='r')
        plt.annotate(str(avg), xy=(self.number_of_nodes-2, avg+0.5))
        self.format_figure('Node Number of Wake-ups','Time', 'Wake-ups', 'wakeups')
        return

    def print_on_time(self):
        print ('>> Printing ON time...')
        plt.figure()
        avg = []
        for i in range (0, len(self.nodes[1]['on_time'])):
            avg.append(float(self.nodes[1]['on_time'][i]) / float(self.number_of_nodes - 1))

        for j in range (2, self.number_of_nodes):
            for i in range (0, len(avg)):
                try:
                    avg[i] += float(self.nodes[j]['on_time'][i]) / float(self.number_of_nodes - 1)
                except:
                    continue
                    # print ('>> ERROR print on time: '+ str(i) + ' ' +str(j))
        for i in range (1, self.number_of_nodes):
            plt.plot(self.nodes[i]['on_time'])
        plt.plot(avg,'dr', linewidth=5)

        self.format_figure('Node ON Time','Time', 'ON Time', 'on_time')
        return

    def print_dc(self):
        print ('>> Printing DC...')
        plt.figure()
        avg_dc_array = []
        for i in range (1, self.number_of_nodes):
            node_dc, avg_dc = self.get_node_dc(i)
            avg_dc_array.append(avg_dc)
            plt.plot(node_dc)
        self.format_figure('Node DC', 'Time', 'DC (%)', 'duty_cycle')

        plt.figure()
        for i in range (0, len(avg_dc_array)):
            plt.bar(i+1,avg_dc_array[i],width=0.3 ,align='center')
        plt.axhline( float(sum(avg_dc_array)) / float(len(avg_dc_array)) , color='r')
        plt.annotate(str(float(sum(avg_dc_array)) / float(len(avg_dc_array)) ), xy=(self.number_of_nodes - 2,(float(sum(avg_dc_array)) / float(len(avg_dc_array)) + 0.5)))
        self.format_figure('Node avg DC', 'Node', 'avg DC (%)', 'avg_duty_cycle')

    def generate_graphs(self):
        print ('>> Generating graphics...')
        self.print_avg_edc()
        self.print_boxplot_edc()

        self.print_energy_levels()
        self.print_energy_bar()
        self.print_energy_total()

        self.print_harvesting_rate()
        self.print_on_time()
        self.print_wakeups()
        try:
            self.print_node_state() #Average node state
        except:
            print ('>> ERROR on print_node_state')
        try:
            self.printf_node_state() #graph with node state changes
        except:
            print ('>> ERROR on printf_node_state')

        self.print_packet_created()
        # try:
        self.print_dead_node()
        # except:
            # print ('>> ERROR on print_dead_node')
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
    # try:
    adapter = LogConverter(sys.argv[1], int(sys.argv[2]))
    # except:
        # print('Error at LogConverter')
