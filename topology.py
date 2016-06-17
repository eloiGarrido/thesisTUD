#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: Xin Wang
 
 
import networkx as nx
from matplotlib import pyplot as plt
import math
CANVAS_SIZE = 12
 
 
class Network(object):
 
    def __init__(self, nodes):
        self.nodes = nodes
        self.G = nx.Graph()
 
    def build_graph(self, labels=None, node_colors=None):
        if node_colors is None:
            node_colors = '#FFFCBF'
        print (self.nodes.keys())
        nx.draw_networkx_nodes(self.G,
                               self.nodes,
                               nodelist=self.nodes.keys(),
                               node_color=node_colors,
                               node_size=CANVAS_SIZE * 50,
                               alpha=.8)
 
        network_labels = {}
 
        # node ID
        for label in self.nodes.keys():
            if labels is None:
                network_labels[label] = label
            else:
                network_labels[label] = labels[label]
 
        nx.draw_networkx_labels(self.G,
                                self.nodes,
                                network_labels,
                                font_size=CANVAS_SIZE)
 
    def add_text(self, t, x=0.2, y=0.8):
        figure = plt.gcf()
        figure.text(x, y, t, color='blue', fontsize=25)
 
    def set_topology(self, topo):
        edges = []
        for node, neighbors in topo.items():
            for neighbor in neighbors:
                # neighbor -> node
                edges.append((neighbor, node))
 
        nx.draw_networkx_edges(self.G,
                               self.nodes,
                               edgelist=edges,
                               edge_color='b')
 
    def misc_setup(self):
 
        axis = plt.gca()
        x = axis.get_xlim()
 
        # x e.g. (-20, 120), which is (0 - MARGIN, 100 + MARGIN)
        # get margin -> e.g. 20
        margin = 0 - x[0]
        # shrink margin
        margin = int(margin / 2)
 
        # invert y, because Cooja uses different direction
        if axis.get_ylim()[0] < axis.get_ylim()[1]:
            y = axis.get_ylim()[::-1]
        else:
            y = axis.get_ylim()
 
        x = (x[0] - margin, x[1] + margin)
        y = (y[0] + margin, y[1] - margin)
 
        axis.set_ylim(y)
        axis.set_xlim(x)
 
        plt.ylabel('y')
        plt.xlabel('x')
 
 
    def set_title(self, t):
        plt.title(t, fontdict={'fontsize': CANVAS_SIZE})
 
    def show(self):
        self.misc_setup()
        plt.show()
        plt.clf()
 
    def save(self, filename):
        self.misc_setup()
        figure = plt.gcf()
        figure.set_size_inches(CANVAS_SIZE, CANVAS_SIZE)
        plt.savefig(filename, dpi=96)
        plt.clf()


def compute_distance(x1,y1,x2,y2):
    return math.hypot(x2 - x1, y2 - y1)

def compute_edges(positions):
    max_dist = 50
    topology = {}
    for node1, value in positions.iteritems():
        x1 = value[0]
        y1 = value[1]
        connections = []
        for node2, value2 in positions.iteritems():
            if node2 != node1:
                if compute_distance(x1,y1,value2[0],value2[1]) <= max_dist:
                    connections.append(node2)
        topology[node1] = connections
    return topology
def gen_topology(positions):
    net = Network(positions)
    topology = compute_edges(positions)
    net.set_topology(topology)
    net.build_graph()
    net.save("test.png")
    net.show()
