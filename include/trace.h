#ifndef TRACE_H
#define TRACE_H

#include "pagesim.h"
#include <stdio.h>

/* Trace structure storing a sequence of page references */
typedef struct Trace {
    PageRef *refs;
    size_t count;
    size_t capacity;
    char description[128];
} Trace;

/* Trace lifecycle */
Trace *trace_create(size_t initial_capacity);
void trace_free(Trace *trace);
bool trace_append(Trace *trace, const PageRef *ref);
void trace_clear(Trace *trace);

/* Parsers */
Trace *trace_parse_string(const char *str);
Trace *trace_load_file(const char *filename);
int trace_save_file(const Trace *trace, const char *filename);

/* Streaming reader for huge traces (Milestone 7 producer-consumer) */
typedef struct TraceStream {
    FILE *fp;
    bool is_pipe;
    char buffer[256];
} TraceStream;

TraceStream *trace_stream_open(const char *filename_or_stdin);
bool trace_stream_next(TraceStream *stream, PageRef *out_ref);
void trace_stream_close(TraceStream *stream);

/* Synthetic trace generators */
Trace *trace_gen_belady(void);
Trace *trace_gen_locality(size_t length, int hot_pages, int cold_pages, double hot_access_prob);
Trace *trace_gen_looping(size_t length, int loop_size);
Trace *trace_gen_random(size_t length, int num_pages);
Trace *trace_gen_elf_pattern(size_t length);

#endif /* TRACE_H */
