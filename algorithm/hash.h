#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "common.h"
#include "measure.h"

#ifndef _HASH_H
#define _HASH_H 1

/* Hashing routines.
 */

llu **Zi; // Zobrist table for items
llu **Zl; // Zobrist table for loads

// generic hash table (for configurations)
binconf **ht;

// output hash table (needs to be different)
binconf **outht;

// hash table for dynamic programming calls / feasibility checks
dp_hash_item **dpht;

/* Reads random 64 bits on a Unix machine.
   Does not work elsewhere.
*/
llu rand_64bit()
{
    llu r;
    size_t rv;
    FILE* ur;
    ur = fopen("/dev/urandom", "r");
    rv = fread(&r, sizeof(llu), 1, ur);
    //assert(rv == 1);
    fclose(ur);
    return r;
}

/* Initializes the Zobrist hash table.
   Adding Zl[i][0] and Zi[i][0] enables us
   to "unhash" zero.
 */
void zobrist_init()
{
    Zi = malloc((S+1)*sizeof(llu *));
    Zl = malloc((BINS+1)*sizeof(llu *)); //TODO: make the 3 a generic number

    for(int i=1; i<=BINS; i++)
    {
	Zl[i] = malloc((R+1)* sizeof(llu));
	for(int j=0; j<=R; j++)
	{
	    Zl[i][j] = rand_64bit();
	}	
    }
    
    
    for(int i=1; i<=S; i++) // different sizes of items
    {
	Zi[i] = malloc((R+1)*BINS*sizeof(llu));
	
	for(int j=0; j<=R*BINS; j++) // number of items of this size
	{
	    Zi[i][j] = rand_64bit(); 
	}
    }
}

void global_hashtable_init()
{

    dpht = malloc(HASHSIZE * sizeof(dp_hash_item *));
    assert(dpht != NULL);
    outht = malloc(HASHSIZE * sizeof(binconf *));
    assert(outht != NULL);
    for(int i=0; i< HASHSIZE; i++)
    {
	outht[i] = NULL;
	dpht[i] = NULL;
    }
    zobrist_init();
    measure_init();
}

void local_hashtable_init()
{
    ht = malloc(HASHSIZE * sizeof(binconf *));
    assert(ht != NULL);
    //assert(dpht != NULL);
    for(int i=0; i< HASHSIZE; i++)
    {
	ht[i] = NULL;
	//dpht[i] = NULL;
    }
}

void global_hashtable_cleanup()
{
    int c;
    // zobrist cleanup
    for(int i=1; i<=BINS; i++)
    {
	free(Zl[i]);
    }

    for(int j=1; j<=S; j++)
    {
	free(Zi[j]);
    }
    free(Zl);
    free(Zi);

    binconf *x, *p;
    //hashtable output cleanup
    for(int k=0; k< HASHSIZE; k++)
    {
	c=0;
	x = outht[k];
	// counting objects for debug purposes
	while(x != NULL)
	{
	    x = x->next;
	    c++;
	}
	assert(c<=CHAINLEN);
	// removing objects
	x = outht[k];
	while(x != NULL)
	{
	    p = x->next;
	    free(x);
	    x = p;
	}
    }

    // dynamic programming hash table cleanup
    dp_hash_item *dp_item, *dp_pointer;
    for(int k=0; k< HASHSIZE; k++)
    {
	c=0;
	dp_item = dpht[k];
	// counting objects for debug purposes
	while(dp_item != NULL)
	{
	    dp_item = dp_item->next;
	    c++;
	}
	assert(c<=CHAINLEN);
	// removing objects
	dp_item = dpht[k];
	while(dp_item != NULL)
	{
	    dp_pointer = dp_item->next;
	    free(dp_item);
	    dp_item = dp_pointer;
	}
    }

    free(dpht);
    free(outht); 
}

// cleanup function -- technically not necessary but useful for memory leak checking
void local_hashtable_cleanup()
{
    int c;
    
    //hashtable cleanup
    binconf *x, *p;
    for(int k=0; k< HASHSIZE; k++)
    {
	c=0;
	x = ht[k];
	// counting objects for debug purposes
	while(x != NULL)
	{
	    x = x->next;
	    c++;
	}
	assert(c<=CHAINLEN);
	// removing objects
	x = ht[k];
	while(x != NULL)
	{
	    p = x->next;
	    free(x);
	    x = p;
	}
    }

    free(ht);
}
// Few debug functions.
void hashtable_print()
{
    for(int i=0; i<HASHSIZE; i++)
    {
	if(ht[i] != NULL)
	{
	    int c = 0;
	    binconf *p;
	    p = ht[i];
	    while(p != NULL)
	    {
		p = p->next;
		c++;
	    }
	    fprintf(stderr, "ht[%d] is occupied with %d elements.\n", i, c);
	}
    }
}

void zobrist_print()
{
    for(int i=1; i<=S; i++)
	for(int j=1; j<=R; j++)
	{
	    fprintf(stderr, "Zi[%d][%d] = %llu\n", i,j, Zi[i][j]);
	}
}

void printBits32(unsigned int num)
{
   for(int bit=0;bit<(sizeof(unsigned int) * 8); bit++)
   {
      fprintf(stderr, "%i", num & 0x01);
      num = num >> 1;
   }
   fprintf(stderr, "\n");
}

void printBits64(llu num)
{
   for(int bit=0;bit<(sizeof(llu) * 8); bit++)
   {
      fprintf(stderr, "%llu", num & 0x01);
      num = num >> 1;
   }
   fprintf(stderr, "\n");
}

/* returns a lower HASHLOG bits of a 64-bit number */
unsigned int lowerpart(llu x)
{
    llu mask,y;
    mask = (HASHSIZE) - 1;
    y = (x) & mask;
    return (unsigned int) y;
}

// calculates the hash of b completely.
void hashinit(binconf *d)
{
    d->loadhash=0;
    d->itemhash=0;
    
    for(int i=1; i<=BINS; i++)
    {
	d->loadhash ^= Zl[i][d->loads[i]];
    }
    for(int j=1; j<=S; j++)
    {
	d->itemhash ^= Zi[j][d->items[j]];
    }
}

/* Assuming binconf d is created by adding an element item to an
   arbitrary bin, we update the hash. Since we are sorting the
   bins, the hashes of all loads may change.

   Assume that both *prev and *d have sorted loads in decreasing
   order.
*/
void rehash(binconf *d, const binconf *prev, int item)
{
    
    //assert(d->loads[bin] <= R);
    //assert(formerload <= R);

    // rehash loads
    for(int bin=1; bin<= BINS; bin++)
    {
	d->loadhash ^= Zl[bin][prev->loads[bin]];
	d->loadhash ^= Zl[bin][d->loads[bin]];
    }

    // rehash item lists
    d->itemhash ^= Zi[item][prev->items[item]];
    d->itemhash ^= Zi[item][d->items[item]];
}

// rehash for dynamic programming purposes, assuming we have added
// one item of size "dynitem"
void dp_rehash(binconf *d, int dynitem)
{
    d->itemhash ^= Zi[dynitem][d->items[dynitem] -1];
    d->itemhash ^= Zi[dynitem][d->items[dynitem]];

}

// opposite of dp_rehash -- rehashes after removing one item "dynitem"
void dp_unhash(binconf *d, int dynitem)
{
    d->itemhash ^= Zi[dynitem][d->items[dynitem] + 1];
    d->itemhash ^= Zi[dynitem][d->items[dynitem]];
}

/* Checks if an element is hashed, returns -1 (not hashed)
   or 0/1 if it is. */
int is_conf_hashed(binconf **hashtable, const binconf *d)
{
    int lp = lowerpart(d->itemhash ^ d->loadhash);
    binconf *r;
    r = hashtable[lp];
    while(r != NULL)
    {
	if (r->loadhash == d->loadhash && r->itemhash == d->itemhash)
	{
	    r->accesses++;
#ifdef VERBOSE
	    fprintf(stderr, "Found the following position in a hash table:\n");
	    print_binconf(d);
#endif
	    return r->posvalue;
	}
	r = r->next;
    }
    return -1;
}

/* Adds an element to a configuration hash.
 */

void conf_hashpush(binconf** hashtable, const binconf *d, int posvalue)
{
    binconf *e, *t, *p, *minac;
    int c;
    e = malloc(sizeof(binconf)); assert(e != NULL);
    init(e);
    duplicate(e,d);
    e->posvalue = posvalue;
    unsigned int lp = lowerpart(e->loadhash ^ e->itemhash);
#ifdef VERBOSE
    fprintf(stderr, "Hashing the following position with value %d:\n", posvalue);
    print_binconf(d);
#endif
    
#ifdef VERBOSE
    printBits32(lp);
#endif
    t = hashtable[lp];
    if(t == NULL)
    {
	hashtable[lp] = e;
	e->accesses = 0;
    } else {
	t = hashtable[lp];
	c = 1;
	while((c < (CHAINLEN-1)) && t->next!=NULL)
	{
	    t = t->next;
	    c++;
	}

	if(t->next == NULL)
	{
	    t->next = e;
	} else {
	    // check for the item with the least number of accesses
#ifdef VERBOSE
	    fprintf(stderr, "We have to remove an element.\n");
#endif	    
	    p = hashtable[lp];
	    minac = hashtable[lp];
	    while(p != NULL)
	    {
		if(p->accesses < minac->accesses)
		    minac = p;
		p = p->next;
	    }

	    // remove the item with the least number of accesses
	    p = hashtable[lp];
	    int k = 1;
	    if(minac == p)
	    {
		e->next = hashtable[lp]->next;
		free(hashtable[lp]);
		hashtable[lp] = e;
#ifdef VERBOSE
		fprintf(stderr, "Element removed is %d\n", k);
#endif
	    } else {
		while(p->next != minac)
		{
		    p = p->next;
		    k++;
		}
#ifdef VERBOSE
		fprintf(stderr, "Element removed is %d\n", k);
#endif
	        e->next = p->next->next;
		free(p->next);
		p->next = e;
	    }
	}
	
    }
}

// Checks if a number is in the dynamic programming hash.
// Returns -1 (not hashed) and 0/1 (it is hashed, this is its feasibility)
int dp_hashed(const binconf* b)
{
    unsigned int lp = lowerpart(b->itemhash);
    dp_hash_item *p = dpht[lp];
    while( p != NULL)
    {
	if(p->itemhash == b->itemhash)
	{
	    return p->feasible;
	    break;
	} else {
	    p = p->next;
	}
    }

    return -1;
	
}


// Adds an number to a dynamic programming hash table
void dp_hashpush(const binconf *d, bool feasible)
{
    dp_hash_item *e, *t, *p, *minac;
    int c;
    e = malloc(sizeof(dp_hash_item)); assert(e != NULL);
    dp_hash_init(e,d,feasible);
    unsigned int lp = lowerpart(e->itemhash);
    
    t = dpht[lp];
    if(t == NULL)
    {
	dpht[lp] = e;
    } else {
	t = dpht[lp];
	c = 1;
	while((c < (CHAINLEN-1)) && t->next!=NULL)
	{
	    t = t->next;
	    c++;
	}

	if(t->next == NULL)
	{
	    t->next = e;
	} else {
	    // check for the item with the least number of accesses
#ifdef VERBOSE
	    fprintf(stderr, "DPHT: We have to remove an element.\n");
#endif	    
	    p = dpht[lp];
	    minac = dpht[lp];
	    while(p != NULL)
	    {
		if(p->accesses < minac->accesses)
		    minac = p;
		p = p->next;
	    }

	    // remove the item with the least number of accesses
	    p = dpht[lp];
	    int k = 1;
	    if(minac == p)
	    {
		e->next = dpht[lp]->next;
		free(dpht[lp]);
		dpht[lp] = e;
#ifdef VERBOSE
		fprintf(stderr, "DPHT: Element removed is %d\n", k);
#endif
	    } else {
		while(p->next != minac)
		{
		    p = p->next;
		    k++;
		}
#ifdef VERBOSE
		fprintf(stderr, "DPHT: Element removed is %d\n", k);
#endif
	        e->next = p->next->next;
		free(p->next);
		p->next = e;
	    }
	}
	
    }

}

#endif
