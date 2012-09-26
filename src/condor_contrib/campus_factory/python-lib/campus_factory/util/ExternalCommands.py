# File for running external commands
#
#
import logging
import os
from popen2 import Popen3
from select import select


def RunExternal(command, str_stdin=""):
    """Run an external command 
    
    @param command: String of the command to execute
    @param stdin: String to put put in stdin
    @return: (str(stdout), str(stderr)) of command
    Returns the stdout and stderr
    
    """

    logging.info("Running external command: %s" % command)
    popen_inst = Popen3(command, True)
    logging.debug("stdin = %s" % str_stdin)
    str_stdout = str_stderr = ""
    while 1:
        read_from_child = -1
        if not popen_inst.tochild.closed:
            (rlist, wlist, xlist) = select([popen_inst.fromchild, popen_inst.childerr], \
                                           [popen_inst.tochild], [])
        else:
            (rlist, wlist, xlist) = select([popen_inst.fromchild, popen_inst.childerr], [], [])

        if popen_inst.fromchild in rlist:
            tmpread =  popen_inst.fromchild.read(4096)
            read_from_child = len(tmpread)
            str_stdout += tmpread
        
        if popen_inst.childerr in rlist:
            tmpread = popen_inst.childerr.read(4096)
            read_from_child += len(tmpread)
            str_stderr += tmpread
            
        if popen_inst.tochild in wlist and len(str_stdin) > 0:
            popen_inst.tochild.write(str_stdin[:min( [ len(str_stdin), 4096])])
            str_stdin = str_stdin[min( [ len(str_stdin), 4096]):]
            read_from_child += 1
        elif popen_inst.tochild in wlist:
            popen_inst.tochild.close()

        #logging.debug("len(str_stdin) = %i, read_from_child = %i, rlist = %s, wlist = %s", len(str_stdin), read_from_child, rlist, wlist)
        if popen_inst.poll() != -1 and len(str_stdin) == 0 and (read_from_child == -1 or read_from_child == 0):
            break
    
    logging.debug("Exit code: %i", popen_inst.wait())
    logging.debug("stdout: %s", str_stdout)
    logging.debug("strerr: %s", str_stderr)
    return str_stdout, str_stderr
