# MetaML editor addons

Both addons recognize `.mml` programs, `.mmli`/`.mlli` interfaces, and the exact
project filename `MAMMAL`. Highlighting follows the MetaML lexer: nested `{- -}`
comments, `--` comments, normal/raw strings, escapes, numeric literals, type
variables, modules/constructors, `$n` parameters, `#n` section holes, and directives.
These are editor runtime files, not an LSP server or a type checker.

## Vim and Neovim

Install the `Vim` directory as a native runtime package. From this directory:

```sh
bash Vim/install.sh nvim
bash Vim/install.sh vim
# Optional explicit package destination (including paths containing spaces):
bash Vim/install.sh nvim "$HOME/.local/share/nvim/site/pack/local/start/metaml"
```

The installer creates a symlink to this checkout and refuses to overwrite any
existing path. Neovim defaults to
`${XDG_DATA_HOME:-~/.local/share}/nvim/site/pack/metaml/start/metaml`; Vim defaults
to `~/.vim/pack/metaml/start/metaml`. `VIM_AUX_DIR` overrides the package destination.
Keep this checkout in place, or copy the runtime directories instead. A plugin
manager may also use `addons/Vim` as the plugin root.

Enable runtime features in your vimrc/init.vim (Neovim normally enables these):

```vim
filetype plugin indent on
syntax enable
```

Features include syntax highlighting, comment formatting, type-variable word
motions, two-space indentation, and buffer-local commands:

| Command | Action |
|---|---|
| `:MetaMLCheck` | Run `metaml --check`; JSON diagnostics populate quickfix |
| `:MetaMLRun [ARGS...]` | Run a program; show output and exit status in a scratch split |
| `:MetaMLAST` | Show the parsed AST in a scratch split |
| `:MetaMLMammal show\|check\|build\|run\|test\|install` | Run a project command, discovering `MAMMAL` from the buffer's directory |

In a `MAMMAL` buffer, check/run invoke the project manager. Interfaces cannot be
run with `:MetaMLRun`; use check instead. Arguments use Vim command escaping
(`:MetaMLRun argument\ with\ spaces`), not shell quotation. No mappings are
installed. Commands require a saved file, never save automatically, and run
synchronously. Program stdin is not an interactive terminal. Only the current
buffer's saved state is checked; save other project buffers as needed.

Configure executable paths/options before running commands:

```vim
let g:metaml_command = '/path/to/build/metaml'
let g:metaml_options = ['--stdlib', '/path/to/metalib']
let g:metaml_mammal_command = '/path/to/build/metaml-mammal'
let g:metaml_mammal_options = ['--prefix', '/desired/install/prefix']
```

Executable variables are paths, not shell commands. Options are lists of separate
arguments. `b:metaml_command` and `b:metaml_mammal_command` override executables
per buffer. Executables otherwise resolve on `PATH`. Nothing executes on open,
save, or filetype detection. Explicit MAMMAL commands evaluate trusted project
code and may build, run, or install files.

Indentation is deliberately conservative, not a formatter: it handles explicit
blocks, delimiters, and common continuations. MetaML uses columns to recognize
top-level boundaries; review whole-buffer reindentation of complex `let`/`match`
expressions. Disable with `setlocal indentexpr=` if preferred. Buffer-local
options/commands support Vim's standard filetype undo mechanism.

## Sublime Text 4

Use **Preferences → Browse Packages…**, create a `MetaML` directory, and copy the
contents of `Sublime/` into it. Restart/reopen files as necessary. **View → Syntax
→ MetaML** selects it manually. `MAMMAL` is listed as a filename association.

The package provides a `.sublime-syntax` grammar, two-space syntax settings, and
line/block comment toggling via `Comments.tmPreferences`. Strings and comments
are separate contexts, including recursively nested block comments; no commands,
build systems, or background processes are installed by this syntax package.

## Tests

```sh
python3 addons/tests/check_addons.py --metaml build/metaml
ctest --test-dir build -R metaml_editor_addons --output-on-failure
```

The standalone script runs whichever of Vim/Neovim are installed. `--metaml` is
optional; it adds checks against a real CLI alongside the stubbed command tests.
Sublime JSON/plist settings are validated with Python's standard library; grammar
structure and regex checks additionally run when PyYAML is installed. Sublime's
native highlighting engine is not exercised by this script.
