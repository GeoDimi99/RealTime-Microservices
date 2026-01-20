FROM ubuntu:24.04

# Install build tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake  &&\
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy task-service source

COPY services/task-service/ ./task-service/
COPY common/include/ ./task-service/include/
COPY common/src/ ./task-service/src/




WORKDIR /app/task-service

# Configure and build
RUN cmake -S . -B build
RUN cmake --build build

ENV TASK_NAME=sum
ENV TASK_QUEUE_NAME=sum

ENTRYPOINT ["./build/task-service"]
