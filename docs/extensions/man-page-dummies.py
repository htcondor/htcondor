import os

from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives
from docutils import nodes

from htc_helpers import warn

HIDE_ENV_VAR = "SPHINX_HIDE_HIDDEN_SECTIONS"

class DummyDirective(Directive):
    """Dummy Directive to use for man pages"""
    optional_arguments = 10
    has_content = True
    option_spec = {
        "caption" : directives.unchanged,
        "align" : directives.unchanged,
    }

    def run(self) -> list:
        return []

def setup(app):
    """Setup sphinx to know of this directive"""
    assert os.getenv("MANPAGES", "False") in ["True", "true", "1"]
    app.add_directive('mermaid', DummyDirective)
