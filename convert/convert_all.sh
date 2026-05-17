#!/bin/bash
SRC_DIR="$1"
shift

DEST_DIR="$1"
shift

DEST_EXT="$1"
shift

cd "$SRC_DIR"
find * -type f | while read in_f ;
do
    out_f="$( echo -n "$in_f" | sed -e 's/[^A-Za-z0-9._-]/./g')"
    out_f_dir="$( dirname "$out_f" )"
    mkdir -p "$DEST_DIR/$out_f_dir"
    ffmpeg -nostdin -i "$in_f" $@ "$DEST_DIR/$out_f.$DEST_EXT"
done

