#!/usr/bin/env python3

import os
import json
from pathlib import Path
import shutil
import argparse
from copy import copy, deepcopy

import jinja2 as j


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description = 'Build HTML for HTCondor View',
    )

    parser.add_argument(
        '-i', '--input',
        action = 'store',
        default = 'html',
        type = str,
        help = 'the directory that the HTML templates are in',
    )
    parser.add_argument(
        '-o', '--output',
        action = 'store',
        default = 'view',
        type = str,
        help = 'the directory to place the built HTML in',
    )
    parser.add_argument(
        '-b', '--base',
        action = 'store',
        default = '/view/',
        type = str,
        help = 'the base name of the website',
    )

    args = parser.parse_args()

    args.input = Path(args.input)
    args.output = Path(args.output)

    return args


def walk_sitemap(sitemap):
    for name, template_path in sitemap.items():
        if not isinstance(template_path, str):
            for kk, vv in template_path.items():
                template_path = '/'.join([name.replace(' ', '_'), vv.lstrip('/')])
                yield name, template_path
        else:
            yield name, template_path


def ensure_parents_exist(path: Path):
    Path(path).parent.mkdir(parents = True, exist_ok = True)


def get_linkmap(sitemap):
    linkmap = deepcopy(sitemap)

    for k, v in sitemap.items():
        if not isinstance(v, str):
            for kk, vv in v.items():
                vv = vv.lstrip('/')
                linkmap[k][kk] = '/'.join((k, vv)).replace(' ', '_').lower()
                if '#' in kk:
                    linkmap[k].pop(kk)
        elif '#' in k:
            linkmap.pop(k)

    return linkmap


def clean_build_dir(build_dir):
    """clean the build directory, but don't delete the data!"""
    for path in Path.iterdir(build_dir):
        if path.match('data'):
            continue

        if path.is_dir():
            print(f'deleted dir {path}')
            shutil.rmtree(path, ignore_errors = False)
        else:
            print(f'deleted file {path}')
            path.unlink()


def build(args, sitemap):
    clean_build_dir(build_dir = args.output)

    env = j.Environment(
        loader = j.FileSystemLoader([args.input]),
        autoescape = j.select_autoescape(['html', 'xml']),
    )

    custom = Path('custom')
    default = Path('default')

    linkmap = get_linkmap(sitemap)

    for k, v in walk_sitemap(sitemap):
        v = v.lower().replace(' ', '_')
        try:
            template = env.get_template(str(custom / v))
        except j.exceptions.TemplateNotFound:
            template = env.get_template(str(default / v))
        output_path = args.output / v
        ensure_parents_exist(output_path)
        with output_path.open(mode = 'w') as f:
            f.write(
                template.render(
                    linkmap = linkmap,
                    base = args.base,
                )
            )
        print(f'rendered {template.filename} -> {output_path}')

    for static_base in (args.input / default / 'static', args.input / custom / 'static'):
        for dirpath, dirnames, filenames in os.walk(static_base):
            for fname in filenames:
                source = Path(dirpath) / fname
                target = args.output / 'static' / fname
                ensure_parents_exist(target)
                shutil.copy2(source, target)
                print(f'copied {source} -> {target}')

    (args.output / 'data').mkdir(exist_ok = True)


def main(args):
    with open('html/sitemap.json', mode = 'r') as f:
        sitemap = json.load(f)

    print(f'Building HTCondor View HTML from templates in {args.input.absolute()}')
    print(f'Using relative base {args.base}')
    print(f'Placing output in {args.output.absolute()}')
    build(args, sitemap)


if __name__ == '__main__':
    args = parse_args()
    main(args)
