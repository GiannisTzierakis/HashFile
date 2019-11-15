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
  int buckets_per_block = BF_BLOCK_SIZE/sizeof(int);
  for (int i = 0; i < buckets; i++) {       // We want to initialize every bucket with the number -1
    if (i % buckets_per_block == 0) {          // In this case we want to use the next hash_block
      block_num++;
      BF_Block_Init(&block);
      CALL_BF(BF_AllocateBlock(file_desc, block));
      CALL_BF(BF_GetBlock(file_desc, block_num, block));
      block_data = BF_Block_GetData(block);
    }

    memcpy(block_data+(i%buckets_per_block)*sizeof(int), &num, sizeof(int));       // (i%ints_per_block) is the number of buckets before the bucket we want to initialize
    BF_Block_SetDirty(block);

    if((i%buckets_per_block == buckets_per_block-1) && i!=buckets-1)               // Unpin the block if the bucket is the last one that fits in the block, except it's the last one we want to initialize
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

  // We take the information from the first block
  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));
  first_block_data = BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  if(strcmp(first_block_data, "hash")!=0){      // check if the file is a hashfile
    printf("This is not a hashfile\n");
    return HT_ERROR;
  }

  for (int i = 0; i < MAX_OPEN_FILES; i++){
    if (open_files[i] == -1){                   // find the first empty index in the open_files table
      open_files[i] = file_desc;                // save the file_desc in the table
      *indexDesc = i;                           // return the indexDesc
      return HT_OK;
    }
  }
  printf("You have reached maximum open files capacity\n");

  return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc) {

  int file_desc = open_files[indexDesc];
  if (file_desc == -1){
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }

  CALL_BF(BF_CloseFile(file_desc));
  open_files[indexDesc] = -1;                 // free this position of the table

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
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));
  first_block_data=BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  memcpy(&buckets, first_block_data+5*sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data+9*sizeof(char), sizeof(int));        // sizeof(int)=4*sizeof(char)

  hashcode=record.id%buckets;
  buckets_per_block=BF_BLOCK_SIZE/sizeof(int);

  BF_Block_Init(&hash_block);
  CALL_BF(BF_GetBlock(file_desc, (hashcode/buckets_per_block)+1, hash_block));         // (hashcode/buckets_per_block)+1 the number of the block that has the bucket with this hashcode
  hash_block_data=BF_Block_GetData(hash_block);

  memcpy(&bucket, hash_block_data+(hashcode%buckets_per_block)*sizeof(int), sizeof(int));           // (hashcode%buckets_per_block) is the number of buckets before the bucket with the hashcode

  BF_Block_Init(&block);
  if(bucket==-1){         // In this case the bucket is empty, so we insert a new block
    CALL_BF(BF_AllocateBlock(file_desc, block));
    CALL_BF(BF_GetBlockCounter(file_desc, &bucket));
    bucket--;             // bucket is now the number of the block we allocated
    CALL_BF(BF_GetBlock(file_desc, bucket, block));
    memcpy(hash_block_data+(hashcode%buckets_per_block)*sizeof(int), &bucket, sizeof(int));         // save the number of the block in the hashblock
    BF_Block_SetDirty(hash_block);

    CALL_BF(BF_GetBlock(file_desc, bucket, block));          // we initialize the information in the block
    block_data=BF_Block_GetData(block);
    next_block=-1;
    memcpy(block_data, &next_block, sizeof(int));            // initialize next_block as -1 because we haven't created an overflow chain yet
    number_of_records=0;
    memcpy(block_data+sizeof(int), &number_of_records, sizeof(int));
    BF_Block_SetDirty(block);
  }
  else{           // if the bucket isn't empty take the first block of the chain
    CALL_BF(BF_GetBlock(file_desc, bucket, block));
    block_data=BF_Block_GetData(block);
  }
  BF_UnpinBlock(hash_block);

  memcpy(&next_block, block_data, sizeof(int));
  while(next_block!=-1){          //now we want to find the last block of the chain
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, next_block, block));
    block_data=BF_Block_GetData(block);
    memcpy(&next_block, block_data, sizeof(int));
  }

  memcpy(&number_of_records, block_data+sizeof(int), sizeof(int));
  if(number_of_records==8){         //If the last block is full, add a new one in the chain
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

    BF_Block_Init(&block);        //block is now the last block
    int last_block;
    CALL_BF(BF_GetBlockCounter(file_desc, &last_block));
    last_block--;
    CALL_BF(BF_GetBlock(file_desc, last_block, block));
    block_data=BF_Block_GetData(block);
  }

  memcpy(block_data+(number_of_records*sizeof(Record))+2*sizeof(int), &record, sizeof(Record));         // save the record in the last empty position of the block
  number_of_records++;
  memcpy(block_data+sizeof(int), &number_of_records, sizeof(int));          // also we increase the number_of_records in the block by one

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
  int file_desc, buckets, records_per_block, hashcode, record_id, num_of_hashblocks, bucket, buckets_per_block, number_of_records, next_block;
  BF_Block *first_block, *hash_block, *block;
  char *first_block_data, *hash_block_data, *block_data, *current_record;
  Record record;

  // check if there is an open file with given file_desc
  file_desc = open_files[indexDesc];
  if(file_desc == -1){
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }

  BF_Block_Init(&first_block);
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));      // first we take the information from the first block in order to get records_per_block and buckets information
  first_block_data = BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));


  memcpy(&buckets, first_block_data + 5 * sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data + 9 * sizeof(char), sizeof(int));        // sizeof(int)=4*sizeof(char)

  char name[15];
  char surname[20];
  char city[20];

  buckets_per_block = BF_BLOCK_SIZE / sizeof(int); // calculate buckets_per_block

  if(id == NULL){ // id == NULL case

    num_of_hashblocks = buckets/buckets_per_block + 1; // calculate hasblocks to work in

    for (int k = 1; k <= num_of_hashblocks; k++) { // all the hashblocks

      BF_Block_Init(&hash_block);
      CALL_BF(BF_GetBlock(file_desc, k, hash_block));
      hash_block_data = BF_Block_GetData(hash_block);

      for(int i = 0 ; i < buckets_per_block ; i++) { // for every bucket in current hash_block
        if(i+buckets_per_block*(k-1) == buckets)
          break;

        memcpy(&next_block, hash_block_data + (i%buckets_per_block)*sizeof(int), sizeof(int));

        BF_Block_Init(&block);
        while(next_block != -1){ // for every block

          CALL_BF(BF_GetBlock(file_desc, next_block, block));
          block_data = BF_Block_GetData(block);

          memcpy(&next_block, block_data, sizeof(int));
          block_data += sizeof(int); // increase block data every next block
          memcpy(&number_of_records, block_data, sizeof(int));
          block_data += sizeof(int);

          current_record = block_data;
          // print every entry for every record
          for(int j = 0; j < number_of_records; j++, current_record += sizeof(Record)){
            memcpy(&record, current_record, sizeof(Record));
            printf("%d, %s, %s, %s\n", record.id, record.name, record.surname, record.city);
          }
        CALL_BF(BF_UnpinBlock(block));
        }
      }
      CALL_BF(BF_UnpinBlock(hash_block));
    }
    return HT_OK;
  }
  else {
    hashcode = *id % buckets;
    BF_Block_Init(&hash_block);
    CALL_BF(BF_GetBlock(file_desc, (hashcode/buckets_per_block)+1, hash_block));  // (hashcode/buckets_per_block)+1 the number of the block that has the bucket with this hashcode
    hash_block_data = BF_Block_GetData(hash_block);

    memcpy(&bucket, hash_block_data + (hashcode % buckets_per_block)*sizeof(int), sizeof(int));

    // checks if there is a record with given id
    if(bucket == -1){
      printf("There is no record with id = %d\n", *id);
      return HT_ERROR;
    }

    BF_Block_Init(&block);
    next_block = bucket;

    do{
      CALL_BF(BF_GetBlock(file_desc, next_block, block));
      block_data = BF_Block_GetData(block);
      memcpy(&next_block, block_data, sizeof(int));
      block_data += sizeof(int);
      memcpy(&number_of_records, block_data, sizeof(int));
      block_data += sizeof(int);

      current_record = block_data;
      for(int i = 0; i < number_of_records; i++, current_record += sizeof(Record)){
        memcpy(&record, current_record, sizeof(Record));
        // print every entry for given id
        if(record.id == *id){
          printf("%d, %s, %s, %s\n", record.id, record.name, record.surname, record.city);
          return HT_OK;
        }
      }
    BF_UnpinBlock(block);
    }
    while(next_block!=-1);
    // if HT_Ok isn't returned there is no record with wanted id
    printf("There is no record with id = %d\n", *id);
  }
  return HT_OK;
}

HT_ErrorCode HT_DeleteEntry(int indexDesc, int id) {

  int file_desc, buckets, records_per_block, bucket, hashcode, buckets_per_block, next_block, number_of_records, record_id, found_flag, current_block, delete_index, dirty_block, blocks;
  BF_Block *first_block, *hash_block, *block, *last_block;
  char *first_block_data, *hash_block_data, *block_data, *current_record, *record_to_delete, *last_record;

  file_desc = open_files[indexDesc];
  if(file_desc == -1) {
    printf("There is no open file in this index\n");
    return HT_ERROR;
  }

  BF_Block_Init(&first_block);       // first block information
  CALL_BF(BF_GetBlock(file_desc, 0, first_block));
  first_block_data = BF_Block_GetData(first_block);
  CALL_BF(BF_UnpinBlock(first_block));

  memcpy(&buckets, first_block_data + 5 * sizeof(char), sizeof(int));
  memcpy(&records_per_block, first_block_data + 9 * sizeof(char), sizeof(int));

  // calculating the hashcode of the current id
  hashcode = id % buckets;
  buckets_per_block = BF_BLOCK_SIZE / sizeof(int);

  BF_Block_Init(&hash_block);
  CALL_BF(BF_GetBlock(file_desc, (hashcode / buckets_per_block)  +1, hash_block));
  hash_block_data = BF_Block_GetData(hash_block);
  CALL_BF(BF_UnpinBlock(hash_block));

  memcpy(&bucket, hash_block_data + (hashcode % buckets_per_block) * sizeof(int), sizeof(int));

  // if bucket is empty
  if(bucket == -1){
    printf("There is no record with id = %d\n", id);
    return HT_ERROR;
  }

  BF_Block_Init(&block);
  next_block = bucket;
  found_flag = 0;

  // iterating all the block of the overflow chain
  do {
    // finding  the next block of records with the current hashcode
    CALL_BF(BF_GetBlock(file_desc, next_block, block));
    block_data = BF_Block_GetData(block);
    current_block = next_block;                   // saving the current block
    memcpy(&next_block, block_data, sizeof(int));
    block_data += sizeof(int);

    // searching all the records in the current block
    memcpy(&number_of_records, block_data, sizeof(int));
    block_data += sizeof(int);
    current_record = block_data;
    for(int i = 0; i < number_of_records; i++, current_record += sizeof(Record)){
      memcpy(&record_id, current_record, sizeof(int));
      if(record_id == id) {
        delete_index = i;                // saving the wanted record's place to modify later
        dirty_block = current_block;     // also saving the current block to set dirty
        found_flag = 1;                  // id was found so no need to check the remaining records
        break;
      }
    }
    CALL_BF(BF_UnpinBlock(block));
  } while(next_block != -1 || found_flag == 0);     // exit the loop when last block was reached or record was found

  // if the record didn't exist
  if(!found_flag){
    printf("There is no record with id = %d\n", id);
    return HT_ERROR;
  }

  // finding the last record of the last block
  CALL_BF(BF_GetBlockCounter(file_desc, &blocks));
  BF_Block_Init(&last_block);
  CALL_BF(BF_GetBlock(file_desc, blocks-1, last_block));
  block_data = BF_Block_GetData(last_block);
  block_data += sizeof(int);
  memcpy(&number_of_records, block_data, sizeof(int));
  last_record = block_data += sizeof(int) + (number_of_records - 1) * sizeof(Record);

  // copying the contents of the last record into the record set for deletion and setting the block to dirty
  CALL_BF(BF_GetBlock(file_desc, dirty_block, block));
  record_to_delete = BF_Block_GetData(block);
  record_to_delete += (2 * sizeof(int)) + (delete_index * sizeof(Record));
  memcpy(record_to_delete, last_record, sizeof(Record));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  // changing the number of records in the last block so the last record can be overwritten
  block_data = BF_Block_GetData(last_block);
  memcpy(&number_of_records, block_data + sizeof(int), sizeof(int));
  number_of_records--;
  memcpy(block_data + sizeof(int), &number_of_records, sizeof(int));
  last_record = block_data + (2 * sizeof(int)) + (number_of_records) * sizeof(Record);
  memset(last_record, -1, sizeof(int));               // putting (-1) in the place of the deleted record
  BF_Block_SetDirty(last_block);
  CALL_BF(BF_UnpinBlock(last_block));

  return HT_OK;
}
