#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_BUFFER_SIZE 256
#define INVALID_BIT_READ -1
#define INVALID_BIT_WRITE -1

#define FAILURE 1
#define SUCCESS 0
#define FILE_OPEN_FAIL -1
#define END_OF_FILE -1
#define MEM_ALLOC_FAIL -1

int num_alphabets = 256;
int num_active = 0;
int *frequency = NULL;
int *frequency_copy = NULL;
unsigned int original_size = 0;
int start_write = 0;

typedef struct {
    int index;
    unsigned int weight;
} node_t;

node_t *nodes = NULL;
int num_nodes = 0;
int *leaf_index = NULL;
int *parent_index = NULL;

int free_index = 1;

int **stack;
int *stack_top;
int size_text;
int *size_text_all;

unsigned char buffer[MAX_BUFFER_SIZE];
unsigned char buffer_dummy[MAX_BUFFER_SIZE];
int bits_in_buffer = 0;
int current_bit = 0;

int eof_input = 0;

char* in;
char* out;

void determine_frequency(char *infile, int rank, int nProcesses);
int read_header(FILE *f);
int write_header(FILE *f);
int read_bit(FILE *f);
int write_bit(FILE *f, int bit);
int flush_buffer(FILE *f);
void decode_bit_stream(FILE *fin, FILE *fout);
int decode(const char* ifile, const char *ofile);
void encode_alphabet(FILE *fout, int character);
void encode_character();
int encode(FILE* fout, int rank, const char *infile, int nProcesses);
void build_tree();
void add_leaves();
int add_node(int index, int weight);
void finalise();
void init();
void readTopology(char* filename, int* neigh, int rank, int* nr_elements);
void readData(char* filename, int rank, int chunkSize, int offset, char* text);
int getSize(char *filename);

void readData(char* filename, int rank, int chunkSize, int offset, char* text)
{
    FILE* fin;
    int i;

    fin = fopen(filename, "r");
    if (fin == NULL) {
        perror("Nu exista fisierul\n");
        exit(1);
    }

    fseek(fin, offset, SEEK_SET);

    fread(text, chunkSize, 1, fin);

    text[chunkSize] = '\0';

    fclose(fin);
}

int getSize(char *filename)
{

    FILE *fin;
    int len;

    fin = fopen(filename, "r");
    if (fin == NULL) {
        perror("Nu exista fisierul\n");
        exit(1);
    }

    fseek(fin, 0, SEEK_END);
    len = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    fclose(fin);

    return len;
    
}

void determine_frequency(char *infile, int rank, int nProcesses) 
{
    int i, c;
    int len = getSize(infile);
    int steps = (len / nProcesses) + 1;
    int start = rank * steps;
    int end = (rank + 1) * steps;


    FILE *fin;

    fin = fopen(infile, "r");
    if (fin == NULL) {
        perror("Nu exista fisierul\n");
        return FILE_OPEN_FAIL;
    }

    if (len <= 100) {
        if (rank == 0) {
            while ((c = fgetc(fin)) != EOF) {
                frequency[c]++;
                frequency_copy[c]++;

            }
        }
    } else {
        if (end > len)
            end = len; 
        fseek(fin, start, SEEK_CUR);
        for(i = start; i < end; i++){
            c = fgetc(fin);
            frequency[c]++;
            frequency_copy[c]++;
        }
    }

    fclose(fin);
}

void init() {
    frequency = (int *)
        calloc(2 * num_alphabets, sizeof(int));
    frequency_copy = (int *)
        calloc(2 * num_alphabets, sizeof(int));
    leaf_index = frequency + num_alphabets - 1;
}

void allocate_tree() {
    nodes = (node_t *)
        calloc(2 * num_active, sizeof(node_t));
    parent_index = (int *)
        calloc(num_active, sizeof(int));
}

void finalise() {
    free(parent_index);
    free(frequency);
    free(nodes);
}

int add_node(int index, int weight) {
    int i = num_nodes++;
    while (i > 0 && nodes[i].weight > weight) {
        memcpy(&nodes[i + 1], &nodes[i], sizeof(node_t));
        if (nodes[i].index < 0)
            ++leaf_index[-nodes[i].index];
        else
            ++parent_index[nodes[i].index];
        --i;
    }

    ++i;
    nodes[i].index = index;
    nodes[i].weight = weight;
    if (index < 0)
        leaf_index[-index] = i;
    else
        parent_index[index] = i;

    return i;
}

void add_leaves() {
    int i, freq;
    for (i = 0; i < num_alphabets; ++i) {
        freq = frequency[i];
        if (freq > 0)
            add_node(-(i + 1), freq);
    }
}

void build_tree() {
    int a, b, index;
    while (free_index < num_nodes) {
        a = free_index++;
        b = free_index++;
        index = add_node(b/2,
            nodes[a].weight + nodes[b].weight);
        parent_index[b/2] = index;
    }
}


int encode(FILE *fout, int rank, const char *infile, int nProcesses) {

    int start_write = 0;
    int steps = (original_size / nProcesses) + 1;
    int start = rank * steps;
    int end = (rank + 1) * steps;
    int bit_number, k, i, c;
    int size;
    int leaf_len = 0;
    int parent_len = 0;

    MPI_Status status;

    FILE* fin;
    fin = fopen(infile, "r");
    if (fin == NULL) {
        perror("Failed to open output file");
        return FILE_OPEN_FAIL;
    }

    size = start_write * 8;

    for (i = 0; i < rank; i++) {
        size += size_text_all[i];
    }


    if (rank != 0) {
        fseek(fout, size / 8, SEEK_SET);
            for( i = 0; i < size % 8; i++) {
                ++bits_in_buffer;
            }
    }


    fseek(fin, start, SEEK_SET);

    for(i = start; i < end; i++) {
        c = fgetc(fin);
        bit_number = stack_top[c];
    
        while (--bit_number > -1) {
            if (bits_in_buffer == MAX_BUFFER_SIZE << 3) {

                size_t bytes_written = fwrite(buffer, 1, MAX_BUFFER_SIZE, fout);

                bits_in_buffer = 0;

                memset(buffer, 0, MAX_BUFFER_SIZE);
                
            }
            if (stack[c][bit_number])
                buffer[bits_in_buffer >> 3] |= (0x1 << (7 - bits_in_buffer % 8));
            ++bits_in_buffer;      
        }
    }

    if (rank == 0) {
        while ((c = fgetc(fin)) != EOF)
        encode_alphabet(fout, c);

        flush_buffer(fout);    
    }
    
    free(stack_top);

    for(i = 0; i < num_alphabets; i++)
        free(stack[i]);

    free(stack);

    fclose(fin);
    fclose(fout);
    
    return 0;
}

void encode_alphabet(FILE *fout, int character) {
    int aux;
    aux = stack_top[character];
    while (--aux > -1)
        write_bit(fout, stack[character][aux]);
}

void encode_character() {
    int node_index, i;

    size_text = 0;
    stack_top = (int*)calloc(num_alphabets,sizeof(int));
    stack = (int**)calloc(num_alphabets,sizeof(int*));

    for(i = 0; i < num_alphabets; ++i)
        stack[i] = (int *) calloc(num_active - 1, sizeof(int));

    for(i = 0; i < num_alphabets; ++i) {
        char ch = i;
        if( frequency[i] != 0 ) {
            stack_top[i] = 0;
            node_index = leaf_index[i + 1];
            while (node_index < num_nodes) {
                stack[i][stack_top[i]++] = node_index % 2;
                node_index = parent_index[(node_index + 1) / 2];
            }
        }
    }
    for(i = 0; i < num_alphabets; i++)
        size_text += frequency_copy[i]*stack_top[i];

}

int write_bit(FILE *f, int bit) {
    if (bits_in_buffer == MAX_BUFFER_SIZE << 3) {
        size_t bytes_written =
            fwrite(buffer, 1, MAX_BUFFER_SIZE, f);
        if (bytes_written < MAX_BUFFER_SIZE && ferror(f))
            return INVALID_BIT_WRITE;
        bits_in_buffer = 0;
        memset(buffer, 0, MAX_BUFFER_SIZE);
    }
    if (bit)
        buffer[bits_in_buffer >> 3] |=
            (0x1 << (7 - bits_in_buffer % 8));
    ++bits_in_buffer;
    return SUCCESS;
}

int flush_buffer(FILE *f) {
    if (bits_in_buffer) {
        size_t bytes_written =
            fwrite(buffer, 1,
                (bits_in_buffer + 7) >> 3, f);
        if (bytes_written < MAX_BUFFER_SIZE && ferror(f))
            return -1;
        bits_in_buffer = 0;
    }
    return 0;
}

int write_header(FILE *f) {
     int i, j, byte = 0,
         size = sizeof(unsigned int) + 1 +
              num_active * (1 + sizeof(int));
     unsigned int weight;
     char *buffer = (char *) calloc(size, 1);
     if (buffer == NULL)
         return MEM_ALLOC_FAIL;

     j = sizeof(int);
     while (j--)
         buffer[byte++] =
             (original_size >> (j << 3)) & 0xff;
     buffer[byte++] = (char) num_active;
     for (i = 1; i <= num_active; ++i) {
         weight = nodes[i].weight;
         buffer[byte++] =
             (char) (-nodes[i].index - 1);
         j = sizeof(int);
         while (j--)
             buffer[byte++] =
                 (weight >> (j << 3)) & 0xff;
     }
     fwrite(buffer, 1, size, f);
     free(buffer);
     return 0;
}

void print_help() {
      fprintf(stderr,
          "USAGE: mpirun -np X ./huffman "
          "<input file> <output file>, where X is the number of processes\n");
}

int main(int argc, char** argv) {

	int rank;
	int nProcesses;

	MPI_Init(&argc, &argv);
	MPI_Status status;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

	int chunkSize = 0;
	int i, j, c;
	int parent = 0;
    int* local_frequency;
    int len;
    int offset = 0;
    int size = 0;


    if (argc != 3) {
        print_help();
        return FAILURE;
    }

    in = strdup(argv[1]);
    out = strdup(argv[2]);


	if (rank == 0) {
		len = getSize(in);
        original_size = len;

		chunkSize = len / nProcesses;

		for (i = 1; i < nProcesses; i++) {

			MPI_Send(&chunkSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		}

		chunkSize = (len - (nProcesses - 1) * chunkSize);

	}
	//other ranks
	else {
        len = getSize(in);

		MPI_Recv(&chunkSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
	
	}

	//all ranks
    local_frequency = (int *)
        calloc(2 * num_alphabets, sizeof(int));

	init();

    determine_frequency(in, rank, nProcesses);

    if (rank == 0) {
        for (i = 1; i < nProcesses; i++) {
            MPI_Recv(local_frequency, num_alphabets, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
            for(j = 0; j < num_alphabets; j++) {
                if (local_frequency[j] > 0)
                    frequency[j]+=local_frequency[j];
            }   
            
        }

        for (i = 1; i < nProcesses; i++) {
            MPI_Send(frequency, num_alphabets, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&original_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
        
    }

    else {
        MPI_Send(frequency, num_alphabets, MPI_INT, 0, 0, MPI_COMM_WORLD);

        MPI_Recv(frequency, num_alphabets, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(&original_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    }

    //all ranks
    size_text_all = calloc(nProcesses, sizeof(int));

    for (i = 0; i < num_alphabets; i++)
        if (frequency[i] > 0)
            ++num_active;


    FILE *fout;
    if ((fout = fopen(out, "wb")) == NULL) {
        
    }

    allocate_tree();

    if (rank == 0) {
        add_leaves();
        write_header(fout);
        build_tree();
    }

    start_write = getSize(out);

    MPI_Bcast(&num_nodes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(leaf_index, num_alphabets, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(parent_index, num_active, MPI_INT, 0, MPI_COMM_WORLD);

    encode_character();

    if (rank != 0) {
        MPI_Send(&size_text, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

        MPI_Recv(size_text_all, nProcesses, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);        

    }

    else {
        size_text_all[rank] = size_text;
        for (i = 1; i < nProcesses; i++) {
            MPI_Recv(&size_text_all[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
        }

        for (i = 1; i < nProcesses; i++) {
            MPI_Send(size_text_all, nProcesses, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

    }

    encode(fout, rank, in, nProcesses);    

	MPI_Finalize();
}