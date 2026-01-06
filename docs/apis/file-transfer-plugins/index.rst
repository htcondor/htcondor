File Transfer Plug-Ins
======================

See :ref:`the admin manual <admin-manual/file-and-cred-transfer:Enabling the Transfer of Files Specified by a URL>`
for an overview of file transfer plug-ins; this part of the manual details
the interface between HTCondor and a file transfer plug-in.

There are three versions of this interface.  The first version, retroactively
protocol version 1, invoked the plug-in once for each file being transferred;
this was simple and easy, but not very efficient.  In the second version --
introducing protocol version 2 -- "multi-file" plug-ins could transfer more
than one file per invocation.  Protocol version 4 extends version 2 with
additional information in the plug-in's input, allowing for more precise
control (e.g., over which caches a particular transfer method uses).  It
also explicitly requires some behaviors intended to minimize the need for
future protocol version changes.

Protocol Version 1
------------------

Although protocol version 1 is still supported, new plug-ins should not
be written to use it; the `example <https://github.com/htcondor/htcondor/blob/master/src/condor_examples/filetransfer_example_plugin.py>`_
plug-in supports version 2, so the additional complexity should not be
a burden.

Protocol Version 2
------------------

Presently, a file transfer plug-in may be invoked at six different times:

1.  When the EP initially starts up, to determine which protocols the EP
    should advertise as being supported.
2.  When the EP initially starts up, to test the plug-in, if that
    is enabled.  (See :macro:`<PLUGIN>_TEST_URL`.)
3.  Just before file transfer begins for a job.  This is a duplicate
    of item #1.
4.  Just after item #3.  This is a duplicate of item #2.
5.  Once when doing input transfer, for all supported protocols.
6.  At least once for output tansfer, no more than once per supported protocol.

We call the interface for instances 1 and 3 the "query" interface, and
the interface for instances 2, 4, 5, and 6 the "transfer" interface.

Plug-ins specified by :subcom:`transfer_plugins` are not (can not be)
run by instances 1 or 2.

The Query Interface (v2)
''''''''''''''''''''''''

Instead of requiring the tedious (and static) specification of everything
HTCondor needs to know about a file transfer plug-in in configuration,
HTCondor invokes the plug-in with the ``-classad`` command-line argument and
parses the ClassAd written to standard out.  Presently, the ClassAd must be
serialized in the "long" (line-oriented) format.

The ClassAd must contain four attributes, and should contain five.  Extra
attributes will be ignored, but preserved in the epoch log.  Those
attributes are:

* ``MultipleFileSupport`` which must be the boolean ``True``;
* ``PluginVersion``, an arbitrary string potentially useful for debugging;
* ``PluginType``, which must be the string value ``FileTransfer``;
* ``SupportedMethods``, a string containing a comma-separated list of the
  method(s) (protocol(s), scheme(s)) the plug-in supports;
* and ``PluginVersion``, an integer which must be the integer ``2``.

This can be optionally followed by 2 or more attributes that give the
plugin the ability to put arbitrary information, such as the plugin
version, into the EP's slot ads.  One attribute must be ``StartdAttrs``,
a string containing a comma-separated list of one or more attribute names;
the others must be the named attributes.

For example, the "curl" plug-in might produce the following output:

.. code-block:: condor-classad

    MultipleFileSupport = true
    PluginVersion = "0.2"
    PluginType = "FileTransfer"
    ProtocolVersion = 2
    SupportedMethods = "http,https,ftp,file,dav,davs"
    StartdAttrs = "CurlPluginInfo"
    CurlPluginInfo = [ CurlVersion = "7.15.1"; ]

The Transfer Interface (v2)
'''''''''''''''''''''''''''

If a job specifies a URL transfer, the plug-in is invoked without the
command line argument ``-classad``.  It will instead be given two other
command line arguments, ``-infile <input-filename>`` and
``-outfile <output-filename>``.  The former specifies a path to file on
disk which contains a series of input ads; the latter specifies the file
to which the plug-in should write its output ads.  Plug-ins must default
to "download" (or "input") mode, where URLs are transferred to local files;
if ``-upload`` is specified, local files are transferred ("uploaded") to
URLs, instead.

The plug-in is expected to do the transfer, exiting with status 0 if the
transfer was successful, and a non-zero status if the transfer was not
successful.  If the transfer was not succesful, the job will usually be
placed on hold; but see :ref:`the_output_file` below.  Regardless, the
job ClassAd attribute :ad-attr:`HoldReasonSubCode` will be set to the exit
status of the plug-in.

The Input File (v2)
"""""""""""""""""""

The input file is a sequence of ClassAds serialized in the "new" format;
see :ref:`classads/classad-mechanism:ClassAds: Old and New` for details.  There
will be one ad for each URL (if downloading) or file (if uploading).  Each
ClassAd will contain the following attributes:

* ``URL``, a string containing the URL to transfer to/from.
* ``LocalFileName``, a string containing the path to transfer to/from.

(Remember that ClassAd attribute names are not case-sensitive.)  Plug-ins must
parse the complete ClassAd language, and not just the subset any particular
version of HTCondor generates; see
:macro:`ASSUME_COMPATIBLE_MULTIFILE_PLUGINS`; the example plug-in does
this.  Plug-ins must ignore any attribute(s) they
don't know how to deal with; it is HTCondor's responsibility not to pass
attributes that shouldn't be ignored.  (For example: if a later version of
the protocol were to require plug-ins to honor an "untar" flag, HTCondor
should not pass that flag to an earlier version, because the user's job may
depend on that URL being untarred.)

As a reminder: the input file does *not* specify which direction the
transfer is going; that is done by the absence or presence of the
``-upload`` command line option.

By default, later versions of HTCondor may include
additional attributes in each input ad, based on attributes (optionally)
set in the job ad.  This feature is not presently documented because we
expect it to change -- and conformant plug-ins will ignore the extra
attributes.

.. _the_output_file:

The Output File (v2)
""""""""""""""""""""

Like the input file, the output file is a sequence of ClassAds serialized
in the "new" format; there must be one ad for each ad in the input file
(and thus for each file or URL, depending on the direction of transfer).

.. note::

    The output file is pre-allocated!  When you open the output file,
    do *not* truncate it.  The output file is pre-allocated so that if
    the transfer plug-in runs out of disk space, it still has room to
    tell HTCondor what happened.  (If the plug-in cleans up partial
    transfers, or if the disk was full but space becomes available before
    HTCondor has a chance to look around, it might not be able to tell,
    so this ability is important.)  Carefully consider allocating additional
    space to the output file (e.g., write more spaces) if your output comes
    close to filling it up.

The following attributes are required:

* ``TransferFileName``, a string.  Must be a ``LocalFileName`` from an
  input ad.
* ``TransferURL``, a string.  Must be a ``URL`` from the input ad.
* ``TransferSuccess``, a bool.  True if and only if the transfer of this
  URL to/from this file name succeeded.  If false, the ``TransferError``
  attribute must also be set.

The ``TransferFileName`` / ``TransferURL`` pair must correspond to one of
the ``LocalFileName`` / ``URL`` pairs from the input ads.

The following attributes are optional:

* ``TransferTotalBytes``, an integer.  Specifies how many bytes were sent
  or received to perform the transfer, which may differ from the number
  of bytes read from or written to the file.
* ``TransferError``, a string.  If ``TransferSuccess`` is false, this must
  be set.  It should be meaningful to the job submitter.
* ``DeveloperData``, a ClassAd.
* ``TransferErrorData``, a list of ClassAds.

The ``DeveloperData`` ClassAd is copied to the epoch history log and
otherwise ignored; it is intended to help plug-in developers debug problems.

Each ClassAd in ``TransferErrorData`` specifies additional data about
the nature of the failed transfer(s); this data is intended to allow HTCondor
to make better decisions(s) about how the handle the failure(s).  For
example, a plug-in may be told that a particular service is down but
should be back up in a few minutes.  The transfer fails, because that's longer
than the plug-in is willing to wait, but ``TransferErrorData`` allows the
plug-in to tell HTCondor that the transfer might work if tried again in a few
minutes -- as opposed to putting the job on hold, as would probably otherwise
be the case.

See :doc:`transfer_error_data` for details.

Protocol Version 4
------------------

Protocol version 4 is a small evolution of protocol version 2.  (There is
no protocol version 3.)

The Query Interface (v4)
''''''''''''''''''''''''

The query interface is identical, except that the plug-in must not add
attributes to the query ad that might conflict with attributes defined
in later versions of this document.  We'll try not to specify attribute
names that begin with ``_``, but the safest thing to do is not generate
any attributes other than those named in this specification.

The Transfer Interface (v4)
'''''''''''''''''''''''''''

The transfer interface is identical, with the following exceptions.

The Input File (v4)
"""""""""""""""""""

* Instead of duplicating information from the job ad into each individual
  input ad, HTCondor provides additional ads in the input file.  These ads do
  not correspond to a particular input file (or URL). Each individual input ad
  may still have additional
  attributes, but they will be specific to that file.  (For example, the user
  may wish tarball T to be untarred, but not zip file Z, because the former
  contains  a compressed application and the latter is itself an input to that
  application.)  This feature is not presently documented because we expect
  it to change -- and conformant plug-ins will ignore the extra attributes.
* Ads that don't correspond to a particular input file (or URL) will not have
  the `URL` or `LocalFileName` attributes.
* Ads that don't correspond to a particular input file (or URL) may be present
  anywhere in the input file.  However, these ads must only affect file ads
  after them in the input file.

The Output File (v4)
""""""""""""""""""""

* The ``TransferErrorData`` attribute is mandatory in version 4.

Further Protocol Versions
-------------------------

The intent is to change protocol version numbers sparingly, and only when
plug-ins conformant to the previous version might break.  To achieve this
goal, we will from time to time specify additional (optional) attributes
for the various ads that will change how HTCondor interacts with the plug-in.
(To continue the previous example, we might add a ''HasUntar'' boolean to
the query interface which causes the ''MustUntar'' flag to be set in some
specific transfer ads; we might further expect the plug-in to explicitly
note in the corresponding output ad if the untarring was succesful.)
