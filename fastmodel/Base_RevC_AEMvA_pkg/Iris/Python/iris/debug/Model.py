#!/usr/bin/env python
# \file   Model.py
# \brief  High-level Python interface to Iris models.
#
# \date   Copyright ARM Limited 2020-2022 All Rights Reserved.

import iris.iris as iris
from .Target import AsyncTarget, SyncTarget
from .Exceptions import TargetBusyError, TimeoutError

from collections import namedtuple
from warnings import warn
import threading
import time

TargetInfo = namedtuple("TargetInfo", ["instance_name", "target_name", "version",
                                       "description", "component_type",
                                       "executes_software", "is_hardware_model"])

class ModelBase(object):
    """
    This class wraps an Iris model.
    """

    def __init__(self, client, verbose):
        self.client = client
        self.verbose = verbose

        # The instance infos for all targets
        all_instance_infos = iris.try_iris(self.client.irisCall().instanceRegistry_getList, [], prefix = 'component')
        self.instance_infos = {inst_info.instName: inst_info for inst_info in all_instance_infos}

        # Targets indexed by instId. Each target will be created at most once.
        self.targets = {}

        # List of all CPU targets.
        self.cpus = []

    def __repr__(self):
        return "<%s(%s) object at %s>" % (
                self.__class__.__name__, str(self.client), hex(id(self)))

    def get_target(self, instance_name):
        """
        Obtain an interface to a target

        :param instance_name: The instance name corresponding to the desired target
        """
        if instance_name not in self.instance_infos:
            raise ValueError("Model has no target named \"%s\"" % instance_name)

        instId = self.instance_infos[instance_name].instId
        if instId not in self.targets:
            self.targets[instId] = self._new_target(self.instance_infos[instance_name])
        return self.targets[instId]

    def get_cpus(self):
        """
        Returns all targets which have executesSoftware set or have
        componentType = 'Core'
        """
        if not self.cpus:
            for info in self.instance_infos.values():
                properties = iris.try_iris(self.client.irisCall().instance_getProperties,
                                           {}, instId = info.instId)
                if (properties.get('componentType') == 'Core') or \
                            (properties.get('executesSoftware', 0) != 0):
                    self.cpus.append(self.get_target(info.instName))
            # Sort by instance id, which is compatible with fm.debug
            self.cpus.sort(key = lambda x: x.instId)

        return self.cpus

    def get_target_info(self):
        """Returns an iterator over namedtuples containing information about
           all of the target instances contained in the model"""
        for info in self.instance_infos.values():
            properties = iris.try_iris(self.client.irisCall().instance_getProperties,
                                       {}, instId = info.instId)
            yield TargetInfo(info.instName,
                             properties.get('componentName'),  # target_name
                             properties.get('version'),
                             properties.get('description'),
                             properties.get('componentType'),
                             properties.get('executesSoftware', 0) != 0,
                             properties.get('isHardwareModel', 0) != 0)

    def get_targets(self):
        """Generator function to iterate all targets in the simulation"""
        for info in self.instance_infos.items():
            yield self.get_target(info[1].instName)

    @property
    def is_running(self):
        try:
            return self.client.irisCall().simulationTime_get(instId=1).running
        except iris.IrisError:
            raise ValueError("Failed to get similation time information")

    def save_state(self, stream_directory, save_all=True):
        """
        Save Simulation-wide state.

        :param stream_directory:
            String treated as a directory name
            TODO: support checkpoint delegate object

        :param save_all:
            If True, save the state of the simulation and all targets inside the
            simulation that support checkpointing.
            If False, only save the simulation state.
            This parameter defaults to True.

        If save_all is False and the simulation engine does not support checkpointing,
        this method will raise NotImplementedError

        Returns True if all components saved successfully
        """

        success = True

        try:
            self.client.irisCall().checkpoint_save(instId=1, checkpointDir=stream_directory)
        except iris.error.IrisError as iris_e:
            if iris_e.code == iris.error.E_function_not_supported_by_instance:
                if not save_all:
                    raise NotImplementedError('Simulation does not support checkpointing')
            elif iris_e.code != iris.error.E_ok:
                success = False

        if save_all:
            success = all(t.save_state(stream_directory) for t in self.get_targets()) and success

        return success

    def restore_state(self, stream_directory, restore_all=True):
        """
        Restore Simulation-wide state.

        :param stream_directory:
            String treated as a directory name
            TODO: support checkpoint delegate object

        :param restore_all:
            If True, restore the state of the simulation and all targets inside the
            simulation that support checkpointing.
            If False, only restore the simulation state.
            This parameter defaults to True.

        If restore_all is False and the simulation engine does not support checkpointing,
        this method will raise NotImplementedError

        Returns True if all components restored successfully
        """

        success = True

        try:
            self.client.irisCall().checkpoint_restore(instId=1, checkpointDir=stream_directory)
        except iris.error.IrisError as iris_e:
            if iris_e.code == iris.error.E_function_not_supported_by_instance:
                if not restore_all:
                    raise NotImplementedError('Simulation does not support checkpointing')
            elif iris_e.code != iris.error.E_ok:
                success = False

        if restore_all:
            success = all(t.restore_state(stream_directory) for t in self.get_targets()) and success

        return success

    def _shutdown_model(self):
        """Request shutdown without disconnecting the tcp"""
        self.client.irisCall().simulation_requestShutdown(instId=1)

    def reset(self, allow_partial_reset=False):
        """
        Reset the simulation to exactly the same state it had after instantiation.
        Reason why we might need the argument allowPartialReset:
        We do not necessarily control all components in a SystemC platform and we are only able to
        reset Fast Models components. We can only fulfil the request if:
        1) This is an ISIM model. ISIM's are built purely from Fast Models components so we can reset
            the whole platform.
        2) allowPartialReset=true was set. This option indicates that the user knows that no all
           components will be reset and accepts the consequences.
        """

        self.client.irisCall().simulation_reset(instId=1, allowPartialReset=allow_partial_reset)

    def release(self, shutdown=False):
        """
        End the simulation and release the model.

        :param shutdown:
            If True, the simulation will be shutdown and any other scripts or debuggers
            will have to disconnect.

            If False, a simulation might be kept alive after disconnection.
        """
        if shutdown:
            self._shutdown_model()
        self.client.disconnect()


class AsyncModel(ModelBase):
    """An Iris model optimized for asynchronous processing.

    The latency is reduced when running the model many instruction at a time,
    processing most events as the model is running.
    """
    def __init__(self, client, verbose):
        ModelBase.__init__(self, client, verbose)

        # Register callbacks for IRIS_SIMULATON_TIME_EVENT
        ecFunc = "ec_IRIS_SIMULATION_TIME_EVENT"
        description = "IRIS_SIMULATION_TIME_EVENT callback for client targets"
        self.client.instance.registerEventCallback(ecFunc,
                                                   self.ec_IRIS_SIMULATION_TIME_EVENT,
                                                   description)

        event_src = self.client.irisCall().event_getEventSource(instId = 1,
                                                                name = "IRIS_SIMULATION_TIME_EVENT")
        self.client.irisCall().eventStream_create(instId = 1,
                                                  evSrcId = event_src.evSrcId,
                                                  ecInstId = self.client.instId,
                                                  ecFunc = ecFunc)
        self._stop_event = threading.Event() # Triggered when simulation time stops.
        self._run_event = threading.Event() # Triggered when simulation time starts.

    def _new_target(self, target_info):
        return AsyncTarget(target_info, self)

    # this method is called by the receive thread in iris library
    def ec_IRIS_SIMULATION_TIME_EVENT(self, esId, fields, time, sInstId, syncEc = False):
        if fields['RUNNING']:
            # Simulation time has started
            # Reset the state of all cpu breakpoints.
            for c in self.get_cpus():
                c.clear_bpts()

            # Notify anyone waiting on this event that the simulation has started running.
            self._run_event.set()
        else:
            # Simulation time has stopped
            self._stop_event.set()

    # Checks if the model is already running and clears _stop_event
    def _request_run(self):
        if self.is_running:
            raise TargetBusyError("The model is already running")
        self._stop_event.clear()
        self.client.irisCall().simulationTime_run(instId=1)

    # Checks if the model is already stopped and clears _stop_event
    def _request_stop(self):
        if not self.is_running:
            raise TargetBusyError("The model is already stopped")
        self._stop_event.clear()
        self.client.irisCall().simulationTime_stop(instId=1)

    def run(self, blocking = True, timeout = None):
        """
        Starts the model executing

        :param blocking:
            If True, this call will block until the model stops executing
            (typically due to a breakpoint).
            If False, this call will return once that target has started executing.

        :param timeout:
            If None, this call will wait indefinitely for the target to enter
            the correct state.
            If set to a float or int, this parameter gives the maximum number of
            seconds to wait.

        :raises TimeoutError: If the timeout expires.
                TargetBusyError: If the model is running.
        """
        if not blocking:
            self._run_event.clear()

        self._request_run()

        if blocking:
            if not self._stop_event.wait(timeout):
                raise TimeoutError()
            self._stop_event.clear()
            return [bpt for target in self.targets.values() for bpt in target.get_hit_breakpoints()]
        else:
            if not self._run_event.wait(timeout):
                raise TimeoutError()
            self._run_event.clear()
            return None

    def stop(self, timeout = None):
        """
        Stops the model executing.

        Silently return when the model is already stopped.

        :param timeout:
            If None, this call will wait indefinitely for the target to enter
            the correct state.
            If set to a float or int, this parameter gives the maximum number of
            seconds to wait.

        :raises TimeoutError: If the timeout expires.
        """

        if not self.is_running:
            return

        self._request_stop()

        # Wait for simulation time stop event after calling simulationTime_stop()
        if not self._stop_event.wait(timeout):
            raise TimeoutError()
        self._stop_event.clear()

    def step(self, count=1, timeout=None):
        """
        Step all cores in the system for `count` instructions each (blocking).

        Cores are stepped individually and sequentially: The first core is
        stepped for "count" instructions. When that completed,
        the second core is stepped for "count" instructions and so on.
        This is intrusive debugging as it permutes the scheduling order of the
        cores, and it will generally let more simulation time pass than
        indicated by "count". Also the number of steps executed is independent
        of the relative clock speeds of the CPUs.

        Only cores which have get_execution_state()==True are processed by this
        function, thus it is possible to select which cores should be stepped
        by calling set_execution_state() before calling this function.

        Note: This is an exotic stepping function. Use core.set_steps()/model.run()
        functions for normal non-intrusive stepping.

        :param count:
            The number of processor instructions to execute.

        :param timeout:
            If None, this call will wait indefinitely for the target to enter
            the correct state.
            If set to a float or int, this parameter gives the maximum number of
            seconds to wait.

        :raises TimeoutError: If the timeout expires.
                TargetBusyError: If the model is running.
                ValueError: If no cpus present or if not all cpus support per-instance execution control.
        """
        if self.is_running:
            raise TargetBusyError("Cannot step a running model.")

        cores = self.get_cpus()
        if len(cores) == 0:
            raise ValueError("Cannot find any cpus in the model to step.")

        if not all(core.supports_per_inst_exec_control for core in cores):
            raise ValueError('Cannot step MP systems unless all cpus support per-instance execution control.')

        target_cores = [c for c in cores if c.get_execution_state()]

        if len(target_cores) == 0:
            return

        if timeout is not None:
            start_time = time.time()

        for target_to_step in target_cores:
            # Set up the core we are stepping
            try:
                target_to_step.set_steps(count)
            except ValueError:
                # The core we want to step refuses to be stepped because it is held in reset.
                # Ignore this core and step the other ones.
                continue
            target_to_step.set_execution_state(True)
            for target in target_cores:
                if target != target_to_step:
                    # Halt all the cores we are not stepping this round.
                    target.set_execution_state(False)

            # Run the simulation to execute the steps.
            self._stop_event.clear()
            self.client.irisCall().simulationTime_run()

            if not self._stop_event.wait(timeout):
                raise TimeoutError()
            self._stop_event.clear()

            if timeout is not None:
                # Adjust the timeout for the next time round
                delta = time.time() - start_time
                timeout -= delta

        # Reset the execution state for all the cores we stepped
        for target in target_cores:
            target.set_execution_state(True)


class SyncModel(ModelBase):
    """An Iris model optimized for synchronous processing.

    The latency is reduced when stepping the model one or few instructions at a
    time.
    """
    def __init__(self, client, verbose):
        ModelBase.__init__(self, client, verbose)

    def _new_target(self, target_info):
        return SyncTarget(target_info, self)


class LibraryModel(AsyncModel):
    """A model that is not supported in Iris"""

    def __init__(self, filename, parameters=None, verbose=False):
        """Raises NotImplementedError in Iris"""
        raise NotImplementedError("LibraryModel is not supported by Iris")


def _NewModel(synchronous, client, verbose):
    if synchronous:
        return SyncModel(client, verbose)
    else:
        return AsyncModel(client, verbose)


def _NewTCPClient(host, port, client_name, timeoutInMs, verbose):
    client = iris.IrisTcpClient(client_name, iris.IrisInstance(), verbose, None, timeoutInMs)
    client.connect(host, port, timeoutInMs = timeoutInMs)
    # Ensure that the simulation has been instantiated and target components are available.
    try:
        client.irisCall().simulation_waitForInstantiation(instId=1)
    except iris.error.IrisError as e:
        if e.code == iris.error.E_function_not_supported_by_instance:
            # The simulation does not support simulation_waitForInstantiation(), carry on.
            warn("Simulation does not support simulation_waitForInstantiation: unable to guarantee instantiation")
        else:
            # A different error occurred. Propagate it so it doesn't get lost.
            raise e
    return client


def NewNetworkModel(host = "localhost", port = 0, timeoutInMs = 5000, client_name = "client.iris_debug", synchronous = False, verbose = False):
    """Creates an Iris model connected to an Iris server

    When port is 0 this means to scan the port range 7100..7109 for Iris servers and connect to the first server.

    Connects to an already initialised Iris server

    :param host:
        Hostname or IP address of the computer running the model.

    :param port:
        Port number that the model is listening on.

    :param verbose:
        If True, extra debugging information is printed
    """
    client = _NewTCPClient(host, port, client_name, timeoutInMs, verbose)
    return _NewModel(synchronous, client, verbose)


# NOTE: kept for backward compatibility reason. Prefer using NewNetworkModel.
class NetworkModel(AsyncModel):
    """An Iris model connected to an Iris server

    When port is 0 this means to scan the port range 7100..7109 for Iris servers and connect to the first server.
    """

    def __init__(self, host = "localhost", port = 0, timeoutInMs = 5000, client_name = "client.iris_debug", verbose = False):
        """
        Connects to an already initialised Iris server

        :param host:
            Hostname or IP address of the computer running the model.

        :param port:
            Port number that the model is listening on.

        :param verbose:
            If True, extra debugging information is printed
        """
        client = _NewTCPClient(host, port, client_name, timeoutInMs, verbose)
        AsyncModel.__init__(self, client, verbose)


def NewUnixDomainSocketModel(sock, timeoutInMs=5000, client_name="client.iris_debug", synchronous=False, verbose=False):
    """
    An Iris model connected to an Iris instance via unix domain socket
    """
    client = iris.IrisUnixDomainClient.ConnectToSocket(
            sock, client_name=client_name, timeout=timeoutInMs,
            verbose=1 if verbose else 0)
    try:
        client.irisCall().simulation_waitForInstantiation(instId=1)
    except iris.error.IrisError as e:
        if e.code == iris.error.E_function_not_supported_by_instance:
            warn("Simulation does not support simulation_waitForInstantiation: unable to guarantee instantiation")
        else:
            raise

    return _NewModel(synchronous, client, verbose)
