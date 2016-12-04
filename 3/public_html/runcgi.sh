#!/bin/sh

#REQUEST_METHOD="GET" CONTENT_LENGTH="0" QUERY_STRING="h1=localhost&p1=9527&f1=t1.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&h5=&p5=&f5=" gdb -q ./hw3.cgi
REQUEST_METHOD="GET" CONTENT_LENGTH="0" QUERY_STRING="h1=localhost&p1=9527&f1=t1.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&h5=&p5=&f5=" ./hw3.cgi
