import os

from sphinx.util.docutils import SphinxDirective
from docutils.parsers.rst import directives
from docutils import nodes

from htc_helpers import warn

HIDE_ENV_VAR = "SPHINX_HIDE_HIDDEN_SECTIONS"

class HiddenSection(SphinxDirective):
    """Custom directive to hide documentation in offical build but not local"""
    optional_arguments = 1 # Allow a specified version history to no longer be hidden
    has_content = True # Allow additional content (to hide)
    option_spec = {"comment" : directives.flag} # Allow :comment: option to always hide

    # Note returns list[nodes.Node]
    def run(self) -> list:
        """Execute this directive"""
        if "comment" in self.options:
            return []

        # Check the current release versrion against labeled release version
        if len(self.arguments) > 0:
            RELEASE_VERSION = tuple(int(x) for x in self.config.release.split("."))
            version = tuple(int(x) for x in self.arguments[0].split("."))
            if version <= RELEASE_VERSION:
                warn(f"{self.env.docname} @ {self.lineno} | Hidden section labeled for v{self.arguments[0]} release... currently v{self.config.release}")

        info = list()

        # Check if we should actually parse the contents for display or hide
        if os.getenv(HIDE_ENV_VAR, "False") not in ["True", "true", "1"]:
            text = '\n'.join(self.content)
            node = nodes.container(text)
            self.add_name(node)
            self.state.nested_parse(self.content, self.content_offset, node)
            info.append(node)

        return info

def setup(app):
    """Setup sphinx to know of this directive"""
    app.add_directive('hidden', HiddenSection)
