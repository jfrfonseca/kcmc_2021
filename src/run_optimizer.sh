#!/bin/sh

# For adding quotes:
# cat /data/instances.csv | sed 's/^ */ "/; s/ *$/" /; s/ *| */"| "/g' >> /tmp/data.csv
# For direct copy
# cat /data/instances.csv >> /tmp/data.csv
# Replace pipe by tab
# sed -i 's/| /\t/g' /tmp/data.csv

# Filter lines with python, to avoid too-long-command-lines
rm -f /tmp/data.csv
cat /data/instances.csv | python3 -c 'import sys; [print((";".join(line.split(";", 4)[:4]))+";END\t"+(line.split("|")[-1]).strip()) for line in sys.stdin]' >> /tmp/data.csv

# Process in parallel
rm -f /tmp/*.par
parallel -a /tmp/data.csv --colsep '\t' --files /app/optimizer

# Store results
rm -f /results/optimizer.csv
cat /tmp/*.par > /results/optimizer.csv
