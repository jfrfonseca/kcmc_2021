#!/bin/sh

parallel -a /data/gupta_instance_classes.csv --colsep ' ' --files /app/instance_generator
cat /tmp/*.par > /data/instances.csv