#include "../header/bmem.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//single global map/list stored internally
static MemMap *map = NULL;
static MemList *list = NULL;

static MemList* init_memlist(void) {
  MemList *mlist = malloc(sizeof(MemList));
  if (!mlist) {
    printf("Error: allocating memlist\n");
    return NULL;
  }
  mlist->head = NULL;
  return mlist;
}

static MemNode* init_memnode(const void* ptr) {
  MemNode *nd = malloc(sizeof(MemNode));
  if (!nd) {
    printf("Error: allocating memnode\n");
    return NULL;
  }
  nd->ptr = ptr;
  nd->next = NULL;
  nd->prev = NULL;

  return nd;
}

static MemNode* list_insert(const void *ptr) {
  MemNode *nd = init_memnode(ptr);
  if (!nd) return NULL;
  if (!list) {
    list = init_memlist();
    if (!list) return NULL;
  }
  if (!list->head){
    list->head = nd;
  } else {
    nd->next = list->head;
    list->head->prev = nd;
    list->head = nd;
  }
  return nd;
}

static bool list_remove(const void *ptr) {
  if (!list || !list->head) return false;
  MemNode *list_nd = map_get(ptr); 
  if (!list_nd) return false;
  MemNode *prev = list_nd->prev;
  MemNode* next = list_nd->next;

  if (!prev) {
    list->head = next; 
  } else {
    prev->next = next;
  }

  if (next){
    next->prev = prev; 
  }
  free(list_nd);
  return true;
}

static void list_remove_by_node(MemNode *nd) {
  if (!nd) return;
  if (nd->prev) nd->prev->next = nd->next;
  if (nd->next) nd->next->prev = nd->prev;
  if (list && list->head == nd) list->head = nd->next;
  free(nd);
}

//memmap operations
static MemMap* init_memmap(void) {
  MemMap *mp = malloc(sizeof(MemMap));
  if (!mp){
    printf("Error: mem map allocation\n");
    return NULL;
  }
  mp->size = 0;
  mp->cap = 0;
  mp->arr = NULL;
  if (!resize_map(&mp->arr, &mp->cap)) {
    printf("Error: resizing map allocation\n");
    free(mp);
    return NULL;
  }
  return mp;
}

static bool resize_map(MapNode ***arr, size_t *cap) {
  size_t new_cap = *cap ? *cap << 1 : INIT_MAP_CAP;
  MapNode **tmp = realloc(*arr, new_cap * sizeof(MapNode*));
  if (!tmp) {
    printf("Error: reallocating map table\n");
    return false;
  }
  //zero out new entries that are uninitialized
  for (size_t i = *cap; i < new_cap; i++) {
    tmp[i] = NULL;
  }
  *arr = tmp;
  *cap = new_cap;
  return true;
}

static bool check_capacity(MemMap *mp) {
  if (mp->size > 3 * mp->cap / 4) {
    if (!resize_map(&mp->arr, &mp->cap)) {
      printf("Error: resizing map allocation\n");
      free(mp);
      return false;
    };
  }
  return true;
}

static MapNode* init_mapnode(const void *ptr, MemNode *value){
  MapNode* nd = malloc(sizeof(MapNode));
  if (!nd){
    printf("Error: allocating mapnode\n");
    return NULL;
  }
  nd->key = (uintptr_t) ptr;
  nd->value = value;
  nd->next = NULL;
  return nd;
}

static size_t hash_idx(const void *ptr, size_t table_size) {
  uintptr_t ptr_num = (uintptr_t) ptr;
  ptr_num ^= ptr_num >> 33;
  ptr_num *= 0xff51afd7ed558ccdULL;
  ptr_num ^= ptr_num >> 33;
  ptr_num *= 0xc4ceb9fe1a85ec53ULL;
  ptr_num ^= ptr_num >> 33;
  return (size_t) (ptr_num % table_size);
}

static bool map_insert(const void *ptr, MemNode *nd) {
  if (!map) {
    map = init_memmap();
    if (!map) return false;
  }
  if (!check_capacity(map)) return false;
  size_t idx = hash_idx(ptr, map->cap);
  MapNode *map_nd = map->arr[idx];
  MapNode *prev = NULL;
  while (map_nd) {
    prev = map_nd;
    map_nd = map_nd->next;
  }
  //create new node
  MapNode *new_node = init_mapnode(ptr,  nd);
  if (!new_node) {
    printf("Error: new mapnode allocation in insert\n");
    return false;
  }

  if (!prev){
    map->arr[idx] = new_node;
  } else {
    prev->next = new_node; 
  }
  map->size++;
  return true;
}

static bool map_remove(const void *ptr) {
  if (!map) return false;
  size_t idx = hash_idx(ptr, map->cap);
  MapNode *map_nd = map->arr[idx];
  MapNode* prev = NULL;
  while (map_nd && map_nd->key != (uintptr_t) ptr) {
    prev = map_nd;
    map_nd = map_nd->next;
  } 
  if (!map_nd) return false;
  if (prev) {
    prev->next = map_nd->next;
  } else {
    map->arr[idx] = map_nd->next;
  }
  free(map_nd);
  map->size--;
  return true;
}

static MemNode* map_get(const void *ptr) {
  if (!map) return NULL;
  size_t idx = hash_idx(ptr, map->cap);
  MapNode *map_nd = map->arr[idx];
  while (map_nd && map_nd->key != (uintptr_t) ptr) map_nd = map_nd->next;
  if (!map_nd) return NULL;
  return map_nd->value;
}

void* b_alloc(size_t size) {
  //step 1: allocate pointer for user
  void *ptr = malloc(size);
  if (!ptr) {
    printf("Error: Wrapped call to malloc failed\n");
    return NULL;
  }
  MemNode *list_nd = list_insert(ptr);
  if (!list_nd) return NULL;
  if (!map_insert(ptr, list_nd)){
    list_remove_by_node(list_nd);
    free(ptr);
    return NULL;
  } 
  return ptr;
}

void b_free(void *ptr) {
  // NOTE: must remove from list before map, map lookup done in list removal for O(1)
  if (!list_remove(ptr)) {
    printf("Warning: allocated memory not found\n");    
    return;
  }
  map_remove(ptr);
  free(ptr);
}

void b_cleanup(void) {
    if (map && map->arr) {
        for (size_t i = 0; i < map->cap; ++i) {
            MapNode *curr = map->arr[i];
            while (curr) {
                MapNode *next = curr->next;
                if (curr->value) {
                    free(curr->value); // frees list nodes from hashmap
                }
                free(curr);
                curr = next;
            }
        }
        free(map->arr);
        free(map);
        map = NULL;
    }

    if (list) {
        free(list);
        list = NULL;
    }
}

