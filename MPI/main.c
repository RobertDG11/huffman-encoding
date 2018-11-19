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
	int i;
	int num_alphabets = 256;
	int num_active = 0;
	int* frequency;
	node_t *nodes = NULL;
	int num_nodes = 0;
	int *leaf_index;
	int *parent_index;
	int free_index = 1;


	//master rank
	if (rank == 0) {
		int len;
		readTopology("topology.in", neigh, 0, &nr_elements);

		len = getSize("text.in");

		//printf("[%d]The len of the file is %d\n", rank,len);

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

	determine_frequency(text, frequency, &num_active, num_alphabets);

	// if (rank == 0) {
		printf("%d\n", leaf_index);
		add_leaves(frequency, num_alphabets, leaf_index, parent_index, nodes, &num_nodes);
		printf("%d\n", num_nodes);
		for (i = 0; i < num_active; i++)
			printf("[i]:%d, [w]:%d\n", nodes[i].index, nodes[i].weight);
	// }

	printf("AJUNGE AICI\n");

	//build_tree(&free_index, parent_index, nodes, leaf_index, &num_nodes);

	// if (rank == 0) {
	// 	printf("%c\n", nodes[0].weight);
	// }

	// if (rank == 0) {
	// 	printf("%s\n", text);
	// 	for (i = 0; i < num_alphabets; i++) {
	// 		if (frequency[i] > 0)
	// 			printf("[%c]:%d\n", i, frequency[i]);
	// 	}

	// 	printf("%d\n", num_active);
	// }





	


	MPI_Finalize();


}
