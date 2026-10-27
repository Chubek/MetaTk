# Vim support

Run `sh addons/install.sh` to install GPPX support in `$HOME/.vim`, or select
a runtime directory explicitly:

```sh
sh addons/install.sh --prefix "$HOME/.config/nvim"
```

The installer creates `syntax/gppx.vim`, `indent/gppx.vim`, and
`ftdetect/gppx.vim`. Re-running replaces those three files. It does not edit
vimrc or other runtime files. Enable `filetype plugin indent on` and
`syntax on` in your editor configuration.

Detection covers `.gppx` and `.vmwv`. Highlighting recognizes the built-in
`#`, `%`, and `@` directive prefixes, comments, strings, explicit macros and
raw blocks. Indentation follows GPPX conditional and generation blocks,
using `shiftwidth()` and preserving raw block contents. It ignores directives
inside C-style comments and quoted strings. It does not infer arbitrary
runtime `#syntax` changes or indent the embedded target language. For other
extensions use `:setfiletype gppx` or add your own detection rule.

The source filename `vim/syntax/vim.syntax` is retained for compatibility
with the repository layout; it is installed under Vim's conventional name.
