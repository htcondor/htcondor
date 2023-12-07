#!/usr/bin/python3

import htcondor2 as htcondor
import random
import string
from operator import itemgetter
import sys
import os
from traceback import print_exc
import time

n_vars = 4
n_queues = 1
n_submits = 100

# start jobs on hold (they don't need to run to test)
base_submit_description = '''
executable = /bin/sleep
arguments = 1
transfer_executable = false
output = $(cluster).$(process).out
error = $(cluster).$(process).err
log = $(cluster).log
request_memory = 128MB
request_disk = 100MB
hold = true
'''

def gen_rand_str(length = 8):
    return ''.join([str(random.choice(string.ascii_lowercase)) for i in range(length)])

# set all test variables to some string
undef = 'Undefined'
for i in range(n_vars):
    base_submit_description += 'var{0} = "{1}"\nMy.Var{0} = $(var{0})\n'.format(i, undef)
base_submit_description += 'My.Item = {0}'.format(undef)

def gen_single_var_data(n, dat = False):
    itemdata = {}
    for i in range(n):
        key = 'Var{0}'.format(i)
        if i == 0:
            rand_str = gen_rand_str()
            if dat:
                datfile = rand_str + '.dat'
                itemdata[key] = datfile
                open(datfile, 'a').close()
            else:
                itemdata[key] = rand_str
        else:
            itemdata[key] = undef
    return itemdata

normal_inputs = [dict([('Var{0}'.format(j), undef) for j in range(n_vars)]) for i in range(n_queues)]

itemdata_inputs = [gen_single_var_data(n_vars) for i in range(n_queues)]

globdata_inputs = [gen_single_var_data(n_vars, dat = True) for i in range(n_queues)]
globdata_inputs.sort(key = itemgetter('Var0')) # sort list by filename

filedata_inputs = [dict([('Var{0}'.format(j), gen_rand_str()) for j in range(n_vars)]) for i in range(n_queues)]
filedata_name = 'inputs.txt'
with open(filedata_name, 'w') as f:
    for fd in filedata_inputs:
        values = []
        for i in range(n_vars):
            values.append(fd['Var{0}'.format(i)])
        f.write(','.join(values) + '\n')

multilinedata_inputs = [dict([('Var{0}'.format(j), gen_rand_str()) for j in range(n_vars)]) for i in range(n_queues)]

def queue_normal(inputs):
    queue_statement = 'queue {0}'.format(len(inputs))
    return queue_statement

def queue_itemdata(inputs):
    values = []
    for iput in inputs:
        values.append(iput['Var0'])
    queue_statement = 'queue var0 in ({0})'.format(','.join(values))
    return queue_statement

def queue_globdata(inputs):
    values = []
    for iput in inputs:
        values.append(iput['Var0'])
    queue_statement = 'queue var0 matching files (*.dat)'
    return queue_statement

def queue_filedata(inputs):
    variables = list(inputs[0].keys())
    variables.sort()
    queue_statement = 'queue {0} from {1}'.format(','.join(variables).lower(), filedata_name)
    return queue_statement

def queue_multilinedata(inputs):
    variables = list(inputs[0].keys())
    variables.sort()
    queue_statement = 'queue {0} from '.format(','.join(variables).lower()) + ' (\n'
    for iput in inputs:
        queue_statement = queue_statement + '{0}\n'.format(','.join([iput[v] for v in variables]))
    queue_statement = queue_statement + ')\n'
    return queue_statement

queues = {
    queue_normal(normal_inputs): normal_inputs,
    queue_itemdata(itemdata_inputs): itemdata_inputs,
    queue_globdata(globdata_inputs): globdata_inputs,
    queue_filedata(filedata_inputs): filedata_inputs,
    queue_multilinedata(multilinedata_inputs): multilinedata_inputs
}

# queue jobs without overwriting queue statements
clusters = {}
schedd = htcondor.Schedd()

# test a bunch
for q in queues.keys():
    print(q)
    for i in range(n_submits):
        submit_description = base_submit_description + '\n' + q
        sub = htcondor.Submit(submit_description)
        result = schedd.submit(sub)
        clusters[(q, i)] = result.cluster()
        sys.stdout.write('{0} '.format(i))
        sys.stdout.flush()
        if (i % 20 == 0) and (i > 0):
            sys.stdout.write('\n')
    sys.stdout.write('\n')

# remove held jobs
for cid in clusters.values():
    removed = schedd.act(htcondor.JobAction.Remove, 'ClusterId == {0}'.format(cid))

# muck with the inputdata
del clusters
results = {}
itemdata = [dict([('Var{0}'.format(j), gen_rand_str()) for j in range(n_vars)]) for i in range(n_queues)]
randint = random.randint(1,100)
randstr = gen_rand_str(5)

queue_signatures = {
    'double_nil': (),
    'nil': (1,),
    'none': (1, None),
    'empty_listdict': (1, [{}]),
    'iter_empty_listdict': (1, iter([{}])),
    'int_list': (1, [randint]),
    'str_list': (1, [randstr]),
    'bare_list': (1, []),
    'bare_dict': (1, {}),
    'bare_int': (1, randint),
    'bare_str': (1, randstr),
    'itemdata': (1, itemdata[:]),
}
check_new_output = ['iter_itemdata']
check_old_output = ['iter_empty_listdict']
check_undef_output = ['double_nil', 'nil', 'none', 'bare_dict', 'bare_list', 'empty_listdict']
report_output = ['int_list', 'str_list', 'bare_int', 'bare_str', 'iter_blank_itemdata', 'iter_itemdata_matching', 'itemdata']
signature_exceptions = {
    'bare_int': (TypeError,),
    'bare_str': (TypeError,),
}

#for q in queues.keys():
for q in [queue_normal(normal_inputs)]: # just run against queue n for now
    submit_description = base_submit_description + '\n' + q
    sub = htcondor.Submit(submit_description)
    queue_signatures['iter_blank_itemdata'] = (1, iter(sub.itemdata()))
    queue_signatures['iter_itemdata_matching'] = (1, iter(sub.itemdata('matching *.dat')))
    for sig_name, sig in queue_signatures.items():
        print('{0} w/ {1}'.format(q, sig_name))
        results = []
        for i in range(n_submits):
            with schedd.transaction() as txn:
                try:
                    results.append(sub.queue_with_itemdata(txn, *sig))
                except Exception as e:
                    if (sig_name in signature_exceptions) and (e.__class__ in signature_exceptions[sig_name]):
                        pass
                    else:
                        print_exc()
                else:
                    if (sig_name in signature_exceptions):
                        pass
            sys.stdout.write('{0} '.format(i))
            sys.stdout.flush()
            if (i % 20 == 0) and (i > 0):
                sys.stdout.write('\n')
        sys.stdout.write('\n')
        for result in results:
            schedd.act(htcondor.JobAction.Remove, 'ClusterId == {0}'.format(result.cluster()))

# remove dat files
for gd in globdata_inputs:
    os.remove(gd['Var0'])
