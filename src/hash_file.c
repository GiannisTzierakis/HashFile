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
  BF_Block_Init(&first_block);        //at the first block we save imformations about the file
  CALL_BF(BF_AllocateBlock(file_desc, first_block));
  CALL_BF(BF_GetBlock(file_desc, block_num, first_block));
  first_block_data = BF_Block_GetData(first_block);

  memcpy(first_block_data, "hash", sizeof(5*sizeof(char)));       //the first 5 bites have the word "hash" so we know it's a hashfile
  num = buckets;
  memcpy(first_block_data+5*sizeof(char), &num, sizeof(int));     //then we save the number of buckets we are using in the hashfile
  records_per_block = (BF_BLOCK_SIZE-2*sizeof(int))/sizeof(Record);       //At each block we will save 2 (int) numbers, one for the number of records at the block and one in case we want to use overflow chains
  memcpy(first_block_data+5*sizeof(char)+sizeof(int), &records_per_block, sizeof(int));  //in the end of the block we save the number of records we can save in a block

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
//      CALL_BF(BF_UnpinBlock(block))
    }

    memcpy(block_data+i*sizeof(int), &num, sizeof(int));
    BF_Block_SetDirty(block);

    if(i%ints_per_block == ints_per_block-1 && i!=buckets-1);
      CALL_BF(BF_UnpinBlock(block));
  }
  CALL_BF(BF_UnpinBlock(block));

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
  BF_Block *first_block;
  char *first_block_data;

  CALL_BF(BF_OpenFile(fileName, &file_desc));

  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));
  first_block_data=BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  if(strcmp(first_block_data, "hash")!=0){      //check if the file is a hashfile
    printf("This is not a hashfile\n");
    return HT_ERROR;
  }

  for (int i = 0; i < MAX_OPEN_FILES; i++){
    if (open_files[i] == -1){                   //find the first empty index in the files table
      open_files[i] = file_desc;                //save the file_desc in the table
      *indexDesc = i;                           //return the index
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

  int file_desc, buckets, records_per_block, hashcode, bucket, buckets_per_block, number_of_records, next_block;
  BF_Block *first_block, *hash_block, *block;
  char *first_block_data, *hash_block_data, *block_data;

  file_desc=open_files[indexDesc];
  if(file_desc==-1){
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }

  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));      //first we take the imformations from the first block
  first_block_data=BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  memcpy(&buckets, first_block_data+5*sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data+9*sizeof(char), sizeof(int));        //sizeof(int)=4*sizeof(char)

  hashcode=record.id%buckets;
  buckets_per_block=BF_BLOCK_SIZE/sizeof(int);

  BF_Block_Init(&hash_block);
  CALL_BF(BF_GetBlock(file_desc, (hashcode/buckets_per_block)+1, hash_block));         //(hashcode/buckets_per_block)+1 the number of the block that has the bucket with this hashcode
  hash_block_data=BF_Block_GetData(hash_block);

  memcpy(&bucket, hash_block_data+(hashcode%buckets_per_block)*sizeof(int), sizeof(int));

  BF_Block_Init(&block);
  if(bucket==-1){
    CALL_BF(BF_AllocateBlock(file_desc, block));
    CALL_BF(BF_GetBlockCounter(file_desc, &bucket));
    bucket--;
    CALL_BF(BF_GetBlock(file_desc, bucket, block));
    memcpy(hash_block_data+(hashcode%buckets_per_block)*sizeof(int), &bucket, sizeof(int));
    BF_Block_SetDirty(hash_block);

    CALL_BF(BF_GetBlock(file_desc, bucket, block));
    block_data=BF_Block_GetData(block);
    next_block=-1;
    memcpy(block_data, &next_block, sizeof(int));
    number_of_records=0;
    memcpy(block_data+sizeof(int), &number_of_records, sizeof(int));
    BF_Block_SetDirty(block);
  }
  else{
    CALL_BF(BF_GetBlock(file_desc, bucket, block));
    block_data=BF_Block_GetData(block);
  }
  BF_UnpinBlock(hash_block);

  memcpy(&next_block, block_data, sizeof(int));
  while(next_block!=-1){
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, next_block, block));
    block_data=BF_Block_GetData(block);
    memcpy(&next_block, block_data, sizeof(int));
  }

  memcpy(&number_of_records, block_data+sizeof(int), sizeof(int));
  if(number_of_records==8){
    BF_Block *new_block;
    char *new_block_data;

    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));

    CALL_BF(BF_GetBlockCounter(file_desc, &next_block));
    next_block--;
    memcpy(block_data, &next_block, sizeof(int));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));

    CALL_BF(BF_GetBlock(file_desc, next_block, new_block));
    new_block_data=BF_Block_GetData(new_block);

    next_block=-1;
    memcpy(new_block_data, &next_block, sizeof(int));
    number_of_records=0;
    memcpy(new_block_data+sizeof(int), &number_of_records, sizeof(int));

    BF_Block_SetDirty(new_block);
    CALL_BF(BF_UnpinBlock(new_block));

    BF_Block_Init(&block);
    int last_block;
    CALL_BF(BF_GetBlockCounter(file_desc, &last_block));
    last_block--;
    CALL_BF(BF_GetBlock(file_desc, last_block, block));
    block_data=BF_Block_GetData(block);
  }

  memcpy(block_data+number_of_records*sizeof(Record)+2*sizeof(int), &record, sizeof(Record));
  number_of_records++;
  memcpy(block_data+sizeof(int), &number_of_records, sizeof(int));

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {

  return HT_OK;
}

HT_ErrorCode HT_DeleteEntry(int indexDesc, int id) {
  //insert code here
  return HT_OK;
}
