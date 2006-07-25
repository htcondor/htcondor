
Some Simple Condor NT Example Programs

This directory contains several subdirectories.
Each subdirectory contains a simple program and a Condor
submit-description file.

To run these examples, copy this subdirectory to your 
machine's local hard disk into a subdirectory
where you have write access.

Example descriptions:

*** Subdirectory printname:
This demos Condor NT running a batch script.
printname.bat is a simple batch script simply writes the 
name of the machine to stdout by running the "net.exe" 
command which comes with Windows NT.
You can simply run "printname.bat" to see what the
program does before submitting it to Condor.
To submit it to Condor, run "condor_submit name.sub".
When the job completes, printname.out will contain
the stdout from the program, and printname.log will contain
information from Condor on exactly where/when Condor ran the
job.

*** Subdirectory rc5:
This subdirectory submits the distributed.net RC5 cracker
as a low priority (e.g. "nice-user") job.
This is the "Bovine" program -- see http://www.distributed.net/rc5/.
The example will submit one cluster with 10 Bovine jobs.
This example makes use of "transfer_files = ALWAYS" and
"transfer_input_files" parameters.  It also shows how to
run multiple instances of a program in different subdirectories.
Note: this program never exits.
See the comments in rc5.sub for more information.
To submit it to Condor, run "condor_submit rc5.sub".

*** Subdirectory cpusoak:
The program cpusoak.exe is a GUI program which simply 
goes into an infinite busy loop after a couple seconds.  
To submit, run "condor_submit cpusoak.sub".


