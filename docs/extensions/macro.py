import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app
    docname = inliner.document.settings.env.docname
    is_macro_indexed = False
    macro_record = str(text) + "," + docname + "\n"

    # Check if this macro is already in the reference index. If so, don't add it again.
    with open("reference-index.csv", "r") as index_file:
        index_data = index_file.read()
    index_file.close()
    if index_data.find(macro_record) >= 0:
        is_macro_indexed = True

    if is_macro_indexed is False:
        index_file = open("reference-index.csv", "a")
        index_file.write(macro_record)
        index_file.close()

    node = nodes.raw("", "<a name=\"" + str(text) + "\"></a><b>" + str(text) + "</b>", format="html")
    return [node], []

def setup(app):
    app.add_role("macro", macro_role)

