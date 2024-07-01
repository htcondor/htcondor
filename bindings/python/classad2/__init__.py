#
# Python > 3.8 on Windows doesn't use PATH to find DLLs any more,
# which breaks everything.
#
def _add_dll_dir():
    import platform as _platform

    if _platform.system() in ["Windows"]:
        import sys as _sys
        import os as _os
        from os import path as _path

        if _sys.version_info >= (3,8):
            bin_path = _path.realpath(_path.join(__file__, r'..\..\..\..\bin'))
            return _os.add_dll_directory(bin_path)
    else:
        from contextlib import contextmanager as _contextmanager
        @_contextmanager
        def _noop():
            yield None

        return _noop()


with _add_dll_dir():
    from ._class_ad import ClassAd
    # This should not be part of the API...
    from ._class_ad import _convert_local_datetime_to_utc_ts
    from ._value import Value
    from ._expr_tree import ExprTree
    from ._parser_type import ParserType

    from ._class_ad import _parseAds as parseAds
    from ._class_ad import _parseOne as parseOne
    from ._class_ad import _parseNext as parseNext
    from ._class_ad import _quote as quote
    from ._class_ad import _unquote as unquote
    from ._class_ad import _lastError as lastError
    from ._class_ad import _version as version

    # For compability with version 1.
    from ._parser_type import ParserType as Parser
