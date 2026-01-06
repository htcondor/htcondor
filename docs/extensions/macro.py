import os
import sys
import re
from pathlib import Path

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

SPECIAL_CASE_KNOBS = ["<SUBSYS>"]
TEMPLATE_FILE = "introduction-to-configuration"

CONFIG_REGEX = {}
CONFIG_KNOBS = {}
TEMPLATES = {}

def find_conf_knobs(dir: str):
    knobs = {}
    regex_map = {}

    base = Path(dir)
    config_dir = base / "admin-manual" / "configuration"

    for path in config_dir.iterdir():
        if path.suffix != ".rst":
            continue

        url = path.relative_to(base).with_suffix(".html")
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                for knob in get_all_defined_role("macro-def", line):
                    if "[" in knob:
                        info = knob.find("[")
                        knob = knob[:info]
                    # Store macro for warning check
                    # Handle special case knobs
                    if knob in SPECIAL_CASE_KNOBS:
                        knobs[knob] = url
                    # Check if knob requires regex matching
                    elif "*" in knob:
                        regex = rf"{knob.replace('*', '(.+)')}"
                        regex_map.update({regex : (knob, url)})
                    elif "<" in knob:
                        temp = knob
                        while "<" in temp:
                            start = temp.find("<")
                            end = temp.find(">")
                            temp = regex = rf"{temp.replace(temp[start:end+1], '(.+)')}"
                        # Don't allow (.+) to be a regex match option
                        if regex != "(.+)":
                            regex_map.update({regex : (knob, url)})
                    # Store whatever is left (if not already included)
                    else:
                        knobs[knob] = url

    sorted_regex_map = {k: regex_map[k] for k in sorted(regex_map.keys(), reverse=True)}

    return (knobs, sorted_regex_map)

def find_templates(dir: str):
    templates = {}
    intro_file = Path(dir) / "admin-manual" / f"{TEMPLATE_FILE}.rst"
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

def generate_old_redirect_page(dir: str):
    """Write javascript to redirect old monolithic configuration macro URLs to new locations"""
    javascript = Path(dir) / "_static" / "js" / "config-macro-redirect.js"
    with open(javascript, "w") as f:
        f.write("""
// Map of tag -> URL end
const configuration = new Map([
""")

        for macro, path in CONFIG_KNOBS.items():
            f.write(f'\t["{macro.upper()}", "{path}"],\n')

        f.write("""]);


const config_regex = new Map([
""")

        for regex, info in CONFIG_REGEX.items():
            f.write(f'\t[/{regex.upper()}/, "{info[1]}"],\n')

        f.write("""]);

// Magic Redirect function
function configRedirect() {
	var end = window.location.href.indexOf("admin-manual");
	var url_root = window.location.href.substring(0, end);
	var redirect_url = url_root + "admin-manual/configuration/index.html";

	if (window.location.hash.length > 0) {
		var macro = decodeURI(window.location.hash.slice(1)).toUpperCase();

		if (configuration.has(macro)) {
			// Specific macro found
			redirect_url = url_root + configuration.get(macro) + "#" + macro;
		} else {
			// Macro matches regex configuration option
			for (const check of config_regex.keys()) {
				if (check.test(macro)) {
					redirect_url = url_root + config_regex.get(check) + "#" + macro;
					break;
				}
			}
		}
	}

	window.location.replace(redirect_url);
}

window.addEventListener('DOMContentLoaded', configRedirect());
""")

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
        specifier, macro = macro_name.split(".", 1) if "." in macro_name else (None, macro_name)
        ref = macro
        if macro not in CONFIG_KNOBS.keys():
            regex_match = False
            for r in CONFIG_REGEX.keys():
                if re.match(r, macro, flags=re.IGNORECASE):
                    regex_match = True
                    ref, url_path = CONFIG_REGEX[r]
                    break
            # If here then not in pure defined list or matched a recorded regex
            if not regex_match:
                docname = inliner.document.settings.env.docname
                warn(f"{docname}:{lineno} | Config knob '{macro}' not found in defined list. Either a typo or knob needs definition.")
        ref_link = f"href=\"{root_dir}/{url_path}#" + str(ref) + "\""
    return make_ref_and_index_nodes(name, macro_name, macro_index,
                                    ref_link, rawtext, inliner, lineno, options)

def setup(app):
    global CONFIG_KNOBS
    global CONFIG_REGEX
    global TEMPLATES

    CONFIG_KNOBS, CONFIG_REGEX = find_conf_knobs(app.srcdir)
    TEMPLATES = find_templates(app.srcdir)

    generate_old_redirect_page(app.srcdir)

    app.add_role("macro", macro_role)

