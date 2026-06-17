#!/bin/sh
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT
#
# Docker entrypoint: generate SSL keystores on first start, then run the kernel.

set -eu
export PATH="/opt/java/openjdk/bin:${PATH}"
cd "$(dirname "$0")"

if [ ! -f config/keystore.p12 ] || [ ! -f config/truststore.p12 ]; then
  echo "Generating SSL keystore/truststore for openTCS Kernel..."
  chmod +x ./generateKeystores.sh ./startKernel.sh
  ./generateKeystores.sh
fi

exec ./startKernel.sh
