1. install and compile gloo 
refers to gloo_install.md

2. run benchmark

TBA



# Gloo Benchmark Setup

This directory contains the instructions required to install the dependencies, compile Gloo, and build the Gloo benchmarks in which PEEL is integrated.

We used **Ubuntu 22.04** for our experiments. Accordingly, the instructions below are intended for **Ubuntu-based systems**.

To set up the Gloo benchmarks, follow these steps:

* [Step 1: Clone the PEEL repository](#step-1-clone-the-peel-repository)
* [Step 2: Install OpenSSL dependencies](#step-2-install-openssl-dependencies)
* [Step 3: Install Google Test](#step-3-install-google-test)
* [Step 4: Install Hiredis](#step-4-install-hiredis)
* [Step 5: Install Open MPI](#step-5-install-open-mpi)
* [Step 6: Compile the Gloo benchmarks](#step-6-compile-the-gloo-benchmarks)

The main software dependencies are:

* [Google Test](https://github.com/google/googletest)
* [Hiredis](https://github.com/redis/hiredis)
* [Open MPI](https://www.open-mpi.org/)
* **OpenSSL 3+**, provided by modern Ubuntu installations
* **OpenSSL 1.1.1w**, installed separately for compatibility with Gloo/CMake

---

## Step 1: Clone the PEEL Repository

Clone the PEEL repository:

```bash
git clone https://github.com/foundational-networks/PEEL.git
```

Enter the repository:

```bash
cd PEEL
```

Initialize all Git submodules:

```bash
git submodule update --init --recursive
```

Then enter the Gloo directory:

```bash
cd Benchmark/gloo/
```

---

## Step 2: Install OpenSSL Dependencies

The build environment requires both the system OpenSSL installation and a separate installation of **OpenSSL 1.1.1w**.

### 2.1 Install OpenSSL 1.1.1w

OpenSSL 1.1.1w is installed under `/opt/openssl-1.1.1` so that it does not overwrite the system OpenSSL installation.

Download and extract OpenSSL 1.1.1w:

```bash
cd /opt/
wget https://www.openssl.org/source/openssl-1.1.1w.tar.gz
tar -xzf openssl-1.1.1w.tar.gz
cd openssl-1.1.1w
```

Update the package index:

```bash
apt update
```

Install the zlib development package required for compiling OpenSSL:

```bash
sudo apt-get install zlib1g-dev
```

> **Note:** On Ubuntu/Debian, the package is called `zlib1g-dev`, not `libz-dev`.

Configure OpenSSL to install under `/opt/openssl-1.1.1`:

```bash
./config --prefix=/opt/openssl-1.1.1 --openssldir=/opt/openssl-1.1.1 shared zlib
```

Compile OpenSSL:

```bash
make -j$(nproc)
```

Install it:

```bash
sudo make install
```

Add the OpenSSL 1.1.1 library directory to `LD_LIBRARY_PATH`:

```bash
echo 'export LD_LIBRARY_PATH=/opt/openssl-1.1.1/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

Verify the installation:

```bash
/opt/openssl-1.1.1/bin/openssl version
```

The output should report **OpenSSL 1.1.1w**, for example:

```text
OpenSSL 1.1.1w  11 Sep 2023
```

### 2.2 Install and Verify the System OpenSSL Version

Install the system OpenSSL development package:

```bash
sudo apt install libssl-dev
```

Verify the default OpenSSL installation:

```bash
openssl version
```

On modern Ubuntu installations, the default OpenSSL version should be **3.x**, for example:

```text
OpenSSL 3.0.2 15 Mar 2022 (Library: OpenSSL 3.0.2 15 Mar 2022)
```

At this point, the system should have both:

* OpenSSL 3.x as the default system OpenSSL.
* OpenSSL 1.1.1w installed separately under `/opt/openssl-1.1.1`.

---

## Step 3: Install Google Test

Install the packages required for compiling Google Test:

```bash
apt install -y build-essential cmake git
```

Clone Google Test version `v1.17.0`:

```bash
cd /opt
git clone https://github.com/google/googletest.git -b v1.17.0
cd googletest
```

Create a build directory and configure the project:

```bash
mkdir build
cd build
cmake ..
```

Compile Google Test:

```bash
make
```

Install it:

```bash
sudo make install
```

Verify that the Google Test headers were installed successfully:

```bash
ls /usr/local/include | grep gtest
```

If the installation was successful, `gtest` should appear in the output.

---

## Step 4: Install Hiredis

Download Hiredis version `v1.3.0`:

```bash
cd /opt
wget https://github.com/redis/hiredis/archive/refs/tags/v1.3.0.tar.gz
tar -zxvf v1.3.0.tar.gz
cd hiredis-1.3.0
```

Make sure `binutils` is installed and up to date:

```bash
apt install -y binutils
```

Compile Hiredis:

```bash
make
```

Install it:

```bash
sudo make install
```

Refresh the shared-library cache:

```bash
sudo ldconfig
```

Verify the installation:

```bash
ls /usr/local/lib | grep hiredis
```

The output should contain the installed Hiredis libraries, such as `libhiredis.so`.

---

## Step 5: Install Open MPI

Install Open MPI and its development libraries:

```bash
sudo apt install -y openmpi-bin libopenmpi-dev
```

Before compiling the Gloo benchmark, also install the Hiredis development package and `pkg-config`:

```bash
sudo apt install -y libhiredis-dev pkg-config
```

---

## Step 6: Compile the Gloo Benchmarks

> **Note:** These instructions are based on Gloo commit:
> `661af9d17afbd6b71476097e79e0945d3db9286a`.

Return to the Gloo directory in the PEEL repository. For example, if PEEL was cloned into your home directory:

```bash
cd ~/PEEL/Benchmark/gloo/
```

Create the benchmark build directory:

```bash
mkdir build_bench
cd build_bench
```

Configure Gloo with benchmark and Redis support:

```bash
cmake .. \
  -DUSE_REDIS=ON \
  -DBUILD_BENCHMARK=ON \
  -DHIREDIS_INCLUDE_DIRS=/usr/local/include/hiredis \
  -DHIREDIS_LIBRARIES=/usr/local/lib/libhiredis.so \
  -DCMAKE_EXE_LINKER_FLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib" \
  -DCMAKE_SHARED_LINKER_FLAGS="-L/usr/local/lib -Wl,-rpath,/usr/local/lib"
```

Compile Gloo:

```bash
make
```

### Using GCC 14

If the machine uses GCC 14, explicitly specify the C and C++ compilers when running CMake by adding:

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

Then compile:

```bash
make
```

### Verify the Build

After compilation completes, verify that the benchmark executable was generated:

```bash
ls -l gloo/benchmark/benchmark
```

If this file exists, the Gloo benchmark has been compiled successfully.
