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
    _classad_flatten,
    _classad_internal_refs,
    _classad_external_refs,
    _classad_print_json,
    _classad_print_old,
    _classad_last_error,
    _classad_version,
    ClassAdException,
)

import io

# So that the typehints match version 1.
from ._parser_type import ParserType as Parser

from ._value import Value

from collections.abc import MutableMapping
from datetime import datetime, timedelta, timezone

from typing import (
    Iterator,
    Union,
    IO,
    Optional,
    List,
)

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
    R"""
    The `ClassAd language <https://htcondor.org/classad/refman/>`_
    defines a ClassAd as a mapping from attribute names to expressions.  A
    :class:`ClassAd` is a :class:`collections.abc.MutableMapping` from
    :class:`str`\ing keys to :class:`ExprTree` values.  Expressions are
    always evaluated before being returned, and unless you call
    :meth:`lookup`, converted into the corresponding Python type.

    A ClassAd expression may evaluate to (but is returned by
    :class:`ClassAd` as):

      * undefined (:data:`classad2.Value.Undefined`)
      * a boolean (:class:`bool`)
      * an integer (:class:`int`)
      * a float (:class:`float`)
      * a string (:class:`str`)
      * an absolute time value (:class:`datetime.datetime`)
      * a relative time value (:class:`float`)
      * a list (:class:`list`)
      * a ClassAd (:class:`ClassAd`)
      * error (:data:`classad2.Value.Error`)

    When setting a value, the reverse is true.  Additionally, :py:obj:`None`
    is converted to :data:`classad2.Value.Undefined` and :class:`dict`\s
    are converted to :class:`ClassAd`\s.
    """

    def __init__(self, input : Optional[Union[str, dict]] = None):
        """
        :param input:
            * If :py:obj:`None`, create an empty ClassAd.
            * If a :class:`str`, parse the :const:`ParserType.New` format
              string and create the corresponding ClassAd.
            * If a :class:`dict`, convert the dictionary to a ClassAd.
        """
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


    # This tells the pickle module to reconstruct ClassAd objects by
    # calling the constructor (that is, `__init__()`) with the given
    # tuple of arguments, intead of its default method, which tries
    # to pickle the handle, which is never going to work.
    def __reduce__(self):
        return (ClassAd, (repr(self),))


    def eval(self, attr : str):
        """
        Evaluate the attribute named by *attr* in the context of this ClassAd.

        :param attr: The attribute to evaluate.
        """
        expression = self.lookup(attr)
        return expression.eval(self)


    def externalRefs(self, expr: "ExprTree") -> List[str]:
        """
        Returns the list of attributes referenced by the expression
        *expr* which are **not** defined by this ClassAd.

        :param expr: The expression.
        """
        if not isinstance(expr, ExprTree):
            raise TypeError("expr must be an ExprTree")

        string_list = _classad_external_refs(self._handle, expr._handle)
        return string_list.split(",")


    def flatten(self, expr : "ExprTree") -> "ExprTree":
        """
        Partially evaluate the expression in the context of this ad.
        Attributes referenced by the expression which are not defined
        by this ClassAd are not evaluated, but other attributes and
        subexpressions are.  Thus, ``RequestMemory * 1024 * 1024`` will
        evaluate to the amount of requested memory in bytes if this
        ClassAd defines the ``RequestMemory`` attribute, and to
        ``RequestMemory * 1048576`` otherwise.

        :param expr: The expression.
        """
        if not isinstance(expr, ExprTree):
            raise TypeError("expr must be an ExprTree")
        return _classad_flatten(self._handle, expr._handle)


    def internalRefs(self, expr: "ExprTree") -> List[str]:
        """
        Returns the list of attributes referenced by the expression *expr*
        which **are** defined by this ClassAd.

        :param expr: The expression.
        """
        if not isinstance(expr, ExprTree):
            raise TypeError("expr must be an ExprTree")

        string_list = _classad_internal_refs(self._handle, expr._handle)
        return string_list.split(",")


    def lookup(self, attr : str) -> "ExprTree":
        """
        Return the :class:`ExprTree` named by *attr*.

        :param attr: The attribute to look up.
        """
        if not isinstance(attr, str):
            raise TypeError("ClassAd keys are strings")
        return _classad_get_item(self._handle, attr, False)


    def matches(self, ad : "ClassAd") -> bool:
        """
        Evaluate the ``requirements`` attribute of the given *ad* in the
        context of this one.

        :param ad: The ClassAd to test for matching.
        :return:  ``True`` if and only if the evaluation returned ``True``.
        """
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


    def printJson(self) -> str:
        """
        Serialize this ClassAd in the JSON format.
        """
        return _classad_print_json(self._handle)


    def printOld(self) -> str:
        """
        Serialize this ClassAd in the "old" format.
        """
        return _classad_print_old(self._handle)


    def symmetricMatch(self, ad : "ClassAd") -> bool:
        '''
        Equivalent to ``self.matches(ad) and ad.matches(self)``.

        :param ad:  The ClassAd to match for and against.
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
            raise TypeError("ClassAd keys are strings")
        return _classad_get_item(self._handle, key, True)


    def __setitem__(self, key, value):
        if not isinstance(key, str):
            raise TypeError("ClassAd keys are strings")
        if isinstance(value, dict):
            return _classad_set_item(self._handle, key, ClassAd(value))
        return _classad_set_item(self._handle, key, value)


    def __delitem__(self, key):
        if not isinstance(key, str):
            raise TypeError("ClassAd keys are strings")
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


#
# Lines starting with '#' are ignored in the file parser.  The "old" and
# "new" parsers both handle C-style ('//') and C++-style ('/* */') comments,
# but not when a comment starts a new ad.
#
# The version 1 bindings didn't include JSON or XML, so I'm ignoring the
# fact that JSON doesn't allow multiple ads at all right now (and not
# testing XML).  We should fix this eventually, but that might be a long
# way away.
#
def _parseAds(input : Union[str, IO], parser : Parser = Parser.Auto) -> Iterator[ClassAd]:
    '''
    Returns a generator which will parse each ad in the input.

    Ads serialized in the :const:`ParserType.Old` format must be separated by blank lines.
    Ads serialized in the :const:`ParserType.New` format may be separated by blank lines.

    :param input:  One or more serialized ClassAds.  The serializations must
                   all be in the same format.
    :param parser:  Which parser to use (serialization format to assume).  If
                    unspecified, attempt to determine if the serialized ads
                    are in the :const:`ParserType.Old` or
                    :const:`ParserType.New` format.
    '''
    return _parse_ads_generator(input, parser)


def _parseOne(input : Union[str, Iterator[str]], parser : Parser = Parser.Auto) -> ClassAd:
    '''
    Parses all of the ads in the input, merges them into one, and returns it.

    If *input* is a single string,
    ads serialized in the :const:`ParserType.Old` format must be separated by blank lines;
    ads serialized in the :const:`ParserType.New` format may be separated by blank lines.

    If *input* is an iterator,
    each serialized ad must be in its own string.

    :param input:  One or more serialized ClassAds.  The serializations must
                   all be in the same format.
    :param parser:  Which parser to use (serialization format to assume).  If
                    unspecified, attempt to determine if the serialized ads
                    are in the :const:`ParserType.Old or
                    :const:`ParserType.New` formats.
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

    Ads serialized in the :const:`ParserType.Old` format must be separated by blank lines.
    Ads serialized in the :const:`ParserType.New` format may be separated by blank lines.

    You must specify the parser type if *input* is not a string and
    can not be rewound.

    :param input:  One or more serialized ClassAds.  The serializations must
                   all be in the same format.
    :param parser:  Which parser to use (serialization format to assume).
    :raises ClassAdException:  If ``input`` is not a string and can
      not be rewound.
    '''
    if isinstance(input, str):
        (firstAd, offset) = _classad_parse_next(input, int(parser))
        return firstAd
    else:
        if parser is Parser.Auto:
            try:
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
            except io.UnsupportedOperation as e:
                raise ClassAdException("Can not look ahead in stream; you must explicitly specify a parser.") from e
        return _classad_parse_next_fd(input.fileno(), int(parser))


def _quote(input : str) -> str:
    '''
    Quote *input* so it can be used for building ClassAd expressions.

    :param input:  The string to quote according the ClassAd syntax.
    '''
    return _classad_quote(input)


def _unquote(input : str) -> str:
    '''
    The inverse of :func:`quote`.

    :param input:  The ClassAd string to unquote into its literal value.
    '''
    return _classad_unquote(input)


def _lastError() -> str:
    '''
    Return the string representation of the last error to occur in the
    ClassAd library.

    As the ClassAd language has no concept of an exception, this is the
    only mechanism to receive detailed error messages from functions.
    '''
    return _classad_last_error()


def _version() -> str:
    '''
    Returns the version of ClassAd library this module is linked against.
    '''
    return _classad_version()

# Sphinx whines about forward references in the type hints if this isn't here.
from ._expr_tree import ExprTree
