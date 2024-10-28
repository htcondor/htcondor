import os
from docutils.parsers.rst import Directive
from docutils import nodes

class MultiTabCodeBlocks(Directive):
    """
    This directive is intended to be used to contain a group of
    code blocks that show variations of something. i.e:
        - python code/c++ code
        - Some output V1/V2
    When rendered as HTML the the examples will all be rolled up
    into a single display area with buttons to select between the different
    languages.
    """

    has_content = True

    def run(self):
        self.assert_has_content()
        text = '\n'.join(self.content)
        node = nodes.container(text)
        node['classes'].append('example-code')
        self.add_name(node)
        self.state.nested_parse(self.content, self.content_offset, node)
        return [node]


def setup(app):
    app.add_directive('multi-tab',  MultiTabCodeBlocks)
