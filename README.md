# qperf
A performance measurement tool for QUIC similar to iperf.
Uses https://github.com/h2o/quicly 

# basic usage and example output
```
Usage: ./qperf [options]

Options:
  -p              port to listen on/connect to (default 18080)
  -s              run as server
  -c target       run as client and connect to target server
  -t time (s)     run for X seconds (default 10s)
  -e              measure time for connection establishment and first byte only
  -h              print this help
```

server
```
./qperf -s
starting server on port 18080
got new connection
request received, sending data
connection 0 second 0 send window: 410688458
connection 0 second 1 send window: 825786506
connection 0 second 2 send window: 1301139487
connection 0 second 3 send window: 1781791568
connection 0 second 4 send window: 1295055881
connection 0 second 5 send window: 1295055881
connection 0 second 6 send window: 634578277
connection 0 second 7 send window: 634578277
connection 0 second 8 send window: 444205689
connection 0 second 9 send window: 444206969
```
*Note*: The server looks for a TLS certificate and key in the current working dir named "server.crt" and "server.key" respectively. You can use a self signed certificate; the client doesn't validate it.


client
```
./qperf -c 127.0.0.1
running client with host=127.0.0.1 and runtime=10s
connection establishment time: 6ms
time to first byte: 7ms
second 0: 3.144 gbit/s (422030372 bytes received)
second 1: 3.444 gbit/s (462189378 bytes received)
second 2: 3.184 gbit/s (427337822 bytes received)
second 3: 3.333 gbit/s (447304096 bytes received)
second 4: 2.996 gbit/s (402100242 bytes received)
second 5: 3.274 gbit/s (439462608 bytes received)
second 6: 3.083 gbit/s (413746021 bytes received)
second 7: 3.336 gbit/s (447686682 bytes received)
second 8: 3.034 gbit/s (407235597 bytes received)
second 9: 3.02 gbit/s (405314061 bytes received)
```

# how to build
```
git clone --recurse-submodules git@github.com:rbruenig/qperf.git
mkdir build-qperf
cd build-qperf
cmake ../qperf
make
```
