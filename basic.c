#pragma once

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
// #include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __linux__
#include <sys/mman.h>
#endif

#define false 0
#define true 1

#define memzero(x) memset((x), 0, sizeof *(x))

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) ((Value)*1024LL*1024LL)
#define Gigabytes(Value) ((Value)*1024LL*1024LL*1024LL)
#define Terabytes(Value) ((Value)*1024LL*1024LL*1024LL*1024LL)

#define CEIL_DIV(x, d) ((x) % (d) == 0 ? (x) / (d) : ((x) / (d)) + 1)
#define ARRAY_COUNT(x) ((sizeof (x)) / (sizeof (*(x))))

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ba_assert(condition) do { if (!(condition)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #condition); abort(); } } while (0)
#define ba_static_assert(value, msg) typedef char __static_assert_##msg[(value) ? 1 : -1]

typedef uint8_t bool8_t;

// Bytes of zero-initalized data if necessary for ZAII

#define ZEROES_BUFFER_SIZE Kilobytes(128)
uint8_t zeroes_buffer[ZEROES_BUFFER_SIZE];

// Pool allocator implementation

typedef struct {
 uint8_t *data;
 uint64_t data_size;

 uint64_t offset;
 uint32_t alignment;
} Pool;

static void pool_init(Pool *pool, uint8_t *data, uint64_t data_size) {
 pool->data = data;
 pool->data_size = data_size;
 pool->offset = 0;

 pool->alignment = sizeof (void*); // Could be changed to something else
}

static void *pool_alloc(Pool *pool, uint64_t size) {
 if (size == 0) return 0;

 uintptr_t curr_ptr = (uintptr_t)(pool->data + pool->offset);

 if (curr_ptr % pool->alignment != 0) {
  curr_ptr += pool->alignment - (curr_ptr % pool->alignment);
 }

 pool->offset = (uint8_t*)(curr_ptr) - pool->data;

 if (pool->offset + size >= pool->data_size) return 0;

 uint8_t *res = pool->data + pool->offset;
 pool->offset += size;

 return res;
}

static void *pool_alloc_zero(Pool *pool, uint64_t size) {
 void *mem = pool_alloc(pool, size);
 if (mem) {
  memset(mem, 0, size);
 }

 return mem;
}

#define pool_alloc_type(pool, type) pool_alloc((pool), sizeof (type))
#define pool_alloc_array(pool, type, count) pool_alloc((pool), sizeof (type) * (count))

#define pool_alloc_type_zero(pool, type) pool_alloc_zero((pool), sizeof (type))
#define pool_alloc_array_zero(pool, type, count) pool_alloc_zero((pool), sizeof (type) * (count))


static void pool_clear(Pool *pool) {
 pool->offset = 0;
}

static uint64_t pool_checkpoint(Pool *pool) {
 return pool->offset;
}

static void pool_set_checkpoint(Pool *pool, uint64_t checkpoint) {
 pool->offset = checkpoint;
}

static Pool pool_subpool(Pool *pool, uint64_t size) {
 Pool subpool = {0};

 if (pool->offset + size < pool->data_size) {
  subpool.data = pool->data + pool->offset;
  subpool.data_size = size;
  subpool.offset = 0;
  subpool.alignment = pool->alignment;

  pool->offset += size;
 }

 return subpool;
}

static void *pool_buffer_copy(Pool *pool, void *buffer, uint64_t size) {
 void *new_buffer = pool_alloc(pool, size);

 if (new_buffer != 0) {
  memcpy(new_buffer, buffer, size);
 }

 return new_buffer;
}

#define pool_temp_begin(pool) uint64_t _pool_checkpoint = pool_checkpoint(pool)
#define pool_temp_end(pool) pool_set_checkpoint(pool, _pool_checkpoint)

// Strings implementation

typedef struct {
 char *data;
 uint64_t count;
} String;

#define STR_L(x) ((String){ x, (sizeof x) - 1 })
#define STR(x) ((String){ x, strlen(x) })

static bool8_t string_to_int(String string, int64_t *out_int) {
 *out_int = 0;
 uint64_t base = 1;
 int8_t sign = 1;

 bool8_t success = false;

 if (string.count > 1 && string.data[0] == '-') {
  sign = -1;
  string.data++;
  string.count--;
 }

 if (string.count <= 18) {              /* No overflow */
  success = true;

  for (int64_t i = string.count - 1; i >= 0; i--) {
   if (string.data[i] < '0' || string.data[i] > '9') {
    success = false;
    break;
   }

   *out_int += base * (string.data[i] - '0');
   base *= 10;
  }

  *out_int *= sign;
 }

 return success;
}

static String *string_split(Pool *pool, String string, char delimeter, uint32_t *out_count) {
 uint32_t delimeters_count = 0;
 uint64_t i = 0;

 for (; i < string.count && string.data[i] == delimeter; i++);

 for (; i < string.count; i++) {
  if (string.data[i] == delimeter) {
   delimeters_count++;
  }

  while (i < string.count && string.data[i] == delimeter) {
   i++;
  }
 }

 *out_count = delimeters_count + 1;
 delimeters_count = 0;

 String *spliced = pool_alloc(pool, sizeof (String) * (*out_count));
 if (spliced == 0) {
  *out_count = 0;
  return 0;
 }

 for (i = 0; i < string.count && string.data[i] == delimeter; i++);
 spliced[0].data = string.data + i;

 uint64_t last_i = i;

 for (; i < string.count; i++) {
  if (string.data[i] == delimeter) {
   spliced[delimeters_count].count = i - last_i;

   while (i < string.count && string.data[i] == delimeter) {
    i++;
   }

   last_i = i;
   spliced[delimeters_count + 1].data = string.data + i;
   delimeters_count++;
  }

 }

 spliced[delimeters_count].count = string.count - last_i;

 ba_assert(delimeters_count + 1 == *out_count);

 return spliced;
}

static bool8_t string_equal(String s1, String s2) {
 if (s1.count != s2.count)
  return false;

 for (uint64_t i = 0; i < s1.count; i++) {
  if (s1.data[i] != s2.data[i])
   return false;
 }

 return true;
}

static String string_copy(Pool *pool, String str) {
 String new_string;
 new_string.count = str.count;
 new_string.data = pool_alloc(pool, str.count);
 if (new_string.data == 0) return (String){0};

 memcpy(new_string.data, str.data, str.count);
 return new_string;
}

static char *string_to_cstr(Pool *pool, String str) {
 char *cstr = pool_alloc(pool, str.count + 1);
 if (cstr == 0) return 0;

 memcpy(cstr, str.data, str.count);
 cstr[str.count] = '\0';

 return cstr;
}

static String string_concat(Pool *pool, String str1, String str2) {
 String new_string = {0};
 new_string.data = pool_alloc(pool, str1.count + str2.count);
 if (new_string.data == 0) return new_string;

 new_string.count = str1.count + str2.count;

 memcpy(new_string.data, str1.data, str1.count);
 memcpy(new_string.data + str1.count, str2.data, str2.count);

 return new_string;
}

static String string_sub(String string, int64_t start, int64_t end) {
 String sub = {};

 if (start < 0)
  start = string.count + start + 1;
 if (end < 0)
  end = string.count + end + 1;

 // Error checking
 if (start < 0 || end < 0 || start > end ||
     (uint64_t)start >= string.count || (uint64_t)end > string.count) {
  return sub;
 }

 sub.data = string.data + start;
 sub.count = end - start;

 return sub;
}

static void string_fprint(FILE *out, String string) {
 fprintf(out, "%.*s", (int)string.count, string.data);
}

#define STR_FMT_LIT "%.*s"
#define STR_FMT(s) ((int)(s).count), (s).data

/*
  Simple library for data serialization. Only supports machines that are
  little endian and use two's complement representation for their signed
  integers.

  - TODO: Floating point serialization.
*/

#pragma once
#include <inttypes.h>
#include <string.h>

static uint8_t *se_buffer;
static uint64_t se_buffer_size;
static uint64_t se_offset;

static void se_begin(uint8_t *buffer, uint64_t size) {
 se_buffer = buffer;
 se_buffer_size = size;

 se_offset = 0;
}

static bool8_t se_end(uint64_t *out_size) {
 if (se_offset >= se_buffer_size) {
  // ba_assert(false);
  return false;
 }

 if (out_size) *out_size = se_offset;

 se_buffer = 0;
 se_buffer_size = 0;
 se_offset = 0;

 return true;
}

static void se_write_bytes(uint8_t *bytes, uint64_t count) {
 uint8_t *dst = se_buffer + se_offset;
 se_offset += count;

 if (se_offset < se_buffer_size) memcpy(dst, bytes, count);
}

static void se_write_string(String str) {
 uint8_t *dst = se_buffer + se_offset;
 se_offset += 8 + str.count;
 if (se_offset < se_buffer_size) {
  memcpy(dst, &str.count, 8);
  memcpy(dst + 8, str.data, str.count);
 }
}

static void se_write_u8(uint8_t x) {
 uint8_t *dst = se_buffer + se_offset;
 se_offset++;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_u16(uint16_t x) {
 uint16_t *dst = (uint16_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_u32(uint32_t x) {
 uint32_t *dst = (uint32_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_u64(uint64_t x) {
 uint64_t *dst = (uint64_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_s8(int8_t x) {
 int8_t *dst = (int8_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_s16(int16_t x) {
 int16_t *dst = (int16_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_s32(int32_t x) {
 int32_t *dst = (int32_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static void se_write_s64(int64_t x) {
 int64_t *dst = (int64_t*)(se_buffer + se_offset);
 se_offset += sizeof x;

 if (se_offset < se_buffer_size) *dst = x;
}

static bool8_t se_read_bytes(uint8_t *out_bytes, uint64_t size) {
 if (se_offset + size >= se_buffer_size) return false;

 memcpy(out_bytes, se_buffer + se_offset, size);
 se_offset += size;

 return true;
}

static String se_read_string(Pool *pool) {
 String res = {};

 uint8_t *data = se_buffer + se_offset;
 if (se_offset + 8 < se_buffer_size) {
  uint64_t count = *(uint64_t*)data;
  if (se_offset + 8 + count < se_buffer_size) {
   res.count = count;
   res.data = pool_alloc(pool, count);

   memcpy(res.data, data + 8, count);

   se_offset += 8 + count;
  }
 }

 return res;
}

static bool8_t se_read_u8(uint8_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(uint8_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_u16(uint16_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(uint16_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_u32(uint32_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(uint32_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_u64(uint64_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(uint64_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_s8(int8_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(int8_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_s16(int16_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(int16_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_s32(int32_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(int32_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

static bool8_t se_read_s64(int64_t *out) {
 if (se_offset + sizeof (*out) >= se_buffer_size) return false;

 *out = *(int64_t*)(se_buffer + se_offset);
 se_offset += sizeof (*out);

 return true;
}

// Hash table

struct hash_table_slot {
 String key;
 void *data;
 struct hash_table_slot *next;
};

typedef struct {
 Pool *pool;
 uint64_t size;
 struct hash_table_slot **slots;
} HashTable;

static void hash_table_init(HashTable *table, Pool *pool, uint64_t size) {
 table->pool = pool;
 table->size = size;

 table->slots = pool_alloc_array_zero(pool, struct hash_table_slot *, size);
}

static uint64_t hash_table_hash(String key) {
 uint64_t res = 0;

 for (uint64_t i = 0; i < key.count; i++){
  res = key.data[i] + (res << 6) + (res << 16) - res;
 }

 return res;
}

static void hash_table_set(HashTable *table, String key, void *value) {
 uint64_t hash = hash_table_hash(key) % table->size;

 struct hash_table_slot *slot = table->slots[hash];
 if (slot == 0) {
  table->slots[hash] = pool_alloc_type_zero(table->pool, struct hash_table_slot);
  table->slots[hash]->key = string_copy(table->pool, key);
  table->slots[hash]->data = value;
  table->slots[hash]->next = 0;
 } else {
  struct hash_table_slot *last_slot = 0;
  for (; slot != 0; slot = slot->next) {
   if (string_equal(slot->key, key)) {
    break;
   }

   last_slot = slot;
  }

  if (slot == 0) {
   last_slot->next = pool_alloc_type_zero(table->pool, struct hash_table_slot);
   last_slot->next->key = string_copy(table->pool, key);
   last_slot->next->data = value;
   last_slot->next->next = 0;
  } else {
   slot->data = value;
  }
 }
}

static void *hash_table_get(HashTable *table, String key) {
 uint64_t hash = hash_table_hash(key) % table->size;

 struct hash_table_slot *slot = table->slots[hash];

 for (; slot != 0; slot = slot->next) {
  if (string_equal(slot->key, key)) {
   break;
  }
 }

 if (slot == 0) {
  return 0;
 } else {
  return slot->data;
 }
}

// Misc

static uint16_t le_to_be_u16(uint64_t x) {
 return ((uint16_t)(((uint8_t*)&x)[0]) << 8) | (uint16_t)(((uint8_t*)&x)[1]);
}

static bool8_t parse_ipv4(String str, uint8_t *ip) {
 uint8_t buffer[1024];
 Pool pool;
 pool_init(&pool, buffer, sizeof buffer);

 uint32_t numbers_count;
 String *numbers = string_split(&pool, str, '.', &numbers_count);

 if (numbers == 0 || numbers_count != 4) return false;

 for (uint32_t i = 0; i < 4; i++) {
  int64_t x;
  if (!string_to_int(numbers[i], &x)) return false;

  if (x < 0 || x > 255) return false;

  ip[i] = (uint8_t)x;
 }

 return true;
}

#ifdef __linux__

static void *allocate_pages(uint64_t size) {
 void *ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

 if (ptr == MAP_FAILED) {
  return 0;
 }

 return ptr;
}

static bool8_t deallocate_pages(void *mem, uint64_t size) {
 return munmap(mem, size) == 0;
}

#endif

#define HEXDUMP_LINE_LEN 15

static void hexdump(void *data, uint64_t size) {
 uint8_t *readable_data = data;
 uint32_t lines_count = CEIL_DIV(size, HEXDUMP_LINE_LEN);

 for (uint32_t i = 0; i < lines_count; i++) {
  uint32_t len = (i == lines_count - 1) ? (size % HEXDUMP_LINE_LEN) : HEXDUMP_LINE_LEN;

  for (uint32_t j = 0; j < len; j++) {
   printf("%02x ", readable_data[i * HEXDUMP_LINE_LEN + j]);
  }

  printf("| ");

  for (uint32_t j = 0; j < len; j++) {
   char c = readable_data[i * HEXDUMP_LINE_LEN + j];
   printf("%c ", (c >= 32 && c <= 126) ? c : '.');
  }

  printf("\n");
 }
}

static char *tprintf(Pool *pool, const char *format, ...) {
 va_list args;

 va_start(args, format);
 int64_t size = vsnprintf(0, 0, format, args);
 va_end(args);

 char *buffer = pool_alloc(pool, size + 1);

 va_start(args, format);
 vsprintf(buffer, format, args);
 va_end(args);

 return buffer;
}

static uint64_t xorshift_state = 0xdeadbeefdeadbeef;

static void xorshift_seed(uint64_t seed) {
 xorshift_state = seed;
}

static uint64_t xorshift_rand() {
 uint64_t x = xorshift_state;
 x ^= x << 13;
 x ^= x >> 7;
 x ^= x << 17;
 xorshift_state = x;

 return x;
}

// sorting

void sort_raw(uint32_t *index_array, uint32_t index_array_count, void *array, uint32_t array_element_size, uint32_t array_element_offset, bool8_t increasing) {
 for (uint32_t i = 0; i < index_array_count; i++) {
  uint32_t max_i = i;
                
  for (uint32_t j = i + 1; j < index_array_count; j++) {
   uint32_t j_index = index_array[j];
   int32_t j_value = *(int32_t*)((uint8_t*)array + array_element_size * j_index + array_element_offset);

   uint32_t max_i_index = index_array[max_i];
   int32_t max_i_value = *(int32_t*)((uint8_t*)array + array_element_size * max_i_index + array_element_offset);
                        
   if ((!increasing && j_value > max_i_value) ||
       (increasing && j_value < max_i_value))
   {
    max_i = j;
   }
  }

  uint32_t tmp = index_array[i];
  index_array[i] = index_array[max_i];
  index_array[max_i] = tmp;
 }
}

#define SORT(index_array, index_array_count, array, member, increasing) \
 sort_raw(index_array, index_array_count, (void*)(array), sizeof (array)[0], (uint32_t)((uint64_t)&(((array)[0]).member) - (uint64_t)&((array)[0])), increasing)
