#!/usr/bin/env bash

function debug {
    echo "$@" >&2
}

while read LINE; do

    debug "Got line \"${LINE}\""

    case "${LINE}" in
        *\:\ !fortune)
            debug "Sending fortune"
            fortune -u -s
            ;;
        *\ has\ joined\ the\ chat.)
            WHO=$(echo "$LINE" | cut -f1 -d' ')
            echo "/msg ${WHO} Type !fortune if you want hear a fortune"
            ;;
    esac
done
