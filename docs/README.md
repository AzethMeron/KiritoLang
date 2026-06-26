# Kirito documentation

Hand-authored Markdown in `pages/`, rendered to a static HTML site in `site/` by `build_docs.py`
(stock Python 3, no dependencies). Documentation lives in these Markdown files — it is **not**
scraped from source-code comments, so the codebase stays uncluttered.

## Build

```
python3 docs/build_docs.py     # writes docs/site/*.html
```

Open `docs/site/index.html` in a browser (works straight from disk — no server needed).

## Extending the docs

Drop a new `NN-title.md` file in `pages/`. It appears in the sidebar automatically, ordered by its
numeric prefix; the first line (`# Title`) becomes its sidebar label and page title. The renderer
supports headings, lists, tables, blockquotes, links, inline `code`/**bold**/*italic*, and fenced
code blocks (use ` ```kirito ` for Kirito syntax highlighting).

## Pages

- **Introduction / Getting Started / Embedding / Extending** — overview, build, C++ integration.
- **Language Guide / Recipes** — syntax-and-semantics reference and task-oriented snippets.
- **Builtins / Standard Library** — every built-in function, module, and method.
- **Packages & kpm** — installing, versioning, and publishing packages.
- **Course** — a hands-on lesson path: 16 core lessons (editor setup → capstone) plus 6 bonus lessons
  for specialized libraries (regex, CLI programs, tabular data, linear algebra, tensors/autograd).
