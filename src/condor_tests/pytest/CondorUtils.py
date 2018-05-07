import datetime
import htcondor
import os
import subprocess
import time

class CondorUtils(object):

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