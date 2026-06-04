# Lesson 0 — Setting Up Your Editor

Before the first line of code, spend two minutes giving your editor **Kirito syntax highlighting**.
It's optional — Kirito runs fine from a plain text file — but coloured keywords, strings, and
comments make the rest of this course much easier to read. Kirito is young, so no editor knows it out
of the box; the repository ships ready-to-install definitions under [`editors/`](https://github.com/AzethMeron/KiritoLang/tree/main/editors).

Pick whichever editor you use.

## VS Code (recommended)

The `editors/vscode/` folder is a complete extension — a grammar plus comment-toggling, bracket
matching, and auto-indent after `:`.

```text
cp -r editors/vscode ~/.vscode/extensions/kirito-language-0.1.0
```

(On Windows that's `%USERPROFILE%\.vscode\extensions\kirito-language-0.1.0`.) Reload VS Code and any
`.ki` file lights up. To share it, package a `.vsix` with `vsce package` and
`code --install-extension`.

## Notepad++

Notepad++ has a built-in **User Defined Language** system, so no plugin is needed:

1. `Language` menu → `User Defined Language` → `Define your language…`
2. Click `Import…` and choose `editors/notepad++/kirito.xml`.
3. Restart Notepad++.

`.ki` files are then highlighted automatically (or pick **Kirito** from the `Language` menu).

> The colours are **theme-agnostic**: no style paints a background (the editor's own background
> shows through) and only meaningful tokens are tinted, in hues chosen to read on **both light and
> dark** Notepad++ themes. Plain text and operators inherit the theme's default colour, so there's
> nothing to hand-tweak when you switch themes.

## Vim / Neovim

Copy `editors/vim/kirito.vim` to `~/.vim/syntax/kirito.vim` (Neovim:
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

- Where to find Kirito editor support (`editors/`), and how to install it for VS Code, Notepad++, and
  Vim.
- The quick "treat it as Python" fallback for any other editor.

With colours in place, let's write some code.
