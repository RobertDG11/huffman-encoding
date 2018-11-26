#!/bin/bash
make build
make build-pthreads
good=0
for i in 1 2 3 4 5
do
	echo -e "\n\n\nrunda $i"
	for j in 1 2 3 4
   	do
		echo "test $j"
		./hufp encode input$j out$j 4
		./huf decode out$j fin$j
		DIFF=$(diff fin$j input$j)
		if [ "$DIFF" != "" ]
		then
			echo "gresit la rularea numarul $i la testul $j"
			good= 1
		fi
	done
done

if [ $good == 1 ]
then
	echo -e "\n\n\nUn test picat"
fi
if [ $good == 0 ]
then
	echo -e "\n\n\nOutput corect"
fi
