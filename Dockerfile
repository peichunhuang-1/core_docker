# Use an official Ubuntu base image
FROM ubuntu:20.04

# Avoid warnings by switching to noninteractive for the build process
ENV DEBIAN_FRONTEND=noninteractive

ENV USER=root

# Install XFCE, VNC server, dbus-x11, and xfonts-base
RUN apt-get update && apt-get install -y --no-install-recommends \
    xfce4 \
    xfce4-goodies \
    tightvncserver \
    dbus-x11 \
    xfonts-base \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Setup VNC server
RUN mkdir /root/.vnc \
    && echo "password" | vncpasswd -f > /root/.vnc/passwd \
    && chmod 600 /root/.vnc/passwd

# Create an .Xauthority file
RUN touch /root/.Xauthority

# Set display resolution (change as needed)
ENV RESOLUTION=1920x1080

# Expose VNC port
EXPOSE 5901

# Set the working directory in the container
WORKDIR /app

# Copy a script to start the VNC server
COPY start-vnc.sh start-vnc.sh
RUN chmod +x start-vnc.sh

# List the contents of the /app directory
RUN ls -a /app


# install grpc

ENV INSTALL_PREFIX=${HOME}/.local
RUN mkdir -p $INSTALL_PREFIX
ENV PATH="$INSTALL_PREFIX/bin:$PATH"

RUN apt-get update -y
RUN apt-get upgrade -y
RUN apt install -y cmake
RUN apt install -y build-essential autoconf libtool pkg-config git
RUN git clone --recurse-submodules -b v1.62.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc && \
    cd grpc && \
    mkdir -p cmake/build && \
    cd ./cmake/build  && \
    cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ../.. && \
    make -j 4 && \
    make install && \
    cd ../..
RUN apt-get install vim -y

# install valgrind
RUN apt-get install kdesdk valgrind graphviz -y

RUN rm -r grpc

RUN apt-get install net-tools

RUN apt-get install -y lsb-release

# install ROS
RUN sh -c 'echo "deb http://packages.ros.org/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list' && \
    curl -s https://raw.githubusercontent.com/ros/rosdistro/master/ros.asc | sudo apt-key add - && \
    apt update && apt install ros-noetic-desktop-full -y

RUN echo "source /opt/ros/noetic/setup.bash" >> ~/.bashrc

RUN apt install python3-rosdep python3-rosinstall python3-rosinstall-generator python3-wstool build-essential -y

RUN rosdep init && rosdep update

RUN apt-get install python3-catkin-tools -y

RUN git clone https://github.com/google/glog.git -b v0.6.0 && \
    cd glog && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX && \
    make && \
    make install && \
    cd ../..