# \file   iris.py
# \brief  Iris bindings for Python.
# \date   Copyright ARM Limited 2015-2019 All Rights Reserved.

import contextlib
import socket
import threading
import json
import sys
import struct
import numbers
import re
import fnmatch
import atexit
import time

from . import error
from . import u64json

if sys.version_info < (3,0):
    # Queue is renamed to queue in Python 3
    import Queue as queue_namespace
    range = xrange
else:
    import queue as queue_namespace

IRIS_UINT64_MAX = 0xffffFFFFffffFFFF

class StructDict(dict):
    """Dictionary which allows to access members as attributes.
    """
    def __getattr__(self, attr):
        return self[attr]
    def __setattr__(self, attr, value):
        self[attr] = value


def toStructDict(pythonValue):
    """Convert dicts in any Python value to StructDicts recursively.
    """
    if isinstance(pythonValue, dict):
        return StructDict({ k : toStructDict(v) for k, v in pythonValue.items()})
    elif isinstance(pythonValue, list):
        return [ toStructDict(i) for i in pythonValue]
    else:
        return pythonValue


def compactStr(value, multiline = False, filterKeys = None):
    """Return a compact representation of a python value as string.

    - Omit Python 2.x unicode and long int prefixes/suffixes.
    - Omit quotes around object member names.
    - Print numbers in dec and hex.

    If multiline is True the return value will potentially span multiple lines,
    but simple arrays and objects which still be represented on a single line
    for compactness.

    filterKeys is an optional function which gets passed each key of dicts as
    argument. Members are ignored if filterKeys returns False.
    """
    # Using the "list of strings" idiom for efficiency.
    ret = []
    compactStrInternal(value, ret, multiline, "", "", filterKeys)
    return "".join(ret)


def isArrayOrObject(value):
    """Return True iff value is an array or an object.
    """
    return isinstance(value, dict) or isinstance(value, list)


def compactStrIsComplex(value):
    """Return True iff value should be represented across multiple lines by compactStr() because it is too complex.
    """
    if isinstance(value, dict):
        if len(value) > 4:
            return True
        for k, v in value.items():
            if isArrayOrObject(v):
                return True
    elif isinstance(value, list):
        for v in value:
            if isArrayOrObject(v):
                return True
    return False


def compactStrInternal(value, ret, multiline, indentSingleLine, indentMultiLine, filterKeys):
    """Helper for compactStr(): Recrusively build string representation for value.

    Append a compact representation of value as strings to ret.
    If multiline is True use a multi-line representation if the value
    is complex enough to justify this.

    Indent everything by "indent" which is a string containing space.
    """
    parSep = ""
    commaSep = " "
    indentOnce = ""
    indentInner = ""
    if multiline and compactStrIsComplex(value):
        parSep = "\n"
        commaSep = "\n"
        indentOnce = "    "
        indentInner = indentMultiLine
    if isinstance(value, dict):
        if (len(ret) == 0) or ret[-1][-1] == "\n":
            ret.append(indentSingleLine)
        ret.append("{" + parSep)
        first = True
        for k, v in sorted(value.items()):
            if filterKeys != None:
                if not filterKeys(k):
                    continue
            if not first:
                if (len(ret[-1]) > 0) and ret[-1][-1] == "\n":
                    ret[-1] = ret[-1][:-1]
                ret.append("," + commaSep)
            else:
                first = False
            ret.append(indentInner + indentOnce + k)
            ret.append(": ")
            compactStrInternal(v, ret, multiline, "", indentInner + indentOnce, filterKeys);
        ret.append(parSep + indentInner + "}" + parSep)
    elif isinstance(value, list):
        if (len(ret) == 0) or ret[-1][-1] == "\n":
            ret.append(indentSingleLine)
        ret.append("[" + parSep)
        first = True
        for v in value:
            if not first:
                if ret[-1][-1] == "\n":
                    ret[-1] = ret[-1][:-1]
                ret.append("," + commaSep)
            else:
                first = False
            compactStrInternal(v, ret, multiline, indentInner + indentOnce, indentInner + indentOnce, filterKeys);
        ret.append(parSep + indentInner + "]" + parSep)
    elif sys.version_info < (3, 0):
        if isinstance(value, (int, long)) and not isinstance(value, bool):
            if abs(value) < 10:
                ret.append(indentSingleLine + ("%d" % value))
            else:
                ret.append(indentSingleLine + ("%d (0x%x)" % (value, value)))
        elif isinstance(value, unicode):
            ret.append(indentSingleLine + ("\"%s\"" % value))
        elif isinstance(value, str):
            ret.append(indentSingleLine + ("\"%s\"" % value))
        else:
            ret.append(indentSingleLine + str(value))
    else:
        if isinstance(value, int) and not isinstance(value, bool):
            if abs(value) < 10:
                ret.append(indentSingleLine + ("%d" % value))
            else:
                ret.append(indentSingleLine + ("%d (0x%x)" % (value, value)))
        else:
            ret.append(indentSingleLine + str(value))

def nameToCname(name):
    """Return a valid C indetinfier deruve from name accoring to the
    rules specified for ResourceInfo.cname:
        - return "_" if name is empty
        - replace all non-C-identifier chars with underscores
        - if the first char is a digit, prepend an underscore
    """
    if len(name) == 0:
        return "_"
    name = re.sub("[^a-zA-Z0-9_]", "_", name)
    if name[0].isdigit():
        return "_" + name
    return name


def getAsInt(thing, default = 0):
    """Convert thing to an int, returning default if the conversion fails.
    """
    try:
        return int(thing)
    except ValueError:
        return default


class IrisPythonAdapter(object):
    """Adapter which allows to call Iris functions using python syntax.
    """
    def __init__(self, irisTcpClient, verbose):
        """Constructor.

        verbose is an unsigned integer indicating the verbose level.
        """
        self.irisTcpClient = irisTcpClient
        self.verbose = verbose


    def __getattr__(self, item):
        """This is called for everything which is not explicitly defined in this object.
        We treat everything unknown as an Iris function since this is the purpose of this class.
        """
        def thunk(**kwargs):
            """Wrapper calling callIrisFunction() with the function name and arguments when being invoked as a function.
            """
            return self.callIrisFunction(item, **kwargs)
        self.__dict__[item] = thunk
        return thunk


    def setVerbose(self, level):
        """Set verbose level.
        """
        self.verbose = level


    def callIrisFunction(self, functionName, **kwargs):
        """Call Iris function.

        Return pair (IrisErrorCode,ReturnValue).
        """
        if self.verbose:
            print("C {}({})".format(functionName, compactStr(kwargs)))
        message = dict()
        requestId = self.irisTcpClient.generateRequestId()
        message["id"] = requestId
        message["jsonrpc"] = "2.0"
        message["method"] = functionName
        message["params"] = kwargs
        self.irisTcpClient.sendMessage(message)
        response = self.irisTcpClient.waitForResponse(requestId)
        if "result" in response:
            if self.verbose:
                print("R result=" + compactStr(response["result"]))
            return toStructDict(response["result"])
        elif "error" in response:
            if self.verbose:
                print("R error=" + compactStr(response["error"]))
            raise error.IrisError(response['error']['code'], response['error']['message'])
        else:
            raise error.IrisError(error.E_malformatted_response, 'Received an invalid response', response)

def isValidRequest(request):
    # No need to check 'id', which is missing for a notification.
    return all(attr in request for attr in ('method', 'params', 'jsonrpc')) and request.jsonrpc == '2.0'

class IrisInstance(object):

    def __init__(self):
        self.propertyMap = dict()
        self.images = []
        self.functionInfoMap = dict()
        self.functionInfoMap["instance_ping"] = \
            {"description":"Ping instance. Send a dummy payload argument (any type) to an instance and receive the same payload value back as a return value. No-op. Has no side effects on instance.",
             "args":{
               "instId":{"type":"NumberU64", "description":"Instance id."},
               "payload":{"type":"Value", "description":"Dummy payload argument which is returned unmodified."}},
             "retval": {"type":"Value", "description":"Payload argument."}}
        self.functionInfoMap["instance_getProperties"] = \
            {"description":"Get instance properties.",
             "args":{"instId":{"type":"NumberU64", "description":"Instance id."}},
             "retval":{"type":"Object", "description":"Object mapping property names onto property values (any type)."}}
        self.functionInfoMap["instance_checkFunctionSupport"] = \
             {"description":"Interface discovery: Check whether an instance supports all given functions and all given arguments. Returns true iff all functions and all arguments are supported.",
              "args":{
                "instId":{"type":"NumberU64", "description":"Instance id."},
                "functions":{"type":"Array", "description":"Array of FunctionSupportRequest objects (list of functions and args to query)."}},
              "retval":{"type":"Boolean", "description":"True iff all given functions and arguments are supported."}}
        self.functionInfoMap["instance_getFunctionInfo"] = \
             {"description":"Interface discovery: Get list of all functions supported by an instance, and all function meta-info like arguments and supported enum values. Optionally the list of functions can be filtered by a given prefix.",
              "args":{
                "instId":{"type":"NumberU64", "description":"Instance id."},
                "prefix":{"type":"String", "description":"Return only functions which have a name which starts with prefix.", "optional":True}},
              "retval":{"type":"Object", "description":"Object which maps function names onto FunctionInfo objects."}}

        self.functionInfoMap["image_loadDataRead"] = \
             {"description":"Retrieve a chunk of data. Callback function of image_loadDataPull().",
              "args":{
                "instId":{"type":"NumberU64", "description":"Instance id."},
                "tag":{"type":"NumberU64", "description":"The tag used in the image_loadDataPull() call."},
                "position":{"type":"NumberU64", "description":"Absolute read position in bytes, relative to the start of the image."},
                "size":{"type":"NumberU64", "description":"Size of the chunk to read in bytes."},
                "end":{"type":"Boolean", "description":"If this is present and True, This is the last callback for this tag.", "optional":True}},
              "retval":{"type":"Object", "description":"ImageReadResult Object. Read data and size."}}

    def instance_checkFunctionSupport(self, functions):
        for function in functions:
            if function.name not in self.functionInfoMap:
                return False
            if args in function:
                raise error.IrisError(error.E_function_not_supported_by_instance, 'TODO')
        return True


    def instance_getProperties(self):
        return self.propertyMap

    def instance_getFunctionInfo(self, prefix=None):
        if prefix:
            return {functionName: info for functionName, info in self.functionInfoMap.items() if functionName.startswith(prefix)}
        return self.functionInfoMap

    def instance_ping(self, payload=None):
        return payload

    def setProperty(self, name, value):
        self.propertyMap[name] = value

    def openImage(self, path):
        tag = len(self.images)
        try:
            image = open(path, 'r')
            self.images.append(image)
            return tag
        except IOError:
            return -1

    def registerFunction(self, name, func, func_desc, uniquify=False):
        """Publish an Iris function to this instance.
        """
        if uniquify:
            if name in self.functionInfoMap:
                for n in range(100):
                    new_name = "{}_{}".format(name, n)
                    if new_name not in self.functionInfoMap:
                        name = new_name
                        break

        if name in self.functionInfoMap:
            if uniquify:
                raise ValueError("Too many function starting with {}".format(name))
            else:
                raise ValueError("A function named {} is already published".format(name))

        setattr(self, name, func)
        self.functionInfoMap[name] = func_desc
        return name

    def registerEventCallback(self, name, func, description):
        setattr(self, name, func)
        self.functionInfoMap[name] = \
                 {"description":description,
                  "args":{
                    "instId":{"type":"NumberU64", "description":"Instance id."},
                    "esId":{"type":"NumberU64", "description":"Event stream id."},
                    "fields":{"type":"Object", "description":"Object which contains the names and values of all event source fields requested by the eventEnable() call."},
                    "time":{"type":"NumberU64", "description":"Simulation time timestamp of the event."},
                    "sInstId":{"type":"NumberU64", "description":"Source instId: Instance which generated and sent this event."},
                    "syncEc":{"type":"Boolean", "description":"Synchronous callback behaviour.", "optional":True}},
                  "retval":{"type":"Null"}}

    def image_loadDataRead(self, tag, position, size, end = False):
        if tag >= len(self.images):
            raise error.IrisError(error.E_io_error, 'No such image with the specific tag')

        image = self.images[tag];
        if image == None:
            raise error.IrisError(error.E_io_error, 'No such image with the specific tag')

        image.seek(position, 0)
        strValue = image.read(size)
        numBytesRead = len(strValue)

        result = dict()
        result['data'] = convertStringToU64Array(strValue)
        result['size'] = numBytesRead

        if end:
            image.close()
            self.images[tag] = None

        return result

class IrisClientBase(object):
    """TCP client which can connect to an Iris TCP server.

    This class has two threads: receive thread and process thread. The receive thread is responsible
    to receive incoming requests/responses and put requests in a queue. The process thread is
    responsible to process the requests in that queue.

    We don't process a request directly in the receive thread, which will block on a nested request
    and fail to receive incoming messages. E.g. If we call an Iris function in a callback, the receive
    thread will wait for the response of that Iris function, so it will fail to receive any incoming
    messages.
    """
    def __init__(self, instanceName, instance, verbose, msg_format=None, timeoutInMs = 1000):
        """Constructor.

        verbose is an unsigned integer indicating the verbose level.
        """
        self.instanceName = instanceName # may be modified by connect() due to uniquification
        self.instance = instance
        self.instId = 0 # Will be set by connect(). Must be 0 so instanceRegistry_registerInstance() has a valid integer to compute a requestId. (The instId does not matter in this special case.)
        self.verbose = verbose
        self.sock = None
        self.irisPythonAdapter = IrisPythonAdapter(self, verbose)
        self.receiveThread = None
        self._setIsConnected(False)
        self.receivedResponses = dict()
        self.receivedResponsesCondVar = threading.Condition(threading.Lock())
        self.receivedResponsesSuccess = False
        self.receivedRequests = queue_namespace.Queue()
        self.nextRequestId = 0
        self.send_format = None
        self.timeoutInMs = timeoutInMs

        # A list of message formats that this client supports in order of preference.
        all_supported_formats = ['IrisU64Json', 'IrisJson']
        if msg_format is None:
            self.supported_formats = all_supported_formats
        elif msg_format in all_supported_formats:
            self.supported_formats = [msg_format]
        else:
            raise ValueError('Unsupported message format {}'.format(msg_format))


    def setVerbose(self, level):
        """Set verbose level.
        """
        self.verbose = level
        self.irisPythonAdapter.setVerbose(level)

    def getInstanceInfoList(self, instName = "*", onlyCores = False, allowEmptyList = False):
        """Convenience function to get a filtered list of instance infos.
        """
        instanceInfoList = self.irisCall().instanceRegistry_getList()
        r = []
        for instanceInfo in instanceInfoList:
            # Skip if name does not match.
            if not fnmatch.fnmatch(instanceInfo.instName, instName):
                continue

            if onlyCores:
                # Skip if not a core.
                try:
                    properties = self.irisCall().instance_getProperties(instId = instanceInfo.instId)
                except error.IrisError:
                    properties = dict()
                executesSoftware = getAsInt(properties.get('executesSoftware', 0)) != 0
                if not executesSoftware:
                    continue

            r.append(instanceInfo)

        if not allowEmptyList and (len(r) == 0):
            raise error.IrisError(error.E_unknown_instance_id, "No instances matching pattern found.")

        r.sort(key = lambda x: x.instName)

        return r

    def isConnected(self):
        """Return True iff connected.
        """
        return self.isConnected_;

    def handle_exit(self):
        """
        When an error or user shut-down forces iris to terminate unexpectedly,
        remember to call the disconnect function first.
        """
        if self.isConnected():
            self.disconnect(force = True, unregister_exit_handler = False)

    def _connect_handshake(self):
        """
        Performs the initial handshake to determine the format.
        """
        # IrisRpc CONNECT.
        request = 'CONNECT / IrisRpc/1.0\r\nSupported-Formats: {}\r\n\r\n'.format(', '.join(self.supported_formats))
        self.sendBlocking(request.encode())
        response = ""
        while True:
            if sys.version_info < (3, 0):
                response += self.sock.recv(4096)
            else:
                response += self.sock.recv(4096).decode()
            if "\r\n\r\n" in response:
                break

        for fmt in self.supported_formats:
            if fmt in response:
                self.send_format = fmt
                break
        else:
            raise error.IrisError(error.E_not_compatible,
                    'Server is not compatible with supported formats {}.'.format(', '.join(self.supported_formats)))

    def _connect_epilog(self):
        """
        Performs the last set up steps after the communication has been established
        """
        self._setIsConnected(True)

        #Register a function to be executed at system shutdown, so that
        #thread  closure is enforced on error or manual user shut-down.
        atexit.register(self.handle_exit)

        # Start receive loop.
        self.receiveThread = threading.Thread(target = self.receiveThreadProc, name = self.instanceName + '.receive_thread')
        self.receiveThread.daemon = True # Kill thread when program exits.
        self.receiveThread.start()

        # Start the thread to process received requests.
        self.processThread = threading.Thread(target = self.processReceivedRequests, name = self.instanceName + '.process_thread')
        self.processThread.daemon = True # Kill thread when program exits.
        self.processThread.start()

        # Call instanceRegistry_registerInstance().
        instanceInfo = self.irisCall().instanceRegistry_registerInstance(instName = self.instanceName, uniquify = True)
        self.instanceName = instanceInfo.instName # may be modified due to uniquification
        self.instId = instanceInfo.instId
        self.instance.setProperty("instId", str(self.instId))
        self.instance.setProperty("instName", self.instanceName)
        self.instance.setProperty("componentName", "iris.IrisTcpClient")


    def disconnect(self, force = False, unregister_exit_handler = True):
        """Disconnect from the Iris TCP server.

        The server will also unregister this instance when losing the connection.

        When force is True then do not process any pending requests/responses.
        This is useful when terminating the Python client with Ctrl-C (KeyboardInterrupt).
        """
        if force:
            # Tell threads to exit soon.
            self._setIsConnected(False)
            # Clear self.receivedRequests wuthout processing any
            # pending requests.
            while True:
                try:
                    self.receivedRequests.get(block = False)
                except queue_namespace.Empty:
                    break
            while True:
                try:
                    self.receivedRequests.task_done()
                except ValueError:
                    break
            # Close both directions of the socket.
            self.sock.close()
            return

        # Wait until all received requests are processed.
        self.receivedRequests.join()
        self._setIsConnected(False)
        # Done sending data. This will signal the server to unregister this client.
        self.sock.shutdown(1)    # 0 = done receiving, 1 = done sending, 2 = both
        self.receivedRequests.put(None) # Add an empty item to the queue to wake it up immediately.
        self.processThread.join()
        self.receiveThread.join()

        # Deregister the exit handler. Note: it is not actually needed,
        # but it is preferable to free up resources if possible.
        if sys.version_info[0] > 2 and unregister_exit_handler:
            atexit.unregister(self.handle_exit)

    def irisCall(self):
        """Call an Iris function.

        Calling this function returns an IrisPythonAdapter instance which allows
        to call arbitrary Iris functions.

        These Iris functions return a tuple (IrisErrorCode,ReturnValue).
        """
        return self.irisPythonAdapter


    def sendBlocking(self, data):
        """Send data to the connected server.

        Block until all data has been sent.
        """
        offset = 0
        while offset < len(data):
            numBytesSent = self.sock.send(data)
            offset += numBytesSent


    def receiveThreadProc(self):
        """Receive loop.

        This function continuously receives requests and adds responses to self.receivedResponses.
        """
        receiveBuf = b''
        needResync = False

        # Stop receiving data if the client is disconnected.
        while self.isConnected_:
            # Receive some data. This blocks if 0 bytes are available.
            try:
                receiveBuf += self.sock.recv(4096)
            except socket.timeout:
                if receiveBuf == b'':
                    continue
            except socket.error:
                self.sock.close()
                self._setIsConnected(False)
                break

            # The connection is closed as we received empty data.
            if receiveBuf == b'':
                self._setIsConnected(False)
                break

            # Did we previously lose synchronisation? Try to re-sync.
            if needResync:
                print("error: IrisTcpClient: need to re-sync after receiving trash")
                offset = receiveBuf.find(b'Iris')
                if offset >= 0:
                    receiveBuf = receiveBuf[offset:] # remove trash
                    needResync = False
                else:
                    continue

            # Check whether we received complete messages.
            while ((len(receiveBuf) >= 20 and receiveBuf.startswith(b'IrisJson:'))
                   or (len(receiveBuf) >= 16 and receiveBuf.startswith(b'IrisU64Json:'))):
                if receiveBuf.startswith(b'IrisJson:'):
                    parts = receiveBuf.split(b':', 2)
                    if len(parts) != 3:
                        print("error: IrisTcpClient: received trash after 'IrisJson:'")
                        needResync = True
                        break

                    try:
                        contentLength = int(parts[1])
                    except ValueError:
                        print("error: IrisTcpClient: parse error in content length integer after 'IrisJson:'")
                        needResync = True
                        break

                    # Not a complete request/response. Need to receive more data
                    if len(parts[2]) < contentLength + 1:
                        break

                    if parts[2][contentLength:contentLength+1] != b'\n':
                        print("error: IrisTcpClient: missing LF after IrisJson message")
                        needResync = True
                        break
                    # We got a complete message.
                    self.receiveThreadReceivedJsonMessage(parts[2][:contentLength].decode())
                    receiveBuf = parts[2][contentLength + 1:]
                elif receiveBuf.startswith(b'IrisU64Json:'):
                    contentLength = struct.unpack('<I', receiveBuf[12:16])[0]

                    if len(receiveBuf) < 16 + contentLength + 1:
                        # Not a complete message. Need to receive more data.
                        break
                    else:
                        # We have a complete message.
                        self.receiveThreadReceivedU64JsonMessage(receiveBuf[16:16+contentLength])
                        receiveBuf = receiveBuf[16 + contentLength + 1:]


    def receiveThreadReceivedMessage(self, message):
        """
        Handle a received message.

        Always called from the receive thread.
        """
        if not isinstance(message, dict):
            print("error: IrisTcpClient: receivedThreadReceivedJsonMessage(): message must be an object")
        if "method" in message:
            # Enqueue an incoming request, which will be handled by the process thread
            self.receivedRequests.put(toStructDict(message))
        else:
            # This is an incoming response.
            # Add response to receivedResponses and notify all threads waiting on responses.
            requestId = message["id"]
            with self.receivedResponsesCondVar:
                self.receivedResponses[requestId] = message
                self.receivedResponsesSuccess = True
                self.receivedResponsesCondVar.notify_all()

    def receiveThreadReceivedJsonMessage(self, messageText):
        """
        Decode and handle a recieved JSON message.

        Always called from the receive thread.
        """
        if self.verbose:
            message = json.loads(messageText, object_hook=jsonIntegerObjectWarning)
        else:
            message = json.loads(messageText)
        self.receiveThreadReceivedMessage(message)

    def receiveThreadReceivedU64JsonMessage(self, messageData):
        """
        Decode and handle a received U64Json message.

        Always called from the receive thread.
        """
        message = u64json.loads(messageData)
        self.receiveThreadReceivedMessage(message)


    def generateRequestId(self):
        """Generate request id for a new request.
        """
        requestId = (self.instId << 32) | self.nextRequestId
        self.nextRequestId += 1
        return requestId

    def sendMessage(self, message):
        """Send message (dict()).

        This function blocks until the message is sent.
        """
        if not self.isConnected_:
            raise error.IrisError(error.E_not_connected, "Not connected.")
        if self.send_format == 'IrisU64Json':
            msg_data = u64json.dumps(message)
            msg_len = struct.pack('<I', len(msg_data))
            messageText = b'IrisU64Json:' + msg_len + msg_data + b'\n'
        elif self.send_format == 'IrisJson':
            messageText = json.dumps(message, separators=(",", ":"))
            messageText = 'IrisJson:{}:{}\n'.format(len(messageText), messageText)
            messageText = messageText.encode()
        self.sendBlocking(messageText)


    def waitForResponse(self, requestId):
        """Wait until a response for requestId is received.

        Return response (dict()).

        Raise E_not_connected if the connection is closed.
        """
        with self.receivedResponsesCondVar:
            while requestId not in self.receivedResponses:
                if not self.isConnected_:
                    raise error.IrisError(error.E_not_connected, "Connection is closed.")
                if sys.version_info < (3, 0):
                    now = time.clock()
                    deadline = now + self.timeoutInMs * 1e-3
                    while now < deadline and not self.receivedResponsesSuccess:
                        self.receivedResponsesCondVar.wait(deadline - now)
                        now = time.clock()
                else:
                    self.receivedResponsesCondVar.wait_for(lambda: self.receivedResponsesSuccess, timeout=self.timeoutInMs/1000.0)
                if not self.receivedResponsesSuccess:
                    raise error.IrisError(error.E_no_response_yet, "No answer received for request: " + str(requestId))
                self.receivedResponsesSuccess = False
            response = self.receivedResponses[requestId]
            del self.receivedResponses[requestId]
        return response

    def processReceivedRequests(self):
        """
        Process all outstanding requests that have been received by this client.

        This method is called from the process thread. It blocks until this client is disconnected.
        """
        while self.isConnected_:
            try:
                # Set a timeout so that it won't block on an empty queue.
                req = self.receivedRequests.get(timeout = 10)
            except queue_namespace.Empty:
                continue;

            if req is not None:
                self.processRequest(req)
            if not self.isConnected_:
                break # happens on disconnect(force = True)
            self.receivedRequests.task_done()

        # Deplete the request queue.
        # We are disconnected from the server so it is arguable whether we should handle
        # outstanding requests as there will be nowhere to send the response, but handling them
        # is less confusing than silently ignoring them.
        # Notifications on the other hand (for example event callbacks) don't require a response
        # and might be valuable.
        while not self.receivedRequests.empty():
            req = self.receivedRequests.get(timeout = 10)
            if req is not None:
                self.processRequest(req)
            self.receivedRequests.task_done()

    def processRequest(self, request):
        hasResponse = 'id' in request  # A notification has no 'id' and has no response.
        if hasResponse:
            response = {}
            response['id'] = request.id
            response['jsonrpc'] = '2.0'

        if not isValidRequest(request):
            print('error: IrisTcpClient: Received invalid request {}'.format(request))
            if hasResponse:
                response['error'] = {}
                response['error']['code'] = error.E_malformatted_request
                response['error']['message'] = 'Invalid request'
        else:
            # Start building response object

            if not hasattr(self.instance, request.method):
                if hasResponse:
                    response['error'] = {}
                    response['error']['code'] = error.E_function_not_supported_by_instance
                    response['error']['message'] = 'Method "{}" not supported by instance'.format(request.method)
            else:

                handler = getattr(self.instance, request.method)

                try:
                    params = dict(request.params)
                    if self.verbose:
                        print("RecvC {}({})".format(request.method, compactStr(params)))
                    del params['instId']
                    result = handler(**params)
                    if hasResponse:
                        response['result'] = result
                except error.IrisError as iris_error:
                    if hasResponse:
                        response['error'] = {}
                        response['error']['code'] = iris_error.code
                        response['error']['message'] = iris_error.message
                        if iris_error.data is not None:
                            response['error']['data'] = iris_error.data

        # Never send a response for a notification even if the method is specified to return a value.
        if hasResponse:
            if self.verbose:
                if "result" in response:
                    print("SendR result=" + compactStr(response["result"]))
                elif "error" in response:
                    print("SendR error=" + compactStr(response["error"]))
                else:
                    print("SendR ? (result and error missing)")
            self.sendMessage(response)

    def _setIsConnected(self, isConnected):
        """Set connection state.
        """
        self.isConnected_ = isConnected


class IrisTcpClient(IrisClientBase):

    def __init__(self, instanceName, instance, verbose, msg_format=None, timeoutInMs=1000):
        IrisClientBase.__init__(self, instanceName, instance, verbose, msg_format, timeoutInMs)

    @staticmethod
    def connectToHostPort(hostname="localhost", port=0, timeout=1000, verbose=0):
        """Static convenience function: Create an IrisTcpClient and Try to
        connect it to Iris TCP server on hostname:port.

        If port is 0 then scan for Iris TCP server on ports 7100..7109.

        Returns a registered IrisTcpClient() instance or throws an iris error.
        """
        client = IrisTcpClient("client.IrisTcpClient_iris_py", IrisInstance(), verbose)
        client.connect(hostname, port, timeout)
        return client

    def connect(self, hostname, port, timeoutInMs=None):
        """Connect to an Iris TCP server.
        """
        if timeoutInMs is None:
            timeoutInMs = self.timeoutInMs

        if self.verbose >= 1:
            print("IrisTcpClient.connect(hostname={}, port={}, timeoutInMs={})".format(hostname, port, timeoutInMs))
        if self.isConnected_:
            raise error.IrisError(error.E_already_connected, 'Already connected to a server')

        # Handle port==0 which means: Scan ports 7100..7109 and connect to the first server found.
        if port == 0:
            for port in range(7100, 7110):
                try:
                    self.connect(hostname, port, timeoutInMs)
                    break
                except error.IrisError:
                    continue
            if not self.isConnected():
                raise error.IrisError(error.E_not_connected, "No TCP server(s) found!")
            return

        # TCP connect.
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeoutInMs/1000)
        try:
            self.sock.connect((hostname, port))
            self._connect_handshake()
        except socket.error:
            raise error.IrisError(error.E_connection_refused, 'Unable to connect to server')

        self.hostname = hostname
        self.port = port
        self._connect_epilog()


class IrisUnixDomainClient(IrisClientBase):

    DFLT_TIMEOUT = 5000

    def __init__(self, instanceName, instance, verbose, msg_format=None, timeoutInMs=DFLT_TIMEOUT):
        IrisClientBase.__init__(self, instanceName, instance, verbose, msg_format, timeoutInMs)

    @staticmethod
    def ConnectToSocket(
            sock, timeout=DFLT_TIMEOUT,
            client_name="client.IrisUnixDomainClient_iris_py", verbose=0):
        """Static convenience function: Create an IrisUnixDomainClient and try
        to connect to Iris instance at the other end of the specified socket.

        Returns a registered IrisUnixDomainClient() instance or throws an iris
        error.
        """
        client = IrisUnixDomainClient("", IrisInstance(), verbose, timeoutInMs=timeout)
        client.connect(sock)
        return client

    def connect(self, sock, timeoutInMs=None):
        """Establish communication on an already established connection.

        Mainly meant to be used for Unix Domain socket or other connection that
        are established before the simulation process is even created.
        """
        if timeoutInMs is None:
            timeoutInMs = self.timeoutInMs

        if self.verbose >= 1:
            print("IrisUnixDomainClient.connect(sock={}, timeoutInMs={})".format(sock.fileno(), timeoutInMs))
        if self.isConnected_:
            raise error.IrisError(error.E_already_connected, 'Already connected to a server')

        self.sock = sock
        self.sock.settimeout(timeoutInMs/1000)
        try:
            self._connect_handshake()
        except socket.error:
            raise
            #raise error.IrisError(error.E_connection_refused, 'Unable to connect to server')

        self._connect_epilog()


def _reinterpret_bitpattern(value, in_fmt, out_fmt):
    """Treat value as a bit pattern in one format and re-interpret it as another format"""
    # Add '<' to the beginning of the format to make sure we use little-endian byte order.
    in_fmt = '<' + in_fmt
    out_fmt = '<' + out_fmt

    return struct.unpack(out_fmt, struct.pack(in_fmt, value))

def _split_u64_chunks(value, bit_width):
    """Split a arbitrary size number into unsigned 64-bit chunks"""
    # Calculate the number of chunks it will be split into based on the bit width.
    n_chunks = (bit_width + 63) // 64

    if value < 0:
        # We have a signed negative value which needs to be converted to a bit pattern
        # (i.e the same value but treated as positive unsigned). We can do that by
        # doing a bitwise and.
        value = value & ((1 << bit_width) - 1)

    # Loop through each chunk, slicing off the lowest 64-bits each time and adding
    # them to a list.
    chunks = []
    for i in range(n_chunks):
        chunks.append(value & 0xFFFFFFFFFFFFFFFF)
        value >>= 64

    return chunks

def to_bitpattern(value, bit_width=64):
    """Convert a numeric value into a NumberU64[] bit pattern representation."""
    if isinstance(value, numbers.Integral):
        return _split_u64_chunks(value, bit_width)
    elif isinstance(value, numbers.Real):
        float_to_int = {
            # bitWidth  Input Format    Output format
            64:         ('d',           'Q'),
            32:         ('f',           'I'),
        }
        if bit_width not in float_to_int:
            raise ValueError('Invalid bit width {} (must be one of [{}])'.format(bit_width,
                                                                                 ', '.join(sorted(float_to_int.keys()))))
        in_fmt, out_fmt = float_to_int[bit_width]
        return _reinterpret_bitpattern(value, in_fmt, out_fmt)
    elif isinstance(value, bytes) or isinstance(value, bytearray):
        chunks = []
        for i in range(0, len(value), 8):
            chunks.append(int.from_bytes(value[i:i+8], 'little'))
        return chunks
    else:
        raise TypeError('Value must be a floating-point or integer type')

def from_bitpattern(bit_pattern, type, bit_width=64):
    """Convert a NumberU64[] bit pattern to a numeric value"""
    if type == 'numericFp':
        int_to_float = {
            # bitWidth  Input Format    Output format
            64:         ('Q',           'd'),
            32:         ('I',           'f'),
        }
        if bit_width not in int_to_float:
            raise ValueError('Invalid bit width {} (must be one of [{}])'.format(bit_width,
                                                                                 ', '.join(sorted(int_to_float.keys()))))
        in_fmt, out_fmt = int_to_float[bit_width]
        return _reinterpret_bitpattern(bit_pattern[0], in_fmt, out_fmt)[0]
    elif type in ('numeric', 'numericSigned'):
        value = 0
        for i, chunk in enumerate(bit_pattern):
            # Chunks are in little endian order
            value += (chunk << (i * 64))

        # Mask out any data that appears above bit_with bits
        value = value & ((1 << bit_width) - 1)

        if type == 'numericSigned':
            if value >> (bit_width - 1) != 0:
                # The top bit is set so this is a negative number.
                value -= (1 << bit_width)

        return value
    else:
        raise ValueError('Invalid type "{}"'.format(type))

def decodeResourceReadResult(resourceInfos, resourceReadResult):
    """Generator function which decodes the values returned by resource_read().

    Return sequence of tuples (resourceInfo, value, undefinedBits, errorCode).

    value:
    - an integral type for type == numeric or numericSigned
    - float for type == numericFp
    - str for type == string
    - None if there was an error (see errorCode)

    undefinedBits:
        Is either None (no undefined bits or error) or an unsigned integer with a bit set for
        each undefined bit in value.

    errorCode:
        IrisErrorCode if an error occured for this resource, else None which means ok.
    """
    # Get errors.
    rscIdToError = {}
    if "error" in resourceReadResult:
        for index in range(0, len(resourceReadResult.error) - 1, 2):
            rscIdToError[resourceReadResult.error[index]] = resourceReadResult.error[index + 1]

    # Decode values and undefined bits and generate list entries.
    dataIndex = 0
    data = resourceReadResult.data
    dataLen = len(data)
    strIndex = 0
    strings = resourceReadResult.strings if 'strings' in resourceReadResult else []
    strLen = len(strings)
    undefinedBits = None
    if "undefinedBits" in resourceReadResult:
        undefinedBits = resourceReadResult.undefinedBits
    for regInfo in resourceInfos:
        value = None
        value_undef = None
        errorCode = None
        # Get errorCode.
        if regInfo.rscId in rscIdToError:
            errorCode = rscIdToError[regInfo.rscId]

        # Get value.
        type = regInfo.get('type', 'numeric')
        if type in ('numeric', 'numericSigned', 'numericFp'):
            n_chunks = (regInfo.bitWidth + 63) // 64
            if dataIndex + n_chunks > dataLen:
                raise error.IrisError(error.E_data_size_error,
                                'Data returned by resource_read() contains too few elements for resources.')

            value_chunks = data[dataIndex : dataIndex + n_chunks]
            value = from_bitpattern(value_chunks, type, regInfo.bitWidth)

            if undefinedBits is not None:
                undef_chunks = undefinedBits[dataIndex : dataIndex + n_chunks]
                value_undef = from_bitpattern(undef_chunks, 'numeric', regInfo.bitWidth)

            dataIndex += n_chunks

        elif regInfo.type == "string":
            if strIndex >= strLen:
                raise error.IrisError(error.E_data_size_error,
                                'strings returned by resource_read() contains too few elements for string resource.')
            value = str(strings[strIndex])  # Convert from unicode string to string.
            strIndex += 1
        elif regInfo.type == "noValue":
            pass # no data
        else:
            raise error.IrisError(E_internal_error, "Unknown resource type {}".format(regInfo.type))

        yield (regInfo, value, value_undef, errorCode)


def isAnyBitSet(bits):
    """Return True iff any bit is set in bits.

    Bits may be either a number or a list of numbers.
    """
    if bits == None:
        return False
    elif type(bits) is list:
        return sum(bits) > 0
    else:
        return bits != 0

def getResourceValueString(regInfo, value, undefinedBits, errorCode, decimal = False):
    """Format value, undefinedBits and errorCode into a readable string.
    """
    if errorCode:
        return "(Error: {})".format(error.IrisErrorCode(errorCode))
    if (sys.version_info < (3, 0) and type(value) in (int, long)) or (sys.version_info >= (3, 0) and type(value) is int):
        if decimal:
            s = "{:d}".format(value)
        else:
            s = "{:#0{width}x}".format(value, width = (regInfo.bitWidth + 3) // 4 + 2)
    elif type(value) is float:
        s = "{:g}".format(value)
    elif type(value) is list:
        s = " ".join("{:#016x}".format(x) for x in value)
    elif type(value) is str:
        s = value.encode("unicode_escape").decode("latin1")
    else:
        s = ""
    if isAnyBitSet(undefinedBits):
        s += " (undefinedBits={})".format(getResourceValueString(regInfo, undefinedBits, None, None))
    return s

def makeNamesHierarchical(resourceInfos, addMembers = True):
    """Make members name and cname hierarchical names for register fields.

    This is the same as IrisInstanceResource::makeNamesHierarchical() in C++.

    Iff addMembers is True then do not modify name and cname and instead add
    nameHierarchical and cnameHierarchical to each entry.
    """
    # Build lookup table.
    rscIdToIndex = {resourceInfos[i].get("rscId", IRIS_UINT64_MAX): i for i in range(len(resourceInfos)) if resourceInfos[i].rscId != IRIS_UINT64_MAX}

    # Remember which registers we already looked at.
    # This is necessary not only because of potential loops in the parentRscId
    # pointers, but it also solves the problem that we do no know the order in
    # which to process the resources upfront.
    done = set()

    # Modify names recursively.
    for i in range(len(resourceInfos)):
        makeNamesHierarchicalInternal(resourceInfos, rscIdToIndex, done, i, addMembers)

def makeNamesHierarchicalInternal(resourceInfos, rscIdToIndex, done, index, addMembers):
    """Internal helper function for makeNamesHierarchicalInternal.
    """
    if index in done:
        # Already done.
        return

    # Break loops.
    done.add(index)

    # Find parent resource.
    info = resourceInfos[index]
    if info.get("parentRscId", IRIS_UINT64_MAX) not in rscIdToIndex:
        # Either a non-child resource or an invalid or unknown parentRscId.
        if addMembers:
            info.nameHierarchical = info.name
            info.cnameHierarchical = info.cname
        return
    parentIndex = rscIdToIndex[info.parentRscId]

    # Recursively call for parent to potentially update parents name and cname.
    makeNamesHierarchicalInternal(resourceInfos, rscIdToIndex, done, parentIndex, addMembers)

    # Update names.
    if addMembers:
        info.nameHierarchical = resourceInfos[parentIndex].nameHierarchical + "." + info.name
        info.cnameHierarchical = resourceInfos[parentIndex].cnameHierarchical + "_" + info.cname
    else:
        info.name = resourceInfos[parentIndex].name + "." + info.name
        info.cname = resourceInfos[parentIndex].cname + "_" + info.cname

def convertStringToResourceData(regInfo, strValue):
    """Convert the input string value to resource data value according to resource type
    """
    if ('type' not in regInfo) or (regInfo.type in ('', 'numeric', 'numericFp')):
        try:
            value = int(strValue, 0)
        except ValueError:
            raise ValueError('expect an integer value for numeric/numericFp type')

        if (value >> regInfo.bitWidth) != 0:
            raise ValueError('too large value for the resource')
        counter = regInfo.bitWidth
        data = []
        # Insert uint64_t element into the result list
        while counter > 0:
            data.append(value & 0xFFFFffffFFFFffff)    # append an uint64_t to the list
            value = value >> 64                        # right shift and prepare for the next data
            counter = counter - 64
        return data
    elif regInfo.type == 'noValue':
        # Writing a noValue resource is always silently ignored.
        pass
    else:
        raise ValueError('regInfo has invalid type')

def convertStringToU64Array(strValue):
    """Convert the string to an array of NumberU64.

    Encoding: The bytes are stored in little endian in the NumberU64 values. The last element in
    the array may contain 1..8 valid bytes. Unused bytes are set to 0.
    """
    numBytes = len(strValue)
    if numBytes == 0:
        return []

    numU64 = (numBytes + 7) // 8
    # Extend the string ending with '\0', so that the string length is multiple of 8.
    # E.g. 'hello' is extended to: 'hello'+\0\0\0
    strExt = strValue.ljust(8 * numU64, '\0')
    # Convert the string to a list of uint64_t in little endian
    return struct.unpack('<{}Q'.format(numU64), strExt)

def convertBytearrayToU64Array(data):
    """Convert a bytearray to an array of NumberU64.

    Encoding: The bytes are stored in little endian in the NumberU64 values. The last element in
    the array may contain 1..8 valid bytes. Unused bytes are set to 0.
    """
    numBytes = len(data)
    if numBytes == 0:
        return []

    numU64 = (numBytes + 7) // 8
    # Append the data with 0, so the size is multiple of 8.
    # E.g. bytearray(b'hello') is extended to: bytearray(b'hello\x00\x00\x00')
    # But make a copy not to modifiy the user's data
    if (numBytes % 8) != 0:
        data = data[:]
        for _ in range((8 - numBytes % 8) % 8):
            data.append(0)
    # Convert the bytearray to a list of uint64_t in little endian
    if sys.version_info < (3, 0):
        return struct.unpack('<{}Q'.format(numU64), str(data))
    else:
        return struct.unpack('<{}Q'.format(numU64), data)

def jsonIntegerObjectWarning(obj):
    if '__Iris64L' in obj:
        print("warning: received integer encoded as object; this not supported by the Iris Python library: {}".format(obj))

    return obj


def try_iris(method, default, **kwargs):
    try:
        return method(**kwargs)
    except error.IrisError:
        return default

@contextlib.contextmanager
def connect(name, port, hostname='localhost', timeoutInMs=1000, verbosity=0, *args, **kwargs):
    """
    Utility function to manage Iris client lifecycle.
    Use connect() in a with statement to connect to an Iris server and automaticlly
    disconnect from it when the with statement context goes out of scope:

        with iris.connect('client.MyPythonScript', 7100) as client:
            client.irisCall().instance_ping(instId=123)

        # Client is now automatically disconnected.
    """
    instance = IrisInstance()
    client = IrisTcpClient(name, instance, verbosity, *args, **kwargs)
    client.connect(hostname, port, timeoutInMs)
    yield client
    client.disconnect()
