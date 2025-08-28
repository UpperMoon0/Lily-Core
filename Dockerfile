# Use Python 3.11 for compatibility with async operations
FROM python:3.11-slim

# Set the working directory in the container
WORKDIR /app

# Copy the dependencies file to the working directory
COPY requirements.txt .

# Install any needed packages specified in requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy the content of the local directory to the working directory
COPY main.py .
COPY mcp_client.py .

# Copy environment file if it exists
COPY .env* ./

# Copy Web-Scout as a dependency (for MCP integration)
COPY ../Web-Scout ./Web-Scout/

# Make sure the Web-Scout directory is accessible
RUN chmod +x ./Web-Scout/mcp_server.py

# Expose port 8001 for Lily-Core (different from Web-Scout's 8000)
EXPOSE 8001

# Set the default command to run Lily-Core
CMD ["python", "main.py"]