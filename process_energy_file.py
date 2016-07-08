import sys

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

def write_out_file(Energy):
    file = open('energy_values.txt', 'w')
    for line in Energy:
        file.write(str(line)+'\n')
    file.close()

if __name__ == '__main__':
    E = process_file(sys.argv[1])
    E_mJ = adapt_to_mJoules(E)
    write_out_file(E_mJ)