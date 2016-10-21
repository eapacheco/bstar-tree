/*
	Desenvolvido por Eduardo A. Pacheco
	"Porque dele, e por meio dele, e para ele sao todas as coisas.
	A ele, pois, a gloria eternamente. Amem!" (Romanos 11:36 ARA)

	21 de junho de 2016, Brasil

	Copyright © 2016 Eduardo Pacheco. All rights reserved.

	------------------------------------------

	bs-tree.c
*/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

#include "bs-tree.h"

//tenta escrever dados no arquivo, se a escrita falhar, retorna 0,
//senao, incrementa um acumulador com a quantidade de bytes escritos
#define trytowrite(aux, arr, size, qtd, file, acc) \
if ( (aux = fwrite (arr, size, qtd, file)) != qtd ) \
return 0; \
else \
acc += qtd * size;

//tenta ler dados do arquivo, se a leitura falhar, retorna falso
//senao, nada acontece
#define trytoread(arr, size, qtd, file) \
if ( fread (arr, size, qtd, file) != qtd ) \
return false; \

//ocupacao maxima (numero de chaves) do noh
#define MAX_RATE(t) (t->degree - 1)
//ocupacao minima do noh; soh nao se aplica aa raiz
#define MIN_RATE(t) (2*MAX_RATE(t)/3)

//cansei de esquecer o sizeof; isso sim eh vida
#undef memcpy
#define memcpy(dest, orig, size, count) memcpy (dest, orig, (size) * (count))

typedef struct pair {
	void *key;
	size_t recoffset; //offset do registro referente aa chave
} PAIR;

typedef struct node {
	bool changed;	// indica se o noh foi alterado e precisa ser reescrito
	size_t offset;	// offset do noh

	/*** dados persistidos ***/
	unsigned int count; // quantidade de pares no noh
	PAIR **pairs;
	size_t *children; // filhos do noh, podendo ser NULL
	//size: [int] + [(keysize + size_t) * n] + [size_t * (n+1)]
} BSNODE;

struct tree {
	BSNODE *root; //a raiz soh eh liberada quando o indice eh fechado
	compare_declare(compar); // funcao que compara chaves,
	FILE* file; //arquivo fica aberto, em modo escrita e leitura, enquanto o indice estiver aberto
	int degree;

	/***** header - persistent data *****/
	int height; // altura da arvore
	unsigned long count; // quantidade de pares armazenados
	size_t keysize; // tamanho das chaves
	size_t blocksize; // tamanho do bloco
	size_t trashstack; // topo da lista de nohs deletados, para reaproveitamento de espaco
	//size: 8*4 + 4*1 = 36
};

size_t _bstree_search(BSTREE *t, BSNODE *n, void* key);

size_t _bstree_writeheader (BSTREE *t); // escreve o cabecalho; nao adiciona frag. interna
bool _bstree_readheader (BSTREE *t);

void _bstree_splitroot (BSTREE*);
void _bstree_splitchild2to3 (BSTREE*,BSNODE*,int,BSNODE*,BSNODE*);//arvore; pai; pos do filho, filho
void _bstree_redistributeby2 (BSTREE*,BSNODE*,int,BSNODE*,BSNODE*); //avore; pai; pos esq; node esq; node dir;
void _bstree_redistributeby3 (BSTREE*,BSNODE*,int,BSNODE*,BSNODE*,BSNODE*);

bool _bstree_insert (BSTREE*,BSNODE*,PAIR*);//arvore; pai; no atual; elemento
void _bstree_insert_rearrange (BSTREE*,BSNODE*,int,BSNODE*);

size_t _bstree_delete (BSTREE*,BSNODE*,void*);
PAIR* _bstree_delete_biggest (BSTREE*,BSNODE*);
PAIR* _bstree_delete_smallest (BSTREE*,BSNODE*);
size_t _bstree_delete_atleaf (BSTREE*,BSNODE*,int);
void _bstree_delete_rearrange (BSTREE*,BSNODE*,int,BSNODE*);
void _bstree_merge_root (BSTREE*,BSNODE*,BSNODE*);
void _bstree_merge3into2 (BSTREE*,BSNODE*,int,BSNODE*,BSNODE*,BSNODE*);

BSNODE* _bsnode_new (); //cria instancia de BSNODE com valores padroes
bool _bsnode_free (BSTREE*,BSNODE*); //REGRA: somente a funcao que carregou o no pode libera-lo - mesmo que seja redundante
void _bsnode_delete_atdisc (BSTREE*,BSNODE*); //NAO libera o no da memoria; adiciona seu bloco na lista de reuso
BSNODE* _bsnode_read (BSTREE*,size_t);
size_t _bsnode_write (BSTREE*,BSNODE*); //escreve o noh na posicao atual do arquivo; nao ocupara o bloco inteiro
size_t _bsnode_create (BSTREE*,BSNODE*); //escreve o noh no final do arquivo; adiciona a frag. interna; salva offsetcr
size_t _bsnode_create_root (BSTREE*,BSNODE*); //aloca dois blocos no final do arquivo para a raiz

void _pair_free (PAIR*);
PAIR* _pair_copy (BSTREE*,PAIR*); // copia um PAIR e a chave contida nele

void _dfs (BSTREE*,BSNODE*,runkey_type,runkey_type,runkey_type,va_list);

size_t bstgetblocksize (BSTREE *t) {
	return t->blocksize;
}

size_t bstgetkeysize (BSTREE *t) {
	return t->keysize;
}

int bstgetheight (BSTREE *t) {
	return t->height;
}

unsigned long bstgetcount (BSTREE *t) {
	return t->count;
}

int bstgetdegree (BSTREE *t) {
	return bstcalcdegree(t->blocksize, t->keysize);
}

BSTREE* bstopen (char *indexPath, compare_declare(compar)) {
	FILE *f;

	if ( !(f = fopen (indexPath, "r+b")) )
		return NULL;

	BSTREE *t = (BSTREE*) malloc (sizeof(BSTREE));

	t->compar = compar;
	t->file = f;
	_bstree_readheader(t);
	t->root = _bsnode_read(t, t->blocksize); // o noh raiz é sempre o segundo (e terceiro) bloco do arquivo.
	t->degree = bstcalcdegree(t->blocksize, t->keysize);

	return t;
}

BSTREE* bstcreate (char *indexPath, size_t blocksize, size_t keysize, compare_declare(compar)) {
	if (blocksize < HEADER_SIZE)
		return NULL;

	BSTREE *t = (BSTREE*) malloc (sizeof(BSTREE));

	if ( !(t->file = fopen (indexPath, "w+b")) ) {
		free(t);
		return NULL;
	}

	t->compar = compar;

	t->height = 1;
	t->count = 0;
	t->keysize = keysize;
	t->blocksize = blocksize;
	t->trashstack = 0;
	t->degree = bstcalcdegree(t->blocksize, t->keysize);

	if ( t->degree < 4 )
	{
		fclose (t->file);
		free (t);
		return NULL;
	}

	t->root = _bsnode_new ();

	size_t written = _bstree_writeheader (t);

	for (; written && written < t->blocksize; ++written)
		if (fputc (0, t->file) == EOF) //todo: rollback?
			written = 0;

	if (!written)
	{
		_bsnode_free(t, t->root);
		fclose(t->file);
		free (t);

		return NULL;
	}

	if ( !_bsnode_create_root (t, t->root) ) {
		_bsnode_free (t, t->root);
		fclose(t->file);
		free (t);

		return NULL;
	}

	return t;
}

bool bstclose (BSTREE *t) {
	if (!_bstree_writeheader (t))
		return false;

	_bsnode_free(t, t->root);
	fclose (t->file);
	free (t);
	return true;
}

void _bstree_redistributeby3 (BSTREE *t, BSNODE *parent, int pos, BSNODE *child0, BSNODE *child1, BSNODE *child2) {
	int total = child0->count + 1 + child1->count + 1 + child2->count;
	int tmod = (total - 2) % 3;
	int portion0 = (total - 2)/3 + (tmod ? 1 : 0);
	int portion1 = (total - 2)/3 + (tmod == 2 ? 1 : 0);
	int portion2 = (total - 2)/3;

	//copia tudo
	PAIR **pairs = (PAIR**) malloc (sizeof(PAIR*) * total);
	memcpy (pairs, child0->pairs, sizeof(PAIR*), child0->count);
	pairs[child0->count] = parent->pairs[pos];
	memcpy (pairs + child0->count + 1, child1->pairs, sizeof(PAIR*), child1->count);
	pairs[child0->count + child1->count + 1] = parent->pairs[pos + 1];
	memcpy (pairs + child0->count + child1->count + 2, child2->pairs, sizeof(PAIR*), child2->count);

	//split
	child0->pairs = (PAIR**) realloc (child0->pairs, sizeof(PAIR*) * portion0);
	child1->pairs = (PAIR**) realloc (child1->pairs, sizeof(PAIR*) * portion1);
	child2->pairs = (PAIR**) realloc (child2->pairs, sizeof(PAIR*) * portion2);
	memcpy (child0->pairs, pairs, sizeof(PAIR*), portion0);
	parent->pairs[pos] = pairs[portion0];
	memcpy (child1->pairs, pairs + portion0 + 1, sizeof(PAIR*), portion1);
	parent->pairs[pos+1] = pairs[portion0 + portion1 + 1];
	memcpy (child2->pairs, pairs + portion0 + portion1 + 2, sizeof(PAIR*), portion2);

	if (child0->children)
	{
		child0->children = (size_t*) realloc (child0->children, sizeof(size_t) * (portion0 + 1));
		child1->children = (size_t*) realloc (child1->children, sizeof(size_t) * (portion1 + 1));
		child2->children = (size_t*) realloc (child2->children, sizeof(size_t) * (portion2 + 1));

		//copia tudo
		size_t *children = (size_t*) malloc (sizeof(size_t) * (total + 1));
		memcpy (children, child0->children, sizeof(size_t), child0->count + 1);
		memcpy (children + child0->count + 1, child1->children, sizeof(size_t), child1->count + 1);
		memcpy (children + child0->count + child1->count + 2, child2->children, sizeof(size_t), child2->count + 1);

		//split
		memcpy (child0->children, children, sizeof(size_t), portion0 + 1);
		memcpy (child1->children, children + portion0 + 1, sizeof(size_t), portion1 + 1);
		memcpy (child2->children, children + portion0 + portion1 + 2, sizeof(size_t), portion2 + 1);
	}

	child0->count = portion0;
	child1->count = portion1;
	child2->count = portion2;

	parent->changed = true;
	child0->changed = true;
	child1->changed = true;
	child2->changed = true;
}

void _bstree_redistributeby2 (BSTREE *t, BSNODE *parent, int pos, BSNODE *left, BSNODE *right) {
	int total = left->count + 1 + right->count;
	int middle = total / 2;
	int rhalf = middle - !(total % 2); // se o total era par, nao sobrou um para subir

	PAIR ** pairs = (PAIR**) malloc (sizeof(PAIR*) * total); //todo: verificar sucesso?
	memcpy (pairs, left->pairs, sizeof(PAIR*), left->count);
	pairs[left->count] = parent->pairs[pos];
	memcpy (pairs + left->count + 1, right->pairs, sizeof(PAIR*), right->count);

	free (left->pairs);
	left->pairs = pairs;

	right->pairs = (PAIR**) realloc (right->pairs, sizeof(PAIR*) * rhalf);
	memcpy (right->pairs, pairs + middle + 1, sizeof(PAIR*), rhalf);

	if (left->children)
	{
		size_t *children = (size_t*) malloc (sizeof(size_t) * (total + 1));
		memcpy (children, left->children, sizeof(size_t), left->count + 1);
		memcpy (children + left->count + 1, right->children, sizeof(size_t), right->count + 1);

		free (left->children);
		left->children = children;

		free (right->children);
		right->children = (size_t*) malloc (sizeof(size_t) * (rhalf + 1));
		memcpy(right->children, children + middle + 1, sizeof(size_t), rhalf + 1);
	}

	parent->pairs[pos] = pairs[middle];

	left->count = middle;
	right->count = rhalf;

	parent->changed = true;
	left->changed = true;
	right->changed = true;
}

size_t bstdelete (BSTREE *t, void *key) {
	return _bstree_delete (t, t->root, key);
}

size_t _bstree_delete (BSTREE *t, BSNODE *node, void *key) {
	int i, cmp = -1;
	size_t recoffset;
	BSNODE *child;

	for (i = 0; i < node->count && (cmp = t->compar(key, node->pairs[i]->key)) > 0; ++i);

	if (cmp == 0)
	{
		if (node->children == NULL)
			return _bstree_delete_atleaf (t, node, i);

		char chance = rand() % 2; // sorteio para nao pender a arvore
		child = _bsnode_read (t, node->children[i + chance]);
		PAIR *aux = chance ? _bstree_delete_smallest (t, child) : _bstree_delete_biggest (t, child);

		recoffset = node->pairs[i]->recoffset;
		_pair_free(node->pairs[i]);
		node->pairs[i] = aux; // substituo a chave a ser deletada pela escolhida

		i = i + chance;
		node->changed = true;
	}
	else
	{
		if (node->children == NULL)
			return ULONG_MAX;

		child = _bsnode_read(t, node->children[i]);
		recoffset = _bstree_delete(t, child, key);
	}

	if (child->count < MIN_RATE(t))
		_bstree_delete_rearrange (t, node, i, child);

	_bsnode_free(t, child);
	return recoffset;
}

PAIR* _bstree_delete_biggest (BSTREE *t, BSNODE *node) {
	if (node->children == NULL)
	{
		PAIR *pair = _pair_copy (t, node->pairs[node->count - 1]); // copio o par, pois será deletado em seguida
		_bstree_delete_atleaf (t, node, node->count - 1);
		return pair;
	}

	BSNODE *child = _bsnode_read(t, node->children[node->count]);
	PAIR *pair = _bstree_delete_biggest(t, child);

	if (child->count < MIN_RATE(t))
		_bstree_delete_rearrange (t, node, node->count, child);

	_bsnode_free(t, child);
	return pair;
}

PAIR* _bstree_delete_smallest (BSTREE *t, BSNODE *node) {
	if (node->children == NULL)
	{
		PAIR *pair = _pair_copy (t, node->pairs[0]); // copio o par, pois será deletado em seguida
		_bstree_delete_atleaf (t, node, 0);
		return pair;
	}

	BSNODE *child = _bsnode_read(t, node->children[0]);
	PAIR *pair = _bstree_delete_smallest(t, child);

	if (child->count < MIN_RATE(t))
		_bstree_delete_rearrange (t, node, 0, child);

	_bsnode_free(t, child);
	return pair;
}

PAIR* _pair_copy (BSTREE *t, PAIR *pair) {
	PAIR *cpy = (PAIR*) malloc (sizeof(PAIR));
	cpy->recoffset = pair->recoffset;
	cpy->key = malloc(t->keysize);
	memcpy (cpy->key, pair->key, 1, t->keysize);
	return cpy;
}

void _bstree_delete_rearrange (BSTREE *t, BSNODE *parent, int pos, BSNODE *child) {
	BSNODE *sibling0 = NULL;
	BSNODE *sibling1 = NULL;

	if (pos > 0)
	{
		sibling0 = _bsnode_read(t, parent->children[pos - 1]);
		if (sibling0->count > MIN_RATE(t))
		{
			_bstree_redistributeby2(t, parent, pos - 1, sibling0, child);
			_bsnode_free(t, sibling0);
			return;
		}
	}

	if (pos < parent->count)
	{
		sibling1 = _bsnode_read(t, parent->children[pos + 1]);
		if (sibling1->count > MIN_RATE(t))
		{
			_bsnode_free(t, sibling0);
			_bstree_redistributeby2(t, parent, pos, child, sibling1);
			_bsnode_free(t, sibling1);
			return;
		}
	}

	if (parent == t->root && parent->count == 1) // se tudo funciona, sao condicoes ambiguas
	{
		if (sibling0 != NULL)
			_bstree_merge_root (t, sibling0, child);
		else
			_bstree_merge_root (t, child, sibling1);
	}
	else if (sibling0 == NULL)
	{
		sibling0 = _bsnode_read(t, parent->children[pos + 2]);
		if (sibling0->count > MIN_RATE(t))
			_bstree_redistributeby3 (t, parent, pos, child, sibling1, sibling0);
		else
			_bstree_merge3into2 (t, parent, pos, child, sibling1, sibling0);
	}
	else if (sibling1 == NULL)
	{
		sibling1 = _bsnode_read(t, parent->children[pos - 2]);
		if (sibling1->count > MIN_RATE(t))
			_bstree_redistributeby3 (t, parent, pos - 2, sibling1, sibling0, child);
		else
			_bstree_merge3into2 (t, parent, pos - 2, sibling1, sibling0, child);
	}
	else
		_bstree_merge3into2 (t, parent, pos - 1, sibling0, child, sibling1);

	_bsnode_free(t, sibling0);
	_bsnode_free(t, sibling1);
}

void _bstree_merge_root (BSTREE *t, BSNODE *left, BSNODE *right) {
	int total = 2*MIN_RATE(t); // -1(underflow) +1(parent)

	t->root->pairs = (PAIR**) realloc (t->root->pairs, sizeof(PAIR*) * total);
	t->root->pairs[left->count] = t->root->pairs[0];
	memcpy (t->root->pairs, left->pairs, sizeof(PAIR*), left->count);
	memcpy (t->root->pairs + left->count + 1, right->pairs, sizeof(PAIR*), right->count);

	if (left->children)
	{
		t->root->children = (size_t*) realloc (t->root->children, sizeof(size_t) * (total + 1));
		memcpy (t->root->children, left->children, sizeof(size_t), left->count + 1);
		memcpy (t->root->children + left->count + 1, right->children, sizeof(size_t), right->count + 1);
	}
	else
	{
		free (t->root->children);
		t->root->children = NULL;
	}

	left->count = 0;
	right->count = 0;
	t->root->count = total;
	t->root->changed = true;
	t->height--;

	_bsnode_delete_atdisc(t, left);
	_bsnode_delete_atdisc(t, right);
}

void _bstree_merge3into2 (BSTREE *t, BSNODE *parent, int pos, BSNODE *child0, BSNODE *child1, BSNODE *child2) {
	int total = 3*MIN_RATE(t) + 1; // -1 +2
	int lcount = (total-1)/2 + (total-1)%2;
	int rcount = (total-1)/2;

	// copia tudo
	PAIR **pairs = (PAIR**) malloc (sizeof(PAIR*) * total);
	memcpy (pairs, child0->pairs, sizeof(PAIR*), child0->count);
	pairs[child0->count] = parent->pairs[pos];
	memcpy (pairs + child0->count + 1, child1->pairs, sizeof(PAIR*), child1->count);
	pairs[child0->count + child1->count + 1] = parent->pairs[pos + 1];
	memcpy (pairs + child0->count + child1->count + 2, child2->pairs, sizeof(PAIR*), child2->count);

	// split
	child0->pairs = realloc(child0->pairs, sizeof(PAIR*) * lcount);
	child1->pairs = realloc(child1->pairs, sizeof(PAIR*) * rcount);
	memcpy (child0->pairs, pairs, sizeof(PAIR*), lcount);
	parent->pairs[pos] = pairs [lcount];
	memcpy (child1->pairs, pairs + lcount + 1, sizeof(PAIR*), rcount);
	free (pairs);

	if (child0->children)
	{
		// copia tudo
		size_t *children = (size_t*) malloc (sizeof(size_t) * (total + 1));
		memcpy (children, child0->children, sizeof(size_t), child0->count + 1);
		memcpy (children + child0->count + 1, child1->children, sizeof(size_t), child1->count + 1);
		memcpy (children + child0->count + child1->count + 2, child2->children, sizeof(size_t), child2->count + 1);

		// split
		child0->children = realloc(child0->children, sizeof(size_t) * (lcount + 1));
		child1->children = realloc(child1->children, sizeof(size_t) * (rcount + 1));
		memcpy (child0->children, children, sizeof(size_t), lcount + 1);
		memcpy (child1->children, children + lcount + 1, sizeof(size_t), rcount + 1);
		free (children);
	}

	// ----- DOWNGRADE
	if (pos + 1 < parent->count)
	{
		memcpy (parent->pairs + pos + 1, parent->pairs + pos + 2, sizeof(PAIR*), parent->count - pos - 2);
		memcpy (parent->children + pos + 2, parent->children + pos + 3, sizeof(size_t), parent->count - pos - 2);
	}

	parent->count--;
	child0->count = lcount;
	child1->count = rcount;
	child2->count = 0;

	parent->changed = true;
	child0->changed = true;
	child1->changed = true;
	_bsnode_delete_atdisc(t, child2);
}

size_t _bstree_delete_atleaf (BSTREE *t, BSNODE *node, int pos) {
	size_t recoffset = node->pairs[pos]->recoffset;
	_pair_free(node->pairs[pos]);

	if (pos < node->count - 1) // se nao for o ultimo, shift para esq. dos proximos
		memcpy (node->pairs + pos, node->pairs + pos + 1, sizeof(PAIR*), node->count - pos - 1);

	node->count--;
	node->changed = true;

	t->count--;
	return recoffset; // soh volta, quem chamou a funcao trata o underflow - como no insert
}

bool bstinsert (BSTREE *t, void *key, size_t recoffset) {
	PAIR *pair = (PAIR*) malloc(sizeof(PAIR));
	pair->key = (void*) malloc(t->keysize);
	memcpy (pair->key, key, 1, t->keysize);
	pair->recoffset = recoffset;

	if (_bstree_insert (t, t->root, pair) ) //recursao down-up
	{
		if (t->root->count > 2*MAX_RATE(t))
			_bstree_splitroot (t);

		return true;
	}

	_pair_free (pair);
	return false;
}

bool _bstree_insert (BSTREE *t, BSNODE *node, PAIR* pair) {
	int i;
	int cmp = -1;

	for (i = 0; i < node->count && (cmp = t->compar(pair->key, node->pairs[i]->key)) > 0; ++i);

	if (cmp == 0)
		return false;

	if (node->children != NULL) // busca o noh folha
	{
		BSNODE *child = _bsnode_read (t, node->children[i]);
		bool ret = _bstree_insert(t, child, pair);

		if (child->count > MAX_RATE(t))			// verifico se houve overflow no filho
			_bstree_insert_rearrange(t, node, i, child); // solucionar o overflow requer referencia ao pai
		// essa abordagem tbm permite tratar a raiz exclusivo
		_bsnode_free(t, child);
		return ret;
	}

	node->pairs = (PAIR**) realloc (node->pairs, sizeof(PAIR*) * (node->count + 1)); //todo: verificar sucesso

	if (i != node->count)
		memcpy(node->pairs + i + 1, node->pairs + i, sizeof(PAIR*), node->count - i);

	node->pairs[i] = pair;
	node->count++;
	node->changed = true;

	t->count++;
	return true;
}

void _bstree_insert_rearrange (BSTREE *t, BSNODE *parent, int pos, BSNODE *child) {
	BSNODE *sibling0 = NULL;
	BSNODE *sibling1 = NULL;

	if (pos > 0)
	{
		sibling0 = _bsnode_read(t, parent->children[pos - 1]);
		if ( sibling0->count < MAX_RATE(t) )
		{
			_bstree_redistributeby2(t, parent, pos - 1, sibling0, child);
			_bsnode_free(t, sibling0);
			return;
		}
	}

	if (pos < parent->count)
	{
		sibling1 = _bsnode_read(t, parent->children[pos + 1]);
		if ( sibling1->count < MAX_RATE(t) )
		{
			_bsnode_free(t, sibling0);
			_bstree_redistributeby2(t, parent, pos, child, sibling1);
			_bsnode_free(t, sibling1);
			return;
		}
	}

	if (rand() % 2) // nao priorizo nem a sub aa esq., nem aa dir., mas, sim, sorteio
	{				// se impar, prioriza a esq.
		if (sibling0)
			_bstree_splitchild2to3(t, parent, pos - 1, sibling0, child);
		else
			_bstree_splitchild2to3(t, parent, pos, child, sibling1);
	}
	else
	{
		if (sibling1)
			_bstree_splitchild2to3(t, parent, pos, child, sibling1);
		else
			_bstree_splitchild2to3(t, parent, pos - 1, sibling0, child);
	}

	_bsnode_free(t, sibling0);
	_bsnode_free(t, sibling1);
}

void _bstree_splitchild2to3 (BSTREE *t, BSNODE *parent, int childPos, BSNODE *left, BSNODE *right) {
	BSNODE *nchild = _bsnode_new();
	int total = 2 * MAX_RATE(t) + 2; // +1(overflow) +1(parent); 3*(2*n/3) + 2 = 2*n + 2
	int tmod = 2 * MAX_RATE(t) % 3;
	int count0 = MIN_RATE(t) + (tmod ? 1 : 0); // (2*n)/3 == MIN
	int count1 = MIN_RATE(t) + (tmod == 2 ? 1 : 0);
	int count2 = MIN_RATE(t);

	//copia tudo
	PAIR **pairs = (PAIR**) malloc( sizeof(PAIR*) * total);
	memcpy (pairs, left->pairs, sizeof(PAIR*), left->count);
	pairs[left->count] = parent->pairs[childPos];
	memcpy (pairs + left->count + 1, right->pairs, sizeof(PAIR*), right->count);

	// ----- SPLIT
	//left esta pronto e cabe, basta mudar o count
	memcpy(right->pairs, pairs + count0 + 1, sizeof(PAIR*), count1); // right cabe, mudar count
	nchild->pairs = (PAIR**) malloc (sizeof(PAIR*) * count2);
	memcpy(nchild->pairs, pairs + count0 + count1 + 2, sizeof(PAIR*), count2);

	if (left->children)
	{
		size_t *children = (size_t*) malloc( sizeof(size_t) * (total + 1) );
		memcpy(children, left->children, sizeof(size_t), left->count + 1);
		memcpy(children + left->count + 1, right->children, sizeof(size_t), right->count + 1);

		// left esta pronto
		memcpy (right->children, children + count0 + 1, sizeof(size_t), count1 + 1);
		nchild->children = (size_t*) malloc (sizeof(size_t) * (count2 + 1));
		memcpy(nchild->children, children + count0 + count1 + 2, sizeof(size_t), count2 + 1);
		free (children);
	}

	left->count = count0;
	right->count = count1;
	nchild->count = count2;

	// ----- PROMOTE
	parent->pairs = (PAIR**) realloc(parent->pairs, sizeof(PAIR*) * (parent->count + 1));
	parent->children = (size_t*) realloc (parent->children, sizeof(size_t) * (parent->count + 2));

	if (childPos < parent->count - 1) // se ha elementos a serem empurrados
	{
		memcpy (parent->pairs + childPos + 2, parent->pairs + childPos + 1,
				sizeof(PAIR*), parent->count - (childPos + 1));
		memcpy (parent->children + childPos + 3, parent->children + childPos + 2,
				sizeof(size_t), (parent->count + 1) - (childPos + 2));
	}

	parent->pairs[childPos] = pairs [count0];
	parent->pairs[childPos + 1] = pairs [count0 + 1 + count1];
	parent->children[childPos + 2] = _bsnode_create(t, nchild);

	parent->count++;

	// FINALIZACAO
	free(pairs);
	parent->changed = true;
	left->changed = true;
	right->changed = true;
	_bsnode_free(t, nchild); // ja foi salvo ao criar
}

void _bstree_splitroot (BSTREE *t) {
	BSNODE *left = _bsnode_new();
	BSNODE *right = _bsnode_new();

	left->pairs = (PAIR**) malloc (sizeof(PAIR*) * MAX_RATE(t));
	right->pairs = (PAIR**) malloc (sizeof(PAIR*) * MAX_RATE(t));
	left->count = MAX_RATE(t);
	right->count = MAX_RATE(t);

	int otherHalf = MAX_RATE(t) + 1;
	memcpy (left->pairs, t->root->pairs, sizeof(PAIR*), MAX_RATE(t));
	memcpy (right->pairs, t->root->pairs + otherHalf, sizeof(void*), MAX_RATE(t));

	if (t->root->children)
	{
		left->children = (size_t*) malloc (sizeof(size_t) * (MAX_RATE(t) + 1));
		right->children = (size_t*) malloc (sizeof(size_t) * (MAX_RATE(t) + 1));
		memcpy(left->children, t->root->children, sizeof(size_t), MAX_RATE(t) + 1);
		memcpy(right->children, t->root->children + otherHalf, sizeof(size_t), MAX_RATE(t) + 1);
	}

	t->root->pairs[0] = t->root->pairs [MAX_RATE(t)];
	t->root->pairs = (PAIR**) realloc (t->root->pairs, sizeof(PAIR*));
	t->root->children = (size_t*) realloc(t->root->children, sizeof(size_t) * 2);
	t->root->children[0] = _bsnode_create(t, left);
	t->root->children[1] = _bsnode_create(t, right);
	t->root->count = 1;
	t->root->changed = true;

	t->height++;
}

size_t _bstree_writeheader (BSTREE* t) {
	size_t written = 0, aux;
	rewind(t->file);

	trytowrite(aux, &t->height, sizeof(int), 1, t->file, written);
	trytowrite(aux, &t->count, sizeof(unsigned long), 1, t->file, written);
	trytowrite(aux, &t->keysize, sizeof(size_t), 1, t->file, written);
	trytowrite(aux, &t->blocksize, sizeof(size_t), 1, t->file, written);
	trytowrite(aux, &t->trashstack, sizeof(size_t), 1, t->file, written);

	return written;
}

bool _bstree_readheader (BSTREE* t) {
	rewind(t->file);

	trytoread(&t->height, sizeof(int), 1, t->file);
	trytoread(&t->count, sizeof(unsigned long), 1, t->file);
	trytoread(&t->keysize, sizeof(size_t), 1, t->file);
	trytoread(&t->blocksize, sizeof(size_t), 1, t->file);
	trytoread(&t->trashstack, sizeof(size_t), 1, t->file);

	return true;
}


int bstcalcdegree (size_t bloco, size_t keysize) {
	//bloco = int.count + ( (degree - 1) * elemento + degree * size_t.children )
	//bloco = int.count + degree*(elemento + size_t.children) - elemento
	//degree = (bloco - int.count + elemento) / (elemento + size_t.children)

	size_t elemento = keysize + sizeof(size_t);
	return (int) (bloco - sizeof(int) + elemento) / (elemento + sizeof(size_t));
}

size_t _bstree_search(BSTREE *t, BSNODE *node, void* key) {
	int i, cmp = 0;

	for (i = 0; i < node->count && (cmp = t->compar(key, node->pairs[i]->key)) > 0; ++i); // procura a chave ou a possivel sub-arvore

	if (cmp == 0) // se achou
		return node->pairs[i]->recoffset;

	if (node->children == NULL) // se nao encontrou
		return ULONG_MAX;

	BSNODE *child = _bsnode_read (t, node->children[i]);
	size_t recoffset = _bstree_search (t, child, key);
	_bsnode_free(t, child);

	return recoffset;
}

BSNODE* _bsnode_read (BSTREE *t, size_t offset) {
	BSNODE *n = (BSNODE*) malloc (sizeof(BSNODE));
	n->changed = false;
	n->offset = offset;

	fseek (t->file, offset, SEEK_SET);
	if (fread (&n->count, sizeof(unsigned int), 1, t->file) != 1) // todo: falta liberar BSNODE
		return NULL;

	n->pairs = (PAIR**) malloc (sizeof(PAIR*) * n->count);

	int i;
	for (i = 0; i < n->count; ++i)
	{
		PAIR *pair = (PAIR*) malloc (sizeof(PAIR));
		pair->key = (void*) malloc (t->keysize);

		if (fread(pair->key, t->keysize, 1, t->file) != 1) // falta liberar pairs/BSNODE
			return NULL;

		if (fread(&pair->recoffset, sizeof(size_t), 1, t->file) != 1) // falta liberar pairs/BSNODE
			return NULL;

		n->pairs[i] = pair;
	}

	size_t aux;
	if (fread (&aux, sizeof(size_t), 1, t->file) != 1) // falta liberar pairs/BSNODE
		return NULL;

	if (aux != 0) //ha filhos
	{
		n->children = (size_t*) malloc (sizeof(size_t) * (n->count+1));
		n->children[0] = aux;

		for (i = 1; i < n->count+1; ++i)
			if (fread (&n->children[i], sizeof(size_t), 1, t->file) != 1) // falta liberar children/pairs/BSNODE
				return NULL;
	}
	else
		n->children = NULL;

	return n;
}

size_t _bsnode_write (BSTREE *t, BSNODE *n) {
	size_t written = 0;

	if (fwrite (&n->count, sizeof(unsigned int), 1, t->file) != 1) // todo: rollback ?
		return 0;

	int i;
	for (i = 0; i < n->count; ++i)
	{
		PAIR *pair = n->pairs[i];

		if (fwrite (pair->key, t->keysize, 1, t->file) != 1) // falta liberar pairs/BSNODE
			return 0;

		if (fwrite (&pair->recoffset, sizeof(size_t), 1, t->file) != 1) // falta liberar pairs/BSNODE
			return 0;
	}

	if (n->children != NULL)
	{
		for (i = 0; i < n->count+1; ++i)
			if (fwrite (&n->children[i], sizeof(size_t), 1, t->file) != 1)
				return 0;

		written = (n->count+1) * sizeof(size_t); // no. filhos * sizeof(size_t)
	}
	else //sinaliza que nao a filhos
		if (fwrite(&written, sizeof(size_t), 1, t->file) != 1) // print size_t = 0
			return 0;
		else
			written = sizeof(size_t);

	written += (t->keysize + sizeof(size_t))*n->count // no. chaves * (tamanho da chave + size_t)
	+ sizeof(int); // "cabecalho" de quantidade = sizeof(int)

	n->changed = false;
	return written;
}

size_t _bsnode_create_root (BSTREE *t, BSNODE *n) {
	if ( !_bsnode_create(t, n) )
		return 0;

	size_t i;
	for (i = 0; i < t->blocksize; ++i)
		if (fputc (0, t->file) == EOF) // aloca mais um bloco
			return 0;

	return n->offset;
}

size_t _bsnode_create (BSTREE *t, BSNODE *n) {
	size_t aux;

	if (t->trashstack)
	{
		fseek(t->file, t->trashstack, SEEK_SET);
		n->offset = t->trashstack;

		fread (&t->trashstack, sizeof(size_t), 1, t->file);
		fseek(t->file, -sizeof(size_t), SEEK_CUR);

		if (_bsnode_write (t, n) == 0)
			return 0;
	}
	else
	{
		fseek (t->file, 0, SEEK_END);
		n->offset = ftell (t->file);

		if ((aux = _bsnode_write (t, n)) == 0)
			return 0;

		for (; aux < t->blocksize; ++aux)
			if (fputc (0, t->file) == EOF) // aloca restante do bloco para o noh
				return 0; //todo: rollback?
	}

	n->changed = false;
	return n->offset;
}

bool _bsnode_free (BSTREE *t, BSNODE *n) {
	if (!n)
		return true;

	if (n->changed)
	{
		fseek (t->file, n->offset, SEEK_SET);
		if ( !_bsnode_write (t, n) )
			return false;
	}

	int i;
	for (i = 0; i < n-> count; ++i)
		_pair_free (n->pairs[i]);

	free (n->pairs);
	free (n->children);
	free (n);
	return true;
}

void _pair_free (PAIR *pair) {
	if (!pair)
		return;

	free (pair->key);
	free (pair);
}

BSNODE* _bsnode_new () {
	BSNODE *n = (BSNODE*) malloc (sizeof(BSNODE));

	n->offset = 0;
	n->count = 0;
	n->pairs = NULL;
	n->children = NULL;
	n->changed = false;

	return n;
}

void _bsnode_delete_atdisc (BSTREE *t, BSNODE *node) {
	fseek(t->file, node->offset, SEEK_SET);
	fwrite(&t->trashstack, sizeof(size_t), 1, t->file);
	t->trashstack = node->offset;
	node->changed = false;
}

size_t bstsearch (BSTREE *t, void *key) {
	return _bstree_search(t, t->root, key);
}

void _dfs (BSTREE *t, BSNODE *n, runkey_declare(pre), runkey_declare(in), runkey_declare(pos), va_list args) {
	int i;

	for (i = 0; i < n->count; ++i)
	{
		if (pre)
		{
			va_list copy;
			va_copy (copy, args);
			pre (n->pairs[i]->key, n->pairs[i]->recoffset, copy);
			va_end (copy);
		}

		if (n->children != NULL)
		{
			BSNODE *child = _bsnode_read (t, n->children[i]);
			_dfs (t, child, pre, in, pos, args);
			_bsnode_free (t, child);
		}

		if (in)
		{
			va_list copy;
			va_copy (copy, args);
			in (n->pairs[i]->key, n->pairs[i]->recoffset, copy);
			va_end (copy);
		}
	}

	if (n->children != NULL)
	{
		BSNODE *child = _bsnode_read (t, n->children[i]);
		_dfs (t, child, pre, in, pos, args);
		_bsnode_free (t, child);
	}

	if (pos)
	{
		for (i = n->count - 1; i >= 0; --i)
		{
			va_list copy;
			va_copy (copy, args);
			pos (n->pairs[i]->key, n->pairs[i]->recoffset, copy);
			va_end (copy);
		}
	}
}

void __bstree_debug_dfs (BSTREE *t, runkey_declare(pre), runkey_declare(in), runkey_declare(pos), ...) {
	va_list args;
	BSNODE *n;

	va_start (args, pos);
	n = t->root;
	_dfs (t, n, pre, in, pos, args);
	va_end (args);
}

void _printtree_runnode(BSNODE *node, int level, printkey_declare(runkey))
{
	int i;
	for (i = 0; i < node->count-1; ++i)
	{
		runkey (node->pairs[i]->key);
		putchar(' ');
	}

	runkey (node->pairs[i]->key);
	putchar('\n');
}

void __bstree_debug_printtree (BSTREE *t, printkey_declare(runkey)) {
	size_t *queue = NULL;
	int qsize = 0;
	int qcur = 0;
	int curlevel = 1;
	int i;

	_printtree_runnode(t->root, 1, runkey);

	if (t->root->children)
	{
		qsize = 2 * (t->root->count + 1);
		queue = (size_t*) malloc (sizeof(size_t) * qsize);
		for (i = 0; i <= t->root->count; i++)
		{
			queue[i*2] = t->root->children[i];
			queue [(i*2)+1] = 2; // altura do nó, na arvore - de cima para baixo
		}
	}

	while (qcur < qsize)
	{
		BSNODE *node = _bsnode_read(t, queue[qcur]);
		int level = (int) queue [qcur + 1];
		qcur += 2;

		if (level > curlevel)
		{
			curlevel = level;
			putchar('\n');
		}

		_printtree_runnode(node, level, runkey);

		if (node->children)
		{
			queue = (size_t*) realloc (queue, sizeof(size_t) * (qsize + 2*(node->count + 1)));
			for (i = 0; i <= node->count; ++i)
			{
				queue[qsize + i*2] = node->children[i];
				queue[qsize + i*2 + 1] = level + 1;
			}
			
			qsize += 2* (node->count + 1);
		}
		
		_bsnode_free(t, node);
	}
	
	//	putchar('\n');
	free (queue);
}