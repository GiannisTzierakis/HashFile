#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 15000 // you can change it if you want
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

  int indexDesc;
  Record record;

  BF_Init(LRU);
  CALL_OR_DIE(HT_Init());

  char* filename = strdup(FILE_NAME);

  for (int i = 0; i < 5; i++, filename[4]++) {        //Create and Open 5 new files
    CALL_OR_DIE(HT_CreateIndex(filename,BUCKETS_NUM));
    CALL_OR_DIE(HT_OpenIndex(filename, &indexDesc));
    printf("%s\n", filename);
  }

  for (int l = 0; l < 5; l++) {       //Insert "RECORDS_NUM" record in every file that we opered
    srand(l*5652);
    int r;
    printf("Insert Entries\n");
    for (int id = 0; id < RECORDS_NUM; ++id) {
      record.id = id;
      r = rand() % 12;
      memcpy(record.name, names[r], strlen(names[r]) + 1);
      r = rand() % 12;
      memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
      r = rand() % 10;
      memcpy(record.city, cities[r], strlen(cities[r]) + 1);

      CALL_OR_DIE(HT_InsertEntry(l, record));
    }
  }
  printf("RUN PrintAllEntries\n");        //Print the entries of each file
  for (int j = 0; j < 5; j++) {
    printf("*******************************************************************************************************\n");
    CALL_OR_DIE(HT_PrintAllEntries(j, NULL));
  }

  int id=468;
  printf("Printf the record with id=%d in the third file: ", id);
  CALL_OR_DIE(HT_PrintAllEntries(2, &id));
  CALL_OR_DIE(HT_DeleteEntry(2, id));
  CALL_OR_DIE(HT_PrintAllEntries(2, &id));        //After we delete the entry we should get an error message

  for(int i=0 ; i < 5; i++)       //Close the files
    CALL_OR_DIE(HT_CloseFile(i));

  return 0;
}
