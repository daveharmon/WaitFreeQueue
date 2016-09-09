# WaitFreeQueue
A C implementation of a wait free queue based on [this paper](http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf)

This is a far from finished project, see TODO section.

### Overview
A wait free data structure is a concurrent structure that guarantees each thread or process will complete in a bounded number of steps.
This is a stronger guarentee than a lock free data structure that guarantees each thread will complete its task.  Lock free structures don't take advantage of strong concurrency since only one thread does not starve.  

### TODOS
	- Test file currently does not work, segfaults with multiple threads
	- Once test file is working, verify our implementation
	- Add memory management, there is currently a lot of leakage
	- Improve Documentation, and test/verify thoroughly
