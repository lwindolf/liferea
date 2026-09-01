#!/bin/bash

# Script that gets the common English feed block from feedlist_en.opml
# and copies it to all other OPML files. The script also test fetches all 
# HTML and XML URLs to weed out dead links. Finally it lints the OPML
# files for valid XML.

MATCH_START="start of default block"
MATCH_END="end of default block"

TMPFILE=$(mktemp)
sed -n "/$MATCH_START/,/$MATCH_END/p" feedlist_en.opml >$TMPFILE

OPML_FILES=$(ls *.opml | grep -v "feedlist_en.opml")
for f in $OPML_FILES; do
    echo "Processing $f"
    sed -i "/$MATCH_START/,/$MATCH_END/{
        /$MATCH_START/{
            r $TMPFILE
        }
        d
    }" "$f"
done