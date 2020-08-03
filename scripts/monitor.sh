stdbuf -o0 inotifywait -q -m -e close_write $@ | stdbuf -o0 awk '{print $1}' |
	while read events; do
		echo Generating $events.dot
		m4 $events > $events.dot
		echo Generating $events.pdf
		dot -Tpdf -o $events.pdf < $events.dot
		echo done

	done

