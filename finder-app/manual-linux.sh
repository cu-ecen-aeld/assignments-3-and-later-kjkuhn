#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

echo "Installing dependencies"
apt update && apt install -y curl

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

    # Kernel build steps
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all modules dtbs
    #make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} Image modules dtbs
    
    # Make sure the Image is present after the build
    if [ ! -e arch/${ARCH}/boot/Image ]; then
        echo "ERROR: Kernel Image not found after build!"
        exit 1
    fi
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

# pushd "${OUTDIR}/linux-stable"
# make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules_install INSTALL_MOD_PATH=${OUTDIR}/rootfs  
# popd

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"


cp $(${CROSS_COMPILE}gcc -print-sysroot)/lib/* ${OUTDIR}/rootfs/lib
cp $(${CROSS_COMPILE}gcc -print-sysroot)/lib64/* ${OUTDIR}/rootfs/lib64

cd "${OUTDIR}"
if [ -d "${OUTDIR}/bash-5.2" ]
then
    rm -rf "${OUTDIR}/bash-5.2"
fi
if [ -d "${OUTDIR}/bash_build" ]
then
    rm -rf "${OUTDIR}/bash_build"
fi
if [ -e "${OUTDIR}/bash-5.2.tar.gz" ]
then
    rm "${OUTDIR}/bash-5.2.tar.gz"
fi

curl -L -o bash-5.2.tar.gz https://ftp.gnu.org/gnu/bash/bash-5.2.tar.gz
tar xzf bash-5.2.tar.gz
cd bash-5.2
./configure --host=${CROSS_COMPILE%?} --prefix=${OUTDIR}/bash_build
make -j$(nproc)
make install
cp ${OUTDIR}/bash_build/bin/bash ${OUTDIR}/rootfs/bin

cd "${OUTDIR}/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/zero c 1 5
sudo mknod -m 644 dev/console c 5 1


cd ${FINDER_APP_DIR}
make clean && make CROSS_COMPILE=${CROSS_COMPILE}


cp finder.sh ${OUTDIR}/rootfs/home
cp writer ${OUTDIR}/rootfs/home
cp -r ../conf ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home
cp autorun-qemu.sh ${OUTDIR}/rootfs/home

sudo chown -R root:root "${OUTDIR}/rootfs" 

cd "${OUTDIR}/rootfs"
find . | cpio -o -H newc | gzip > "${OUTDIR}/initramfs.cpio.gz"
