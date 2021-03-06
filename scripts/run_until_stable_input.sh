#!/bin/bash

if [[ "$DIR" == "" ]]; then
	echo 'Please provide a directory for runfiles' >&2
	exit 1
fi

mkdir -p $DIR

trap "echo Exited!; exit;" SIGINT SIGTERM

clear

let iter=0
while true; do
	runs=""
	let iter++
	let max=0
	let count=0
	for filename in $(ls $DIR | grep '\.boprun$'); do
		runs="$runs -r $DIR/$filename"
		num=${filename/\.boprun/}
		re='^[0-9]+$'
		let count++
		if [[ $num =~ $re ]] ; then
			if (( $max <= $num )); then
				let "max=$num+1"
			fi
		fi
	done

	#read foo
	killall redis-server
	sleep 0.01
	killall redis-server
	#clear
	#echo -ne "\e[H"
	echo "=== STARTING RUN $max, (found $count runs) (iter # $iter) ==="
	echo "./painbox -d $runs -s $DIR/$max.boprun " "$@"
	rm foo.pipe foo2.pipe foo3.pipe foo4.pipe
	mkfifo foo.pipe
	mkfifo foo2.pipe
	mkfifo foo3.pipe
	mkfifo foo4.pipe

	#CMD_S="connect\nSET foo \"bar\"\nGET foo\nSET baz \"abcd\"\nGET baz"
	CMD_S="connect\nSET foo \"bar\"" #\nGET foo\nSET baz \"abcd\"\nGET baz"

	(sleep 0.7; echo ""; sleep 1; echo -e $CMD_S) > foo.pipe &
	(sleep 0.7; echo ""; sleep 1; echo -e $CMD_S) > foo2.pipe &
	#(sleep 0.7; echo ""; sleep 1; echo -e $CMD_S) > foo3.pipe &
	#(sleep 0.7; echo ""; sleep 1; echo -e $CMD_S) > foo4.pipe &
	./painbox -d $runs -s $DIR/$max.boprun "$@"
	res=$?
	wait
	if [[ "$res" == "255" ]]; then
		echo ERROR RETURNED: $res >&2
		exit 1
	fi

	if [[ "$res" == "0" ]]; then
		if [[ "$MODE" != "continue" ]]; then
			echo "Stable run found"
			exit 0
		fi
	else
		m4 $DIR/$max.boprun.m4 > $DIR/$max.dot
		dot -Tpdf -o $DIR/$max.pdf $DIR/$max.dot
		#clear
	fi

	# we fell off!
done

