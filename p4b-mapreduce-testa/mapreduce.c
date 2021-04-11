#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

#include "mapreduce.h"

//
struct pair_t {
	char* key;
	char* value;
};


// Global state variables
struct pair_t** buckets;
char** fnames;
int* n_bucket_pairs;
int* n_bucket_alloc;
int* n_bucket_access;

pthread_mutex_t lock, file_lock;

Partitioner p;
Reducer r;
Mapper m;

int n_buckets;
int n_files_processed;
int n_files;

// Action for mapper threads
void* mapperAction(void* arg)
{
	while(n_files_processed < n_files) {
		pthread_mutex_lock(&file_lock);
		char* filename = NULL;
		if(n_files_processed < n_files) {
			filename = fnames[n_files_processed];
			n_files_processed++;
		}
		pthread_mutex_unlock(&file_lock);
		if(filename != NULL)
			m(filename);
	}
	return arg;
}

// Pull from queue
char* get_next(char* key, int bucket_number)
{
	int num = n_bucket_access[bucket_number];
	if(num < n_bucket_pairs[bucket_number]
		&& strcmp(key, buckets[bucket_number][num].key) == 0)
	{
		n_bucket_access[bucket_number]++;
		return buckets[bucket_number][num].value;
	}
	else {
		return NULL;
	}
}

// Action for reducer thread
void* reducerAction(void *arg)
{
	int* partitionNumber = (int *) arg;
	for(int i = 0; i < n_bucket_pairs[*partitionNumber]; i++) {
		if(i == n_bucket_access[*partitionNumber]) {
			r(buckets[*partitionNumber][i].key, get_next, *partitionNumber);
		}
	}
	return arg;
}

// Sort the buckets by key and then by value in ascending order
int compare(const void* pair1, const void* pair2)
{
	struct pair_t* p1 = (struct pair_t*) pair1;
	struct pair_t* p2 = (struct pair_t*) pair2;
	return strcmp(p1->key, p2->key)
		? strcmp(p1->key, p2->key)
		: strcmp(p1->value, p2->value);
}

// Sort files by increasing size
int compareFiles(const void* p1, const void* p2)
{
	char** f1 = (char**) p1;
	char** f2 = (char**) p2;
	struct stat st1, st2;
	stat(*f1, &st1);
	stat(*f2, &st2);
	long int size1 = st1.st_size;
	long int size2 = st2.st_size;
	return (size1 - size2);
}

void
MR_Emit (char * key, char * value)
{
	pthread_mutex_lock(&lock);
	// Assign to a bucket
	unsigned long bn = p(key, n_buckets);
	n_bucket_pairs[bn]++;
	int curCount = n_bucket_pairs[bn];
	// Checking if allocated memory has been exceeded,if yes allocating more memory
	if (curCount > n_bucket_alloc[bn]) {
		n_bucket_alloc[bn] *= 2;
		buckets[bn] = (struct pair_t*) realloc(buckets[bn], n_bucket_alloc[bn] * sizeof(struct pair_t));
	}
	buckets[bn][curCount - 1].key = (char*)malloc((strlen(key) + 1) * sizeof(char));
	strcpy(buckets[bn][curCount-1].key, key);
	buckets[bn][curCount - 1].value = (char*)malloc((strlen(value) + 1) * sizeof(char));
	strcpy(buckets[bn][curCount - 1].value, value);
	pthread_mutex_unlock(&lock);
}

unsigned long
MR_DefaultHashPartition (char * key, int num_buckets)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_buckets;
}

void
MR_Run (int argc, char * argv[],
        Mapper map, int num_mappers,
        Reducer reduce, int num_reducers,
        Partitioner partition)
{
	// At least as many mappers as files
	if (argc - 1 < num_mappers)
		num_mappers = argc - 1;

	// Initialize state
	pthread_t mapper_threads[num_mappers];
	pthread_t reducer_threads[num_reducers];
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&file_lock, NULL);
	p = partition;
	m = map;
	r = reduce;
	n_buckets = num_reducers;
	buckets = malloc(num_reducers * sizeof(struct pair_t*));
	fnames = malloc((argc - 1) * sizeof(char*));
	n_bucket_pairs = malloc(num_reducers * sizeof(int));
	n_bucket_alloc = malloc(num_reducers * sizeof(int));
	n_bucket_access = malloc(num_reducers * sizeof(int));
	n_files_processed = 0;
	n_files = argc - 1;
	int arr_idx[num_reducers];

	// Initialze arrays
	for (int i = 0; i < num_reducers; i++) {
		buckets[i] = malloc(1024 * sizeof(struct pair_t));
		n_bucket_pairs[i] = 0;
		n_bucket_alloc[i] = 1024;
		arr_idx[i] = i;
		n_bucket_access[i] = 0;
	}

	// Copy files for sorting
	for (int i = 0; i < argc - 1; i++) {
		fnames[i] = malloc((strlen(argv[i + 1]) + 1) * sizeof(char));
		strcpy(fnames[i], argv[i + 1]);
	}

	// Sorting files as Shortest File first
	qsort(&fnames[0], argc - 1, sizeof(char*), compareFiles);

	// Run mapper threads
	for (int i = 0; i < num_mappers; i++)
		pthread_create(&mapper_threads[i], NULL, mapperAction, NULL);
	for (int i = 0; i < num_mappers; i++)
		pthread_join(mapper_threads[i], NULL);


	// Sort partitions
	for (int i = 0; i < num_reducers; i++)
		qsort(buckets[i], n_bucket_pairs[i], sizeof(struct pair_t), compare);

	// Execute reducer threads
	for (int i = 0; i < num_reducers; i++)
	    if(pthread_create(&reducer_threads[i], NULL, reducerAction, &arr_idx[i]))
	    	printf("Error\n");
	for (int i = 0; i < num_reducers; i++)
		pthread_join(reducer_threads[i], NULL);


	// Release resources
	pthread_mutex_destroy(&lock);
	pthread_mutex_destroy(&file_lock);
	for (int i = 0; i < num_reducers; i++) {
		for (int j = 0; j < n_bucket_pairs[i]; j++)
			if (buckets[i][j].key != NULL && buckets[i][j].value != NULL) {
				free(buckets[i][j].key);
		    	free(buckets[i][j].value);
			}
		free(buckets[i]);
	}
	for (int i = 0; i < argc - 1; i++)
		free(fnames[i]);
	free(buckets);
	free(fnames);
	free(n_bucket_pairs);
	free(n_bucket_alloc);
	free(n_bucket_access);
}
