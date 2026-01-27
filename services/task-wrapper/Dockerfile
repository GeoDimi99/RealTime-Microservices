FROM ubuntu:24.04

# Install build tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake  &&\
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

ENTRYPOINT ["./build/task-wrapper"]
