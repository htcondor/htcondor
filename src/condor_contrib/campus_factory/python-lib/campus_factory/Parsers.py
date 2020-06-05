import logging
import xml.sax.handler
import os
from select import select

from campus_factory.util.ExternalCommands import RunExternal


class AvailableGlideins(xml.sax.handler.ContentHandler, object):
    
    # Command to query the collector for available glideins
    command = "condor_status -avail -const '(IsUndefined(Offline) == True) || (Offline == false)' -format '<glidein name=\"%s\"/>' 'Name'"
    
    
    def __init__(self):
        self.owner_idle = {}
        pass

    def GetIdle(self):
        self.idle = 0
        self.found = False

        # Get the xml from the collector
        to_parse, stderr = RunExternal(self.command)
        formatted_to_parse = "<doc>%s</doc>" % to_parse

        # Parse the data
        try:
            xml.sax.parseString(formatted_to_parse, self)
        except xml.sax._exceptions.SAXParseException as inst:
            logging.error("Error parsing:")
            logging.error("command = %s" % self.command)
            logging.error("stderr = %s" % stderr)
            logging.error("stdout = %s" % to_parse)
            logging.error("Error: %s - %s" % ( str(inst), inst.args ))
    
        if not self.found and (len(stderr) != 0):
            logging.error("No valid output received from command: %s"% self.command)
            logging.error("stderr = %s" % stderr)
            logging.error("stdout = %s" % to_parse)
            return None

        return self.idle
    
    def Run(self):
        """
        Generic function for when this class is inherited
        """
        return self.GetIdle()

    def startElement(self, name, attributes):
        if name == "glidein":
            self.idle += 1
            self.found = True
            
            if not self.owner_idle.has_key(attributes['owner']):
                self.owner_idle[attributes['owner']] = 0
            self.owner_idle[attributes['owner']] += 1
            
    def GetOwnerIdle(self):
        return self.owner_idle


class IdleGlideins(AvailableGlideins):
    
    command = "condor_q -const '(GlideinJob == true) && (JobStatus == 1)' -format '<glidein owner=\"%s\"/>' 'Owner'"
    

class IdleJobs(AvailableGlideins):
    
    command = "condor_q -name %s -const '(GlideinJob =!= true) &&  (JobStatus == 1) && (JobUniverse == 5)' -format '<glidein owner=\"%%s\"/>' 'Owner'"

    def __init__(self, schedd):
        super(IdleJobs, self).__init__()
        self.command = self.command % schedd

class IdleLocalJobs(AvailableGlideins):
    
    command = "condor_q -const '(GlideinJob =!= true) &&  (JobStatus == 1) && (JobUniverse == 5)' -format '<glidein owner=\"%s\"/>' 'Owner'"
        

class FactoryID(AvailableGlideins):
    command = "condor_q -const '(IsUndefined(IsFactory) == FALSE)' -format '<factory id=\"%s\"/>' 'ClusterId'"
    
    def startElement(self, name, attributes):
        if name == "factory":
            self.factory_id = attributes.getValue("id")
            self.found = True

    def GetId(self):
        self.GetIdle()
        return self.factory_id

class RunningGlideinsJobs(AvailableGlideins):
    """
    Gets the number of running glidein jobs (condor_q)
    """
    
    command = "condor_q -const '(GlideinJob == true) && (JobStatus == 2)' -format '<glidein owner=\"%s\"/>' 'Owner'"
    

class RunningGlideins(AvailableGlideins):
    """
    Returns the number of startd's reporting to the collector (condor_status)
    """
    
    command = "condor_status -const '(IsUndefined(IS_GLIDEIN) == FALSE) && (IS_GLIDEIN == TRUE) && (IsUndefined(Offline))' -format '<glidein name=\"%s\"/>' 'Name'"
    
    
    
    
