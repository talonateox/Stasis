#include <stdbool.h>
#include <stdint.h>

#include "../vfs/vfs.h"

typedef enum {
    PARTITION_TYPE_MBR,
    PARTITION_TYPE_GPT,
} partition_table_type_t;

typedef struct {
    int index;
    uint8_t type;
    char type_name[32];
    uint32_t lba_start;
    uint32_t num_sectors;
    bool bootable;
} partition_info_t;

#define MAX_PARTITIONS 16

typedef struct {
    char device_path[256];
    partition_table_type_t type;
    int num_partitions;
    partition_info_t partitions[MAX_PARTITIONS];
} partition_table_t;

partition_table_t* partition_parse_mbr(const char* device_path);

void partition_free(partition_table_t* table);

partition_info_t* partition_get(partition_table_t* table, int index);
void partition_register(partition_table_t* table);