FROM nvidia/cuda:12.6.0-devel-ubuntu24.04

# Установка переменных для cuda для всех типов оболочек (bash, sh и других)
RUN echo 'export PATH=/usr/local/cuda/bin:$PATH' >> /etc/profile.d/cuda.sh && \
    echo 'export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH' >> /etc/profile.d/cuda.sh && \
    chmod +x /etc/profile.d/cuda.sh


# Установка необходимых пакетов
RUN apt update &&\ 
    apt install -y openssh-server sudo &&\
    apt install -y openmpi-bin libopenmpi-dev &&\
    apt install -y mc &&\
	apt install -y build-essential pkg-config ocl-icd-opencl-dev clinfo

# Создание директории для SSH
RUN mkdir /var/run/sshd

# Создание пользователей и установка паролей. Сделаем в скрипте запуска, а не тут,  
# чтобы папки пользователей создались уже после монтированию волюмов
#RUN useradd -m user1 && echo 'user1:password1' | chpasswd # && adduser user1 sudo
#RUN useradd -m user2 && echo 'user2:password2' | chpasswd # && adduser user2 sudo

# Разрешение входа root и пользователей через SSH
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config

# Открытие порта 22
EXPOSE 22

# Копирование файла с  супер пользователями
COPY superusers.txt /root/superusers.txt

# Копирование файла с пользователями
COPY users.txt /root/users.txt

# Копирование скрипта для добавления пользователей
COPY create_users.sh /root/create_users.sh
RUN chmod +x /root/create_users.sh

# Запуск скрипта создания пользователей и SSH-сервера
CMD bash -c "/root/create_users.sh && \
			/usr/sbin/sshd -D"