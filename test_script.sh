#!/bin/sh
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 csc_file repeat_times"
    echo "example: $0 ~/contiki/your_file.csc 10"
    exit
fi
cd /home/user/contiki/tools/cooja/dist

for i in `seq 1 $2`
do
echo ">>>>> Running $i"
java -mx512m -jar cooja.jar -nogui=$1
mv COOJA.testlog "$1.$i.log"
done