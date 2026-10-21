# XBind addons

This directory is reserved for optional XBind extensions that are not part of
the core spec contract. Examples may include project-specific feature packs,
experimental target helpers, or compatibility shims for external binding tools.

Keep addons optional: the core `xbind --check-layout`, `--check-spec`, and
regimen tests must pass without loading anything from this directory.
