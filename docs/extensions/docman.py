import re

from docutils import nodes
from sphinx import addnodes
from sphinx.util.nodes import split_explicit_title
from htc_helpers import warn

_SECTION_RE = re.compile(r'^(.*)\((\d+)\)$')

def docman_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    """
    Role that emits a :doc: cross-reference for HTML builders and a
    :manpage: node for man-page builders.

    Usage::

        :docman:`condor_submit`           -> doc link / condor_submit(1)
        :docman:`condor_submit(5)`        -> doc link / condor_submit(5)
        :docman:`title <condor_submit(5)>` -> explicit title in both outputs
    """
    env = inliner.document.settings.env
    has_title, title, target = split_explicit_title(text)

    # Strip optional section number from target
    section = "1"
    doc_target = target
    m = _SECTION_RE.match(target)
    if m:
        doc_target, section = m.group(1), m.group(2)

    display = title if has_title else doc_target

    if env.app.builder.name == 'man':
        manpage_ref = f"{doc_target}({section})"
        node = addnodes.manpage(rawtext, manpage_ref,
                                manpage=doc_target,
                                section=section)
        return [node], []

    # HTML and all other builders: emit a pending :doc: cross-reference
    ref = addnodes.pending_xref(rawtext,
                                reftype='doc',
                                refdomain='std',
                                reftarget=doc_target,
                                refexplicit=has_title)
    ref += nodes.inline(rawtext, display, classes=['xref', 'doc'])
    return [ref], []


def setup(app):
    app.add_role('docman', docman_role)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}
