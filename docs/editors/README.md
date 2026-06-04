# Editor support for Kirito

Kirito is a young language, so editors don't know it out of the box. This directory ships
ready-to-install syntax definitions. They highlight Kirito's keywords (`var`, `Function`, `class`,
`switch`/`case`, `catch`/`throw`, `todo`, `discard`, …), the built-in types and functions, `#`
comments, `"strings"` and `f"strings"` with escapes, and decimal/hex/octal/binary/float numbers.

Pick your editor:

## Notepad++  (no plugin needed)

Notepad++ has a built-in **User Defined Language** system — no plugin required.

There are **two** colour files — pick the one matching your Notepad++ theme:

- [`notepad++/kirito-dark.xml`](notepad++/kirito-dark.xml) — for a **dark** theme (registers as
  *Kirito (Dark)*).
- [`notepad++/kirito-light.xml`](notepad++/kirito-light.xml) — for a **light** theme (registers as
  *Kirito (Light)*).

1. `Language` menu → `User Defined Language` → `Define your language…`
2. Click `Import…` and choose the file for your theme.
3. Restart Notepad++.

Any `.ki` file is then highlighted automatically (or pick the matching **Kirito (Dark/Light)** entry
from the `Language` menu).

Why two files? A Notepad++ UDL **cannot** give the base text/operator styles a transparent
background — when their background is left unset, Notepad++ paints a solid white box behind the text
(it shows up immediately on a dark theme). So instead of fighting it, each file paints **every**
style's background to match its theme, which renders seamlessly with no boxes; the foreground hues
are then tuned for that background. If you switch your Notepad++ theme, re-import the other file (UDL
colours are cached, so a re-import + restart is needed to pick up a change).

> Note: UDL folding is brace-based, so Kirito's indentation blocks don't fold. For folding plus
> bracket-matching, use the VS Code grammar below.

## VS Code  (best experience)

The `vscode/` folder is a complete extension (a TextMate grammar + language configuration with
comment toggling, bracket matching, and indent-after-`:`).

**Quick, local install:**

```
cp -r docs/editors/vscode ~/.vscode/extensions/kirito-language-0.1.0
```

(Windows: `%USERPROFILE%\.vscode\extensions\kirito-language-0.1.0`.) Reload VS Code — `.ki` files
light up.

**Or package it as a `.vsix`** (shareable):

```
npm install -g @vscode/vsce
cd docs/editors/vscode
vsce package          # produces kirito-language-0.1.0.vsix
code --install-extension kirito-language-0.1.0.vsix
```

The grammar's `scopeName` is `source.kirito`; the same `syntaxes/kirito.tmLanguage.json` also works
in any TextMate-grammar-aware editor (Sublime Text via a package, the `bat` pager, etc.).

## Vim / Neovim

Copy [`vim/kirito.vim`](vim/kirito.vim) to `~/.vim/syntax/kirito.vim` (Neovim:
`~/.config/nvim/syntax/kirito.vim`) and add to your config:

```vim
autocmd BufRead,BufNewFile *.ki set filetype=kirito
```

## Quick fallback (any editor)

Kirito's surface looks a lot like Python (significant indentation, `#` comments, similar control
flow), so as a stopgap you can just tell your editor to treat `.ki` files as **Python** — you'll get
~80% of the benefit instantly. The dedicated grammars above add the Kirito-specific keywords
(`Function`, `var`, `switch`/`case`/`default`, `catch`/`throw`, `todo`, `discard`) and the fact that
`print`/`input` are *not* builtins (they live in the `io` module).

---

These definitions are generated from the language's actual keyword set
(`src/kirito/lexer.hpp`) and built-in list (`src/kirito/runtime.hpp`); keep them in sync when
the language gains keywords.
