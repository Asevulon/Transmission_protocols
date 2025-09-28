#!/bin/bash

export PATH="$PATH:$(pwd)"

_run_completion() {
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local candidates=()

    if [[ -d "${script_dir}/install/bin" ]]; then
        while IFS= read -r -d '' file; do
            candidates+=("${file##*/}")
        done < <(find ./install/bin -maxdepth 1 -type f -print0 2>/dev/null)
    fi

    if [[ -d "${script_dir}/config" ]]; then
        while IFS= read -r -d '' file; do
            candidates+=("${file##*/}")
        done < <(find ./config -maxdepth 1 -type f -print0 2>/dev/null)
    fi

    COMPREPLY=($(compgen -W "${candidates[*]}" -- "$cur"))
}

if [[ $- == *i* ]]; then
    complete -F _run_completion run
fi