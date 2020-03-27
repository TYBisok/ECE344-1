#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "common.h"
#include "wc.h"
#include "string.h"

struct linked_list_entry {
	char *key;						// The inserted word
	int occurence;					// Number of times the word has occured
	struct linked_list_entry *next; // Next entry of the list
};

struct wc {
	// hash_table is a pointer to an array of linked list entries
	struct linked_list_entry **hash_table;
};

int TABLE_SIZE = 1;

unsigned int get_hash_value(const char *word) {
	
	unsigned int value = 0;

	// Good way to hash entries
	for(int i= 0; i < strlen(word); ++i) {
				value = value * 37 + word[i];
	}

	// Insure value is somewhere between 0 and TABLE_SIZE
	value = value % TABLE_SIZE;

	return value;
}

struct wc *create_hash_table() {


	// Allocate space for the hash table
	struct wc *wc = (struct wc *)malloc(sizeof(struct wc));
	
	// Allocate space for each entry in table
	wc->hash_table = malloc(sizeof(struct linked_list_entry*) *TABLE_SIZE);

	// Initialize entries to null
	for(int i = 0; i< TABLE_SIZE; ++i) {
		wc->hash_table[i] = NULL;
	}

	return wc;
}

struct linked_list_entry *create_hash_entry(const char *key) {

	// create space for new entry
	struct linked_list_entry *new_entry = malloc(sizeof(new_entry));
	new_entry->key = malloc(strlen(key) + 1);

	// Set values of new entry
	strcpy(new_entry->key, key);
	new_entry->next = NULL;

	return new_entry;
}

void insert_entry(struct wc *wc, const char *key) {


	// Assign hash value
	unsigned int hash_val = get_hash_value(key);

	struct linked_list_entry *entry = wc->hash_table[hash_val];
	struct linked_list_entry *prev;

	// If there is no current entry in that slot
	if(entry == NULL) {
		wc->hash_table[hash_val] = create_hash_entry(key);
		wc->hash_table[hash_val]->occurence = 1;
		return;
	}

	bool found_occurence = false;

	// Iterate through the linked list until either 
	// a duplicate is found or the end of the list 
	// is reached and an entry must be created there
	while(entry != NULL) {

		if(strcmp(entry->key, key) == 0) {
			entry->occurence++;
			found_occurence = true;
			break;
		}
		prev = entry;
		entry = prev->next;
	}
	if(!found_occurence) {
		entry = create_hash_entry(key);
		entry->occurence = 1;
		prev->next = entry;
	}
	return;

}



struct wc *
wc_init(char *word_array, long size)
{
	// table_size set to be roughly twice the number of entries
	for(int i = 0; i < size; ++i) {
		if(isspace(word_array[i])) TABLE_SIZE++;
	}
	TABLE_SIZE = 2 * TABLE_SIZE;

	// Inialize wc
	struct wc *wc;
	wc = create_hash_table();
	assert(wc);	
	
	// Slice the words and insert them into hash table
	char *word_array_dupe = strdup(word_array);
	char delim[] = " \n\t\v\f\r";
	char *current_word = strtok(word_array_dupe, delim);
	
	while(current_word != NULL){

		insert_entry(wc, current_word);
		current_word = strtok(NULL, delim);
	}
	return wc;
}

void
wc_output(struct wc *wc)
{
	for(int i = 0; i < TABLE_SIZE; ++i) {
		
		struct linked_list_entry *entry = wc->hash_table[i];

		if(entry == NULL) continue;

		while(1) {
			printf("%s:%d\n", entry->key, entry->occurence);
			if(entry->next == NULL) 
				break;
			else 
				entry = entry->next;
		}
	}
}

void
wc_destroy(struct wc *wc)
{
	for(int i=0; i < TABLE_SIZE; ++i) {
	
		struct linked_list_entry *current = wc->hash_table[i];
		struct linked_list_entry *next;

		while(current != NULL) {
			next = current->next;
			free(current->key);
			free(current);
			current = next;
		}
		wc->hash_table[i] = NULL;
	}
	free(wc->hash_table);
	free(wc);
}
