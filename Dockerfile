# Use Python 3.11 for compatibility with async operations
FROM python:3.11-slim

# Set the working directory in the container
WORKDIR /app

# Copy the dependencies file to the working directory
COPY requirements.txt .

# Install any needed packages specified in requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy the Lily-Core package structure
COPY . .

# Copy environment file if it exists
COPY .env* ./

# Expose port 8000 for Lily-Core (default port for HTTP server)
EXPOSE 8000

# Create directory for conversation storage
RUN mkdir -p conversations

# Set the default command to run Lily-Core
CMD ["python", "main.py"]