#!/bin/bash

set -euo pipefail

# ---- Configuration ----
PYTHON_VERSION="3.9.25"
INSTALL_PREFIX="/opt/python-${PYTHON_VERSION}"
TMP_DIR="/tmp/python-build-${PYTHON_VERSION}"

# ---- Install dependencies ----
apt-get update
apt-get install -y \
  build-essential \
  libssl-dev \
  zlib1g-dev \
  libncurses5-dev \
  libncursesw5-dev \
  libreadline-dev \
  libsqlite3-dev \
  libgdbm-dev \
  libdb5.3-dev \
  libbz2-dev \
  libexpat1-dev \
  liblzma-dev \
  tk-dev \
  wget \
  curl \
  xz-utils \
  libffi-dev

# ---- Prepare build directory ----
rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"
cd "${TMP_DIR}"

# ---- Download and extract Python source ----
curl -O "https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tgz"
tar -xf "Python-${PYTHON_VERSION}.tgz"
cd "Python-${PYTHON_VERSION}"

# ---- Build and install ----
./configure --prefix="${INSTALL_PREFIX}" --enable-optimizations
make -j"$(nproc)"
make install
rm -rf "${TMP_DIR}"
# ---- Verify ----
"${INSTALL_PREFIX}/bin/python3" --version

echo "Python ${PYTHON_VERSION} installed to ${INSTALL_PREFIX}"
