#!/bin/sh
# Install only GPPX-owned runtime files; never rewrite the user's vimrc.
set -eu
usage() { printf '%s\n' 'Usage: install.sh [--prefix VIM_RUNTIME_DIR]' 'Default: $HOME/.vim (also accepts a Neovim runtime directory)'; }
if [ "${1-}" = --help ] || [ "${1-}" = -h ]; then
    usage
    exit 0
fi
if [ "$#" -eq 0 ]; then
    : "${HOME:?HOME is required unless --prefix is supplied}"
    dest=$HOME/.vim
elif [ "$#" -eq 2 ] && [ "$1" = --prefix ] && [ -n "$2" ]; then
    dest=$2
else
    usage >&2
    exit 2
fi
# Prefix relative paths to avoid interpreting a leading dash as an option.
case $dest in /*) ;; *) dest=./$dest ;; esac
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$dest/syntax" "$dest/indent" "$dest/ftdetect"
cp "$src/vim/syntax/vim.syntax" "$dest/syntax/gppx.vim"
cp "$src/vim/syntax/indent.vim" "$dest/indent/gppx.vim"
cat > "$dest/ftdetect/gppx.vim" <<'VIM'
augroup gppx_filetype
  autocmd!
  autocmd BufRead,BufNewFile *.gppx,*.vmwv setfiletype gppx
augroup END
VIM
printf 'Installed GPPX Vim support in %s\n' "$dest"
