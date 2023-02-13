#!/usr/bin/env python
# \file   NetworkModelInitializer.py
# \brief  Prepares and establishes a connection between the Iris debugger and an Iris server.
#
# \date   Copyright ARM Limited 2016 All Rights Reserved.

import subprocess
import re
import os
import shlex
import socket
from time import sleep

from . import debug
NetworkModel = debug.Model.NetworkModel

SERVER_WAIT_TIME_IN_SECONDS = 4


class NetworkModelInitializer(object):
    """
    The NetworkModelInitializer class represents an established or pending connection
    between an Iris Model Debugger, accessible via the class NetworkModel, and an Iris
    server which is embedded either within an ISIM or another simulation using an ISIM as
    a library.
    You should use the NetworkModelFactory class below to create an instance of this class.

    Once the class is created you can use it in two ways:

    1: network_model below is an instance of NetworkModel, all resources are automatically
       deallocated at the end of the with statement context.

       with NetworkModelFactory.CreateNetworkToHost(host, port) as network_model:
           network_model.get_targets()

    2: network_model below is an instance of NetworkModel, all resource are NOT automatically
       deallocatted so you  need to handle exception and force deallocation manually.

       network_model_initializer = NetworkModelFactory.CreateNetworkToHost(host, port)
       network_model = network_model_initializer.start()
       try:
           network_model.get_targets()
       finally:
           network_model_initializer.close()

    A full working example is in the Python/Example folder
    """

    def __init__(self, server_startup_command = None, host = 'localhost', port = None, timeout_in_ms = 1000, synchronous = False, verbose = False):
        self.server_startup_command = server_startup_command
        self.process = None
        self.host = host
        self.port = port
        self.timeout_in_ms = timeout_in_ms
        self.synchronous = synchronous
        self.verbose = verbose

    def __get_port(self):
        while self.process.returncode is None:
            match = re.match('Iris server started listening to port ([0-9]+)',
                             self.process.stdout.readline().decode())
            if match is not None:
                return int(match.group(1))
            self.process.poll()

        raise RuntimeError('isim exited without printing a port number\n returncode: {}'.format(self.process.returncode))

    def __start_server(self):
        if self.server_startup_command is not None:
            self.process = subprocess.Popen(self.server_startup_command,
                                        stdin = subprocess.PIPE,
                                        stderr = subprocess.PIPE,
                                        stdout = subprocess.PIPE)

            #Give some time to the server to print the port
            sleep(SERVER_WAIT_TIME_IN_SECONDS)
            self.port = self.__get_port()


    def __start_client(self):
       self.network_model = debug.Model.NewNetworkModel(
               self.host, self.port, self.timeout_in_ms, synchronous=self.synchronous, verbose=self.verbose
               );

    def close(self):
        """
        Deallocate the Iris server process if one was created previously
        """
        if self.process is not None:
            self.process.kill()

    def start(self):
        """
        Start the Iris server (if necessary) and connects the Iris Debugger client to the server
        """
        try:
            self.__start_server()
            self.__start_client()
        except:
            self.close()
            raise
        return self.network_model

    def __enter__(self):
        return self.start()

    def __exit__(self, exception_type, exception_value, traceback):
        self.close()


class LocalNetworkModelInitializer(object):
    def __init__(self, server_startup_command, timeout_in_ms, verbose=False, synchronous=False):
        self.__command = server_startup_command
        self.__timeout = timeout_in_ms
        self.__verbose = verbose
        self.__process = None
        self.__synchronous = synchronous

    def start(self):
        if os.name == 'nt':
            raise NotImplementedError()
        else:
            lhs, rhs = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM, 0)
            command = self.__command[:]
            command.append("--iris-connect")
            command.append("socketfd={}".format(rhs.fileno()))
            try:
                self.__process = subprocess.Popen(command, pass_fds=(rhs.fileno(),))
            except Exception:
                lhs.close()
                raise
            finally:
                rhs.close()
            try:
                return debug.Model.NewUnixDomainSocketModel(
                        lhs, timeoutInMs=self.__timeout,
                        verbose=self.__verbose, synchronous=self.__synchronous)
            except Exception:
                lhs.close()
                self.close()
                raise

    def close(self):
        self.__process.kill()
        self.__process.wait()

    def __enter__(self):
        return self.start()

    def __exit__(self, exception_type, exception_value, traceback):
        self.close()


class NetworkModelFactory:
    """
    The NetworkModelFactory class allows the creation of NetworkModelInitializers. It contains only class methods.
    """

    @classmethod
    def CreateNetworkFromIsim(cls, isim_filename, parameters = None, timeout_in_ms = 1000):
        """
        Create a network initializer to an isim yet to be started
        """

        parameters = parameters or {}
        isim_startup_command = [isim_filename, '-I', '-p']  # Start Iris server and print the port

        for param, value in parameters.items():
            isim_startup_command += ['-C', '{}={}'.format(param, value)]

        return NetworkModelInitializer(server_startup_command = isim_startup_command, timeout_in_ms = timeout_in_ms)

    @classmethod
    def CreateLocalFromIsim(cls, isim_filename, parameters = dict(), timeout_in_ms=5000, verbose=False, xargs=[], synchronous=False):
        """
        Create a network initializer to an isim yet to be started using 1:1
        network communication, no TCP server are started and the isim will
        automatically shut down would this process terminate unexpectedly.

        :param synchronous
            Whether to instantiate a SyncModel or an AsyncModel. Threads are
            used either way for the communication.
        """
        isim_startup_command = [isim_filename] + xargs

        for k, v in parameters.items():
            isim_startup_command.append("-C")
            isim_startup_command.append("{}={}".format(k,v))

        return LocalNetworkModelInitializer(server_startup_command=isim_startup_command, timeout_in_ms=timeout_in_ms, verbose=verbose, synchronous=synchronous)

    @classmethod
    def CreateNetworkFromLibrary(cls, simulation_command, library_filename, parameters = None, timeout_in_ms = 1000):
        """
        Create a network initializer to a simulation application that uses an isim as a library and is not yet started
        """

        parameters = parameters or {}
        simulation_startup_command = [simulation_command, library_filename, '-I', '-p']  # Start Iris server and print the port

        for param, value in parameters.items():
            simulation_startup_command += ['-C', '{}={}'.format(param, value)]

        return NetworkModelInitializer(server_startup_command = simulation_startup_command, timeout_in_ms = timeout_in_ms)


    @classmethod
    def CreateNetworkFromCommand(cls, command_line, timeout_in_ms = 1000):
        """
        Create a network initializer to an Iris server to be started by the input command line
        """

        return NetworkModelInitializer(server_startup_command = shlex.split(command_line), timeout_in_ms = timeout_in_ms)


    @classmethod
    def CreateNetworkToHost(cls, hostname, port, timeout_in_ms = 1000, synchronous = False, verbose = False):
        """
        Create a network initializer to an iris server which was already started and is accessible at the given hostname and port
        """

        return NetworkModelInitializer(host = hostname, port = port, timeout_in_ms = timeout_in_ms, synchronous = synchronous, verbose = verbose)
