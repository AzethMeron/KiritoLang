# Lesson 0 — Setting Up Your Editor

Before the first line of code, spend two minutes giving your editor **Kirito syntax highlighting**.
It's optional — Kirito runs fine from a plain text file — but coloured keywords, strings, and
comments make the rest of this course much easier to read. Kirito is young, so no editor knows it out
of the box; the repository ships ready-to-install definitions under [`docs/editors/`](https://github.com/AzethMeron/KiritoLang/tree/main/editors).

Pick whichever editor you use.

## VS Code (recommended)

The `docs/editors/vscode/` folder is a complete extension — a grammar plus comment-toggling, bracket
matching, and auto-indent after `:`.

```text
cp -r docs/editors/vscode ~/.vscode/extensions/kirito-language-0.1.0
```

(On Windows that's `%USERPROFILE%\.vscode\extensions\kirito-language-0.1.0`.) Reload VS Code and any
`.ki` file lights up. To share it, package a `.vsix` with `vsce package` and
`code --install-extension`.

## Notepad++

Notepad++ has a built-in **User Defined Language** system, so no plugin is needed:

Pick the colour file matching your Notepad++ theme — `docs/editors/notepad++/kirito-dark.xml` (dark) or
`kirito-light.xml` (light):

1. `Language` menu → `User Defined Language` → `Define your language…`
2. Click `Import…` and choose the file for your theme.
3. Restart Notepad++.

`.ki` files are then highlighted automatically (or pick the matching **Kirito (Dark/Light)** entry
from the `Language` menu).

> Why two files? A Notepad++ UDL can't make the base text/operator background transparent — leaving
> it unset paints a solid white box behind the text (obvious on a dark theme). So each file paints
> every style's background to match its theme (seamless, no boxes) with foreground hues tuned for
> that background. Switching themes? Re-import the other file (UDL colours are cached — a re-import +
> restart picks up the change).

## Vim / Neovim

Copy `docs/editors/vim/kirito.vim` to `~/.vim/syntax/kirito.vim` (Neovim:
`~/.config/nvim/syntax/kirito.vim`) and tell Vim to use it for `.ki`:

```vim
autocmd BufRead,BufNewFile *.ki set filetype=kirito
```

## Anything else (the 30-second fallback)

Kirito looks a lot like Python — significant indentation, `#` comments, similar control flow — so as
a stopgap you can tell any editor to treat `.ki` files as **Python** and get most of the benefit
instantly. The dedicated definitions above just add the Kirito-specific keywords (`Function`, `var`,
`switch`/`case`, `catch`/`throw`, `todo`, `discard`) and the fact that `print`/`input` aren't
builtins — they live in the `io` module.

## What you learned

- Where to find Kirito editor support (`docs/editors/`), and how to install it for VS Code, Notepad++, and
  Vim.
- The quick "treat it as Python" fallback for any other editor.

With colours in place, let's write some code.
