#!/bin/bash

# Change dir to the directory where this script is located
cd $(dirname $0)
echo ${PWD}

cores_psoc_dir="${PWD}/../cores/psoc"
core_api_submodule_dir="${PWD}/../extras/arduino-core-api"

function git_submodule_setup {
    git submodule init
    git submodule update
}

function core_api_setup {
    # Create symbolic link (overwrite) to the api/ 
    # folder of ArduinoCore-API submodule.
    # Note: Symlinks might not work without full paths
    # https://stackoverflow.com/questions/8601988/symlinks-not-working-when-link-is-made-in-another-directory
    ln -sf ${core_api_submodule_dir}/api ${cores_psoc_dir}
}

# Check if a function name is passed as an argument
if [ $# -gt 0 ]; then
    if declare -f "$1" > /dev/null; then
        "$1"
    else
        echo "Function $1 not found"
        exit 1
    fi
else
    git_submodule_setup
    core_api_setup
fi
