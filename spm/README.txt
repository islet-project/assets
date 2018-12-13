This is a prototype loosely based on the SPCI Alpha and SPRT pre-alpha
specifications. Any interface / platform API introduced for this is subject to
change as it evolves.

Cactus is meant to be the main test Secure Partition. It is the one meant to
have most of the tests that a Secure Partition has to do. Ivy is meant to be
more minimalistic. In the future, Cactus may be modified to be a S-EL1 partition
while Ivy will remain as a S-EL0 partition.
