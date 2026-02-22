#!/usr/bin/env python3
"""Convert INFO.md to info_html.h for the plugin Info tab.

Usage: gen_info_html.py input.md output.h
"""
import re
import sys


def inline_fmt(text):
    """Apply inline markdown: links, bold, italic."""
    # Angle-bracket URLs <url>
    text = re.sub(r'<(https?://[^>]+)>', r'<a href="\1">\1</a>', text)
    # Markdown links [text](url)  — handles http:, mailto:, etc.
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', text)
    # Bold **text**
    text = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', text)
    # Italic *text* (not **)
    text = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<i>\1</i>', text)
    return text


def preprocess(lines):
    """Join bullet-item continuation lines with their parent bullet.

    Handles both indented continuations (standard markdown):
        - item text
          more text
    and unindented continuations (common in hand-wrapped markdown):
        - item text
        more text
    Continuation stops at blank lines, headings, rulers, table rows, or new bullets.
    """
    def is_block_start(s):
        return (not s.strip()
                or re.match(r'^#{1,3} ', s)
                or re.match(r'^-{3,}\s*$', s)
                or re.match(r'^ *- ', s)
                or s.startswith('|'))

    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r'^( *)- (.+)$', line)
        if m:
            indent = len(m.group(1))
            joined = line.rstrip()
            i += 1
            while i < len(lines):
                nxt = lines[i]
                # Indented continuation (standard): at least indent+2 spaces, not a bullet
                indented = (nxt.startswith(' ' * (indent + 2))
                            and not re.match(r'^ *- ', nxt)
                            and nxt.strip())
                # Unindented continuation (hand-wrapped): non-blank, no block-start markers
                unindented = (not is_block_start(nxt)
                              and nxt.strip()
                              and indent == 0)
                if indented or unindented:
                    joined += ' ' + nxt.strip()
                    i += 1
                else:
                    break
            out.append(joined)
        else:
            out.append(line)
            i += 1
    return out


def convert(md_text):
    lines = preprocess(md_text.splitlines())

    parts = ['<html><body>']
    para_buf = []
    list_stack = []  # stack of currently open indent levels
    in_table = False

    def flush_para():
        nonlocal para_buf
        if para_buf:
            parts.append('<p>' + inline_fmt(' '.join(para_buf)) + '</p>')
            para_buf = []

    def close_lists_to(target):
        while list_stack and list_stack[-1] >= target:
            parts.append('</ul>')
            list_stack.pop()

    def close_table():
        nonlocal in_table
        if in_table:
            parts.append('</table>')
            in_table = False

    for idx, line in enumerate(lines):
        stripped = line.rstrip()

        # Blank line
        if not stripped.strip():
            flush_para()
            close_lists_to(0)
            close_table()
            continue

        # Heading  (### / ## / #)
        m = re.match(r'^(#{1,3}) (.+)$', stripped)
        if m:
            flush_para()
            close_lists_to(0)
            close_table()
            lvl = len(m.group(1))
            parts.append(f'<h{lvl}>{inline_fmt(m.group(2))}</h{lvl}>')
            continue

        # Horizontal rule
        if re.match(r'^-{3,}\s*$', stripped):
            flush_para()
            close_lists_to(0)
            close_table()
            parts.append('<hr>')
            continue

        # Table row
        if stripped.startswith('|'):
            flush_para()
            close_lists_to(0)
            # Skip separator rows like |---|---|
            if re.match(r'^\|[\-| :]+\|?\s*$', stripped):
                continue
            if not in_table:
                parts.append('<table border="0" cellpadding="4" cellspacing="2">')
                in_table = True
            cells = [c.strip() for c in stripped.strip('|').split('|')]
            parts.append('<tr>' + ''.join(f'<td>{inline_fmt(c)}</td>' for c in cells) + '</tr>')
            # Close table if next non-blank line isn't a table row
            peek = idx + 1
            while peek < len(lines) and not lines[peek].strip():
                peek += 1
            if peek >= len(lines) or not lines[peek].startswith('|'):
                close_table()
            continue

        # List item
        m = re.match(r'^( *)- (.+)$', stripped)
        if m:
            flush_para()
            close_table()
            indent = len(m.group(1))
            content = m.group(2).strip()
            if not list_stack or list_stack[-1] < indent:
                parts.append('<ul>')
                list_stack.append(indent)
            elif list_stack[-1] > indent:
                close_lists_to(indent + 1)
                if not list_stack or list_stack[-1] != indent:
                    parts.append('<ul>')
                    list_stack.append(indent)
            parts.append(f'<li>{inline_fmt(content)}</li>')
            continue

        # Regular text — close any open list if we're back at col 0
        close_table()
        if list_stack and (len(line) - len(line.lstrip())) == 0:
            close_lists_to(0)
        para_buf.append(stripped.strip())

    flush_para()
    close_lists_to(0)
    close_table()
    parts.append('</body></html>')
    return '\n'.join(parts)


def main():
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} input.md output.h', file=sys.stderr)
        sys.exit(1)
    with open(sys.argv[1], 'r', encoding='utf-8') as f:
        md = f.read()
    html = convert(md)
    with open(sys.argv[2], 'w', encoding='utf-8') as f:
        f.write('// Auto-generated from INFO.md — do not edit manually.\n')
        f.write('static const char kInfoHTML[] = R"INFOHTML(\n')
        f.write(html)
        f.write('\n)INFOHTML";\n')


if __name__ == '__main__':
    main()
