# Use a base image, you can choose any base image according to your needs
FROM gcc:latest

# Set the working directory inside the container
WORKDIR /app

# Copy any necessary files into the container
COPY . .

# Install any dependencies or required packages
# If you need specific command line tools, install them here
RUN gcc st1.c zcsLan1.c multicast.c -o servicetest1 && \
    gcc service.c zcsLan1.c multicast.c -o servicetest2 && \
    gcc app1.c zcsLan1.c multicast.c -o apptest1 && \
    gcc app2.c zcsLan1.c multicast.c -o apptest2 && \
    gcc relay.c multicast.c -o relay

# Set the default command to run when the container starts
# CMD [ "BASH" ]