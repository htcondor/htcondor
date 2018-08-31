import datetime
import errno
import os
import sys
import time
import threading
import subprocess

class Utils(object):


    @staticmethod
    def Log(message):
        print("\t" + message)


    @staticmethod
    def TLog(message):
        timestamp = time.time()
        timestamp_str = datetime.datetime.fromtimestamp(timestamp).strftime('%d/%m/%y %H:%M:%S')
        print(timestamp_str + " " + message)


    @staticmethod
    def RunCondorTool(cmd):
        output = os.popen(cmd).read()
        return output


    @staticmethod
    def MakedirsIgnoreExist(directory):
        try:
            os.makedirs(directory)
        except:
            exctype, oe = sys.exc_info()[:2]
            if not issubclass(exctype, OSError): raise
            if oe.errno != errno.EEXIST:
                raise


    @staticmethod
    def RemoveIgnoreMissing(file):
        try:
            os.unlink(file)
        except:
            exctype, oe = sys.exc_info()[:2]
            if not issubclass(exctype, OSError): raise
            if oe.errno != errno.ENOENT:
                raise


    @staticmethod
    def IsWindows():
        if os.name == 'nt':
            return True
        return False

    @staticmethod
    def WriteFile( name, contents ):
        try:
            with open( name, 'w' ) as fd:
                fd.write( contents )
                return True
        except IOError as ioe:
            return False

    # For internal use only.
    @staticmethod
    def Alarm( child, timeout ):
        time.sleep( timeout )
        try:
            # Potentially-useful on Windows.
            # child.send_signal( subprocess.CTRL_C_EVENT )
            # child.send_signal( subprocess.CTRL_BREAK_EVENT )
            child.terminate()
            # Potentially-useful off Windows.
            # child.kill()
        except OSError as ose:
            if( ose.errno != 3 ):
                raise

    class CarefulCommandReturn(object):
        def __init__(self, result, expected_result, output, error):
            self.result = result
            self._expected_result = expected_result
            self.output = output
            self.error = error

        def __bool__(self):
            return self.result == self._expected_result
        # Python 3 compability.
        __nonzero__ = __bool__

    # The timeout is in seconds.
    @staticmethod
    def RunCommandCarefully( argv, timeout = 20, stdin = None, desired_return_code = 0 ):
        # The subprocess library doesn't include a time out in Python 3.3; the
        # timeout is available (nonetheless) in the Pip module subprocess32,
        # but to quote that module's README.md: "This code has not been tested
        # on Windows or other non-POSIX platforms."  On the other hand,
        # subprocess.Popen does everything we want except time out.  So, we
        # create a thread just before communicate()ing with the Popen object
        # that just sleeps until it's time to kill the child.
        child = subprocess.Popen( argv,
            stdin = None, stdout = subprocess.PIPE, stderr = subprocess.PIPE,
            universal_newlines = True,
            creationflags = getattr( subprocess, 'CREATE_NEW_PROCESS_GROUP', 0 ) )
        alarm = threading.Thread( target = Utils.Alarm, args = ( child, timeout ) )
        # The Python interpreter will exit if only daemon threads are left.
        # Since this thread blocks until the child process completes or the
        # daemon thread kills it, we know it's always safe to interrupt the
        # daemon thread (it has no shutdown code to run anyway).
        alarm.daemon = True
        alarm.start()
        ( output, error ) = child.communicate( stdin )
        return Utils.CarefulCommandReturn( child.returncode, desired_return_code,
            output, error )
