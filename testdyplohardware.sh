#!/bin/sh
/sbin/modprobe -r dyplo || true
set -e
cat /usr/share/fpga.bin > /dev/xdevcfg
cat /usr/share/bitstreams/adder/adder_node_1_partial.* > /dev/xdevcfg
cat /usr/share/bitstreams/adder/adder_node_2_partial.* > /dev/xdevcfg
cat /usr/share/bitstreams/adder/adder_node_3_partial.* > /dev/xdevcfg
cat /usr/share/bitstreams/adder/adder_node_4_partial.* > /dev/xdevcfg
/sbin/modprobe dyplo
/usr/bin/testdyplodriver $*
