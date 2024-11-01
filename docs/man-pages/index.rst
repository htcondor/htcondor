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
         condor_q
         :::link:condor_q
         condor_qedit
         :::link:condor_qedit
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

.. toctree::
    :maxdepth: 1
    :glob:

    classads

Job Management Commands
-----------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_continue
    condor_history
    condor_hold
    condor_prio
    condor_q
    condor_qedit
    condor_qsub
    condor_release
    condor_rm
    condor_run
    condor_submit
    condor_submit_dag
    condor_suspend
    condor_transfer_data
    condor_watch_q

Job Interaction Commands
------------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_check_userlogs
    condor_chirp
    condor_evicted_files
    condor_gather_info
    condor_ssh_to_job
    condor_tail
    condor_userlog
    condor_vacate_job
    condor_wait

Access Point Commands
---------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_job_router_info
    condor_userprio
    condor_qusers
    condor_remote_cluster
    condor_router_history
    condor_router_q
    condor_router_rm

Execution Point Commands
------------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_drain
    condor_findhost
    condor_gpu_discovery
    condor_now
    condor_power
    condor_restart
    condor_status
    condor_update_machine_ad
    condor_updates_stats
    condor_vacate
    condor_who

Central Manager Commands
------------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_advertise
    condor_pool_job_report
    condor_reschedule

General Administrator Commands
------------------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_check_password
    condor_config_val
    condor_configure
    condor_fetchlog
    condor_install
    condor_master
    condor_off
    condor_on
    condor_ping
    condor_preen
    condor_procd
    condor_reconfig
    condor_restart
    condor_set_shutdown
    condor_sos
    condor_top
    condor_transform_ads
    condor_upgrade_check
    condor_urlfetch
    condor_version
    get_htcondor
    gidd_alloc
    procd_ctl

Credential Commands
-------------------

.. toctree::
    :maxdepth: 1
    :glob:

    condor_store_cred
    condor_test_token
    condor_token_create
    condor_token_fetch
    condor_token_list
    condor_token_request
    condor_token_request_approve
    condor_token_request_auto_approve
    condor_token_request_list

Other
-----

.. toctree::
    :maxdepth: 1
    :glob:

    classad_eval
    condor_adstash
    condor_annex
    condor_dagman
    condor_rmdir
    condor_ssh_start
    condor_ssl_fingerprint
    htcondor
