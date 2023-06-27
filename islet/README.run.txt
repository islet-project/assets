Running instruction

# Copy aosp images to islet
#   combined-ramdisk.img and system-qemu.img,
#   into ${islet}/out with new names,
#   initrd-aosp.img and system-qemu-aosp.img respectively.
FVP_BASE="out/target/product/fvpbase"
ISLET_BASE="/islet"

cp $FVP_BASE/combined-ramdisk.img $ISLET_BASE/out/initrd-aosp.img
cp $FVP_BASE/system-qemu.img $ISLET_BASE/out/system-qemu-aosp.img
Use built images
in $ISLET_BASE/scripts/fvp-cca,

def prepare_nw_aosp():
...
 #  run(["cp", PREBUILT_AOSP_INITRD, OUT], cwd=ROOT)
Build android kernel for fvp
./scripts/fvp-cca -bo -nw aosp

Running fvp with android kernel
./scripts/fvp-cca -ro -nw aosp
