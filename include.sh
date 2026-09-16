#!/usr/bin/env bash

## GETS THE CURRENT MODULE ROOT DIRECTORY
MOD_ITEM_LEVEL_SCALING_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source $MOD_ITEM_LEVEL_SCALING_ROOT"/conf/conf.sh.dist"

if [ -f $MOD_ITEM_LEVEL_SCALING_ROOT"/conf/conf.sh" ]; then
    source $MOD_ITEM_LEVEL_SCALING_ROOT"/conf/conf.sh"
fi
