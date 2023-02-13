# \file   error.py
# \brief  Iris error code handling.
# \date   Copyright Arm Limited 2018. All Rights Reserved.

import json
import numbers

# Import all error codes in to this module. This will allow users to refer to
# error codes as iris.error.E_my_error.
from ._error_code_list import *

# Compile a mapping of error code to symbol.
_code_to_symbol = {v:E for E, v in globals().items() if E.startswith('E_')}

class IrisErrorCode(object):
    """An Iris error code and symbol."""

    def __init__(self, number):
        self.number = number

    def __str__(self):
        if self.number in _code_to_symbol:
            return _code_to_symbol[self.number]
        else:
            return "E_unknown_code_0x{:04X}".format(self.number)

    def __repr__(self):
        if self.number in _code_to_symbol:
            # This is a recognised error code so we can use the symbol representation.
            return 'iris.error.{}'.format(_code_to_symbol[self.number])
        else:
            # Not a recognised error code so we need to generate the fallback repr
            return 'iris.error.IrisErrorCode({})'.format(self.number)

    def __eq__(self, other):
        if isinstance(other, IrisErrorCode):
            return self.number == other.number
        elif isinstance(other, numbers.Integral):
            return self.number == other
        elif isinstance(other, str):
            if self.number in _code_to_symbol:
                return _code_to_symbol[self.number] == other
        return False

    def __ne__(self, other):
        return not self.__eq__(other)


class IrisError(Exception):
    """
    Represents an error from an Iris call.
    Specific details are given by the code, message and data.
    """

    def __init__(self, code, message, data=None):
        if isinstance(code, numbers.Integral):
            code = IrisErrorCode(code)

        self.code = code
        self.message = message
        self.data = data

    def __str__(self):
        r = '{} {}'.format(self.message, self.code)
        if self.data:
            r += json.dumps(self.data, separators=(",", ":"))
        return r

