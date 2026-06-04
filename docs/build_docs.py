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
:root{--bg:#16161a;--fg:#e4e4ea;--muted:#9a9aa6;--accent:#9b87ff;--code-bg:#1e1e25;
--border:#2c2c35;--side:#1b1b21;--side-hover:#26262f}
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
#sidebar a:hover{background:var(--side-hover);text-decoration:none}
#sidebar a.active{background:var(--accent);color:#16161a}
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
background:var(--code-bg);border-radius:0 8px 8px 0;color:var(--fg)}
.kw{color:#c678dd}.str{color:#98c379}.com{color:#7f848e;font-style:italic}.num{color:#d19a66}
.builtin{color:#61afef}
a.xref{text-decoration:none}
a.xref:hover{text-decoration:underline}
a.xref code{color:inherit}
hr{border:none;border-top:1px solid var(--border);margin:2em 0}
.note{font-size:13px;color:var(--muted);margin-top:40px}
"""

# Kirito keywords / builtins for lightweight highlighting of ```kirito fences.
KW = set("var Function if elif else while for in break continue return and or not "
         "class try catch finally throw with as pass assert import discard True False None".split())
BUILTINS = set("print input len range sum min max abs round sorted enumerate zip map filter type "
               "inspect all any reversed divmod isinstance ord chr bin oct hex pow Integer Float "
               "bitand bitor bitxor bitnot shl shr String Bool List Set Dict".split())


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
    # heading already owns the name. A name defined this way in two different places is ambiguous —
    # including the same `sym-` name under two different *sections* of one page (e.g. `dumps`/`loads`
    # in both `json` and `serialize`, or `parse` in `json` and `csv`): such generic names must not
    # auto-cross-link to whichever module happened to render first. They keep their anchor but are
    # not auto-linked, so a stray inline mention never points at the wrong entry.
    defined_section = {}  # name -> (file, section-heading) where it was first defined
    for _, _, fn, text in pages:
        in_fence = False
        section = None
        for line in text.split("\n"):
            if line.lstrip().startswith("```"):  # fences may be indented (nested under a list item)
                in_fence = not in_fence
                continue
            h = re.match(r"^#{1,4}\s+(.*)$", line)
            if h:
                section = (fn, h.group(1).strip())
                continue
            if in_fence:
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
                prev = defined_section.get(name)
                if (name in SYMBOLS and SYMBOLS[name] != target) or (prev is not None and prev != section):
                    _AMBIGUOUS.add(name)
                else:
                    SYMBOLS[name] = target
                    defined_section.setdefault(name, section)


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
    # Order matters: escape first, then re-inject markup. Inline-code spans are stashed behind
    # placeholders *before* bold/italic/link rules run, so operators inside `code` (e.g. `2 ** 10`,
    # `*args`, `a[0]`) are never mistaken for Markdown emphasis. They are restored last.
    text = html.escape(text)
    spans = []

    def stash(m):
        spans.append(m.group(1))
        return f"\x00{len(spans) - 1}\x00"

    text = re.sub(r"`([^`]+)`", stash, text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)
    text = re.sub(r"\x00(\d+)\x00", lambda m: f"<code>{spans[int(m.group(1))]}</code>", text)
    return text


def _is_block_start(s):
    # True if the line begins a NEW block, so it can't be a wrapped continuation of a list item.
    # Code fences count even when indented (a fence nested under a list item ends the item's text).
    return bool(re.match(r"^\s*[-*]\s+", s) or re.match(r"^\s*\d+\.\s+", s)
                or s.startswith("#") or s.startswith(">")
                or s.lstrip().startswith("```") or s.strip() == "")


def render_markdown(md):
    lines = md.split("\n")
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        # fenced code block — recognized even when indented (e.g. a fence nested under a list item).
        # The fence's own indentation is stripped from each content line so nested code isn't shown
        # over-indented; relative indentation inside the block is preserved.
        stripped = line.lstrip()
        if stripped.startswith("```"):
            indent = len(line) - len(stripped)
            lang = stripped[3:].strip()
            i += 1
            buf = []
            while i < n and not lines[i].lstrip().startswith("```"):
                row = lines[i]
                k = 0
                while k < indent and k < len(row) and row[k] == " ":
                    k += 1
                buf.append(row[k:])
                i += 1
            i += 1  # skip closing fence
            code = "\n".join(buf)
            if lang in ("kirito", "ki"):
                out.append(f"<pre><code>{highlight_kirito(code)}</code></pre>")
            else:
                out.append(f"<pre><code>{html.escape(code)}</code></pre>")
            continue
        # HTML comment lines (e.g. the `<!--norun ...-->` markers that flag a code block as not to be
        # executed by the snippet checker): authoring/tooling metadata, never display content — skip
        # them so they don't render as a literal paragraph. Handles a comment spanning several lines.
        if line.lstrip().startswith("<!--"):
            while i < n and "-->" not in lines[i]:
                i += 1
            i += 1  # consume the line that closes the comment
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
        # lists (a wrapped/indented continuation line — non-blank, not itself a new block — folds
        # into the current item, so a bullet's text may span several source lines)
        if re.match(r"^\s*[-*]\s+", line):
            buf = []
            while i < n and (re.match(r"^\s*[-*]\s+", lines[i])
                             or (buf and not _is_block_start(lines[i]))):
                if re.match(r"^\s*[-*]\s+", lines[i]):
                    buf.append(re.sub(r"^\s*[-*]\s+", "", lines[i]))
                else:
                    buf[-1] = buf[-1] + " " + lines[i].strip()  # continuation of the bullet above
                i += 1
            items = []
            for x in buf:
                lid = _row_anchor(x.lstrip("*")) if x.lstrip("*").lstrip().startswith("`") else ""
                items.append(f"<li{lid}>{render_inline(x)}</li>")
            out.append("<ul>" + "".join(items) + "</ul>")
            continue
        if re.match(r"^\s*\d+\.\s+", line):
            buf = []
            while i < n and (re.match(r"^\s*\d+\.\s+", lines[i])
                             or (buf and not _is_block_start(lines[i]))):
                if re.match(r"^\s*\d+\.\s+", lines[i]):
                    buf.append(re.sub(r"^\s*\d+\.\s+", "", lines[i]))
                else:
                    buf[-1] = buf[-1] + " " + lines[i].strip()  # continuation of the item above
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
