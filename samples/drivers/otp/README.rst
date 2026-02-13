.. Copyright (c) 2026 James Walmsley <james@fullfat-fs.co.uk>
.. SPDX-License-Identifier: Apache-2.0

.. _otp_sample:

OTP Sample
##########

Overview
********

This sample exercises the Zephyr OTP driver API through shell commands that
read and program a small test pattern in an OTP device.

On the ``nucleo_f413zh`` board this repository defines an OTP device bound to
the real OTP area and an ``otp_sample`` cell near the end of that region.

Notes
*****

- The sample never erases. OTP is write-once; make sure the ``otp_sample`` cell
  is blank (0xFF) before running the sample.
- The ``otp_lock`` cell corresponds to OTP lock bytes; programming those bytes
  permanently locks 32-byte OTP blocks. Leave them as 0xFF unless you intend
  to lock.
- Once programmed, the sample will detect the existing pattern and skip
  reprogramming.

Building and Running
********************

Build and flash the sample:

.. code-block:: console

   west build -b nucleo_f413zh samples/drivers/otp
   west flash

After flashing, use the shell commands below to read or program the OTP test
cell.

Shell Commands
**************

Once the sample is running, you can dump the entire OTP contents:

.. code-block:: console

   otp dump

To program the test pattern into the ``otp_sample`` cell:

.. code-block:: console

   otp program

To verify the test pattern matches ``otp_sample``:

.. code-block:: console

   otp verify

To write raw byte values at an arbitrary OTP offset:

.. code-block:: console

   otp write <offset> <byte> [byte ...]

To program one lock byte (permanently locks a 32-byte OTP block):

.. code-block:: console

   otp lock <index> [value]
