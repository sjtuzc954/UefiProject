# Compile Linux

```shell
cd linux
make x86_64_defconfig
./scripts/config --file .config --enable EFIVAR_FS
./scripts/config --file .config --enable SECURITYFS
./scripts/config --file .config --enable CONFIGFS_FS
make olddefconfig
make -j$(nproc)
```

# Init edk2 submodules

```shell
cd edk2
git submodule update --init
```
