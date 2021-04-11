#include <sys/sysinfo.h>    // for get_nprocs()
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>

// Defines
// The size in bytes for each chunk
#define ZIP_CHUNK_SIZE 4095

// Structs
// Represents a repeated character
struct sequence_t {
    // Number of occurences
    unsigned int n : 32;

    // Character that's being repeated
    char c : 8;

    // NOTE there may be padding at the end so we can't use sizeof
};

// Items for our work queue that includes both the input and output for threads
struct chunk_todo_t {
    // Task: tell thread where to get input from

    // File number to use
    // TODO: maybe try using mmap
    int file_descriptor;

    // Chunked region of file
    int chunk_number;

    // Solutions: written to by threads instead of return value so that order is preserved

    // Array of sequences
    // TODO maybe try using malloc so we don't use so much RAM
    struct sequence_t sequences[ZIP_CHUNK_SIZE + 1];

    // Number of sequences in the sequences array
    size_t sequences_len;
};

// Forward declarations
void zip_init(FILE**, int);
void* zip_thread(void*);
void combine_results();
void write_entry(uint32_t, char);

// Global variables
// Current chunk number of the file that we're using
long work_queue_index = 0;

// Mutex lock for work queue
pthread_mutex_t work_queue_index_mutex;
pthread_mutex_t chunk_lock;

// Task queue + return values
struct chunk_todo_t* work_queue;

// How many items in the work queue
long work_queue_len;

int main (int argc, char ** argv) {
	int nthreads;
    int nfiles = argc - 1;
    FILE* files[nfiles];

    // Handle No args
	if (argc < 2) {
		puts("pzip: file1 [file2 ...]");
		return 1;
	}

    // Open input files
    for (int i = 1; i < argc; i++) {
        files[i - 1] = fopen(argv[i], "r");
        if (!files[i - 1]) {
            puts("file not found!!");
            return 2;
        }
    }

    // Initialize zipper
    zip_init(files, nfiles);

    // Spawn threads
    nthreads = get_nprocs();
    pthread_t tid[nthreads];
    for (int i = 0; i < nthreads; i++){
        int rc;
        if ((rc = pthread_create(&tid[i], NULL, zip_thread, NULL)) != 0) {
            puts("pthread_create error");
            exit(EXIT_FAILURE);
        }
    }

    // Join threads
    for (int i = 0; i < nthreads; i++) {
        int rc;
        if ((rc = pthread_join(tid[i], NULL)) != 0) {
            puts("pthread_join error");
            exit(EXIT_FAILURE);
        }
    }

    // Debug results
    // for (int i = 0; i < work_queue_len; i++) {
    //     printf("Chunk %d:\n", i);
    //     int len = work_queue[i].sequences_len;
    //     for (int j = 0; j < len; j++)
    //         printf("\t%d %c\n", work_queue[i].sequences[j].n, work_queue[i].sequences[j].c);
    // }

    // Get results from threads
    combine_results();

    return 0;
}

// Iinitialize shared state
void zip_init(FILE** files, int nfiles)
{
    // Initialize locks
    pthread_mutex_init(&work_queue_index_mutex, NULL);

    // Figure out number of tasks needed
    long nchunks[nfiles];
    work_queue_len = 0;
    for (int i = 0; i < nfiles; i++) {
        FILE* fp = files[i];
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp) / ZIP_CHUNK_SIZE + 1;
        nchunks[i] = size;
        work_queue_len += size;
    }

    // Make work queue
    work_queue = malloc(work_queue_len * sizeof(struct chunk_todo_t));

    // Initialize work queue
    work_queue_len = 0;
    for (long n = 0; n < nfiles; n++)
        for (long c = 0; c < nchunks[n]; c++) {
            struct chunk_todo_t* p = work_queue + work_queue_len++;
            p->file_descriptor = fileno(files[n]);
            p->chunk_number = c;
            p->sequences_len = 0;
        }
}

// Thread for zipping
void* zip_thread(void* _) {
    // Pull tasks from queue until we run out of work
    while (1) {
        // Get next task
        pthread_mutex_lock(&work_queue_index_mutex);
        int task_i = work_queue_index++;
        pthread_mutex_unlock(&work_queue_index_mutex);
        if (task_i >= work_queue_len)
            break;
        // printf("chunk # %d\n", task_i);
        struct chunk_todo_t* task = work_queue + task_i;

        // Open and seek to chunk
        FILE* fp = fdopen(dup(task->file_descriptor), "r");
        fseek(fp, task->chunk_number * ZIP_CHUNK_SIZE, SEEK_SET);

        // Sequence chunk as though it was just a single file
        long i = 0;
        struct sequence_t seq;
        seq.n = 1;
        seq.c = fgetc(fp);
        while (i < ZIP_CHUNK_SIZE) {
            // Get next character
            char c = fgetc(fp);
            // printf("seq: %c %d - %c\n", seq.c, seq.n, c);

            if (c == seq.c) {
                // Another duplicate
                seq.n++;
            } else {
                // New character, end of sequence
                // printf("push(%ld): %c(%c) %d\n", task->sequences_len, seq.c, c, seq.n);
                task->sequences[task->sequences_len++] = seq;

                seq.n = 1;
                seq.c = c;
            }

            // Check eof
            if (c == EOF)
                break;

            // Next char
            i++;
        }

        // Add last character too
        task->sequences[task->sequences_len++] = seq;

        fclose(fp);
    }

    return 0;
}

// Unifies the results of the zipping threads.
// Goes through the global `work_queue` and cleans up the "edges."
void combine_results() {
    int prev_n = -1;
    char prev_c = 0;
    int zerochunk = 0;
    for (int i = 0; i < work_queue_len; i++) {
        int len = work_queue[i].sequences_len;
        int last_index = len - 1;

        // Check if the first char of this chunk is same as last char of the previous chunk
        int cur_n = work_queue[i].sequences[0].n;
        char cur_c = work_queue[i].sequences[0].c;
        if (len == 0)
            zerochunk++;

        if (len == 1) {
            if (cur_c == prev_c) {
                prev_n += cur_n;
            }
            else {
                write_entry(prev_n, prev_c);
                prev_n = cur_n;
                prev_c = cur_c;
            }
            continue;
        }
        else if (cur_c == prev_c)
            write_entry(prev_n + cur_n, cur_c);
        else if (prev_n != -1) {
            write_entry(prev_n, prev_c);
            write_entry(cur_n, cur_c);
        }
        else {
            write_entry(cur_n, cur_c);
        }

        // Write middle sequences
        for (int j = 1; j < last_index; j++)
            write_entry(work_queue[i].sequences[j].n, work_queue[i].sequences[j].c);

        // Save last char for checking against first char of next chunk
        if (i != work_queue_len - 1) {
            prev_n = work_queue[i].sequences[last_index].n;
            prev_c = work_queue[i].sequences[last_index].c;
        }
        else
            write_entry(work_queue[i].sequences[last_index].n, work_queue[i].sequences[last_index].c);
    }
}

// Writes a single 5-byte run-length encoded entry to the standard output stream.
// The entry consists of a 4-byte integer (the run length), n, and the single character, c.
void write_entry(uint32_t n, char c) {
    fwrite(&n, sizeof(uint32_t), 1, stdout);
    fwrite(&c, sizeof(char), 1, stdout);
}
