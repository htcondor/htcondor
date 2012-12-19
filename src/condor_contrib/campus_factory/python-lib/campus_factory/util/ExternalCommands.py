# File for running external commands
#
#
import logging
import os
import subprocess
from select import select


def RunExternal(command, str_stdin=""):
    """Run an external command 
    
    @param command: String of the command to execute
    @param stdin: String to put put in stdin
    @return: (str(stdout), str(stderr)) of command
    Returns the stdout and stderr
    
    """

    logging.info("Running external command: %s" % command)
    popen_inst = subprocess.Popen(command, shell=True, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE )
    logging.debug("stdin = %s" % str_stdin)
    str_stdout = str_stderr = ""
    while 1:
        read_from_child = -1
        if not popen_inst.stdin.closed:
            (rlist, wlist, xlist) = select([popen_inst.stdout, popen_inst.stderr], \
                                           [popen_inst.stdin], [])
        else:
            (rlist, wlist, xlist) = select([popen_inst.stdout, popen_inst.stderr], [], [])

        if popen_inst.stdout in rlist:
            tmpread =  popen_inst.stdout.read(4096)
            read_from_child = len(tmpread)
            str_stdout += tmpread
        
        if popen_inst.stderr in rlist:
            tmpread = popen_inst.stderr.read(4096)
            read_from_child += len(tmpread)
            str_stderr += tmpread
            
        if popen_inst.stdin in wlist and len(str_stdin) > 0:
            popen_inst.stdin.write(str_stdin[:min( [ len(str_stdin), 4096])])
            str_stdin = str_stdin[min( [ len(str_stdin), 4096]):]
            read_from_child += 1
        elif popen_inst.stdin in wlist:
            popen_inst.stdin.close()

        #logging.debug("len(str_stdin) = %i, read_from_child = %i, rlist = %s, wlist = %s", len(str_stdin), read_from_child, rlist, wlist)
        if popen_inst.poll() != None and len(str_stdin) == 0 and (read_from_child == -1 or read_from_child == 0):
            break
    
    logging.debug("Exit code: %i", popen_inst.wait())
    logging.debug("stdout: %s", str_stdout)
    logging.debug("strerr: %s", str_stderr)
    return str_stdout, str_stderr
