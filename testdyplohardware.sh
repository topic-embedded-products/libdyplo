#!/bin/sh
/sbin/modprobe -r dyplo || true
set -e
cat /usr/share/fpga.bin > /dev/xdevcfg
/sbin/modprobe dyplo
/usr/bin/testdyplodriver $*
