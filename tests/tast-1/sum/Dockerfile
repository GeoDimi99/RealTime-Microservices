FROM ubuntu:24.04

# Install build tools
RUN apt-get update --allow-insecure-repositories && \
    apt-get install -y --no-install-recommends --allow-unauthenticated \
        cmake \
        build-essential \
        libglib2.0-dev  \
        libjson-glib-dev \
        valgrind&&\
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy task-wrapper source

COPY services/task-wrapper/ ./task-wrapper/
COPY common/include/ ./task-wrapper/include/
COPY common/src/ ./task-wrapper/src/




WORKDIR /app/task-wrapper

# Configure and build
RUN cmake -S . -B build
RUN cmake --build build

#ENV TASK_NAME=sum
#ENV TASK_QUEUE_NAME=sum

ENV G_DEBUG=gc-friendly
ENV G_SLICE=always-malloc

#ENTRYPOINT ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--track-origins=yes", "./build/task-wrapper"]
ENTRYPOINT ["./build/task-wrapper"]

