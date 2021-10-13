.. _admin_install_windows:

Windows (as Administrator)
==========================

Installation of HTCondor must be done by a user with administrator
privileges.  We have provided quickstart instructions below to walk
you through a single-node HTCondor installation using the HTCondor
Windows installer GUI.

For more information about the installation options, or how to use
the installer in unattended batch mode, see the complete
:doc:`../admin-manual/windows-installer` guide.

It is possible to manually install HTCondor on Windows, without the
provided MSI program, but we strongly discourage this unless you have
a specific need for this approach and have extensive HTCondor experience.

Quickstart Installation Instructions
------------------------------------

To download the latest HTCondor Windows Installer:

#.  Go to the
    `current channel <https://research.cs.wisc.edu/htcondor/tarball/current/>`_
    download site.
#.  Click on the second-latest version.  (The latest version should always be
    the under-development version and will only have ``daily`` builds.)
#.  Click on the ``release`` folder.
#.  Click on the file ending in ``.msi`` (usually the first one).

Start the installer by double clicking on the MSI file once it's downloaded.
Then follow the directions below for each option.

If HTCondor is already installed.
    If HTCondor has been previously installed, a dialog box will appear
    before the installation of HTCondor proceeds. The question asks if
    you wish to preserve your current HTCondor configuration files.
    Answer yes or no, as appropriate.

    If you answer yes, your configuration files will not be changed, and
    you will proceed to the point where the new binaries will be
    installed.

    If you answer no, then there will be a second question that asks if
    you want to use answers given during the previous installation as
    default answers.

STEP 1: License Agreement.
    Agree to the HTCondor license agreement.

STEP 2: HTCondor Pool Configuration.
    Choose the option to create a new pool and enter a name.

STEP 3: This Machine's Roles.
    Check the "submit jobs" box.  From the list of execution options,
    choose "always run jobs".

STEP 4: The Account Domain.
    Skip this entry.

STEP 5: E-mail Settings.
    Specify the desired email address(es), if any.

STEP 6: Java Settings.
    If this entry is already set, accept it.  Otherwise, skip it.

STEP 7: Host Permission Settings.
    Enter ``127.0.0.1`` for all settings.

STEP 8: VM Universe Setting.
    Disable the **vm** universe.

STEP 9: Choose Destination Folder
    :index:`location of files<single: location of files; installation>`

    Accept the default settings.

This should complete the installation process. The installer will have
automatically started HTCondor in the background and you do **not** need to
restart Windows for HTCondor to work.

Open a command prompt to follow the next set of instructions.

.. include:: minicondor-test-and-quickstart.include

.. _admin_install_windows_pool:

Setting Up a Whole Pool with Windows
------------------------------------

Follow the instructions above through Step 1. Then, customize the
installation as follows:

STEP 2: HTCondor Pool Configuration.

    Create a new pool
    only on the machine you've chosen as their central manager.  See
    the :doc:`admin-quick-start`.  Otherwise, choose the option to
    join an existing pool and enter the name or IP address of the
    central manager.

STEP 3: This Machine's Roles.

    Check the "submit jobs"
    box to select the submit role, or choose "always run jobs" to select
    the execute role.

STEP 4: The Account Domain.
    Enter the
    same name on all submit-role machines.  This helps ensure that a
    user can't get more resources by logging in to more than one machine.

STEP 5: E-mail Settings.
    Specify the desired email address(es), if any.

STEP 6: Java Settings.
    If this entry is already set, accept it.  Otherwise, skip it.

    Experienced users who know they want to use the **java** universe
    should instead enter the path to the Java executable on the machine,
    if it isn't already set, or they want to use a different one.

    To disable use of the **java** universe, leave the field blank.

STEP 7: Host Permission Settings.
    Leave all three
    entries blank and configure security as appropriate for the
    machine's role by editing HTCondor configuration files; see the
    ``get_htcondor`` :doc:`man page <../man-pages/get_htcondor>` for
    details.

STEP 8: VM Universe Setting.
    Disable the **vm** universe.

    Experienced users with VMWare and Perl already installed may enable
    the **vm** universe.

STEP 9: Choose Destination Folder
    :index:`location of files<single: location of files; installation>`

    Experienced users may change the default installation path
    (``c:\Condor``), but we don't recommend doing so.  The default path
    is assumed in a number of script and configuration paths, so you should
    expect problems if you do so.
