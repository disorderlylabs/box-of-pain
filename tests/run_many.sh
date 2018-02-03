#!/bin/sh
#sed -ri 's/[0-9]+.[0-9]+.[0-9]+.[0-9]+:[0-9]+/localhost/g' out1.dot

mkdir -p out
for i in $(seq 1 $1); do
	echo $i
	../painbox -e ../quorum_server -e ../client -e ../client -e ../client -d  >/dev/null 2>/dev/null 
	m4 out.m4 > out/out$i.dot
	rm out.m4
	sed -ri 's/[0-9]+.[0-9]+.[0-9]+.[0-9]+:[0-9]+/localhost/g' out/out$i.dot
done


