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

	sleep 0.01
	#clear
	#echo -ne "\e[H"
	echo "=== STARTING RUN $max, (found $count runs) (iter # $iter) ==="
	echo "./painbox -d $runs -s $DIR/$max.boprun " "$@"
	./painbox -d $runs -s $DIR/$max.boprun "$@"
	res=$?
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

