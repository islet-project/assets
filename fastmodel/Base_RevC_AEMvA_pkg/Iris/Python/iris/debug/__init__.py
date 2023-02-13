#!/usr/bin/env python
# \brief   High level Python interface to Fast Models.
# \date    Copyright 2016 ARM Limited. All rights reserved.
"""
High level Python interface to Fast Models.

An existing model can be connected to by creating a new NetworkModel, passing
the address (IP address or hostname) and port number.

The model is comprised of multiple targets which represent the components in the
system.

A Target object can be obtained by calling Model.get_target(name) on an
instantiated model passing it the name of the target.

A convenience method Model.get_cpus() is also provided which will return a list
of Target objects for all targets for which componentType == 'Core' or have the
executesSoftware flag set.

Example:
>>> model = NetworkModel(host = 'localhost', port = 7100)
>>> cpus = model.get_cpus()
>>> cpus[0].read_register("CPSR")
>>> model.run()
"""

from .Model import NetworkModel
from .Exceptions import TargetError, TimeoutError, SecurityError, TargetBusyError, SimulationEndedError

__all__ = ["NetworkModel", "TargetError", "TimeoutError", "SecurityError",
           "TargetBusyError", "SimulationEndedError"]
