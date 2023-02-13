#!/usr/bin/env python
# \file   Breakpoint.py
# \brief  Interface to a breakpoint in an Iris target
#
# \date   Copyright ARM Limited 2016 All Rights Reserved.

import iris.iris as iris

from threading import Event
from warnings import warn

_breakpoint_type_names = {'code':'Program', 'data':'Memory', 'register':'Register'}


class BreakpointBase(object):
    """Provides a high level interface to breakpoints."""

    def __init__(self, target, bpt_info):
        self.target = target
        self.info = bpt_info

        # bptId is just used to initialize self._number.
        self._number = bpt_info.bptId

    @property
    def memory_space(self):
        """
        The name of the memory space that this breakpoint is set in

        Only valid for code and data breakpoints
        """
        if self.info.type in ('code', 'data'):
            return self.target._memory_spaces_by_number[self.info.spaceId].name
        else:
            raise AttributeError("This type of breakpoint does not have a memory space")

    @property
    def address(self):
        """
        The memory address that this breakpoint is set on

        Only valid for code and data breakpoints
        """
        if self.info.type in ('code', 'data'):
            return self.info.address
        else:
            raise AttributeError("This type of breakpoint does not have an address")

    @property
    def register(self):
        """
        The name of the register that this breakpoint is set on

        Only valid for register breakpoints
        """
        if self.info.type == 'register':
            return self.target.all_registers_by_id[self.info.rscId].name
        else:
            raise AttributeError("This type of breakpoint does not have a register")

    @property
    def on_read(self):
        """
        True if this breakpoint is triggered on read

        Only valid for register and memory breakpoints
        """
        if self.info.type in ('data', 'register'):
            return 'r' in self.info.get('rwMode', 'rw')
        else:
            raise AttributeError("This type of breakpoint does not have a trigger type")

    @property
    def on_write(self):
        """
        True if this breakpoint is triggered on write

        Only valid for register and memory breakpoints
        """
        if self.info.type in ('data', 'register'):
            return 'w' in self.info.get('rwMode', 'rw')
        else:
            raise AttributeError("This type of breakpoint does not have a trigger type")

    @property
    def on_modify(self):
        """
        Deprecated

        True if this breakpoint is triggered on modify

        Only valid for register and memory breakpoints
        """
        warn('Deprecated method: Breakpoint.on_modify')

    @property
    def bpt_type(self):
        """
        The name of the breakpoint type

        Valid values are:
            * Program  (code)
            * Memory   (data)
            * Register (register)

        Such names are compatible with name in fm.debug.
        """
        return _breakpoint_type_names[self.info.type]

    @property
    def number(self):
        """
        Identification number of this breakpoint

        This is the same as the key in the Target.breakpoints dictionary.

        If the number is non-negative, it is equal to the bptId and the breakpoint is enabled.
        If the number is negative, the breakpoint is disabled.

        This is only valid until the breakpoint is deleted, and breakpoint
        numbers can be re-used and modified.
        """
        return self._number

    @number.setter
    def number(self, value):
        self._number = value

    @property
    def enabled(self):
        """True if the breakpoint is currently enabled"""
        return self.number >= 0

    @enabled.setter
    def enabled(self, value):
        if value:
            self.enable()
        else:
            self.disable()

    def enable(self):
        """Enable the breakpoint if supported by the model."""
        if self.enabled:
            return

        bptId = self.target.client.irisCall().breakpoint_set(instId = self.target.instId,
                                                            type = self.info.type,
                                                            spaceId = self.info.get('spaceId', ''),
                                                            address = self.info.get('address', 0),
                                                            rscId = self.info.get('rscId', 0),
                                                            rwMode = self.info.get('rwMode', ''))

        # Change the key in Target.breakpoints
        self.target.breakpoints.pop(self.number)
        self.number = bptId
        self.target.breakpoints[self.number] = self

    def disable(self):
        """Disable the breakpoint if supported by the model."""
        if not self.enabled:
            return

        self.target.client.irisCall().breakpoint_delete(instId = self.target.instId,
                                                       bptId = self.number)

        # Change the key in Target.breakpoints
        self.target.breakpoints.pop(self.number)
        self.number = self.target.next_dis_bpt_idx
        self.target.breakpoints[self.number] = self
        self.target.next_dis_bpt_idx -= 1  # Decrease the index by 1

    def delete(self):
        """
        Remove the breakpoint from the target
        """
        if self.enabled:
            self.target.client.irisCall().breakpoint_delete(instId = self.target.instId,
                                                           bptId = self.number)
        self.target.breakpoints.pop(self.number)
        self.number = None

    def __repr__(self):
        additional = []
        additional.append("enabled=%s" % self.enabled)
        if self.info.type == 'code':
            additional.append("address=%s" % hex(self.info.address).rstrip("lL"))
        if self.info.type == 'data':
            additional.append("address=%s" % hex(self.info.address).rstrip("lL"))
        if self.info.type == 'register':
            additional.append("register=%s" % self.target.all_registers_by_id[self.info.rscId].name)
        type_name = _breakpoint_type_names[self.info.type]
        return "<Breakpoint type=%s %s number=%d>" % (type_name,
                                                      " ".join(additional),
                                                      self.number)


class AsyncBreakpoint(BreakpointBase):
    def __init__(self, target, bpt_info):
        BreakpointBase.__init__(self, target, bpt_info)
        self.triggered = Event()

    # This is called by the debug thread when the simulation starts running
    def _clear_event(self):
        """Clear the trigger event"""
        self.triggered.clear()

    # This is called by debug thread when the breakpiont is hit
    def _set_event(self):
        """Set the trigger event"""
        self.triggered.set()

    @property
    def is_hit(self):
        """True if the breakpoint was hit last time the target was running"""
        return self.triggered.is_set()

    def wait(self, timeout = None):
        """
        Block until the breakpoint is triggered or the timeout expires.
        Returns True if the breakpoint was triggered, False otherwise.
        """
        return self.triggered.wait(timeout)


class SyncBreakpoint(BreakpointBase):
    def __init__(self, target, bpt_info):
        BreakpointBase.__init__(self, target, bpt_info)
        self.__triggered = False

    # This is called by the debug thread when the simulation starts running
    def _clear_event(self):
        """Clear the trigger event"""
        self.__triggered = False

    # This is called by debug thread when the breakpiont is hit
    def _set_event(self):
        """Set the trigger event"""
        self.__triggered = True

    @property
    def is_hit(self):
        """True if the breakpoint was hit last time the target was running"""
        return self.__triggered
