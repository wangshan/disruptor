disruptor
=========

C++ implementation of LMAX disruptor

The original Java implementation is here:
https://github.com/LMAX-Exchange/disruptor

This implementation is based on version 2.10.3, with addition of a dynamic
ringbuffer, which is based on a lock free queue implementation from this
project:
https://github.com/cameron314/readerwriterqueue
