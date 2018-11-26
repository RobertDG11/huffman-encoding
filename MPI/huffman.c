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
unsigned int original_size = 0;

typedef struct {
    int index;
    unsigned int weight;
} node_t;

node_t *nodes = NULL;
int num_nodes = 0;
int *leaf_index = NULL;
int *parent_index = NULL;

int free_index = 1;

int *stack;
int stack_top;

unsigned char buffer[MAX_BUFFER_SIZE];
int bits_in_buffer = 0;
int current_bit = 0;

int eof_input = 0;

void determine_frequency(char* text);
int read_header(FILE *f);
int write_header(FILE *f);
int read_bit(FILE *f);
int write_bit(FILE *f, int bit);
int flush_buffer(FILE *f);
void decode_bit_stream(FILE *fin, FILE *fout);
int decode(const char* ifile, const char *ofile);
void encode_alphabet(FILE *fout, int character);
int encode(char* text, const char *ofile, int chunkSize);
void build_tree();
void add_leaves();
int add_node(int index, int weight);
void finalise();
void init();
void readTopology(char* filename, int* neigh, int rank, int* nr_elements);
void readData(char* filename, int rank, int chunkSize, char* text);
int getSize(char *filename);
void readData(char* filename, int rank, int chunkSize, char* text);

void readTopology(char* filename, int* neigh, int rank, int* nr_elements) 
{
    FILE* fin;
    char line[200];

    fin = fopen(filename, "r");
    if (fin == NULL) {
        perror("Nu exista fisierul\n");
        exit(1);
    }

    while (fgets(line, 200, fin) != NULL) {
        int count = 0;
        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = ' ';
        }
        //every rank reads only his neighbours
        if (atoi(&line[0]) == rank) {
            char *tok = strtok(line, " ");
            while(tok != NULL) {
                if(count > 0) {
                    neigh[count - 1] = atoi(tok);
                    (*nr_elements)++;
                }
                count++;
                tok = strtok(NULL, " ");
            }  
        }
    }

    fclose(fin);
}

void readData(char* filename, int rank, int chunkSize, char* text)
{
    FILE* fin;
    int i;

    fin = fopen(filename, "r");
    if (fin == NULL) {
        perror("Nu exista fisierul\n");
        exit(1);
    }

    fseek(fin, rank * chunkSize, SEEK_SET);

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

void determine_frequency(char* text) 
{
    int i;

    for (i = 0; i < strlen(text); i++) {
        frequency[text[i]]++;
        original_size++;
    }

    for (i = 0; i < num_alphabets; i++) {
        if (frequency[i] > 0) {
            num_active++;
        }
    }
}

void init() {
    frequency = (int *)
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


int encode(char* text, const char *ofile, int chunkSize) {
    FILE *fout;
    if ((fout = fopen(ofile, "wb")) == NULL) {
        perror("Failed to open output file");
        return FILE_OPEN_FAIL;
    }

    determine_frequency(text);
    stack = (int *) calloc(num_active - 1, sizeof(int));
    allocate_tree();

    add_leaves();
    //write_header(fout);
    build_tree();

    int c;
    for (c = 0; c < chunkSize; c++) {
    	encode_alphabet(fout, (int)text[c]);
    }

    flush_buffer(fout);
    free(stack);
    fclose(fout);

    return 0;
}

void encode_alphabet(FILE *fout, int character) {
    int node_index;
    stack_top = 0;
    node_index = leaf_index[character + 1];
    while (node_index < num_nodes) {
        stack[stack_top++] = node_index % 2;
        node_index = parent_index[(node_index + 1) / 2];
    }
    while (--stack_top > -1)
        write_bit(fout, stack[stack_top]);
}

int decode(const char* ifile, const char *ofile) {
    FILE *fin, *fout;
    if ((fin = fopen(ifile, "rb")) == NULL) {
        perror("Failed to open input file");
        return FILE_OPEN_FAIL;
    }
    if ((fout = fopen(ofile, "wb")) == NULL) {
        perror("Failed to open output file");
        fclose(fin);
        return FILE_OPEN_FAIL;
    }

    if (read_header(fin) == 0) {
        build_tree();
        decode_bit_stream(fin, fout);
    }
    fclose(fin);
    fclose(fout);

    return 0;
}

void decode_bit_stream(FILE *fin, FILE *fout) {
    int i = 0, bit, node_index = nodes[num_nodes].index;
    while (1) {
        bit = read_bit(fin);
        if (bit == -1)
            break;
        node_index = nodes[node_index * 2 - bit].index;
        if (node_index < 0) {
            char c = -node_index - 1;
            fwrite(&c, 1, 1, fout);
            if (++i == original_size)
                break;
            node_index = nodes[num_nodes].index;
        }
    }
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

int read_bit(FILE *f) {
    if (current_bit == bits_in_buffer) {
        if (eof_input)
            return END_OF_FILE;
        else {
            size_t bytes_read =
                fread(buffer, 1, MAX_BUFFER_SIZE, f);
            if (bytes_read < MAX_BUFFER_SIZE) {
                if (feof(f))
                    eof_input = 1;
            }
            bits_in_buffer = bytes_read << 3;
            current_bit = 0;
        }
    }

    if (bits_in_buffer == 0)
        return END_OF_FILE;
    int bit = (buffer[current_bit >> 3] >>
        (7 - current_bit % 8)) & 0x1;
    ++current_bit;
    return bit;
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

int read_header(FILE *f) {
     int i, j, byte = 0, size;
     size_t bytes_read;
     unsigned char buff[4];

     bytes_read = fread(&buff, 1, sizeof(int), f);
     if (bytes_read < 1)
         return END_OF_FILE;
     byte = 0;
     original_size = buff[byte++];
     while (byte < sizeof(int))
         original_size =
             (original_size << (1 << 3)) | buff[byte++];

     bytes_read = fread(&num_active, 1, 1, f);
     if (bytes_read < 1)
         return END_OF_FILE;

     allocate_tree();

     size = num_active * (1 + sizeof(int));
     unsigned int weight;
     char *buffer = (char *) calloc(size, 1);
     if (buffer == NULL)
         return MEM_ALLOC_FAIL;
     fread(buffer, 1, size, f);
     byte = 0;
     for (i = 1; i <= num_active; ++i) {
         nodes[i].index = -(buffer[byte++] + 1);
         j = 0;
         weight = (unsigned char) buffer[byte++];
         while (++j < sizeof(int)) {
             weight = (weight << (1 << 3)) |
                 (unsigned char) buffer[byte++];
         }
         nodes[i].weight = weight;
     }
     num_nodes = (int) num_active;
     free(buffer);
     return 0;
}


int main(int argc, char** argv) {

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
    int* local_frequency;
    int loca_original_size;
    int local_num_active;

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
	char* ofile = malloc(30 * sizeof(char));

	readData("text.in", rank, chunkSize, text);

    local_frequency = (int *)
        calloc(2 * num_alphabets, sizeof(int));

	sprintf(ofile, "output%d.dat", rank);

	init();

	encode(text, ofile, chunkSize);

    chunkSize = getSize(ofile);

    if (rank == 0) {
        for (i = 0; i < nr_elements; i++) {
            MPI_Recv(local_frequency, num_alphabets, MPI_INT, neigh[i], 0, MPI_COMM_WORLD, &status);
            
            for(i = 0; i < num_alphabets; i++) {
                frequency[i]+=local_frequency[i];
            }   
        }

        for (i = 0; i < num_alphabets; i++) {
            if (frequency[i] > 0)
                printf("[%c]%d\n",i,frequency[i]);
        }
        
    }

    else {
        //leaf
        if (nr_elements == 1) {
            MPI_Send(frequency, num_alphabets, MPI_INT, parent, 0, MPI_COMM_WORLD);
        }
        else {
            //first we recive
            for (i = 0; i < nr_elements; i++) {
                if (neigh[i] != parent) {
                    MPI_Recv(local_frequency, num_alphabets, MPI_INT, neigh[i], 0, MPI_COMM_WORLD, &status); 

                    for(i = 0; i < num_alphabets; i++) {
                        frequency[i]+=local_frequency[i];
                    } 
                }
                else {
                    MPI_Send(frequency, num_alphabets, MPI_INT, parent, 0, MPI_COMM_WORLD);
                }
            }
        }
    }

	MPI_Finalize();
}