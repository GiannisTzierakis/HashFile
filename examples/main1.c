#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 1700 // you can change it if you want
#define BUCKETS_NUM 13  // you can change it if you want
#define FILE_NAME "data.db"

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

  BF_Init(LRU);
  CALL_OR_DIE(HT_Init());

  CALL_OR_DIE(HT_CreateIndex(FILE_NAME,BUCKETS_NUM));
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc));

  Record record;
  record.id=10;
  memcpy(record.name, "Giorgos", sizeof(record.name));
  memcpy(record.surname, "Michas", sizeof(record.surname));
  memcpy(record.city, "Athens", sizeof(record.city));

  CALL_OR_DIE(HT_InsertEntry(0, record));
  CALL_OR_DIE(HT_DeleteEntry(0, 0));
  CALL_OR_DIE(HT_CloseFile(indexDesc));

  return 0;
}
