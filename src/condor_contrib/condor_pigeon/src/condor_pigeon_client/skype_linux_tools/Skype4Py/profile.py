'''Current user profile.
'''

from utils import *
import weakref


class IProfile(object):
    '''Represents the profile of currently logged in user. Access using L{ISkype.CurrentUserProfile<skype.ISkype.CurrentUserProfile>}.
    '''

    def __init__(self, Skype):
        '''__init__.

        @param Skype: Skype object.
        @type Skype: L{ISkype}
        '''
        self._SkypeRef = weakref.ref(Skype)

    def _Property(self, PropName, Set=None):
        return self._Skype._Property('PROFILE', '', PropName, Set)

    def _Get_Skype(self):
        skype = self._SkypeRef()
        if skype:
            return skype
        raise Exception()

    _Skype = property(_Get_Skype)

    def _GetAbout(self):
        return self._Property('ABOUT')

    def _SetAbout(self, value):
        self._Property('ABOUT', value)

    About = property(_GetAbout, _SetAbout,
    doc='''"About" field of the profile.

    @type: unicode
    ''')

    def _GetBalance(self):
        return int(self._Property('PSTN_BALANCE'))

    Balance = property(_GetBalance,
    doc='''Skype credit balance. Note that the precision of profile balance value is currently
    fixed at 2 decimal places, regardless of currency or any other settings. Use L{BalanceValue}
    to get the balance expressed in currency.

    @type: int
    @see: L{BalanceCurrency}, L{BalanceToText}, L{BalanceValue}
    ''')

    def _GetBalanceCurrency(self):
        return self._Property('PSTN_BALANCE_CURRENCY')

    BalanceCurrency = property(_GetBalanceCurrency,
    doc='''Skype credit balance currency.

    @type: unicode
    @see: L{Balance}, L{BalanceToText}, L{BalanceValue}
    ''')

    def _GetBalanceToText(self):
        return (u'%s %.2f' % (self.BalanceCurrency, self.BalanceValue)).strip()

    BalanceToText = property(_GetBalanceToText,
    doc='''Skype credit balance as properly formatted text with currency.

    @type: unicode
    @see: L{Balance}, L{BalanceCurrency}, L{BalanceValue}
    ''')

    def _GetBalanceValue(self):
        return float(self._Property('PSTN_BALANCE')) / 100

    BalanceValue = property(_GetBalanceValue,
    doc='''Skype credit balance expressed in currency.

    @type: float
    @see: L{Balance}, L{BalanceCurrency}, L{BalanceToText}
    ''')

    def _GetBirthday(self):
        value = self._Property('BIRTHDAY')
        if len(value) == 8:
            from datetime import date
            from time import strptime
            return date(*strptime(value, '%Y%m%d')[:3])

    def _SetBirthday(self, value):
        if value:
            self._Property('BIRTHDAY', value.strftime('%Y%m%d'))
        else:
            self._Property('BIRTHDAY', 0)

    Birthday = property(_GetBirthday, _SetBirthday,
    doc='''"Birthday" field of the profile.

    @type: datetime.date
    ''')

    def _GetCallApplyCF(self):
        return self._Property('CALL_APPLY_CF') == 'TRUE'

    def _SetCallApplyCF(self, value):
        self._Property('CALL_APPLY_CF', cndexp(value, 'TRUE', 'FALSE'))

    CallApplyCF = property(_GetCallApplyCF, _SetCallApplyCF,
    doc='''Tells if call forwarding is enabled in the profile.

    @type: bool
    ''')

    def _GetCallForwardRules(self):
        return self._Property('CALL_FORWARD_RULES')

    def _SetCallForwardRules(self, value):
        self._Property('CALL_FORWARD_RULES', value)

    CallForwardRules = property(_GetCallForwardRules, _SetCallForwardRules,
    doc='''Call forwarding rules of the profile.

    @type: unicode
    ''')

    def _GetCallNoAnswerTimeout(self):
        return int(self._Property('CALL_NOANSWER_TIMEOUT'))

    def _SetCallNoAnswerTimeout(self, value):
        self._Property('CALL_NOANSWER_TIMEOUT', value)

    CallNoAnswerTimeout = property(_GetCallNoAnswerTimeout, _SetCallNoAnswerTimeout,
    doc='''Number of seconds a call will ring without being answered before it stops ringing.

    @type: int
    ''')

    def _GetCallSendToVM(self):
        return self._Property('CALL_SEND_TO_VM') == 'TRUE'

    def _SetCallSendToVM(self, value):
        self._Property('CALL_SEND_TO_VM', cndexp(value, 'TRUE', 'FALSE'))

    CallSendToVM = property(_GetCallSendToVM, _SetCallSendToVM,
    doc='''Tells whether calls will be sent to the voicemail.

    @type: bool
    ''')

    def _GetCity(self):
        return self._Property('CITY')

    def _SetCity(self, value):
        self._Property('CITY', value)

    City = property(_GetCity, _SetCity,
    doc='''"City" field of the profile.

    @type: unicode
    ''')

    def _GetCountry(self):
        return chop(self._Property('COUNTRY'))[0]

    def _SetCountry(self, value):
        self._Property('COUNTRY', value)

    Country = property(_GetCountry, _SetCountry,
    doc='''"Country" field of the profile.

    @type: unicode
    ''')

    def _GetFullName(self):
        return self._Property('FULLNAME')

    def _SetFullName(self, value):
        self._Property('FULLNAME', value)

    FullName = property(_GetFullName, _SetFullName,
    doc='''"Full name" field of the profile.

    @type: unicode
    ''')

    def _GetHomepage(self):
        return self._Property('HOMEPAGE')

    def _SetHomepage(self, value):
        self._Property('HOMEPAGE', value)

    Homepage = property(_GetHomepage, _SetHomepage,
    doc='''"Homepage" field of the profile.

    @type: unicode
    ''')

    def _GetIPCountry(self):
        return self._Property('IPCOUNTRY')

    IPCountry = property(_GetIPCountry,
    doc='''ISO country code queried by IP address.

    @type: unicode
    ''')

    def _GetLanguages(self):
        return tuple(esplit(self._Property('LANGUAGES')))

    def _SetLanguages(self, value):
        self._Property('LANGUAGES', ' '.join(value))

    Languages = property(_GetLanguages, _SetLanguages,
    doc='''"ISO language codes of the profile.

    @type: tuple of unicode
    ''')

    def _GetMoodText(self):
        return self._Property('MOOD_TEXT')

    def _SetMoodText(self, value):
        self._Property('MOOD_TEXT', value)

    MoodText = property(_GetMoodText, _SetMoodText,
    doc='''"Mood text" field of the profile.

    @type: unicode
    ''')

    def _GetPhoneHome(self):
        return self._Property('PHONE_HOME')

    def _SetPhoneHome(self, value):
        self._Property('PHONE_HOME', value)

    PhoneHome = property(_GetPhoneHome, _SetPhoneHome,
    doc='''"Phone home" field of the profile.

    @type: unicode
    ''')

    def _GetPhoneMobile(self):
        return self._Property('PHONE_MOBILE')

    def _SetPhoneMobile(self, value):
        self._Property('PHONE_MOBILE', value)

    PhoneMobile = property(_GetPhoneMobile, _SetPhoneMobile,
    doc='''"Phone mobile" field of the profile.

    @type: unicode
    ''')

    def _GetPhoneOffice(self):
        return self._Property('PHONE_OFFICE')

    def _SetPhoneOffice(self, value):
        self._Property('PHONE_OFFICE', value)

    PhoneOffice = property(_GetPhoneOffice, _SetPhoneOffice,
    doc='''"Phone office" field of the profile.

    @type: unicode
    ''')

    def _GetProvince(self):
        return self._Property('PROVINCE')

    def _SetProvince(self, value):
        self._Property('PROVINCE', value)

    Province = property(_GetProvince, _SetProvince,
    doc='''"Province" field of the profile.

    @type: unicode
    ''')

    def _GetRichMoodText(self):
        return self._Property('RICH_MOOD_TEXT')

    def _SetRichMoodText(self, value):
        self._Property('RICH_MOOD_TEXT', value)

    RichMoodText = property(_GetRichMoodText, _SetRichMoodText,
    doc='''Rich mood text of the profile.

    @type: unicode
    @see: U{https://developer.skype.com/Docs/ApiDoc/SET_PROFILE_RICH_MOOD_TEXT}
    ''')

    def _GetSex(self):
        return self._Property('SEX')

    def _SetSex(self, value):
        self._Property('SEX', value)

    Sex = property(_GetSex, _SetSex,
    doc='''"Sex" field of the profile.

    @type: L{User sex<enums.usexUnknown>}
    ''')

    def _GetTimezone(self):
        return int(self._Property('TIMEZONE'))

    def _SetTimezone(self, value):
        self._Property('TIMEZONE', value)

    Timezone = property(_GetTimezone, _SetTimezone,
    doc='''Timezone of the current profile in minutes from GMT.

    @type: int
    ''')

    def _GetValidatedSmsNumbers(self):
        return tuple(esplit(self._Property('SMS_VALIDATED_NUMBERS'), ', '))

    ValidatedSmsNumbers = property(_GetValidatedSmsNumbers,
    doc='''List of phone numbers the user has registered for usage in reply-to field of SMS messages.

    @type: tuple of unicode
    ''')
