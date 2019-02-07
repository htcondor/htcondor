import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive

class ReferenceIndex(Directive):
    required_arguments = 0
    def run(self):
        # Open the input file
        with open("reference-index.csv", "r") as index_file:
            index_lines = index_file.readlines()
        index_file.close()
        index_lines = sorted(index_lines)

        index_html = "<ul>"
        for line in index_lines:
            line_data = line.split(",")
            macro_name = line_data[0]
            macro_url = line_data[1].replace("\n", "")
            index_html += "<li><b>" + macro_name + "</b> [<a href=\"" + macro_url + ".html#" + macro_name + "\">" + macro_url + "</a>]</li>"
        index_html += "</ul>"

        index_node = nodes.raw("", index_html, format="html")
        return [index_node]

def setup(app):
    app.add_directive("reference-index", ReferenceIndex)

