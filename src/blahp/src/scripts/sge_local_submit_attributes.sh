#!/bin/bash

# $Id: sge_local_submit_attributes.sh,v 1.1 2011/07/20 06:15:34 rrosende Exp $

# The memory and wall time requirements as passed from the job
# description file (JDL) need to be passed on to the local batch
# system. This script, when installed in /opt/glite/bin, will be
# called by BLAH moments before the job is submitted to the Torque
# batch system.

# The requirements are passed according the tho GLUE schema, and for
# the purposes of this script involve the properties
# MainMemoryRAMSize, and MaxWallClockTime.

# see the GlUE Schema v1.3, http://glueschema.forge.cnaf.infn.it/Spec/V13

# See http://grid.pd.infn.it/cream/field.php?n=Main.ForwardOfRequirementsToTheBatchSystem

# The requirements will have been translated from their original Glue
# form into shell variables; upper and lower bounds are indicated by
# appending _Max or _Min, respectively, to the variable name.

# Some care must be taken to pass only sensible values, i.e. numbers,
# lest we leave opportunity to befoul the submit script with stray
# parameters. 

# The assumption here is, and it will have to be checked, that all
# values are specified as integer decimal numbers.

# For a treatise on parameter passing to Torque, see
# http://www.clusterresources.com/torquedocs21/2.1jobsubmission.shtml#nodeExamples.

# Part 1: memory requirements
# The parameter to watch for is called MainMemoryRamSize. 
# this is specified in megabytes according to Glue.

memmin="$GlueHostMainMemoryRAMSize_Min"

# If the user was so careless to ask for an exact match instead of
# a minimum, the variable name is different.
if [ -z "$memmin" ]; then
    memmin="$GlueHostMainMemoryRAMSize"
fi

# At this point $memmin is either empty, if no requirement was given,
# or some string. It must be a number but let's make sure by using
# the test below.
if [ $memmin -gt 0 ]; then
    echo "#$ -l s_vmem=${memmin}"
fi

# Part 2: Wall clock time requirement.
#
# While the memory specification according to the Glue schema is a
# property of a Compute Element, the maximum (wall clock) time a job
# can spend is set by policy. A curious aspect of the Glue schema is
# the subtle difference between the meaning of MaxWallClockTime and
# MaxObtainableWallClockTime. 
# 
#+--------------------------+-----------------------------+
#|property                  |description                  |
#+--------------------------+-----------------------------+
#|MaxWallClockTime          |The default maximum wall     |
#|                          |clock time allowed to each   |
#|                          |job by the batch system if no|
#|                          |limit is requested. Once this|
#|                          |time has expired the job will|
#|                          |most likely be killed or     |
#|                          |removed from the queue       |
#+--------------------------+-----------------------------+
#|MaxObtainableWallClockTime|The maximum obtainable wall  |
#|                          |clock time that can be       |
#|                          |granted to the job upon user |
#|                          |request                      |
#+--------------------------+-----------------------------+
#

# In the new Glue Schema (2.0) the DefaultWallTime and MaxWallTime
# properties have a like relationship.

# If for some strange reason the JDL file mentions requirements on both
# attributes, let's be prudent and take the maximum of the two.

# We take the same approach as before; testing for an exact match as well
# as a lower bound.
maxwall=0
#GlueCEPolicyMaxWallClockTime
#if [ -n "$GlueCEMaxWallClockTime_Min" ]; then
if [ -n "$GlueCEPolicyMaxWallClockTime_Min" ]; then
    maxwall="$GlueCEPolicyMaxWallClockTime_Min"
elif [ -n "$GlueCEPolicyMaxWallClockTime" ] ; then
    maxwall="$GlueCEPolicyMaxWallClockTime"
fi

maxobtain=0
if [ -n "$GlueCEPolicyMaxWallClockTime_Min" ]; then
    maxobtain="$GlueCEPolicyMaxWallClockTime_Min"
elif [ -n "$GlueCEPolicyMaxWallClockTime" ]; then
    maxobtain="$GlueCEPolicyMaxWallClockTime"
fi

# Select the larger of the two values; the use of -gt uses
# integer parsing so it effectively enforces input sanitation.
if [ $maxobtain -gt $maxwall ]; then
    walltime=$maxobtain
else
    walltime=$maxwall
fi

if [ $walltime -gt 0 ]; then
# The time unit, according to the Glue schema, is one minute
# but the Torque parameter is in seconds
    wallsec=$(($walltime*60))
    echo "#$ -l s_rt=${wallsec}"
fi

# The CPU time gets mostly the same treatment as the wall time.

maxcpu=0
#GlueCEPolicyMaxCPUTime
if [ -n "$GlueCEPolicyMaxCPUTime_Min" ]; then
    maxcpu="$GlueCEPolicyMaxCPUTime_Min"
elif [ -n "$GlueCEPolicyMaxCPUTime" ] ; then
    maxcpu="$GlueCEPolicyMaxCPUTime"
fi

maxobtaincpu=0
if [ -n "$GlueCEPolicyMaxObtainableCPUTime_Min" ]; then
    maxobtaincpu="$GlueCEPolicyMaxObtainableCPUTime_Min"
elif [ -n "$GlueCEPolicyMaxObtainableCPUTime" ]; then
    maxobtaincpu="$GlueCEPolicyMaxObtainableCPUTime"
fi

# Select the larger of the two values; the use of -gt uses
# integer parsing so it effectively enforces input sanitation.
if [ $maxobtaincpu -gt $maxcpu ]; then
    cputime=$maxobtaincpu
else
    cputime=$maxcpu
fi
if [ $cputime -gt 0 ]; then
# The time unit, according to the Glue schema, is one minute
# but the Torque parameter is in seconds
    cpusec=$(($cputime*60)) && echo "#$ -l s_cpu=${cpusec}"
fi

exit 0
