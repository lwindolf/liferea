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
    echo "Udpating $f"
    sed -i "/$MATCH_START/,/$MATCH_END/{
        /$MATCH_START/{
            r $TMPFILE
        }
        d
    }" "$f"
done

echo "Linting OPML files..."
for f in $OPML_FILES; do
    echo "Linting $f"
    xmllint --noout "$f"
done

echo "Testing URLs in OPML files..."

declare -A already_tested
test_url() {
        url=$1
        if [ "${already_tested[$url]}" != "" ]; then
                if [ "${already_tested[$url]}" != "ok" ]; then
                        echo "- (cached) HTTP ${already_tested[$url]} $url"
                else
                        echo "- (cached) OK $url"
                fi
                return
        fi

        if [ "$url" == "vfolder" ]; then
                return
        fi

        status=$(curl -s --user-agent "Liferea/2.0 (Android 14; Mobile; https://lzone.de/liferea/) AppleWebKit (KHTML, like Gecko)" -o /dev/null -w "%{http_code}" "$url")
        if ! [[ $status =~ ^[23][0-9][0-9]$ ]]; then
                echo "- HTTP $status $url"
                already_tested[$url]=$status
        else
                already_tested[$url]="ok"
        fi
        
}

# Extract all URLs and test them
for f in $OPML_FILES; do
    echo "Checking URLs in $f"
    
    grep -oP 'htmlUrl="\K[^"]+' "$f" | while read -r url; do
        test_url "$url"
    done

    grep -oP 'xmlUrl="\K[^"]+' "$f" | while read -r url; do
        test_url "$url"
    done
done

rm "$TMPFILE"
echo "Tests complete."