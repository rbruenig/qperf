# qperf
A performance measurement tool for QUIC similar to iperf.
Uses https://github.com/h2o/quicly 

# basic usage and example output
```
Usage: ./qperf [options]

Options:
  -c target            run as client and connect to target server
  --cc [reno,cubic]    congestion control algorithm to use (default reno)
  -e                   measure time for connection establishment and first byte only
  -g                   enable UDP generic segmentation offload
  --iw initial-window  initial window to use (default 10)
  -l log-file          file to log tls secrets
  -p                   port to listen on/connect to (default 18080)
  -s  address          listen as server on address
  -t time (s)          run for X seconds (default 10s)
  -n num_cores         number of cores to use (default nproc)
  -h                   print this help
```


#### client
```
./qperf -c 2a01:4f8:1c1b:fd7a::1 -p 18000 -n 1
starting client with host 2a01:4f8:1c1b:fd7a::1, port 18000, runtime 10s, cc reno, iw 10
connection establishment time: 16ms
time to first byte: 16ms
second 0: 1.158 gbit/s, cpu 0: 21.65%
second 1: 1.161 gbit/s, cpu 2: 50.00%
second 2: 1.205 gbit/s, cpu 0: 32.29%
second 3: 1.162 gbit/s, cpu 2: 52.87%
second 4: 1.258 gbit/s, cpu 1: 34.04%
second 5: 1.387 gbit/s, cpu 1: 50.00%
second 6: 1.393 gbit/s, cpu 2: 15.96%
second 7: 1.366 gbit/s, cpu 0: 17.35%
second 8: 1.195 gbit/s, cpu 0: 46.67%
second 9: 1.226 gbit/s, cpu 1: 15.62%
connection closed
```


#### server

```
./qperf -s :: -p 18000
starting server with pid 15291,address ::, port 18000, cc reno, iw 10
starting server with pid 15293,address ::, port 18002, cc reno, iw 10
starting server with pid 15292,address ::, port 18001, cc reno, iw 10
starting server with pid 15294,address ::, port 18003, cc reno, iw 10
got new connection
request received, sending data
connection 0 second 0 send window: 622682 packets sent: 133403 packets lost: 2289, cpu 3: 1.99%
connection 0 second 1 send window: 716581 packets sent: 126095 packets lost: 23, cpu 3: 98.99%
connection 0 second 2 send window: 652943 packets sent: 131597 packets lost: 37, cpu 3: 97.98%
connection 0 second 3 send window: 563577 packets sent: 138904 packets lost: 35, cpu 3: 96.97%
connection 0 second 4 send window: 637191 packets sent: 127947 packets lost: 43, cpu 3: 100.00%
connection 0 second 5 send window: 727889 packets sent: 119218 packets lost: 13, cpu 3: 99.00%
connection 0 second 6 send window: 577401 packets sent: 111255 packets lost: 62, cpu 2: 53.33%
connection 0 second 7 send window: 807801 packets sent: 97623 packets lost: 0, cpu 2: 100.00%
connection 0 second 8 send window: 976761 packets sent: 92145 packets lost: 0, cpu 3: 53.54%
connection 0 second 9 send window: 890324 packets sent: 104484 packets lost: 11, cpu 3: 100.00%
transport close:code=0x0;frame=0;reason=
connection 0 second 10 send window: 891604 packets sent: 0 packets lost: 0, cpu 3: -nan%
connection 0 total packets sent: 1182671 total packets lost: 2513
connection closed
```
*Note*: The server looks for a TLS certificate and key in the current working dir named "server.crt" and "server.key" respectively([See TLS](#TLS)). You can use a self signed certificate; the client doesn't validate it.


# how to build
## 1. Install required dependencies 
```
sudo apt update
sudo apt install git cmake libssl-dev libev-dev g++ -y
```
## 2.  
```
git clone --recurse-submodules https://github.com/rbruenig/qperf.git
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
