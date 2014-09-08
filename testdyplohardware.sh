#!/bin/sh
/sbin/modprobe -r dyplo || true
set -e
echo 0 > /sys/class/xdevcfg/xdevcfg/device/is_partial_bitstream
cat /usr/share/fpga.bin > /dev/xdevcfg
/sbin/modprobe dyplo
/usr/bin/testdyplodriver $*
