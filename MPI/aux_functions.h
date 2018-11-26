#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_BUFFER_SIZE 256
#define INVALID_BIT_READ -1
#define INVALID_BIT_WRITE -1

#define FAILURE 1
#define SUCCESS 0
#define FILE_OPEN_FAIL -1
#define END_OF_FILE -1
#define MEM_ALLOC_FAIL -1

typedef struct {
	int index;
	int weight;
} node_t;

//reads the topology
void readTopology(char *filename, int* neigh, int rank, int* nr_elements);
//computes the zise of a file in bytes
int getSize(char *filename);
//reads the data
void readData(char* filename, int rank, int chunkSize, char* text);
//determine the frequency of each letter
void determine_frequency(char* text, int* frequency, int *num_active, int num_alphabets, int* original_size);

int add_node(int index, int weight, int* leaf_index, int *parent_index, node_t* nodes, int* num_nodes);

void build_tree(int *free_index, int *parent_index, node_t* nodes, int* leaf_index, int num_nodes);

int write_bit(FILE *f, int bit, int* bits_in_buffer, unsigned char* buffer);

int flush_buffer(FILE *f, int* bits_in_buffer, unsigned char* buffer);

void encode_alphabet(FILE *fout, int character, int* stack_top, int* stack, 
	int* leaf_index, int num_nodes, int* parent_index, int* bits_in_buffer, 
	unsigned char* buffer);

int write_header(FILE *f, node_t* nodes, int num_active, int original_size);