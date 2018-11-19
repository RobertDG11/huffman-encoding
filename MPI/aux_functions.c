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