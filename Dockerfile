# Stage 1: Build the C++ application
FROM debian:bullseye AS builder

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    ninja-build \
    git \
    libboost-all-dev \
    gfortran \
    libcoarrays-openmpi-dev \
    libcpprest-dev \
    libssl-dev \
    pkg-config

# Set the working directory
WORKDIR /app

# Copy the source code
COPY . .

# Create a directory for third-party libraries
RUN mkdir -p third_party

# Clone third-party libraries
RUN git clone https://github.com/nlohmann/json.git third_party/json && \
    git clone https://github.com/zaphoyd/websocketpp.git third_party/websocketpp && \
    git clone https://github.com/chriskohlhoff/asio.git third_party/asio

# Create a build directory
RUN cmake -B build -S . -G Ninja

# Build the project
RUN cmake --build build

# Stage 2: Create the final image
FROM debian:bullseye-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    ca-certificates \
    libcpprest2.10 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libssl1.1 \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the built executable from the builder stage
COPY --from=builder /app/build/lily_core .

# Expose the port the application runs on
EXPOSE 8000

# Command to run the executable
CMD ["./lily_core"]