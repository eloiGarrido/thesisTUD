__author__ = 'egarrido'

def read_output(orig_file, output_file):
    with open(output_file, "a") as myfile:
        myfile.write(orig_file)
        with open(orig_file, "r") as input_file:
            for line in input_file:
                myfile.write(line)
    input_file.close()
    myfile.close()

if __name__ == '__main__':
    '''
    Main function call, pass origin file name + pass output file name.
    '''
    if len(sys.argv) < 2:
        print len(sys.argv)
        exit(1)
    # try:
    adapter = read_output(sys.argv[1], sys.argv[2])
