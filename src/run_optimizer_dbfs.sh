#!/bin/sh

# For adding quotes:
# cat /data/instances.csv | sed 's/^ */ "/; s/ *$/" /; s/ *| */"| "/g' >> /tmp/data.csv
# For direct copy
cat /data/instances.csv >> /tmp/data.csv

# Replace pipe by tab
sed -i 's/| /\t/g' /tmp/data.csv

# Process in parallel
parallel -a /tmp/data.csv --colsep '\t' --files /app/optimizer_dbfs

# Store results
cat /tmp/*.par > /results/optimizer_dbfs.csv