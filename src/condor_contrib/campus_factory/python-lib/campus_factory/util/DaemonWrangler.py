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
from campus_factory.util.ExternalCommands import RunExternal


DEFAULT_GLIDEIN_DAEMONS = [ 'condor_master', 'condor_procd', 'condor_startd', \
                            'condor_starter' ]

class DaemonWrangler:
    def __init__(self, daemons=None, base_condor_dir = None, dumb_package = False):
        """
        @param daemons: A list of daemons that will be included in the package
        """
        if daemons is None:
            self.daemons = DEFAULT_GLIDEIN_DAEMONS
        else:
            self.daemons = daemons
        
        try:
            self.glidein_dir = get_option("GLIDEIN_DIRECTORY")
        except:
            self.glidein_dir = ""
            
        self.base_condor_dir = base_condor_dir
        self.dumb_package = dumb_package
        
            
    def Package(self, name=""): 
        if name == "":
            name = os.path.join(self.glidein_dir,"glideinExec.tar.gz")
        
        # See if the daemons exist
        daemon_paths = self._CheckDaemons()
        if daemon_paths is None:
            logging.error("Unable to read all daemons, not packaging...")
            raise Exception("Unable to check all daemons")
        
        # And now libraries
        if self.dumb_package:
            library_paths = self._GetAllLibs()
        else:
            library_paths = self._GetDynamicLibraries(daemon_paths)
            
        try:
            self._CheckLibraries(library_paths)
        except Exception as e:
            logging.error("Unable to read all libraries, not packaging libraries...")
            raise e
        
        daemon_paths.extend(library_paths)
        
        
        # Copy the daemons to a special directory to tar them
        tmpdir = tempfile.mkdtemp()
        target_dir = os.path.join(tmpdir, "glideinExec")
        os.mkdir(target_dir)
        for daemon_path in daemon_paths:
            shutil.copy(daemon_path, target_dir)
        
        # Add any other files that should go into the tar file
        shutil.copy(os.path.join(self.glidein_dir, "glidein_startup"), target_dir)
        
        tfile = None
        try:
            tfile = tarfile.open(name, mode='w:gz')
            tfile.add(target_dir, arcname="glideinExec")
            tfile.close()
        except IOError as e:
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
        
        # If running on a specific base dir
        if self.base_condor_dir:
            condor_sbin = os.path.join(self.base_condor_dir, "sbin")
        else:
            condor_sbin = get_option("SBIN")
        logging.debug("Found SBIN directory = %s" % condor_sbin)
        daemon_paths = []
        for daemon in self.daemons:
            daemon_path = os.path.join(condor_sbin, daemon)
            if self._CheckFile(daemon_path):
                daemon_paths.append(daemon_path)
        
        # Done checking all the daemons
        return daemon_paths


    def _GetAllLibs(self, libdirs = ['lib', 'lib/condor']):
        """
        Get all libs in the lib directories 
        """
        toreturn = []
       
        # For each dir in libdirs, get the files
        for libdir in libdirs:
            pathname = os.path.join(self.base_condor_dir, libdir)
            for libfile in os.listdir(pathname):
                full_libfile = os.path.join(pathname, libfile)
                if not os.path.isdir(full_libfile):
                    toreturn.append(full_libfile)

        return toreturn
        


    def _CheckLibraries(self, library_paths):
        """
        Make sure that the libraries that are supposed to be packaged are
        available and readable.
        """
        checked_library_paths = []
        for library in library_paths:
            if self._CheckFile(library):
                checked_library_paths.append(library)
            else:
                logging.debug("Unable to read library file %s" % library)
                raise Exception("Unable to read library file %s" % library)
            

        
        # Done checking all the daemons
        return checked_library_paths


    def _CheckFile(self, path):
        logging.debug("Looking for file at: %s" % path)
            
        # Look for the daemons in the condor sbin directory.
        if os.path.exists(path):
            # Try opening the file
            fp = None
            try:
                fp = open(path)
                fp.close()
            except IOError as e:
                logging.error("Unable to open file: %s" % path)
                logging.error(str(e))
                return None
        else:
            # If the file doesn't exist
            return None
        
        return path
    
    
    def _ldd(self, file):
        """
        Given a file return all the libraries referenced by the file
    
        @type file: string
        @param file: Full path to the file
    
        @return: List containing linked libraries required by the file
        @rtype: list
        """
    
        rlist = []
        if os.path.exists(file):
            (stdout, stderr) = RunExternal("ldd %s" % file)
            for line in stdout.split('\n'):
                tokens = line.split('=>')
                if len(tokens) == 2:
                    lib_loc = ((tokens[1].strip()).split(' '))[0].strip()
                    if os.path.exists(lib_loc):
                        rlist.append(os.path.abspath(lib_loc))
        return rlist
    
    
    def _GetDynamicLibraries(self, files, libdirs = ['lib', 'lib/condor']):
        """
        Get the dynamic libraries that the files are using
        (Adapted from get_condor_dlls in glideinwms)
        
        @param files: files to check for dynamic libraries
        """
        
        libstodo = set()
        libsdone = set()
        rlist = []
        
        condor_dir = get_option("RELEASE_DIR")
        
        # First, get the initial libraries
        for file in files:
            libstodo.update(self._ldd(file))
        
        while len(libstodo) > 0:
            lib = libstodo.pop()
            
            # Already did library?
            if lib in rlist:
                continue
            
            if not lib.startswith(condor_dir):
                # Check if the library is provided by condor
                # If so, add the condor provided lib to process
                # Overriding the system's library (condor knows best?)
                libname = os.path.basename(lib)
                for libdir in libdirs:
                    if os.path.exists(os.path.join(condor_dir, libdir, libname)):
                        new_lib = os.path.join(condor_dir, libdir, libname)
                        if new_lib not in rlist:
                            libstodo.add(new_lib)
                            libsdone.add(lib)
            else:
                # In the condor directory
                new_libstodo = set(self._ldd(lib))
                libsdone.add(lib)
                libstodo.update(new_libstodo - libsdone)
                rlist.append(lib)
                
        return rlist




