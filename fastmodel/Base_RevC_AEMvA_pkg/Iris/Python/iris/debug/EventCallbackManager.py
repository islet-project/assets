#!/usr/bin/env python
# \file   EventCallbackManager.py
# \brief  Manage event callbacks to user funtions.
#
# \date   Copyright Arm Limited 2018 All Rights Reserved.

import iris.iris as iris
import iris.error as error

import collections
import sys
import warnings

from . import Exceptions

if sys.version_info < (3, 0):
    _string_type = unicode
    _bytes_type = str
else:
    _string_type = str
    _bytes_type = bytes


class EventCallbackManager(object):
    """Manage user event callbacks for a particular target instance."""

    def __init__(self, client, target, verbose):
        self._client = client
        self._target_instId = target.instId
        self._user_callbacks = {}
        self._esIds_for_event_source = {}
        self._verbose = verbose
        self._ec_func_name = 'ec_userEventCallback_{}'.format(self._target_instId)

        # Add the Event callback function used for use callbacks.
        self._client.instance.registerEventCallback(self._ec_func_name, self._ec_userEventCallback,
                                                   'Call a user event callback function in the Python script.')

    def _ec_userEventCallback(self, esId, fields, time, sInstId, syncEc=False):
        """
        Event callback which is called by the lower level iris.iris library for any events registered by this object.
        It will check which event the call represents and call the user callback as appropriate.
        """
        if (sInstId != self._target_instId) or (esId not in self._user_callbacks):
            # Something has gone wrong and we have either received a callback from the wrong instance (this shouldn't happen
            # as we only register events for one target) or the source instance is correct and we don't have a callback
            # for this event stream. We could arrive at this situation in a few ways:
            # 1. A previous (now-unregistered) instance created this event stream, we ended up with the same instId
            #    and the target has not yet processed that it was removed which automatically destroys event streams.
            # 2. We registered for this event but it has fired before we managed to handle the response and add the
            #    user callback func to the dictionary.
            # 3. The event message was corrupt or there is a bug somewhere.
            # 1 and 2 are both race conditions and it is safe to ignore the event but it is useful to have some logging
            # in case it is caused by 3 and there is a bug.

            if self._verbose:
                warnings.warn('Received spurious event callback: esId={}, sInstId={}, fields={}'.format(esId, sInstId, fields))
            return
        func = self._user_callbacks[esId]
        func(time, fields)

    def add_callback(self, evSrcId, func, fields=None):
        """
        Create an event stream for the specified event source which will call back func().
        """
        args = {
            'instId' : self._target_instId,
            'ecFunc' : self._ec_func_name,
            'ecInstId' : self._client.instId,
            'evSrcId' : evSrcId,
        }
        if fields is not None:
            args['fields'] = fields

        try:
            esId = self._client.irisCall().eventStream_create(**args)
        except error.IrisError as e:
            if e.code == error.E_unknown_event_source_id:
                raise ValueError('Unknown event source')
            elif e.code == error.E_unknown_event_field:
                raise ValueError('Unknown event field')
            else:
                raise Exceptions.TargetError('Failed to add event callback: {}'.format(e))

        self._user_callbacks[esId] = func

        if evSrcId not in self._esIds_for_event_source:
            self._esIds_for_event_source[evSrcId] = []

        self._esIds_for_event_source[evSrcId].append(esId)

    def _destroy_event_stream(self, esId):
        """
        Destroy an event stream created by this object.
        """
        try:
            self._client.irisCall().eventStream_destroy(instId=self._target_instId, esId=esId)
        except error.IrisError as e:
            raise ValueError('Failed to remove event callback: {}'.format(e))

        del self._user_callbacks[esId]
        for evSrcId in self._esIds_for_event_source:
            if esId in self._esIds_for_event_source[evSrcId]:
                self._esIds_for_event_source[evSrcId].remove(esId)


    def remove_callback_func(self, func_to_remove):
        """
        Remove a registered callback by function object.
        """
        # Find all event streams for that function
        event_streams_to_destroy = []
        for esId, cb_func in self._user_callbacks.items():
            if cb_func == func_to_remove:
                event_streams_to_destroy.append(esId)

        if len(event_streams_to_destroy) == 0:
            raise ValueError('No event stream associated with function')

        for esId in event_streams_to_destroy:
            self._destroy_event_stream(esId)

    def remove_callback_evSrcId(self, evSrcId):
        """
        Remove a registered callback by evSrcId.
        """
        if evSrcId not in self._esIds_for_event_source:
            # Nothing to remove
            return

        for esId in self._esIds_for_event_source[evSrcId]:
            self._destroy_event_stream(esId)
