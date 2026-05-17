#!/bin/bash

SOURCE_DIR=""
OUTPUT_DIR=""
FORMAT=""

usage() {
    echo "Usage: $0 -s <source_dir> -o <output_dir> -f <format>"
    exit 1
}

while getopts "s:o:f:" opt; do
    case "$opt" in
        s) SOURCE_DIR="$OPTARG" ;;
        o) OUTPUT_DIR="$OPTARG" ;;
        f) FORMAT="$OPTARG" ;;
        *) usage ;;
    esac
done

# Ensure paths are absolute to avoid "file not found" issues
SOURCE_DIR=$(realpath "$SOURCE_DIR")
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR=$(realpath "$OUTPUT_DIR")

if [[ -z "$SOURCE_DIR" || -z "$OUTPUT_DIR" || -z "$FORMAT" ]]; then
    usage
fi

# We use -print0 and IFS= read -r -d '' to safely handle any filename
find "$SOURCE_DIR" -type f -print0 | while IFS= read -r -d '' FILE; do
    
    # 1. Get the path relative to the source directory
    REL_PATH="${FILE#$SOURCE_DIR/}"
    
    DIR_PART=$(dirname "$REL_PATH")
    FILE_PART=$(basename "$REL_PATH")
    
    # 2. Create prefix from subfolders
    if [ "$DIR_PART" == "." ]; then
        PREFIX=""
    else
        PREFIX="${DIR_PART//\//_}_"
    fi
    
    # 3. Clean filename: replace everything except alphanumeric, dots, and dashes with underscores
    RAW_NAME="${PREFIX}${FILE_PART%.*}"
    # This sed command is more aggressive to ensure Unicode is stripped
    CLEAN_NAME=$(echo "$RAW_NAME" | iconv -t ascii//TRANSLIT | sed 's/[^a-zA-Z0-9._-]/_/g')
    
    OUTPUT_FILE="$OUTPUT_DIR/${CLEAN_NAME}.$FORMAT"

    echo "Processing: $FILE_PART"

    # 4. ffmpeg < /dev/null is the FIX for the "No such file" error in loops
    ffmpeg -n -i "$FILE" -vn -b:a 192k "$OUTPUT_FILE" -loglevel error < /dev/null

done

echo "---------------------------------------"
echo "Done! Files saved to: $OUTPUT_DIR"

