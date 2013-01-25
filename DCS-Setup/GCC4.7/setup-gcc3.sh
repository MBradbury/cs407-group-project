#http://wiki.contiki-os.org/doku.php?id=msp430x
#http://comments.gmane.org/gmane.os.contiki.devel/13869
INSTALL_PREFIX="/var/tmp/mspgcc"
cd /tmp/mspgcc4.7

# 4) installing binutils in INSTALL_PREFIX
cd binutils-2.22-msp430/
../binutils-2.22/configure --target=msp430 --program-prefix="msp430-" --prefix=$INSTALL_PREFIX
make -j 5
make install

# 6) compiling gcc-4.7.0 in INSTALL_PREFIX
cd ../gcc-4.7.0-msp430
../gcc-4.7.0/configure --target=msp430 --enable-languages=c --program-prefix="msp430-" --prefix=$INSTALL_PREFIX
make -j 5
make install

# 7) compiping msp430mcu in INSTALL_PREFIX
cd ../msp430mcu-20120716
export MSP430MCU_ROOT=`pwd`
./scripts/install.sh ${INSTALL_PREFIX}/

# 8) compiling the msp430 lib in INSTALL_PREFIX
cd ../msp430-libc-20120716
cd src
export PATH=${INSTALL_PREFIX}/bin:$PATH
make -j 5
make PREFIX=$INSTALL_PREFIX install

# cleanup
# no need since every thing created in tmp
echo Reminder: remove tmp

