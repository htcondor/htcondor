# Copyright 2019 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from typing import Iterable, TypeVar, List, Callable, Optional

import logging

import itertools

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


T = TypeVar("T")


def in_order(items: Iterable[T], expected: Iterable[T]) -> bool:
    """
    .. attention::
        This function never asserts on its own! You must assert some condition
        on its return value.

    Given an iterable of items and a list of expected items, return ``True``
    if and only if the items occur in exactly the given order. Extra items may
    appear between expected items, but expected items may not appear out of order.

    When this function returns ``False``, it emits a detailed log message at
    ERROR level showing a "match" display for debugging.

    Parameters
    ----------
    items
        The items to check.
    expected
        The expected order of some subset of the items.

    Returns
    -------
    result : bool
        ``True`` if the items were ordered as expected, ``False`` otherwise.
    """
    items = iter(items)
    items, backup = itertools.tee(items)

    expected = list(expected)
    expected_set = set(expected)
    next_expected_idx = 0

    found_at = {}

    for found_idx, item in enumerate(items):
        if item not in expected_set:
            continue

        if item == expected[next_expected_idx]:
            found_at[found_idx] = expected[next_expected_idx]
            next_expected_idx += 1

            if next_expected_idx == len(expected):
                # we have seen all the expected events
                return True
        else:
            break

    backup = list(backup)
    msg_lines = ["Items were not in the correct order:"]
    for idx, item in enumerate(backup):
        msg = " {:6}  {}".format(idx, item)

        maybe_found = found_at.get(idx)
        if maybe_found is not None:
            msg = "*" + msg[1:]
            msg += "  MATCHED  {}".format(maybe_found)

        msg_lines.append(msg)

    logger.error("\n".join(msg_lines))
    return False


def track_quantity(
    items: Iterable[T],
    increment_condition: Callable[[T], bool],
    decrement_condition: Callable[[T], bool],
    initial_quantity: int = 0,
    min_quantity: Optional[int] = None,
    max_quantity: Optional[int] = None,
    expected_quantity: Optional[int] = None,
) -> List[int]:
    """
    .. attention::
        This function never asserts on its own! You must assert some condition
        on its return value.

    Given an iterable of items, track the value of a "quantity" as it changes
    over the items. The increment and decrement conditions are given as functions
    that will receive a single item as their argument and should return a boolean.
    If the increment (decrement) condition returns ``True`` for an item,
    the quantity is incremented (decremented).

    Both conditions are checked for every item, so an item may cause the quantity
    to both increment and decrement (in which case it would be reported as staying
    the same).

    The parameters ``min_quantity``, ``max_quantity``, and ``expected_quantity``
    do not effect the return value, but they can cause an ERROR-level logging
    message to be printed when their condition is not satisfied at the end of tracking.
    The message shows the item-by-item tracking of the quantity.

    Parameters
    ----------
    items
        The items to track a quantity over.
    increment_condition
        If this condition evaluates to ``True`` on an item, the quantity will be incremented.
    decrement_condition
        If this condition evaluates to ``True`` on an item, the quantity will be decremented.
    min_quantity
        If given, and the quantity is ever lower than this value, print the debug message.
    max_quantity
        If given, and the quantity is ever higher this value, print the debug message.
    expected_quantity
        If given, and does not appear in the quantity history, print the debug message.

    Returns
    -------
    history : List[int]
        The "history" of the tracked quantity, one element for each entry in the items.
    """
    quantity_history = []
    quantity_current = initial_quantity

    msg_lines = ["A quantity tracking condition was not satisfied:"]

    for item in items:
        if increment_condition(item):
            quantity_current += 1
        if decrement_condition(item):
            quantity_current -= 1

        quantity_history.append(quantity_current)

        msg_lines.append(
            "{} {}/{} | {}".format(
                "*"
                if len(quantity_history) > 2
                and quantity_history[-1] != quantity_history[-2]
                else " ",
                str(quantity_current).rjust(2),
                max_quantity or "-",
                str(item),
            )
        )

    if any(
        (
            (min_quantity is not None and min(quantity_history) < min_quantity),
            (max_quantity is not None and max(quantity_history) > max_quantity),
            (
                expected_quantity is not None
                and expected_quantity not in quantity_history
            ),
        )
    ):
        logger.error("\n".join(msg_lines))

    return quantity_history
