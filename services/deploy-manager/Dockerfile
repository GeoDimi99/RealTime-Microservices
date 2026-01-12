# Single-stage Dockerfile for deploy-manager

FROM python:3.14-slim

# Environment variables
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

# Working directory
WORKDIR /app

# Runtime dependencies (Git needed by GitPython)
RUN apt-get update && \
    apt-get install -y --no-install-recommends git && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy packaging metadata first (better layer caching)
COPY pyproject.toml .

# Copy source code
COPY src/ ./src/

# Install deploy-manager and its dependencies
RUN pip install --no-cache-dir .

# Entrypoint
ENTRYPOINT ["python", "-m", "src.main"]
