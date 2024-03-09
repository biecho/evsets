#ifndef evsets_api_H
#define evsets_api_H

#include "public_structs.h"

int init_evsets(struct config *conf);
int find_evsets();
int get_num_evsets();
cache_block_t *get_evset(int id);
void close_evsets();

#endif /* evsets_api_H */
