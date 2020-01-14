
build:
export PATH=$PATH:$PWD/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin
mkdir out
make ARCH=arm64 O=out c330ae-perf_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- O=out -j8

out:
Kernel : arch/arm64/boot/Image.gz

clean:
make ARCH=arm64 distclean
rm -rf out