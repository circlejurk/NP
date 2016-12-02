#!/bin/sh

./delayclient localhost 9527 in1 > out1
./delayclient localhost 9527 in2 > out2
./delayclient localhost 9527 in3 > out3
./delayclient localhost 9527 in4 > out4
./delayclient localhost 9527 in5 > out5
./delayclient localhost 9527 in6 > out6
./delayclient localhost 9527 in7 > out7

cd /u/cs/103/0310004/rwg
rm -f baozi *.txt
cd -
