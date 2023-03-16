export MY_HOME=/local/scratch/rb2018
# make clean
# make mrproper
# make distclean
# make ARCH=arm64 CC=$MY_HOME/linux-x86/clang-r468909b/bin/clang CROSS_COMPILE=aarch64-linux-gnu- -j $(nproc) defconfig
# sed -i 's/CONFIG_RANDOMIZE_BASE=y/# CONFIG_RANDOMIZE_BASE is not set/' .config
# sed -i 's/# CONFIG_NVHE_EL2_DEBUG is not set/CONFIG_NVHE_EL2_DEBUG=y/' .config
# sed -i 's/# CONFIG_KVM_ARM_HYP_DEBUG_UART is not set/CONFIG_KVM_ARM_HYP_DEBUG_UART=y/' .config

# Add kcov flag to end of .config file
# echo "CONFIG_KCOV=y" >> .config
# echo -ne '\n' | time make ARCH=arm64  CC=$MY_HOME/linux-x86/clang-r468909b/bin/clang CROSS_COMPILE=aarch64-linux-gnu- -j $(nproc)
printf '\nn\ny\n\n' | time make ARCH=arm64  CC=$MY_HOME/linux-x86/clang-r468909b/bin/clang CROSS_COMPILE=aarch64-linux-gnu- -j $(nproc)
