#!/bin/bash
set -euo pipefail
umask 077

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
        # Гарантировать интерактивную оболочку с историей и стрелками
        current_shell="$(getent passwd "$username" | cut -d: -f7 || echo "")"
        if [ "$current_shell" != "/bin/bash" ]; then
            usermod -s /bin/bash "$username" 2>/dev/null || true
        fi
    else
        echo "Creating user $username"
        useradd -m -s /bin/bash "$username" && echo "$username:$password" | chpasswd
        if [ "$is_sudo" = "true" ]; then
            adduser "$username" sudo
        fi
    fi

    # Обеспечить корректные права на домашнюю директорию
    local home_dir="/home/$username"
    mkdir -p "$home_dir"
    chown "$username:$username" "$home_dir"
    chmod 700 "$home_dir"
}

# Определяем источники файлов с пользователями (секреты/монтирование)
SUPERUSERS_FILE=""
USERS_FILE=""
if [ -f /run/secrets/superusers.txt ]; then SUPERUSERS_FILE="/run/secrets/superusers.txt"; elif [ -f /root/superusers.txt ]; then SUPERUSERS_FILE="/root/superusers.txt"; fi
if [ -f /run/secrets/users.txt ]; then USERS_FILE="/run/secrets/users.txt"; elif [ -f /root/users.txt ]; then USERS_FILE="/root/users.txt"; fi

# Создание суперпользователей
if [ -n "${SUPERUSERS_FILE:-}" ] && [ -f "$SUPERUSERS_FILE" ]; then
    while IFS=: read -r username password; do
        [ -z "${username:-}" ] && continue
        create_user_if_not_exists "$username" "$password" "true"
    done < "$SUPERUSERS_FILE"
fi

# Создание обычных пользователей
if [ -n "${USERS_FILE:-}" ] && [ -f "$USERS_FILE" ]; then
    while IFS=: read -r username password; do
        [ -z "${username:-}" ] && continue
        create_user_if_not_exists "$username" "$password" "false"
    done < "$USERS_FILE"
fi

# Ограничить SSH доступ только созданными пользователями (AllowUsers)
ALLOW_USERS=""
add_allow_user() {
    local u="$1"
    [ -z "$u" ] && return 0
    case " $ALLOW_USERS " in
        *" $u "*) ;;
        *) ALLOW_USERS="$ALLOW_USERS $u" ;;
    esac
}
if [ -n "${SUPERUSERS_FILE:-}" ] && [ -f "$SUPERUSERS_FILE" ]; then
    while IFS=: read -r username _; do
        add_allow_user "$username"
    done < "$SUPERUSERS_FILE"
fi
if [ -n "${USERS_FILE:-}" ] && [ -f "$USERS_FILE" ]; then
    while IFS=: read -r username _; do
        add_allow_user "$username"
    done < "$USERS_FILE"
fi
if [ -n "$ALLOW_USERS" ]; then
    if grep -q '^AllowUsers' /etc/ssh/sshd_config; then
        sed -i "s/^AllowUsers.*/AllowUsers $ALLOW_USERS/" /etc/ssh/sshd_config
    else
        echo "AllowUsers $ALLOW_USERS" >> /etc/ssh/sshd_config
    fi
fi

echo "Create users done!"