#!/bin/bash
set -euo pipefail

echo "Create users start"

# Функция для создания пользователя, если он не существует
create_user_if_not_exists() {
    local username="$1"
    local password="$2"
    local is_sudo="$3"
    
    if id "$username" &>/dev/null; then
        echo "User $username already exists, updating password"
        echo "$username:$password" | chpasswd
        if [ "$is_sudo" = "true" ]; then
            usermod -aG sudo "$username" 2>/dev/null || true
        fi
    else
        echo "Creating user $username"
        useradd -m "$username" && echo "$username:$password" | chpasswd
        if [ "$is_sudo" = "true" ]; then
            adduser "$username" sudo
        fi
    fi
}

# Создание суперпользователей
if [ -f /root/superusers.txt ]; then
    while IFS=: read -r username password; do
        create_user_if_not_exists "$username" "$password" "true"
    done < /root/superusers.txt
    # НЕ удаляем файл, чтобы можно было перезапускать контейнер
fi

# Создание обычных пользователей
if [ -f /root/users.txt ]; then
    while IFS=: read -r username password; do
        create_user_if_not_exists "$username" "$password" "false"
    done < /root/users.txt
    # НЕ удаляем файл, чтобы можно было перезапускать контейнер
fi

echo "Create users done!"