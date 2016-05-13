__author__ = 'egarrido'
import os

def format_file():
	file = "positions.dat"
	output_file = "positions_o.dat"
	next = True
	try:
		f = open(file, 'r')
		out_f = open(output_file, 'w')

		for line in f:
			if (line[0] != '0'):
				if (next == True):
					out_f.write(line)
				else:
					next = True
			else:
				next = False
		f.close()
		out_f.close()

	except ValueError:
		print ('Error when opening files')

def main():
	format_file()

if __name__=="__main__":
	main()