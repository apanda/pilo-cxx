Yet Another PILO Simulator
=========================

Needs gcc-4.9 or later. To run on c32.

```
mkdir bin
CC=gcc-5 CXX=g++-5 cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

If you are setting up on a new machine, you want to run

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++-5
```
