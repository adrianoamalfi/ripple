#!/usr/bin/env bash
# Prints the CHANGELOG.md section for one version (without the heading).
#   changelog-notes.sh 0.1.0
set -euo pipefail
version="${1:?usage: changelog-notes.sh <version>}"
awk -v v="$version" '
  $0 ~ "^## \\[" v "\\]" { found = 1; next }
  found && /^## \[/ { exit }
  found { print }
' CHANGELOG.md | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}'
