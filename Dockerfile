# Entorno Linux/POSIX para compilar y probar msh
FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8

RUN sed -i "s/Components: main/Components: main non-free/" /etc/apt/sources.list.d/debian.sources \
    && apt-get update && apt-get install -y --no-install-recommends \
        build-essential gcc make bison flex xutils-dev \
        gdb valgrind strace \
        procps psmisc less vim nano man-db manpages-dev manpages-posix-dev \
        bash dash coreutils diffutils file ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Usuario con el mismo UID/GID que en el Mac para no dejar ficheros de root
ARG UID=501
ARG GID=20
RUN groupadd -o -g ${GID} dev && useradd -m -o -u ${UID} -g ${GID} -s /bin/bash dev
USER dev
WORKDIR /msh

RUN echo 'PS1="\[\e[32m\]linux\[\e[0m\]:\w\$ "' >> ~/.bashrc
CMD ["bash"]
