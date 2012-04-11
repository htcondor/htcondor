#
# File for taring up necessary daemons for glidein to run properly
#
# Note, we still need glidein_startup

import tarfile
import logging
import os
import sys
import shutil
import tempfile
import glob
from campus_factory.util.CampusConfig import get_option


DEFAULT_GLIDEIN_DAEMONS = [ 'condor_master', 'condor_procd', 'condor_startd', \
                            'condor_starter' ]

# Glidein Librairies, can be globs
DEFAULT_GLIDEIN_LIBRARIES = [ 'libclassad.so*', 'libcondor_utils_*.so' ]

class DaemonWrangler:
    def __init__(self, daemons=None):
        """
        @param daemons: A list of daemons that will be included in the package
        """
        if daemons == None:
            self.daemons = DEFAULT_GLIDEIN_DAEMONS
        else:
            self.daemons = daemons
            
        self.libraries = DEFAULT_GLIDEIN_LIBRARIES
            
        self.campus_dir = os.environ['CAMPUSFACTORY_DIR']
            
    def Package(self, name=""): 
        if name == "":
            name = os.path.join(self.campus_dir,"share/glidein_jobs/glideinExec.tar.gz")
        
        # See if the daemons exist
        daemon_paths = self._CheckDaemons()
        if daemon_paths == None:
            logging.error("Unable to read all daemons, not packaging...")
            raise Exception("Unable to check all daemons")
        
        # And now libraries
        library_paths = self._CheckLibraries()
        if library_paths == None:
            logging.error("Unable to read all libraries, not packaging libraries...")
        else:
            daemon_paths.extend(library_paths)
        
        # Copy the daemons to a special directory to tar them
        tmpdir = tempfile.mkdtemp()
        target_dir = os.path.join(tmpdir, "glideinExec")
        os.mkdir(target_dir)
        for daemon_path in daemon_paths:
            shutil.copy(daemon_path, target_dir)
        
        # Add any other files that should go into the tar file
        shutil.copy(os.path.join(self.campus_dir, "share/glidein_jobs/glidein_startup"), target_dir)
        
        tfile = None
        try:
            tfile = tarfile.open(name, mode='w:gz')
            tfile.add(target_dir, arcname="glideinExec")
            tfile.close()
        except IOError, e:
            logging.error("Unable to open package file %s" % name)
            logging.error(str(e))
            shutil.rmtree(tmpdir)
            raise e
        
        # Clean up the temporary file
        shutil.rmtree(tmpdir)
        
        
        
    

    def _CheckDaemons(self):
        """
        Make sure that the daemons that are supposed to be packaged are
        available and readable.
        """
        
        condor_sbin = get_option("SBIN")
        logging.debug("Found SBIN directory = %s" % condor_sbin)
        daemon_paths = []
        for daemon in self.daemons:
            daemon_path = os.path.join(condor_sbin, daemon)
            logging.debug("Looking for daemon at: %s" % daemon_path)
            
            # Look for the daemons in the condor sbin directory.
            if os.path.exists(daemon_path):
                # Try opening the file
                fp = None
                try:
                    fp = open(daemon_path)
                    fp.close()
                    daemon_paths.append(daemon_path)
                except IOError, e:
                    logging.error("Unable to open file: %s" % daemon_path)
                    logging.error(str(e))
                    return None
            else:
                # If the file doesn't exist
                return None
        
        # Done checking all the daemons
        return daemon_paths


    def _CheckLibraries(self):
        """
        Make sure that the libraries that are supposed to be packaged are
        available and readable.
        """
        condor_lib = get_option("LIB")
        logging.debug("Found SBIN directory = %s" % condor_lib)
        library_paths = []
        for library in self.libraries:
            library_file_paths = glob.glob(os.path.join(condor_lib, library))
            for library_file in library_file_paths:
                if self._CheckFile(library_file):
                    library_paths.append(library_file)
            

        
        # Done checking all the daemons
        return library_paths


    def _CheckFile(self, path):
        logging.debug("Looking for file at: %s" % path)
            
        # Look for the daemons in the condor sbin directory.
        if os.path.exists(path):
            # Try opening the file
            fp = None
            try:
                fp = open(path)
                fp.close()
            except IOError, e:
                logging.error("Unable to open file: %s" % path)
                logging.error(str(e))
                return None
        else:
            # If the file doesn't exist
            return None
        
        return path







