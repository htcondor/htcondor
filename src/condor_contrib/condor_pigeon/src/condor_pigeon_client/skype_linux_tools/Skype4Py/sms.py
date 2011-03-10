'''Short messaging to cell phones.
'''

from utils import *


class ISmsChunk(Cached):
    '''Represents a single chunk of a multi-part SMS message.
    '''

    def __repr__(self):
        return '<%s with Id=%s, Message=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id), repr(self.Message))

    def _Init(self, Id_Message):
        Id, Message = Id_Message
        self._Id = int(Id)
        self._Message = Message

    def _GetCharactersLeft(self):
        count, left = map(int, chop(self._Message._Property('CHUNKING', Cache=False)))
        if self._Id == count - 1:
            return left
        return 0

    CharactersLeft = property(_GetCharactersLeft,
    doc='''CharactersLeft.

    @type: int
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''SMS chunk Id.

    @type: int
    ''')

    def _GetMessage(self):
        return self._Message

    Message = property(_GetMessage,
    doc='''SMS message associated with this chunk.

    @type: L{ISmsMessage}
    ''')

    def _GetText(self):
        return self._Message._Property('CHUNK %s' % self._Id)

    Text = property(_GetText,
    doc='''Text (body) of this SMS chunk.

    @type: unicode
    ''')


class ISmsMessage(Cached):
    '''Represents an SMS message.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Alter(self, AlterName, Args=None):
        return self._Skype._Alter('SMS', self._Id, AlterName, Args)

    def _Init(self, Id, Skype):
        self._Id = int(Id)
        self._Skype = Skype

    def _Property(self, PropName, Set=None, Cache=True):
        return self._Skype._Property('SMS', self._Id, PropName, Set, Cache)

    def Delete(self):
        '''Deletes this SMS message.
        '''
        self._Skype._DoCommand('DELETE SMS %s' % self._Id)

    def Send(self):
        '''Sends this SMS message.
        '''
        self._Alter('SEND')

    def _GetBody(self):
        return self._Property('BODY')

    def _SetBody(self, value):
        self._Property('BODY', value)

    Body = property(_GetBody, _SetBody,
    doc='''Text of this SMS message.

    @type: unicode
    ''')

    def _GetChunks(self):
        return tuple([ISmsChunk((x, self)) for x in range(int(chop(self._Property('CHUNKING', Cache=False))[0]))])

    Chunks = property(_GetChunks,
    doc='''Chunks of this SMS message. More than one if this is a multi-part message.

    @type: tuple of L{ISmsChunk}
    ''')

    def _GetDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.Timestamp)

    Datetime = property(_GetDatetime,
    doc='''Timestamp of this SMS message as datetime object.

    @type: datetime.datetime
    ''')

    def _GetFailureReason(self):
        return self._Property('FAILUREREASON')

    FailureReason = property(_GetFailureReason,
    doc='''Reason an SMS message failed. Read this if L{Status} == L{smsMessageStatusFailed<enums.smsMessageStatusFailed>}.

    @type: L{SMS failure reason<enums.smsFailureReasonUnknown>}
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Unique SMS message Id.

    @type: int
    ''')

    def _GetIsFailedUnseen(self):
        return self._Property('IS_FAILED_UNSEEN') == 'TRUE'

    IsFailedUnseen = property(_GetIsFailedUnseen,
    doc='''Tells if a failed SMS message was unseen.

    @type: bool
    ''')

    def _GetPrice(self):
        return int(self._Property('PRICE'))

    Price = property(_GetPrice,
    doc='''SMS price. Expressed using L{PricePrecision}. For a value expressed using L{PriceCurrency}, use L{PriceValue}.

    @type: int
    @see: L{PriceCurrency}, L{PricePrecision}, L{PriceToText}, L{PriceValue}
    ''')

    def _GetPriceCurrency(self):
        return self._Property('PRICE_CURRENCY')

    PriceCurrency = property(_GetPriceCurrency,
    doc='''SMS price currency.

    @type: unicode
    @see: L{Price}, L{PricePrecision}, L{PriceToText}, L{PriceValue}
    ''')

    def _GetPricePrecision(self):
        return int(self._Property('PRICE_PRECISION'))

    PricePrecision = property(_GetPricePrecision,
    doc='''SMS price precision.

    @type: int
    @see: L{Price}, L{PriceCurrency}, L{PriceToText}, L{PriceValue}
    ''')

    def _GetPriceToText(self):
        return (u'%s %.3f' % (self.PriceCurrency, self.PriceValue)).strip()

    PriceToText = property(_GetPriceToText,
    doc='''SMS price as properly formatted text with currency.

    @type: unicode
    @see: L{Price}, L{PriceCurrency}, L{PricePrecision}, L{PriceValue}
    ''')

    def _GetPriceValue(self):
        if self.Price < 0:
            return 0.0
        return float(self.Price) / (10 ** self.PricePrecision)

    PriceValue = property(_GetPriceValue,
    doc='''SMS price. Expressed in L{PriceCurrency}.

    @type: float
    @see: L{Price}, L{PriceCurrency}, L{PricePrecision}, L{PriceToText}
    ''')

    def _GetReplyToNumber(self):
        return self._Property('REPLY_TO_NUMBER')

    def _SetReplyToNumber(self, value):
        self._Property('REPLY_TO_NUMBER', value)

    ReplyToNumber = property(_GetReplyToNumber, _SetReplyToNumber,
    doc='''Reply-to number for this SMS message.

    @type: unicode
    ''')

    def _SetSeen(self, value):
        self._Property('SEEN', cndexp(value, 'TRUE', 'FALSE'))

    Seen = property(fset=_SetSeen,
    doc='''Read status of the SMS message.

    @type: bool
    ''')

    def _GetStatus(self):
        return self._Property('STATUS')

    Status = property(_GetStatus,
    doc='''SMS message status.

    @type: L{SMS message status<enums.smsMessageStatusUnknown>}
    ''')

    def _GetTargetNumbers(self):
        return tuple(esplit(self._Property('TARGET_NUMBERS'), ', '))

    def _SetTargetNumbers(self, value):
        self._Property('TARGET_NUMBERS', ', '.join(value))

    TargetNumbers = property(_GetTargetNumbers, _SetTargetNumbers,
    doc='''Target phone numbers.

    @type: tuple of unicode
    ''')

    def _GetTargets(self):
        return tuple([ISmsTarget((x, self)) for x in esplit(self._Property('TARGET_NUMBERS'), ', ')])

    Targets = property(_GetTargets,
    doc='''Target objects.

    @type: tuple of L{ISmsTarget}
    ''')

    def _GetTimestamp(self):
        return float(self._Property('TIMESTAMP'))

    Timestamp = property(_GetTimestamp,
    doc='''Timestamp of this SMS message.

    @type: float
    @see: L{Datetime}
    ''')

    def _GetType(self):
        return self._Property('TYPE')

    Type = property(_GetType,
    doc='''SMS message type

    @type: L{SMS message type<enums.smsMessageTypeUnknown>}
    ''')


class ISmsTarget(Cached):
    '''Represents a single target of a multi-target SMS message.
    '''

    def __repr__(self):
        return '<%s with Number=%s, Message=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Number), repr(self.Message))

    def _Init(self, Number_Message):
        Number, Message = Number_Message
        self._Number = Number
        self._Message = Message

    def _GetMessage(self):
        return self._Message

    Message = property(_GetMessage,
    doc='''An SMS message object this target refers to.

    @type: L{ISmsMessage}
    ''')

    def _GetNumber(self):
        return self._Number

    Number = property(_GetNumber,
    doc='''Target phone number.

    @type: unicode
    ''')

    def _GetStatus(self):
        for t in esplit(self._Message._Property('TARGET_STATUSES'), ', '):
            number, status = t.split('=')
            if number == self._Number:
                return status

    Status = property(_GetStatus,
    doc='''Status of this target.

    @type: L{SMS target status<enums.smsTargetStatusUnknown>}
    ''')
