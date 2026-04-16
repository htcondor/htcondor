.. _Vacate Reason Codes:

Vacate Reason Codes
===================

Whenever a job or claim on an EP slot is kicked off the slot, the
job ad will be updated with a :ad-attr:`VacateReasonCode` attribute, which will be set to a
numeric code indicating the reason for the vacate.  The following table lists the
possible values for the :ad-attr:`VacateReasonCode` attribute, along with a brief 
description of each code.  In addition, the :ad-attr:`VacateReasonCode` attribute may be
accompanied by a :ad-attr:`VacateReasonSubCode` to give additional details.

The :ad-attr:`NumVacatesByReason` job attribute will also be updated with the number of times
the job or claim has been vacated for each reason code, keyed by the
`NumVacatesByReason Label`.
For example, if a job has been vacated twice for reason code 12 and
once for reason code 1023, the :ad-attr:`NumVacatesByReason` attribute will look like this:

    .. code-block:: condor-classad

        NumVacatesByReason = [ TransferOutputError = 2; JobRemoved = 1 ]


.. include:: /_static/generated/rst/vacate-code-combined-table.rst

.. note::

    For vacate codes 12 [TransferOutputError] and 13 [TransferInputError],
    file transfer may invoke file-transfer plug-ins.  If it does, the vacate
    subcodes may additionally be:

    * 62 (ETIME) if the file-transfer plug-in timed out.
    * -1001 (FileTransferPluginNotFound) if the file-transfer plug-in was not found installed on the EP.
    * -1002 (FileTransferPluginNotOperational) if the file-transfer plug-in failed to execute properly.
    * -1003 (FileTransferPluginExecFailed) if the EP was unable to launch the file-transfer plug-in (maybe EP is out of memory?).
    * -1004 (FileTransferPluginNoResultReported) if the file-transfer plug-in failed to report a result (maybe disk is full?).
    * Otherwise, the exit code of the plug-in shifted left by eight bits.
