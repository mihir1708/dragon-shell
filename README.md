# DragonShell

A minimal Unix-style shell written in C that supports command execution, background jobs, and basic job tracking.

---

## Overview

DragonShell is a lightweight command-line shell implemented in C, designed to explore how Unix shells manage processes and jobs. It supports executing external programs, running commands in the background, and tracking active jobs through a dedicated job manager.

The project focuses on low-level system programming concepts such as process creation, signal handling, and inter-process coordination.

---

## What the Project Does

* Parses and executes user commands
* Launches foreground and background processes
* Tracks active and completed jobs
* Maintains a job table for running processes
* Uses system calls to manage process lifecycle

---

## Implementation Highlights

* Process management using `fork`, `exec`, and `wait`
* Background job tracking via a custom job tracker
* Separation of concerns between shell logic and job management
* Build automation using a Makefile

---

## Tools Used

* C (POSIX)
* GCC / Make
* Unix process and signal APIs

---

## Files

```
dragonshell.c     # Shell implementation and command loop
jobstracker.c     # Job tracking logic
jobstracker.h     # Job tracker definitions
Makefile          # Build instructions
```

---

## Notes

This project is intended as a focused exploration of systems programming concepts behind Unix shells. It prioritizes clarity and correctness over feature completeness or production-level robustness.
