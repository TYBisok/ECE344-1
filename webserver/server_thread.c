#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <stdbool.h>
#include <unistd.h>

struct cache_element {
	struct file_data* cache_file;
	bool occurrence;
	int in_use; // how many threads are currently using this file
	struct cache_element *next;
};

struct cache_table {
		struct cache_element **hast_table;
		int size;
};

struct lru_queue{
	char* file_name;
	int file_size;
	int in_use;
	struct lru_queue* next;
};

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int curr_cache_size;
	struct lru_queue* lru;
	int exiting;
	int *buffer;
	pthread_t *thread_pool;
	struct cache_table *cache; 
};

/* static functions */
pthread_mutex_t  lock;
pthread_cond_t full;
pthread_cond_t empty;
int buff_in = 0;
int buff_out = 0;
int request_count = 0;

struct lru_queue* create_lru_entry(struct lru_queue* dest, struct cache_element* src){

		dest = (struct lru_queue*) malloc(sizeof(struct lru_queue));
		dest->file_name = (char*) malloc(strlen(src->cache_file->file_name) + 1);
		strcpy(dest->file_name, src->cache_file->file_name);
		dest->file_size = src->cache_file->file_size;
		dest->in_use = src->in_use;
	
		return dest;
}

void insert_lru(struct server* sv, struct cache_element* source){
	struct lru_queue* temp = sv->lru;
	struct lru_queue* tempPrev = NULL;
	if(temp == NULL) {
		sv->lru = create_lru_entry(sv->lru, source);
		sv->lru->next = NULL;
	}
	else
	{
		while(temp != NULL){
			tempPrev = temp;
			temp = temp->next;
		}
		tempPrev->next = create_lru_entry(tempPrev->next, source);
		tempPrev->next->next = NULL;
	}
	return;
}

void print_lru(struct server* sv){
	struct lru_queue* temp = sv->lru;
	int counter = 0; 
	while(temp != NULL){
		printf("%d: %s\n", counter, temp->file_name);
		counter++;

		temp = temp->next;
	}
}

void update_lru(struct server* sv, struct cache_element* source) {

	struct lru_queue* update = sv->lru;
	if(update == NULL || update->next == NULL) {
		return;
	}

	if(!strcmp(update->file_name, source->cache_file->file_name)){
		struct lru_queue* temp = sv->lru;
		while(temp->next != NULL){
			temp = temp->next;
		}
		sv->lru = sv->lru->next;
		temp->next = update;
		update->next = NULL;	
		return; 
	}

	while(update->next != NULL){
		if(!strcmp(update->next->file_name, source->cache_file->file_name))
			break;
		else
			update = update->next;
	}
	if(update->next == NULL){
		return;
	}

	struct lru_queue* swap = update->next;

	if(swap->next == NULL){
		return;
	}

	while(swap->next != NULL){
		swap = swap->next;
	}


	struct lru_queue* temp = update->next;
	update->next = update->next->next;
	swap->next = temp;
	temp->next = NULL;
	return;
	
}

unsigned int get_hash_value(struct server *sv, const char *word) {
	
	unsigned int value = 0;

	// Good way to hash entries
	for(int i= 0; i < strlen(word); ++i) {
				value = value * 37 + word[i];
	}

	// Insure value is somewhere between 0 and TABLE_SIZE
	value = value % sv->cache->size;

	return value;
}

struct cache_element* cache_lookup(struct server* sv, char* word){
	int hash_val = get_hash_value(sv, word);
	if(sv->cache->hast_table[hash_val] == NULL)
		return NULL;
	else{
		struct cache_element* current = sv->cache->hast_table[hash_val];
		while(current != NULL){
			if(!strcmp(current->cache_file->file_name, word))
				return current;
			else
				current = current->next;
			
		}
	}
	return NULL;
}


struct cache_element *create_hash_entry(const struct file_data* data) {

	// create space for new entry
	struct cache_element *new_entry = malloc(sizeof(new_entry));
	new_entry->occurrence = 1;

	new_entry->cache_file = (struct file_data*) malloc(sizeof(struct file_data));
	new_entry->cache_file->file_buf = (char*) malloc(strlen(data->file_buf) + 1);
	new_entry->cache_file->file_name = (char*) malloc(strlen(data->file_name) + 1);

	new_entry->cache_file->file_size = data->file_size;
	strcpy(new_entry->cache_file->file_buf, data->file_buf);
	strcpy(new_entry->cache_file->file_name, data->file_name);
	new_entry->in_use = 0;
	new_entry->next = NULL;

	return new_entry;
}

void removeLRU(struct server* sv){
	
	struct lru_queue* del = sv->lru;
	sv->lru = sv->lru->next;

	struct cache_element* del_cache = cache_lookup(sv, del->file_name);
	del_cache = del_cache->next;
	return;
}

bool cache_evict(struct server* sv, int amount_to_evict){

	int amount_evicted = 0;
	struct lru_queue* curr = sv->lru;

	while(curr != NULL && amount_evicted < amount_to_evict){
		//if(curr->in_use <= 0){
			amount_evicted = amount_evicted + curr->file_size;
			removeLRU(sv);
		//}
		curr = curr->next;
	}
	if(amount_evicted < amount_to_evict)
		return false;
	else 
		return true;
}

struct cache_element* cache_insert(struct server* sv, const struct file_data* data){

	// Assign hash value
	if(sv->curr_cache_size + data->file_size > sv->max_cache_size){
		bool success = cache_evict(sv, sv->curr_cache_size + data->file_size - sv->max_cache_size);
		if(success == false)
			return NULL;
	}

	sv->curr_cache_size = sv->curr_cache_size + data->file_size;

	unsigned int hash_val = get_hash_value(sv, data->file_name);

	struct cache_element *entry = sv->cache->hast_table[hash_val];
	struct cache_element *prev;

	// If there is no current entry in that slot
	if(entry == NULL) {
		sv->cache->hast_table[hash_val] = create_hash_entry(data);
		entry = sv->cache->hast_table[hash_val];
		insert_lru(sv, entry);
		return entry;
	}

	bool found_occurence = false;

	// Iterate through the linked list until either 
	// a duplicate is found or the end of the list 
	// is reached and an entry must be created there
	while(entry != NULL) {

		if(strcmp(entry->cache_file->file_name, data->file_name) == 0) {
			entry->occurrence++;
			found_occurence = true;
			break;
		}
		prev = entry;
		entry = prev->next;
	}
	if(!found_occurence) {
		entry = create_hash_entry(data);
		prev->next = entry;
	}

	insert_lru(sv, entry);
	return entry;
}

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}

	if(sv->max_cache_size <= 0){
		/* read file, 
		* fills data->file_buf with the file contents,
		* data->file_size with file size. */
		ret = request_readfile(rq);
		if (ret == 0) { /* couldn't read file */
			goto out;
		}
		/* send file to client */
		request_sendfile(rq);
		goto out;
	}


	pthread_mutex_lock(&lock);
	struct cache_element* current = cache_lookup(sv, data->file_name);

	if(current != NULL){ // if exists in cache
		update_lru(sv, current);
		data->file_buf = strdup(current->cache_file->file_buf);
		data->file_size = current->cache_file->file_size;
		current->in_use++;
	}
	else{
		pthread_mutex_unlock(&lock);
		ret = request_readfile(rq);
		if (ret == 0) {
			goto out;
		}
		pthread_mutex_lock(&lock);

		current = cache_lookup(sv, data->file_name);
		if(current != NULL){ // check again if exists in cache
			update_lru(sv, current);
			data->file_buf = strdup(current->cache_file->file_buf);
			data->file_size = current->cache_file->file_size;
			current->in_use++;
		}
		else{
			current = cache_insert(sv, data);
			if(current != NULL)
				current->in_use++;
		}
		
	}
	request_set_data(rq, data);
	pthread_mutex_unlock(&lock);
	request_sendfile(rq);
	if(current != NULL){
		pthread_mutex_lock(&lock);
		current->in_use--;
		pthread_mutex_unlock(&lock);
	}
out:
	request_destroy(rq);
	file_data_free(data);
}

/* entry point functions */
void worker_thread(struct server *sv){
	while(sv->exiting != 1){
		pthread_mutex_lock(&lock);
		while(request_count == 0){
			pthread_cond_wait(&empty, &lock);
			if(sv->exiting){
				pthread_mutex_unlock(&lock);
				return;
			}
		}
		if(request_count == sv->max_requests){
			pthread_cond_broadcast(&full);
		}
		int connection_fd = sv->buffer[buff_out];

		buff_out++;
		buff_out = buff_out%(sv->max_requests);
		request_count--;

		pthread_mutex_unlock(&lock);
		do_server_request(sv, connection_fd);
	}
}

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;
	sv->curr_cache_size = 0;
	sv->buffer = NULL;
	sv->thread_pool = NULL;
	sv->cache = NULL;
	sv->lru = NULL;

	int ret = pthread_mutex_init(&lock, NULL);
	assert(!ret);
	ret = pthread_cond_init(&empty, NULL);
	assert(!ret);
	ret = pthread_cond_init(&full, NULL);
	assert(!ret);
	
	if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {

		sv->buffer = (int*) malloc(max_requests * sizeof(int));
		
		sv->thread_pool = (pthread_t*) malloc(nr_threads * sizeof(pthread_t));
		for(int i = 0; i < nr_threads; i++){
			pthread_create( &(sv->thread_pool[i]), NULL, (void*)&worker_thread, sv);
		}
		sv->cache = (struct cache_table*) malloc(sizeof(struct cache_table));
		sv->cache->size = max_cache_size;
		sv->cache->hast_table = (struct cache_element**) malloc(max_cache_size*sizeof(struct cache_element*));
		for(int i = 0; i < max_cache_size; ++i){
			sv->cache->hast_table[i] = NULL;
		}
	}

	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */
		pthread_mutex_lock(&lock);
		
		while(request_count == sv->max_requests){
			pthread_cond_wait(&full,  &lock);
		}
		
		sv->buffer[buff_in] = connfd;
		
		if(request_count == 0){
			pthread_cond_broadcast(&empty);
		}

		buff_in++;
		buff_in = buff_in%(sv->max_requests);
		request_count++;

		pthread_mutex_unlock(&lock);
	}
}

void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;

	pthread_cond_broadcast(&full);
	pthread_cond_broadcast(&empty);

	for(int i = 0; i < sv->nr_threads; i++){
		pthread_join( (pthread_t) sv->thread_pool[i], NULL);
	}

	free(sv->thread_pool);
	free(sv->buffer);

	/* make sure to free any allocated resources */
	free(sv);
}
