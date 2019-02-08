      

The HTCondorView Client Contrib Module
======================================

The HTCondorView Client contrib module is used to automatically generate
World Wide Web pages to display usage statistics of an HTCondor pool.
Included in the module is a shell script which invokes the
*condor\_stats* command to retrieve pool usage statistics from the
HTCondorView server, and generate HTML pages from the results. Also
included is a Java applet, which graphically visualizes HTCondor usage
information. Users can interact with the applet to customize the
visualization and to zoom in to a specific time frame.
Figure \ `10.1 <#x81-5910031>`__ on page \ `1657 <#x81-5910031>`__ is a
screen shot of a web page created by HTCondorView.

--------------

| |PIC|

Figure 10.1: Screen shot of HTCondorView Client

--------------

After unpacking and installing the HTCondorView Client, a script named
*make\_stats* can be invoked to create HTML pages displaying HTCondor
usage for the past hour, day, week, or month. By using the Unix *cron*
facility to periodically execute *make\_stats*, HTCondor pool usage
statistics can be kept up to date automatically. This simple model
allows the HTCondorView Client to be easily installed; no Web server CGI
interface is needed.

Step-by-Step Installation of the HTCondorView Client
----------------------------------------------------

#. Make certain that the HTCondorView Server is configured. Section
    `3.14.7 <SettingUpforSpecialEnvironments.html#x42-3600003.14.7>`__
   describes configuration of the server. The server logs information on
   disk in order to provide a persistent, historical database of pool
   statistics. The HTCondorView Client makes queries over the network to
   this database. The *condor\_collector* includes this database
   support. To activate the persistent database logging, add the
   following entries to the configuration file for the
   *condor\_collector* chosen to act as the ViewServer.

   ::

           POOL_HISTORY_DIR = /full/path/to/directory/to/store/historical/data 
           KEEP_POOL_HISTORY = True

#. Create a directory where HTCondorView is to place the HTML files.
   This directory should be one published by a web server, so that HTML
   files which exist in this directory can be accessed using a web
   browser. This directory is referred to as the ``VIEWDIR`` directory.
#. Download the *view\_client* contrib module. Follow links for contrib
   modules from the wiki at
   `https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki>`__.
#. Unpack or untar this contrib module into the directory ``VIEWDIR``.
   This creates several files and subdirectories. Further unpack the jar
   file within the ``VIEWDIR`` directory with:

   ::

         jar -xf condorview.jar

#. Edit the *make\_stats* script. At the beginning of the file are six
   parameters to customize. The parameters are

    ``ORGNAME``
       A brief name that identifies an organization. An example is “Univ
       of Wisconsin”. Do not use any slashes in the name or other
       special regular-expression characters. Avoid the characters \\ |ˆ
       |\ and $.
    ``CONDORADMIN``
       The e-mail address of the HTCondor administrator at your site.
       This e-mail address will appear at the bottom of the web pages.
    ``VIEWDIR``
       The full path name (not a relative path) to the ``VIEWDIR``
       directory set by installation step 2. It is the directory that
       contains the *make\_stats* script.
    ``STATSDIR``
       The full path name of the directory which contains the
       *condor\_stats* binary. The *condor\_stats* program is included
       in the ``<release_dir>/bin`` directory. The value for
       ``STATSDIR`` is added to the ``PATH`` parameter by default.
    ``PATH``
       A list of subdirectories, separated by colons, where the
       *make\_stats* script can find the *awk*, *bc*, *sed*, *date*, and
       *condor\_stats* programs. If *perl* is installed, the path should
       also include the directory where *perl* is installed. The
       following default works on most systems:

       ::

                   PATH=/bin:/usr/bin:$STATSDIR:/usr/local/bin 
                   

#. To create all of the initial HTML files, run

   ::

               ./make_stats setup

   Open the file ``index.html`` to verify that things look good.

#. Add the *make\_stats* program to *cron*. Running *make\_stats* in
   step 6 created a ``cronentries`` file. This ``cronentries`` file is
   ready to be processed by the Unix *crontab* command. The *crontab*
   manual page contains details about the *crontab* command and the
   *cron* daemon. Look at the ``cronentries`` file; by default, it will
   run *make\_stats* *hour* every 15 minutes, *make\_stats* *day* once
   an hour, *make\_stats* *week* twice per day, and *make\_stats*
   *month* once per day. These are reasonable defaults. Add these
   commands to cron on any system that can access the ``VIEWDIR`` and
   ``STATSDIR`` directories, even on a system that does not have
   HTCondor installed. The commands do not need to run as root user; in
   fact, they should probably not run as root. These commands can run as
   any user that has read/write access to the ``VIEWDIR`` directory. The
   command

   ::

         crontab cronentries

   can set the crontab file; note that this command overwrites the
   current, existing crontab file with the entries from the file
   ``cronentries``.

#. Point the web browser at the ``VIEWDIR`` directory to complete the
   installation.

      

.. |PIC| image:: ref4x.png
.. |ˆ | image:: ref5x.png
