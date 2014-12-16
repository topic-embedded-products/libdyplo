#!/bin/sh
set -e
prefix=`dirname $0`
${prefix}/testdyplo
${prefix}/testdyplodriver
${prefix}/testdyplostress
