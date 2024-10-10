#!/bin/bash

if which nproc > /dev/null; then
    MAKEOPTS="-j$(nproc)"
else
    MAKEOPTS="-j$(sysctl -n hw.ncpu)"
fi

# Ensure known OPEN_MAX (NO_FILES) limit.
ulimit -n 1024

########################################################################################
# c code formatting

function ci_c_code_formatting_setup {
    sudo apt-get install black
    black --version
    sudo apt-get install uncrustify
    uncrustify --version
}

function ci_c_code_formatting_run {
    python tools/codeformat.py -v
}

########################################################################################
# commit formatting

function ci_commit_formatting_run {
    git remote add upstream https://github.com/Infineon/arduino-core-psoc.git
    git fetch --depth=100 upstream main
    # If the common ancestor commit hasn't been found, fetch more.
    git merge-base upstream/main HEAD || git fetch upstream main
    # For a PR, upstream/main..HEAD ends with a merge commit into main, exclude that one.
    python tools/verifygitlog.py -v upstream/main..HEAD --no-merges
}

