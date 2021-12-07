# ****************
# * BLAHP daemon * 
# ****************
#
# $Id: Makefile.am,v 1.57.2.3 2012/03/20 13:38:43 mezzadri Exp $ 
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
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#

if HAVE_GLOBUS
GLOBUS_CFLAGS = -DHAVE_GLOBUS
           $(GLOBUS_GSI_CRED_CFLAGS) \
           $(GLOBUS_GSI_PROXY_CFLAGS) \
           $(GLOBUS_GSI_UTILS_CFLAGS) \
           $(GLOBUS_GSS_ASSIST_CFLAGS) \
           $(GLOBUS_GSI_SYSCFG_CFLAGS) 
GLOBUS_EXECS = BPRclient BPRserver
else
GLOBUB_CFLAGS =
GLOBUS_EXECS =
endif

INCLUDES = -I. $(CLASSAD_CFLAGS) \
           $(GLOBUS_CFLAGS)

SUBDIRS = scripts

sbin_PROGRAMS = blahpd_daemon blah_job_registry_add blah_job_registry_lkup blah_job_registry_scan_by_subject blah_check_config blah_job_registry_dump blah_job_registry_purge
bin_PROGRAMS = blahpd
libexec_PROGRAMS = BLClient BLParserLSF BLParserPBS BUpdaterCondor BNotifier BUpdaterLSF BUpdaterPBS BUpdaterSGE $(GLOBUS_EXECS)  blparser_master
noinst_PROGRAMS = test_job_registry_create test_job_registry_purge test_job_registry_update test_job_registry_access test_job_registry_update_from_network test_cmdbuffer

common_sources = console.c job_status.c resbuffer.c server.c commands.c classad_binary_op_unwind.C classad_c_helper.C proxy_hashcontainer.c config.c job_registry.c blah_utils.c env_helper.c mapped_exec.c md5.c cmdbuffer.c

blahpd_SOURCES = main.c $(common_sources)

blahpd_daemon_SOURCES = main_daemon.c $(common_sources)

blahpd_LDADD = $(CLASSAD_LIBS) $(GLOBUS_GSSSAPI_GSI_LIBS) $(GLOBUS_GSS_ASSIST_LIBS)

blahpd_daemon_LDADD = $(blahpd_LDADD)

BPRclient_SOURCES = BPRclient.c BPRcomm.c tokens.c

BPRclient_LDADD = \
		$(GLOBUS_GSI_CRED_LIBS) \
		$(GLOBUS_GSI_PROXY_LIBS) \
		$(GLOBUS_GSI_UTILS_LIBS) \
		$(GLOBUS_GSS_ASSIST_LIBS) \
		$(GLOBUS_GSI_SYSCFG_LIBS)

BPRserver_SOURCES = BPRserver.c BPRcomm.c tokens.c

BPRserver_LDFLAGS = -static

BPRserver_LDADD = \
		$(GLOBUS_GSI_CRED_LIBS) \
		$(GLOBUS_GSI_PROXY_LIBS) \
		$(GLOBUS_GSI_UTILS_LIBS) \
		$(GLOBUS_GSS_ASSIST_LIBS) \
		$(GLOBUS_GSI_SYSCFG_LIBS)

BLClient_SOURCES = BLClient.c blah_utils.c BLfunctions.c

BLClient_LDADD =

BLParserLSF_SOURCES = BLParserLSF.c blah_utils.c BLfunctions.c

BLParserLSF_LDADD = 

BLParserPBS_SOURCES = BLParserPBS.c blah_utils.c BLfunctions.c

BLParserPBS_LDADD =

blah_job_registry_add_SOURCES = blah_job_registry_add.c job_registry.c job_registry_updater.c md5.c config.c
blah_job_registry_add_CFLAGS = $(AM_CFLAGS)

blah_job_registry_lkup_SOURCES = blah_job_registry_lkup.c job_registry.c md5.c config.c
blah_job_registry_lkup_CFLAGS = $(AM_CFLAGS)

blah_job_registry_scan_by_subject_SOURCES = blah_job_registry_scan_by_subject.c classad_c_helper.C classad_binary_op_unwind.C job_registry.c md5.c config.c
blah_job_registry_scan_by_subject_LDADD = $(CLASSAD_LIBS)
blah_job_registry_scan_by_subject_CFLAGS = $(AM_CFLAGS)

blah_job_registry_purge_SOURCES = blah_job_registry_purge.c job_registry.c md5.c
blah_job_registry_purge_CFLAGS = $(AM_CFLAGS)

test_job_registry_purge_SOURCES = test_job_registry_purge.c job_registry.c md5.c
test_job_registry_purge_CFLAGS = $(AM_CFLAGS)

test_job_registry_create_SOURCES = test_job_registry_create.c job_registry.c md5.c
test_job_registry_create_CFLAGS = $(AM_CFLAGS)

test_job_registry_update_SOURCES = test_job_registry_update.c job_registry.c md5.c
test_job_registry_update_CFLAGS = $(AM_CFLAGS)

test_job_registry_access_SOURCES = test_job_registry_access.c job_registry.c md5.c
test_job_registry_access_CFLAGS = $(AM_CFLAGS)

test_job_registry_update_from_network_SOURCES = test_job_registry_update_from_network.c job_registry.c job_registry_updater.c md5.c config.c
test_job_registry_update_from_network_CFLAGS = $(AM_CFLAGS)

BUpdaterCondor_SOURCES = BUpdaterCondor.c Bfunctions.c job_registry.c md5.c config.c blah_utils.c job_registry_updater.c
BUpdaterCondor_LDADD = -lpthread

BNotifier_SOURCES = BNotifier.c Bfunctions.c job_registry.c md5.c config.c blah_utils.c
BNotifier_LDADD = -lpthread

BUpdaterLSF_SOURCES = BUpdaterLSF.c Bfunctions.c job_registry.c md5.c config.c blah_utils.c job_registry_updater.c
BUpdaterLSF_LDADD = -lpthread -lm

BUpdaterPBS_SOURCES = BUpdaterPBS.c Bfunctions.c job_registry.c md5.c config.c blah_utils.c job_registry_updater.c
BUpdaterPBS_LDADD = -lpthread -lm

BUpdaterSGE_SOURCES = BUpdaterSGE.c Bfunctions.c job_registry.c md5.c config.c blah_utils.c
BUpdaterSGE_LDADD = -lpthread

blparser_master_SOURCES = blparser_master.c config.c blah_utils.c
blparser_master_LDADD = 

blah_check_config_SOURCES = blah_check_config.c Bfunctions.c config.c blah_utils.c
blah_check_config_LDADD =

blah_job_registry_dump_SOURCES = blah_job_registry_dump.c job_registry.c md5.c config.c
blah_job_registry_dump_CFLAGS = $(AM_CFLAGS)

test_cmdbuffer_SOURCES = cmdbuffer.c
test_cmdbuffer_CFLAGS = $(AM_CFLAGS) -DCMDBUF_DEBUG

noinst_HEADERS = blahpd.h classad_binary_op_unwind.h classad_c_helper.h commands.h job_status.h resbuffer.h server.h console.h BPRcomm.h tokens.h BLParserPBS.h BLParserLSF.h proxy_hashcontainer.h job_registry.h md5.h config.h BUpdaterCondor.h Bfunctions.h BNotifier.h BUpdaterLSF.h BUpdaterPBS.h BUpdaterSGE.h blah_utils.h env_helper.h mapped_exec.h blah_check_config.h BLfunctions.h cmdbuffer.h job_registry_updater.h

