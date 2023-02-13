#!/usr/bin/env python
# \file   Target.py
# \brief  High-level Python interface to Iris models.
#
# \date   Copyright ARM Limited 2016 All Rights Reserved.

import iris.iris as iris
from .Breakpoint import AsyncBreakpoint, SyncBreakpoint
from .Parameters import ParameterDict
from .Stream import Stream
from .EventCallbackManager import EventCallbackManager
from .EventBufferManager import EventBufferManager

import struct
import sys
from numbers import Integral
from collections import namedtuple
from warnings import warn

RegisterInfo = namedtuple("RegisterInfo", ['name', 'description', 'bits_wide',
                                           'has_side_effects', 'is_compound',
                                           'symbols', 'lsb_offset', 'type',
                                           'is_readable', 'is_writable'])

TableInfo    = namedtuple("TableInfo",    ['name', 'columns', 'description',
                                           'max_index', 'min_index'])

ColumnInfo   = namedtuple("ColumnInfo",   ['name', 'description', 'bits_wide',
                                           'type', 'is_readable', 'is_writable'])

EventFieldInfo = namedtuple('EventFieldInfo', ['name', 'description', 'size', 'type',
                                               'enum_value_to_symbol', 'is_signed'])

EventSourceInfo = namedtuple('EventSourceInfo', ['evSrcId', 'name',
                                                 'description', 'fields'])


class TargetEventSourceInfo(object):
    """Holds the list of EventSource and their properties published by a target
    """

    def __init__(self, client, target_instId):
        self._event_info = []
        self._event_name_to_event_info = {}

        # Initialise Event Source info
        iris_event_info = iris.try_iris(client.irisCall().event_getEventSources,
                                        {}, instId=target_instId)
        for event in iris_event_info:
            info = TargetEventSourceInfo.to_event_source_info(event)
            self._event_info.append(info)
            self._event_name_to_event_info[event.name] = info

    @classmethod
    def to_event_source_info(cls, event):
        """Turns an Iris EventSourceInfo to a Python EventSourceInfo

        :param event
            An Iris's EventSourceInfo instance as returned by
            event_getEventSource.
        """
        fields = []
        for field in event.fields:
            size = None
            type = None
            enums = {}
            is_signed = None
            if field.type in ('uint', 'int'):
                size = field.size
                type = int
                is_signed = field.type == 'int'
            elif field.type == 'bool':
                size = field.size # Should be 1
                type = bool
            elif field.type == 'float':
                size = field.size
                type = float
            elif field.type == 'string':
                type = str
            elif field.type == 'bytes':
                type = bytes
            elif field.type == 'uint[]':
                type = []
            elif field.type == 'object':
                type = dict
            else:
                # Don't prevent the creation of the EventCallbackManager
                # for an unknown field type as we can still register a
                # functional event handler for it.
                warnings.warn('Unrecognised event field type "{}"'.format(field.type))

            for enum_info in field.get('enums', []):
                enums[enum_info.value] = enum_info.get('symbol', '')

            field_info = EventFieldInfo(field.name, field.get('description', ''), size, type, enums, is_signed)
            fields.append(field_info)

        return EventSourceInfo(event.evSrcId, event.name, event.get('description', ''), fields)


    def get_info(self):
        """
        Yeilds EventSourceInfo for all events in the target instance.
        """
        for event in self._event_info:
            yield event

    def get_info_by_name(self, name):
        try:
            return self._event_name_to_event_info[name]
        except KeyError:
            raise ValueError('Unknown event "{}"'.format(name))

    def get_evSrcId(self, name):
        """Get the event source Id for the named event"""
        try:
            return self._event_name_to_event_info[name].evSrcId
        except KeyError:
            raise ValueError('Unknown event "{}"'.format(name))


class TargetBase(object):
    """
    Wraps an Iris object providing a simplified interface to common tasks.

    RegInfos can be accessed using methods, for example:

        cpu.write_register("Core.R5", 1000)
    """

    def __init__(self, instInfo, model):
        self.instName = instInfo.instName
        self.instId = instInfo.instId
        self.client = model.client
        self.model = model

        self.long_register_names = {}
        self.short_register_names = {}
        self.ambiguous_register_names = []

        self.all_registers_by_id = {}

        self.properties = iris.try_iris(self.client.irisCall().instance_getProperties,
                                        {}, instId = self.instId)

        self.pc_info = None
        self.pc_name_prefix = ''
        self.pc_space_id_info = None

        self.event_buffer = None

        group_info_list = iris.try_iris(self.client.irisCall().resource_getListOfResourceGroups, [], instId = self.instId)
        param_info_list = []
        for group_info in group_info_list:
            resource_info_list = iris.try_iris(self.client.irisCall().resource_getList, [], instId = self.instId, group = group_info.name)
            for resource_info in resource_info_list:
                if 'registerInfo' in resource_info:
                  self.long_register_names[group_info.name + '.' + resource_info.name] = resource_info;

                  tags = resource_info.get('tags', {})
                  if tags.get('isPc', 0) != 0:
                      self.pc_info = resource_info
                      self.pc_name_prefix = group_info.name + '.'
                  if tags.get('isPcSpaceId', 0) != 0:
                      self.pc_space_id_info = resource_info

                  self.all_registers_by_id[resource_info.rscId] = resource_info

                  short_name = resource_info.name
                  # if the name is already ambiguous then we don't need to do anything
                  if short_name in self.ambiguous_register_names:
                      continue

                  # check if the register name is ambiguous now
                  if short_name in self.short_register_names:
                      self.short_register_names.pop(short_name)
                      self.ambiguous_register_names.append(short_name)
                  else:
                      self.short_register_names[short_name] = resource_info

                elif 'parameterInfo' in resource_info:
                  param_info_list.append(resource_info)

        self.parameters = ParameterDict(self, param_info_list)

        memory_spaces = iris.try_iris(self.client.irisCall().memory_getMemorySpaces, [], instId = self.instId)
        self._memory_spaces_by_name = {space.name: space for space in memory_spaces}
        self._memory_spaces_by_number = {space.spaceId: space for space in memory_spaces}

        # warnings will be generated if more than one of these matches for each short name
        # It is acceptable for any number of these to not match an existing name, as not all
        # targets will have a memory space for each alias
        self.__add_memspace_alias("Normal", "N")
        self.__add_memspace_alias("Memory", "N")
        self.__add_memspace_alias("Guest", "N")

        self.__add_memspace_alias("Secure", "S")
        self.__add_memspace_alias("Secure Monitor", "S")

        self.__add_memspace_alias("NS Hyp", "H")

        self.__add_memspace_alias("Physical Memory (Non Secure)", "NP")
        self.__add_memspace_alias("Physical Memory", "NP")

        self.__add_memspace_alias("Physical Memory (Secure)", "SP")

        self.supports_per_inst_exec_control = False
        try:
            self.supports_per_inst_exec_control = self.client.irisCall().instance_checkFunctionSupport(
                    instId = self.instId, functions = [{'name':'perInstanceExecution_getState'}])
        except iris.error.IrisError:
            pass

        self.supports_disassembly = False

        self._disass_modes = iris.try_iris(self.client.irisCall().disassembler_getModes, [], instId = self.instId)
        if self._disass_modes != []:
            self.supports_disassembly = True

        # initialize event handling
        self._event_info = TargetEventSourceInfo(self.client, self.instId)
        self._event_manager = self._new_event_manager()

        # enable breakpoints if supported
        self.breakpoints = {}
        supports_breakpoint = iris.try_iris(self.client.irisCall().instance_checkFunctionSupport, False,
                                            instId = self.instId, functions = [{'name':'breakpoint_set'}])
        if supports_breakpoint:
            bpt_infos = iris.try_iris(self.client.irisCall().breakpoint_getList, [], instId = self.instId)
            for bpt_info in bpt_infos:
                self._insert_breakpoint(bpt_info)

            # The negative index for the next disabled breakpoint.
            # It will be decreased by 1 each time a breakpoint is disabled.
            self.next_dis_bpt_idx = -1;

            self.add_event_callback("IRIS_BREAKPOINT_HIT", self.__on_bpt_hit)

        # Get the list of tables for this target.
        table_list = iris.try_iris(self.client.irisCall().table_getList, [], instId = self.instId)
        self._table_info_by_name = {table_info.name: table_info for table_info in table_list}


    def __add_memspace_alias(self, existing_name, new_name):
        if existing_name in self._memory_spaces_by_name:
            if new_name in self._memory_spaces_by_name:
                if self.model.verbose:
                    warn("Multiple memory spaces with alias '{}' found in target '{}'".format(new_name, self.instance_name or ''))
                return
            self._memory_spaces_by_name[new_name] = self._memory_spaces_by_name[existing_name]

    def _new_breakpoint(self, bpt_info):
        """Creates a new breakpoint

        To be specialized.
        """
        raise NotImplementedError()

    def _new_event_manager(self):
        """Called once to create the event manager

        To be specialized.

        The event manager is responsible for handling event callbacks for this
        instance.
        """
        raise NotImplementedError()

    # this method is called by the receive thread in iris library
    def __on_bpt_hit(self, time, fields):
        bptId = fields['BPT_ID']
        if bptId in self.breakpoints:
            self.breakpoints[bptId]._set_event()

    @property
    def component_type(self):
        return self.properties.get('componentType')

    @property
    def description(self):
        return self.properties.get('description')

    @property
    def executes_software(self):
        return self.properties.get('executesSoftware', 0) != 0

    @property
    def instance_name(self):
        return self.instName

    @property
    def target_name(self):
        return self.properties.get('name')

    @property
    def is_running(self):
        """Returns True if the target is currently running"""
        # Return True if both simulation time is running and the execution of this instance is enabled
        simTimeObj = iris.try_iris(self.client.irisCall().simulationTime_get, None,
                                   instId = 1)
        if simTimeObj and simTimeObj.running:
            # True if it doesn't support per-instance execution control
            if not self.supports_per_inst_exec_control:
                return True
            return self.get_execution_state()
        return False

    def clear_bpts(self):
        for bpt_obj in self.breakpoints.values():
            bpt_obj._clear_event()

    def get_execution_state(self):
        """
        Returns True if the execution state is enabled

        Raises ValueError if can not get the execution state
        """
        try:
            result = self.client.irisCall().perInstanceExecution_getState(instId = self.instId)
        except iris.error.IrisError as e:
            raise ValueError("Cannot get the execution state: {}".format(e))

        return result.enable

    def set_execution_state(self, enable):
        """
        Set the execution state

        Raises ValueError if can not set the execution state
        """
        try:
            self.client.irisCall().perInstanceExecution_setState(instId = self.instId, enable = enable)
        except iris.error.IrisError as e:
            raise ValueError("Cannot set the execution state: {}".format(e))

    def get_steps(self, unit = 'instruction'):
        """
        Returns the remaining steps

        Raises ValueError if can not get the remaining steps
        """
        try:
            return self.client.irisCall().step_getRemainingSteps(instId = self.instId, unit = unit)
        except iris.error.IrisError as e:
            raise ValueError("Failed to get the remaining steps: {}".format(e))

    def set_steps(self, steps, unit = 'instruction'):
        """
        Set the remaining steps

        Raises ValueError if can not set the remaining steps
        """
        try:
            self.client.irisCall().step_setup(instId = self.instId, steps = steps, unit = unit)
        except iris.error.IrisError as e:
            raise ValueError("Cannot set the remaining steps: {}".format(e))

    def _get_register_by_name(self, name):
        """
        Returns register info of a register

        :param name: Name of the register

        Raises ValueError if the register name does not exist, or if the group
        name is omitted and there is more than one regInfos for 'name' in
        different groups (ambiguous)
        """
        if name in self.long_register_names:
            return self.long_register_names[name]
        elif name in self.short_register_names:
            return self.short_register_names[name]
        elif name in self.ambiguous_register_names:
            raise ValueError("Register name '%s' is ambiguous (exists in multiple register groups)" % name)
        raise ValueError("Register '%s' does not exist" % name)

    def has_register(self, name):
        """
        Does the target have the named register.

        :param name: Name of the register

        Returns True if the register exists and has an unambiguous name, or False otherwise.
        """
        if name in self.long_register_names or name in self.short_register_names:
            return True
        else:
            return False

    def read_register(self, name=None, side_effects = False, rscId=None):
        """
        Read the current value of a register

        :param name: The name of the register to read from. If rscId is provided, 'name' is ignored

        :param side_effects: Deprecated

        :param rscID: Resource id of the register to read from. If this is provided, 'name' is ignored

        """
        if not (name or rscId != None):
          raise ValueError('Either register name or rscID is required')

        if side_effects:
            warn('Deprecated parameter: side_effects.')

        if rscId != None:
          reg_info = self.all_registers_by_id[rscId]
        else:
          reg_info = self._get_register_by_name(name)

        read_result = self.client.irisCall().resource_read(instId = self.instId, rscIds = [reg_info.rscId])
        reg_info_and_values = iris.decodeResourceReadResult([reg_info], read_result)
        for (regInfo, value, undefinedBits, errorCode) in reg_info_and_values:
            if errorCode != None:
                raise ValueError('Register read failed: {}'.format(iris.error.IrisErrorCode(errorCode)))
            return value

    def read_all_registers(self):
      """
      Read the current value of all registers

      Returns dictionary of read register values keyed off register id

      Raises ValueError if reading failed on a readable register
      """
      reg_ids = list(self.all_registers_by_id.keys())
      reg_infos = list(self.all_registers_by_id.values())
      read_results = self.client.irisCall().resource_read(instId = self.instId, rscIds = reg_ids)

      reg_info_and_values = iris.decodeResourceReadResult(reg_infos, read_results)
      return_dict = {}
      for (regInfo, value, undefinedBits, errorCode) in reg_info_and_values:
        if errorCode != None:
          return_dict[regInfo.rscId] = '???'
        else:
          return_dict[regInfo.rscId] = value

      return return_dict

    def write_register(self, name=False, value=None, side_effects = False, rscId=None):
        """
        Write a value to a register

        :param name: The name of the register to write to. If rscId is provided, 'name' is ignored

        :param value: The value to write to the register

        :param side_effects: Deprecated

        :param rscID: Resource id of the register to write to. If this is provided, 'name' is ignored

        """
        if not (name or rscId != None):
          raise ValueError('Either register name or rscID is required')

        if side_effects:
            warn('Deprecated parameter: side_effects.')

        if rscId != None:
          reg_info = self.all_registers_by_id[rscId]
        else:
          reg_info = self._get_register_by_name(name)

        value_args = {
            'data': [], # data is mandatory, event if only writing strings.
        }

        reg_type = reg_info.get('type', 'numeric')

        if reg_type == 'string':
            value_args['strings'] = [value]
        else:
            value_args['data'] = iris.to_bitpattern(value, reg_info.bitWidth)

        write_result = self.client.irisCall().resource_write(instId = self.instId, rscIds = [reg_info.rscId], **value_args)

        if 'error' in write_result:
            rscId, code = write_result.error
            raise ValueError('Register write failed: {}'.format(iris.error.IrisErrorCode(code)))

    def get_register_info(self, name = None):
        """
        This method can be used to retrieve information about about the registers
        present in this Target.

        It is used in two ways:
            * ``get_register_info(name)``
                will return the info for the named register
            * ``get_register_info()``
                The function acts as a generator and will yield information about
                all registers.

        :param name:
            The name of the register to provide info for.

            If None, it will yield information about all registers.

            It follows the same rules as the ``name`` parameter of
            :py:meth:`.read_register` and :py:meth:`.write_register`
        """

        def fix_name(info, name):
            value_type = { 'numeric' : int,
                           'numericSigned': int,
                           'numericFp': float,
                           'string' : str,
                           'noValue' : type(None),
            }[info.get('type', 'numeric')]

            return RegisterInfo(name,
                                info.get('description'),
                                info.get('bitWidth'),
                                None,  # Register info has no 'hasSideEffects'
                                None,  # Register info has no 'isCompound'
                                {e.value : e.symbol for e in info.get('enums', [])},
                                info.get('lsbOffset', 0),
                                value_type,
                                'r' in info.get('rwMode', 'rw'),
                                'w' in info.get('rwMode', 'rw'))
        def info_generator():
            for name in self.long_register_names:
                info = self.long_register_names[name]
                yield fix_name(info, name)

        if name is None:
            return info_generator()
        else:
            reg_info = self._get_register_by_name(name)
            return fix_name(reg_info, reg_info.name)

    def disassemble(self, address, count = 1, mode = None, memory_space = None):
        """
        Disassemble intructions.

        If count=1 this method will return a 3-tuple of:
            addr, opcode, disass

        where:
            addr is the address of the instruction
            opcode is a string containing the instruction opcode at that address
            disass is a string containing the disassembled representation of the instruction

        If count is > 0, this method will behave like a generator function which
        yields one 3-tuple per instruction disassembled.

        :param address:
            Address to start disassembling from.

        :param count:
            Number of instructions to disassemble.
            Default value is 1.

        :param mode:
            Disassembly mode to use.Must be either None (the target's current mode will
            be used) or one of the values returned by get_disass_modes().
            Default value is None

        :param memory_space:
            Memory space for address. Must be the name of a valid memory space for this
            target or None. If None, the current memory space will be used.
            Default value is None

        Raises ValueError if the target does not support disassembly

        This method may yield fewer than count times if an error occurs during disassembly.
        """
        if not self.supports_disassembly:
            raise NotImplementedError("Target {} does not support disassembly"
                  .format(self.instance_name))
        if mode != None and mode != 'Current' and\
           mode not in [item.name for item in self._disass_modes]:
            raise ValueError("Disassembly mode {} not valid".format(mode))
        if count < 1:
            raise ValueError("Argument count is less than 1: 'count = {}'".format(count))

        # the ID of the memory space
        spaceId = None

        # get the 'Current' mode if none is specified
        mode = 'Current' if mode is None else mode

        if memory_space != None:
            space = self._memory_spaces_by_name.get(memory_space, None)
            if space is None:
                raise ValueError("Target {} has no memory space called {}"
                      .format(self.instance_name, memory_space))
            spaceId = space.spaceId
        else:
            read_res = self.client.irisCall().resource_read(
                                              instId = self.instId,
                                              rscIds = [self.pc_space_id_info.rscId]
                                              )
            (_, spaceId, _, errorCode) = next(iris.decodeResourceReadResult([self.pc_space_id_info],
                                                                       read_res))
            if errorCode != None:
                raise ValueError("PC space id read failed: %s" % errorCode)
            space = self._memory_spaces_by_number.get(spaceId, None)

        # raise ValueError if no memory_space is provided and no PC id has been found
        if spaceId == None:
            raise ValueError("No space id found to disassemble from in target {}"
                  .format(self.instance_name))

        result = self.client.irisCall().disassembler_getDisassembly(instId = self.instId,
                                                                    address = address,
                                                                    count = count,
                                                                    mode = mode,
                                                                    spaceId = spaceId)

        return (instr for instr in result)

    def get_disass_modes(self):
        """Returns the disassembly modes for this target"""
        return self._disass_modes

    @property
    def disass_mode(self):
        """Returns the current disassembly mode for this target"""
        return self.client.irisCall().disassembler_getCurrentMode(instId = self.instId)

    def get_instruction_count(self):
        """Return Target's current instruction count"""
        return iris.try_iris(self.client.irisCall().step_getStepCounterValue, 0,
                             instId = self.instId, unit = "instruction")

    def get_pc(self):
        """
        Returns the current value of program counter
        """
        if not self.pc_info:
            raise ValueError("Target does not have a PC register")
        return self.read_register(self.pc_name_prefix + self.pc_info.name)

    def load_application(self, filename, loadData = None, verbose = None, parameters = None):
        """
        Load an application to run on the model.

        :param filename:    The filename of the application to load.
        :param loadData:    Deprecated.
                            If set to True, the target loads data, symbols, and code.
                            If set to False, the target does not reload the application
                            code to its program memory. This can be used, for example,
                            to either:

                            * forward information about applications that are loaded
                              to a target by other platform components.
                            * change command line parameters for an application that
                              was loaded by a previous call.

        :param verbose:     Deprecated.
                            Set this to True to allow the target to print verbose.
                            messages.
        :param parameters:  Deprecated.
                            FM never supported this anyway.
                            A list of command line parameters to pass to the application
                            or None.
        """
        if loadData or verbose or parameters:
            warn('Deprecated parameters: loadData, verbose and parameters.')

        # Pass the file name directly as we could not check a file on a remote system.
        self.client.irisCall().image_loadFile(instId = self.instId, path = filename)

    def _get_address_space(self, space_name):
        """
        Gets the MemorySpaceInfo object with the name ``space_name``

        If 'space_name' is None, returns the default memory space, which may
        depend on the current state of the target.

        Raises ValueError if the target does not have a memory
        space with the name space_name.
        """
        if space_name is None:
            return self._get_default_memory_space()
        else:
            try:
                return self._memory_spaces_by_name[space_name]
            except KeyError:
                raise ValueError("Memory space '%s' does not exist in this target" % space_name)

    def _get_default_memory_space(self):
        """
        Gets the MemorySpaceInfo object of the default memory space
        """

        if self.pc_space_id_info:
            return self._memory_spaces_by_number[self.read_register(self.pc_space_id_info.name)]
        elif len(self._memory_spaces_by_number) > 0:
            return tuple(self._memory_spaces_by_number.values())[0]
        else:
            raise ValueError("Target does not have any memory spaces")

    def read_memory(self, address, memory_space = None, size = 1, count = 1, do_side_effects = False):
        """
        :param address:
            Address to begin reading from.

        :param memory_space:
            Name of the memory space to read or None which will read the core's current memory space.

        :param size:
            Size of memory access unit in bytes. Must be one of 1, 2, 4 or 8.
            Note that not all values are supported by all models.
            Note that the data is always returned as bytes, so calling with
            size=4, count=1 will return a byte array of length 4.

        :param count:
            Number of units to read.

        :param do_side_effects: Deprecated
            If True, the target must perform any side-effects normally triggered
            by the read, for example clear-on-read.

        Returns a bytearray of length size*count

        """

        space = self._get_address_space(memory_space)
        if address < space.get('minAddr', 0):
            raise ValueError("Address is below minimum address of memory space '%s'"
                            % space.name)

        if address > space.get('maxAddr', 0xffffFFFFffffFFFF):
            raise ValueError("Address is above maximum address of memory space '%s'"
                            % space.name)

        if size not in [1, 2, 4, 8]:
            raise ValueError("'size' must be 1, 2, 4 or 8")
        if count == 0:  # Read nothing
            return bytearray()

        result = iris.try_iris(self.client.irisCall().memory_read, None,
                               instId = self.instId, spaceId = space.spaceId,
                               address = address, byteWidth = size, count = count)

        if result is None or "error" in result:
            raise ValueError("Failed to read memory at address '%s'" % address)

        # The read data is u64 array (little endian), pack it to a string.
        data_str = struct.pack("<{}Q".format(len(result.data)), *result.data)
        return bytearray(data_str)[0:size * count]

    def write_memory(self, address, data, memory_space = None, size = 1, count = None, do_side_effects = False):
        """
        :param address:
            Address to begin reading from

        :param data:
            The data to write.
            If count is 1, this may be an integer
            Otherwise it must be a bytearray with length >= size*count

        :param memory_space:
            memory space to read.
            Default is None which will read the core's current memory space.

        :param size:
            Size of memory access unit in bytes. Must be one of 1, 2, 4 or 8.
            Note that not all values are supported by all models.

        :param count:
            Number of units to write. If None, count is automatically calculated
            such that all data from the array is written to the target

        :param do_side_effects: Deprecated
            If True, the target must perform any side-effects normally triggered
            by the write, for example triggering an interrupt.
        """

        space = self._get_address_space(memory_space)

        if address < space.get('minAddr', 0):
            raise ValueError("Address is below minimum address of memory space '%s'"
                            % space.name)

        if address > space.get('maxAddr', 0xffffFFFFffffFFFF):
            raise ValueError("Address is above maximum address of memory space '%s'"
                            % space.name)

        if size not in [1, 2, 4, 8]:
            raise ValueError("'size' must be 1, 2, 4 or 8")

        if isinstance(data, Integral):
            if count is None:
                # skip the count detection below, we know the count is 1
                count = 1
            elif count != 1:
                raise TypeError("data must be given as a bytearray for writes with count > 1")

            if data < 0 or data >= 2 ** (size * 8):
                raise ValueError('Data value "{}" is out of range for size "{}"'
                                 'Must be in the range(0, 2**(size*8))'.format(data, size))

            u64_array = [data]
        else:
            if count is None:
                count = len(data) / size
                if len(data) % size != 0:
                    raise ValueError("len(data) must be a multiple of size")

            if not isinstance(data, bytearray) or len(data) < size * count:
                raise ValueError("'data' must be either an integer, or a bytearray with length >= size*count")
            if count == 0:  # Write nothing
                return

            u64_array = iris.convertBytearrayToU64Array(data)

        write_result = iris.try_iris(self.client.irisCall().memory_write, None,
                                     instId = self.instId, spaceId = space.spaceId,
                                     address = address, byteWidth = size,
                                     count = count, data = u64_array)
        if write_result is None or "error" in write_result:
            raise ValueError("Failed to write memory at address '%s'" % address)

    def _insert_breakpoint(self, bpt_info):
        """Create and append a Breakpoint object to the bpt_obj list"""
        bpt_obj = self._new_breakpoint(bpt_info)
        self.breakpoints[bpt_obj.number] = bpt_obj
        return bpt_obj

    def add_bpt_prog(self, address, memory_space = None):
        """
        Set a new breakpoint (type = code), which will be hit when program
        execution reaches a memory address

        :param address:
            The address to set the breakpoint on

        :param memory_space:
            The name of the memory space that ``address`` is in.
            If None, the current memory space of the core is used
        """
        space = self._get_address_space(memory_space)

        try:
            bptId = self.client.irisCall().breakpoint_set(instId = self.instId, type = 'code',
                                                         address = address, spaceId = space.spaceId)
        except iris.error.IrisError as e:
            raise ValueError('Failed to set code breakpoint: {}'.format(e))

        bpt_info = {'bptId':bptId, 'type':'code', 'address':address, 'spaceId':space.spaceId}
        return self._insert_breakpoint(iris.toStructDict(bpt_info))

    def add_bpt_mem(self, address, memory_space = None, on_read = True, on_write = True, on_modify = None):
        """
        Set a new breakpoint (type = data), which will be hit when a memory
        location is accessed

        :param address:
            The address to set the breakpoint on

        :param memory_space:
            The name of the memory space that ``address`` is in.
            If None, the current memory space of the core is used

        :param on_read:
            If True, the breakpoint will be triggered when the memory location
            is read from.

        :param on_write:
            If True, the breakpoint will be triggered when the memory location
            is written to.

        :param on_modify: Deprecated
            If True, the breakpoint will be triggered when the memory location
            is modified.
        """
        if on_modify:
            warn("Deprecated parameter will be ignored: on_modify")

        space = self._get_address_space(memory_space)

        rwMode = ''
        if on_read:
            rwMode += 'r'
        if on_write:
            rwMode += 'w'
        if rwMode == '':
            raise ValueError("Invalid read/write mode")

        try:
            bptId = self.client.irisCall().breakpoint_set(instId = self.instId, type = 'data',
                                                         address = address, spaceId = space.spaceId,
                                                         rwMode = rwMode)
        except iris.error.IrisError as e:
            raise ValueError('Failed to set data breakpoint: {}'.format(e))

        bpt_info = {'bptId':bptId, 'type':'data', 'address':address, 'spaceId':space.spaceId,
                    'rwMode':rwMode}
        return self._insert_breakpoint(iris.toStructDict(bpt_info))

    def add_bpt_reg(self, reg_name, on_read = True, on_write = True, on_modify = None):
        """
        Set a new breakpoint, which will be hit when a register is accessed

        :param reg_name:
            The name of the register to set the breakpoint on.
            The name can be in one of the following formats:

                * ``<group>.<register>``
                * ``<group>.<register>.<field>``
                * ``<register>``
                * ``<register>.<field>``

            The last two forms can only be used if the register name is unambiguous

        :param on_read:
            If True, the breakpoint will be triggered when the register is read from.

        :param on_write:
            If True, the breakpoint will be triggered when the register is written to.

        :param on_modify: Deprecated
            If True, the breakpoint will be triggered when the register is modified.
        """
        if on_modify:
            warn("Deprecated parameter will be ignored: on_modify")

        # No ARM models support register breakpoints (as of Apr 2013), so this is untested
        reg_info = self._get_register_by_name(reg_name)

        rwMode = ''
        if on_read:
            rwMode += 'r'
        if on_write:
            rwMode += 'w'
        if rwMode == '':
            raise ValueError("Invalid read/write mode")

        try:
            bptId = self.client.irisCall().breakpoint_set(instId = self.instId, type = 'register',
                                                         rscId = reg_info.rscId, rwMode = rwMode)
        except iris.error.IrisError as e:
            raise ValueError('Failed to set register breakpoint: {}'.format(e))

        bpt_info = {'bptId':bptId, 'type':'register', 'rscId':reg_info.rscId, 'rwMode':rwMode}
        return self._insert_breakpoint(iris.toStructDict(bpt_info))

    def remove_bpt(self, bptId):
        if bptId not in self.breakpoints.keys():
            raise ValueError("The bptId {} does not exist".format(bptId))
        else:
            try:
                self.client.irisCall().breakpoint_delete(instId = self.instId, bptId = bptId)
                del self.breakpoints[bptId]
            except iris.error.IrisError as iris_e:
                raise ValueError('Failed to delete breakpoint: {}'.format(iris_e))

    def get_hit_breakpoints(self):
        """Returns the list of breakpoints that were hit last time the target
        was running"""
        for bpt in self.breakpoints.values():
            if bpt.is_hit:
                yield bpt

    def save_state(self, checkpoint_directory):
        try:
            self.client.irisCall().checkpoint_save(instId = self.instId, checkpointDir = checkpoint_directory)
        except iris.error.IrisError as iris_e:
            if iris_e.code == iris.error.E_function_not_supported_by_instance:
                return True
            if iris_e.code != iris.error.E_ok:
                return False
        return True

    def restore_state(self, checkpoint_directory):
        try:
            self.client.irisCall().checkpoint_restore(instId = self.instId, checkpointDir = checkpoint_directory)
        except iris.error.IrisError as iris_e:
            if iris_e.code == iris.error.E_function_not_supported_by_instance:
                return True
            if iris_e.code != iris.error.E_ok:
                return False
        return True

    def supports_tables(self):
        """Does the target have any tables"""
        return (self._table_info_by_name != {})

    def has_table(self, name):
        """Does the target have the named table"""
        return (name in self._table_info_by_name)

    def read_table(self, name, index = None, count = 1):
        """
        Read specified rows from a named table.

        :param name:
            The name of the Table to read from.

        :param index:
            Row where to start reading. Default is minIndex of table.

        :param count:
            Number of rows to read, starting from index. Default is 1.

        Raises ValueError if the table name does not exist, or if count is less than 1.

        Returns read rows as a dictionary {index : {<col name> : <value>, ...}, ...}
        """

        if count <= 0:
            raise ValueError("count cannot be less that 1: 'count={}'".format(count))
        if not self.has_table(name = name):
            raise ValueError("'{}' is not a valid table name for '{}'".format(name,
                                                                     self.instance_name))

        table_id = self._table_info_by_name[name].tableId

        if index is None:
            index = self._table_info_by_name[name].get('minIndex', 0)

        read_result = self.client.irisCall().table_read(count = count,
                                                        index = index,
                                                        instId = self.instId,
                                                        tableId = table_id)

        if 'error' in read_result and read_result['error'] != []:
            raise iris.error.IrisError(read_result['error'][0].errorCode,
                                       "Error when reading from table '{}'".format(name))

        return {row_index : row_data for row_index, row_data in [(r.index, r.row) for r in read_result['data']]}

    def write_table(self, name, table_records):
        """
        Write specified records in a table.

        :param name:
            The name of the Table to write to.

        :param table_records:
            Should be a dictionary { index : rowdata, ...}, where:
                * index  is the value of the row index where rowdata will be written.
                * rowdata is the cells in dict form { <col name> : <value>, ... }.
            The table record may have a subset of the cells of the row to which a write
            should take place.
            This is the same format which read_table() returns.

        Raises ValueError if the table name does not exist.
        """

        if not self.has_table(name = name):
            raise ValueError("'{}' is not a valid table name for '{}'".format(name,
                                                                     self.instance_name))

        table_id = self._table_info_by_name[name].tableId

        write_result = self.client.irisCall().table_write(instId = self.instId,
                                                          records = table_records,
                                                          tableId = table_id)

        if 'error' in write_result and write_result['error'] != []:
            raise iris.error.IrisError(write_result['error'][0].errorCode,
                                       "Error when writing to table '{}' in column '{}', index '{}'"
                                       .format(name,
                                               write_result['error'][0].name,
                                         write_result['error'][0].index))

    def get_table_info(self, name = None):
        """
        This method can be used to retrieve information about the tables
        present in this Target.

        It is used in two ways:
            * ``get_table_info(name)``
                Will return the info for the named table and its columns.
            * ``get_table_info()``
                The function acts as a generator and will yield information about
                all tables.

        :param name:
            The name of the table to provide info for.

            If None, it will yield information about all tables.
        """

        def get_info_rep(info):
            if (sys.version_info < (3, 0)):
                value_types = {'NumberU64' : long,
                               'NumberS64': int,
                               'NumberU64[]': list,
                               'Boolean' : bool,
                               'String' : str,
                              }
            else:
                value_types = {'NumberU64' : int,
                               'NumberS64': int,
                               'NumberU64[]': list,
                               'Boolean' : bool,
                               'String' : str,
                              }
            column_info = []
            for col in info.columns:
                column_info.append(ColumnInfo(col.name,
                                              col.description,
                                              col.bitWidth,
                                              value_types[col.get('type', 'NumberU64')],
                                              'r' in col.get('rwMode','rw'),
                                              'w' in col.get('rwMode','rw')))

            return TableInfo(info.name,
                             column_info,
                             info.description,
                             info.get('maxIndex', 2**64-1),
                             info.get('minIndex', 0))

        def generator(info_dict):
            for t_name in info_dict:
                yield get_info_rep(info_dict[t_name])

        if name is None:
            return generator(self._table_info_by_name)
        else:
            return get_info_rep(self._table_info_by_name[name])

    def get_event_info(self, name=None):
        """
        Retrieve information about event sources provided by this target.

        It is used in two ways:
            * ``get_event_info(name)``
                Will return the info for the named event and its fields.
            * ``get_event_info()``
                The function acts as a generator and will yield information about
                all events.

        :param name:
            The name of the event to provide info for.

            If None, it will yield information about all tables.
        """
        if name is not None:
            for event_info in self._event_info.get_info():
                if event_info.name == name:
                    return event_info
            raise ValueError('Target does not have an event named "{}"'.format(name))
        else:
            return self._event_info.get_info()


class AsyncTarget(TargetBase):
    def __init__(self, inst_info, model):
        TargetBase.__init__(self, inst_info, model)

        self._handle_semihost_io = False
        self._stdin  = None
        self._stdout = None
        self._stderr = None

    def _new_breakpoint(self, bpt_info):
        return AsyncBreakpoint(self, bpt_info)

    def _new_event_manager(self):
        return EventCallbackManager(self.client, self, self.model.verbose)

    # this method is called by the receive thread in iris library
    def ec_IRIS_SEMIHOSTING_INPUT_REQUEST(self, esId, fields, time, sInstId, syncEc = False):

        # Extract fields (with some useful defaults for missing fields)
        f_des     = fields.get('FDES', 0)
        non_block = fields.get('NONBLOCK', False)
        raw       = fields.get('RAW', False)
        size_hint = fields.get('SIZEHINT', 0)

        # Obtain the requested data
        if f_des == 0:

            # For read() and readline() below no limit is represented by -1
            if size_hint == 0:
                size_hint = -1

            if non_block:
                # Just read everything that is currently available
                value = self._stdin.read()
            elif raw:
                # Read size_hint chars
                value = self._stdin.read(size_hint)
            else:
                # Read size_hint chars or up until the terminator
                value = self._stdin.readline(size_hint)
        else:
            # Stream not supported return empty string
            value = ""

        # Send the data back to the requestor
        u64_array = iris.convertStringToU64Array(value)
        iris.try_iris(self.client.irisCall().semihosting_provideInputData, -1,
                      instId = self.instId, fDes = f_des, data = u64_array, size = len(value))

    # this method is called by the receive thread in iris library
    def ec_IRIS_SEMIHOSTING_OUTPUT(self, esId, fields, time, sInstId, syncEc = False):
        def write_to_stream(outstream, bytearr_u64, size):
            for d in bytearr_u64:
                outstream.write(struct.pack('<Q', d)[0:size])
                # each data entry holds 8 bytes, last one may hold less and it will
                # be padded with trailing 0s
                size -= 8

        if fields['FDES'] == 1:
            write_to_stream(self._stdout, fields['DATA'], fields['SIZE'])
        elif fields['FDES'] == 2:
            write_to_stream(self._stderr, fields['DATA'], fields['SIZE'])

    @property
    def stdin(self):
        """Target's semihosting stdin"""
        if not self._handle_semihost_io:
            raise ValueError("Not handling semihosting for target {}, try calling handle_semi_hosted_io()"
                  .format(self.instance_name))
        elif self._stdin is None:
            raise ValueError("Target {} does not support semihosting input"
                  .format(self.instance_name))
        else:
            return self._stdin

    @stdin.setter
    def stdin(self, value):
        if not self._handle_semihost_io:
            raise ValueError("Not handling semihosting for target {}, try calling handle_semi_hosted_io()"
                  .format(self.instance_name))
        elif self._stdin is None:
            raise ValueError("Target {} does not support semihosting input"
                  .format(self.instance_name))
        else:
            self._stdin = value

    @property
    def stdout(self):
        """Target's semihosting stdout"""
        if not self._handle_semihost_io:
            raise ValueError("Not handling semihosting for target {}, try calling handle_semi_hosted_io()"
                  .format(self.instance_name))
        elif self._stdout is None:
            raise ValueError("Target {} does not support semihosting output"
                  .format(self.instance_name))
        else:
            return self._stdout

    @stdout.setter
    def stdout(self, value):
        if not self._handle_semihost_io:
            raise ValueError("Not handling semihosting for target {}, try calling handle_semi_hosted_io()"
                  .format(self.instance_name))
        elif self._stdout is None:
            raise ValueError("Target {} does not support semihosting output"
                  .format(self.instance_name))
        else:
            self._stdout = value

    @property
    def stderr(self):
        """Target's semihosting stderr"""
        if not self._handle_semihost_io:
            raise ValueError("Not handling semihosting for target {}, try calling handle_semi_hosted_io()"
                  .format(self.instance_name))
        elif self._stderr is None:
            raise ValueError("Target {} does not support semihosting output"
                  .format(self.instance_name))
        else:
            return self._stderr

    @stderr.setter
    def stderr(self, value):
        if not self._handle_semihost_io:
            raise ValueError("Not handling semihosting for target {}, try calling handle_semi_hosted_io()"
                  .format(self.instance_name))
        elif self._stderr is None:
            raise ValueError("Target {} does not support semihosting output"
                  .format(self.instance_name))
        else:
            self._stderr = value

    # NOTE: in theory, this could be supported by the SyncTarget as well but
    # this can easily lead to deadlocks if mishandled.
    def handle_semihost_io(self):
        """
        Request that semihosted input and output are handled for this target
        using this Iris client.
        """

        try:
            # Create a stream for semihosted input
            self._stdin  = Stream()

            # Register callbacks for IRIS_SEMIHOSTING_INPUT_REQUEST
            ecFunc = "ec_IRIS_SEMIHOSTING_INPUT_{}".format(self.instId)
            description = "IRIS_SEMIHOSTING_INPUT callback for target instId = {}".format(self.instId)
            self.client.instance.registerEventCallback(ecFunc,
                                                       self.ec_IRIS_SEMIHOSTING_INPUT_REQUEST,
                                                       description)

            event_src = self.client.irisCall().event_getEventSource(instId = self.instId,
                                                                    name = "IRIS_SEMIHOSTING_INPUT_REQUEST")

            self.client.irisCall().eventStream_create(instId = self.instId,
                                                      evSrcId = event_src.evSrcId,
                                                      ecInstId = self.client.instId,
                                                      ecFunc = ecFunc)

        except iris.error.IrisError as iris_e:
            if iris_e.code in [iris.error.E_unknown_event_source,
                               iris.error.E_function_not_supported_by_instance]:
                self._stdin = None
            else:
                raise

        try:
            # Create  streams for semihosted output
            self._stdout = Stream()
            self._stderr = Stream()

            # Register callbacks for IRIS_SEMIHOSTING_OUTPUT
            ecFunc = "ec_IRIS_SEMIHOSTING_OUTPUT_{}".format(self.instId)
            description = "IRIS_SEMIHOSTING_OUTPUT callback for target instId = {}".format(self.instId)
            self.client.instance.registerEventCallback(ecFunc,
                                                       self.ec_IRIS_SEMIHOSTING_OUTPUT,
                                                       description)

            event_src = self.client.irisCall().event_getEventSource(instId = self.instId,
                                                                    name = "IRIS_SEMIHOSTING_OUTPUT")

            self.client.irisCall().eventStream_create(instId = self.instId,
                                                      evSrcId = event_src.evSrcId,
                                                      ecInstId = self.client.instId,
                                                      ecFunc = ecFunc)

        except iris.error.IrisError as iris_e:
            if iris_e.code in [iris.error.E_unknown_event_source,
                               iris.error.E_function_not_supported_by_instance]:
                self._stdout = None
                self._stderr = None
            else:
                raise iris_e

        self._handle_semihost_io = True

    def add_event_callback(self, event_name, func, fields=None):
        """
        Add a callback function for the named event. This function will be called when the
        event fires.

        :param event_name:
            The name of the event.

        :param func:
            A callback to be called when the event fires taking two arguments:
                The first argument is the cycle counter, the second argument is
                a dictionary containing at least all the requested fields.

        :param fields:
            A list of event fields that the callback should provide.
        """
        evSrcId = self._event_info.get_evSrcId(event_name)

        self._event_manager.add_callback(evSrcId, func, fields)

    def remove_event_callback(self, event_name_or_func):
        """
        Remove an event callback function previously added to this target.

        :param event_name_or_func:
            This can either be the name of an event, or a callable object that we previously
            added to this target as an event callback.
        """
        if callable(event_name_or_func):
            # Assume it is a function
            self._event_manager.remove_callback_func(event_name_or_func)
        else:
            # Assume it is a name
            evSrcId = self._event_info.get_evSrcId(event_name_or_func)
            self._event_manager.remove_callback_evSrcId(evSrcId)


class SyncTarget(TargetBase):
    def __init__(self, inst_info, model):
        TargetBase.__init__(self, inst_info, model)

    def __get_event_info(self, instId, event_name):
        # small optimization: if registering to an event of this instance, use
        # the cached info
        if instId == self.instId:
            return self._event_info.get_info_by_name(event_name)

        info = iris.try_iris(self.client.irisCall().event_getEventSource,
                             {}, name=event_name, instId=instId)
        return TargetEventSourceInfo.to_event_source_info(info)

    def _new_breakpoint(self, bpt_info):
        return SyncBreakpoint(self, bpt_info)

    def _new_event_manager(self):
        return EventBufferManager(self.client, self, self.model.verbose)

    def step(self, count=1, unit='instruction'):
        """Step this core the specified amount of time.

        :param count Amount to step

        :param unit Either 'instruction' or 'cycle'

        All event callbacks are triggered after the step is done and before
        this function returns.
        """
        self.clear_bpts()
        self._event_manager.rebuilds()
        data = self.client.irisCall().step_syncStep(instId=self.instId, steps=count, unit=unit)
        self._event_manager.trigger_callbacks(data["events"])

    def flush_events(self):
        """Trigger the callbacks of all pending events.

        This is normally not needed if the only function called is step, but
        it might be necessary when setting registers that have side effects
        that can trigger events. If such events need to be observed before
        the next step, then one would need to call this function first.
        """
        events = self._event_manager.flush_event_buffer()
        self._event_manager.trigger_callbacks(events)

    def add_event_callback(self, event_name, func, fields=None, instId=None):
        """
        Add a callback function for the named event. This function will be
        called when the event fires.

        :param event_name:
            The name of the event.

        :param func:
            A callback to be called when the event fires taking two arguments:
                The first argument is the cycle counter, the second argument is
                a dictionary containing at least all the requested fields.

        :param fields:
            A list of event fields that the callback should provide.

        :param instId:
            Optionally, the instance id of the component publishing the event.
            Mostly meant to register to global events that might happen when
            stepping this core.
        """
        if instId is None:
            instId = self.instId

        info = self.__get_event_info(instId, event_name)
        self._event_manager.add_callback(instId, info, func, fields)

    def remove_event_callback(self, event_name_or_func, instId=None):
        """
        Remove an event callback function previously added to this target.

        :param event_name_or_func:
            This can either be the name of an event, or a callable object that
            we previously added to this target as an event callback.
        """
        if callable(event_name_or_func):
            # Assume it is a function
            self._event_manager.remove_callback_func(event_name_or_func)
        else:
            # Assume it is a name
            if instId is None:
                instId = self.instId

            info = self.__get_event_info(instId, event_name_or_func)
            self._event_manager.remove_callback_evSrcId(instId, info.evSrcId)
