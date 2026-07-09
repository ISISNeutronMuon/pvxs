#!/usr/bin/env python3
"""Strip leading context lines from pure-addition hunks in a unified diff.

Reads a unified diff on stdin, writes the adjusted diff to stdout.

Why: sibling patches (e.g. 02-alarm-messages and 03-pvfilter) that each
independently add a line to the same shared file -- ioc/Makefile's source
list, setup.py's DSOS list, sitehooks.cpp's registerHooks() body -- insert
at the same conceptual spot (right after the last existing entry). A
default-context diff anchors its hunk on the lines *before* the insertion
(e.g. "pvxsIoc_SRCS += sitehooks.cpp"), which is the exact same anchor the
sibling patch also uses -- so applying both in sequence conflicts, even
though the two additions don't actually touch any common line.

Dropping the leading context from a hunk that is a pure addition (no
removed lines) means it's found purely via its *trailing* context instead
(e.g. the blank line + "ifdef BASE_3_15" that follows), which isn't
touched by the sibling patch -- so both additions stack cleanly regardless
of order. This is exactly what hand-trimmed unified diffs do; this script
does it mechanically so patches/*.patch never needs hand-editing.

Hunks that remove any line are left untouched -- they're genuine
modifications, not pure insertions, and don't have this anchor-collision
failure mode.
"""
import re
import sys


def process(text):
    lines = text.split('\n')
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$', line)
        if not m:
            out.append(line)
            i += 1
            continue
        old_start, old_count, new_start, new_count, suffix = m.groups()
        old_start = int(old_start)
        old_count = int(old_count) if old_count is not None else 1
        new_start = int(new_start)
        new_count = int(new_count) if new_count is not None else 1
        i += 1
        body = []
        while i < len(lines) and not lines[i].startswith('@@') and not lines[i].startswith('diff --git'):
            body.append(lines[i])
            i += 1

        has_removal = any(l.startswith('-') for l in body)
        if not has_removal:
            lead = 0
            while lead < len(body) and body[lead].startswith(' '):
                lead += 1
            if lead > 0:
                old_start += lead
                old_count -= lead
                new_start += lead
                new_count -= lead
                body = body[lead:]

        header = "@@ -%d" % old_start
        if old_count != 1:
            header += ",%d" % old_count
        header += " +%d" % new_start
        if new_count != 1:
            header += ",%d" % new_count
        header += " @@" + suffix
        out.append(header)
        out.extend(body)
    return '\n'.join(out)


if __name__ == '__main__':
    # Force LF on stdout regardless of platform -- on Windows, the default
    # text-mode stdout translates '\n' to '\r\n', which would leave
    # patches/*.patch with CRLF line endings that don't match the
    # LF-normalized source tree git apply expects.
    sys.stdout.reconfigure(newline='\n')
    sys.stdout.write(process(sys.stdin.read()))
