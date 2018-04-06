import classad
import htcondor
import os
import time

from CondorPersonal import CondorPersonal
from CondorUtils import CondorUtils


class CondorPytest(object):

    def __init__(self, name):
        self._name = name


    def RunTest(self, submit_filename):

        success = False

        # Setup a personal condor environment
        CondorUtils.Debug("Attempting to start test " + self._name)
        personal = CondorPersonal(self._name)
        start_success = personal.StartCondor()
        if start_success is False:
            CondorUtils.Debug("Failed to start the personal condor environment. Exiting.")
            return False
        else:
            CondorUtils.Debug("Personal condor environment started successfully!")

        # Wait until we're sure all daemons have started
        # MRC: Can we do this using python bindings? Or do we need to use condor_who?
        time.sleep(5)

        # Translate the submit file to classad format
        ad_filename = self._name + ".ad"
        self.GenerateAdFile(submit_filename, ad_filename)

        schedd = htcondor.Schedd()
        ad = classad.parseOne(open(ad_filename))
        print("ad = " + str(ad))
        result_ads = []

        # Submit the test
        CondorUtils.Debug("Submitting the test job...")
        try:
            cluster = schedd.submit(ad, 1, False, result_ads)
        except:
            CondorUtils.Debug("Failed to submit job")
            personal.ShutdownCondor()
            return False
            
        CondorUtils.Debug("Test job running on cluster " + str(cluster))

        # Wait until test has finished running by watching the job status
        # MRC: Timeout should be user configurable
        timeout = 240
        for i in range(timeout):
            ads = schedd.query("ClusterId == %d" % cluster, ["JobStatus"])
            CondorUtils.Debug("Ads = " + str(ads))
            # When the job is complete, ads will be empty
            if len(ads) == 0:
                success = True
                break
            else:
                status = ads[0]["JobStatus"]
                if status == 5:
                    CondorUtils.Debug("Job was placed on hold. Aborting.")
                    break
            time.sleep(1)

        # Shutdown personal condor environment + cleanup
        personal.ShutdownCondor()

        return success

    # Takes a submit file and generates a classad file that schedd.submit()
    # will accept. We use condor_submit -dump, then parse the file to make sure
    # all expressions are in double quotes.
    def GenerateAdFile(self, submit_filename, ad_filename):
        os.system("condor_submit -dump " + ad_filename + " " + submit_filename)
        """
        with open(ad_filename) as f:
            ad_contents = f.readlines()
        ad_file = open(ad_filename, "w")
        for line in ad_contents:
            line = line.strip("\n")
            attr = line[:line.find("=")]
            value = line[line.find("=")+1:]
            if len(value) > 0: 
                if value[0] != "\"":
                    value = value.replace("\"", "'")
                    value = "\"" + value + "\""
                ad_file.write(attr + "=" + value + "\n")
        ad_file.close()
        """



