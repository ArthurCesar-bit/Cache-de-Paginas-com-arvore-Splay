/* ===========================================================
 * Etapa 1 — Árvore Binária de Busca (BST) básica
 * ===========================================================
 * Objetivo: estrutura mínima funcional, que servirá de base
 * para as próximas etapas (rotações, splay, cache de páginas).
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>

typedef struct No {
    int chave;
    struct No *esq;
    struct No *dir;
} No;

/* ---------------------------------------------------------
 * Criar nó
 * --------------------------------------------------------- */
No *criarNo(int chave) {
    No *novo = (No *) malloc(sizeof(No));
    if (novo == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para o no.\n");
        exit(EXIT_FAILURE);
    }
    novo->chave = chave;
    novo->esq = NULL;
    novo->dir = NULL;
    return novo;
}

/* ---------------------------------------------------------
 * Inserir
 * Insercao recursiva classica de BST (sem balanceamento).
 * Retorna a raiz (possivelmente nova) da subarvore.
 * --------------------------------------------------------- */
No *inserir(No *raiz, int chave) {
    if (raiz == NULL) {
        return criarNo(chave);
    }

    if (chave < raiz->chave) {
        raiz->esq = inserir(raiz->esq, chave);
    } else if (chave > raiz->chave) {
        raiz->dir = inserir(raiz->dir, chave);
    }
    /* chave == raiz->chave: nao insere duplicado */

    return raiz;
}

/* ---------------------------------------------------------
 * Buscar
 * Retorna o ponteiro para o no encontrado, ou NULL se nao existir.
 * --------------------------------------------------------- */
No *buscar(No *raiz, int chave) {
    if (raiz == NULL || raiz->chave == chave) {
        return raiz;
    }

    if (chave < raiz->chave) {
        return buscar(raiz->esq, chave);
    } else {
        return buscar(raiz->dir, chave);
    }
}

/* ---------------------------------------------------------
 * Percurso em ordem (in-order)
 * Para uma BST, imprime as chaves em ordem crescente.
 * --------------------------------------------------------- */
void emOrdem(No *raiz) {
    if (raiz == NULL) {
        return;
    }
    emOrdem(raiz->esq);
    printf("%d ", raiz->chave);
    emOrdem(raiz->dir);
}

/* ---------------------------------------------------------
 * Liberar memória
 * Libera todos os nos da arvore (pos-ordem).
 * --------------------------------------------------------- */
void liberarArvore(No *raiz) {
    if (raiz == NULL) {
        return;
    }
    liberarArvore(raiz->esq);
    liberarArvore(raiz->dir);
    free(raiz);
}

/* ---------------------------------------------------------
 * Testes
 * --------------------------------------------------------- */
int main(void) {
    No *raiz = NULL;

    int valores[] = {50, 30, 70, 20, 40, 60, 80};
    int n = sizeof(valores) / sizeof(valores[0]);

    printf("Inserindo valores: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", valores[i]);
        raiz = inserir(raiz, valores[i]);
    }
    printf("\n\n");

    printf("Percurso em ordem (deve sair crescente): ");
    emOrdem(raiz);
    printf("\n\n");

    /* Testes de busca */
    int alvos[] = {40, 80, 100};
    int m = sizeof(alvos) / sizeof(alvos[0]);
    for (int i = 0; i < m; i++) {
        No *resultado = buscar(raiz, alvos[i]);
        if (resultado != NULL) {
            printf("Busca por %d: ENCONTRADO (chave = %d)\n", alvos[i], resultado->chave);
        } else {
            printf("Busca por %d: NAO ENCONTRADO\n", alvos[i]);
        }
    }

    printf("\nLiberando memoria da arvore...\n");
    liberarArvore(raiz);
    raiz = NULL;
    printf("Memoria liberada com sucesso.\n");

    return 0;
}
