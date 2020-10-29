#!/usr/bin/env pytest

import pytest

import classad
import htcondor

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def evaluate_expr(expr):
    e = classad.ExprTree(expr)
    return e.eval()


EQUALITIES = [
    ("7", "7", True),
    ("7", "8", False),
    ("7", "7.0", False),
    ("7", "7.0.0", False),

    ("7.0", "7.0", True),
    ("7.0", "8.0", False),
    ("7.0", "7.1", False),
    ("7.0", "8.1", False),
    ("7.0", "7.0.1", False),
    ("7.0", "8.0.1", False),

    ("7.0.0", "7.0.0", True),
    ("7.0.0", "8.0.0", False),
    ("7.0.0", "8.1.0", False),
    ("7.0.0", "8.0.1", False),
    ("7.0.0", "8.1.1", False),
    ("7.0.0", "7.0.1", False),
    ("7.0.0", "7.1.0", False),
    ("7.0.0", "7.1.1", False),
]

@pytest.mark.parametrize("left, right, expected", EQUALITIES)
def test_equality_operator(left, right, expected):
    expr = 'versionEQ( "{}", "{}" )'.format(left, right)
    assert evaluate_expr(expr) == expected

    expr = 'versionEQ( "{}", "{}" )'.format(right, left)
    assert evaluate_expr(expr) == expected


LT = [
    ("versionLT", "7", "8", True),
    ("versionLT", "7", "8.0", True),
    ("versionLT", "7", "8.0.0", True),

    ("versionLT", "7.0", "8", True),
    ("versionLT", "7.0", "8.0", True),
    ("versionLT", "7.0", "8.0.0", True),

    ("versionLT", "7.0.0", "8", True),
    ("versionLT", "7.0.0", "8.0", True),
    ("versionLT", "7.0.0", "8.0.0", True),

    ("versionLT", "7", "7.1", True),
    ("versionLT", "7", "7.0.1", True),

    ("versionLT", "7.0", "7.1", True),
    ("versionLT", "7.0", "7.1.0", True),
    ("versionLT", "7.0", "7.0.1", True),

    ("versionLT", "7.0.0", "7.0.1", True),
    ("versionLT", "7.0.0", "7.1.0", True),
]

LT_NC = [
    ("versionLT", "7", "7", False),
    ("versionLT", "7.0", "7.0", False),
    ("versionLT", "7.0.0", "7.0.0", False),
]

GT = [("versionGT", right, left, expected) for (operator, left, right, expected) in LT]
GT_NC = [("versionGT", right, left, expected) for (operator, left, right, expected) in LT_NC]

LE = [("versionLE", left, right, expected) for (operator, left, right, expected) in LT]
LE_NC = [("versionLE", left, right, not(expected)) for (operator, left, right, expected) in LT_NC]

GE = [("versionGE", left, right, expected) for (operator, left, right, expected) in GT]
GE_NC = [("versionGE", left, right, not(expected)) for (operator, left, right, expected) in GT_NC]

def swap_left_right_and_invert(list_of_tuples):
    return [(operator, right, left, not(expected)) for (operator, left, right, expected) in list_of_tuples]

INEQUALITIES = []

INEQUALITIES.extend(LT)
INEQUALITIES.extend(swap_left_right_and_invert(LT))
INEQUALITIES.extend(LT_NC)

INEQUALITIES.extend(GT)
INEQUALITIES.extend(swap_left_right_and_invert(GT))
INEQUALITIES.extend(GT_NC)

INEQUALITIES.extend(LE)
INEQUALITIES.extend(swap_left_right_and_invert(LE))
INEQUALITIES.extend(LE_NC)

INEQUALITIES.extend(GE)
INEQUALITIES.extend(swap_left_right_and_invert(GE))
INEQUALITIES.extend(GE_NC)


@pytest.mark.parametrize("operator, left, right, expected", INEQUALITIES)
def test_inequality_operators(operator, left, right, expected):
    expr = '{}( "{}", "{}" )'.format(operator, left, right)
    assert evaluate_expr(expr) == expected


RANGES = [
    ("7", "8", "9", True),
    ("9", "8", "7", False),

    # FIXME
]

@pytest.mark.parametrize("low, version, high, expected", RANGES)
def test_version_in_range(low, version, high, expected):
    expr = 'version_in_range("{}", "{}", "{}")'.format(version, low, high)
    assert evaluate_expr(expr) == expected


@pytest.mark.parametrize("left, right, expected", EQUALITIES)
def test_versioncmp_equality(left, right, expected):
    expr = 'versioncmp( "{}", "{}" )'.format(left, right)
    assert (evaluate_expr(expr) == 0) == expected

CMP_INEQUALITIES = []

CMP_INEQUALITIES.extend([
    * [(left, right, -1) for (operator, left, right, expected) in LT],
    * [(left, right, 1) for (operator, left, right, expected) in swap_left_right_and_invert(LT)],
    * [(left, right, 0) for (operator, left, right, expected) in LT_NC],

    * [(left, right, 1) for (operator, left, right, expected) in GT],
    * [(left, right, -1) for (operator, left, right, expected) in swap_left_right_and_invert(GT)],
    * [(left, right, 0) for (operator, left, right, expected) in GT_NC],

    * [(left, right, -1) for (operator, left, right, expected) in LE],
    * [(left, right, 1) for (operator, left, right, expected) in swap_left_right_and_invert(LE)],
    * [(left, right, 0) for (operator, left, right, expected) in LE_NC],

    * [(left, right, 1) for (operator, left, right, expected) in GE],
    * [(left, right, -1) for (operator, left, right, expected) in swap_left_right_and_invert(GE)],
    * [(left, right, 0) for (operator, left, right, expected) in GE_NC],
])

@pytest.mark.parametrize("left, right, expected", CMP_INEQUALITIES)
def test_versioncmp_inequality(left, right, expected):
    expr = 'versioncmp( "{}", "{}" )'.format(left, right)
    rv = evaluate_expr(expr)
    if rv < 0:
        rv = -1
    if rv > 0:
        rv = 1
    assert rv == expected


def test_versioncmp_example():
    sequence = ['000', '00', '01', '010', '09', '0', '1', '9', '10']
    for i,left in enumerate(sequence[:-1]):
        for right in sequence[i+1:]:
            assert evaluate_expr('versionLT("{}","{}")'.format(left,right))
