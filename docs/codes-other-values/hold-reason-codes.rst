.. _Hold Reason Codes:

Hold Reason Codes
=================

Whenever a job is placed on hold, the
job ad will be updated with a :ad-attr:`HoldReasonCode` attribute, which will be set to a
numeric code indicating the reason for the hold.  The following table lists the
possible values for the :ad-attr:`HoldReasonCode` attribute, along with a brief 
description of each code.  In addition, the :ad-attr:`HoldReasonCode` attribute may be
accompanied by a :ad-attr:`HoldReasonSubCode` to give additional details.

The :ad-attr:`NumHoldsByReason` job attribute will also be updated with the number of times
the job has been held for each reason code, keyed by the
`NumHoldsByReason Label`.
For example, if a job has been held twice for reason code 12 and
once for reason code 26, the :ad-attr:`NumHoldsByReason` attribute will look like this:

    .. code-block:: condor-classad

        NumHoldsByReason = [ TransferOutputError = 2; SystemPolicy = 1 ]


.. include::  /codes-other-values/hold-table.rst

.. note::

    For hold codes 12 [TransferOutputError] and 13 [TransferInputError]:
    file transfer may invoke file-transfer plug-ins.  If it does, the hold
    subcodes may additionally be 62 (ETIME), if the file-transfer plug-in
    timed out; or the exit code of the plug-in shifted left by eight bits,
    otherwise.



