import sys
import math
def process_file(filename):
    energy_array = []
    with open(filename) as f:
        lines = f.readlines()
        for line in lines:
            line_split = line.split('\t')
            energy = float(line_split[1]) * 100.0
            energy_array.append(energy)
    return energy_array

def adapt_to_mJoules(Energy):
    E_mJ = []
    for E in Energy:
        mW = E / 1000.0
        # mW_s = mW / 30.0
        E_mJ.append(mW)
    return E_mJ

def write_out_file(Energy, filename):
    file = open(filename, 'w')
    for line in Energy:
        file.write(str(line)+'\n')
    file.close()


def energy_float_to_int(filename):
    energy = []
    with open(filename) as f:
        lines = f.readlines()
        for line in lines:
            line_split = line.split('\n')
            energy.append(round(float(line_split[0])))
        filename_out = 'out_' + filename
        write_out_file(energy,filename_out)


if __name__ == '__main__':
    # E = process_file(sys.argv[1])
    # E_mJ = adapt_to_mJoules(E)
    # write_out_file(E_mJ,'energy_values.txt' )
    energy_float_to_int(sys.argv[1])