#!/bin/bash

# generate versioned site
rm -rf src/drivers/em/deploy
src/drivers/em/scripts/build-site.py src/drivers/em/site src/drivers/em/deploy

