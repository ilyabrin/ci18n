#!/usr/bin/env python3
"""
Check the documentation.

    python3 tools/check_docs.py          # links only, no compiler needed
    python3 tools/check_docs.py --run    # also build and run the README example

Every relative link in every Markdown file must point at a file that exists,
and a #fragment at a heading in it, using GitHub's rules for turning headings
into anchors. External links are not fetched.

With --run, the "In 30 seconds" example in README.md is compiled exactly as
written, with its translation file beside it, and every line it prints must
match the /* comment */ next to the puts() that printed it. The landing page
is the first thing anyone runs, so it is the one that must not go stale.

SPDX-License-Identifier: MIT
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SKIP_DIRS = {'.git', 'build', 'node_modules', 'fuzz_findings'}

LINK = re.compile(r'(?<!!)\[[^\]]*\]\(([^)\s]+)\)')
FENCE = re.compile(r'^(```|~~~)')


def markdown_files():
    for base, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith('build-')]
        for name in files:
            if name.endswith('.md'):
                yield os.path.join(base, name)


def anchor(heading):
    """GitHub's slug: lower case, punctuation dropped, spaces to hyphens."""
    text = re.sub(r'`([^`]*)`', r'\1', heading.strip().lower())
    text = re.sub(r'[^\w\- ]', '', text)
    return text.replace(' ', '-')


def anchors_of(path):
    found = set()
    counts = {}
    in_code = False
    with open(path, encoding='utf-8') as f:
        for line in f:
            if FENCE.match(line):
                in_code = not in_code
                continue
            m = None if in_code else re.match(r'^#{1,6}\s+(.*?)\s*#*\s*$', line)
            if m:
                slug = anchor(m.group(1))
                n = counts.get(slug, 0)
                counts[slug] = n + 1
                found.add(slug if n == 0 else '%s-%d' % (slug, n))
    return found


def check_links():
    problems = []
    cache = {}
    for path in markdown_files():
        in_code = False
        with open(path, encoding='utf-8') as f:
            for number, line in enumerate(f, 1):
                if FENCE.match(line):
                    in_code = not in_code
                    continue
                if in_code:
                    continue
                for target in LINK.findall(line):
                    if re.match(r'^[a-z]+:', target):
                        continue
                    file_part, _, fragment = target.partition('#')
                    dest = path if not file_part else os.path.normpath(
                        os.path.join(os.path.dirname(path), file_part))
                    where = '%s:%d' % (os.path.relpath(path, ROOT), number)
                    if not os.path.exists(dest):
                        problems.append('%s: no such file: %s' % (where, target))
                        continue
                    if fragment and dest.endswith('.md'):
                        if dest not in cache:
                            cache[dest] = anchors_of(dest)
                        if fragment not in cache[dest]:
                            problems.append('%s: no heading for #%s in %s' % (
                                where, fragment, os.path.relpath(dest, ROOT)))
    return problems


def run_readme_example():
    with open(os.path.join(ROOT, 'README.md'), encoding='utf-8') as f:
        text = f.read()
    section = text.split('## In 30 seconds', 1)[1].split('\n## ', 1)[0]
    ini = re.search(r'```ini\n(.*?)```', section, re.S).group(1)
    code = re.search(r'```c\n(.*?)```', section, re.S).group(1)
    expected = re.findall(r'puts\([^;]*;\s*/\*\s*(.*?)\s*\*/', code)
    if not expected:
        return ['README example: no /* expected output */ comments found']

    cc = os.environ.get('CC', 'cc')
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, 'ru.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(ini)
        with open(os.path.join(tmp, 'main.c'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(code)
        exe = os.path.join(tmp, 'hello')
        build = subprocess.run([cc, '-std=c99', '-Wall', '-Wextra', '-Werror',
                                '-I' + os.path.join(ROOT, 'include'),
                                os.path.join(tmp, 'main.c'), '-o', exe],
                               capture_output=True, text=True)
        if build.returncode != 0:
            return ['README example does not compile:\n' + build.stderr]
        run = subprocess.run([exe], cwd=tmp, capture_output=True)
        got = run.stdout.decode('utf-8').replace('\r\n', '\n').splitlines()
    if run.returncode != 0 or got != expected:
        return ['README example printed %r, the comments say %r' % (got, expected)]
    return []


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    parser.add_argument('--run', action='store_true',
                        help='also compile and run the README example')
    args = parser.parse_args()

    problems = check_links()
    if args.run:
        problems += run_readme_example()

    for p in problems:
        print(p)
    if problems:
        return 1
    print('docs ok%s' % (', README example runs' if args.run else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
