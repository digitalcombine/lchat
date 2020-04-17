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
            fortune -u -s
            ;;
        *\ has\ joined\ the\ chat.)
            WHO=$(echo "$LINE" | cut -f1 -d' ')
            echo "/msg ${WHO} Type !fortune if you want hear a fortune"
            ;;
    esac

    read -s LINE
    RRES=$?
    while [ $RRES == 0 -a -z "$LINE" ]; do
        read -s LINE
        RRES=$?
    done

done
