FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends gcc make libc6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /trabalho
COPY . .

RUN make -C sequencial \
    && make -C processos \
    && make -C threads

CMD ["/bin/bash"]
