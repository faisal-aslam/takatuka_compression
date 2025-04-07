// print_stacktrace.h
#ifndef PRINT_STACK_TRACE_H
#define PRINT_STACK_TRACE_H

void print_stacktrace();

#if defined(__linux__)

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <unistd.h>

void print_stacktrace() {
    void *buffer[100];
    int nptrs = backtrace(buffer, 100);
    char **strings = backtrace_symbols(buffer, nptrs);
    
    if (strings == NULL) {
        perror("backtrace_symbols");
        return;
    }

    printf("\n=== Stack Trace ===\n");
    for (int i = 0; i < nptrs; i++) {
        printf("%s\n", strings[i]);
    }
    printf("==================\n");
    
    free(strings);
}

#else
    void print_stacktrace() {
        printf("Stack trace not supported on this platform.\n");
    }
#endif

#endif
