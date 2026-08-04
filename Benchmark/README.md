1. install and compile Gloo 
refers to gloo_install.md (allready added)

2. run benchmark (still remaining)


# Compiling and Running PEEL's Gloo Benchmark

This directory contains the instructions required to install the dependencies, compile Gloo, and build the Gloo benchmarks in which PEEL is integrated.

We used **Ubuntu 22.04** for our experiments. Accordingly, the instructions below are intended for **Ubuntu-based systems**.

To set up the Gloo benchmarks, follow these steps:

* [Step 1: Install OpenSSL dependencies](#step-1-install-openssl-dependencies)
* [Step 2: Install Google Test](#step-2-install-google-test)
* [Step 3: Install Hiredis](#step-3-install-hiredis)
* [Step 4: Install Open MPI](#step-4-install-open-mpi)
* [Step 5: Compile the Gloo benchmarks](#step-5-compile-the-gloo-benchmarks)


---

## Step 1: Install OpenSSL Dependencies

The build environment requires both the system OpenSSL installation and a separate installation of **OpenSSL 1.1.1w**.

### 1.1 Install OpenSSL 1.1.1w

OpenSSL 1.1.1w is installed under `/opt/openssl-1.1.1` so that it does not overwrite the system OpenSSL installation. For installation, run the following commands:

```bash
cd /opt/
wget https://www.openssl.org/source/openssl-1.1.1w.tar.gz
tar -xzf openssl-1.1.1w.tar.gz
cd openssl-1.1.1w
apt update
sudo apt-get install zlib1g-dev
./config --prefix=/opt/openssl-1.1.1 --openssldir=/opt/openssl-1.1.1 shared zlib
make -j$(nproc)
sudo make install
echo 'export LD_LIBRARY_PATH=/opt/openssl-1.1.1/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

You can then verify the installation by:

```bash
/opt/openssl-1.1.1/bin/openssl version
```

The output should report **OpenSSL 1.1.1w**, for example:

```text
OpenSSL 1.1.1w  11 Sep 2023
```

### 1.2 Install and Verify the System OpenSSL Version

To install the system OpenSSL development package:

```bash
sudo apt install libssl-dev
openssl version

```

On modern Ubuntu installations, the default OpenSSL version should be **3.x**, for example:

```text
OpenSSL 3.0.2 15 Mar 2022 (Library: OpenSSL 3.0.2 15 Mar 2022)
```

---

## Step 2: Install Google Test

To install the packages required for compiling Google Test:

```bash
sudo apt install -y build-essential cmake git
cd /opt
git clone https://github.com/google/googletest.git -b v1.17.0
cd googletest
mkdir build
cd build
cmake ..
make
sudo make install

```

Verify that the Google Test headers were installed successfully:

```bash
ls /usr/local/include | grep gtest
```

If the installation was successful, `gtest` should appear in the output.

---

## Step 3: Install Hiredis

To install Hiredis version `v1.3.0`:

```bash
cd /opt
wget https://github.com/redis/hiredis/archive/refs/tags/v1.3.0.tar.gz
tar -zxvf v1.3.0.tar.gz
cd hiredis-1.3.0
sudo apt install -y binutils
make
sudo make install
```

Refresh the shared library cache and verify the installation:

```bash
sudo ldconfig
ls /usr/local/lib | grep hiredis
```

The output should contain the installed Hiredis libraries, such as `libhiredis.so`.

---

## Step 4: Install Open MPI

Install Open MPI and its development libraries using the commands below:

```bash
sudo apt install -y openmpi-bin libopenmpi-dev
sudo apt install -y libhiredis-dev pkg-config
```

---

## Step 5: Compile the Gloo Benchmarks

**Note:** These instructions are based on Gloo commit: `661af9d17afbd6b71476097e79e0945d3db9286a`.

Now, return to the Gloo directory in the PEEL repository (`PEEL/Benchmark/gloo`). Then compile Gloo using the following commands:

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

**Note:** If the machine uses GCC 14, explicitly specify the C and C++ compilers when running CMake by adding:

```text
-DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_C_COMPILER=gcc-14
```

For example:

```bash
cmake .. \
  -DUSE_REDIS=ON \
  -DBUILD_BENCHMARK=ON \
  -DHIREDIS_INCLUDE_DIRS=/usr/local/include/hiredis \
  -DHIREDIS_LIBRARIES=/usr/local/lib/libhiredis.so \
  -DCMAKE_EXE_LINKER_FLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib" \
  -DCMAKE_SHARED_LINKER_FLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib" \
  -DCMAKE_CXX_COMPILER=g++-14 \
  -DCMAKE_C_COMPILER=gcc-14
```


### Verify the Build

After compilation completes, verify that the benchmark executable was generated:

```bash
ls -l gloo/benchmark/benchmark
```

If this file exists, the Gloo benchmark has been compiled successfully.
