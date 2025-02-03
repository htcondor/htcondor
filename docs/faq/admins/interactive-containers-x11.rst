Recipe: Interactive Singularity Jobs and ``condor_ssh_to_job``
==============================================================

Starting with HTCondor 8.8, ``condor_ssh_to_job`` and hence also interactive jobs use an ``sshd`` running directly on the execute node with user privileges. Since 8.8.10, most issue have been ironed out, and
connecting into the job happens using ``condor_nsenter``, which is an ``nsenter`` -like tool to "enter" container namespaces in a generic way. This tool is spawned by the ``starter`` in parallel to ``sshd``.

There are a few remaining issues related to X11 forwarding which can be worked around, and which are partially dependent on the utilised setup. These are discussed on this page in addition to a short discussion of the handling of ``locale`` environment variables.

X11 forwarding
--------------
X11 forwarding in general works by running ``xauth`` as a child of the ``sshd`` process on the execute node. ``sshd`` mostly prunes the environment before, setting a new DISPLAY variable to a forwarded X11 port. It then runs ``xauth`` which by default uses the user's home directory to store the X11 authorization information.

Two issues arise:

* Since ``condor_nsenter`` does not run as a child of the ``sshd``, but as a child of the ``starter``, it can not pass on the ``DISPLAY`` environment variable to the user session.

* In many cases, when containers are used, the actual users may not have a home directory on the execute node, or might not have it mounted inside the container. However, we cannot override the location to store the ``.Xauthority`` file with the environment variable ``XAUTHORITY`` since ``sshd`` prunes that.

Another related issue is that the ``TERM`` environment variable is not inherited from the ``condor_ssh_to_job`` command, which may lead to strange behaviour of special key escape sequences (e.g. ``HOME`` or ``END`` keys).

Workaround for older OS releases (``SSHD`` <= 7.8)
--------------------------------------------------
To solve all these issues at once, we can make use of the fact that ``sshd`` is spawned and configured by HTCondor via the ``condor_ssh_to_job_sshd_config_template``. The location of this template can be set via the knob ``SSH_TO_JOB_SSHD_CONFIG_TEMPLATE``.
We can patch the file shipped with HTCondor and add the line:

.. code-block:: sh

   XAuthLocation /usr/local/bin/condor_xauth_wrapper

Subsequently, we can create the wrapper script (make sure it is executable) with the following content:

.. code-block:: sh

   #!/bin/bash
   
   # Walk up the process tree until we find the second sshd which rewrites cmdline to "sshd: user@tty".
   # The first sshd is our parent process which does not log itself.
   SSHD_PID=$$
   SSHD_CNT=0
   while true; do
     IFS= read -r -d '' CMDLINE </proc/${SSHD_PID}/cmdline || [[ $cmdline ]]
     #echo "Checking ID ${SSHD_PID}, cmdline ${CMDLINE^^}"
     SSHD_MATCHER="^SSHD: "
     if [[ ${CMDLINE^^} =~ ${SSHD_MATCHER} ]]; then
       # We found the sshd!
       SSHD_CNT=$(( SSHD_CNT + 1))
       if [ ${SSHD_CNT} -gt 1 ]; then
         break;
       fi
     fi
     SSHD_PID=$(ps -o ppid= -p ${SSHD_PID} | awk '{print $1}')
     if [ ${SSHD_PID} -eq 1 ]; then
       # We arrived at the INIT process, something very wrong... Let's stop and alert the user.
       echo "Error: Could not determine sshd process, X11 forwarding will not work!"
       echo "       Please let your admins know you got this error!"
       exit 0
     fi
   done
   #echo "SSHD PID is ${SSHD_PID}."
   
   # Find sshd.log, checking through fds.
   FOUND_SSHD_LOG=0
   for FD in $(ls -1 /proc/${SSHD_PID}/fd/); do
     FILE=$(readlink -f /proc/${SSHD_PID}/fd/$FD)
     #echo "Checking FD $FD, file is $FILE"
     SSHD_LOG_MATCHER="sshd\.log$"
     if [[ "${FILE}" =~ ${SSHD_LOG_MATCHER} ]]; then
       #echo "Found ${FILE}!"
       FOUND_SSHD_LOG=1
       SSH_TO_JOB_DIR=$(dirname ${FILE})
       JOB_WORKING_DIR=$(dirname ${SSH_TO_JOB_DIR})
       break;
     fi
   done
   
   if [ ${FOUND_SSHD_LOG} -eq 0 ]; then
     # We could not identify sshd.log, let's stop and alert the user.
     echo "Error: Could not determine sshd process' (PID: ${SSHD_PID}) log, X11 forwarding will not work!"
     echo "       Please let your admins know you got this error!"
     exit 0
   fi
   
   # Finally, if we arrive here, all is well.
   
   # This does NOT work, since env.sh is sourced as forced command, too early.
   #echo "export DISPLAY=${DISPLAY}" >> ${SSH_TO_JOB_DIR}/env.sh
   
   # Ugly hack needed with HTCondor 8.8.10 which does not yet pass through DISPLAY or TERM.
   echo "export DISPLAY=${DISPLAY}" > ${JOB_WORKING_DIR}/.display
   echo "export TERM=${TERM}" >> ${JOB_WORKING_DIR}/.display
   
   export XAUTHORITY=${JOB_WORKING_DIR}/.Xauthority
   /usr/bin/xauth "$@" </dev/stdin

Please note that this script is pretty verbose, and handles very unlikely errors not observed in practice (yet). Most of the code is just there to find out which directory is used as the execute directory for the job, then place the ``DISPLAY`` environment variable and the ``TERM`` environment variable inside a file ``.display`` in there, and finally adjust the environment variable ``XAUTHORITY`` to place the ``.Xauthority`` file there.

This script works combined with two environment hack inside the container:

* The container needs to use the execute directory as ``HOME`` directory. You can for example do that by setting ``SINGULARITY_HOME`` in the ``STARTER_JOB_ENVIRONMENT`` knob, which you may likely touch anyways to set ``SINGULARITY_NOHOME=true`` if you run without the users' home directories on the execute node.

* The container needs to source ``.display`` from the ``HOME`` directory on start (and may also clean up by deleting it).

Example code for the latter task could be, assuming your in-container ``HOME`` directory is ``/jwd``:

.. code-block:: sh

  if [ -r /jwd/.display ]; then
        source /jwd/.display
        rm -f /jwd/.display
  fi

A good place could be a file in ``/etc/profile`` inside the container(s) you use.

Workaround for more recent OS releases
--------------------------------------
Note that if your ``sshd`` is recent enough and understands ``SetEnv`` (should be the case starting from versions >7.8), you could additionally patch ``/usr/libexec/condor/condor_ssh_to_job_sshd_setup``, for example, you could add:

.. code-block:: sh

   echo "SetEnv JOB_WORKING_DIR=${base_dir}" >> ${sshd_config}

directly after the ``sshd_config`` is generated from the template.
You can then simplify the ``/usr/local/bin/condor_xauth_wrapper`` script to the much less error-prone code:

.. code-block:: sh

   #!/bin/bash
   
   # Ugly hack needed with HTCondor 8.8.10 which does not yet pass through DISPLAY or TERM.
   echo "export DISPLAY=${DISPLAY}" > ${JOB_WORKING_DIR}/.display
   echo "export TERM=${TERM}" >> ${JOB_WORKING_DIR}/.display
   
   export XAUTHORITY=${JOB_WORKING_DIR}/.Xauthority
   
   /usr/bin/xauth "$@" </dev/stdin

Note that we can not ``SetEnv`` the variable ``XAUTHORITY`` directly with ``SetEnv``, it is overwritten by ``sshd`` after setting the defined environment variables. Hence, the other steps described above (e.g. sourcing the ``DISPLAY`` variable in ``/etc/profile`` within the container) still apply.

Locale settings
---------------
You may note that the ``locale`` environment variables (``LANGUAGE``, ``LANG``,...) are not "adopted" in the interactive job or when using ``condor_ssh_to_job`` to attach to a running job, similar as is the case with ``DISPLAY`` and ``TERM``. This may cause issues such as UTF-8 not working if the default locale inside your job is not set, e.g. because you are using a container without a default locale, which usually means the ``C`` locale is assumed.

While forwarding such variables is convenient in regular ``ssh`` usage, it would change the behaviour of the payload of the interactive job versus a batch job. This is especially true e.g. for ``LC_NUMERIC`` which affects the way some libraries parse numbers.
To overcome this kind of issue, the best approach seems to be to set a default locale inside the used container, which is then consistently used both for batch and interactive jobs.
