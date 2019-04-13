# qperf
A performance measurement tool for QUIC similar to iperf.

# basic usage
server
```
./qperf -s
```
*Note*: The server looks for a TLS certificate and key in the current working dir named "server.crt" and "server.key" respectively. You can use a self signed certificate; the client doesn't validate it.


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