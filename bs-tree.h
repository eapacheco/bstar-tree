/*
	Desenvolvido por Eduardo A. Pacheco
	"Porque dele, e por meio dele, e para ele sao todas as coisas.
	A ele, pois, a gloria eternamente. Amem!" (Romanos 11:36 ARA)

	21 de junho de 2016, Brasil

	Copyright © 2016 Eduardo Pacheco. All rights reserved.

	------------------------------------------

	bs-tree.h
	Definição formal
	O indice arvore-B* com ordem m e definido formalmente como descrito a seguir.
	1.	Cada pagina (ou no) do indice arvore-B* deve ser, pelo menos, da seguinte forma:
		< P1, <C1,PR1>, P2, <C2,PR2>, ..., Pq-1, <Cq-1,PRq-1>, Pq>, onde (q <= m) e
		Cada Ci (1 <= i <- q-1) e uma chave de busca.
		Cada PRi (1 ≤ i ≤ q-1) e um campo de referencia para o registro no arquivo de
		 dados que contem o registro de dados correspondente a Ci.
		Cada Pj (1 <= j <= q) e um ponteiro para uma subarvore ou assume o valor nulo
		 caso nao exista subarvore.
	2.	Dentro de cada pagina
		C1 < C2 < ... < Cq-1.
	3.	Para todos os valores X da chave na subarvore apontada por Pi,
		em relacao aa pagina de Pi:
		Ci-1 < X < Ci para 1 < i < q
		X < Ci para i = 1
		Ci-1 < X para i = q.
	4.	Cada pagina possui um maximo de m descendentes.
	5.	Cada pagina, exceto a raiz, possui no minimo (2m-1)/3 descendentes
	6.	A raiz possui pelo menos 2 descendentes, a menos que seja um no folha.
	7.	Todas as folhas aparecem no mesmo nivel/altura.
	8.	Uma pagina nao folha com k descendentes possui k-1 chaves.
	9.	Uma pagina folha possui no minimo ⎣2(m-1)/3⎦ chaves e no maximo m-1 chaves (taxa de ocupação).
	10. A raiz e peculiar
		Dobro do tamanho: tem no maximo 2m-2 chaves, 2m-1 descendentes
		Nao tem limite para tamanho minimo, podendo conter ate zero descendetes

	Para mais informacoes, especificas a essa implementacao, veja
	o .pdf que acompanha esse TAD: https://github.com/eapacheco/bstar-tree (PORTUGUESE ONLY)

	Para mais informacoes gerais:
	https://pt.wikipedia.org/wiki/%C3%81rvore_B
	https://pt.wikipedia.org/wiki/%C3%81rvore_B*
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

//funcao que compara a chave da esquerda com outra
//retorna 0, se sao iguais;
//		< 0, se for menor;
//		> 0, se for maior.
#define compare_type int(*)(void*,void*)
#define compare_declare(f) int(*f)(void*,void*)

//funcao que recebe a chave, o offset do registro referente, e uma lista de argumentos
//esta lista sao os parametros inviados pela aplicacao, inalterados,
//veja __bstree_debug_dfs
#define runkey_type void(*)(void*,size_t,va_list)
#define runkey_declare(f) void(*f)(void*,size_t,va_list)

//funcao que imprime um chave, em formato texto, na saida padrao
//veja __bstree_debug_printtree
#define printkey_type void(*)(void*)
#define printkey_declare(f) void(*f)(void*)

//tamanho minimo a ser ocupada pelo cabecalho da arvore
//o cabecalho ocupara, na verdade, o tamanho do bloco especificado
//portanto, esse tamanho define o tamanho minimo do bloco
#define HEADER_SIZE 36

//remove o arquivo presente no caminho indicado
#define bstdrop(path) remove(path)

//instancia representando uma Arvore-B*
//B Star Tree
typedef struct tree BSTREE;

//cria um novo arquivo, no path indicado, e inicializa uma BSTREE nesse arquivo.
//parametros
//	indexpath	caminho do arquivo que sera utilizado como indice
//	blocksize	tamanho dos blocos, em bytes, a serem considerados pelo indice - tamanho do noh da arvore
//				precisa ser maior que, ou igual, a HEADER_SIZE
//	keysize		tamanho das chaves do indice, em bytes
//	func 		funcao que compara as chaves
//retorno
//	instancia de BSTREE que representa o indice fisico
//	NULL, se falhar
BSTREE* bstcreate (char *indexpath, size_t blocksize, size_t keysize, compare_type);

//abre um arquivo indice existente, no path indicado, e carrega o cabecalho e a raiz da
//BSTREE contida.
//parametros
//	indexpath	caminho do arquivo contendo o indice
//	func 		funcao que compara uma chave com os elementos
//				ATENCAO: deve ser equivalente aa que foi utilizada na criacao do indice
//				senao, o comportamento eh indefinido
//retorno
//	instancia de BSTREE que manipula o indice fisico
BSTREE* bstopen (char *indexpath, compare_type);

//persiste os dados do cabecalho e da raiz no disco
//libera a memoria alocada para a arvore
//parametros
//	t 			arvore
//retorno
//	se foi possivel fechar o indice
//	se sim, a memoria apontada por t foi liberada
bool bstclose (BSTREE *t);

//acessa o tamanho do bloco especificado para essa arvore
size_t bstgetblocksize (BSTREE*);

//acessa o tamanho dos chaves armazenados
size_t bstgetkeysize (BSTREE*);

//acessa a altura da arvore
//a arvore que so contem a raiz possui altura 1
int bstgetheight (BSTREE*);

//acessa a quantidade de chaves armazenados na arvore
unsigned long bstgetcount (BSTREE*);

//acessa o grau da arvore
int bstgetdegree (BSTREE*);

//calcula o grau de uma arvore com os dados informados
//parametro
//	blocksize	tamanho dos blocos de disco disponibilizados para cada noh
//	keysize		tamanho da chave a ser armazenada no noh
//retorno
//	o grau correspondente aos parametros
//	dada a seguinte extrutura para o noh:
//		int count 						4 bytes
//		void *keys [grau*2 -1]			keysize * (grau*2 - 1)
//		size_t offset [grau*2 -1]		8 bytes * (grau*2 - 1)
//		size_t filhos [degree*2] 		8 bytes * degree*2
int bstcalcdegree (size_t blocksize, size_t keysize);

//insere um novo par chave/offset na arvore
//parametros
//	t 			arvore
//	key 		chave a ser inserida
//				essa chave sera escrita na integra no disco.
//				portanto, não deve referenciar endereços de memória.
//	recoffset	offset do registro referenciado
//retorno
//	se foi inserida com sucesso, retorna a quantidade de elementos na arvore
//	senao, false
bool bstinsert (BSTREE *t, void *key, size_t recoffset);

//deleta um elementos da arvore
//parametros
//	t			arvore
//	key			chave de busca do elemento
//retorno
//	offset referente a chave excluida, se encontrou
//	senao, ULONG_MAX (limits.h)
size_t bstdelete (BSTREE *t, void *key);

//busca por um elemento atraves da chave
//parametros
//	t 			arvore
//	key 		chave de busca
//retorno
//	offset correspondente aa chave, se encontrar
//	senao, ULONG_MAX (limits.h)
size_t bstsearch (BSTREE *t, void *key);

//percorre todos as chaves da arvore por uma dfs
//parametros
//	arvore
//	funcao para acesso em preordem; pode ser NULL
//	funcao para acesso em inordem; pode ser NULL
//	funcao para acesso em posordem; pode ser NULL
//	parametros para serem repassados aas funcoes de acesso
void __bstree_debug_dfs (BSTREE*, runkey_type, runkey_type, runkey_type, ...);

//imprime, em modo texto, na saida padrao, o seguinte esquema:
//entre cada chave de um mesmo no ha um espaco ' ';
//ao final de um noh ha um quebra de linha '\n';
//ao final de um nivel da arvore a uma segunda quebra de linha '\n',
//gerando uma linha inteiramante branca entre dois niveis.
//exemplo (os comentarios "--" nao estao presentes)
//F			-- RAIZ
//
//A B		-- FILHO 0 DA RAIZ
//I J		-- FILHO 1 DA RAIZ
//parametros
//	arvore
//	funcao que imprime, em modo texto, na saida padrao, a chave
void __bstree_debug_printtree (BSTREE*, printkey_type);


#endif