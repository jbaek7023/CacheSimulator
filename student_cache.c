#include "student_cache.h"


int student_read(address_t addr, student_cache_t *cache, stat_t *stats){
	stats->accesses++;
	stats->reads++;	
	cache_block *block;
	block = findBlock(addr,cache,0);
	if(block == NULL) {
		block = findInvalid(addr,cache);
		if(block == NULL) {
			block = findLRU(addr,cache);
			if(block->dirty && cache->WP == WBWA) {
				
		stats->mem_write_bytes+=1<<(cache->B); 
			}
		}
		block->dirty = 0;
		block->valid = 1;
		block->tag = getTag(addr,cache);
		stats->read_miss++;
		stats->mem_read_bytes+=1<<(cache->B); 		
	return 0;
	}
	stats->read_hit++;
	return 1;
}

cache_block* findBlock(address_t addr, student_cache_t *cache, int write) {
	int index = getIndex(addr,cache);
	int tag = getTag(addr,cache);
	cache_block *block;
	for(int i=0; i<cache->ways_size; i++) {
		block = (cache->ways+i)-> blocks + index;
		if(block->valid && block->tag == tag) {
			if(!write || cache->WP==WBWA) {
				setUsed(cache,index,i); 
			}
			return block;
		}
	}
	return NULL;
}


int getTag(address_t address, student_cache_t *cache) {
	int obits = cache->B;
	int ibits = cache->C - cache->B - cache->S;
	int tbits = 32-cache->C+cache->S; 	/*Tag Bits*/ /*32 - cache->B - ibits*/
	if(tbits==32) {
		return address;
	}
	return (address & (((1 << tbits) - 1) << obits << ibits)) >> obits >> ibits; 
}


int getIndex(address_t address, student_cache_t *cache) {
	int obits = cache->B;
	int ibits =  cache->C - cache->B - cache->S;
	return (address & (((1<< ibits)-1) << obits)) >> obits;
}



int student_write(address_t addr, student_cache_t *cache, stat_t *stats){
	stats->accesses++;
	stats->writes++;
	cache_block *block;
	if(cache->WP == WTWNA ) {
		
		stats->mem_write_bytes+=4;
	
	}
	block = findBlock(addr,cache,1);
	if(block == NULL) {
		if(cache->WP == WTWNA) {
			stats->write_miss++;  
			return 0;
		}
		block = findInvalid(addr,cache);
		if(block == NULL) {
			block = findLRU(addr,cache);
			if(block->dirty) {
				stats->mem_write_bytes+=1<<(cache->B); 
			}
		}
		block->valid = 1;
		block->dirty = 1;
		block->tag = getTag(addr,cache);
		stats->write_miss++;
		stats->mem_read_bytes+=1<<(cache->B); 
		return 0;
	}
	block->dirty = 1;
	stats->write_hit++;
	return 1;
}


cache_block* findLRU(address_t addr, student_cache_t *cache) {
	int index = getIndex(addr,cache);
	cache_LRU *lru;
	cache_block *block;
	lru = cache->LRUs + index; 
	block = (cache->ways + lru->way_index)->blocks+index;
	setUsed(cache,index,lru->way_index);
	return block;
}


student_cache_t *allocate_cache(int C, int B, int S, int WP, stat_t* statistics){
	cache_blocks *way;
	cache_LRU *lru;
	student_cache_t *cache = malloc(sizeof(student_cache_t));    
	cache->WP = WP;
	cache->C = C;
	cache->B = B;
	cache->S = S;
	cache->ways_size = (1<<(S));
	cache->ways = calloc(cache->ways_size , sizeof(cache_blocks));
	cache->LRUs_size = 1<<(C-B-S);
	cache->LRUs = calloc(cache->LRUs_size , sizeof(cache_LRU));
	for(int i=0; i<cache->LRUs_size; i++) {
		lru = cache->LRUs + i;
		lru->way_index = 0;
		for(int j=0; j<cache->ways_size; j++) {
			lru->way_index = j;
			if(j != cache->ways_size - 1) {
				lru->next = malloc(sizeof(cache_LRU));
				lru = lru->next;
			} else {
				lru->next = NULL;
			}
		}
	}

	for(int i=0; i<cache->ways_size; i++) {
		way = cache->ways + i;
		way->blocks_size = 1<<(C-B-S);
		way->blocks = calloc(way->blocks_size , sizeof(cache_block));
	}

	statistics->accesses = 0;
	statistics->misses = 0;
	statistics->reads = 0;
	statistics->writes = 0;
	statistics->read_hit = 0;
	statistics->write_hit = 0;
	statistics->read_miss = 0;
	statistics->write_miss = 0;
	statistics->mem_write_bytes = 0;
	statistics->mem_read_bytes = 0;
	statistics->read_hit_rate = 0;
	statistics->hit_rate = 0;
	statistics->total_bits = 0;
	statistics->AAT = 0;
	return cache;
}

void finalize_stats(student_cache_t *cache, stat_t *statistics){ 
	int block_meta =  (32 - cache->C + cache->S)+ 1;	//tag size + 1
	if(cache->WP == WBWA)	block_meta++;
	statistics->total_bits += (block_meta * (1<<(cache->C - cache->B)));
	statistics->total_bits += ((1<<(cache->S)) * cache->S) * (1<<(cache->C - cache->B - cache->S));
	statistics->total_bits += (1<<(cache->C)) * 8;
	statistics->misses = statistics->read_miss + statistics->write_miss;
	statistics->read_hit_rate = (statistics->read_hit * 1.0)/(statistics->reads);
	statistics->hit_rate = (statistics->read_hit + statistics->write_hit)*1.0 / statistics->accesses;
	statistics->AAT = (2 + .2 * cache->S)+ (1-statistics->read_hit_rate)* 50;
}


cache_block* findInvalid(address_t addr, student_cache_t *cache) {
	int index = getIndex(addr,cache);
		cache_block *block;
	for(int i=0; i<cache->ways_size; i++) {
		block =(cache->ways+i)->blocks + index;
		if(!block->valid) {
			setUsed(cache,index,i); 
			return block;
		} 
	}
	return NULL;
}

void setUsed(student_cache_t *cache, int block_index, int way_index) {
	int hit = 0;
	cache_LRU *current = cache->LRUs + block_index;
	while(current->next != NULL) {
		if(current->way_index == way_index) {
			hit = 1;
		}
	
		if(hit) {
			current->way_index=current->next->way_index;
		}
		current=current->next;
	}
	current->way_index = way_index;
}


