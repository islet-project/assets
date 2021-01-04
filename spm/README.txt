This directory provides sample Secure Partitions:

-Cactus is the main test Secure Partition run at S-EL1 on top of the S-EL2
firmware. It complies with the FF-A 1.0 specification and provides sample
ABI calls for setup and discovery, direct request/response messages, and
memory sharing interfaces.

-Cactus-MM is a sample partition complying with the MM communication
interface (not related to FF-A). It is run at S-EL0 on top of TF-A's
SPM-MM implementation at EL3.

-Ivy and Quark are currently deprecated.
