Commands Reference (man pages)
==============================

HTCondor ships with many command line tools.  While the number may seem overwhelming at first, they can be divided into a few groups:

.. mermaid::
   :caption: A map of all the tools
   :align: center

   mindmap
     All<br/>Commands
       ManagingJobs
         condor_rm
         :::link:condor_rm
         condor_submit
         :::link:condor_submit
         condor_submit_dag
         :::link:condor_submit_dag
         condor_suspend
         :::link:condor_suspend
         condor_continue
         :::link:condor_continue
         condor_hold
         :::link:condor_hold
         condor_release
         :::link:condor_release
         condor_transfer_data
         :::link:condor_transfer_data
         condor_q condor_qedit
         :::link:condor_q
         condor_history
         :::link:condor_history
       ManagingExecution Points
           condor_off
           :::link:condor_off
           condor_on
           :::link:condor_on
           condor_restart
           :::link:condor_restart
           condor_drain
           :::link:condor_drain
           condor_now
           :::link:condor_now
           condor_vacate
           :::link:condor_vacate
           condor_config_val
           :::link:condor_config_val
           condor_reconfig
           :::link:condor_reconfig
           condor_status
           :::link:condor_status
       ManagingRunning Jobs
           condor_ssh_to_job
           :::link:condor_ssh_to_job
           condor_tail
           :::link:condor_tail
           condor_evicted_files
           :::link:condor_evicted_files
           condor_chirp
           :::link:condor_chirp
           condor_vacate_job
           :::link:condor_vacate_job
       Debugging Testing
           classad_eval
           :::link:classad_eval
           condor_version
           :::link:condor_version
           condor_who
           :::link:condor_who
           condor_top
           :::link:condor_top
           condor_fetchlog
           :::link:condor_fetchlog
           condor_transform_ads
           :::link:condor_transform_ads
           condor_gpu_discovery
           :::link:condor_gpu_discovery
           condor_power_state
       Managing Submitters
           condor_userprio
           :::link:condor_userprio
           condor_qusers
           :::link:condor_qusers

Commands that manage jobs:

    *condor_rm*, *condor_submit*, *condor_submit_dag*, *condor_suspend*, *condor_continue*, *condor_hold*, *condor_release*, *condor_transfer_data*, *condor_q*
    *condor_qedit*, *condor_history*

Commands for managing execution points:

    *condor_off*, *condor_on*, *condor_restart*, *condor_drain*, *condor_now*, *condor_vacate*, *condor_config_val*, *condor_reconfig*, *condor_status*

Commands for working with running jobs:

    *condor_ssh_to_job*, *condor_tail*, *condor_evicted_files*, *condor_chirp*, *condor_vacate_job*

Commands for debugging and testing:

    *classad_eval*, *condor_version*, *condor_who*, *condor_top*, *condor_fetchlog*, *condor_transform_ads*, *condor_gpu_discovery*, *condor_power_state*

Commands for managing submitters:

    *condor_userprio*, *condor_qusers*



.. toctree::
   :maxdepth: 1
   :glob:

   classads
   classad_eval
   condor_adstash
   condor_advertise
   condor_annex
   condor_check_password
   condor_check_userlogs
   condor_chirp
   condor_configure
   condor_config_val
   condor_continue
   condor_dagman
   condor_drain
   condor_evicted_files
   condor_fetchlog
   condor_findhost
   condor_gather_info
   condor_gpu_discovery
   condor_history
   condor_hold
   condor_install
   condor_job_router_info
   condor_master
   condor_now
   condor_off
   condor_on
   condor_ping
   condor_pool_job_report
   condor_power
   condor_preen
   condor_prio
   condor_procd
   condor_q
   condor_qedit
   condor_qusers
   condor_qsub
   condor_reconfig
   condor_release
   condor_remote_cluster
   condor_reschedule
   condor_restart
   condor_rm
   condor_rmdir
   condor_router_history
   condor_router_q
   condor_router_rm
   condor_run
   condor_set_shutdown
   condor_sos
   condor_ssh_start
   condor_ssh_to_job
   condor_ssl_fingerprint
   condor_stats
   condor_status
   condor_store_cred
   condor_submit
   condor_submit_dag
   condor_suspend
   condor_tail
   condor_test_token
   condor_token_create
   condor_token_fetch
   condor_token_list
   condor_token_request
   condor_token_request_approve
   condor_token_request_auto_approve
   condor_token_request_list
   condor_top
   condor_transfer_data
   condor_transform_ads
   condor_update_machine_ad
   condor_updates_stats
   condor_upgrade_check
   condor_urlfetch
   condor_userlog
   condor_userprio
   condor_vacate
   condor_vacate_job
   condor_version
   condor_wait
   condor_watch_q
   condor_who
   get_htcondor
   gidd_alloc
   htcondor
   procd_ctl
