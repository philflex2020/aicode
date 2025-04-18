#!/bin/bash
# Extract files from dynamic_dashboard_testpack_v6.md
# Fractal Dashboard v6 - Test Pack Extractor

BUNDLE="v6_test1.md"

if [ ! -f "$BUNDLE" ]; then
    echo "ERROR: $BUNDLE not found!"
    exit 1
fi

echo "Extracting files from $BUNDLE ..."

mkdir -p output
cd output || exit 1

# Parse the bundle
awk '
    BEGIN { in_block=0; filename=""; }
/^## File:/ {
    match($0, /## File: (.*)/, m);
    filename=m[1];
    gsub(/\//, "_", filename); # Replace slashes with underscores
    next;
}
/^```/ {
    if (in_block == 0) {
        in_block = 1;
        if (filename != "") {
            print "Extracting " filename;
            outfile=filename;
            print "" > outfile; # Clear file
        }
    } else {
        in_block = 0;
        outfile="";
    }
    next;
}
{
    if (in_block && outfile != "") {
        print $0 >> outfile;
    }
}
' "../$BUNDLE"

echo "Extraction complete!"
echo "Files are now in ./output/"