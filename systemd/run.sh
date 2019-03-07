#!/bin/bash

# work around the boost::locale::facet::_S_create_c_locale exception
export LC_ALL=C

NODE_NAME=""
CONCURRENCY=2
DATABASE=""
OPTIONS=""

test -f /etc/default/cm4all-workshop && source /etc/default/cm4all-workshop

export WORKSHOP_DATABASE=$DATABASE

exec /usr/sbin/cm4all-workshop \
     --user cm4all-workshop \
     --name "$NODE_NAME" \
     --concurrency "$CONCURRENCY" \
     $OPTIONS
