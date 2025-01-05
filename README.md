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

(In edk2/run-ovmf)

```shell
chmod +x ./run.sh
./run.sh ../../linux
```

##

(In qemu)

```shell
mkdir mnt
mount -t 9p -o trans=virtio,version=9p2000.L host0 mnt
./mnt/
```