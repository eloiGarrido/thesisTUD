#!/bin/bash
cd
cd thesisTUDelft/eh_staffetta

for i in 1 2 3 4 5
do
  python staffetta_get_data.py Desktop/orig/40min/sim2_16_uni-$i.log 16
  filename=out_orig_sim2_16_uni-$i.txt
  orig_f = /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/sim2_16_uni-$i/results_out.txt
  python read_output orig_f filename
done

for i in 1 2 3 4 5
do
  python staffetta_get_data.py Desktop/orig/50min/sim2_16_uni-$i.log 16
  filename=out_orig_sim2_16_uni-$i.txt
  orig_f = /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/sim2_16_uni-$i/results_out.txt
  python read_output orig_f filename
done
# ---------------------------------------------------------------------------
for i in 1 2 3 4 5
do
  python staffetta_get_data.py Desktop/eh/40min/sim2_16_uni-$i.log 16
  filename=out_orig_sim2_16_uni-$i.txt
  orig_f = /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/sim2_16_uni-$i/results_out.txt
  python read_output orig_f filename
done

for i in 1 2 3 4 5
do
  python staffetta_get_data.py Desktop/eh/50min/sim2_16_uni-$i.log 16
  filename=out_eh_sim2_16_uni-$i.txt
  orig_f = /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/sim2_16_uni-$i/results_out.txt
  python read_output orig_f filename
done
