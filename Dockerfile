FROM ubuntu:focal as base

WORKDIR /cpp/src/cpsolver

RUN apt-get update \
	&& apt-get -y install git wget build-essential \
	&& wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
	&& echo "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-15 main" | tee -a /etc/apt/sources.list \
	&& echo "deb-src http://apt.llvm.org/focal/ llvm-toolchain-focal-15 main" | tee -a /etc/apt/sources.list \
	&& apt-get update \
	&& DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -y install tzdata \
	&& apt-get -y install clang-15 clang-tools-15 clang-15-doc libclang-common-15-dev libclang-15-dev libclang1-15 clang-format-15 python3-clang-15 clangd-15 clang-tidy-15 lld-15 libc++-15-dev libc++abi-15-dev libomp-15-dev libc++-15-dev libc++abi-15-dev libunwind-15-dev


FROM base AS cmakebuild

RUN apt-get -y install openssl libssl-dev \
	&& wget https://github.com/Kitware/CMake/releases/download/v3.18.6/cmake-3.18.6.tar.gz \
	&& tar -zvxf cmake-3.18.6.tar.gz \
	&& rm cmake-3.18.6.tar.gz \
	&& cd cmake-3.18.6 \
	&& ./bootstrap \
	&& make -j4 \
	&& make install


FROM cmakebuild as ortoolbuild

RUN git clone -b main https://github.com/google/or-tools \
	&& cd or-tools \
	&& git fetch --all --tags --prune \
	&& git checkout v9.6 \
	&& cmake -S . -B build -DBUILD_DEPS=ON \
	&& cmake --build build --config Release --target all -j 4 -v \
	&& cmake --build build --config Release --target install -v

FROM ortoolbuild as crystalinstall

RUN apt-get install -y curl \
	&& echo 'deb http://download.opensuse.org/repositories/devel:/languages:/crystal/xUbuntu_20.04/ /' | tee /etc/apt/sources.list.d/devel:languages:crystal.list \
	&& curl -fsSL https://download.opensuse.org/repositories/devel:languages:crystal/xUbuntu_20.04/Release.key | gpg --dearmor | tee /etc/apt/trusted.gpg.d/devel_languages_crystal.gpg > /dev/null \
	&& apt-get update \
	&& apt-get install -y crystal

FROM crystalinstall AS build

# COPY ./src /cpp/src/cpsolver

ENTRYPOINT while true; do sleep 1; done