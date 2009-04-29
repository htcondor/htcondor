#! /bin/csh -f
set file = job_dagman_node_prio.order
if (-e $file) then
  rm $file
endif
touch $file
echo -n "$1 " >> $file
