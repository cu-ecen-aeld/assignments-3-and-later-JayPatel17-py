#!/bin/sh
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
# Editor: jay Patel

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

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
    cd "${OUTDIR}/linux-stable"
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
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    	sudo rm  -rf ${OUTDIR}/rootfs
fi
mkdir -p ${OUTDIR}/rootfs

# TODO: Create necessary base directories
echo "Creating necessary base directories for the root filesystem"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd "${OUTDIR}/busybox"
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd "${OUTDIR}/busybox"
fi

# TODO: Make and install busybox
echo "Make and install busybox"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "Adding library dependencies to rootfs"
TOOLCHAIN_LIBC=$(${CROSS_COMPILE}gcc -print-sysroot)
cp ${TOOLCHAIN_LIBC}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp ${TOOLCHAIN_LIBC}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/
cp ${TOOLCHAIN_LIBC}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp ${TOOLCHAIN_LIBC}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
sync

# TODO: Make device nodes
echo "Making device modes"
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Clean and build the writer utility"
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=${CROSS_COMPILE} all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copy the finder related scripts and executables to the /home directory on the target rootfs"
cp -r -p writer.o finder.sh conf/username.txt conf/assignment.txt finder-test.sh autorun-qemu.sh ${OUTDIR}/rootfs/home
sync

# TODO: Chown the root directory
#echo "Chown the root dirctory"
#sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
echo "Create initramfs.cpio.gz"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio
