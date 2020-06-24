#
# Project:
#   glideinWMS
#
# File Version: 
#   $Id: condorExe.py,v 1.6.12.1.8.1.6.1 2010/11/05 18:28:23 sfiligoi Exp $
#
# Description:
#   This module implements the functions to execute condor commands
#
# Author:
#   Igor Sfiligoi (Sept 7th 2006)
#


import os
import os.path
import popen2
import string

class UnconfigError(RuntimeError):
    def __init__(self,str):
        RuntimeError.__init__(self,str)

class ExeError(RuntimeError):
    def __init__(self,str):
        RuntimeError.__init__(self,str)

#
# Configuration
#

# Set path to condor binaries, if needed
def set_path(new_condor_bin_path,new_condor_sbin_path=None):
    global condor_bin_path,condor_sbin_path
    condor_bin_path=new_condor_bin_path
    if new_condor_sbin_path is not None:
        condor_sbin_path=new_condor_sbin_path
    


#
# Execute an arbitrary condor command and return its output as a list of lines
#  condor_exe uses a relative path to $CONDOR_BIN
# Fails if stderr is not empty
#

# can throw UnconfigError or ExeError
def exe_cmd(condor_exe,args,stdin_data=None):
    global condor_bin_path

    if condor_bin_path is None:
        raise UnconfigError("condor_bin_path is undefined!")
    condor_exe_path=os.path.join(condor_bin_path,condor_exe)

    cmd="%s %s" % (condor_exe_path,args)

    return iexe_cmd(cmd,stdin_data)

def exe_cmd_sbin(condor_exe,args,stdin_data=None):
    global condor_sbin_path

    if condor_sbin_path is None:
        raise UnconfigError("condor_sbin_path is undefined!")
    condor_exe_path=os.path.join(condor_sbin_path,condor_exe)

    cmd="%s %s" % (condor_exe_path,args)

    return iexe_cmd(cmd,stdin_data)

############################################################
#
# P R I V A T E, do not use
#
############################################################

# can throw ExeError
def iexe_cmd(cmd,stdin_data=None):
    child=popen2.Popen3(cmd,True)
    if stdin_data is not None:
        child.tochild.write(stdin_data)
    child.tochild.close()
    tempOut = child.fromchild.readlines()
    child.fromchild.close()
    tempErr = child.childerr.readlines()
    child.childerr.close()
    try:
        errcode=child.wait()
    except OSError as e:
        if len(tempOut)!=0:
            # if there was some output, it is probably just a problem of timing
            # have seen a lot of those when running very short processes
            errcode=0
        else:
            raise ExeError("Error running '%s'\nStdout:%s\nStderr:%s\nException OSError: %s"%(cmd,tempOut,tempErr,e))
    if (errcode!=0):
        raise ExeError("Error running '%s'\ncode %i:%s"%(cmd,errcode,tempErr))
    return tempOut

#
# Set condor_bin_path
#

def init1():
    global condor_bin_path
    # try using condor commands to find it out
    try:
        condor_bin_path=iexe_cmd("condor_config_val BIN")[0][:-1] # remove trailing newline
    except ExeError as e:
        # try to find the RELEASE_DIR, and append bin
        try:
            release_path=iexe_cmd("condor_config_val RELEASE_DIR")
            condor_bin_path=os.path.join(release_path[0][:-1],"bin")
        except ExeError as e:
            # try condor_q in the path
            try:
                condorq_bin_path=iexe_cmd("which condor_q")
                condor_bin_path=os.path.dirname(condorq_bin_path[0][:-1])
            except ExeError as e:
                # look for condor_config in /etc
                if os.environ.has_key("CONDOR_CONFIG"):
                    condor_config=os.environ["CONDOR_CONFIG"]
                else:
                    condor_config="/etc/condor/condor_config"
                
                try:
                    # BIN = <path>
                    bin_def=iexe_cmd('grep "^ *BIN" %s'%condor_config)
                    condor_bin_path=string.split(bin_def[0][:-1])[2]
                except ExeError as e:
                    try:
                        # RELEASE_DIR = <path>
                        release_def=iexe_cmd('grep "^ *RELEASE_DIR" %s'%condor_config)
                        condor_bin_path=os.path.join(string.split(release_def[0][:-1])[2],"bin")
                    except ExeError as e:
                        pass # don't know what else to try

#
# Set condor_sbin_path
#

def init2():
    global condor_sbin_path
    # try using condor commands to find it out
    try:
        condor_sbin_path=iexe_cmd("condor_config_val SBIN")[0][:-1] # remove trailing newline
    except ExeError as e:
        # try to find the RELEASE_DIR, and append bin
        try:
            release_path=iexe_cmd("condor_config_val RELEASE_DIR")
            condor_sbin_path=os.path.join(release_path[0][:-1],"sbin")
        except ExeError as e:
            # try condor_q in the path
            try:
                condora_sbin_path=iexe_cmd("which condor_advertise")
                condor_sbin_path=os.path.dirname(condora_sbin_path[0][:-1])
            except ExeError as e:
                # look for condor_config in /etc
                if os.environ.has_key("CONDOR_CONFIG"):
                    condor_config=os.environ["CONDOR_CONFIG"]
                else:
                    condor_config="/etc/condor/condor_config"
                
                try:
                    # BIN = <path>
                    bin_def=iexe_cmd('grep "^ *SBIN" %s'%condor_config)
                    condor_sbin_path=string.split(bin_def[0][:-1])[2]
                except ExeError as e:
                    try:
                        # RELEASE_DIR = <path>
                        release_def=iexe_cmd('grep "^ *RELEASE_DIR" %s'%condor_config)
                        condor_sbin_path=os.path.join(string.split(release_def[0][:-1])[2],"sbin")
                    except ExeError as e:
                        pass # don't know what else to try

def init():
    init1()
    init2()

# This way we know that it is undefined
condor_bin_path=None
condor_sbin_path=None

init()


    
