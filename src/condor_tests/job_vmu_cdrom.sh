#/bin/sh

mount /dev/hdc /mnt
echo `cat /mnt/a.txt` `cat /mnt/b.txt` `cat /mnt/c.txt` > /tmp/condorout
umount /mnt
dd if=/tmp/condorout of=/dev/fd0
