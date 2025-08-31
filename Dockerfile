# Stage 1: Build the C++ application
FROM gcc:latest as builder

# Install CMake and Ninja for building
RUN apt-get update && apt-get install -y cmake ninja-build

# Set the working directory
WORKDIR /app

# Copy the source code
COPY . .

# Create a build directory
RUN cmake -B build -S . -G Ninja

# Build the project
RUN cmake --build build

# Stage 2: Create the final image
FROM debian:bullseye-slim

# Set the working directory
WORKDIR /app

# Copy the built executable from the builder stage
COPY --from=builder /app/build/lily .

# Expose the port the application runs on
EXPOSE 8000

# Command to run the executable
CMD ["./lily"]