#!/bin/sh
# First Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_eh_NoCA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/simple
cd
cp /home/egarrido/simple.log /home/egarrido/staffetta_builds/V3_eh_NoCA/
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py simple.log 6 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_eh_NoCA/
# Second Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_fix_NoCA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/simple
cd
cp /home/egarrido/simple.log /home/egarrido/staffetta_builds/V3_fix_NoCA/
cd
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py simple.log 6 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_fix_NoCA/
# Third Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_eh_CA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/simple
cd
cp /home/egarrido/simple.log /home/egarrido/staffetta_builds/V3_eh_CA/
cd
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py simple.log 6 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_eh_CA/
# Forth Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_fix_CA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/simple
cd
cp /home/egarrido/simple.log /home/egarrido/staffetta_builds/V3_fix_CA/
cd
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py simple.log 6 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_fix_CA/
#################################################################################################################
# Second Topology
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_eh_NoCA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/sim2_16_uni
cd
cp /home/egarrido/sim2_16_uni.log /home/egarrido/staffetta_builds/V3_eh_NoCA/REAL/
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py sim2_16_uni.log 16 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_eh_NoCA/REAL/
# Second Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_fix_NoCA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/sim2_16_uni
cd
cp /home/egarrido/sim2_16_uni.log /home/egarrido/staffetta_builds/V3_fix_NoCA/REAL/
cd
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py sim2_16_uni.log 16 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_fix_NoCA/REAL/
# Third Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_eh_CA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/sim2_16_uni
cd
cp /home/egarrido/sim2_16_uni.log /home/egarrido/staffetta_builds/V3_eh_CA/REAL/
cd
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py sim2_16_uni.log 16 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_eh_CA/REAL/
# Forth Simulation
cd
rm /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/staffetta-test.sky
cp /home/egarrido/staffetta_builds/V3_fix_CA/staffetta-test.sky /home/egarrido/staffetta_sensys2015/eh_staffetta/apps/glossy-test/
cd contiki/tools/cooja/contiki_tests/
bash RUN_TEST /home/egarrido/sim2_16_uni
cd
cp /home/egarrido/sim2_16_uni.log /home/egarrido/staffetta_builds/V3_fix_CA/REAL/
cd
cd /home/egarrido/staffetta_sensys2015/eh_staffetta
python staffetta_get_data.py sim2_16_uni.log 16 20
mv /home/egarrido/staffetta_sensys2015/eh_staffetta/results/original/V3 /home/egarrido/staffetta_builds/V3_fix_CA/REAL/