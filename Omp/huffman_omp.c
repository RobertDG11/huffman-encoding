/* Copyright 2016 Gagarine Yaikhom (MIT License) */

//retinere de codificare la fiecare litera
//scriere in paralel cu threaduri
//calculat cate caractere are fiecare parte de fisier

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define MAX_BUFFER_SIZE 1024
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
int NUM_THREADS = 4;

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

int read_header(FILE *f);
int write_header(FILE *f);
int read_bit(FILE *f);
int write_bit(FILE *f, int bit);
int flush_buffer(FILE *f);
void decode_bit_stream(FILE *fin, FILE *fout);
int decode(const char* ifile, const char *ofile);
void encode_alphabet(FILE *fout, int character);
int encode(const char* ifile, const char *ofile);
void build_tree();
void add_leaves();
int add_node(int index, int weight);
void finalise();
void init();

void determine_frequency(FILE **f) {
	int ID; 		//numar thread
	int **aux;
	int c,j;

	//setare numar threaduri
	omp_set_num_threads( NUM_THREADS );

	//gasire dimensiune initiala a fisierului in nr de caractere
	fseek(f[0],0L,SEEK_END);
	original_size = ftell(f[0]);
	//mutare cap de citire la inceputul fisierului
	fseek(f[0],0L ,SEEK_SET);

	//fisier de dimensiuni mici
	if( original_size / NUM_THREADS < 10 ){
		while ((c = fgetc(f[0])) != EOF) {
			++frequency[c];
		}
		for (c = 0; c < num_alphabets; ++c)
		if (frequency[c] > 0) {
			++num_active;
		}
		return;
	}
	
	//alocare matrice de caractere
	aux = (int**)malloc(NUM_THREADS*sizeof(int*));
	for(c = 0; c < NUM_THREADS; c++)
		aux[c] = (int *)calloc(2 * num_alphabets, sizeof(int));

	#pragma omp parallel private(ID,c)
	{
	ID = omp_get_thread_num();
	int i;
	int steps = (original_size / NUM_THREADS) + 1;
	int start = ID * steps;
	int end = (ID + 1) * steps;

	if (end > original_size)
		end = original_size; 
	
	//printf("Thread %d start %d end %d\n",ID,start,end);
	fseek(f[ID],start ,SEEK_CUR);
	for(i = start; i < end; i++){
		c = fgetc(f[ID]);
		++aux[ID][c];
		}
	//}
	{
		#pragma omp barrier
	}
	if(ID == 0)
	for (c = 0; c < num_alphabets; ++c) {
		char ch = c;
		for(j = 0; j < NUM_THREADS; j++)
			frequency[c] += aux[j][c];
		if( frequency[c] ){
			//printf("total character %c este %d\n",ch,frequency[c]);
			++num_active;
		}
	}
	}
	//printf("done\n");
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


int encode(const char* ifile, const char *ofile) {
	int i,c;
	FILE **fin, *fout;
	fin = (FILE **)malloc(NUM_THREADS*sizeof(FILE *));
	for(i=0;i<NUM_THREADS;i++){
		if ((fin[i] = fopen(ifile, "rb")) == NULL) {
			perror("Failed to open input file");
			return FILE_OPEN_FAIL;
		}
	}
	if ((fout = fopen(ofile, "wb")) == NULL) {
		perror("Failed to open output file");
		for(i=0;i<NUM_THREADS;i++)
			fclose(fin[i]);
		return FILE_OPEN_FAIL;
	}

	determine_frequency(fin);	
	stack = (int *) calloc(num_active - 1, sizeof(int));
	allocate_tree();

	add_leaves();
	write_header(fout);
	
	build_tree();
	fseek(fin[0], 0, SEEK_SET);

	while ((c = fgetc(fin[0])) != EOF)
		encode_alphabet(fout, c);

	flush_buffer(fout);
	free(stack);

	for(i=0;i<NUM_THREADS;i++)
		fclose(fin[i]);
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



void print_help() {
	  fprintf(stderr,
		  "USAGE: ./huffman [encode | decode] "
		  "<input out> <output file> num_of_threads\n");
}

int main(int argc, char **argv) {
	if (argc != 5) {
		print_help();
		return FAILURE;
	}
	init();
	NUM_THREADS = atoi(argv[4]);
	if (strcmp(argv[1], "encode") == 0)
		encode(argv[2], argv[3]);
	else if (strcmp(argv[1], "decode") == 0)
		decode(argv[2], argv[3]);
	else
		print_help();
	finalise();
	return SUCCESS;
}
