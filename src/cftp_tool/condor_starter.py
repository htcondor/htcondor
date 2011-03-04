'''Starts a job on condor'''

import os
import os.path as osp
import subprocess
import tempfile


def start_job( submit_data, submit_args, queue=1 ):
    
    temp_submit = tempfile.NamedTemporaryFile()
    
    print "Creating temporary submit file '%s'.\n" % temp_submit.name 

    for key in submit_data.iterkeys():
        temp_submit.write( "%s = %s\n" % (key, submit_data[key]) )
        print "%s = %s" % (key, submit_data[key])

    temp_submit.write( "Queue %d\n" % queue )
    print "Queue %d" % queue

    temp_submit.flush()

    print "\nWriting to submit file complete."

    submit_cmd = list()
    submit_cmd.extend( submit_args )
    submit_cmd.append( temp_submit.name )

    print "Calling condor_submit: 'condor_submit %s'." % ' '.join(submit_cmd)

    ret_code = subprocess.call( submit_cmd ,
                                executable='condor_submit' )
    
    temp_submit.close()

    return ret_code


if __name__ == "__main__":
    
    submit_desc = { 'Executable':'/scratch/CONDOR_SRC/src/cftp_tool/client',
                    'Output':'client_test.out',
                    'Universe':'vanilla',
                    #'transfer_input_files':'/tmp/datefile',
                    'Arguments':'localhost 9980 datefile' }
    
    submit_args = [ '-verbose', ]
    
    start_job( submit_desc, submit_args, queue=1 )
