#
# Project:
#   glideinWMS
#
# File Version: 
#   $Id: condorSecurity.py,v 1.1.4.1.2.1 2010/08/31 18:49:17 parag Exp $
#
# Description:
#   This module implements classes that will setup
#   the Condor security as needed
#
# Author:
#   Igor Sfiligoi @ UCSD (Apr 2010)
#

import os
import copy

###############
#
# Base classes
#
###############

########################################
# This class contains the state of the
# Condor environment
#
# All info is in the state attribute
class EnvState:
    def __init__(self,filter):
        # filter is a list of Condor variables to save
        self.filter=filter
        self.load()
    
    #################################
    # Restore back to what you found
    # when creating this object
    def restore(self):
        for condor_key in self.state.keys():
            env_key="_CONDOR_%s"%condor_key
            old_val=self.state[condor_key]
            if old_val is not None:
                os.environ[env_key]=old_val
            else:
                del os.environ[env_key]

    ##########################################
    # Load the environment state into
    # Almost never called by the user
    # It gets called automatically by __init__
    def load(self):
        filter=self.filter
        saved_state={}
        for condor_key in filter:
            env_key="_CONDOR_%s"%condor_key
            if os.environ.has_key(env_key):
                saved_state[condor_key]=os.environ[env_key]
            else:
                saved_state[condor_key]=None # unlike requests, we want to remember there was nothing
        self.state=saved_state


########################################
# This class contains the state of the
# security Condor environment

def convert_sec_filter(sec_filter):
        filter=[]
        for context in sec_filter.keys():
            for feature in sec_filter[context]:
                condor_key="SEC_%s_%s"%(context,feature)
                filter.append(condor_key)
        return filter

class SecEnvState(EnvState):
    def __init__(self,sec_filter):
        # sec_filter is a dictionary of [contex]=[feature list]
        EnvState.__init__(self,convert_sec_filter(sec_filter))
        self.sec_filter=sec_filter

###############################################################
# This special value will unset any value and leave the default
# This is different than None, that will not do anything
UNSET_VALUE='UNSET'

#############################################
# This class handle requests for ensuring
# the security state is in a particular state
class SecEnvRequest:
    def __init__(self,requests=None):
        # requests is a dictionary of requests [context][feature]=VAL
        self.requests={}
        if requests is not None:
            for context in requests.keys():
                for feature in requests[context].keys():
                    self.set(context,feature,requests[context][feature])

        self.saved_state=None


    ##############################################
    # Methods for accessig the requests
    def set(self,context,feature,value): # if value is None, remove the request
        if value is not None:
            if not self.requests.has_key(context):
                self.requests[context]={}
            self.requests[context][feature]=value
        elif self.requests.has_key(context):
            if self.requests[context].has_key(feature):
                del self.requests[context][feature]
                if len(self.requests[context].keys())==0:
                    del self.requests[context]
    
    def get(self,context,feature):
        if self.requests.has_key(context):
            if self.requests[context].has_key(feature):
                return self.requests[context][feature]
            else:
                return None
        else:
            return None

    ##############################################
    # Methods for preserving the old state

    def has_saved_state(self):
        return (self.saved_state is not None)

    def save_state(self):
        if self.has_saved_state():
            raise RuntimeError("There is already a saved state! Restore that first.")
        filter={}
        for c in self.requests.keys():
            filter[c]=self.requests[c].keys()
            
        self.saved_state=SecEnvState(filter)

    def restore_state(self):
        if self.saved_state is None:
            return # nothing to do

        self.saved_state.restore()
        self.saved_state=None

    ##############################################
    # Methods for changing to the desired state

    # you should call save_state before this one,
    # if you want to ever get back
    def enforce_requests(self):
        for context in self.requests.keys():
            for feature in self.requests[context].keys():
                condor_key="SEC_%s_%s"%(context,feature)
                env_key="_CONDOR_%s"%condor_key
                val=self.requests[context][feature]
                if val!=UNSET_VALUE:
                    os.environ[env_key]=val
                else:
                    # unset -> make sure it is not in the env after the call
                    if os.environ.has_key(env_key):
                        del os.environ[env_key]
        return

########################################################################
#
# Security protocol handhake classes
# This are the most basic features users may want to change
#
########################################################################

CONDOR_CONTEXT_LIST=('DEFAULT',
                     'ADMINISTRATOR','NEGOTIATOR','CLIENT','OWNER',
                     'READ','WRITE','DAEMON','CONFIG',
                     'ADVERTISE_MASTER','ADVERTISE_STARTD','ADVERTISE_SCHEDD')

CONDOR_PROTO_FEATURE_LIST=('AUTHENTICATION',
                           'INTEGRITY','ENCRYPTION',
                           'NEGOTIATION')

CONDOR_PROTO_VALUE_LIST=('NEVER','OPTIONAL','PREFERRED','REQUIRED')

########################################
class EnvProtoState(SecEnvState):
    def __init__(self,filter=None):
        if filter is not None:
            # validate filter
            for c in filter.keys():
                if not (c in CONDOR_CONTEXT_LIST):
                    raise ValueError( "Invalid contex '%s'. Must be one of %s"%(c,CONDOR_CONTEXT_LIST))
                for f in filter[c]:
                    if not (f in CONDOR_PROTO_FEATURE_LIST):
                        raise ValueError( "Invalid feature '%s'. Must be one of %s"%(f,CONDOR_PROTO_FEATURE_LIST))
        else:
            # do not filter anything out... add all
            filter={}
            for c in CONDOR_CONTEXT_LIST:
                cfilter=[]
                for f in CONDOR_PROTO_FEATURE_LIST:
                    cfilter.append(f)
                filter[c]=cfilter
        
        SecEnvState.__init__(self,filter)

#########################################
# Same as SecEnvRequest, but check that
# the context and feature are related
# to the Condor protocol handling
class ProtoRequest(SecEnvRequest):
    def set(self,context,feature,value): # if value is None, remove the request
        if not (context in CONDOR_CONTEXT_LIST):
            raise ValueError( "Invalid security context '%s'."%context)
        if not (feature in CONDOR_PROTO_FEATURE_LIST):
            raise ValueError( "Invalid authentication feature '%s'."%feature)
        if not (value in (CONDOR_PROTO_VALUE_LIST+(UNSET_VALUE,))):
            raise ValueError( "Invalid value type '%s'."%value)
        SecEnvRequest.set(self,context,feature,value)

    def get(self,context,feature):
        if not (context in CONDOR_CONTEXT_LIST):
            raise ValueError( "Invalid security context '%s'."%context)
        if not (feature in CONDOR_PROTO_FEATURE_LIST):
            raise ValueError( "Invalid authentication feature '%s'."%feature)
        return SecEnvRequest.get(self,context,feature)

########################################################################
#
# GSI specific classes classes
# Extend ProtoRequest
# These assume all the communication will be GSI authenticated
#
########################################################################

class GSIRequest(ProtoRequest):
    def __init__(self, x509_proxy=None, allow_fs=True, proto_requests=None):
        auth_str="GSI"
        if allow_fs:
            auth_str+=",FS"

        # force GSI authentication
        if proto_requests is not None:
            proto_request=copy.deepcopy(proto_requests)
        else:
            proto_request={}
        for context in CONDOR_CONTEXT_LIST:
            if not proto_requests.has_key(context):
                proto_requests[context]={}
            if proto_requests[context].has_key('AUTHENTICATION'):
                if not ('GSI' in proto_requests[context]['AUTHENTICATION'].split(',')):
                    raise ValueError("Must specify GSI as one of the options")
            else:
                proto_requests[context]['AUTHENTICATION']=auth_str
        
        ProtoRequest.__init__(self,proto_requests)
        self.allow_fs=allow_fs

        if x509_proxy is None:
            if not os.environ.has_key('X509_USER_PROXY'):
                raise RuntimeError( "x509_proxy not provided and env(X509_USER_PROXY) undefined")
            x509_proxy=os.environ['X509_USER_PROXY']

        # Here I should probably check if the proxy is valid
        # To be implemented in a future release
        
        self.x509_proxy=x509_proxy

    ##############################################
    def save_state(self):
        if self.has_saved_state():
            raise RuntimeError("There is already a saved state! Restore that first.")

        if os.environ.has_key('X509_USER_PROXY'):
            self.x509_proxy_saved_state=os.environ['X509_USER_PROXY']
        else:
            self.x509_proxy_saved_state=None # unlike requests, we want to remember there was nothing
        ProtoRequest.save_state(self)

    def restore_state(self):
        if self.saved_state is None:
            return # nothing to do

        ProtoRequest.restore_state(self)

        if self.x509_proxy_saved_state is not None:
            os.environ['X509_USER_PROXY']=self.x509_proxy_saved_state
        else:
            del os.environ['X509_USER_PROXY']

        # unset, just to prevent bugs
        self.x509_proxy_saved_state = None


    ##############################################
    def enforce_requests(self):
        ProtoRequest.enforce_requests(self)
        os.environ['X509_USER_PROXY']=self.x509_proxy

