import sys
import json

try:
    logfile = sys.argv[1]
except IndexError as e:
    print('''
usage: python log2json.py /path/to/auditfile.log
output: creates a json file /path/to/auditfile.log.json
Forgot to pass the auditfile argument?
''')
    sys.exit()

onetruedict = dict()

with open(logfile) as f:
    for line in f:
        d = json.loads(line.strip())
        for k, v in d.items():
            if k in onetruedict:
                onetruedict[k].update(v)
            else:
                onetruedict[k] = v

json.dump(onetruedict, open(logfile+'.json', 'w'))
print("wrote json file to", logfile+'.json')
