'''File transfers.
'''

from utils import *
import os


class IFileTransfer(Cached):
    '''Represents a file transfer.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Alter(self, AlterName, Args=None):
        return self._Skype._Alter('FILETRANSFER', self._Id, AlterName, Args)

    def _Init(self, Id, Skype):
        self._Id = int(Id)
        self._Skype = Skype

    def _Property(self, PropName, Set=None):
        return self._Skype._Property('FILETRANSFER', self._Id, PropName, Set)

    def _GetBytesPerSecond(self):
        return int(self._Property('BYTESPERSECOND'))

    BytesPerSecond = property(_GetBytesPerSecond,
    doc='''Transfer speed in bytes per second.

    @type: int
    ''')

    def _GetBytesTransferred(self):
        return long(self._Property('BYTESTRANSFERRED'))

    BytesTransferred = property(_GetBytesTransferred,
    doc='''Number of bytes transferred.

    @type: long
    ''')

    def _GetFailureReason(self):
        return self._Property('FAILUREREASON')

    FailureReason = property(_GetFailureReason,
    doc='''Transfer failure reason.

    @type: L{File transfer failure reason<enums.fileTransferFailureReasonSenderNotAuthorized>}
    ''')

    def _GetFileName(self):
        return os.path.split(self.FilePath)[1]

    FileName = property(_GetFileName,
    doc='''Name of the transferred file.

    @type: unicode
    ''')

    def _GetFilePath(self):
        return self._Property('FILEPATH')

    FilePath = property(_GetFilePath,
    doc='''Full path to the transferred file.

    @type: unicode
    ''')

    def _GetFileSize(self):
        return long(self._Property('FILESIZE'))

    FileSize = property(_GetFileSize,
    doc='''Size of the transferred file in bytes.

    @type: long
    ''')

    def _GetFinishDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.FinishTime)

    FinishDatetime = property(_GetFinishDatetime,
    doc='''File transfer end date and time.

    @type: datetime.datetime
    ''')

    def _GetFinishTime(self):
        return float(self._Property('FINISHTIME'))

    FinishTime = property(_GetFinishTime,
    doc='''File transfer end timestamp.

    @type: float
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Unique file transfer Id.

    @type: int
    ''')

    def _GetPartnerDisplayName(self):
        return self._Property('PARTNER_DISPNAME')

    PartnerDisplayName = property(_GetPartnerDisplayName,
    doc='''File transfer partner DisplayName.

    @type: unicode
    ''')

    def _GetPartnerHandle(self):
        return self._Property('PARTNER_HANDLE')

    PartnerHandle = property(_GetPartnerHandle,
    doc='''File transfer partner Skypename.

    @type: unicode
    ''')

    def _GetStartDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.StartTime)

    StartDatetime = property(_GetStartDatetime,
    doc='''File transfer start date and time.

    @type: datetime.datetime
    ''')

    def _GetStartTime(self):
        return float(self._Property('STARTTIME'))

    StartTime = property(_GetStartTime,
    doc='''File transfer start timestamp.

    @type: float
    ''')

    def _GetStatus(self):
        return self._Property('STATUS')

    Status = property(_GetStatus,
    doc='''File transfer status.

    @type: L{File transfer status<enums.fileTransferStatusNew>}
    ''')

    def _GetType(self):
        return self._Property('TYPE')

    Type = property(_GetType,
    doc='''File transfer type.

    @type: L{File transfer type<enums.fileTransferTypeIncoming>}
    ''')
