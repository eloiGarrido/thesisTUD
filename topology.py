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
    nodes = {1: (141.09817992495704, 96.10330485131327), 2: (65.42609386823965, 18.63035061491741), 3: (113.47201053589093, 145.86525544520526), 4: (141.28463338583504, 56.797701198077704), 5: (60.74118075120142, 82.9714946235649), 6: (89.98164789477084, 63.49105363326121), 7: (0.20898233959432103, 131.27645902649846), 8: (23.664001176850103, 48.16182993934503), 9: (45.38923568084683, 11.856591450338605), 10: (124.31176813116704, 138.40920596682366), 11: (92.40623902391313, 53.79714926544572), 12: (61.230377975294516, 28.89451526538793), 13: (1.626804749364158, 22.048923215746324), 14: (120.6979287239955, 29.429198686476344), 15: (9.243461468936198, 47.06948207241121), 16: (
        40.1435318649867, 140.0708750401764), 17: (97.96193221326895, 4.298056524652704), 18: (63.382005300477466, 104.87192213367919), 19: (60.33280351723047, 58.95714510969468), 20: (25.541406767392612, 141.06674926812335), 21: (49.100249495395666, 125.38568868686048), 22: (80.6865050189868, 122.18295363899648), 23: (34.08390932516025, 126.3062938566484), 24: (38.9306129210169, 64.69409453476935), 25: (58.84566033527845, 42.852580432101576), 26: (83.20600692550532, 147.31409580082013), 27: (127.65703503088358, 4.9051709249950175), 28: (141.951123985819, 109.36800925180769), 29: (7.938461918971, 101.26789124774194), 30: (147.01529774610665, 42.215113367038086)}

    # topo = {1: [10], 2: [11, 12, 19], 3: [10, 26], 4: [], 5: [24], 6: [5], 7: [20, 29, 23, 16], 8: [24, 12], 9: [8, 2, 13], 10: [28, 26, 22, 3], 11: [5], 12: [25], 13: [8, 9], 14: [4], 15: [24, 8], 16: [
    #     7, 21, 23], 17: [27], 18: [19, 23, 5, 16], 19: [5], 20: [7, 29, 21, 23], 21: [23], 22: [21, 23], 23: [22, 21, 16], 24: [8], 25: [24, 12, 5], 26: [22, 3, 16], 27: [30], 28: [1, 10], 29: [23], 30: []}
    #
    topo2 = {1: [], 2: [11, 12], 3: [26], 4: [], 5: [24], 6: [], 7: [29, 23, 16], 8: [24, 12], 9: [2, 13], 10: [28, 26, 22], 11: [], 12: [], 13: [8], 14: [], 15: [24], 16: [
        21, 23], 17: [], 18: [23, 5, 16], 19: [], 20: [29, 21, 23], 21: [], 22: [21], 23: [22, 21], 24: [], 25: [24, 12], 26: [22, 16], 27: [30], 28: [10], 29: [], 30: []}

    net = Network(positions)
    # net = Network(nodes)
    topology = compute_edges(positions)
    net.set_topology(topology)
    # net.set_topology(topo2)
    net.build_graph()
    net.save("test.png")
    net.show()
 
# if __name__ == '__main__':
#     main(sys.argv[1])