// jobstracker.c
// Tracks running/suspended jobs started by dragonshell
// Implementation through linked list

#include "jobstracker.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef LINE_LENGTH
#define LINE_LENGTH 100
#endif

// Node structure for linked list
typedef struct job_node {
    pid_t pid;
    char  state;                        // R or T
    char  cmd[LINE_LENGTH + 1];         // original command
    struct job_node *next;
} job_node;

// List head pointer
static job_node *jobs_head = NULL;

// Add a job (or update if it already exists)
// Returns 0 on success, -1 on memory error
int jobs_add(pid_t pid, const char *cmd, char state) {
    // If job with this pid already exists, update it
    for (job_node *n = jobs_head; n; n = n->next) {
        if (n->pid == pid) {
            n->state = state;
            if (cmd) {
                strncpy(n->cmd, cmd, LINE_LENGTH);
                n->cmd[LINE_LENGTH] ='\0';
            } else {
                n->cmd[0] = '\0';
            }
            return 0;
        }
    }

    // New job; add to front of list
    job_node *nn = (job_node *) malloc(sizeof(job_node));
    if (!nn) return -1;

    nn->pid = pid;
    nn->state = state;

    if (cmd) {
        strncpy(nn->cmd, cmd, LINE_LENGTH);
        nn->cmd[LINE_LENGTH] = '\0';
    } else {
        nn->cmd[0] = '\0';
    }

    nn->next = jobs_head;
    jobs_head = nn;
    return 0;
}

// Update the state of a job
// Returns 0 if found, -1 if not found
int jobs_update(pid_t pid, char new_state) {
    for (job_node *n = jobs_head; n; n = n->next) {
        if (n->pid == pid) {
            n->state = new_state;
            return 0;
        }
    }
    return -1;
}

// Remove a job
// Returns 0 if removed, -1 if not found
int jobs_remove(pid_t pid) {
    job_node *prev = NULL;
    job_node *cur = jobs_head;

    while (cur) {
        if (cur->pid == pid) {

            if (prev) prev->next = cur->next;
            else jobs_head = cur->next;
            free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return -1;
}

// Collect all job PIDs and states into provided arrays
// Returns number of jobs collected (up to max)
int jobs_collect(pid_t *pids, char *states, size_t max) {
    size_t i = 0;
    for (job_node *n = jobs_head; n && i < max; n = n->next) {
        pids[i] = n->pid;
        states[i] = n->state;  // 'R' or 'T'
        i++;
    }
    return (int)i;
}

// Print all jobs in format: PID STATE CMD
void jobs_print(void) {
    for (job_node *n = jobs_head; n; n = n->next) {
        printf("%d %c %s\n", (int)n->pid, n->state, n->cmd);
    }
}

// Clear all jobs
void jobs_clear(void) {
    job_node *n = jobs_head;
    while (n) {
        job_node *next = n->next;
        free(n);
        n = next;
    }
    jobs_head = NULL;
}