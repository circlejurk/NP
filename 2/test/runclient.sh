#!/bin/sh

./delayclient localhost 9527 in1 > out1
./delayclient localhost 9527 in2 > out2
./delayclient localhost 9527 in3 > out3
./delayclient localhost 9527 in4 > out4

cd /u/cs/103/0310004/rwg
rm -f baozi *.txt
cd -
