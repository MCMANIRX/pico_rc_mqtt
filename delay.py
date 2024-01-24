# Non-blocking delay for multithreaded applications

import time


class NonBlockingDelay():
    def __init__(self):
        self._timestamp = 0
        self._delay = 0

    def timeout(self):
        return ((millis() - self._timestamp) > self._delay)

    def delay_ms(self, delay):
        self._timestamp = millis()
        self._delay = delay

def millis():
    return int(time.time() * 1000)

def delay_ms(delay):
    t0 = millis()
    while (millis() - t0) < delay:
        pass