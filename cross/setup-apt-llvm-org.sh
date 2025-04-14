#! /bin/sh

# https://apt.llvm.org/

apt-get update
apt-get install -y curl gnupg1
curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -

cat <<EOF > /etc/apt/sources.list.d/apt-llvm-org.list
deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-20 main
EOF
