.. _google_cloud_marketplace:

Google Cloud Marketplace Entry
==============================

The Center for High-Throughput Computing maintains a Google Cloud Marketplace
entry for a HTCondor-in-the-Cloud.  This web-based tool automates the process
of starting a complete (Linux) HTCondor pool on the Google Cloud Platform.

You will need a Google Cloud Platform account and a GCP project in which to
place the newly-constructed pool.

Instructions
------------

#.  Log into the Gooogle Cloud Platform
#.  Go to the `Marketplace entry <https://console.cloud.google.com/marketplace/details/kbatch-public/htcondor-on-gcp>`_.
#.  Click the blue LAUNCH button.
#.  Select a project in which to place the new pool.
#.  You'll be taken to a new screen, where you should update the
    'administrator e-mail address field'.
#.  You may update any of the other fields, but the only ones we recommend
    changing are under the 'Condor Compute' section.  You should never need
    to change the values under 'Condor Master' section, and only but rarely
    the values under 'Condor Submit' (primarily to give yourself a larger
    disk).
#.  Click the blue DEPLOY button.
#.  You'll be taken to a new screen, where you should wait for a while as
    Google gets your machines started.  The text at the top of the middle
    column will change to '... has been deployed' when everything's ready
    to go.
#.  You may want to bookmark this page for future reference.
#.  Halfway down the right column, a new option should appear, labelled
    'Get started with HTCondor on GCP'.  Click on the 'SSH TO CONDOR SUBMIT
    NODE' link.  This will open a browser window that functions like an
    SSH client, and you can use the gear icon in the upper-right corner to
    upload and download files.

At this point, you can start using HTCondor as normal.  When you're done --
and have downloaded any files you want from the submit node -- you can click
the DELETE button at the top of center column to clean everything up (and
stop being charged).  Select the first option ("... and all resources...")
and click the DELETE ALL button.
