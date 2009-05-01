#! /bin/csh -f
set file = job_dagman_depth_first.order
if (-e $file) then
  rm $file
endif
touch $file
