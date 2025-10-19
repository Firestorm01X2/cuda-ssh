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

# Вход в контейнер
docker exec -it <container_id> /bin/bash

# Настройка SSH-клиентов
ssh user1@localhost -p 2222
ssh user2@localhost -p 2222

# Вход в контейнер
docker exec -it <container_id> /bin/bash

## OpenCL на GPU: NVIDIA и AMD

### Как собрать и запустить пример OpenCL внутри контейнера
- Сборка:
  - `gcc -O2 -std=c11 -o /opt/examples/opencl_vector_add /opt/examples/opencl_vector_add.c -lOpenCL`
- Запуск c предпочтением вендора и запретом CPU-фоллбэка:
  - `OPENCL_PREFER_VENDOR=NVIDIA /opt/examples/opencl_vector_add`
  - Чтобы разрешить запуск на CPU (для отладки): `OPENCL_ALLOW_CPU=1 /opt/examples/opencl_vector_add`
- Программа печатает выбранную платформу/устройство и тип (GPU/CPU), а также время выполнения кернела. Если GPU недоступен и `OPENCL_ALLOW_CPU` не задан, выполнение завершится ошибкой.

### NVIDIA GPU (Linux/WSL2)
- Требования на хосте:
  - Драйвер NVIDIA с поддержкой CUDA
  - NVIDIA Container Toolkit (`nvidia-container-toolkit`)
- Запуск контейнера с GPU:
  - `docker run --gpus all -d -p 2222:22 --name cuda-ssh-container cuda-ssh`
  - Либо `docker compose up -d` (в `docker-compose.yml` уже указаны `gpus: all` и переменные `NVIDIA_*`).
- Проверка внутри контейнера:
  - `clinfo | head -n 40` должен показать платформу/устройство NVIDIA
  - Запустите пример: `OPENCL_PREFER_VENDOR=NVIDIA /opt/examples/opencl_vector_add`

### AMD GPU
- Важно: текущий образ базируется на `nvidia/cuda:*` и ориентирован на NVIDIA. Для AMD рекомендуется собирать отдельный образ на базе ROCm.
- Варианты:
  - Linux (bare metal) с установленными драйверами ROCm: используйте базовые образы AMD (например, `rocm/dev-ubuntu:<версия>`) и установите `rocm-opencl-runtime ocl-icd-libopencl1 clinfo build-essential openssh-server`. При запуске пробросьте устройства: `--device=/dev/kfd --device=/dev/dri --group-add video` (при необходимости добавьте `--cap-add=SYS_PTRACE --security-opt seccomp=unconfined`). Затем соберите и запустите пример как выше.
  - Docker Desktop на Windows 10 с AMD GPU: аппаратный доступ к AMD GPU из Linux-контейнера не поддерживается. Возможные варианты: запуск на Linux-хосте с ROCm, использование удалённой машины с NVIDIA/ROCm, либо обновление до Windows 11 с WSL2 и поддержкой AMD GPU (доступность зависит от драйверов/версии).
- Проверка внутри контейнера с AMD:
  - `clinfo | head -n 40` должен показывать платформу/устройство AMD (а не PoCL CPU)
  - Запуск примера: `OPENCL_PREFER_VENDOR=AMD /opt/examples/opencl_vector_add`

### Почему у вас сейчас виден только CPU (PoCL)
- В образ установлена CPU-реализация OpenCL (PoCL) как резервный вариант, поэтому при отсутствии видимого GPU OpenCL-ICD будет выбирать CPU.
- Для работы на GPU необходимо:
  - На хосте: установленный корректный драйвер (NVIDIA или AMD ROCm)
  - Проброс GPU в контейнер (`--gpus all` для NVIDIA; устройства `/dev/kfd` и `/dev/dri` для AMD)
  - Соответствующий ICD в контейнере (для NVIDIA он появится из драйвера; для AMD — из пакетов ROCm/OpenCL)