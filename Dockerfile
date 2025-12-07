FROM nvidia/cuda:12.8.1-devel-ubuntu22.04

# Установка переменных для cuda для всех типов оболочек (bash, sh и других)
RUN echo 'export PATH=/usr/local/cuda/bin:$PATH' >> /etc/profile.d/cuda.sh && \
    echo 'export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH' >> /etc/profile.d/cuda.sh && \
    chmod +x /etc/profile.d/cuda.sh


# Установка необходимых пакетов
RUN apt update && \
    apt install -y --no-install-recommends \
      openssh-server sudo \
      openmpi-bin libopenmpi-dev \
      mc \
      # OpenCL: общий загрузчик ICD, инструменты и заголовки
      ocl-icd-libopencl1 ocl-icd-opencl-dev clinfo opencl-headers \
      # Инструменты сборки
      build-essential cmake git pkg-config ninja-build \
      python3-dev libpython3-dev \
      libhwloc-dev zlib1g zlib1g-dev libxml2-dev dialog apt-utils wget && \
    rm -rf /var/lib/apt/lists/*

# LLVM/Clang для сборки PoCL (совместимо с PoCL 6.0)
ENV LLVM_VERSION=14
RUN apt update && \
    apt install -y --no-install-recommends \
      libclang-${LLVM_VERSION}-dev clang-${LLVM_VERSION} llvm-${LLVM_VERSION} \
      libclang-cpp${LLVM_VERSION}-dev libclang-cpp${LLVM_VERSION} llvm-${LLVM_VERSION}-dev && \
    rm -rf /var/lib/apt/lists/*

# Сборка и установка PoCL (CUDA backend). Линкуемся со стубами CUDA для сборки в контейнере
RUN git clone --branch v6.0 https://github.com/pocl/pocl /tmp/pocl && \
    mkdir -p /tmp/pocl/build && \
    cd /tmp/pocl/build && \
    cmake \
      -DCMAKE_C_FLAGS="-L/usr/local/cuda/lib64/stubs" \
      -DCMAKE_CXX_FLAGS="-L/usr/local/cuda/lib64/stubs" \
      -DWITH_LLVM_CONFIG=/usr/bin/llvm-config-${LLVM_VERSION} \
      -DENABLE_HOST_CPU_DEVICES=OFF \
      -DENABLE_CUDA=ON .. && \
    make -j"$(nproc)" && \
    make install && \
    ldconfig && \
    mkdir -p /etc/OpenCL/vendors && \
    cp /usr/local/etc/OpenCL/vendors/pocl.icd /etc/OpenCL/vendors/pocl.icd && \
    rm -rf /tmp/pocl

# По умолчанию использовать CUDA-девайс PoCL
ENV POCL_DEVICES=cuda

# Создание директории для SSH
RUN mkdir /var/run/sshd

# Создание пользователей и установка паролей. Сделаем в скрипте запуска, а не тут,  
# чтобы папки пользователей создались уже после монтированию волюмов
#RUN useradd -m user1 && echo 'user1:password1' | chpasswd # && adduser user1 sudo
#RUN useradd -m user2 && echo 'user2:password2' | chpasswd # && adduser user2 sudo

# Настройки безопасности SSH
RUN sed -i 's/^#\?PermitRootLogin.*/PermitRootLogin no/' /etc/ssh/sshd_config && \
    sed -i 's/^#\?PasswordAuthentication.*/PasswordAuthentication yes/' /etc/ssh/sshd_config && \
    sed -i 's/^#\?ChallengeResponseAuthentication.*/ChallengeResponseAuthentication no/' /etc/ssh/sshd_config && \
    sed -i 's/^#\?X11Forwarding.*/X11Forwarding no/' /etc/ssh/sshd_config && \
    echo 'PermitEmptyPasswords no' >> /etc/ssh/sshd_config

# Открытие порта 22
EXPOSE 22

# Файлы с пользователями не копируем в образ (безопасность);
# монтируются как секреты/файлы во время запуска

# Каталог для секретов: запретить доступ не-root
RUN mkdir -p /run/secrets && chown root:root /run/secrets && chmod 700 /run/secrets

# Копирование скрипта для добавления пользователей
COPY create_users.sh /root/create_users.sh
RUN chmod +x /root/create_users.sh

# Копирование примеров в общий каталог пользователей
COPY examples /home/examples
RUN chown -R root:root /home/examples && \
    find /home/examples -type d -exec chmod 755 {} \; && \
    find /home/examples -type f -exec chmod 644 {} \;

# Запуск скрипта создания пользователей и SSH-сервера
CMD bash -c "/root/create_users.sh && \
			/usr/sbin/sshd -D"