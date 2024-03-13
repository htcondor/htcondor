Bringing Your Own Resources
---------------------------

You may have access to more, or different, resources than your HTCondor
administrator.  For example, you may have
`credits <https://path-cc.io/services/credit-accounts/>`_
at the
`PATh facility <https://path-cc.io/facility/index.html>`_
, or an
`ACCESS allocation <https://allocations.access-ci.org/>`_
at
`Anvil <https://www.rcac.purdue.edu/compute/anvil>`_,
`Bridges-2 <https://www.psc.edu/resources/bridges-2/>`_,
`Expanse <https://www.sdsc.edu/services/hpc/expanse/>`_,
or
`Perlmutter <https://docs.nersc.gov/systems/perlmutter/>`_;
or you might have funds for
`AWS <https://aws.amazon.com>`_
VMs.

If you want to make use of any of these resources, you may "bring your own
resources" to any AP which has that functionality enabled.  When you do,
you'll give the set of resources you're leasing a name; we call a named
set of leased resources an *annex*.

HTCondor provides access to two different kinds of annexes:
the so-called "HPC" annexes, through the
:doc:`../man-pages/htcondor`
tool; and cloud annexes, through the
:doc:`../man-pages/condor_annex`
tool.  The latter is described in some detail in
:doc:`../cloud-computing/index`
section; we'll only discuss the former here.

Recipes
'''''''

The following recipes assume you have an
`OSG Connect <https://www.osgconnect.net/>`_
account and password.  If you're using a different AP,
and your AP administrator has
:ref:`enabled <enabling htcondor annex>`
``htcondor annex``, just log into that AP instead of an OSG Connect AP.

| :doc:`../faq/users/path-facility`
| :doc:`../faq/users/anvil`
| :doc:`../faq/users/bridges2`
| :doc:`../faq/users/expanse`
| :doc:`../faq/users/perlmutter`

Overview
''''''''

FIXME
