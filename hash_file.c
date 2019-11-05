#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hash_file.h"
#define MAX_OPEN_FILES 20

extern int *open_files;

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HT_ERROR;        \
  }                         \
}

HT_ErrorCode HT_Init() {
  
  open_files = malloc(MAX_OPEN_FILES * sizeof(int));
  for (int i = 0; i < MAX_OPEN_FILES; i++)
    open_files[i] = -1;

  return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char *filename, int buckets) {

  int file_desc, num, records_per_block, block_num = 0;
  BF_Block *first_block, *block;
  char *first_block_data, *block_data;

  CALL_BF(BF_CreateFile(filename));
  CALL_BF(BF_OpenFile(filename, &file_desc));

  // first block
  BF_Block_Init(&first_block);
  CALL_BF(BF_AllocateBlock(file_desc, first_block));
  CALL_BF(BF_GetBlock(file_desc, block_num, first_block));
  first_block_data = BF_Block_GetData(first_block);

  memcpy(first_block_data, "hash", sizeof(5*sizeof(char)));
  num = 0;
  memcpy(first_block_data+5*sizeof(char), &num, sizeof(int));
  records_per_block = (BF_BLOCK_SIZE-2*sizeof(int))/sizeof(Record);
  memcpy(first_block_data+5*sizeof(char)+sizeof(int), &records_per_block, sizeof(int));

  BF_Block_SetDirty(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  // hash index

  num = -1;
  int ints_per_block = BF_BLOCK_SIZE/sizeof(int);
  for (int i = 0; i < buckets; i++) {
    if (i % ints_per_block == 0) {
      block_num++;
      BF_Block_Init(&block);
      CALL_BF(BF_AllocateBlock(file_desc, block));
      CALL_BF(BF_GetBlock(file_desc, block_num, block));
      block_data = BF_Block_GetData(block);
      CALL_BF(BF_UnpinBlock(block));
    }

    memcpy(block_data+i*sizeof(int), &num, sizeof(int));
    BF_Block_SetDirty(block);
  }

  CALL_BF(BF_CloseFile(file_desc));

  return HT_OK;

  // int buckets_per_block = BF_BLOCK_SIZE/sizeof(int);
  // int blocks = (buckets / buckets_per_block) + 1;
  // num = -1;

  // for (int i = 1; i <= blocks; i++){
  //   BF_Block_Init(&block);
  //   CALL_BF(BF_AllocateBlock(file_desc, block));
  //   CALL_BF(BF_GetBlock(file_desc, i, block));
  //   block_data = BF_Block_GetData(block);
  //   for (int j = 0; i < buckets_per_block; i++){
        
  //   }
  // }

  // int lol;
  // memcpy(&lol, block_data, sizeof(int));
  // printf("%d\n", lol);

}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){
  
  int file_desc;
  CALL_BF(BF_OpenFile(fileName,&file_desc));
    
  for (int i = 0; i < MAX_OPEN_FILES; i++){
    if (open_files[i] == -1){
      open_files[i] = file_desc;
      *indexDesc = i;
      return HT_OK;
    }
  }
  printf("You have reached maximum open files capacity\n");
  return HT_ERROR;
}

HT_ErrorCode HT_CloseFile(int indexDesc) {
  
  int file_desc = open_files[indexDesc];
  if (file_desc == -1){
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }
  
  CALL_BF(BF_CloseFile(file_desc));
  open_files[indexDesc] = -1;
  
  return HT_OK;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {
  //insert code here
  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
  
  return HT_OK;
}

HT_ErrorCode HT_DeleteEntry(int indexDesc, int id) {
  //insert code here
  return HT_OK;
}
