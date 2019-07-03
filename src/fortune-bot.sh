#!/usr/bin/env bash

function debug() {
    echo "$@" >&2
}

read -s LINE
RRES=$?

while [ $RRES == 0 ]; do

    debug "Got line \"${LINE}\""

    case "${LINE}" in
        *\:\ !fortune)
            debug "Sending fortune"
            fortune -s
            ;;
        *\ has\ joined\ the\ chat.)
            WHO=$(echo "$LINE" | cut -f1 -d' ')
            echo "${WHO}: Type !fortune if you want hear a furtune"
            ;;
    esac

    read -s LINE
    RRES=$?

done
