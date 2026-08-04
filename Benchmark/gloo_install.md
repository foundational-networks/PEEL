# ENV Requriement
(detailed dependencies requirement will be tested later with a fresh ubuntu)
#### 1. By Gloo Official:
- [Google Test](https://github.com/google/googletest)
- [Hiredis](https://github.com/redis/hiredis)
- [MPI](https://www.open-mpi.org/)

#### 2. Compiling:
- **OpenSSL 3+** (shipped with most systems, default)
- **OpenSSL 1.1.1w** (required by Gloo/CMake, install separately)


# Clone the repo
git clone https://github.com/foundational-networks/PEEL.git


# Initialize
cd PEEL
git submodule update --init --recursive

# enter the gloo dir
cd Benchmark/gloo/

### 3.2 Install OpenSSL 1.1.1w to a Separate Location
Download and extract OpenSSL 1.1.1w:
```bash
cd /opt/
wget https://www.openssl.org/source/openssl-1.1.1w.tar.gz
tar -xzf openssl-1.1.1w.tar.gz
cd openssl-1.1.1w
sudo apt install libz-dev
```
Configure and build:
```bash
./config --prefix=/opt/openssl-1.1.1 --openssldir=/opt/openssl-1.1.1 shared zlib
make -j$(nproc)
```
```
sudo make install
```
```
echo 'export LD_LIBRARY_PATH=/opt/openssl-1.1.1/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```
Verify the installation:
```bash
/opt/openssl-1.1.1/bin/openssl version
```
Expected output should be version 1.1.1w
```
root@savi:/opt/openssl-1.1.1w# /opt/openssl-1.1.1/bin/openssl version
OpenSSL 1.1.1w  11 Sep 2023
root@savi:/opt/openssl-1.1.1w# sudo apt install libssl-dev
```

### 3.3 Install and Verify Default OpenSSL 3 Version
Install the development package:
```bash
sudo apt install libssl-dev
```
Verify the installation:
```bash
openssl version
```
The default openssl version should be ***3.x***
```
root@vexia:/opt/openssl-1.1.1w# openssl version
OpenSSL 3.0.2 15 Mar 2022 (Library: OpenSSL 3.0.2 15 Mar 2022)
```



## 4. Install Dependency Software

### 4.1 Install Google Test
> **Note:** As of writing, the latest release is v1.15.2.
Verify the dependencies:
```bash
apt install -y build-essential cmake git

```

Clone the repository:
```bash
cd /opt
git clone https://github.com/google/googletest.git -b v1.17.0
cd googletest
```
Build and install:
```bash
mkdir build
cd build
cmake ..
```
```
make
```
```
sudo make install
```
Verify the installation:
```bash
ls /usr/local/include | grep gtest
```

### 4.2 Install Hiredis
Download and extract Hiredis:
```bash
cd /opt
wget https://github.com/redis/hiredis/archive/refs/tags/v1.3.0.tar.gz
tar -zxvf v1.3.0.tar.gz
cd hiredis-1.3.0
```
Make sure ``binutils`` is up to date
```bash
apt install -y binutils
```

Build and install:
```bash
make
```
```
sudo make install
```
```
sudo ldconfig
```
Verify the installation:
```bash
ls /usr/local/lib | grep hiredis
```

### 4.3 Install Open MPI
Install Open MPI:
```bash
sudo apt install -y openmpi-bin libopenmpi-dev
```

## 5. Install Gloo

> **Note:** This instruction is based on the latest git commit at the time of writing (hash: `661af9d17afbd6b71476097e79e0945d3db9286a`).

### 5.1 Install Dependencies for Benchmark
Install required libraries:
```bash
sudo apt install -y libhiredis-dev pkg-config
```

### 5.5 Compile and Install Gloo Benchmark
Prepare and compile benchmarks:
```bash
mkdir build_bench
cd build_bench
cmake .. \
  -DUSE_REDIS=ON \
  -DBUILD_BENCHMARK=ON \
  -DHIREDIS_INCLUDE_DIRS=/usr/local/include/hiredis \
  -DHIREDIS_LIBRARIES=/usr/local/lib/libhiredis.so \
  -DCMAKE_EXE_LINKER_FLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib" \
  -DCMAKE_SHARED_LINKER_FLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib"
make
```

Add ``` -CMAKE_CXX_COMPILER=g++-14 -DCMAKE_C_COMPILER=gcc-14 ``` if using gcc-14.

Verify the benchmarks:
```bash
ls -l gloo/benchmark/benchmark
```