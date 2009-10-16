<?php
// Task class to mirror the Task database table 

require_once "./load_config.inc";
load_config();

class Task {
    
    # vars from the database
    var $task_id;
    var $runid;
    var $platform;
    var $taskname;
    var $exe;
    var $args;
    var $hostname;
    var $start;
    var $finish;
    var $result;
   
    # other vars  
    var $gid;
    var $duration;
    var $user;
    var $strtpath;
    var $filepath;
  
    ## functions ##
    # find the duration, if the run is complete
    function calculateDuration() { 
      if ( !( is_null($this->result)) ) { 
        $this->duration = strtotime( $this->finish ) - strtotime( $this->start );
        # split up duration into readable units  
        $hours = 0; $minutes = 0; $seconds = 0;
        $hours = floor($this->duration / 3600);
        $newruntime = $this->duration % 3600;
        $minutes = floor($newruntime / 60);
        $seconds = $this->duration % 60;

        # fix formatting
        if ($hours < 10)   { $hours = "0$hours"; }
        if ($minutes < 10) { $minutes = "0$minutes"; }
        if ($seconds < 10) { $seconds = "0$seconds"; }

        $this->duration = "$hours:$minutes:$seconds";
      } else {
        $this->duration = "In progress";
      }
    } 

    function convertDates() {
      $this->start = strtotime( $this->start );
      $this->start = $this->start + date("Z", $this->start);
      $this->start = date("m/d/y  H:i:s", $this->start);
      $this->finish = strtotime( $this->finish );
      $this->finish = $this->finish + date("Z", $this->finish);
      $this->finish = date("m/d/y  H:i:s", $this->finish);

      # if not finished, don't display a finish time 
      if ( is_null($this->result) ) {      
        $this->finish = "In progress";    
      }
    }

    # figure out naming for remote files
    # The platform_job task out & err files should *always* be "n/a", not just while they're running.
    # remote_task, if the user declares sub-tasks, becomes a "container" task for all of them, 
    #   representing their collective runtime & result, and should be n/a if this happens
    # The out/err links for all tasks whose hostname==grandcentral should be visible even while the tasks 
    #   are in progress. Currently, they're listed as "n/a" until they complete.
    function determineFilePath() {   
      # use .out file to determine path - .err file base path is indentical  
      $this->filepath = "";
      $basedir = "rundir";   
      $actualbasedir = RUN_DIR;

      if ( strstr($this->taskname, "platform_job") ){
        $this->filepath = "";
      } else if ( strstr($this->hostname, "nmi-s005") || strstr($this->hostname, "nmi-s006") ) {  
        if ( strstr($this->platform, "local") ) {
          #$this->filepath = "$this->gid/$this->taskname";
          $this->filepath = "$this->strtpath/$this->gid/$this->taskname";
        } else {
          $this->filepath = "$this->strtpath/$this->gid/$this->taskname.$this->platform";
        }
      } else if ( !(strstr($this->platform, "local")) ) {   
        $tempFilePath = "$this->strtpath/$this->gid/userdir/$this->platform/$this->taskname.out";
        if (file_exists($tempFilePath)) {
          $this->filepath = "$this->strtpath/$this->gid/userdir/$this->platform/$this->taskname";
        }
      } else if ( !(is_null($this->result)) ) {
        $this->filepath = "$this->strtpath/$this->gid/userdir/$this->platform/$this->taskname";
      }
    } 

    ## constructors ##
    #function __construct ( $dbGid, $dbTaskId, $dbRunid, $dbPlatform, $dbTaskname, $dbExe, $dbArgs, $dbHostname, $dbStart, $dbFinish, $dbResult, $dbFilePath ) {
      #if (func_num_args()==1) {
        #foreach ($dbGid as $k=>$v)
            #$this->$k = $dbGid[$k];
        #return;
      #}
    #}
 
    function Task ( $dbGid, $dbTaskId, $dbRunid, $dbPlatform, $dbTaskname, $dbExe, $dbArgs, $dbHostname, $dbStart, $dbFinish, $dbResult, $dbUser, $dbFilePath ) {
      $this->gid = $dbGid;
      $this->task_id = $dbTaskId;
      $this->runid = $dbRunid;
      $this->platform = $dbPlatform;
      $this->taskname = $dbTaskname;
      $this->exe = $dbExe;
      $this->args = $dbArgs;
      $this->hostname = $dbHostname;
      $this->start = $dbStart; 
      $this->finish = $dbFinish;
      $this->result = $dbResult;
      $this->user = $dbUser;
      $this->strtpath = $dbFilePath;

      $this->calculateDuration();
      $this->convertDates();
      $this->determineFilePath();
    }     
    
    ## getters ##
    function getTaskId() {
        return $this->task_id;
    }

    function getRunid() {
        return $this->runid;
    }

    function getPlatform() {
      return $this->platform;
    }

    function getTaskname() {
      return $this->taskname;
    }
   
    function getExe() {
      return $this->exe;
    }
    
    function getArgs() {
      return $this->args;
    }

    function getHostname() {
      return $this->hostname;
    } 

    function getStart() {
      return $this->start;
    } 

    function getFinish() {
      return $this->finish;
    } 

    function getResult() {
      return $this->result;
    } 

    function getGid() {
      return $this->gid;
    }

    function getDuration() {
      return $this->duration;
    } 

    function getFilePath() {
      return $this->filepath;
    }

    ## setters ##
    function setTaskId( $newTaskId ) {
      $this->task_id = $newTaskId; 
    }

    function setRunid( $newRunid ) {
      $this->runid = $newRunid;
    }

    function setPlatform( $newPlatform ) {
      $this->platform = $newPlatform;
    }

    function setTaskname( $newTaskname ) {
      $this->taskname = $newTaskname;
    }

    function setExe( $newExe ) {
      $this->exe = $newExe;
    }   
 
    function setArgs( $newArgs ) {
      $this->args = $newArgs;
    }

    function setHostname( $newHostname ) {
      $this->hostname = $newHostname;
    }

   function setStart( $newStart ) {
      $this->start = $newStart;
    }

    function setFinish( $newFinish ) {
      $this->finish = $newFinish;
    }   

    function setResult( $newResult ) {
      $this->result = $newResult;
    }   

   function setGid( $newGid ) {
      $this->gid = $newGid;
    }

   function setDuration( $newDuration ) {
      $this->duration = $newDuration;
    }

   function setFilepath( $newFilepath ) {
      $this->filepath = $newFilepath;
    }

}

?>

