#include "trace.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

Trace *trace_create(size_t initial_capacity)
{
    if (initial_capacity == 0) {
        initial_capacity = 64;
    }

    Trace *trace = (Trace *)malloc(sizeof(Trace));
    if (!trace) return NULL;

    trace->refs = (PageRef *)malloc(initial_capacity * sizeof(PageRef));
    if (!trace->refs) {
        free(trace);
        return NULL;
    }

    trace->count = 0;
    trace->capacity = initial_capacity;
    strncpy(trace->description, "User trace", sizeof(trace->description) - 1);
    trace->description[sizeof(trace->description) - 1] = '\0';
    return trace;
}

void trace_free(Trace *trace)
{
    if (!trace) return;
    if (trace->refs) {
        free(trace->refs);
        trace->refs = NULL;
    }
    free(trace);
}

bool trace_append(Trace *trace, const PageRef *ref)
{
    if (!trace || !ref) return false;

    if (trace->count >= trace->capacity) {
        size_t new_cap = trace->capacity * 2;
        PageRef *new_refs = (PageRef *)realloc(trace->refs, new_cap * sizeof(PageRef));
        if (!new_refs) return false;
        trace->refs = new_refs;
        trace->capacity = new_cap;
    }

    trace->refs[trace->count] = *ref;
    trace->refs[trace->count].timestamp = trace->count + 1;
    trace->count++;
    return true;
}

void trace_clear(Trace *trace)
{
    if (trace) {
        trace->count = 0;
    }
}

Trace *trace_parse_string(const char *str)
{
    if (!str) return NULL;

    Trace *trace = trace_create(64);
    if (!trace) return NULL;

    const char *p = str;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';')) {
            p++;
        }
        if (!*p) break;

        /* Check for hex virtual address 0x... */
        uint64_t vaddr = 0;
        int page_id = 0;
        AccessType type = ACCESS_READ;
        int pid = 0;

        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            char *end;
            vaddr = strtoull(p, &end, 16);
            page_id = (int)(vaddr >> 12);
            p = end;
        } else if (isdigit((unsigned char)*p) || *p == '-') {
            char *end;
            page_id = (int)strtol(p, &end, 10);
            vaddr = ((uint64_t)page_id) << 12;
            p = end;
        } else {
            p++;
            continue;
        }

        /* Check for optional :R or :W or trailing R/W token */
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == ':' || *p == '/') {
            p++;
            if (*p == 'W' || *p == 'w') {
                type = ACCESS_WRITE;
                p++;
            } else if (*p == 'R' || *p == 'r') {
                type = ACCESS_READ;
                p++;
            } else if (*p == 'X' || *p == 'x') {
                type = ACCESS_EXEC;
                p++;
            }
        } else if (*p == 'W' || *p == 'w') {
            type = ACCESS_WRITE;
            p++;
        } else if (*p == 'R' || *p == 'r') {
            type = ACCESS_READ;
            p++;
        }

        PageRef ref;
        ref.page_id = page_id;
        ref.access_type = type;
        ref.virtual_addr = vaddr;
        ref.pid = pid;
        ref.timestamp = 0;

        trace_append(trace, &ref);
    }

    return trace;
}

Trace *trace_load_file(const char *filename)
{
    if (!filename) return NULL;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return NULL;
    }

    Trace *trace = trace_create(256);
    if (!trace) {
        fclose(fp);
        return NULL;
    }

    snprintf(trace->description, sizeof(trace->description), "File: %s", filename);

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;

        /* Try parsing as single token or tuple */
        Trace *sub = trace_parse_string(p);
        if (sub) {
            for (size_t i = 0; i < sub->count; i++) {
                trace_append(trace, &sub->refs[i]);
            }
            trace_free(sub);
        }
    }

    fclose(fp);
    return trace;
}

int trace_save_file(const Trace *trace, const char *filename)
{
    if (!trace || !filename) return -1;

    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    fprintf(fp, "# PageSim Reference Trace: %s\n", trace->description);
    fprintf(fp, "# Total references: %lu\n", (unsigned long)trace->count);
    fprintf(fp, "# Format: <page_id> <R|W> [pid] [vaddr]\n");

    for (size_t i = 0; i < trace->count; i++) {
        const PageRef *r = &trace->refs[i];
        fprintf(fp, "%d %c %d 0x%lx\n", r->page_id, (char)r->access_type, r->pid, (unsigned long)r->virtual_addr);
    }

    fclose(fp);
    return 0;
}

TraceStream *trace_stream_open(const char *filename_or_stdin)
{
    TraceStream *ts = (TraceStream *)malloc(sizeof(TraceStream));
    if (!ts) return NULL;

    if (!filename_or_stdin || strcmp(filename_or_stdin, "-") == 0) {
        ts->fp = stdin;
        ts->is_pipe = true;
    } else {
        ts->fp = fopen(filename_or_stdin, "r");
        ts->is_pipe = false;
        if (!ts->fp) {
            free(ts);
            return NULL;
        }
    }

    return ts;
}

bool trace_stream_next(TraceStream *stream, PageRef *out_ref)
{
    if (!stream || !stream->fp || !out_ref) return false;

    while (fgets(stream->buffer, sizeof(stream->buffer), stream->fp)) {
        char *p = stream->buffer;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;

        Trace *t = trace_parse_string(p);
        if (t && t->count > 0) {
            *out_ref = t->refs[0];
            trace_free(t);
            return true;
        }
        if (t) trace_free(t);
    }

    return false;
}

void trace_stream_close(TraceStream *stream)
{
    if (!stream) return;
    if (!stream->is_pipe && stream->fp) {
        fclose(stream->fp);
    }
    free(stream);
}

Trace *trace_gen_belady(void)
{
    /* Classic Bélády's anomaly string: 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 */
    static const int belady_pages[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    size_t n = sizeof(belady_pages) / sizeof(belady_pages[0]);

    Trace *t = trace_create(n);
    if (!t) return NULL;

    strncpy(t->description, "Classic Belady Anomaly Reference String (1 2 3 4 1 2 5 1 2 3 4 5)", sizeof(t->description) - 1);

    for (size_t i = 0; i < n; i++) {
        PageRef ref;
        ref.page_id = belady_pages[i];
        ref.access_type = ACCESS_READ;
        ref.virtual_addr = (uint64_t)belady_pages[i] << 12;
        ref.pid = 1;
        ref.timestamp = i + 1;
        trace_append(t, &ref);
    }

    return t;
}

Trace *trace_gen_locality(size_t length, int hot_pages, int cold_pages, double hot_access_prob)
{
    Trace *t = trace_create(length);
    if (!t) return NULL;

    snprintf(t->description, sizeof(t->description), "80/20 Locality Trace (len=%lu, hot=%d, cold=%d)", (unsigned long)length, hot_pages, cold_pages);

    srand(42); /* Reproducible seed */

    for (size_t i = 0; i < length; i++) {
        double r = (double)rand() / (double)RAND_MAX;
        int page;
        if (r < hot_access_prob) {
            page = 1 + (rand() % hot_pages);
        } else {
            page = 1 + hot_pages + (rand() % cold_pages);
        }

        AccessType type = (rand() % 4 == 0) ? ACCESS_WRITE : ACCESS_READ;

        PageRef ref;
        ref.page_id = page;
        ref.access_type = type;
        ref.virtual_addr = (uint64_t)page << 12;
        ref.pid = 1;
        ref.timestamp = i + 1;
        trace_append(t, &ref);
    }

    return t;
}

Trace *trace_gen_looping(size_t length, int loop_size)
{
    Trace *t = trace_create(length);
    if (!t) return NULL;

    snprintf(t->description, sizeof(t->description), "Looping Sequential Pattern (len=%lu, loop=%d)", (unsigned long)length, loop_size);

    for (size_t i = 0; i < length; i++) {
        int page = 1 + (int)(i % (size_t)loop_size);
        PageRef ref;
        ref.page_id = page;
        ref.access_type = (i % 5 == 0) ? ACCESS_WRITE : ACCESS_READ;
        ref.virtual_addr = (uint64_t)page << 12;
        ref.pid = 1;
        ref.timestamp = i + 1;
        trace_append(t, &ref);
    }

    return t;
}

Trace *trace_gen_random(size_t length, int num_pages)
{
    Trace *t = trace_create(length);
    if (!t) return NULL;

    snprintf(t->description, sizeof(t->description), "Uniform Random Trace (len=%lu, pages=%d)", (unsigned long)length, num_pages);

    srand(1337);

    for (size_t i = 0; i < length; i++) {
        int page = 1 + (rand() % num_pages);
        PageRef ref;
        ref.page_id = page;
        ref.access_type = (rand() % 3 == 0) ? ACCESS_WRITE : ACCESS_READ;
        ref.virtual_addr = (uint64_t)page << 12;
        ref.pid = 1;
        ref.timestamp = i + 1;
        trace_append(t, &ref);
    }

    return t;
}

Trace *trace_gen_elf_pattern(size_t length)
{
    Trace *t = trace_create(length);
    if (!t) return NULL;

    strncpy(t->description, "ELF Program Execution Model (Text loop + Data scan + Stack)", sizeof(t->description) - 1);

    int text_base = 1;      /* Pages 1..4: Text instructions */
    int data_base = 10;     /* Pages 10..18: Array data */
    int stack_base = 100;   /* Pages 100..102: Stack frames */

    for (size_t i = 0; i < length; i++) {
        int page;
        AccessType type;

        int phase = (int)(i % 10);
        if (phase < 6) {
            /* Instruction fetch in tight text loop */
            page = text_base + (int)((i / 2) % 4);
            type = ACCESS_EXEC;
        } else if (phase < 8) {
            /* Data array access */
            page = data_base + (int)((i * 3) % 9);
            type = (i % 2 == 0) ? ACCESS_READ : ACCESS_WRITE;
        } else {
            /* Stack frame push/pop */
            page = stack_base + (int)(i % 3);
            type = ACCESS_WRITE;
        }

        PageRef ref;
        ref.page_id = page;
        ref.access_type = type;
        ref.virtual_addr = (uint64_t)page << 12;
        ref.pid = 1;
        ref.timestamp = i + 1;
        trace_append(t, &ref);
    }

    return t;
}
