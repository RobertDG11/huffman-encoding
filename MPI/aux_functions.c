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

void determine_frequency(char* text, int* frequency, int *num_active, int num_alphabets) 
{
    int i;

    for (i = 0; i < strlen(text); i++) {
        frequency[text[i]]++;
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
    printf("i= %d\n", i);
    nodes[i].index = index;
    nodes[i].weight = weight;
    if (index < 0)
        leaf_index[-index] = i;
    else
        parent_index[index] = i;

    return i;
}

void add_leaves(int* frequency, int num_alphabets, int* leaf_index, int *parent_index, node_t* nodes, int* num_nodes) {
    int i, freq;
    for (i = 0; i < num_alphabets; ++i) {
        freq = frequency[i];
        if (freq > 0) 
            add_node(-(i + 1), freq, leaf_index, parent_index, nodes, num_nodes);
            //printf("Freq > 0\n");
    }
}

void build_tree(int *free_index, int *parent_index, node_t* nodes, int* leaf_index, int *num_nodes) {
    int a, b, index;
    while (free_index < num_nodes) {
        a = (*free_index)++;
        b = (*free_index)++;
        index = add_node(b/2,
            nodes[a].weight + nodes[b].weight, leaf_index, parent_index, nodes, num_nodes);
        parent_index[b/2] = index;
    }
}