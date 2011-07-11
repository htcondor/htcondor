'''Starts a job on condor'''

import os
import os.path as osp
import subprocess
import tempfile

class splitWritter:
    def __init__( self, fdList ):
        self.fds = fdList
    
    def write( self, text ):
        for fd in self.fds:
            if hasattr( fd, "write" ):
                fd.write( text )

    def flush(self ):
        for fd in self.fds:
            if hasattr(fd, "flush" ):
                        fd.flush()
            


def start_job( submit_data, submit_args, queue=1, save=False, debug=False ):
    
    temp_submit = tempfile.NamedTemporaryFile()
    if save:
        backup_submit = open( "./%s.BACKUP" % osp.split(temp_submit.name)[1], 'w'  )
    else:
        backup_submit = None

    writer = splitWritter( (temp_submit, backup_submit) )

    if debug:
        print "Creating temporary submit file '%s'.\n" % temp_submit.name 

    for key in submit_data.iterkeys():
        writer.write( "%s = %s\n" % (key, submit_data[key]) )
        if debug:
            print "%s = %s" % (key, submit_data[key])

    writer.write( "Queue %d\n" % queue )
    if debug:
        print "Queue %d" % queue

    writer.flush()

    if debug:
        print "\nWriting to submit file complete."

    submit_cmd = list()
    submit_cmd.extend( submit_args )
    submit_cmd.append( temp_submit.name )

    if debug:
        print "Calling condor_submit: 'condor_submit %s'." % ' '.join(submit_cmd)

    ret_code = subprocess.call( submit_cmd ,
                                executable='condor_submit' )
    
    temp_submit.close()
    if backup_submit:
        backup_submit.close()

    return ret_code


if __name__ == "__main__":
    
    submit_desc = { 'Executable':'/scratch/CONDOR_SRC/src/cftp_tool/client',
                    'Output':'client_test.out',
                    'Universe':'vanilla',
                    #'transfer_input_files':'/tmp/datefile',
                    'Arguments':'localhost 9980 datefile' }
    
    submit_args = [ '-verbose', ]
    
    start_job( submit_desc, submit_args, queue=1 )
