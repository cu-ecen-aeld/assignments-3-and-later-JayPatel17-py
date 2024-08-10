#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
ROOTFS=${OUTDIR}/rootfs
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64_be-none-linux-gnu-

export PATH=$PATH:/home/jaypatel/toolchain/arm-cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64_be-none-linux-gnu/bin

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    
    # TODO: Add your kernel build steps here
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # addiing binfmt_misc support so solve it's error
    # sed -i 's/^.*# CONFIG_BINFMT_MISC is not set.*$/CONFIG_BINFMT_MISC=y/' .config
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image $OUTDIR
sync

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${ROOTFS}" ]
then
	echo "Deleting rootfs directory at ${ROOTFS} and starting over"
    sudo rm  -rf ${ROOTFS}
fi
mkdir -p ${ROOTFS}
cd "$ROOTFS"

# TODO: Create necessary base directories
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${ROOTFS} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd "$ROOTFS"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
TOOLCHAIN_LIBC=/home/jaypatel/toolchain/arm-cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64_be-none-linux-gnu/aarch64_be-none-linux-gnu/libc
cp ${TOOLCHAIN_LIBC}/lib/ld-linux-aarch64_be.so.1 ${ROOTFS}/lib/
cp ${TOOLCHAIN_LIBC}/lib64/libc.so.6 ${ROOTFS}/lib64/
cp ${TOOLCHAIN_LIBC}/lib64/libm.so.6 ${ROOTFS}/lib64/
cp ${TOOLCHAIN_LIBC}/lib64/libresolv.so.2 ${ROOTFS}/lib64/
sync

# TODO: Make device nodes
sudo mknod -m 666 ${ROOTFS}/dev/null c 1 3
sudo mknod -m 666 ${ROOTFS}/dev/console c 5 1

# TODO: Clean and build the writer utility
cd "/home/jaypatel/coursera/Emb_Linux_Class/week_1/assignment-1-JayPatel17-py/finder-app"
make clean
make all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -r -p writer finder.sh conf/username.txt conf/assignment.txt finder-test.sh autorun-qemu.sh ${ROOTFS}/home
sync

# TODO: Chown the root directory
#sudo chown -R root:root ${ROOTFS}/*

# TODO: Create initramfs.cpio.gz
cd "$ROOTFS"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio
sync
exit 0
