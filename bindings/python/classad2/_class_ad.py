from .classad2_impl import _handle as handle_t

from .classad2_impl import (
    _classad_init,
    _classad_init_from_string,
    _classad_init_from_dict,
    _classad_to_string,
    _classad_to_repr,
    _classad_get_item,
    _classad_set_item,
    _classad_del_item,
    _exprtree_eq,
    _classad_size,
    _classad_keys,
    _classad_parse_next,
    _classad_parse_next_fd,
    _classad_quote,
    _classad_unquote,
)

# So that the typehints match version 1.
from ._parser_type import ParserType as Parser

from ._value import Value

from collections.abc import MutableMapping
from datetime import datetime, timedelta, timezone

from typing import Iterator
from typing import Union
from typing import IO

#
# MutableMapping provides generic implementations for all mutable mapping
# methods except for __[get|set|del]item__(), __iter__, and __len__.
#
# The version 1 documentation says "[A] ClassAd object is iterable
# (returning the attributes) and implements the dictionary protocol.
# The items(), keys(), values(), get(), setdefault(), and update()
# methods have the same semantics as a dictionary."
#

class ClassAd(MutableMapping):

    def __init__(self, input=None):
        self._handle = handle_t()

        if input is None:
            _classad_init(self, self._handle)
            return
        if isinstance(input, dict):
            _classad_init_from_dict(self, self._handle, input)
            return
        if isinstance(input, str):
            _classad_init_from_string(self, self._handle, input)
            return
        raise TypeError("input must be None, str, or dict")


    def __repr__(self):
        return _classad_to_repr(self._handle)


    def __str__(self):
        return _classad_to_string(self._handle)


    def lookup(self, attr : str) -> "ExprTree":
        if not isinstance(attr, str):
            raise TypeError("ClassAd keys are strings")
        return _classad_get_item(self._handle, attr, False)


    def matches(self, ad : "ClassAd") -> bool:
        if not isinstance(ad, ClassAd):
            raise TypeError("ad must be a ClassAd")

        try:
            requirements = ad.lookup("requirements")
        except KeyError:
            return False

        result = requirements.eval(scope=ad, target=self)
        if isinstance(result, Value):
            # Neither `undefined` nor `error` are `true`.
            return False;
        if isinstance(result, bool):
            return result
        if isinstance(result, int):
            return result != 0
        if isinstance(result, float):
            return bool(result)

        return False;


    def symmetricMatch(self, ad : "ClassAd") -> bool:
        '''
        Equivalent to ``self.matches(ad) and ad.matches(self)``.
        '''
        return self.matches(ad) and ad.matches(self)


    # In version 1, we didn't allow Python to check if others knew how
    # to do equality comparisons against us, which was probably wrong.
    #
    # If don't define __eq__(), the one we get from the ABC Mapping
    # allows comparisons against other `Mapping`s, which we may or
    # may not actually want.
    def __eq__(self, other):
        if type(self) is type(other):
            return _exprtree_eq(self._handle, other._handle)
        else:
            return NotImplemented


    #
    # The MutableMapping abstract methods.
    #

    def __len__(self):
        return _classad_size(self._handle)


    def __getitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_get_item(self._handle, key, True)


    def __setitem__(self, key, value):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        if isinstance(value, dict):
            return _classad_set_item(self._handle, key, ClassAd(value))
        return _classad_set_item(self._handle, key, value)


    def __delitem__(self, key):
        if not isinstance(key, str):
            raise KeyError("ClassAd keys are strings")
        return _classad_del_item(self._handle, key)


    def __iter__(self):
        keys = _classad_keys(self._handle)
        for key in keys:
            yield key


def _convert_local_datetime_to_utc_ts(dt):
    if dt.tzinfo is not None and dt.tzinfo.utcoffset(dt) is not None:
        the_epoch = datetime(1970, 1, 1, tzinfo=dt.tzinfo)
        return (dt - the_epoch) // timedelta(seconds=1)
    else:
        the_epoch = datetime(1970, 1, 1)
        naive_ts = (dt - the_epoch) // timedelta(seconds=1)
        offset = dt.astimezone().utcoffset() // timedelta(seconds=1)
        return naive_ts - offset


def _parse_ads_generator(input, parser : Parser = Parser.Auto):
    total_offset = 0
    if not isinstance(input, str):
        total_offset = input.tell()

    input_string = "".join(input)
    while True:
        (ad, offset) = _classad_parse_next(input_string, int(parser))

        input_string = input_string[offset:]
        if not isinstance(input, str):
            total_offset += offset
            input.seek(total_offset, 0)

        if ad is None or offset == 0:
            return

        yield ad


def _parseAds(input : Union[str, IO], parser : Parser = Parser.Auto) -> Iterator[ClassAd]:
    '''
    Parses all of the ads in the input and returns a iterator over them.

    Ads in the input may be separated by blank lines.
    '''
    return _parse_ads_generator(input, parser)


def _parseOne(input : Union[str, Iterator[str]], parser : Parser = Parser.Auto) -> ClassAd:
    '''
    Parses all of the ads in the input, merges them into one, and returns it.

    Ads in the input may be separated by blank lines.
    '''
    total_offset = 0
    if not isinstance(input, str):
        total_offset = input.tell()

    result = ClassAd()
    input_string = "".join(input)
    while True:
        (firstAd, offset) = _classad_parse_next(input_string, int(parser))

        if firstAd is None or offset == 0:
            return result

        if firstAd is not None:
            result.update(firstAd)
        if offset != 0:
            input_string = input_string[offset:]

        if not isinstance(input, str):
            total_offset += offset
            input.seek(total_offset, 0)


# In version 1, this never actually returned an iterator, and it only
# advanced `input` if it was not a string.  Not sure why we bothered
# to allow strings here at all, actually.
def _parseNext(input : Union[str, IO], parser : Parser = Parser.Auto) -> ClassAd:
    '''
    Parses the first ad in the input and returns it.

    Ads in the input may be separated by blank lines.

    You must specify the parser type if ``input`` is not a string and
    can not be rewound.
    '''
    if isinstance(input, str):
        (firstAd, offset) = _classad_parse_next(input, int(parser))
        return firstAd
    else:
        if parser is Parser.Auto:
            if hasattr(input, "tell") and hasattr(input, "read") and hasattr(input, "seek"):
                parser = Parser.New

                location = input.tell()
                while True:
                    c = input.read(1)
                    if c == "/" or c == "[":
                        break
                    if not c.isspace():
                        parser = Parser.Old
                        break
                    if len(c) == 0:
                        break
                input.seek(location)
            else:
                raise ValueError("Stream cannot rewind; you must explicitly specify a parser.")
        return _classad_parse_next_fd(input.fileno(), int(parser))


def _quote(input : str) -> str:
    '''
    Quote the Python string so it can be used for building ClassAd expressions.
    '''
    return _classad_quote(input)


def _unquote(input : str) -> str:
    '''
    The reverse of `meth:quote`.  You should never need to call this.
    '''
    return _classad_unquote(input)
