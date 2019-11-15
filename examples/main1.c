#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 1700 // you can change it if you want
#define BUCKETS_NUM 13  // you can change it if you want
#define FILE_NAME "dataA.db"

int *open_files;

const char* names[] = {
  "Yannis",
  "Christofos",
  "Sofia",
  "Marianna",
  "Vagelis",
  "Maria",
  "Iosif",
  "Dionisis",
  "Konstantina",
  "Theofilos",
  "Giorgos",
  "Dimitris"
};

const char* surnames[] = {
  "Ioannidis",
  "Svingos",
  "Karvounari",
  "Rezkalla",
  "Nikolopoulos",
  "Berreta",
  "Koronis",
  "Gaitanis",
  "Oikonomou",
  "Mailis",
  "Michas",
  "Halatsis"
};

const char* cities[] = {
  "Athens",
  "San Francisco",
  "Los Angeles",
  "Amsterdam",
  "London",
  "New York",
  "Tokyo",
  "Hong Kong",
  "Munich",
  "Miami"
};

#define CALL_OR_DIE(call)     \
  {                           \
    HT_ErrorCode code = call; \
    if (code != HT_OK) {      \
      printf("Error\n");      \
      exit(code);             \
    }                         \
  }

int main(void){

  int indexDesc, i;

  BF_Init(LRU);
  CALL_OR_DIE(HT_Init());

  char* filename = strdup(FILE_NAME);

  // we try to create and open 21 files
  for (i = 0; i < 21; i++, filename[4]++) { // we increment the 5th character
    CALL_OR_DIE(HT_CreateIndex(filename,BUCKETS_NUM));
    CALL_OR_DIE(HT_OpenIndex(filename, &indexDesc)); // at the 21th file we should get an error message
    printf("%s\n", filename);
  }
  filename[4]--;

  for(i=0 ; i < 20 ; i++)
    printf("%d ", open_files[i]);
  printf("\n");

  CALL_OR_DIE(HT_CloseFile(5)); // we close the 6th file of the openfiles table

  for(i=0 ; i < 20 ; i++)
    printf("%d ", open_files[i]);
  printf("\n");

  CALL_OR_DIE(HT_OpenIndex(filename, &indexDesc)); // we try to open the 21st file that was created

  for(i=0 ; i < 20 ; i++)
    printf("%d ", open_files[i]);
  printf("\n");

  for(i=0 ; i < 20 ; i++)   //close all files
    CALL_OR_DIE(HT_CloseFile(i));

  for(i=0 ; i < 20 ; i++)
    printf("%d ", open_files[i]);
  printf("\n");

  return 0;
}
