/*
 * use bucket for memory pool, run LRU on each bucket.
 * don't cache file if size > 256K, because the number is small.
 *
 * 1K -> 2K -> 4K -> 8K -> 16K -> 32K -> 64K ->128K -> 256K
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>

using namespace std;

#define MIN_NUM   1024        // 1K
#define MAX_NUM   262144      // 256K
#define MAX_CACHE 1073741824  // 1G

typedef struct DoublyLinkedList {
	unsigned key;
	char *data;
	struct DoublyLinkedList *pre;
	struct DoublyLinkedList *next;
} node;

typedef struct LinkedBucket {
	bool full;
	node *data;
	struct LinkedBucket *next;
} bucket;

char *mem_pool;
bucket *mem_bucket;

// use map for search node pointer quickly
map<unsigned, node *> mem_map;
map<unsigned, node *>::iterator map_it;

unsigned all_num = 0;
unsigned cache_miss = 0;


void swap_cache(bucket *, node *);
node *replace_cache(bucket *, unsigned, unsigned &);
node *insert_cache(bucket *, unsigned);

char *init_mem_node(bucket *, char *, int, int);
void init_mem_bucket(int *, int);
void init_mem_pool();

void cache_data(unsigned key, unsigned leng);
void handle_req(unsigned key, unsigned leng);

/*
 * move old node to front. eg. y is old node
 *
 *  head  ptr                head
 *   |    |                  |
 *   x -> y -> z     ->      y -> x -> z
 *   x <- y <- z             y <- x <- z
 */
void swap_cache(bucket *bucket_ptr, node *node_ptr)
{
	node *pre, *next, *end;

	pre = node_ptr->pre;
	next = node_ptr->next;
	pre->next = next;
	next->pre = pre;

	end = bucket_ptr->data->next->pre;
	node_ptr->next = bucket_ptr->data->next;
	bucket_ptr->data->next->pre = node_ptr;
	node_ptr->pre = end;
	end->next = node_ptr;

	bucket_ptr->data->next = node_ptr;
}

// replace last node, and move head to last node
node *replace_cache(bucket *bucket_ptr, unsigned name, unsigned &old_name)
{
	node *node_ptr;
	node_ptr = bucket_ptr->data->next->pre;
	old_name = node_ptr->key;
	node_ptr->key = name;
	bucket_ptr->data->next = node_ptr;

	return node_ptr;
}

// push data into last node, and move head to last node
node *insert_cache(bucket *bucket_ptr, unsigned name)
{
	node *node_ptr;
	node_ptr = bucket_ptr->data->next->pre;
	node_ptr->key = name;
	bucket_ptr->data->next = node_ptr;

	if (node_ptr->pre->key != 0)
		bucket_ptr->full = true;

	return node_ptr;
}

// init node, put node->data point to memory pool address
char *init_mem_node(bucket *bucket_ptr, char *pool_ptr, int size, int length)
{
	int i;
	node *node_ptr, *tmp;

	node_ptr = (node *) malloc(sizeof(node));
	node_ptr->key = size;
	node_ptr->data = NULL;
	node_ptr->pre = NULL;
	node_ptr->next = NULL;
	bucket_ptr->data = node_ptr;

	for (i = 0; i < size; i++) {
		tmp = (node *) malloc(sizeof(node));
		tmp->key = 0;
		tmp->data = pool_ptr;
		tmp->pre = node_ptr;

		node_ptr->next = tmp;
		tmp->next = bucket_ptr->data->next;
		node_ptr = node_ptr->next;
		pool_ptr += length;
	}
	bucket_ptr->data->next->pre = node_ptr;

	return pool_ptr;
}

void init_mem_bucket(int arr[], int size)
{
	int i, length = MIN_NUM;
	char *pool_ptr = mem_pool;
	bucket *head, *tmp;

	mem_bucket = (bucket *) malloc(sizeof(bucket));
	head = mem_bucket;
	head->full = false;
	head->data = NULL;
	head->next = NULL;

	for (i = 0; i < size; i++) {
		printf("bucket %d\tsize: %d\n", i, arr[i]);

		tmp = (bucket *) malloc(sizeof(bucket));
		tmp->full = false;
		tmp->next = NULL;
		pool_ptr = init_mem_node(tmp, pool_ptr, arr[i], length);

		head->next = tmp;
		head = head->next;
		length *= 2;
	}
}

// get bucket number, and each bucket size
void init_mem_pool()
{
	int i, total, num = 0;
	unsigned length = 0, rest = MAX_NUM * 20;

	// init memory pool
	mem_pool = (char *) malloc(MAX_CACHE);
	memset(mem_pool, 0, MAX_CACHE);

	for (i = MIN_NUM * 2; i < MAX_NUM; i *= 2) {
		num++;
		length += i;
	}
	int buckets[num + 2];

	// rest: collect residue memory
	total = (MAX_CACHE - rest) / length;
	rest += (MAX_CACHE - rest) % length;

	length = MIN_NUM * 2;
	for (i = 0; i < num; i++) {
		buckets[i + 1] = total / 2;
		rest += total % 2 * length;

		total = total / 2;
		length *= 2;
	}

	// for 1K and 256K bucket
	length = rest / 2;
	buckets[0] = length / MIN_NUM;
	buckets[num + 1] = length / MAX_NUM;

	init_mem_bucket(buckets, num + 2);
}

void cache_data(unsigned key, unsigned leng)
{
	char *pool_ptr;
	unsigned old_key, size = MIN_NUM;
	bucket *bucket_ptr = mem_bucket->next;
	node *node_ptr;

	// find bucket
	while (leng > size) {
		size *= 2;
		bucket_ptr = bucket_ptr->next;
	}

	map_it = mem_map.find(key);
	// cache miss
	if (map_it == mem_map.end()) {
		if (bucket_ptr->full == true) {  // cache is full
			node_ptr = replace_cache(bucket_ptr, key, old_key);
			mem_map.erase(old_key);
		} else {
			node_ptr = insert_cache(bucket_ptr, key);
		}

		pool_ptr = node_ptr->data;
		sprintf(pool_ptr, "%u", key);
		mem_map[key] = node_ptr;

		cache_miss++;
	} else {
		swap_cache(bucket_ptr, map_it->second);
	}
}

void handle_req(unsigned name, unsigned leng)
{
	if (leng > MAX_NUM)
		cache_miss++;
	else
		cache_data(name, leng);

	all_num++;
}

int main(int argc, char **argv)
{
	float rate;
	unsigned name, length;
	char cmd[] = "awk -F ':| ' '{print $7\" \"$12}' download.log";

	init_mem_pool();
	printf("\nmemory pool inited\n");

	FILE *pipe = popen(cmd, "r");
	if (!pipe) {
		printf("exec cmd failed: %s", cmd);
		exit(1);
	}

	while (fscanf(pipe, "%u %u", &name, &length) == 2) {
		handle_req(name, length);
	}

	pclose(pipe);

	rate = (all_num - cache_miss) * 100.0 / all_num;
	printf("\nall_num: %u\ncache_miss: %u\nhit_rate: %f\%\n", all_num, cache_miss, rate);

	return 0;
}

/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */
