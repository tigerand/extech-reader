#!/bin/bash

IFILE="$1"
shift

dd if="$IFILE" ibs=1 skip=14 obs=20 status=none | ./extech-decode "$@"
