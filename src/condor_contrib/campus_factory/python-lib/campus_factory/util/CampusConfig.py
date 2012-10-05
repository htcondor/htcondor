# 
# All configuration options should be received from this file
#

import os
import ConfigParser
import logging

from campus_factory.ClusterStatus import CondorConfig

parsed_config_file = None
local_set_options = {}

def get_option(option, default=None):
    """
    This function looks through the configuration for an option.
    
    @param option: The option to lookup
    @param default: The default if the option isn't found
    @return: The option with key = param option, or default if the option is not found.
    """
    
    global local_set_options
    # First, check locally set options
    if option in local_set_options:
        return local_set_options[option]
    
    # Second get from environment
    env_option = _get_option_env(option)
    if env_option:
        return env_option
    
    # Third get from the campus factory configuration
    config_option = _get_config_option(option)
    if config_option:
        return config_option
    
    # Forth, get from the condor configuration
    condor_option = _get_condor_option(option)
    if condor_option:
        return condor_option
    
    # If all else fails, return the default
    return default

    

def get_option_section(section, option, default=None):
    """
    Option argument only applies to the condor config file
    
    @param section: Section in the configuration file to search for the option
    @param option: Option to return value for
    @param default: Default value
    
    """ 
    
    config_response = _get_config_option(option, section)
    if config_response != None:
        return config_response
    else:
        return default
    

def set_option(option, value):
    """
    Set a function for this instance of the factory.  Useful when one setting
    is detected that can affect another.
    
    @param option: Option to be set
    @param value: Value to set option to.
    """
    global local_set_options
    local_set_options[option] = value
    

def set_config_file(filename):
    global parsed_config_file
    parsed_config_file = ConfigParser.ConfigParser()
    files_read = parsed_config_file.read([filename])
    return files_read
    

def _get_option_env(option):
    real_option_name = "".join(["_campusfactory_", option])
    if os.environ.has_key(real_option_name):
        return os.environ[real_option_name]
    else:
        return None
    
    
def _get_config_option (option, section="general"):
    global parsed_config_file
    if parsed_config_file == None:
        return None
    
    if parsed_config_file.has_option(section, option):
        return parsed_config_file.get(section, option)
    else:
        return None

def _get_condor_option(option):
    condor_config = None
    try:
        condor_config = CondorConfig()
    except EnvironmentError, inst:
        logging.exception(str(inst))
        raise inst
    condor_option = condor_config.get(option)
    if len(condor_option) == 0:
        return None
    else:
        return condor_option


