#!/bin/bash

#scp -i /Users/palvaro/code/camflow-vag/rpm-test/.vagrant/machines/default/virtualbox/private_key -P 2222 vagrant@127.0.0.1:/tmp/audit.log .
python2 log2json.py audit.log
python2 newparse.py > creates.nwo
python2 load.py
