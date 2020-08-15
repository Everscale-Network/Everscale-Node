#!/bin/sh

set -o errexit

find . \
  -not -path './third-party/*' \
  -not -path './example/android/third_party/*' \
  -not -path './cmake-build-debug/*' \
  -name '*.cpp' -or -name '*.hpp'
