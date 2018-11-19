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
	char *text = NULL;
	int nr_elements = 0;
	int chunkSize = 0;
	int parent = 0;
	int i;
	int num_alphabets = 256;
	int num_active = 0;
	int* frequency = NULL; 


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

	readData("text.in", rank, chunkSize, text);

	determine_frequency(text, frequency, &num_active, num_alphabets);

	if (rank == 0) {
		printf("%s\n", text);
		for (i = 0; i < num_alphabets; i++) {
			if (frequency[i] > 0)
				printf("[%c]:%d\n", i, frequency[i]);
		}

		printf("%d\n", num_active);
	}

	


	MPI_Finalize();


}
