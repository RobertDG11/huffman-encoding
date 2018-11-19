#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
	int index;
	unsigned int weight;
} node_t;

//reads the topology
void readTopology(char *filename, int* neigh, int rank, int* nr_elements);
//computes the zise of a file in bytes
int getSize(char *filename);
//reads the data
void readData(char* filename, int rank, int chunkSize, char* text);
//determine the frequency of each letter
void determine_frequency(char* text, int* frequency, int *num_active, int num_alphabets);
