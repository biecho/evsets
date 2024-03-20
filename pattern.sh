#!/bin/bash

sudo ./evsets | awk '/^0x/ {print $0 ","} /^$/ {print "}, {"}'