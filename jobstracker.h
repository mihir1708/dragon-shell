// jobstracker.h
// Interface for tracking jobs in dragonshell.

#ifndef JOBSTRACKER_H
#define JOBSTRACKER_H

#include <stddef.h>
#include <sys/types.h>

// Job states
#define JOB_RUNNING   'R'
#define JOB_SUSPENDED 'T'

// Add a job
// pid: process ID
// cmd: command string
// state: 'R' (running) or 'T' (suspended)
int jobs_add(pid_t pid, const char *cmd, char state);


// Update the state of a job
// Returns 0 if found, -1 if not found
int jobs_update(pid_t pid, char new_state);


// Remove a job
// Returns 0 if removed, -1 if not found.
int jobs_remove(pid_t pid);

// Collect all job PIDs and states into provided arrays.
int jobs_collect(pid_t *pids, char *states, size_t max);

// Print all jobs in format: PID STATE CMD
void jobs_print(void);

// Clear all jobs (for cleanup)
void jobs_clear(void);

#endif // JOBSTRACKER_H