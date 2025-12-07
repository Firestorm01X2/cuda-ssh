#!/bin/bash
set -euo pipefail
umask 077

echo "Create users start"

usage() {
    cat <<'EOF'
Usage:
  /root/create_users.sh
    Создать пользователей из файлов (поведение по умолчанию, используется при старте контейнера).

  /root/create_users.sh add <username> <password> [--sudo]
    Добавить/обновить пользователя в уже запущенном контейнере и сразу выдать доступ по SSH.
    Запуск только с привилегиями root/sudo.
EOF
}

require_superuser() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "Эта операция требует привилегий суперпользователя (sudo/root)." >&2
        exit 1
    fi
}

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

# Управление списком AllowUsers для SSH
ALLOW_USERS=""
add_allow_user() {
    local u="$1"
    [ -z "$u" ] && return 0
    case " $ALLOW_USERS " in
        *" $u "*) ;;
        *) ALLOW_USERS="$ALLOW_USERS $u" ;;
    esac
}

apply_allow_users() {
    [ -z "$ALLOW_USERS" ] && return 0
    # Нормализуем пробелы и удаляем дубликаты
    ALLOW_USERS="$(
        echo "$ALLOW_USERS" | tr ' ' '\n' | sed '/^$/d' | awk '!seen[$0]++' | paste -sd' ' -
    )"
    if grep -q '^AllowUsers' /etc/ssh/sshd_config; then
        sed -i "s/^AllowUsers.*/AllowUsers $ALLOW_USERS/" /etc/ssh/sshd_config
    else
        echo "AllowUsers $ALLOW_USERS" >> /etc/ssh/sshd_config
    fi
    # Применить изменения без перезапуска контейнера
    pkill -HUP sshd 2>/dev/null || true
}

# Режим добавления пользователя в уже запущенном контейнере
if [ "${1:-}" = "add" ]; then
    require_superuser
    shift
    want_sudo="false"
    username=""
    password=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --sudo)
                want_sudo="true"
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                if [ -z "$username" ]; then
                    username="$1"
                elif [ -z "$password" ]; then
                    password="$1"
                else
                    echo "Слишком много аргументов." >&2
                    usage
                    exit 1
                fi
                shift
                ;;
        esac
    done

    if [ -z "$username" ] || [ -z "$password" ]; then
        echo "Нужно указать имя пользователя и пароль." >&2
        usage
        exit 1
    fi

    create_user_if_not_exists "$username" "$password" "$want_sudo"
    add_allow_user "$username"
    apply_allow_users

    echo "Пользователь $username создан/обновлён (sudo=$want_sudo)."
    exit 0
fi

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
        add_allow_user "$username"
    done < "$SUPERUSERS_FILE"
fi

# Создание обычных пользователей
if [ -n "${USERS_FILE:-}" ] && [ -f "$USERS_FILE" ]; then
    while IFS=: read -r username password; do
        [ -z "${username:-}" ] && continue
        create_user_if_not_exists "$username" "$password" "false"
        add_allow_user "$username"
    done < "$USERS_FILE"
fi

apply_allow_users

echo "Create users done!"