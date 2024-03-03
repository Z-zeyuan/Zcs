# Use a base image, you can choose any base image according to your needs
FROM gcc:latest

# Set the working directory inside the container
WORKDIR /app

# Copy any necessary files into the container
COPY . .

# Install any dependencies or required packages
# If you need specific command line tools, install them here
RUN gcc st1.c zcs.c multicast.c -o servicetest1 && \
    gcc service.c zcs.c multicast.c -o servicetest2 && \
    gcc app.c zcs.c multicast.c -o apptest && \
    gcc send_test.c multicast.c -o send_test && \
    gcc receiver.c multicast.c -o receive_test

# Set the default command to run when the container starts
# CMD [ "BASH" ]