import datetime
import htcondor
import os
import time

class CondorUtils(object):

    @staticmethod
    def Debug(message):
        timestamp = time.time()
        timestamp_str = datetime.datetime.fromtimestamp(timestamp).strftime('%d/%m/%y %H:%M:%S')
        print timestamp_str + " " + message