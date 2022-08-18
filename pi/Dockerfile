FROM debian:stable

RUN apt-get update
RUN apt-get upgrade -y

RUN apt-get  -y install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib git cmake make

RUN git clone https://github.com/raspberrypi/pico-sdk /tmp/pico-sdk
ENV PICO_SDK_PATH=/tmp/pico-sdk

RUN apt-get install curl -y
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}" 

RUN apt-get install python3 libclang-dev build-essential -y
RUN git clone https://github.com/corrosion-rs/corrosion.git
RUN cmake -Scorrosion -Bbuild -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release
RUN cmake --install build --config Release

RUN rustup component add rustfmt --toolchain stable
RUN rustup target add thumbv6m-none-eabi
COPY build.sh build.sh

CMD ./build.sh
