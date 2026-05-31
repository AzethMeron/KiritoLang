#!/usr/bin/env python3
"""Generate the Kirito HTML documentation site from the Markdown pages in docs/pages/.

Design goals:
  - Simply expandable: drop a new `NN-title.md` file in docs/pages/ and it appears in the sidebar,
    ordered by its numeric prefix. No code changes needed.
  - Zero dependencies: a small, self-contained Markdown subset renderer (headings, lists, code
    fences, inline code/bold/italic/links, tables, blockquotes) — runs with stock Python 3.
  - Self-contained output: every page is a standalone .html with an embedded stylesheet and a
    shared sidebar, so the `docs/site/` folder works opened directly from disk (no server).

Documentation content is authored in the .md files, NOT scraped from source-code comments.

Usage:  python3 docs/build_docs.py   ->   writes docs/site/*.html
"""
import html
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGES_DIR = os.path.join(HERE, "pages")
OUT_DIR = os.path.join(HERE, "site")

CSS = """
:root{--bg:#fbfbfd;--fg:#1d1d1f;--muted:#6e6e73;--accent:#7c5cff;--code-bg:#f4f4f7;
--border:#e3e3e8;--side:#f7f7fa}
*{box-sizing:border-box}
body{margin:0;font:16px/1.65 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
color:var(--fg);background:var(--bg)}
a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}
#layout{display:flex;min-height:100vh}
#sidebar{width:280px;background:var(--side);border-right:1px solid var(--border);padding:24px 18px;
position:sticky;top:0;height:100vh;overflow-y:auto;flex-shrink:0}
#sidebar h1{font-size:20px;margin:0 0 4px}
#sidebar .tag{color:var(--muted);font-size:13px;margin-bottom:20px}
#sidebar a{display:block;color:var(--fg);padding:5px 10px;border-radius:7px;font-size:14.5px}
#sidebar a:hover{background:#ececf2;text-decoration:none}
#sidebar a.active{background:var(--accent);color:#fff}
#content{flex:1;max-width:860px;margin:0 auto;padding:44px 52px 100px}
h1,h2,h3{line-height:1.25;font-weight:650}
h1{font-size:33px;margin:.2em 0 .6em;border-bottom:1px solid var(--border);padding-bottom:.3em}
h2{font-size:24px;margin:1.7em 0 .5em}h3{font-size:18.5px;margin:1.3em 0 .4em}
code{background:var(--code-bg);padding:2px 6px;border-radius:5px;font-size:.86em;
font-family:"SF Mono",SFMono-Regular,Consolas,"Liberation Mono",Menlo,monospace}
pre{background:var(--code-bg);padding:16px 18px;border-radius:10px;overflow-x:auto;
border:1px solid var(--border)}
pre code{background:none;padding:0;font-size:13.5px;line-height:1.5}
table{border-collapse:collapse;width:100%;margin:1em 0;font-size:14.5px}
th,td{border:1px solid var(--border);padding:8px 12px;text-align:left;vertical-align:top}
th{background:var(--side)}
blockquote{margin:1em 0;padding:.4em 1.1em;border-left:4px solid var(--accent);
background:var(--code-bg);border-radius:0 8px 8px 0;color:#333}
.kw{color:#a626a4}.str{color:#50a14f}.com{color:#a0a1a7;font-style:italic}.num{color:#986801}
.builtin{color:#4078f2}
a.xref{text-decoration:none;border-bottom:1px dotted var(--accent)}
a.xref:hover{text-decoration:none;border-bottom-style:solid}
a.xref code{color:inherit}
hr{border:none;border-top:1px solid var(--border);margin:2em 0}
.note{font-size:13px;color:var(--muted);margin-top:40px}
"""

# Kirito keywords / builtins for lightweight highlighting of ```kirito fences.
KW = set("var Function if elif else while for in break continue return and or not "
         "class try catch finally throw with as pass assert import discard True False None".split())
BUILTINS = set("print input len range sum min max abs round sorted enumerate zip map filter type "
               "inspect all any reversed divmod isinstance ord chr bin oct hex pow Integer Float "
               "String Bool List Set Dict".split())


def highlight_kirito(code):
    out = []
    for line in code.split("\n"):
        # comments
        m = re.match(r"^(.*?)(#.*)$", line)
        comment = ""
        if m and "\"" not in m.group(2):  # naive: only treat as comment if no quote follows
            line, comment = m.group(1), m.group(2)
        # strings
        parts = re.split(r'("(?:[^"\\]|\\.)*")', line)
        rendered = ""
        for i, part in enumerate(parts):
            if i % 2 == 1:
                rendered += f'<span class="str">{html.escape(part)}</span>'
            else:
                def tok(mt):
                    w = mt.group(0)
                    if w in KW:
                        return f'<span class="kw">{w}</span>'
                    if w in BUILTINS:
                        return f'<span class="builtin">{w}</span>'
                    return w
                esc = html.escape(part)
                # Identifiers first, then numbers: the number wrapper emits a <span class="num">…
                # tag, and `class` is itself a Kirito keyword — highlighting identifiers afterwards
                # would match `class` *inside* that tag and produce broken nested markup.
                esc = re.sub(r"[A-Za-z_][A-Za-z0-9_]*", tok, esc)
                esc = re.sub(r"\b\d+\.?\d*\b", lambda m: f'<span class="num">{m.group(0)}</span>', esc)
                rendered += esc
        if comment:
            rendered += f'<span class="com">{html.escape(comment)}</span>'
        out.append(rendered)
    return "\n".join(out)


# --- symbol cross-referencing -----------------------------------------------------------------
# A documented symbol (a builtin, a type, a stdlib module or one of its functions) is auto-anchored
# where it is defined and every later mention of it in `inline code` becomes a clickable link to
# that definition. Definitions are detected from the existing authoring style with no extra syntax:
#   * a heading whose text is a bare identifier            (e.g. `## io`)            -> the module
#   * the first `code` token of a table row or list item   (e.g. `| `range(stop)` |`) -> that symbol
# Names that would be ambiguous (defined in more than one place, e.g. a `pop` method on several
# types) are still anchored but not auto-linked, so links never point at the wrong entry.
SYMBOLS = {}          # name -> (html_file, anchor)
_AMBIGUOUS = set()    # names defined in >1 place: anchored but not auto-linked
_EMITTED = set()      # (file, anchor) already given an id this build, to avoid duplicates
_CUR_FILE = ""        # html filename currently being rendered (for relative xref hrefs)


def _lead_ident(code_text):
    m = re.match(r"\s*`?([A-Za-z_][\w]*(?:\.[A-Za-z_][\w]*)*)", code_text)
    return m.group(1) if m else None


def _slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def collect_symbols(pages):
    # Phase 1 — headings are *canonical* definitions: a bare-identifier heading (e.g. `## io`,
    # `### String`) owns that name, so a type/module links to its reference section rather than to,
    # say, its constructor row. Heading-owned names are never ambiguous.
    heading_syms = set()
    for _, _, fn, text in pages:
        for line in text.split("\n"):
            h = re.match(r"^(#{1,4})\s+(.*)$", line)
            if h:
                title = h.group(2).strip().strip("`")
                if re.fullmatch(r"[A-Za-z_][\w]*", title):
                    SYMBOLS[title] = (fn, _slug(title))
                    heading_syms.add(title)
    # Phase 2 — the first `code` token of a table row or list item defines that symbol, unless a
    # heading already owns the name. A name defined this way in two different places is ambiguous.
    for _, _, fn, text in pages:
        in_fence = False
        for line in text.split("\n"):
            if line.startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence or re.match(r"^#{1,4}\s", line):
                continue
            cell = None
            if line.lstrip().startswith("|") and "|" in line.strip().strip("|"):
                cell = line.strip().strip("|").split("|")[0].strip()
            elif re.match(r"^\s*[-*]\s+", line):
                cell = re.sub(r"^\s*[-*]\s+", "", line).lstrip("*").strip()
            if cell and cell.startswith("`"):
                name = _lead_ident(cell)
                if not name or name in heading_syms:
                    continue
                target = (fn, "sym-" + name)
                if name in SYMBOLS and SYMBOLS[name] != target:
                    _AMBIGUOUS.add(name)
                else:
                    SYMBOLS[name] = target


def _row_anchor(first_cell):
    # Returns an ` id="..."` for a table row / list item that *defines* a symbol on the current page
    # (first occurrence only), else "".
    name = _lead_ident(first_cell) if first_cell.lstrip().startswith("`") else None
    if not name or name not in SYMBOLS:
        return ""
    file, anchor = SYMBOLS[name]
    # Heading-owned symbols (anchor is a plain slug) already carry their id on the heading itself.
    if not anchor.startswith("sym-") or file != _CUR_FILE or (file, anchor) in _EMITTED:
        return ""
    _EMITTED.add((file, anchor))
    return f' id="{anchor}"'


def linkify(body):
    # Turn inline <code>name…</code> spans into links to the symbol's definition. Skips fenced code
    # blocks (<pre>…</pre>) and ambiguous names.
    def repl(m):
        name = _lead_ident(m.group(1))
        if name and name in SYMBOLS and name not in _AMBIGUOUS:
            file, anchor = SYMBOLS[name]
            return f'<a class="xref" href="{file}#{anchor}"><code>{m.group(1)}</code></a>'
        return m.group(0)
    parts = re.split(r"(<pre>.*?</pre>)", body, flags=re.S)
    for i in range(0, len(parts), 2):
        parts[i] = re.sub(r"<code>([^<]+)</code>", repl, parts[i])
    return "".join(parts)


def render_inline(text):
    # Order matters: escape first, then re-inject markup.
    text = html.escape(text)
    text = re.sub(r"`([^`]+)`", lambda m: f"<code>{m.group(1)}</code>", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)
    return text


def render_markdown(md):
    lines = md.split("\n")
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        # fenced code block
        if line.startswith("```"):
            lang = line[3:].strip()
            i += 1
            buf = []
            while i < n and not lines[i].startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1  # skip closing fence
            code = "\n".join(buf)
            if lang in ("kirito", "ki"):
                out.append(f"<pre><code>{highlight_kirito(code)}</code></pre>")
            else:
                out.append(f"<pre><code>{html.escape(code)}</code></pre>")
            continue
        # headings
        m = re.match(r"^(#{1,4})\s+(.*)$", line)
        if m:
            level = len(m.group(1))
            text = render_inline(m.group(2))
            anchor = re.sub(r"[^a-z0-9]+", "-", m.group(2).lower()).strip("-")
            out.append(f'<h{level} id="{anchor}">{text}</h{level}>')
            i += 1
            continue
        # tables (a line with | and a following |---| separator)
        if "|" in line and i + 1 < n and re.match(r"^\s*\|?[\s:|-]+\|?\s*$", lines[i + 1]) and "-" in lines[i + 1]:
            header = [c.strip() for c in line.strip().strip("|").split("|")]
            i += 2
            rows = []
            while i < n and "|" in lines[i]:
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            t = "<table><thead><tr>" + "".join(f"<th>{render_inline(h)}</th>" for h in header) + "</tr></thead><tbody>"
            for r in rows:
                rid = _row_anchor(r[0]) if r else ""
                t += f"<tr{rid}>" + "".join(f"<td>{render_inline(c)}</td>" for c in r) + "</tr>"
            t += "</tbody></table>"
            out.append(t)
            continue
        # blockquote
        if line.startswith(">"):
            buf = []
            while i < n and lines[i].startswith(">"):
                buf.append(lines[i][1:].strip())
                i += 1
            out.append(f"<blockquote>{render_inline(' '.join(buf))}</blockquote>")
            continue
        # lists
        if re.match(r"^\s*[-*]\s+", line):
            buf = []
            while i < n and re.match(r"^\s*[-*]\s+", lines[i]):
                buf.append(re.sub(r"^\s*[-*]\s+", "", lines[i]))
                i += 1
            items = []
            for x in buf:
                lid = _row_anchor(x.lstrip("*")) if x.lstrip("*").lstrip().startswith("`") else ""
                items.append(f"<li{lid}>{render_inline(x)}</li>")
            out.append("<ul>" + "".join(items) + "</ul>")
            continue
        if re.match(r"^\s*\d+\.\s+", line):
            buf = []
            while i < n and re.match(r"^\s*\d+\.\s+", lines[i]):
                buf.append(re.sub(r"^\s*\d+\.\s+", "", lines[i]))
                i += 1
            out.append("<ol>" + "".join(f"<li>{render_inline(x)}</li>" for x in buf) + "</ol>")
            continue
        if line.strip() == "":
            i += 1
            continue
        if line.strip() == "---":
            out.append("<hr>")
            i += 1
            continue
        # paragraph (gather consecutive non-blank, non-special lines)
        buf = [line]
        i += 1
        while i < n and lines[i].strip() and not re.match(r"^(#{1,4}\s|```|>|\s*[-*]\s|\s*\d+\.\s)", lines[i]) \
                and not (lines[i].strip() == "---"):
            buf.append(lines[i])
            i += 1
        out.append(f"<p>{render_inline(' '.join(buf))}</p>")
    return "\n".join(out)


def main():
    files = sorted(f for f in os.listdir(PAGES_DIR) if f.endswith(".md"))
    if not files:
        sys.exit("no pages found in " + PAGES_DIR)
    os.makedirs(OUT_DIR, exist_ok=True)

    pages = []  # (slug, title, html_filename)
    for f in files:
        with open(os.path.join(PAGES_DIR, f), encoding="utf-8") as fh:
            text = fh.read()
        title = text.split("\n", 1)[0].lstrip("# ").strip()
        slug = re.sub(r"^\d+-", "", f[:-3])
        pages.append((slug, title, slug + ".html", text))

    collect_symbols(pages)

    def sidebar(active_html):
        items = ['<h1>Kirito</h1><div class="tag">dynamically-typed scripting language</div>']
        for _, title, fn, _ in pages:
            cls = ' class="active"' if fn == active_html else ""
            items.append(f'<a href="{fn}"{cls}>{html.escape(title)}</a>')
        return '<nav id="sidebar">' + "".join(items) + "</nav>"

    for slug, title, fn, text in pages:
        global _CUR_FILE
        _CUR_FILE = fn
        body = linkify(render_markdown(text))
        doc = f"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(title)} — Kirito</title><style>{CSS}</style></head>
<body><div id="layout">{sidebar(fn)}<main id="content">{body}
<p class="note">Kirito documentation — generated by docs/build_docs.py from docs/pages/.</p>
</main></div></body></html>"""
        with open(os.path.join(OUT_DIR, fn), "w", encoding="utf-8") as fh:
            fh.write(doc)

    # index.html redirects to the first page.
    with open(os.path.join(OUT_DIR, "index.html"), "w", encoding="utf-8") as fh:
        fh.write(f'<!DOCTYPE html><meta charset="utf-8">'
                 f'<meta http-equiv="refresh" content="0; url={pages[0][2]}">'
                 f'<a href="{pages[0][2]}">Kirito documentation</a>')

    print(f"wrote {len(pages) + 1} files to {OUT_DIR}")


if __name__ == "__main__":
    main()
