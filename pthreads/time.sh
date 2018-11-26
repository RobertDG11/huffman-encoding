#!/bin/bash
make build
make build-pthreads
for i in 1 2 3 4 5 6
do
	echo -e "\n\n\ntest $i"
	time ./huf encode input$i out$i
	for j in 1 2 4 8 16 24 32
   	do
		echo "numar threaduri $j"
		time ./hufp encode input$i out$i $j
	done
done

