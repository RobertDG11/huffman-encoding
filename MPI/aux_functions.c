#include "aux_functions.h"


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

void determine_frequency(char* text, int* frequency, int *num_active, int num_alphabets, int* original_size) 
{
    int i;

    for (i = 0; i < strlen(text); i++) {
        frequency[text[i]]++;
        (*original_size)++;
    }

    for (i = 0; i < num_alphabets; i++) {
        if (frequency[i] > 0) {
            (*num_active)++;
        }
    }
}

int add_node(int index, int weight, int* leaf_index, int *parent_index, node_t* nodes, int* num_nodes) {

    int i = (*num_nodes);
    (*num_nodes)++;

    while (i > 0 && nodes[i].weight > weight) {
        memcpy(&nodes[i + 1], &nodes[i], sizeof(node_t));
        if (nodes[i].index < 0)
            leaf_index[-nodes[i].index] += 1;
        else
            parent_index[nodes[i].index] += 1;
        i--;
    }

    i++;
    nodes[i].index = index;
    nodes[i].weight = weight;
    if (index < 0)
        leaf_index[-index] = i;
    else
        parent_index[index] = i;

    return i;
}


void build_tree(int *free_index, int *parent_index, node_t* nodes, int* leaf_index, int num_nodes) {
    int a, b, index;
    while ((*free_index) < num_nodes) {
        a = (*free_index)++;
        b = (*free_index)++;
        index = add_node(b/2,
            nodes[a].weight + nodes[b].weight, leaf_index, parent_index, nodes, &num_nodes);
        parent_index[b/2] = index;
    }
}


int write_bit(FILE *f, int bit, int* bits_in_buffer, unsigned char* buffer) {
    
    return SUCCESS;
}

int flush_buffer(FILE *f, int* bits_in_buffer, unsigned char* buffer) {
    if ((*bits_in_buffer)) {
        size_t bytes_written = 
            fwrite(buffer, 1, (*bits_in_buffer + 7) >> 3, f);
        if (bytes_written < MAX_BUFFER_SIZE && ferror(f))
            return -1;
        (*bits_in_buffer) = 0;
    }

    return 0;
}

void encode_alphabet(FILE *fout, int character, int* stack_top, 
    int* stack, int* leaf_index, int num_nodes, int* parent_index,
    int* bits_in_buffer, unsigned char* buffer) {

    int node_index;
    (*stack_top) = 0;
    node_index = leaf_index[character + 1];

    while (node_index < num_nodes) {
        stack[(*stack_top)++] = node_index % 2;
        node_index = parent_index[(node_index + 1) / 2];
    }

    while (--(*stack_top) > -1) {
        if (*(bits_in_buffer) == MAX_BUFFER_SIZE << 3) {
            size_t bytes_written = fwrite(buffer, 1, MAX_BUFFER_SIZE, fout);
            if (bytes_written < MAX_BUFFER_SIZE && ferror(fout))
                return;
            (*bits_in_buffer) = 0;
            memset(buffer, 0, MAX_BUFFER_SIZE);
        }

        if (stack[*(stack_top)])
            buffer[(*bits_in_buffer) >> 3] |=
                (0x1 << (7 - (*bits_in_buffer) % 8));
        (*bits_in_buffer)++;
    }

}

int write_header(FILE *f, node_t* nodes, int num_active, int original_size) {
     int i, j, byte = 0,
         size = sizeof(unsigned int) + 1 +
              num_active * (1 + sizeof(int));
     unsigned int weight;
     printf("%d\n", size);
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
     printf("PLM<\n");
     free(buffer);
     return 0;
}