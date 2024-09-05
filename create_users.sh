#!/bin/bash
set -euo pipefail

echo "Create users start"

while IFS=: read -r username password; do
    useradd -m "$username" && echo "$username:$password" | chpasswd && adduser $username sudo;
done < /root/superusers.txt
rm -f /root/superusers.txt;

while IFS=: read -r username password; do
    useradd -m "$username" && echo "$username:$password" | chpasswd
done < /root/users.txt
rm -f /root/users.txt;

echo "Create users done!"