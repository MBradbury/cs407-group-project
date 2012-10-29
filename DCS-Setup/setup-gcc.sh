#!/bin/bash

cd /var/tmp
mkdir mspgcc && cd mspgcc
wget http://merlin.fit.vutbr.cz/FITkit/download/msp430gcc-patched.tgz
mkdir MSP-GCC && cd MSP-GCC
tar -xzf ../msp430gcc-patched.tgz

cd /var/tmp/mspgcc/MSP-GCC/binutils-2.18
./configure --target=msp430 --prefix=/var/tmp/mspgcc \
            --disable-nls --disable-shared --enable-debug --disable-threads \
            --with-gcc --with-gnu-as --with-gnu-ld --with-stabs \
            --disable-multilib
make
make install

export PATH=/var/tmp/mspgcc/bin:$PATH

cd /var/tmp/mspgcc/MSP-GCC/gcc-3.2.3
./configure --target=msp430 --prefix=/var/tmp/mspgcc \
            --disable-multilib --disable-libc --disable-libssp \
            --disable-intl --disable-libiberty \
            --with-gcc --with-gnu-ld --with-gnu-as --with-stabs \
            --disable-shared --disable-threads --disable-win32-registry --disable-nls \
            --enable-languages=c,c++
make
make install

cd /var/tmp/mspgcc/MSP-GCC/msp430-libc/src
make prefix=/var/tmp/mspgcc
make install prefix=/var/tmp/mspgcc

echo "export PATH=/var/tmp/mspgcc/bin:\$PATH" >> ~/.bashrc

echo ''
echo 'Cleanup'

cd /var/tmp/mspgcc
rm msp430gcc-patched.tgz
rm -rf MSP-GCC
