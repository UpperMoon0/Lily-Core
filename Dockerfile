# Stage 1: Build the C++ application
FROM debian:bullseye AS builder

# Install build tools and dependencies
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean && \
    apt-get update && apt-get install -y \
    g++ \
    cmake \
    ninja-build \
    git \
    libboost-all-dev \
    gfortran \
    libcoarrays-openmpi-dev \
    libcpprest-dev \
    libssl-dev \
    pkg-config \
    ccache

# Set the working directory
WORKDIR /app

# Create a directory for third-party libraries
RUN mkdir -p third_party

# Clone third-party libraries
# We do this BEFORE copying source code so changes to source don't trigger a re-download of libs
RUN git clone https://github.com/nlohmann/json.git third_party/json && \
    git clone https://github.com/zaphoyd/websocketpp.git third_party/websocketpp && \
    git clone https://github.com/chriskohlhoff/asio.git third_party/asio

# Copy the source code
COPY . .

# Create a build directory
# Use ccache for compilation
ENV CCACHE_DIR=/root/.cache/ccache
RUN cmake -B build -S . -G Ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Build the project
RUN --mount=type=cache,target=/root/.cache/ccache cmake --build build

# Stage 2: Create the final image
FROM debian:bullseye-slim

# Install runtime dependencies
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean && \
    apt-get update && apt-get install -y \
    ca-certificates \
    libcpprest2.10 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libssl1.1

# Set the working directory
WORKDIR /app

# Copy the built executable from the builder stage
COPY --from=builder /app/build/lily_core .

# Copy the include directory for Swagger JSON
COPY --from=builder /app/include ./include

# Expose the port the application runs on
EXPOSE 8000
EXPOSE 9002

# Command to run the executable
CMD ["./lily_core"]