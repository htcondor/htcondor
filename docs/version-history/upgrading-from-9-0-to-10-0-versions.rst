Upgrading from an 9.0 LTS version to an 10.0 LTS version of HTCondor
====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 9.0 LTS version of HTCondor to a 10.0 LTS version will bring
new features introduced in the 9.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Version 9 Feature Releases <../version-history/development-release-series-91.html>`_):

-  Feature 1 :jira:`0000`
-  Feature 2

Upgrading from a 9.0 LTS version of HTCondor to a 10.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

- The semantics of undefined user job policy expressions has changed.  A
  policy whose expression evaluates to undefined is now uniformly ignored,
  instead of either putting the job on hold or treated as false.
  :jira:`442`

- Jobs that use a ``Requirements`` expression to try and match to specific a GPU should
  be changed to use the new ``require_gpus`` submit command or jobs will simply not match. If your machines
  have only a single type of GPU, you may be able to modify the machine configuration
  to allow users to delay having to make this change. This is a consequence of the fact
  that multiple GPUs of different types in a single machine is now supported.
  Attributes such as ``CUDACapability`` will no longer be advertised because it is not reasonable
  to assume that all GPUs will have a single value for this property.  Instead the propertes of
  each GPU will be advertised individually in a format that allows a job to request it run
  on a specific GPU or type of GPU.
  :jira:`953`


-  Item 1 :jira:`0000`
-  Item 2
