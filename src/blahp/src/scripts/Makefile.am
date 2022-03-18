# *****************
# * BLAHP scripts * 
# *****************
#
# $Id: Makefile.am,v 1.10 2012/01/13 11:23:44 mezzadri Exp $ 
#
# File:           Makefile.am
#
# Author(s):      Francesco Prelz ($Author: mezzadri $)
#        e-mail: "Francesco.Prelz@mi.infn.it"
#
# Revision history:
#
# 16-Apr-2004  Created 
#
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# See http://www.eu-egee.org/partners/ for details on the copyright
# holders.  
# 
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
# 
#     http://www.apache.org/licenses/LICENSE-2.0 
# 
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License.
#


libexec_SCRIPTS = blah_load_config.sh blah_common_submit_functions.sh \
  pbs_cancel.sh pbs_status.sh pbs_submit.sh pbs_hold.sh pbs_resume.sh \
  lsf_cancel.sh lsf_status.sh lsf_submit.sh lsf_hold.sh lsf_resume.sh \
  condor_cancel.sh condor_status.sh condor_submit.sh condor_hold.sh condor_resume.sh \
  sge_cancel.sh sge_helper sge_resume.sh sge_submit.sh sge_filestaging \
  sge_hold.sh sge_status.sh runcmd.pl.template sge_local_submit_attributes.sh \
  slurm_cancel.sh slurm_hold.sh slurm_resume.sh slurm_status.sh \
  slurm_submit.sh slurm_local_submit_attributes.sh \
  condor_local_submit_attributes.sh \
  pbs_local_submit_attributes.sh \
  lsf_local_submit_attributes.sh \
  kubernetes_cancel.sh kubernetes_status.sh kubernetes_submit.sh kubernetes_hold.sh kubernetes_resume.sh \
  kubernetes_local_submit_attributes.sh \
  nqs_cancel.sh nqs_status.sh nqs_submit.sh nqs_hold.sh nqs_resume.sh \
  nqs_local_submit_attributes.sh \
  blah.py __init__.py \
  pbs_status.py \
  slurm_status.py \
  lsf_status.py

EXTRA_DIST = $(bin_SCRIPTS)
