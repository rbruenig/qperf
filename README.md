# qperf
A performance measurement tool for QUIC similar to iperf.

# basic usage
server
```
./qperf -s
```

client
```
./qperf -c 127.0.0.1
```

# how to build
```
git clone --recurse-submodules git@github.com:rbruenig/qperf.git
mkdir build-qperf
cd build-qperf
cmake ../qperf
make
```