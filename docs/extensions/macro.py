import os
import sys
import re

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

SPECIAL_CASE_KNOBS = ["<SUBSYS>"]
TEMPLATE_FILE = "introduction-to-configuration"

# List of files that define macros with :macro-def:
CONFIG_FILES = [
    ["admin-manual", "configuration-macros.rst"],
    ["cloud-computing", "annex-configuration.rst"],
]

CONFIG_REGEX = {}
CONFIG_KNOBS = {}
TEMPLATES = {}

def find_conf_knobs(dir: str):
    knobs = {}
    regex_map = {}
    for file_path in CONFIG_FILES:
        url_path = "/".join(file_path).replace(".rst", ".html")
        definition_file = os.path.join(dir, *file_path)
        with open(definition_file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                for knob in get_all_defined_role("macro-def", line):
                    if "[" in knob:
                        info = knob.find("[")
                        knob = knob[:info]
                    # Store macro for warning check
                    # Handle special case knobs
                    if knob in SPECIAL_CASE_KNOBS:
                        knobs[knob] = url_path
                    # Check if knob requires regex matching
                    elif "*" in knob:
                        regex = rf"{knob.replace('*', '(.+)')}"
                        regex_map.update({regex : (knob, url_path)})
                    elif "<" in knob:
                        temp = knob
                        while "<" in temp:
                            start = temp.find("<")
                            end = temp.find(">")
                            temp = regex = rf"{temp.replace(temp[start:end+1], '(.+)')}"
                        # Don't allow (.+) to be a regex match option
                        if regex != "(.+)":
                            regex_map.update({regex : (knob, url_path)})
                    # Store whatever is left (if not already included)
                    else:
                        knobs[knob] = url_path
    return (knobs, regex_map)

def find_templates(dir: str):
    templates = {}
    intro_file = os.path.join(dir, "admin-manual", f"{TEMPLATE_FILE}.rst")
    with open(intro_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if "config-template" in line:
                start = line.find("`") + 1
                cat_start = line.find("<")
                cat_end = line.rfind(">")
                if cat_start == -1:
                    end = line.find("`")
                    category = line[start:end]
                    templates.update({category : []})
                else:
                    category = line[cat_start + 1:cat_end]
                    end = line.find("(") if "(" in line else cat_start
                    template = line[start:end].upper()
                    if category not in templates.keys():
                        templates.update({category : [template]})
                    else:
                        templates[category].append(template)
    return templates

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def macro_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    root_dir = get_rel_path_to_root_dir(inliner)[:-1]
    macro_name, macro_index = custom_ext_parser(text)

    # Handle reference to configuration template
    if macro_name.strip().lower()[:4] == "use ":
        info = macro_name[4:].strip()
        if ":" in info:
            category, template = info.split(":")[:2]
            category = category.strip().upper()
            template = template.strip().upper()
            if category not in TEMPLATES.keys():
                warn(f"Config template category '{category}' is not defined or a typo exists.")
            elif template not in TEMPLATES[category]:
                warn(f"Config template '{category}:{template}' is not defined or a typo exists.")
            ref = category + ":" + template
        else:
            ref = info.upper()
            if ref not in TEMPLATES.keys():
                warn(f"Config template category '{ref}' is not defined or a typo exists.")
        ref_link = f"href=\"{root_dir}/admin-manual/{TEMPLATE_FILE}.html#" + str(ref) + "\""
    # Handle reference to normal configuration knob
    else:
        url_path = CONFIG_KNOBS.get(macro_name, "admin-manual/configuration-macros.html")
        ref = macro_name
        if macro_name not in CONFIG_KNOBS.keys():
            regex_match = False
            for r in CONFIG_REGEX.keys():
                if re.match(r, macro_name):
                    regex_match = True
                    ref, url_path = CONFIG_REGEX[r]
            # If here then not in pure defined list or matched a recorded regex
            if not regex_match:
                docname = inliner.document.settings.env.docname
                warn(f"{docname}:{lineno} | Config knob '{macro_name}' not found in defined list. Either a typo or knob needs definition.")
        ref_link = f"href=\"{root_dir}/{url_path}#" + str(ref) + "\""
    return make_ref_and_index_nodes(name, macro_name, macro_index,
                                    ref_link, rawtext, inliner, lineno, options)

def setup(app):
    global CONFIG_KNOBS
    global CONFIG_REGEX
    global TEMPLATES
    CONFIG_KNOBS, CONFIG_REGEX = find_conf_knobs(app.srcdir)
    TEMPLATES = find_templates(app.srcdir)
    app.add_role("macro", macro_role)

