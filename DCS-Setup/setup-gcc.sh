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
make -j 5
make install

# Only add to the path variable if we haven't done so before
if [[ "$PATH" != */var/tmp/mspgcc/bin* ]]
then
	echo "export PATH=/var/tmp/mspgcc/bin:\$PATH" >> ~/.bashrc
	export PATH=/var/tmp/mspgcc/bin:$PATH
fi

cd /var/tmp/mspgcc/MSP-GCC/gcc-3.2.3
./configure --target=msp430 --prefix=/var/tmp/mspgcc \
            --disable-multilib --disable-libc --disable-libssp \
            --disable-intl --disable-libiberty \
            --with-gcc --with-gnu-ld --with-gnu-as --with-stabs \
            --disable-shared --disable-threads --disable-win32-registry --disable-nls \
            --enable-languages=c,c++
make -j 5
make install

cd /var/tmp/mspgcc/MSP-GCC/msp430-libc/src
make prefix=/var/tmp/mspgcc -j 5
make install prefix=/var/tmp/mspgcc

echo ''
echo 'Cleanup'

cd /var/tmp/mspgcc
rm msp430gcc-patched.tgz
rm -rf MSP-GCC
