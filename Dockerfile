FROM nvidia/cuda:12.6.0-devel-ubuntu24.04

# Установка переменных для cuda для всех типов оболочек (bash, sh и других)
RUN echo 'export PATH=/usr/local/cuda/bin:$PATH' >> /etc/profile.d/cuda.sh && \
    echo 'export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH' >> /etc/profile.d/cuda.sh && \
    chmod +x /etc/profile.d/cuda.sh


# Установка необходимых пакетов
RUN apt update && \ 
	apt install -y openssh-server sudo && \ 
	apt install -y mc

# Создание директории для SSH
RUN mkdir /var/run/sshd

# Создание пользователей и установка паролей. Сделаем в скрипте запуска
#RUN useradd -m user1 && echo 'user1:password1' | chpasswd # && adduser user1 sudo
#RUN useradd -m user2 && echo 'user2:password2' | chpasswd # && adduser user2 sudo

# Разрешение входа root и пользователей через SSH
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config

# Открытие порта 22
EXPOSE 22

# Запуск SSH-сервера
#CMD ["/usr/sbin/sshd", "-D"]

# Тут специально useradd выполняется в скрипте запуска, чтобы создались папки пользователей
# уже после монтирования volume
CMD bash -c "useradd -m user1 && echo 'user1:password1' | chpasswd && \
             useradd -m user2 && echo 'user2:password2' | chpasswd && \
             /usr/sbin/sshd -D"

# Сборка и запуск контейнера
#docker build -t cuda-ssh .
#docker run --gpus all -d -p 2222:22 cuda-ssh

#docker run --gpus all -d -p 2222:22 \
#    -v /path/on/host/user1:/home/user1 \
#    -v /path/on/host/user2:/home/user2 \
#    cuda-ssh

# Настройка SSH-клиентов
# ssh user1@localhost -p 2222
# ssh user2@localhost -p 2222

#Вход в контейнер
#docker exec -it <container_id> /bin/bash