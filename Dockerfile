# syntax=docker/dockerfile:1

# Alpine returns an error on conan build:
# cmake command could not be found (although it is provided as a conan build requirement)
#FROM alpine:3.14
#RUN apk add --no-cache alpine-sdk py-pip

# Ubuntu and required packages
FROM ubuntu:20.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential \
    python3-pip     \
    && rm -rf /var/lib/apt/lists/*

# Conan installation and configuration
RUN pip3 install conan
RUN conan profile new default --detect && conan profile update settings.compiler.libcxx=libstdc++11 default

# Use the code from the corrent folder
COPY conan/ ./conan/
RUN conan install --build missing ../conan
COPY cmake/ ./cmake/
COPY src/ ./src/
COPY CMakeLists.txt .

# Build
RUN mkdir build && cd build
RUN conan build -sf .. ../conan


##
## Production stage
##

FROM ubuntu:20.04
# IMPORTANT: if curl is not installed, the application fails with Status: 0 on each request
# this is really worrying...
# Might be related to: https://github.com/whoshuu/cpr/issues/214
RUN apt-get update && apt-get install -y \
    curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder src/apps/binance-cli/binance-cli \
                    src/apps/dogebot/dogebot \
                    src/apps/initial-fragments/initial-fragments \
                    ./
COPY Docker/* ./
#ENTRYPOINT ["./dogebot"]
