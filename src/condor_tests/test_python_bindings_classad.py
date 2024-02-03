#!/usr/bin/env pytest

import pytest

import pickle
import datetime
import textwrap
import json
import os

import classad2 as classad


# this test replicates python_bindings_classad.run


@pytest.fixture
def ad():
    """A fresh, blank Python ClassAd."""
    return classad.ClassAd()


EQUAL_ADS = [
    (classad.ClassAd(), classad.ClassAd()),
    (classad.ClassAd({"foo": "bar"}), classad.ClassAd({"foo": "bar"})),
]

UNEQUAL_ADS = [
    (classad.ClassAd(), classad.ClassAd({"foo": "bar"})),
    (classad.ClassAd(), None),
    (classad.ClassAd(), {}),
    (classad.ClassAd(), []),
    (classad.ClassAd({"foo": "2"}), classad.ClassAd({"foo": 2})),
    (classad.ClassAd({"foo": classad.ExprTree("1 + 1")}), classad.ClassAd({"foo": 2})),
    ("foo", classad.ClassAd({"foo": "bar"})),
    ({"foo": "bar"}, classad.ClassAd({"foo": "bar"})),
    (1, classad.ClassAd({"foo": "bar"})),
    (1.2, classad.ClassAd({"foo": "bar"})),
    (True, classad.ClassAd({"foo": "bar"})),
    (False, classad.ClassAd({"foo": "bar"})),
    (None, classad.ClassAd({"foo": "bar"})),
    (None, classad.ClassAd({"foo": classad.Value.Undefined})),
]


@pytest.mark.parametrize("a, b", EQUAL_ADS)
def test_equality(a, b):
    assert a == b
    assert b == a


@pytest.mark.parametrize("a, b", UNEQUAL_ADS)
def test_equality_bad(a, b):
    assert not a == b
    assert not b == a


@pytest.mark.parametrize("a, b", UNEQUAL_ADS)
def test_inequality(a, b):
    assert a != b
    assert b != a


@pytest.mark.parametrize("a, b", EQUAL_ADS)
def test_inequality_bad(a, b):
    assert not a != b
    assert not b != a


@pytest.mark.parametrize(
    "arg",
    [
        '[foo = "1"; bar = 2]',
        {"foo": "1", "bar": 2},
        # [('foo', "1"), ('bar', 2)],  # this one would be nice, but doesn't work yet
    ],
)
def test_constructor(arg):
    ad = classad.ClassAd(arg)

    assert ad["foo"] == "1"
    assert ad["bar"] == 2


def test_raise_key_error_for_missing_key(ad):
    with pytest.raises(KeyError):
        ad["missing"]


@pytest.mark.parametrize(
    "value",
    [
        4,
        "2 + 2",
        [1, 2, 3],
        {"a": 1, "b": "wiz"},
        {"outer": ["inner"]},
        True,
        False,
        classad.Value.Undefined,
        # classad.Value.Error does not roundtrip, because looking it up throws an exception
    ],
)
def test_value_roundtrip_through_assignment(ad, value):
    ad["key"] = value

    if isinstance(value, dict):
        assert dict(ad["key"]) == value
    else:
        assert ad["key"] == value


@pytest.mark.xfail(reason="[ClassAd pickling] Not currently implemented in version 2.")
@pytest.mark.parametrize("value", [1, "2", classad.ExprTree("2 + 2")])
def test_round_trip_through_pickle(ad, value):
    ad["key"] = value

    pickled_ad = pickle.dumps(ad)
    loaded_ad = pickle.loads(pickled_ad)

    assert ad == loaded_ad


def test_exprtree_repr(ad):
    ad["key"] = classad.ExprTree("2 + 3")

    assert repr(ad["key"]) == "2 + 3"


@pytest.mark.parametrize(
    "expression, expected",
    [
        ("2 + 3", 5),
        ("2 * 3", 6),
        ("2 + 3 == 5", True),
        ("2 + 3 == 6", False),
        (r'regexps("foo (bar)", "foo bar", "\\1")', "bar"),
        ("a", classad.Value.Undefined),  # undefined reference
        ("a + b", classad.Value.Undefined),
    ],
)
def test_exprtree_eval(ad, expression, expected):
    ad["key"] = classad.ExprTree(expression)

    assert ad["key"].eval() == expected


def test_exprtree_eval_with_references(ad,):
    ad["ref"] = 1
    ad["key"] = classad.ExprTree("2 + ref")

    assert ad["key"].eval(ad) == 3


@pytest.mark.parametrize(
    "value",
    [
        1,
        "2",
        True,
        False,
        [1, 2, 3],
        {"a": 1, "b": "wiz"},
        {"outer": ["inner"]},
        classad.Value.Undefined,
        classad.Value.Error,
    ],
)
def test_lookup_always_returns_an_exprtree(ad, value):
    ad["key"] = value

    assert isinstance(ad.lookup("key"), classad.ExprTree)


def test_get_with_present_key(ad):
    ad["key"] = 1
    assert ad.get("key", 0) == 1


def test_get_with_missing_key(ad):
    assert ad.get("key", 0) == 0


def test_get_with_missing_key_and_no_default(ad):
    assert ad.get("key") is None


def test_setdefault_on_present_key(ad):
    ad["key"] = "bar"
    assert ad.setdefault("key", "foo") == "bar"


def test_setdefault_on_missing_key(ad):
    assert ad.setdefault("key", "foo") == "foo"


@pytest.mark.parametrize(
    "other", [{"key": 1}, [("key", 1)], classad.ClassAd({"key": 1})]
)
def test_update_from_blank(ad, other):
    ad.update(other)

    assert ad["key"] == 1


def test_update_overwrites_existing_values(ad):
    ad["key"] = "foo"

    ad.update({"key": "bar"})

    assert ad["key"] == "bar"


def test_abstime_evals_to_datetime():
    # In version 1, this was incorrectly assumed to be in local time.
    assert classad.ExprTree(
        'absTime("2013-11-12T07:50:23+0000")'
    ).eval() == datetime.datetime(
        year=2013, month=11, day=12, hour=7, minute=50, second=23,
        tzinfo=datetime.timezone.utc
    )


def test_reltime():
    assert classad.ExprTree("relTime(5)").eval() == 5


@pytest.mark.parametrize("input, expected", [("foo", '"foo"'), ('"foo', r'"\"foo"')])
def test_quote(input, expected):
    assert classad.quote(input) == expected


@pytest.mark.parametrize("input", ["foo", '"foo', r"\"foo"])
def test_quote_unquote_is_symmetric(input):
    assert classad.unquote(classad.quote(input)) == input


@pytest.mark.xfail(reason="[classad.register()] Not currently implemented in version 2.")
def test_register_custom_function():
    def concatenateLists(list1, list2):
        return list1 + list2

    classad.register(concatenateLists)

    assert classad.ExprTree("concatenateLists({1, 2}, {3, 4})").eval() == [1, 2, 3, 4]


@pytest.mark.xfail(reason="[classad.register()] Not currently implemented in version 2.")
def test_register_with_exception():
    def bad(a, b):
        raise Exception("oops")

    classad.register(bad)

    with pytest.raises(Exception) as e:
        classad.ExprTree("bad(1, 2)").eval()

    assert str(e.value) == "oops"


@pytest.mark.xfail(reason="[classad.register()] Not currently implemented in version 2.")
def test_custom_function_can_see_python_values():
    local = 1

    def add(a):
        return a + local

    classad.register(add)

    assert classad.ExprTree("add(1)").eval() == 2


def test_evaluate_in_context_of_ad_with_matching_key(ad):
    expr = classad.ExprTree("foo")
    ad["foo"] = "bar"

    assert expr.eval(ad) == "bar"


def test_evaluate_in_context_of_ad_without_matching_key(ad):
    expr = classad.ExprTree("foo")
    ad["no"] = "bar"

    assert expr.eval(ad) == classad.Value.Undefined


@pytest.mark.xfail(reason="[classad.Attribute] Not currently implemented in version 2.")
def test_subscript(ad):
    ad["foo"] = [0, 1, 2, 3]
    expr = classad.Attribute("foo")._get(2)

    assert expr.eval(ad) == 2


@pytest.mark.xfail(reason="[classad.Function] Not currently implemented in version 2.")
def test_function():
    expr = classad.Function("strcat", "hello", " ", "world")

    assert expr.eval() == "hello world"


@pytest.mark.xfail(reason="[classad.Attribute] Not currently implemented in version 2.")
def test_flatten(ad):
    ad["bar"] = 1
    expr = classad.Attribute("foo") == classad.Attribute("bar")

    assert ad.flatten(expr).sameAs(classad.ExprTree("foo == 1"))


@pytest.mark.parametrize(
    "left, right, matches",
    [
        (
            classad.ClassAd("[foo = 3]"),
            classad.ClassAd("[requirements = other.foo == 3]"),
            True,
        ),
        (
            classad.ClassAd("[requirements = other.foo == 3]"),
            classad.ClassAd("[foo = 3]"),
            False,
        ),
    ],
)
def test_matches(left, right, matches):
    assert left.matches(right) is matches


@pytest.mark.parametrize(
    "left, right, matches",
    [
        (
            classad.ClassAd("[foo = 3]"),
            classad.ClassAd("[requirements = other.foo == 3]"),
            False,
        ),
        (
            classad.ClassAd("[requirements = other.foo == 3]"),
            classad.ClassAd("[foo = 3]"),
            False,
        ),
        (
            classad.ClassAd("[requirements = other.foo == 3]"),
            classad.ClassAd("[requirements = other.bar == 1; foo = 3]"),
            False,
        ),
        (
            classad.ClassAd("[requirements = other.foo == 3; bar = 1]"),
            classad.ClassAd("[requirements = other.bar == 1; foo = 3]"),
            True,
        ),
    ],
)
def test_symmetric_match(left, right, matches):
    assert left.symmetricMatch(right) is matches


def test_internal_refs(ad):
    ad["bar"] = 2
    expr = classad.ExprTree("foo =?= bar")

    assert ad.internalRefs(expr) == ["bar"]


def test_external_refs(ad):
    ad["bar"] = 2
    expr = classad.ExprTree("foo =?= bar")

    assert ad.externalRefs(expr) == ["foo"]


@pytest.mark.xfail(reason="[ExprTree.__int__()] Not currently implemented in version 2.")
@pytest.mark.parametrize(
    "expr, value",
    [
        (classad.ExprTree("1 + 3"), 4),
        (classad.ExprTree("1.0 + 3.4"), 4),
        (classad.ExprTree("1.0 + 3.5"), 4),
        (classad.ExprTree("1.0 + 3.6"), 4),
        (classad.ExprTree('"34"'), 34),
    ],
)
def test_int(expr, value):
    assert int(expr) == value


@pytest.mark.xfail(reason="[ExprTree.__int__()] Not currently implemented in version 2.")
@pytest.mark.parametrize(
    "expr",
    [
        classad.ExprTree("foo"),
        classad.ExprTree('"34.foo"'),
        classad.Value.Undefined,
        classad.Value.Error,
    ],
)
def test_int_with_bad_values(expr):
    with pytest.raises(ValueError):
        print(int(expr))


@pytest.mark.xfail(reason="[ExprTree.__float__()] Not currently implemented in version 2.")
@pytest.mark.parametrize(
    "expr, value",
    [(classad.ExprTree("1.0 + 3.5"), 4.5), (classad.ExprTree("35.5"), 35.5)],
)
def test_float(expr, value):
    assert float(expr) == value


@pytest.mark.xfail(reason="[ExprTree.__float__()] Not currently implemented in version 2.")
@pytest.mark.parametrize(
    "expr",
    [
        classad.ExprTree("foo"),
        classad.ExprTree('"34.foo"'),
        classad.Value.Undefined,
        classad.Value.Error,
    ],
)
def test_float_with_bad_values(expr):
    with pytest.raises(ValueError):
        print(int(expr))


@pytest.fixture(
    params=[
        # two old-style ads
        """
        foo = "bar"

        foo = "wiz"

        /* this is a comment */
        """,
        # two new-style adsd
        """
        [
        foo = "bar";
        ]
        [
        foo = "wiz";
        ]
        /* this is a comment */
        """,
    ],
    ids=["old-style", "new-style"],
)
def ad_string(request):
    return textwrap.dedent(request.param).strip()


@pytest.fixture
def ad_file(tmp_path, ad_string):
    ad_file = tmp_path / "ad"
    ad_file.write_text(ad_string)

    return ad_file


def test_parse_ads_from_string(ad_string):
    ads = list(classad.parseAds(ad_string))

    assert ads[0]["foo"] == "bar"
    assert ads[1]["foo"] == "wiz"


def test_parse_ads_from_file_like_object(ad_file):
    ads = list(classad.parseAds(ad_file.open(mode="r")))

    assert ads[0]["foo"] == "bar"
    assert ads[1]["foo"] == "wiz"


@pytest.mark.xfail(reason="This won't work because there's no 'file pointer' to move")
def test_parse_next_ad_from_string(ad_string):
    ads = [classad.parseNext(ad_string), classad.parseNext(ad_string)]

    assert ads[0]["foo"] == "bar"
    assert ads[1]["foo"] == "wiz"


def test_parse_next_ad_from_file_like_object(ad_file):
    with ad_file.open(mode="r") as f:
        ads = [classad.parseNext(f), classad.parseNext(f)]

    assert ads[0]["foo"] == "bar"
    assert ads[1]["foo"] == "wiz"


def test_parse_one_ad_from_string(ad_string):
    ad = classad.parseOne(ad_string)

    assert ad["foo"] == "wiz"


def test_parse_one_ad_from_file_like_object(ad_file):
    ad = classad.parseOne(ad_file.open(mode="r"))

    assert ad["foo"] == "wiz"


@pytest.mark.parametrize(
    "ad_string, parser",
    [("[foo = 1]", classad.Parser.New), ("foo = 1", classad.Parser.Old)],
)
def test_can_parse_ads_across_pipes(ad_string, parser):
    """
    The parser must be set manually. Auto-discovery won't work on a pipe because
    it can't rewind using seek.
    """
    r, w = os.pipe()

    with open(w, mode="w") as wf:
        wf.write(ad_string)

    with open(r, mode="r") as rf:
        ad = classad.parseNext(rf, parser)

    assert ad["foo"] == 1


def test_output_of_printJson_can_be_parsed(ad):
    ad["int"] = 5
    ad["float"] = 6.5
    ad["string"] = "foo"
    ad["expr"] = classad.ExprTree("2 + 3")
    ad["undefined"] = classad.Value.Undefined
    ad["error"] = classad.Value.Error

    j = json.loads(ad.printJson())
    print(j)

    assert j == {
        "int": 5,
        "float": 6.5,
        "string": "foo",
        "expr": "/Expr(2 + 3)/",
        "undefined": None,
        "error": "/Expr(error)/",
    }
