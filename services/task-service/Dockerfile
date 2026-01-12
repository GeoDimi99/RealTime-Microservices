FROM ubuntu:24.04

# Install build tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake  &&\
        #pkg-config \
        #libpthread-stubs0-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy task-service source
COPY . ./task-service/

WORKDIR /app/task-service

# Configure and build
RUN cmake -S . -B build
RUN cmake --build build

ENTRYPOINT ["./build/task-service"]
