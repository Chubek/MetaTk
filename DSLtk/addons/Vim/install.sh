#!/usr/bin/env bash
set -euo pipefail

editor="${1:-nvim}"
case "$editor" in
  vim) default="${HOME:?}/.vim/pack/metaml/start/metaml" ;;
  nvim) default="${XDG_DATA_HOME:-${HOME:?}/.local/share}/nvim/site/pack/metaml/start/metaml" ;;
  *) echo "Usage: $0 [vim|nvim] [package-directory]" >&2; exit 2 ;;
esac
if (( $# > 2 )); then
  echo "Usage: $0 [vim|nvim] [package-directory]" >&2
  exit 2
fi
source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
destination="${2:-${VIM_AUX_DIR:-$default}}"
if [[ -e "$destination" || -L "$destination" ]]; then
  echo "Refusing to replace existing path: $destination" >&2
  exit 1
fi
mkdir -p -- "$(dirname -- "$destination")"
ln -s -- "$source_dir" "$destination"
echo "Installed MetaML for $editor at $destination"
