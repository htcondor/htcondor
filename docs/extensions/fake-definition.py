import os

from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives
from docutils import nodes

class FakeDefinition(Directive):
    """Custom directive to be used as filler for multi-term definitions"""
    optional_arguments = 0
    has_content = False

    def run(self) -> list:
        node = nodes.inline("", "", classes=['faux-definition'])
        return [node]

def setup(app):
    """Setup sphinx to know of this directive"""
    app.add_directive('faux-definition', FakeDefinition)
    app.add_directive('fake-definition', FakeDefinition)
