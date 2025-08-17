#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef B_MEM

#define INIT_MAP_CAP 128

typedef struct _MemNode {
  const void *ptr;
  struct _MemNode *prev;
  struct _MemNode *next;
  //const char *scope; //TODO: implement named scopes
} MemNode;

typedef struct {
  MemNode *head;
} MemList;

typedef struct _MapNode {
  uintptr_t key;
  MemNode *value;
  struct _MapNode *next;
} MapNode;

typedef struct {
  MapNode **arr;
  size_t size;
  size_t cap;
} MemMap;


//memlist operations
static MemList* init_memlist(void);
static MemNode* init_memnode(const void *ptr);
static MemNode* list_insert(const void *ptr);
static bool list_remove(const void *ptr); //uses map to lookup
static void list_remove_by_node(MemNode *nd);

//memmap operations
static MemMap* init_memmap(void);
static MapNode* init_mapnode(const void *ptr, MemNode *value);
static uintptr_t hash_idx(const void *, size_t);
static bool resize_map(MapNode ***arr, size_t *cap);
static bool check_capacity(MemMap *mp);
static bool map_insert(const void *ptr, MemNode *nd);
static bool map_remove(const void *ptr);
static MemNode* map_get(const void *ptr);

//public funtions
void* b_alloc(size_t);
void b_free(void *);
void b_cleanup(void);

#endif
