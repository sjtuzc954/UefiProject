## Compile Linux

```shell
cd linux
make x86_64_defconfig
./scripts/config --file .config --enable EFIVAR_FS
./scripts/config --file .config --enable SECURITYFS
./scripts/config --file .config --enable CONFIGFS_FS
make olddefconfig
make -j$(nproc)
```

## Init edk2 submodules

```shell
cd edk2
git submodule update --init
```

## Add edk2 module and compile firmware

```shell
cd edk2
cp -r ../SmartSmm OvmfPkg
cp ../OvmfPkgX64.* OvmfPkg
source ./edksetup.sh
make -C BaseTools
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc -D SMM_REQUIRE=1
```

## Prepare root filesystem and run qemu

Prepare Debian bullseye filesystem (using script from [syzkaller](https://github.com/google/syzkaller)):

```shell
cd edk2
cp -r ../run-ovmf .
cd run-ovmf/image
chmod +x ./create-image.sh 
./create-image.sh    # needs sudo; downloading can take a relatively long time
```

Run qemu:

```shell
cd edk2/run-ovmf
chmod +x ./run.sh
./run.sh ../../linux
```

## Mount hda-contents

(In qemu)

```shell
mkdir mnt
mount -t 9p -o trans=virtio,version=9p2000.L host0 mnt
```

## Start experiment

```shell
# Compile and run victim process
gcc -o rookie mnt/rookie
./rookie

# Console prints: "Pointer p: <virt_adder>", this is the virtual address of the heap of this process.
# At first, the heap contains 10 integers from 0 to 9.
# Now press ctrl+Z to stop the rookie.
<ctrl+Z>

# Check the process id of rookie, let it be PID.
ps -aux | grep rookie

# First read some data from the heap top of process rookie.
# v2p usage: ./attack_rookie.sh PROCESS_ID VIRTUAL_ADDRESS OPTION(0:read/1:write) DATA_SIZE(BYTES) [CONTENTS_TO_WRITE(for write option)]
./attack_rookie.sh <PID> <virt_addr> 0 20

# Outside the Qemu open debug.log, we should see outputs at the very bottom.

# Then we modify the heap contents of rookie via SMI. Note that every 2 digits represents a HEX number of uint8_t. Use hyphon to connect.

./attack_rookie.sh <PID> <virt_addr> 1 4 "10-00-00-00"

# Wake rookie up. It will read in any integer input from STDIN to print the contents of its heap.
fg
<input any integer>

# We should see the first element to be 16, instead of 0.
```