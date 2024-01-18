# Load htcondor_cli nouns from third-party plugins
import warnings

try:
    from importlib.metadata import entry_points
except ModuleNotFoundError:  # python < 3.9
    from importlib_metadata import entry_points

__author__ = "Duncan Macleod <duncan.macleod@ligo.org>"


def load_entry_points(group="htcondor_cli", name=None):
    """Load all entry points belonging to the given group.

    Returns a `dict` of `(name, object)` pairs for each plugin
    that matches the `group` (and optionally `name` attributes).

    A `UserWarning` will be emitted if a plugin fails to load
    (for any reason).
    """
    plugins = {}
    for entry_point in entry_points(group=group):
        if name is not None and entry_point.name != name:
            continue
        try:
            plugin = entry_point.load()
        except Exception as exc:
            warnings.warn(
                "Error while loading htcondor_cli entry point: "
                f"{entry_point.name} ({exc})",
            )
            continue
        plugins[entry_point.name] = plugin
    return plugins
