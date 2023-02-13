#!/usr/bin/env python
# \file   Parameters.py
# \brief  Interface to the Stream in an iris target
#
# \date   Copyright ARM Limited 2018 All Rights Reserved.

import os

from io import BytesIO
from threading import Lock

class Stream(object):
    """Threadsafe string stream"""

    def __init__(self):
        self.stream = BytesIO()
        self.lock = Lock()
        self.position = 0

    def read(self, n=-1):
        """Read to the front of the stream"""
        with self.lock:
            result = None
            prev_position = self.stream.tell()
            self.stream.seek(self.position)
            result = self.stream.read(n)
            self.position = self.stream.tell()
            self.stream.seek(prev_position)
        return result

    def write(self, s):
        """Write to the back of the stream"""
        with self.lock:
            prev_position = self.stream.tell()
            self.stream.seek(0, os.SEEK_END)
            self.stream.write(s)
            self.stream.seek(prev_position)

    def __getattr__(self, attr):
        return getattr(self.stream, attr)
