import functools


def wraps(func):
    """
    A wrapper around functools.wraps that is safe for use with Boost-defined functions.
    """
    # assigned protects us from accessing attributes of the function object
    # that may not exist inside functools.wraps, which can fail on 2.7
    # Compare https://github.com/python/cpython/blob/2.7/Lib/functools.py#L33 to
    #         https://github.com/python/cpython/blob/3.7/Lib/functools.py#L53
    assigned = (a for a in functools.WRAPPER_ASSIGNMENTS if hasattr(func, a))

    return functools.wraps(func, assigned=assigned)
