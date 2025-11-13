# cuda-ssh
# Сборка и запуск сервера под Windows (этого сделает всё)
restart.bat

# Остановка сервера
stop.bat

# Сборка образа
docker build -t cuda-ssh .

# Запуск через docker-compose (рекоммендуется)
docker-compose up -d

# Запуск контейнера
docker run --gpus all -d -p 2222:22 cuda-ssh

# Запуск контейнера с volume
docker run --gpus all -d -p 2222:22 \
    -v /path/on/host/user1:/home/user1 \
    -v /path/on/host/user2:/home/user2 \
    cuda-ssh

# Запуск с OpenCL
docker run -d --gpus all -p 2222:22 -e NVIDIA_VISIBLE_DEVICES=all -e NVIDIA_DRIVER_CAPABILITIES=compute,utility cuda-ssh

# Вход в контейнер
docker exec -it <container_id> /bin/bash

# Перенос данных тома (volume) на другой компьютер
# Имя тома закреплено как: cuda-ssh_home-data

# Создать резервную копию (в текущую папку) с сохранением прав:
docker run --rm -v cuda-ssh_home-data:/from -v ${PWD}:/backup alpine sh -c "cd /from && tar -czf /backup/home-data.tar.gz ."

# Перенести файл home-data.tar.gz на новый компьютер и восстановить:
docker volume create cuda-ssh_home-data
docker run --rm -v cuda-ssh_home-data:/to -v ${PWD}:/backup alpine sh -c "cd /to && tar -xzf /backup/home-data.tar.gz"

# Проверить путь тома (опционально):
docker volume inspect cuda-ssh_home-data --format "{{.Mountpoint}}"

# На Windows (Docker Desktop, WSL2) данные физически лежат в:
# \\wsl$\\docker-desktop-data\\data\\docker\\volumes\\cuda-ssh_home-data\\_data
# (на некоторых версиях: \\wsl$\\docker-desktop-data\\version-pack-data\\community\\docker\\volumes\\cuda-ssh_home-data\\_data)

# Настройка SSH-клиентов
ssh user1@localhost -p 2222
ssh user2@localhost -p 2222

# Вход в контейнер
docker exec -it <container_id> /bin/bash