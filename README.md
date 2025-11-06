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

# Настройка SSH-клиентов
ssh user1@localhost -p 2222
ssh user2@localhost -p 2222

# Вход в контейнер
docker exec -it <container_id> /bin/bash