#!/bin/bash

DRIVE="$1"
IMAGE="$2"
SYS_SIZE="$3"
SCRIPT_DIR="$(pwd)"

dd if="${IMAGE}" of="${DRIVE}" bs=16M || exit 1
sync

( printf "d\n2\nn\n\n\n18000\n\n+${SYS_SIZE}G\nn\n\n\n18000\n\n\n\nw\n" | fdisk "${DRIVE}" ) || exit 1

e2fsck -f "${DRIVE}2" || exit 1
resize2fs "${DRIVE}2" || exit 1
mkfs.ext4 -L rw "${DRIVE}3" || exit 1

mount "${DRIVE}2" /mnt || exit 1
mkdir -p /mnt/rw || exit 1
mount "${DRIVE}3" /mnt/rw || exit 1
( cd /mnt && "${SCRIPT_DIR}/prepare-readonly.sh" ) || exit 1

( umount /mnt/rw && umount /mnt ) || exit 1
