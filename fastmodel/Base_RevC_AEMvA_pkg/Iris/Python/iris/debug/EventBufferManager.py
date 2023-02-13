import threading

from collections import namedtuple

EventSourceEntry = namedtuple("EventSourceEntry", "fields callbacks")

# Fully identifies an EventSource
# inst_id: The instance id of the emitter
# source_id: The EventSourceId, the Id of the EventSource on the emitter
EventKey = namedtuple("EventKey", "inst_id source_id")


class EventBufferManager(object):
    """Manage user event callbacks for a particular target instance."""

    EventBufferCallbackDesc = {
            "description": "Receive the content of the event buffer",
            "args": {
                "evBufId": {"type": "NumberU64", "description": "EventBuffer id"},
                "reason": {"type": "String", "description": "Either 'send' or 'flush' based on the EventBuffer mode of operation and the source of the flush"},
                "events": {"type": "Array", "description": "List of events that were stored in the buffer"}
                },
            "retval": {"type": "Null"}
            }


    def __init__(self, client, target, verbose=False):
        self.__verbose = verbose
        self.__target = target
        self.__client = client
        self.__instId = target.instId
        self.__dirty = False
        self.__event_buffer = None
        self.__event_info = []
        self.__callbacks_by_event_key = {}
        self.__event_key_by_info_id = []
        self.__event_buffer_flush_event = threading.Event()
        self.__event_buffer_flush_data = None
        self.__event_buffer_callback_name = self.__client.instance.registerFunction(
                "onEventBufferFlush", self.__on_event_buffer_flush,
                EventBufferManager.EventBufferCallbackDesc, uniquify=True
                )

    def __on_event_buffer_flush(self, evBufId, reason, events):
        assert self.__event_buffer_flush_data is None
        self.__event_buffer_flush_data = events
        self.__event_buffer_flush_event.set()

    def flush_event_buffer(self):
        if self.__event_buffer is None:
            return []
        self.__event_buffer_flush_event.clear()
        self.__client.irisCall().eventBuffer_flush(
                instId=self.__instId, evBufId=self.__event_buffer
                )
        if not self.__event_buffer_flush_event.wait(
                timeout=(self.__client.timeoutInMs / 1000.0)
                ):
            raise TimeoutError()
        events = self.__event_buffer_flush_data
        self.__event_buffer_flush_data = None
        return events

    def rebuilds(self):
        if not self.__dirty:
            return

        self.__dirty = False

        # delete the previous event buffer if any
        if self.__event_buffer:
            self.__client.irisCall().eventBuffer_destroy(
                    instId=self.__instId, evBufId=self.__event_buffer)
        # build the eventStreamInfos expected by Iris endpoint
        event_info = []
        for ((instId, evSrcId), e) in self.__callbacks_by_event_key.items():
            event_info.append({
                "sInstId": instId,
                "evSrcId": evSrcId,
                "fields": e.fields
                })
        # create the new event buffer with the new event info
        info = self.__client.irisCall().eventBuffer_create(
                instId=self.__instId, eventStreamInfos=event_info,
                ebcFunc=self.__event_buffer_callback_name,
                ebcInstId=self.__client.instId, syncEbc=False)
        self.__event_buffer = info["evBufId"]
        # assign the new event buffer to the sync step
        self.__client.irisCall().step_syncStepSetup(
                instId=self.__instId, evBufId=self.__event_buffer)
        # build the map EventInfoId -> EventKey to allow us to trigger the
        # appropriate callbacks
        self.__event_key_by_info_id = []
        for i in info["eventStreamInfos"]:
            self.__event_key_by_info_id.append(
                    EventKey(i['sInstId'], i['evSrcId']))

    def trigger_callbacks(self, event_data):
        """Call the event callbacks based on the given event data.

        :param event_data The "events" field of the data returned by
                          `step_syncStep`
        """
        for ev in event_data:
            fields = ev["fields"]
            time = ev["time"]
            idx = ev["esInfoId"]
            event_key = self.__event_key_by_info_id[idx]
            callbacks = self.__callbacks_by_event_key[event_key].callbacks
            for c in callbacks:
                c(time, fields)

    def add_callback(self, instId, event_info, func, fields=None):
        if fields is None:
            fields = set(field.name for field in event_info.fields)
        elif not isinstance(fields, set):
            fields = set(fields)

        event_key = EventKey(instId, event_info.evSrcId)
        try:
            entry = self.__callbacks_by_event_key[event_key]
        except KeyError:
            # not a know event, but we are committed to add the callback
            entry = EventSourceEntry(set(), [])
            self.__callbacks_by_event_key[event_key] = entry
            self.__dirty = True

        entry.callbacks.append(func)

        if fields.issubset(entry.fields):
            return

        # we will need to rebuild the EventBuffer to register to those
        # additional fields
        entry.fields.update(fields)
        self.__dirty = True

    def remove_callback_evSrcId(self, instId, evSrcId):
        try:
            del self.__callbacks_by_event_key[(instId, evSrcId)]
        except KeyError:
            return
        self.__dirty = True

    def remove_callback_func(self, func_to_remove):
        for ek, entry in self.__callbacks_by_event_key.items():
            cbs = entry.callbacks
            idx = cbs.find(func_to_remove)
            if idx >= 0:
                del cbs[idx]
                self.__dirty = True
