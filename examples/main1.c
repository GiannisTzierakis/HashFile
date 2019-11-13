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

  Record record1;
  record1.id=10;
  memcpy(record1.name, "Giorgos", sizeof(record1.name));
  memcpy(record1.surname, "Michas", sizeof(record1.surname));
  memcpy(record1.city, "Athens", sizeof(record1.city));

  Record record2;
  record2.id=23;      // 23 % 13 = 10
  memcpy(record2.name, "Kostas", sizeof(record2.name));
  memcpy(record2.surname, "Skordakis", sizeof(record2.surname));
  memcpy(record2.city, "Larisa", sizeof(record2.city));

  int num=10;

  CALL_OR_DIE(HT_InsertEntry(0, record1));
  CALL_OR_DIE(HT_InsertEntry(0, record2));
<<<<<<< HEAD
  CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL));
//  CALL_OR_DIE(HT_DeleteEntry(0, record1.id));
=======

  // int *NULL_ID = NULL;
  // CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL_ID));

  CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &record1.id));
  CALL_OR_DIE(HT_DeleteEntry(0, record1.id));
>>>>>>> d366600b7929c93a7bb339a12d7825c95523985f
  CALL_OR_DIE(HT_CloseFile(indexDesc));


  return 0;
}
