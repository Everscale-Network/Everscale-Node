#!/bin/sh

set -o errexit

if ! [ -x "$(command -v clang-format)" ]; then
  echo "clang-format is not installed"
  exit 1
fi

./src.sh | xargs -n 1 clang-format -verbose -style=file -i
