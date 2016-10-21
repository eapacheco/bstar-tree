/*
	Developed by Eduardo A. Pacheco
	"For of him, and through him, and to him, are all things:
	to whom be glory for ever. Amen." (Romans 11:36 KJV)

	June 21, 2016 - Brazil

	Copyright © 2016 Eduardo Pacheco. All rights reserved.

	------------------------------------------

	bs-tree.h
	Formal definition: 
	The B*-Tree index with a degree m is the defined as:
	1.	Every page (or node) in it has the following format
		<P1, <C1,PR1>, P2, <C2,PR2>, ..., Pq-1, <Cq-1,PRq-1>, Pq>, where (q <= m) and
		Each Ci (1 <= i <= q-1) is a search key
		Each PRi (1 <= i <= q-1) is a reference field pointing to the record at the
			data file
		Each Pj (1 <= j <= q) points to a subtree or has a NULL value, if it has no subtree
	2.	Inside each page
		C1 < C2 < ... < Cq-1.
	3.	Every key X in the subtree at Pi, about Ci keys in parent 
		Ci-1 < X < Ci (1 < i < q)
		X < Ci (i = 1)
		Ci-1 < X (i = q)
	4.	Each page has a maximum of m descendents, but the root
	5. 	Each page, but the root, has at least (2m-1)/3 descendents
	6. 	Root has at least two descendents, unless it's also a leaf
	7.	Every leaf is at the same height/level
	8. 	A non-leaf page has n descendents and n-1 keys
	9. 	A leaf page has at least ⎣2(m-1)/3⎦ keys and a maximum of m-1 keys (occupancy rate)
	10.	The root page is peculiar:
		Double-sized: has a maximum of 2m-2 keys and 2m-1 descendents
		Has no lower bounds for occupancy: it may have less than ⎣2(m-1)/3⎦, including zero.

	More information, specific of this implementation, may be found
	on the attached .pdf file at: https://github.com/eapacheco/bstar-tree (PORTUGUESE ONLY)

	Further information may be found at:
	https://en.wikipedia.org/wiki/B-tree
	https://en.wikipedia.org/wiki/B-tree#Variants
*/

#ifndef __BTREE__
#define __BTREE__

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef __BOOL_TYPE__
#define __BOOL_TYPE__ 
typedef char bool;
#define true (!0)
#define false 0
#endif

//tamplate for a compare function, which will be comparing keys
//returns 0, if equal;
//		< 0, if lesser;
//		> 0, if bigger.
#define compare_type int(*)(void*,void*)
#define compare_declare(f) int(*f)(void*,void*)

//template for running a key and it's related byte offset, it is accompanied by
//a list of arguments that were previously requested by the caller
//see __bstree_debug_dfs
#define runkey_type void(*)(void*,size_t,va_list)
#define runkey_declare(f) void(*f)(void*,size_t,va_list)

//function that prints only a key on the standard output
//see __bstree_debug_printtree
#define printkey_type void(*)(void*)
#define printkey_declare(f) void(*f)(void*)

//the minimum size for the header
//therefore, the minimum size for a block
#define HEADER_SIZE 36

//removes the single index file from disc
//attention, performing this operation before closing the index (bstclose)
//may cause unexpected behaviour
#define bstdrop(path) remove(path)

//The single instance to represent a B*-Tree
//B Star Tree
typedef struct tree BSTREE;

//creates a new file and initializes a BSTREE on it.
//parameters
//	indexpath	file path
//	blocksize	block size (page size, node size), in bytes
//				it must be bigger than HEADER_SIZE
//	keysize		key size, in bytes
//	func 		compare function, for the keys
//returns
//	instance of BSTREE
//	NULL, if it fails
BSTREE* bstcreate (char *indexpath, size_t blocksize, size_t keysize, compare_type);

//opens and existing index, only the root and header are loaded to memory
//parameters
//	indexpath	index path
//	func 		comparing function
//				WARNING: must be equivalent to the one used for creation
//				otherwise, the behaviour may be unexpected
//returns 
//	BSTREE instance
BSTREE* bstopen (char *indexpath, compare_type);

//persists header and root information
//frees allocated memory
//parameters
//	t 			tree
//returns
//	if it succedded to save and free
bool bstclose (bstclose) (BSTREE *t);

//returns the specified block size
size_t bstgetblocksize (BSTREE*);

//returns the specified key size
size_t bstgetkeysize (BSTREE*);

//return the tree height
//minimum height is 1 (one)
int bstgetheight (BSTREE*);

//return the count of keys stored in the tree
unsigned long bstgetcount (BSTREE*);

//returns the tree degree
int bstgetdegree (BSTREE*);

//calculates the degree for the specific conditions
//parameters
//	blocksize	block size
//	keysize		key size
//returns
//	the maximum degree for the proposed conditions
//	the calculus is based on the following node structure:
//		int count 						4 bytes
//		void *keys [degree*2 -1]			keysize * (grau*2 - 1)
//		size_t offset [degree*2 -1]		8 bytes * (degree*2 - 1)
//		size_t filhos [degree*2] 		8 bytes * degree*2
int bstcalcdegree (size_t blocksize, size_t keysize);

//inserts a new key/degree pair in the tree
//parameters
//	t 			tree
//	key 		key; it will be integrally written on disc and it is not
//				expected to point to other memory locations
//	recoffset	data record offset
//returns
//	if it succedded, the return is also the count of keys in the tree
bool bstinsert (BSTREE *t, void *key, size_t recoffset);

//delets a key from the tree
//parameters
//	t			tree
//	key			search key
//returns
//	if it was found, the record offset for the deleted key
//	otherwise, ULONG_MAX (limits.h)
size_t bstdelete (BSTREE *t, void *key);

//searches for a key
//parameters
//	t 			tree
//	key 		search key
//returns
//	if it was found, record offset for the key
//	otherwise, ULONG_MAX (limits.h)
size_t bstsearch (BSTREE *t, void *key);

//traverse all the keys through a DFS (Depth-first search) algorithm
//parameters
//	t 			tree
//	pre			run elements in pre-order; may be null
//	in 			run elements in in-order; may be null
//	post 		run elements in post-order; may be null
//	... 		arguments to be attached to the run function
void __bstree_debug_dfs (BSTREE*, runkey_type, runkey_type, runkey_type, ...);

//prints, on text mode, on the standard output, the following scheme:
//between each key, of the same page, a blank space ' ';
//at the end of a page, a line break '\n';
//at the end of a level, a second line break '\n',
//	that produces a blank line between levels;
//e.g. (the "--" comments wont be printed
//F			-- ROOT
//
//A B		-- CHILD 0 OF ROOT
//I J		-- CHILD 1 OF ROOT
//parameters
//	t 			tree
//	func 		printkey function
void __bstree_debug_printtree (BSTREE*, printkey_type);


#endif