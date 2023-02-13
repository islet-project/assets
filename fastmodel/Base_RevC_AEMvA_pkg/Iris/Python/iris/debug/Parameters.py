#!/usr/bin/env python
# \file   Parameters.py
# \brief  Interface to the parameters in an Iris target
#
# \date   Copyright ARM Limited 2016 All Rights Reserved.

import iris.iris as iris
from numbers import Integral
import sys

class ModelParameter(object):
    def __init__(self, info):
        object.__init__(self)
        self.info = info

    @property
    def description(self):
        return self.info.get('description')

    @property
    def isRunTime(self):
        return not self.info.parameterInfo.get('initOnly', False)

    @property
    def name(self):
        return self.info.name

# Alias 'long' to 'int' for Python3.x.
if sys.version_info >= (3, 0):
    long = int

class ModelIntParameter(long, ModelParameter):
    def __init__(self, info, value = None):
        if not (isinstance(value, Integral) or value is None):
            raise TypeError("Parameter must be an integer, not '%s'" % type(value))

        ModelParameter.__init__(self, info)
        long.__init__(self)
        self.info = info

    def __new__(cls, info, value = None):
        if value is None:
            # No value is provided so we use the default for this parameter.
            value = ModelIntParameter._decode_metadata_bit_pattern_attr(info, 'defaultData')

        return long.__new__(cls, value)

    @staticmethod
    def _decode_metadata_bit_pattern_attr(info, attr):
        """Decode the value of a ParameterInfo attribute bit pattern."""
        if  attr in info.parameterInfo:
            return iris.from_bitpattern(info.parameterInfo[attr],
                                        info.get('type', 'numeric'),
                                        info.bitWidth)
        else:
            return None

    @property
    def defaultValue(self):
        return ModelIntParameter._decode_metadata_bit_pattern_attr(self.info, 'defaultData')

    @property
    def maxValue(self):
        return ModelIntParameter._decode_metadata_bit_pattern_attr(self.info, 'max')

    @property
    def minValue(self):
        return ModelIntParameter._decode_metadata_bit_pattern_attr(self.info, 'min')


class ModelBoolParameter(ModelParameter):
    def __init__(self, info, value = None):
        if not (isinstance(value, bool) or isinstance(value, ModelBoolParameter) or value is None):
            raise TypeError("Parameter must be a bool, not '%s'" % type(value))

        ModelParameter.__init__(self, info)
        self.value = value
        self.info = info

    def __getattr__(self, name):
        return self.value.__getattribute__(name)

    def __repr__(self):
        return self.value.__repr__()

    def __nonzero__(self):
        return self.value.__nonzero__()

    def __bool__(self):
        return self.value.__bool__()

    def __cmp__(self, other):
        return self.value.__cmp__(other)

    def __and__(self, other):
        if isinstance(other, bool) or isinstance(other, Integral):
            return self.value & other
        elif isinstance(other, ModelBoolParameter):
            return self.value & other.value

    @property
    def defaultValue(self):
        defaultData = self.info.parameterInfo.get('defaultData')
        if defaultData is None:
            return None
        return defaultData[0] != 0


class ModelStringParameter(str, ModelParameter):
    def __init__(self, info, value = None):
        if not (isinstance(value, str) or value is None):
            raise TypeError("Parameter must be a string, not '%s'" % type(value))

        ModelParameter.__init__(self, info)
        self.info = info

    def __new__(cls, info, value = None):
        if value is not None:
            return str.__new__(cls, value)
        else:
            return str.__new__(cls, self.info.parameterInfo.get('defaultString'))

    @property
    def defaultValue(self):
        return self.info.parameterInfo.get('defaultString')


class ParameterDict(dict):
    """
    Derives from dict and keeps runtime and init-time parameters

    Allows runtime Parameters to be set by name using the index operator.
    e.g:
        parameters["semihosting-enable"] = False
    The value will be immediately set using iris resource_write().

    Attempts to set the value of init-time parameters will result in ValueError.

    Values are read once on demand and then cached.

    param_info_list: List of ResourceInfos of all parameters. This must contain only parameters.
    """
    def __init__(self, target, param_info_list):
        self.target = target
        self.infos = {info.name: info for info in param_info_list}

        for info in param_info_list:
            super(ParameterDict, self).__setitem__(info.name, None)

    def __getitem__(self, key):
        if key not in self.infos:
            raise KeyError(key)
        info = self.infos[key]
        value = super(ParameterDict, self).__getitem__(key)
        if value is None:
            # Value not yet read. Read it now.
            value = iris.try_iris(self.target.client.irisCall().resource_read, None,
                                  instId = self.target.instId, rscIds = [info.rscId])
            if value != None:
                if 'error' in value:
                    value = None
                else:
                    value = value.strings[0] if info.get('type') == 'string' else value.data[0]

            value = self._convert_to_parameter_object(info, value)
            super(ParameterDict, self).__setitem__(info.name, value)
        return super(ParameterDict, self).__getitem__(key)

    def __setitem__(self, key, value):
        if key not in self.infos:
            raise KeyError(key)
        # TODO: Support init-time parameter set in simulation instantiation
        if not self[key].isRunTime:
            raise ValueError("Cannot set init-time parameters")

        if not isinstance(value, ModelParameter):
            value = self._convert_to_parameter_object(self.infos[key], value)

        try:
            if isinstance(value, ModelStringParameter):
                result = self.target.client.irisCall().resource_write(instId = self.target.instId,
                                                                     rscIds = [self.infos[key].rscId],
                                                                     data = [], strings = [value])
            else:
                result = self.target.client.irisCall().resource_write(instId = self.target.instId,
                                                                     rscIds = [self.infos[key].rscId],
                                                                     data = [value & 0xFFFFFFFFFFFFFFFF])
            if 'error' in result and len(result['error']) > 0:
                error_code = error.IrisErrorCode(result['error'][0])
                raise ValueError('resource_write() returned the error {}'.format(error_code))
        except Exception as e:
            raise ValueError("Cannot write the value to the parameter: {}".format(e))

        super(ParameterDict, self).__setitem__(key, value)

    def _convert_to_parameter_object(self, info, value):
        """Construct the appropriate Model*Parameter object for a parameter"""

        type = info.get('type', 'numeric')
        if type == 'numeric' or type == 'numericSigned':
            if info.bitWidth == 1:
                return ModelBoolParameter(info, value != 0)
            elif info.bitWidth <= 64:
                return ModelIntParameter(info, value)
            else:
                raise ValueError('Invalid bit width for parameters')
        elif type == 'string':
            return ModelStringParameter(info, str(value))
        else:
            raise TypeError("The parameter has invalid type %i" % info.type)

