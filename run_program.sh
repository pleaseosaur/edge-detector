#!/bin/bash

# check if an argument was provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <directory>"
    exit 1
fi

# check if the argument is a directory
DIRECTORY=$1

# check if the specified directory exists
if [ ! -d "$DIRECTORY" ]; then
    echo "Directory $DIRECTORY does not exist."
    exit 1
fi

# initialize an array to store the image files
image_files=()

# store the image files in the array
for file in "$DIRECTORY"/*.ppm; do
    if [ -e "$file" ]; then
        image_files+=("$file")
    fi
done

# check if no image files were found
if [ ${#image_files[@]} -eq 0 ]; then
    echo "No image files found in $DIRECTORY."
    exit 1
fi

# compile the edge_detector
gcc -o edge_detector edge_detector.c -lm -lpthread

# run the edge_detector
echo "Running edge_detector on image files in $DIRECTORY..."
./edge_detector "${image_files[@]}"

echo "Images processed successfully"
