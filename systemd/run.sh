#!/bin/bash

NODE_NAME=""
CONCURRENCY=2
DATABASE=""
WORKSHOP_USER=cm4all-workshop
OPTIONS=""

test -f /etc/default/cm4all-workshop && source /etc/default/cm4all-workshop

if test -z "$NODE_NAME"; then
    echo "No node name configured in /etc/default/cm4all-workshop" >&2
    exit 1
fi

export WORKSHOP_DATABASE=$DATABASE

exec /usr/sbin/cm4all-workshop \
     --user "$WORKSHOP_USER" \
     --name "$NODE_NAME" \
     --concurrency "$CONCURRENCY" \
     $OPTIONS
