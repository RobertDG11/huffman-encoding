#include "aux_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char** argv)
{

	int rank;
	int nProcesses;
	int l;

	MPI_Init(&argc, &argv);
	MPI_Status status;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

	int* neigh = (int*)malloc(20 * sizeof(int));
	char *text;
	int nr_elements = 0;
	int chunkSize = 0;
	int parent = 0;
	int i, c;
	int num_alphabets = 256;
	int num_active = 0;
	int* frequency;
	node_t *nodes;
	int num_nodes = 0;
	int *leaf_index;
	int *parent_index;
	int free_index = 1;
	unsigned int original_size = 0;

	int *stack;
	int stack_top;

	unsigned char buffer[MAX_BUFFER_SIZE];
	int bits_in_buffer = 0;
	int current_bit = 0;


	//master rank
	if (rank == 0) {
		int len;
		readTopology("topology.in", neigh, 0, &nr_elements);

		len = getSize("text.in");

		chunkSize = len / nProcesses;

		for (i = 0; i < nr_elements; i++) {

			MPI_Send(&chunkSize, 1, MPI_INT, neigh[i], 0, MPI_COMM_WORLD);
		}

		chunkSize = (len - (nProcesses - 1) * chunkSize);

	}
	//other ranks
	else {
		readTopology("topology.in", neigh, rank, &nr_elements);

		MPI_Recv(&chunkSize, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		parent = status.MPI_SOURCE;

		for (i = 0; i < nr_elements; i++) {

			if (neigh[i] == parent) {
				continue;
			}

			MPI_Send(&chunkSize, 1, MPI_INT, neigh[i], 0, MPI_COMM_WORLD);
		}
		
	}
	
	//all ranks
	text = (char*)malloc(chunkSize);
	frequency = (int*)calloc(2 * num_alphabets, sizeof(int));
	leaf_index = frequency + num_alphabets - 1;
	nodes = (node_t *)calloc(2 * num_active, sizeof(node_t));
  	parent_index = (int *)calloc(num_active, sizeof(int));

	readData("text.in", rank, chunkSize, text);

	if (rank == 0)
		printf("%s\n", text);

	FILE *fout;
	char* ofile = malloc(30 * sizeof(char));

	sprintf(ofile, "output%d.dat", rank);

	fout == fopen(ofile, "wb");

	determine_frequency(text, frequency, &num_active, num_alphabets, &original_size);

	stack = (int *)calloc(num_active - 1, sizeof(int));

	for (i = 0; i < num_alphabets; i++) {
		if (frequency[i] > 0) {
			add_node(-(i + 1), frequency[i], leaf_index, parent_index, nodes, &num_nodes);
		}
	}

	write_header(fout, nodes, num_active, original_size);

	build_tree(&free_index, parent_index, nodes, leaf_index, num_nodes);
	if(rank == 0)
		printf("%d\n", free_index);

	for (c = 0; c < chunkSize; c++) {
		int node_index;
		int character = (int)text[c];
		stack_top = 0;
		node_index = leaf_index[character + 1];

		while (node_index < num_nodes) {
		    stack[stack_top++] = node_index % 2;
		    node_index = parent_index[(node_index + 1) / 2];
		}

		while (--stack_top> -1) {
		    if (bits_in_buffer == MAX_BUFFER_SIZE << 3) {
		    	printf("LALALA\n");
		        size_t bytes_written = fwrite(buffer, 1, MAX_BUFFER_SIZE, fout);
		        
		        bits_in_buffer = 0;
		        memset(buffer, 0, MAX_BUFFER_SIZE);
		    }

		    if (stack[stack_top])
		        buffer[bits_in_buffer >> 3] |=
		            (0x1 << (7 - bits_in_buffer % 8));
		    bits_in_buffer++;
		}

	}

	//flush_buffer(fout, &bits_in_buffer, buffer);

	// free(stack);
	// fclose(fout);

	MPI_Finalize();


}
