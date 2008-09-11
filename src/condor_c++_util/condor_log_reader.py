#! /usr/bin/env python
##**************************************************************
##
## Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

import re
import os
import sys
import time

# Static global data( yeah, yuck)
class EventPatterns( object  ):
    def __init__( self ) :
        self.__re_hdr = re.compile( '(\d{3}) \((\d+)\.(\d+)\.(\d+)\) ' +
                                    '(\d+/\d+ \d+:\d+:\d+) (.*)' )
    def IsSeparatorLine( self, line ) :
        return line == "..."
    def HasSeparator( self, line ) :
        return line.find( "..." ) >= 0
    def IsHeaderLine( self, line ) :
        m = self.__re_hdr.match( line )
        return m is not None
    def LineHasHeader( self, line ) :
        m = self.__re_hdr.search( line )
        return m is not None
    def FindHeaderInLine( self, line, match = True ) :
        m = None
        if match :
            m = self.__re_hdr.match( line )
        else :
            m = self.__re_hdr.search( line )
        if m is None :
            return None
        return m

_patterns = EventPatterns( )


# Event header class
class EventHeader( object ) :
    def __init__( self, event_num, cluster, proc, subproc, time, text):
        self.__event_num = event_num
        self.__cluster   = cluster
        self.__proc      = proc
        self.__subproc   = subproc
        self.__time      = time
        self.__text      = text

        self.__line      = None
        self.__line_num  = None
        self.__file_name = None
    def EventNum( self ) :
        return self.__event_num
    def IsEvent( self, event_num ) :
        return event_num == self.__event_num
    def Cluster( self ) :
        return self.__cluster
    def Proc( self ) :
        return self.__proc
    def SubProc( self ) :
        return self.__subproc
    def JobId( self ) :
        return "%d.%d.%d" % ( self.__cluster, self.__proc, self.__subproc )
    def Time( self ) :
        return self.__time
    def Text( self ) :
        return self.__text
        
    def SetLineInfo( self, filename, line_num, line ) :
        self.__file_name = filename
        self.__line_num = line_num
        self.__line = line
    def LineText( self ) :
        return self.__line
    def LineNum( self ) :
        return self.__line_num
    def SetLineNum( self, num ) :
        return self.__line_num
    def FileName( self ) :
        return self.__file_name

class EventMisMatchException( Exception ) :
    pass
class EventVerifyException( Exception ) :
    pass

# Base event class
class BaseEvent( object ) :

    class VerifyData( object ) :
        def __init__( self, lnum, string, where = 0, pos = 0, expect = True ) :
            self.__lnum = lnum		# Line #
            self.__string = string	# String to search for
            self.__where = where	# -1=endswith,0=find,1=startswith
            self.__pos = pos		# position
            self.__expect = expect
        def Verify( self, lines ) :
            line = lines[self.__lnum]
            pos = -1
            error = False
            op = ""
            if self.__where < 0 :
                pos = line.endswith(self.__string, self.__pos)
                op = "endswith"
            elif self.__where > 0 :
                pos = line.startswith(self.__string, self.__pos)
                op = "startswith"
            else :
                pos = line.find(self.__string, self.__pos)
                op = "find"

            if self.__expect :
                if pos < 0 :
                    return "line # %d '%s' : '%s' not found @ %d" % \
                           ( self.__lnum, line, self.__string, self.__pos )
            else :
                if pos >= 0 :
                    return "line # %d '%s' : '%s' found @ %d" % \
                           ( self.__lnum, line, self.__string, self.__pos )
            return None


    def __init__( self, event_num, event_name, header = None ) :
        self._header = header
        self._lines = []
        if header is not None :
            if not header.IsEvent( event_num ) :
                raise EventMisMatchException
            self._lines.append( header.LineText() )
        self._event_num = event_num
        self._event_name = event_name

    def EventNum( self ) :
        return self._event_num
    def EventName( self ) :
        return self._event_name
    def EventNameUc( self ) :
        return self._event_name.upper()
    def HeaderNumCheck( self, header ) :
        return header.IsEvent( self.EventNum() )

    
    def NumLines( self ) :
        return len(self._lines)
    # Lines excluding header and separator
    def NumBodyLines( self ) :
        return len(self._lines) - 2
    def LineNum( self, num = 0 ) :
        return self._header.LineNum() + num

    def GetHostFromText( self, text ) :
        m = re.search( '(<\d+\.\d+\.\d+\.\d+:\d+>)', text )
        if m is not None :
            return m.group(1)
        else :
            return None

    def GetHostFromHeader( self ) :
        assert self._header
        return self.GetHostFromText( self._header.Text() )

    # By default, keep reading 'til we get a "..."
    def ProcessEventLine( self, line ) :
        self._lines.append( line )
        return _patterns.IsSeparatorLine( line )

    def Verify( self, level = 0 ):
        return True

    def SimpleVerify( self, level,
                      min_body_lines, max_body_lines, verify_data ) :
        s = "Event @ line # %d (%s): "%( self.LineNum(0), self.EventNameUc() )
        if min_body_lines is not None  and \
               self.NumBodyLines() < min_body_lines :
            s += "Too few lines (%d < %d)" % \
                 ( self.NumBodyLines(), min_body_lines )
            raise EventVerifyException( s )
        if max_body_lines is not None  and \
               self.NumBodyLines() > max_body_lines :
            s += "Too many lines (%d < %d)" % \
                 ( self.NumBodyLines(), max_body_lines )
            raise EventVerifyException( s )
        for verify in verify_data :
            vs = verify.Verify( self._lines )
            if vs is not None :
                raise EventVerifyException( vs )
        if level >= 1 :
            for n in range(1, len(self._lines)-1 ) :
                line = self._lines[n]
                s = "Event @ line # %d (%s): " % ( self.LineNum(n),
                                                   self.EventNameUc() )
                if _patterns.HasSeparator( line ) :
                    raise EventVerifyException( s + " '...' separator found" )
                if _patterns.LineHasHeader( line ) :
                    raise EventVerifyException( s + " header found" )

    def Print( self, full = False ) :
        print "Event %s (%d) @ line %d (%d body lines)%s" % \
              ( self.EventNameUc(), self.EventNum(),
                self.LineNum(), self.NumBodyLines(),
                (":" if full else "") )
        if full :
            for l in self._lines :
                print l


# Job submitted               ULOG_SUBMIT               	= 0
class SubmitEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 0, "submit", header )
        if header is not None :
            self._host = self.GetHostFromHeader()
    def EventNum( self ) :
        return 0
    def Verify( self, level = 0 ) :
        self.SimpleVerify( level, 0, None,
                           [self.VerifyData(0, "Job submitted", 1)]  )

# Job now running             ULOG_EXECUTE 					= 1
class ExecuteEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 1, "execute", header )
        if header is not None :
            self._host = self.GetHostFromHeader()
    def EventNum( self ) :
        return 1
    def Verify( self, level = 0 ) :
        self.SimpleVerify( level, 0, 0,
                           [self.VerifyData(0, "Job executing", 1)]  )

# Error in executable         ULOG_EXECUTABLE_ERROR 		= 2
class ExecutableErrorEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 2, "executable error", header )
    def EventNum( self ) : return 2

# Job was checkpointed        ULOG_CHECKPOINTED 			= 3
class CheckpointEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 3, "checkpointed", header )
    def EventNum( self ) :
        return 3

# Job evicted from machine    ULOG_JOB_EVICTED 				= 4
class JobEvictedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 4, "evicted", header )
    def EventNum( self ) :
        return 4
    def Verify( self, level = 0 ) :
        v = ( self.VerifyData(0, "evicted.", -1),
              self.VerifyData(1, "checkpointed.", -1),
              self.VerifyData(2, "Remote Usage", -1),
              self.VerifyData(3, "Local Usage", -1),
              self.VerifyData(4, "Sent By Job", -1),
              self.VerifyData(5, "Received By Job", -1) )
        self.SimpleVerify( level, 5, 5, v )

# Job terminated              ULOG_JOB_TERMINATED 			= 5
class JobTerminateEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 5, "terminated", header )
    def EventNum( self ) : return 5

# Image size of job updated   ULOG_IMAGE_SIZE 				= 6
class ImageSizeEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 6, "image size", header )
    def EventNum( self ) : return 6

# Shadow threw an exception   ULOG_SHADOW_EXCEPTION 		= 7
class ShadowExceptionEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 7, "shadow exception", header )
    def EventNum( self ) : return 7

# Generic Log Event           ULOG_GENERIC 					= 8
class GenericEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 8, "generic", header )
    def EventNum( self ) :
        return 8

# Job Aborted                 ULOG_JOB_ABORTED 				= 9
class JobAbortedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 9, "job aborted", header )
    def EventNum( self ) :
        return 9
    def Verify( self, level = 0 ) :
        self.SimpleVerify( level, 0, None,
                           [self.VerifyData(0, "Job was aborted", 1)]  )

# Job was suspended           ULOG_JOB_SUSPENDED 			= 10
class JobSuspendedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 10, "job suspended", header )
    def EventNum( self ) : return 10

# Job was unsuspended         ULOG_JOB_UNSUSPENDED 			= 11
class JobUnsuspendedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 11, "job unsuspended", header )
    def EventNum( self ) : return 11

# Job was held                ULOG_JOB_HELD 				= 12
class JobHeldEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 12, "job held", header )
    def EventNum( self ) : return 12

# Job was released            ULOG_JOB_RELEASED 			= 13
class JobReleasedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 13, "job released", header )
    def EventNum( self ) : return 13

# Parallel Node executed      ULOG_NODE_EXECUTE 			= 14
class NodeExecuteEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 14, header )
    def EventNum( self ) : return 14

# Parallel Node terminated    ULOG_NODE_TERMINATED 			= 15
class NodeTerminateEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 15, "node terminated", header )
    def EventNum( self ) : return 15

# POST script terminated      ULOG_POST_SCRIPT_TERMINATED	= 16
class PostScriptTerminateEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 16, "post script terminated", header )
    def EventNum( self ) : return 16

# Job Submitted to Globus     ULOG_GLOBUS_SUBMIT 			= 17
class GlobusSubmitEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 17, "globus submit", header )
    def EventNum( self ) : return 17

# Globus Submit failed        ULOG_GLOBUS_SUBMIT_FAILED 	= 18
class GlobusSubmitFailedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 18, "globus submit failed", header )
    def EventNum( self ) : return 18

# Globus Resource Up          ULOG_GLOBUS_RESOURCE_UP 		= 19
class GlobusResourceUpEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 19, "globus resource up", header )
    def EventNum( self ) : return 19

# Globus Resource Down        ULOG_GLOBUS_RESOURCE_DOWN 	= 20
class GlobusResourceDownEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 20, "globus resource down", header )
    def EventNum( self ) : return 20

# Remote Error                ULOG_REMOTE_ERROR             = 21
class RemoteErrorEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 21, "remote error", header )
    def EventNum( self ) : return 21

# RSC socket lost             ULOG_JOB_DISCONNECTED         = 22
class JobDisconnectedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 22, "job disconnected", header )
    def EventNum( self ) : return 22

# RSC socket re-established   ULOG_JOB_RECONNECTED          = 23
class JobReconnectedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 23, "job reconnected", header )
    def EventNum( self ) : return 23

# RSC reconnect failure       ULOG_JOB_RECONNECT_FAILED     = 24
class JobReconnectFailedEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 24, "job reconnect failed", header )
    def EventNum( self ) : return 24

# Grid Resource Up            ULOG_GRID_RESOURCE_UP         = 25
class GridResourceUpEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 25, "grid resource up", header )
    def EventNum( self ) : return 25

# Grid Resource Down          ULOG_GRID_RESOURCE_DOWN       = 26
class GridResourceDownEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 26, "grid resource down", header )
    def EventNum( self ) : return 26

# Job Submitted remotely      ULOG_GRID_SUBMIT 	    		= 27
class GridSubmitEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 27, "grid resource submit", header )
    def EventNum( self ) : return 27

# Report job ad information   ULOG_JOB_AD_INFORMATION		= 28
class JobAdInformationEvent( BaseEvent ) :
    def __init__( self, header = None ) :
        BaseEvent.__init__( self, 28, "job ad information", header )
    def EventNum( self ) : return 28

class EventParserException( Exception ) :
    pass

# Event log parser
class EventParser( object ) :

    class EventLookup( object ) :
        def __init__( self, event_class ) :
            self.__type = event_class
            try :
                self.__event = event_class( )
            except Exception, e :
                print >> sys.stderr, "Can't create", event_class, ":", e
        def HeaderMatch( self, header ) :
            return self.__event.HeaderNumCheck( header )
        def Type( self ) :
            return self.__type
        def Event( self, header ) :
            return self.__type( header )

    def __init__( self ) :
        self._verbose = 0
        self._quiet = False
        
        self.__lookup = (
            self.EventLookup( SubmitEvent ),
            self.EventLookup( ExecuteEvent ),
            self.EventLookup( ExecutableErrorEvent ),
            self.EventLookup( CheckpointEvent ),
            self.EventLookup( JobEvictedEvent ),
            self.EventLookup( JobTerminateEvent ),
            self.EventLookup( ImageSizeEvent ),
            self.EventLookup( ShadowExceptionEvent ),
            self.EventLookup( GenericEvent ),
            self.EventLookup( JobAbortedEvent ),
            self.EventLookup( JobSuspendedEvent ),
            self.EventLookup( JobUnsuspendedEvent ),
            self.EventLookup( JobHeldEvent ),
            self.EventLookup( JobReleasedEvent ),
            self.EventLookup( NodeExecuteEvent ),
            self.EventLookup( NodeTerminateEvent ),
            self.EventLookup( PostScriptTerminateEvent ),
            self.EventLookup( GlobusSubmitFailedEvent ),
            self.EventLookup( GlobusResourceUpEvent ),
            self.EventLookup( GlobusResourceDownEvent ),
            self.EventLookup( RemoteErrorEvent ),
            self.EventLookup( JobDisconnectedEvent ),
            self.EventLookup( JobReconnectedEvent ),
            self.EventLookup( JobReconnectFailedEvent ),
            self.EventLookup( GridResourceUpEvent ),
            self.EventLookup( GridResourceDownEvent ),
            self.EventLookup( GridSubmitEvent ),
            )

    def Verbose( self, v = None ) :
        if v is not None :
            self._verbose = v
        return self._verbose
    def Quiet( self, q = None ) :
        if q is not None :
            self._quiet = q
        return self._quiet


    def GenHeaderFromReMatch( self, line, re_match ) :
        event_num  = int(re_match.group(1))
        cluster    = int(re_match.group(2))
        proc       = int(re_match.group(3))
        subproc    = int(re_match.group(4))
        time_str   = re_match.group(5)
        text       = re_match.group(6)
        try:
            event_time = time.strptime( time_str, '%m/%d %H:%M:%S' )
        except Exception, e:
            print >>sys.stderr, \
                  "header line time parse error '"+time_str+"':", e
            return None
        return EventHeader( event_num, cluster, proc, subproc,
                            event_time, text )

    def ParseHeaderLine( self, line ) :
        m = _patterns.FindHeaderInLine( line, True )
        if m is None :
            return None
        return self.GenHeaderFromReMatch( line, m )

    def GetEventFromHeader( self, header ) :
        for lookup in self.__lookup :
            if lookup.HeaderMatch( header ) :
                return lookup.Event( header )
        return None


class EventLogParser( EventParser ) :

    def __init__( self, filename ) :
        self._filename = filename
        try:
            self._file = open( filename )
        except Exception, e:
            s = "Failed to open", filename, ":", e
            print >>sys.stderr, s
            raise Exception( s )

        self._line = None
        self._line_num = 0
        EventParser.__init__( self )

    def GetFileName( self ) :
        return self._filename
    def GetFile( self ) :
        return self._file
    def GetLine( self ) :
        return self._line
    def GetLineNum( self ) :
        return self._line_num

    def ReadEventBody( self, header, fobj ) :
        event = self.GetEventFromHeader( header )
        if event is None :
            s = "%s %d (type %d)" % ( self.GetFileName(),
                                      self.GetLineNum(),
                                      header.EventNum() )
            raise EventParserException( s )

        # Now, read it from the file
        for line in fobj :
            line = line.rstrip()
            self._line_num += 1
            if _patterns.LineHasHeader( line ) :
                s = "%s %d: unexpected header" % ( self._filename,
                                                   self._line_num )
                raise EventParserException( s )
            if event.ProcessEventLine( line ) :
                break
        return event

    def ParseLog( self, die ) :
        for l in self._file :
            self._line = l.rstrip()
            self._line_num += 1

            header = self.ParseHeaderLine( self._line )
            if header is None :
                s = "%s line %d: '%s': header line expected" % \
                    ( self._filename, self._line_num, self._line )
                self.HandleParseError( s )
                continue
            header.SetLineInfo( self._filename, self._line_num, self._line )

            event = None
            try:
                event = self.ReadEventBody( header, self._file )
            except EventParserException, e:
                s = "Invalid event: %s" % ( str(e) )
                self.HandleParseError( s )

            if event is not None :
                self.ProcessEvent( event )
                if self.Verbose() :
                    event.Print( self.Verbose() > 1 )

    def ProcessEvent( self, event ) :
        pass

    def HandleParseError( self, s ) :
        raise EventParserException( e )


### Local Variables: ***
### py-indent-offset:4 ***
### python-indent:4 ***
### python-continuation-offset:4 ***
### tab-width:4  ***
### End: ***
