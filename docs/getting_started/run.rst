Running the TF-A Tests
======================

Refer to the sections `Running the software on FVP`_ and `Running the software
on Juno`_ in `TF-A User Guide`_. The same instructions mostly apply to run the
TF-A Tests on those 2 platforms. The difference is that the following images are
not needed here:

-  Normal World bootloader. The TFTF replaces it in the boot flow;

-  Linux Kernel;

-  Device tree;

-  Filesystem.

In other words, only the following software images are needed:

-  ``BL1`` firmware image;

-  ``FIP`` image containing the following images:

   -  ``BL2``;
   -  ``SCP_BL2`` if required by the platform (e.g. Juno);
   -  ``BL31``;
   -  ``BL32`` (optional);
   -  ``tftf.bin`` (standing as the BL33 image).

Running the manual tests on FVP
```````````````````````````````
The manual tests rely on storing state in non-volatile memory (NVM) across
reboot. On FVP the NVM is not persistent across reboots, so the following
flag must be used to write the NVM to a file when the model exits.

::

        -C bp.flashloader0.fnameWrite=[filename]

To ensure the model exits on shutdown the following flag must be used:

::

        -C bp.ve_sysregs.exit_on_shutdown=1

After the model has been shutdown, this file must be fed back in to continue
the test. Note this flash file includes the FIP image, so the original fip.bin
does not need to be passed in. The following flag is used:

::

        -C bp.flashloader0.fname=[filename]

Running the FWU tests
`````````````````````

As previously mentioned in section `Putting it all together`_, there are a
couple of extra images involved when running the FWU tests. They need to be
loaded at the right addresses, which depend on the platform.

FVP
'''

In addition to the usual BL1 and FIP images, the following extra images must be
loaded:

-  ``NS_BL1U`` image at address ``0x0BEB8000`` (i.e. NS_BL1U_BASE macro in TF-A)
-  ``FWU_FIP`` image at address ``0x08400000`` (i.e. NS_BL2U_BASE macro in TF-A)
-  ``Backup FIP`` image at address ``0x09000000`` (i.e. FIP_BKP_ADDRESS macro in
   TF-A tests).

An example script is provided in `scripts/run_fwu_fvp.sh`_.

Juno
''''

The same set of extra images and load addresses apply for Juno as for FVP.

The new images must be programmed in flash memory by adding some entries in the
``SITE1/HBI0262x/images.txt`` configuration file on the Juno SD card (where
``x`` depends on the revision of the Juno board). Refer to the `Juno Getting
Started Guide`_, section 2.3 "Flash memory programming" for more
information. Users should ensure these do not overlap with any other entries in
the file.

Addresses in this file are expressed as an offset from the base address of the
flash (that is, ``0x08000000``).

::

    NOR10UPDATE: AUTO                       ; Image Update:NONE/AUTO/FORCE
    NOR10ADDRESS: 0x00400000                ; Image Flash Address
    NOR10FILE: \SOFTWARE\fwu_fip.bin        ; Image File Name
    NOR10LOAD: 00000000                     ; Image Load Address
    NOR10ENTRY: 00000000                    ; Image Entry Point

    NOR11UPDATE: AUTO                       ; Image Update:NONE/AUTO/FORCE
    NOR11ADDRESS: 0x03EB8000                ; Image Flash Address
    NOR11FILE: \SOFTWARE\ns_bl1u.bin        ; Image File Name
    NOR11LOAD: 00000000                     ; Image Load Address
    NOR11ENTRY: 00000000                    ; Image Load Address

    NOR12UPDATE: AUTO                       ; Image Update:NONE/AUTO/FORCE
    NOR12ADDRESS: 0x01000000                ; Image Flash Address
    NOR12FILE: \SOFTWARE\backup_fip.bin     ; Image File Name
    NOR12LOAD: 00000000                     ; Image Load Address
    NOR12ENTRY: 00000000                    ; Image Entry Point

--------------

*Copyright (c) 2019, Arm Limited. All rights reserved.*

.. _scripts/run_fwu_fvp.sh: ../scripts/run_fwu_fvp.sh
.. _Juno Getting Started Guide: http://infocenter.arm.com/help/topic/com.arm.doc.dui0928e/DUI0928E_juno_arm_development_platform_gsg.pdf
.. _Running the software on FVP: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/user-guide.rst#running-the-software-on-fvp
.. _Running the software on Juno: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/about/docs/user-guide.rst#running-the-software-on-juno
