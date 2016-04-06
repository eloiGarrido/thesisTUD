#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: Eloi Garrido
# @Date:   2016-02-12
__author__ = 'egarrido'
import sys
import matplotlib.pyplot as plt
import os
import shutil
from operator import add
'''
Log Converter
convert Cooja results into statistical data and graphs
'''
env = 'uni'
# env = 'home'
# simulation = 'orig'
simulation = 'eh'

simulation_name = str(simulation) + "_" + str(env) + "_energy_10k_21_10min"
file_path = ""
if env == 'uni':
    general_path = "/home/egarrido/contiki/tools/cooja/build/"
    if simulation == 'orig':
        file_path = "/home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/" + simulation_name
    elif simulation == 'eh':
        file_path = "/home/egarrido/staffetta_sensys2015/eh_staffetta/results/eh_staffetta/" + simulation_name
elif env == 'home':
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
        # self.output = []
        self.output = []
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

        # self.print_delay(pkt_delay)

        # self.print_dc()
        # self.print_drop_ratio(pkt_delay)

        # for i in range (1, number_of_nodes):
        #     self.print_rendezvous(i)
        # plt.show()

        self.generate_graphs()
        try:
            shutil.copy( general_path + "COOJA.testlog", file_path )
        except:
            print ('>> Error when moving COOJA.testlog')

    def create_structure(self):
        '''
        Create data structure,  array of dictionaries containing all node information
        '''
        for i in range (0, self.number_of_nodes):
            self.nodes.append({'id':i, 'node_state': [],'rv_time':[], 'time2':[], 'time3':[], 'time4':[], 'time6':[], 'time_on': [], 'time_off': [], 'abs_time_off': [], 'pkt':[], 'num_wakeups':[], 'on_time': [], 'avg_edc':[], 'seq':[], 'node_energy_state':[], 'remaining_energy':[], 'harvesting_rate':[]})

#--------------------------- Output Functions ---------------------------#
#TODO Create a function to output each type of file data
    def output_energy_values(self, filename):
        txt_name = str(filename)

        with open(txt_name, 'w') as fp:
            fp.write('\n'.join(self.output))



    def output_file(self, element, filename, num):
        # txt_name = file_path + str(filename) + str(i) + ".txt"
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

        # try:
        print ('>> Reading file: ' + file_name + '...')
        f = open(file_name,'r')
        for line in f:
            self.parse(line)
        f.close()

        # except Exception as e:
        #     print ('>> Error, No file with that name: ',e)

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
            self.nodes[id-1]['node_state'].append(msg[3])

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

        for i in range (0, len(self.nodes[node_id]['time_on'])-1):

            # period = self.nodes[node_id]['time_off'][i+1] - self.nodes[node_id]['time_off'][i]
            period = self.nodes[node_id]['abs_time_off'][i+1] - self.nodes[node_id]['abs_time_off'][i]
            on = abs( self.nodes[node_id]['time_on'][i] - self.nodes[node_id]['time_off'][i] )
            node_dc.append( float(on) / float(period) )
            total_on = sum(self.nodes[node_id]['time_on'])
            # total_on += abs( self.nodes[node_id]['time_on'][i] - self.nodes[node_id]['time_off'][i+1] )
            counter += 1.0
        try:
            avg_dc_t = float(600000000) / float(total_on)
            avg_dc =   float(total_on) / float(self.nodes[node_id]['time_off'][len(self.nodes[node_id]['time_off'])-1])
            # print ('avg_dc_t: '+str(avg_dc_t) + ' avg_dc:'+str(avg_dc))
        except:
            avg_dc = 0
            avg_dc_t = 0
            print ('>> ERROR: No DC data')
        # avg_dc = avg_dc / counter
        return node_dc, avg_dc_t


#--------------------------- Printing Functions ---------------------------#
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
            plt.plot(self.nodes[i]['node_state'])
        self.format_figure('Node State', 'Node', 'State', 'node_state')

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
            # print ('Node: ' + str(i) + ' pkts: ' + str(total_created[i]))
        for i in range(0, len(pkt_delay)):
            node = int(pkt_delay[i]['src'])
            total_pkt[node-1] += 1.0
            if pkt_delay[i]['delay'] == 'lost':
                # continue
            # else:
                drop_pkt[node-1] += 1.0



        for i in range(0, len(total_pkt)):
            try:
                plt.bar(i+1, ( drop_pkt[i] / total_pkt[i]),align='center' )
            except:
                plt.bar(i+1, 0 ,align='center')
            # print ('Node: ' + str(i) + ' pkts: ' + str(total_pkt[i]))
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
        plt.savefig(file_path+filename)


    def print_energy_levels(self):
        print ('>> Printing energy levels...')
        plt.figure()
        avg = []
        for i in range (0, len(self.nodes[1]['remaining_energy'])):
            avg.append(0.0)

        for i in range (1, self.number_of_nodes):
            # for j in range (0, len(self.nodes[i]['remaining_energy'])):
            #     try:
            #         avg[j] += float(self.nodes[i]['remaining_energy'][j]) / float(self.number_of_nodes - 1)
            #     except:
            #         avg.append(float(self.nodes[i]['remaining_energy'][j]) / float(self.number_of_nodes - 1))
        # plt.plot(avg)
            plt.plot((self.nodes[i]['remaining_energy']) )
        self.format_figure('Node Energy Levels','Time', 'Energy', 'node_energy')
        return

    def print_node_state(self):
        print ('>> Printing node state...')
        plt.figure()
        avg_state = []

        for i in range (1, self.number_of_nodes):
            sum_t  = 0.0
            counter = 0.0
            for j in range (0, len(self.nodes[i]['node_energy_state'])):
                sum_t += float(self.nodes[i]['node_energy_state'][j])
                counter += 1.0
            avg_state.append(sum_t/counter)

            # plt.plot(self.nodes[i]['node_energy_state'])
        for i in range(0,self.number_of_nodes-1):
            plt.bar(i+1, avg_state[i] ,align='center')

        avg = float(sum(avg_state)) / float(self.number_of_nodes - 1)
        plt.axhline(avg, color='r')
        plt.annotate(str(avg), xy=(self.number_of_nodes-2, 0 ))
        self.format_figure('Node State','Time', 'State', 'node_state')
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
                    print ('>> ERROR print on time: '+ str(i) + ' ' +str(j))
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
        self.print_energy_levels()
        self.print_harvesting_rate()
        # self.print_node_state()
        self.print_on_time()
        self.print_wakeups()
        # self.printf_node_state()
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
