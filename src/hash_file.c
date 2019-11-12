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
  if (code != BF_OK) {      \
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
  BF_Block_Init(&first_block);        // at the first block we save information about the file
  CALL_BF(BF_AllocateBlock(file_desc, first_block));
  CALL_BF(BF_GetBlock(file_desc, block_num, first_block));
  first_block_data = BF_Block_GetData(first_block);

  memcpy(first_block_data, "hash", sizeof(5*sizeof(char)));       // the first 5 bytes have the word "hash" so we know it's a hashfile
  memcpy(first_block_data+5*sizeof(char), &buckets, sizeof(int));     // then we save the number of buckets we are using in the hashfile
  records_per_block = (BF_BLOCK_SIZE-2*sizeof(int))/sizeof(Record);       // At each block we will save 2 (int) numbers, one for the number of records at the block and one in case we want to use overflow chains
  memcpy(first_block_data+5*sizeof(char)+sizeof(int), &records_per_block, sizeof(int));  // in the end of the block we save the number of records we can save in a block

  BF_Block_SetDirty(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  // hash index blocks
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

    memcpy(block_data+(i%ints_per_block)*sizeof(int), &num, sizeof(int));    // <--- block_data points in 2nd hash block so in memcpy: block_data+(i%ints_per_block)*sizeof(int)
    BF_Block_SetDirty(block);

    if((i%ints_per_block == ints_per_block-1) && i!=buckets-1);
      CALL_BF(BF_UnpinBlock(block));
  }

  CALL_BF(BF_UnpinBlock(block));
  CALL_BF(BF_CloseFile(file_desc));

  return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){

  int file_desc;
  BF_Block *first_block;
  char *first_block_data;

  CALL_BF(BF_OpenFile(fileName, &file_desc));

  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));
  first_block_data = BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  if(strcmp(first_block_data, "hash")!=0){      // check if the file is a hashfile
    printf("This is not a hashfile\n");
    return HT_ERROR;
  }

  for (int i = 0; i < MAX_OPEN_FILES; i++){
    if (open_files[i] == -1){                   // find the first empty index in the open files table
      open_files[i] = file_desc;                // save the file_desc in the table
      *indexDesc = i;                           // return the indexDesc
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
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));      // first we take the information from the first block
  first_block_data=BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  memcpy(&buckets, first_block_data+5*sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data+9*sizeof(char), sizeof(int));        // sizeof(int)=4*sizeof(char)
  
  hashcode=record.id%buckets;
  buckets_per_block=BF_BLOCK_SIZE/sizeof(int);

  BF_Block_Init(&hash_block);
  CALL_BF(BF_GetBlock(file_desc, (hashcode/buckets_per_block)+1, hash_block));         // (hashcode/buckets_per_block)+1 the number of the block that has the bucket with this hashcode
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
  int file_desc, buckets, records_per_block, hashcode,record_id; 
  int bucket, buckets_per_block, number_of_records, next_block; // there were too many so I split them
  BF_Block *first_block, *hash_block, *block;
  char *first_block_data, *hash_block_data, *block_data, *current_record;

  file_desc = open_files[indexDesc];
  if(file_desc == -1){
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }

  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));      // first we take the information from the first block
  first_block_data = BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  memcpy(&buckets, first_block_data + 5 * sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data + 9 * sizeof(char), sizeof(int));        // sizeof(int)=4*sizeof(char)

  hashcode = *id % buckets;                           // <-- if(id !- NULL)
  buckets_per_block = BF_BLOCK_SIZE/sizeof(int);

  BF_Block_Init(&hash_block);
  CALL_BF(BF_GetBlock(file_desc, (hashcode/buckets_per_block)+1, hash_block));         // (hashcode/buckets_per_block)+1 the number of the block that has the bucket with this hashcode
  hash_block_data = BF_Block_GetData(hash_block);
  
  memcpy(&bucket, hash_block_data + (hashcode % buckets_per_block)*sizeof(int), sizeof(int));

  BF_Block_Init(&block);
  if(bucket == -1){
    printf("There is no record with id = %d\n", *id);
    return HT_ERROR;
  }
  
  BF_Block_Init(&block);
  next_block = bucket;

  char name[15];
  char surname[20];
  char city[20];
  
  while(next_block!=-1){
    // finding next block of records with the current hashcode
    CALL_BF(BF_GetBlock(file_desc, next_block, block));
    block_data = BF_Block_GetData(block);
    memcpy(&next_block, block_data, sizeof(int));
    block_data += sizeof(int);
    memcpy(&number_of_records, block_data, sizeof(int));
    block_data += sizeof(int);
    
    current_record = block_data;
    for(int i = 0; i < number_of_records; i++){
      
      memcpy(&record_id, current_record, sizeof(int));
      current_record += sizeof(int);
      memcpy(name, current_record, 15);
      current_record += 15;
      memcpy(surname, current_record, 20);
      current_record += 20;
      memcpy(city, current_record, 20);

      if(id == NULL) {
        printf("%d, %s, %s, %s\n", record_id, name, surname, city);
      }else if(record_id == *id) {
        printf("%d ,%s, %s, %s\n", record_id, name, surname, city);
      }
    }
  }
  BF_UnpinBlock(block);           // <-- left unpinned except last one?

  return HT_OK;
}

HT_ErrorCode HT_DeleteEntry(int indexDesc, int id) {
  
  int file_desc, buckets, records_per_block, bucket, hashcode, buckets_per_block, next_block, number_of_records, record_id;
  BF_Block *first_block, *hash_block, *block, *dirty_block;
  char *first_block_data, *hash_block_data, *block_data, *current_record, *record_to_delete, *last_record;

  file_desc = open_files[indexDesc];
  if(file_desc == -1) {
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }

  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));
  first_block_data = BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  memcpy(&buckets, first_block_data + 5 * sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data + 9 * sizeof(char), sizeof(int));

  hashcode = id % buckets;
  buckets_per_block = BF_BLOCK_SIZE / sizeof(int);

  BF_Block_Init(&hash_block);
  CALL_BF(BF_GetBlock(file_desc, (hashcode / buckets_per_block)  +1, hash_block));
  hash_block_data = BF_Block_GetData(hash_block);
  CALL_BF(BF_UnpinBlock(hash_block));

  memcpy(&bucket, hash_block_data + (hashcode % buckets_per_block) * sizeof(int), sizeof(int));

  if(bucket == -1){
    printf("There is no record with id = %d\n", id);
    return HT_ERROR;
  }

  BF_Block_Init(&block);
  next_block = bucket;

  do {
    // finding next block of records with the current hashcode
    CALL_BF(BF_GetBlock(file_desc, next_block, block));
    block_data = BF_Block_GetData(block);
    memcpy(&next_block, block_data, sizeof(int));
    block_data += sizeof(int);

    // searching all the records in the current block
    memcpy(&number_of_records, block_data, sizeof(int));
    block_data += sizeof(int);
    current_record = block_data;
    for(int i = 0; i < number_of_records; i++, current_record += sizeof(Record)){
      memcpy(&record_id, current_record, sizeof(int));
      // printf("record id = %d\n", record_id);
      if(record_id == id) {
        printf("Found record with id = %d\n", id);
        record_to_delete = current_record;                // saving its place to delete later
        dirty_block = block;                              // also saving block to set dirty
      }
    }
    if(next_block != -1)
      CALL_BF(BF_UnpinBlock(block));
  } while(next_block != -1);

  // finding the last record of the last block visited
  // and inserting it in the place of the one we want to delete
  current_record -= sizeof(Record);
  last_record = current_record;
  memcpy(record_to_delete, last_record, sizeof(Record));
  BF_Block_SetDirty(dirty_block);

  // changing the number_of_records in the last block so the deleted one can be overwritten
  block_data = BF_Block_GetData(block);
  memcpy(&number_of_records, block_data + sizeof(int), sizeof(int));
  // printf("number of records = %d\n", number_of_records);
  number_of_records--;
  memcpy(block_data + sizeof(int), &number_of_records, sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  // for checking

  // memcpy(&number_of_records, block_data + sizeof(int), sizeof(int));
  // printf("new number of records = %d\n", number_of_records);
  // int last_id, new_id;
  // memcpy(&last_id, last_record, sizeof(int));
  // memcpy(&new_id, record_to_delete, sizeof(int));
  // printf("last id = %d\n", last_id);
  // printf("new id = %d\n", new_id);

  return HT_OK;
}
