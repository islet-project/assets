# To Build
. build/envsetup.sh
lunch fvp-eng
m -j<num_cpu>

# To Clean (same as 'rm -rf /out')
m clean
