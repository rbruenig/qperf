# qperf
A performance measurement tool for QUIC similar to iperf.
Uses https://github.com/h2o/quicly 

# basic usage and example output
```
Usage: ./qperf [options]

Options:
  -c target             run as client and connect to target server
  --cc [reno,cubic]     congestion control algorithm to use (default reno)
  -e                    measure time for connection establishment and first byte only
  -g                    enable UDP generic segmentation offload
  --iw initial-window   initial window to use (default 10)
  -l log-file           file to log tls secrets
  -p                    port to listen on/connect to (default 18080)
  -s                    run as server
  -t time (s)           run for X seconds (default 10s)
  -h                    print this help
```

server
```
./qperf -s
starting server with pid 5624 on port 18080
got new connection
request received, sending data
connection 0 second 0 send window: 1112923 packets sent: 364792 packets lost: 373
connection 0 second 1 send window: 1238055 packets sent: 377515 packets lost: 123
connection 0 second 2 send window: 583352 packets sent: 355482 packets lost: 862
connection 0 second 3 send window: 275563 packets sent: 367538 packets lost: 607
connection 0 second 4 send window: 1100261 packets sent: 366005 packets lost: 20
connection 0 second 5 send window: 633010 packets sent: 356021 packets lost: 857
connection 0 second 6 send window: 1266610 packets sent: 367866 packets lost: 0
connection 0 second 7 send window: 1668530 packets sent: 360649 packets lost: 0
connection 0 second 8 send window: 1994930 packets sent: 364087 packets lost: 0
connection 0 second 9 send window: 1779683 packets sent: 374804 packets lost: 80
connection 0 total packets sent: 3654759 total packets lost: 2922
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

# TLS
QUIC requires TLS, so qperf requires TLS certificates when running in server mode. It will look for a "server.crt" and "server.key" file in the current working directory.

You can create these files by creating a self-signed certificate via openssl with the following one-liner:
```
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -sha256 -days 365 -nodes -subj "/C=US/ST=Oregon/L=Portland/O=Company Name/OU=Org/CN=www.example.com"
```
