import pytest

import pickle
import datetime

import classad


# this test replicates python_bindings_classad.run


@pytest.fixture
def ad():
    """A fresh, blank Python ClassAd."""
    return classad.ClassAd()


def ads_are_identical(ad1, ad2):
    """To test classad equality, we need to convert them into dictionaries."""
    return dict(ad1.items()) == dict(ad2.items())


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

    assert ad["key"] == value


@pytest.mark.parametrize("value", [1, "2", classad.ExprTree("2 + 2")])
def test_round_trip_through_pickle(ad, value):
    ad["key"] = value

    pickled_ad = pickle.dumps(ad)
    loaded_ad = pickle.loads(pickled_ad)

    assert ads_are_identical(ad, loaded_ad)


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


def test_exprtree_eval_with_references(ad, ):
    ad["ref"] = 1
    ad["key"] = classad.ExprTree("2 + ref")

    assert ad["key"].eval() == 3


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
    dt = classad.ExprTree('absTime("2013-11-12T07:50:23")').eval()

    assert dt == datetime.datetime(
        year = 2013, month = 11, day = 12, hour = 7, minute = 50, second = 23
    )


def test_reltime():
    assert classad.ExprTree("relTime(5)").eval() == 5


@pytest.mark.parametrize("input, expected", [("foo", '"foo"'), ('"foo', r'"\"foo"')])
def test_quote(input, expected):
    assert classad.quote(input) == expected


@pytest.mark.parametrize("input", ["foo", '"foo', r'\"foo'])
def test_quote_unquote_is_symmetric(input):
    assert classad.unquote(classad.quote(input)) == input


def test_register_custom_function():
    def concatenateLists(list1, list2):
        return list1 + list2

    classad.register(concatenateLists)

    assert classad.ExprTree("concatenateLists({1, 2}, {3, 4})").eval() == [1, 2, 3, 4]
