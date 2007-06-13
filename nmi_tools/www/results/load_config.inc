<?php
/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

define("NMI_CONFIG_PATH", "/nmi/etc/nmi.conf");

   // ---------------------------------------------------------
   // load_config()
   //
   // Takes in a configuration file containing key-value pairs
   // and will create constants for those variables. Each line
   // in the configuration file is assumed to contain a variable
   // name and a value separated by whitespace or an equals sign ('=') 
   // All variable names must be composed of alpha-numeric characters
   // They will also be converted to all uppercase letters. Also note
   // that any values encapsulated in quotation marks will have those
   // quotation marks included in the value (they are not removed by
   // the parser here).
   //
   // Example:
   //    Config File:
   //      DB_NAME = database_name
   //      DB_HOST      database.cs.wisc.edu
   //      db_type=mysql
   //      DB_USER = "user"
   //
   //    Will produce the following constant values
   //      DB_NAME => database_name
   //      DB_HOST => database.cs.wisc.edu
   //      DB_TYPE => mysql
   //      DB_USER => "user"
   //
   // @param $filePath - path to the config file to parse
   // @return true/false - true if the file exists and could be read
   // --------------------------------------------------------- 

   function load_config($filePath = NMI_CONFIG_PATH) {
      //
      // First make sure the file exists
      //
      if (!file_exists($filePath)) {
         $msg = "Config file '{$filePath}' does not exist";
         echo "ERROR: {$msg}\n";
         // This will be used when we switch into the new NMI web framework
         // setError($msg);
         return (false);
      //
      // If it does exist, make sure we can read it
      //
      } elseif (!is_readable($filePath)) {
         $msg = "Config file '{$filePath}' is not readable";
         echo "ERROR: {$msg}\n";
         // This will be used when we switch into the new NMI web framework
         // setError($msg);
         return (false);
      }
      
      //
      // Ok so let's do some simple parsing!
      //
      $fileDump = file_get_contents($filePath, false);
      $config = Array();
      if (preg_match_all("/^[\s]*(\S+)[\s]*=(.*)\n/m", $fileDump, $matches, PREG_SET_ORDER)) {
         //
         // Create defines for all the matches that we find
         //
         for ($ctr = 0, $cnt = count($matches); $ctr < $cnt; $ctr++) {
            $key = trim($matches[$ctr][1]);
            $val = trim($matches[$ctr][2]);
            $config[$key] = $val;
         } // FOR
      }
      
      //
      // This is broken out in case somebody wants to come back and
      // change what we do with our values. For now I'll just make a nice constant 
      // for them. It should be noted that I'm following the convention that
      // all constant values will need to be completely upper-case
      //
      foreach ($config AS $key => $val) {
         if (!defined($key)) {
            $key = strtoupper($key);
            define($key, $val);
            // echo "$key => $val\n";
         }
      } // FOR
      return (true);
   }
?>

