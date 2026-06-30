/* ===========================================================
 * bst.c — Árvore Splay + Cache de Páginas (protótipo monolítico)
 * ===========================================================
 * Arquivo único consolidando TODAS as etapas do roteiro do
 * DIARIO.md (Etapas 1 a 15). Reúne, em ordem de construção:
 *
 *   1.  BST básica (criarNo, inserir, buscar, emOrdem)
 *   2.  Ponteiro para o pai (campo ->raiz)
 *   3-7. Rotações + casos Splay (Zig, Zig-Zig, Zig-Zag) e
 *        o algoritmo splay() completo
 *   8-9. buscarSplay / inserirSplay
 *   10. removerSplay (splay da chave, remove a raiz, une subárvores)
 *   11. Estatísticas (rotações, profundidade média, hit/miss)
 *   12. Estrutura Pagina (id + dados[256])
 *   13. Cache de páginas (criarCache/buscar/inserir/remover/liberar)
 *   14. Localidade temporal (printArvore para inspeção visual)
 *   15. Políticas de substituição (LRU por profundidade, LFU por
 *        frequência, Híbrida) + benchmark comparativo
 *
 * Substitui os snapshots incrementais "bst.c (N)" que antes
 * existiam nesta pasta — esta é a versão final acumulada.
 * Compila limpo: gcc -std=c11 -Wall -Wextra -Werror bst.c -lm
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* ---------------------------------------------------------
 * Pagina — unidade de dados armazenada em cada no da arvore.
 * A partir desta etapa, o cache nao guarda mais apenas um
 * inteiro solto: guarda uma "pagina" completa, identificada
 * por um id (usado para ordenacao/busca na BST) e um bloco de
 * dados de tamanho fixo.
 * --------------------------------------------------------- */
typedef struct Pagina {
    int id;
    char dados[256];
} Pagina;

typedef struct No {
    Pagina pagina;
    int    acessos;    /* contador de acessos a este no (para politica LFU) */
    struct No *esq;
    struct No *dir;
    struct No *pai;   /* ponteiro para o pai deste no (antes chamado "raiz") */
} No;

/* ---------------------------------------------------------
 * Estatisticas de desempenho
 *
 * Usamos uma variavel global "stats" para nao precisar mudar
 * a assinatura de funcoes ja existentes (rotacoes, buscarSplay,
 * etc). As funcoes que afetam cada metrica incrementam os
 * campos diretamente:
 *   - buscas:    incrementado em cada chamada de buscarSplay
 *   - rotacoes:  incrementado dentro de rotacaoDireita/Esquerda
 *   - hits:      buscarSplay encontrou a chave
 *   - misses:    buscarSplay NAO encontrou a chave
 *
 * tempoTotalAcesso (em segundos) e medido externamente, em
 * torno das chamadas de buscarSplay, usando clock().
 * --------------------------------------------------------- */
typedef struct {
    long buscas;
    long rotacoes;
    long hits;
    long misses;
} Estatisticas;

Estatisticas stats = {0, 0, 0, 0};
double tempoTotalAcesso = 0.0; /* em segundos, acumulado entre buscas medidas */

/* zera todas as estatisticas (util entre rodadas de teste) */
void resetEstatisticas(void) {
    stats.buscas = 0;
    stats.rotacoes = 0;
    stats.hits = 0;
    stats.misses = 0;
    tempoTotalAcesso = 0.0;
}

/* ---------------------------------------------------------
 * Criar nó
 *
 * Recebe o id da pagina e seus dados (uma string de at[e] 255
 * caracteres + terminador). Se "dados" for NULL, o bloco de
 * dados fica vazio (todos os bytes zerados).
 * --------------------------------------------------------- */
No *criarNo(int id, const char *dados) {
    No *novo = (No *) malloc(sizeof(No));
    if (novo == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para o no.\n");
        exit(EXIT_FAILURE);
    }
    novo->pagina.id = id;
    novo->acessos = 0;
    if (dados != NULL) {
        /* copia no maximo 255 caracteres + terminador nulo, para
           nunca escrever fora dos limites do vetor de 256 bytes */
        snprintf(novo->pagina.dados, sizeof(novo->pagina.dados), "%s", dados);
    } else {
        novo->pagina.dados[0] = '\0';
    }
    novo->esq = NULL;
    novo->dir = NULL;
    novo->pai = NULL;
    return novo;
}

/* ---------------------------------------------------------
 * Inserir
 * --------------------------------------------------------- */
No *inserir(No *no, int id, const char *dados) {
    if (no == NULL) {
        return criarNo(id, dados);
    }

    if (id < no->pagina.id) {
        no->esq = inserir(no->esq, id, dados);
        no->esq->pai = no;
    } else if (id > no->pagina.id) {
        no->dir = inserir(no->dir, id, dados);
        no->dir->pai = no;
    }

    return no;
}

/* ---------------------------------------------------------
 * Buscar
 * --------------------------------------------------------- */
No *buscar(No *no, int id) {
    if (no == NULL || no->pagina.id == id) {
        return no;
    }

    if (id < no->pagina.id) {
        return buscar(no->esq, id);
    } else {
        return buscar(no->dir, id);
    }
}

/* ---------------------------------------------------------
 * Liberar memória
 * (movida para perto do topo do arquivo, pois liberarCache
 * precisa dela e e definida antes do bloco de Splay/Cache)
 * --------------------------------------------------------- */
void liberarArvore(No *no) {
    if (no == NULL) {
        return;
    }
    liberarArvore(no->esq);
    liberarArvore(no->dir);
    free(no);
}

/* (a remocao agora e implementada via removerSplay, mais abaixo,
   apos as rotacoes e o splay, ja que depende deles) */

/* ---------------------------------------------------------
 * Rotação à direita em torno de x
 *
 *      x                y
 *     /                  \
 *    y        ===>         x
 *
 * Pre-condicao: x != NULL e x->esq != NULL.
 * Retorna y (novo topo da subarvore).
 * --------------------------------------------------------- */
No *rotacaoDireita(No *x) {
    if (x == NULL || x->esq == NULL) {
        /* rotacao invalida: nao ha filho esquerdo para subir */
        return x;
    }

    stats.rotacoes++;

    No *y = x->esq;
    No *pai = x->pai;     /* pai original de x, antes da rotacao */
    No *B   = y->dir;      /* subarvore que muda de "filho de y" para "filho de x" */

    /* y assume o lugar de x */
    y->dir = x;
    x->pai = y;

    /* B (antiga subarvore direita de y) passa a ser a subarvore esquerda de x */
    x->esq = B;
    if (B != NULL) {
        B->pai = x;
    }

    /* y assume o pai que x tinha antes */
    y->pai = pai;
    if (pai != NULL) {
        if (pai->esq == x) {
            pai->esq = y;
        } else if (pai->dir == x) {
            pai->dir = y;
        }
    }
    /* se pai == NULL, x era a raiz global; quem chamou deve
       atualizar sua variavel raiz para o valor retornado (y) */

    return y;
}

/* ---------------------------------------------------------
 * Rotação à esquerda em torno de x
 *
 *    x                    y
 *     \                  /
 *      y      ===>      x
 *
 * Pre-condicao: x != NULL e x->dir != NULL.
 * Retorna y (novo topo da subarvore).
 * --------------------------------------------------------- */
No *rotacaoEsquerda(No *x) {
    if (x == NULL || x->dir == NULL) {
        /* rotacao invalida: nao ha filho direito para subir */
        return x;
    }

    stats.rotacoes++;

    No *y = x->dir;
    No *pai = x->pai;     /* pai original de x, antes da rotacao */
    No *B   = y->esq;      /* subarvore que muda de "filho de y" para "filho de x" */

    /* y assume o lugar de x */
    y->esq = x;
    x->pai = y;

    /* B (antiga subarvore esquerda de y) passa a ser a subarvore direita de x */
    x->dir = B;
    if (B != NULL) {
        B->pai = x;
    }

    /* y assume o pai que x tinha antes */
    y->pai = pai;
    if (pai != NULL) {
        if (pai->esq == x) {
            pai->esq = y;
        } else if (pai->dir == x) {
            pai->dir = y;
        }
    }
    /* se pai == NULL, x era a raiz global; quem chamou deve
       atualizar sua variavel raiz para o valor retornado (y) */

    return y;
}

/* ---------------------------------------------------------
 * splayZig — caso-base do Splay
 *
 * Aplica-se quando x e filho DIRETO da raiz (x->pai->pai == NULL,
 * ou seja, x nao tem "avo"). E o caso mais simples: uma unica
 * rotacao em torno do pai de x.
 *
 * Se x e filho esquerdo do pai:
 *
 *      pai                x
 *     /                    \
 *    x       ===>           pai
 *
 *   (rotacaoDireita(pai))
 *
 * Se x e filho direito do pai:
 *
 *    pai                    x
 *      \                   /
 *       x      ===>      pai
 *
 *   (rotacaoEsquerda(pai))
 *
 * Apos a rotacao, x assume o lugar do pai. Como o pai de x e a
 * propria raiz global, x se torna a NOVA raiz — por isso a funcao
 * recebe **raiz (ponteiro para o ponteiro) e atualiza *raiz = x.
 * --------------------------------------------------------- */
void splayZig(No **raiz, No *x) {
    if (raiz == NULL || x == NULL) {
        return;
    }

    No *pai = x->pai;
    if (pai == NULL) {
        /* x ja e a raiz; nada a fazer */
        return;
    }

    if (pai->esq == x) {
        /* x e filho esquerdo do pai -> rotacao a direita no pai */
        rotacaoDireita(pai);
    } else {
        /* x e filho direito do pai -> rotacao a esquerda no pai */
        rotacaoEsquerda(pai);
    }

    /* apos a rotacao, x subiu e ocupou o lugar do pai. Como o pai
       era a raiz global, x agora e a nova raiz global. */
    *raiz = x;
}

/* ---------------------------------------------------------
 * splayZigZig — caso em que x e o pai estao na MESMA direcao
 * em relacao ao avo (esquerda-esquerda ou direita-direita).
 *
 * ESQUERDA-ESQUERDA:
 *
 *        avo                x
 *       /                    \
 *      pai      ===>          pai
 *     /                         \
 *    x                           avo
 *
 *   Passos: 1) rotacaoDireita(avo)  -> pai sobe, avo desce
 *           2) rotacaoDireita(pai)  -> x sobe, pai desce
 *   (NUNCA rotacionar so o x duas vezes; a ordem avo-depois-pai
 *    e o que caracteriza o Splay e garante o balanceamento
 *    amortizado.)
 *
 * DIREITA-DIREITA (espelho):
 *
 *    avo                        x
 *      \                       /
 *       pai      ===>        pai
 *        \                   /
 *         x                avo
 *
 *   Passos: 1) rotacaoEsquerda(avo)
 *           2) rotacaoEsquerda(pai)
 *
 * Pre-condicao: x->pai (pai) != NULL e x->pai->pai (avo) != NULL,
 * e x esta na MESMA direcao que pai em relacao ao avo. Se a direcao
 * for diferente (zig-zag), esta funcao nao deve ser usada — esse
 * caso sera tratado em outra etapa.
 *
 * Assim como splayZig, recebe **raiz porque x pode acabar virando
 * a raiz global (quando avo era a raiz da arvore).
 * --------------------------------------------------------- */
void splayZigZig(No **raiz, No *x) {
    if (raiz == NULL || x == NULL) {
        return;
    }

    No *pai = x->pai;
    if (pai == NULL) {
        return; /* x ja e a raiz */
    }

    No *avo = pai->pai;
    if (avo == NULL) {
        return; /* nao ha avo: isto seria um caso Zig, nao Zig-Zig */
    }

    int avoEraRaizGlobal = (avo->pai == NULL);

    if (avo->esq == pai && pai->esq == x) {
        /* esquerda-esquerda */
        rotacaoDireita(avo);  /* pai sobe, avo desce */
        rotacaoDireita(pai);  /* x sobe, pai desce */
    } else if (avo->dir == pai && pai->dir == x) {
        /* direita-direita */
        rotacaoEsquerda(avo); /* pai sobe, avo desce */
        rotacaoEsquerda(pai); /* x sobe, pai desce */
    } else {
        /* direcoes diferentes: isto e um Zig-Zag, nao Zig-Zig.
           Esta funcao nao trata esse caso. */
        return;
    }

    /* se o avo era a raiz global, x agora ocupa esse lugar */
    if (avoEraRaizGlobal) {
        *raiz = x;
    }
}

/* ---------------------------------------------------------
 * splayZigZag — caso em que x e o pai estao em direcoes
 * OPOSTAS em relacao ao avo (esquerda-direita ou direita-esquerda).
 *
 * ESQUERDA-DIREITA (pai e filho esquerdo do avo, x e filho
 * direito do pai):
 *
 *      avo                      x
 *     /                       /   \
 *    pai        ===>        pai   avo
 *      \
 *       x
 *
 *   Passos: 1) rotacaoEsquerda(pai) -> x sobe, ocupa o lugar
 *              do pai, tornando-se filho esquerdo do avo
 *           2) rotacaoDireita(avo)  -> x sobe de novo, avo e
 *              o antigo pai ficam como seus dois filhos
 *   (Diferente do Zig-Zig: aqui rotacionamos o PAI primeiro,
 *    depois o AVO — e nao o contrario.)
 *
 * DIREITA-ESQUERDA (espelho: pai e filho direito do avo, x e
 * filho esquerdo do pai):
 *
 *    avo                        x
 *      \                      /   \
 *       pai      ===>       avo   pai
 *      /
 *     x
 *
 *   Passos: 1) rotacaoDireita(pai)
 *           2) rotacaoEsquerda(avo)
 *
 * Pre-condicao: x->pai (pai) != NULL e x->pai->pai (avo) != NULL,
 * e x esta em direcao OPOSTA a pai em relacao ao avo. Se a direcao
 * for igual (zig-zig), esta funcao nao deve ser usada.
 *
 * Assim como nos casos anteriores, recebe **raiz porque x pode
 * acabar virando a raiz global (quando avo era a raiz da arvore).
 * --------------------------------------------------------- */
void splayZigZag(No **raiz, No *x) {
    if (raiz == NULL || x == NULL) {
        return;
    }

    No *pai = x->pai;
    if (pai == NULL) {
        return; /* x ja e a raiz */
    }

    No *avo = pai->pai;
    if (avo == NULL) {
        return; /* nao ha avo: isto seria um caso Zig, nao Zig-Zag */
    }

    int avoEraRaizGlobal = (avo->pai == NULL);

    if (avo->esq == pai && pai->dir == x) {
        /* esquerda-direita */
        rotacaoEsquerda(pai); /* x sobe, ocupa o lugar do pai */
        rotacaoDireita(avo);  /* x sobe de novo, acima do avo */
    } else if (avo->dir == pai && pai->esq == x) {
        /* direita-esquerda */
        rotacaoDireita(pai);  /* x sobe, ocupa o lugar do pai */
        rotacaoEsquerda(avo); /* x sobe de novo, acima do avo */
    } else {
        /* mesma direcao: isto e um Zig-Zig, nao Zig-Zag.
           Esta funcao nao trata esse caso. */
        return;
    }

    /* se o avo era a raiz global, x agora ocupa esse lugar */
    if (avoEraRaizGlobal) {
        *raiz = x;
    }
}

/* ---------------------------------------------------------
 * splay — algoritmo Splay completo
 *
 * Sobe x recursivamente (via while) até que se torne a raiz da
 * arvore, aplicando a cada iteracao o caso adequado:
 *
 *   - SEM avo (x->pai->pai == NULL)        -> Zig
 *   - COM avo, mesma direcao de x e do pai   -> Zig-Zig
 *   - COM avo, direcoes opostas               -> Zig-Zag
 *
 * Observacao de nomenclatura: o campo que aponta para o pai de
 * um no, na nossa struct, se chama "raiz" (definido na Etapa 2),
 * nao "pai". Entao "x->pai == NULL" equivale a "x->pai == NULL"
 * do pseudocodigo do enunciado: significa que x ja e a raiz
 * GLOBAL da arvore (nao tem mais pai).
 * --------------------------------------------------------- */
void splay(No **raiz, No *x) {
    if (raiz == NULL || x == NULL) {
        return;
    }

    while (x->pai != NULL) {
        No *pai = x->pai;
        No *avo = pai->pai;

        if (avo == NULL) {
            /* Zig: x e filho direto da raiz */
            splayZig(raiz, x);
        } else if ((avo->esq == pai && pai->esq == x) ||
                   (avo->dir == pai && pai->dir == x)) {
            /* Zig-Zig: x e o pai estao na mesma direcao */
            splayZigZig(raiz, x);
        } else {
            /* Zig-Zag: x e o pai estao em direcoes opostas */
            splayZigZag(raiz, x);
        }
    }

    /* ao final do laco, x->pai == NULL, ou seja, x e a raiz */
    *raiz = x;
}

/* ---------------------------------------------------------
 * buscarSplay — busca com splay
 *
 * 1) Localiza o no com a chave procurada (igual a buscar()).
 * 2) Executa splay() sobre o no encontrado, trazendo-o para
 *    a raiz da arvore.
 *
 * Decisao de design: se a chave NAO existir na arvore, fazemos
 * splay do ULTIMO no visitado durante a busca (o no onde a busca
 * parou, que seria o pai do lugar em que a chave entraria caso
 * fosse inserida). Essa e a pratica usual em Splay Trees: mantem
 * a propriedade de localidade temporal mesmo em buscas que falham
 * — uma chave "vizinha" da procurada sobe ao topo, o que e
 * especialmente util quando esta arvore for usada como cache de
 * paginas (Etapa futura).
 *
 * Retorna o no encontrado (com a chave correta), ou NULL se a
 * arvore estiver vazia ou a chave nao existir (mas, neste ultimo
 * caso, o splay do ultimo no visitado ainda assim acontece).
 * --------------------------------------------------------- */
No *buscarSplay(No **raiz, int id) {
    if (raiz == NULL || *raiz == NULL) {
        return NULL;
    }

    clock_t inicio = clock();

    stats.buscas++;

    No *atual = *raiz;
    No *ultimoVisitado = atual;

    while (atual != NULL && atual->pagina.id != id) {
        ultimoVisitado = atual;
        if (id < atual->pagina.id) {
            atual = atual->esq;
        } else {
            atual = atual->dir;
        }
    }

    No *resultado;
    if (atual != NULL) {
        /* chave encontrada: splay no proprio no */
        stats.hits++;
        splay(raiz, atual);
        resultado = atual;
    } else {
        /* chave nao encontrada: splay no ultimo no visitado */
        stats.misses++;
        splay(raiz, ultimoVisitado);
        resultado = NULL;
    }

    clock_t fim = clock();
    tempoTotalAcesso += (double) (fim - inicio) / CLOCKS_PER_SEC;

    return resultado;
}

/* ---------------------------------------------------------
 * inserirSplay — insercao com splay
 *
 * 1) Insere a chave normalmente (igual a inserir()), mas de
 *    forma ITERATIVA — assim ja guardamos o ponteiro exato do
 *    no recem-criado, sem precisar buscar de novo depois.
 * 2) Executa splay() sobre esse no, trazendo-o para a raiz.
 *
 * Decisao de design: se a chave JA EXISTIR na arvore (nao havera
 * insercao de duplicado, igual ao comportamento de inserir()),
 * ainda assim fazemos splay no no existente com aquela chave.
 * Isso mantem o comportamento consistente: toda operacao que
 * "toca" em uma chave (ache ela ja la ou crie ela agora) traz
 * aquele no para o topo.
 *
 * Retorna o no que ficou na raiz (o recem-inserido, ou o
 * preexistente, em caso de chave duplicada).
 * --------------------------------------------------------- */
No *inserirSplay(No **raiz, int id, const char *dados) {
    if (raiz == NULL) {
        return NULL;
    }

    if (*raiz == NULL) {
        /* arvore vazia: o novo no nasce direto como raiz */
        *raiz = criarNo(id, dados);
        return *raiz;
    }

    No *atual = *raiz;
    No *pai = NULL;

    while (atual != NULL && atual->pagina.id != id) {
        pai = atual;
        if (id < atual->pagina.id) {
            atual = atual->esq;
        } else {
            atual = atual->dir;
        }
    }

    if (atual != NULL) {
        /* chave ja existia: nao insere duplicado, so faz splay nela */
        splay(raiz, atual);
        return atual;
    }

    /* chave nao existia: cria o novo no e o conecta ao pai */
    No *novo = criarNo(id, dados);
    novo->pai = pai;
    if (id < pai->pagina.id) {
        pai->esq = novo;
    } else {
        pai->dir = novo;
    }

    splay(raiz, novo);
    return novo;
}

/* ---------------------------------------------------------
 * maximo — encontra o no com a MAIOR chave de uma subarvore
 * (o no mais a direita). Usado por removerSplay para unir
 * as subarvores esquerda e direita apos remover a raiz.
 * --------------------------------------------------------- */
No *maximo(No *no) {
    if (no == NULL) {
        return NULL;
    }
    while (no->dir != NULL) {
        no = no->dir;
    }
    return no;
}

/* ---------------------------------------------------------
 * removerSplay — remocao com splay
 *
 * Passos (algoritmo classico de remocao em Splay Tree):
 *   1) Fazer splay da chave a remover -> ela vira a raiz.
 *      (usamos buscarSplay; se a chave nao existir, a busca
 *      ainda faz splay do ultimo no visitado, mas detectamos
 *      que a chave nao foi encontrada e nao removemos nada)
 *   2) Separar as subarvores esquerda (E) e direita (D) da raiz,
 *      e liberar a memoria da raiz antiga.
 *   3) Unir E e D:
 *        - se E for NULL, a nova raiz e D
 *        - se D for NULL, a nova raiz e E
 *        - senao: fazer splay do MAIOR elemento de E (o no mais
 *          a direita de E). Como esse elemento e o maior de E,
 *          depois do splay ele fica sem filho direito — entao
 *          basta encaixar D ali, como seu novo filho direito.
 *          Isso preserva a propriedade da BST, pois todo elemento
 *          de E e menor que todo elemento de D.
 *
 * Retorna 1 se a chave foi encontrada e removida, ou 0 se a
 * chave nao existia na arvore (nada e removido nesse caso, mas
 * a arvore ainda sofre o splay do ultimo no visitado).
 * --------------------------------------------------------- */
int removerSplay(No **raiz, int id) {
    if (raiz == NULL || *raiz == NULL) {
        return 0;
    }

    No *encontrado = buscarSplay(raiz, id);
    if (encontrado == NULL) {
        /* chave nao existe; buscarSplay ja fez splay do ultimo
           no visitado, nada mais a fazer aqui */
        return 0;
    }

    /* apos o splay, a chave removida e a raiz da arvore */
    No *raizAntiga = *raiz;
    No *E = raizAntiga->esq;
    No *D = raizAntiga->dir;

    if (E != NULL) {
        E->pai = NULL; /* E passa a ser uma arvore independente */
    }
    if (D != NULL) {
        D->pai = NULL; /* D passa a ser uma arvore independente */
    }

    free(raizAntiga);

    if (E == NULL) {
        *raiz = D;
    } else if (D == NULL) {
        *raiz = E;
    } else {
        /* caso geral: splay do maior elemento de E, depois
           encaixa D como filho direito desse novo topo */
        No *maiorDeE = maximo(E);
        splay(&E, maiorDeE);  /* E agora tem maiorDeE como raiz,
                                  e maiorDeE->dir == NULL */
        E->dir = D;
        D->pai = E;
        *raiz = E;
    }

    if (*raiz != NULL) {
        (*raiz)->pai = NULL; /* a nova raiz global nao tem pai */
    }

    return 1;
}

/* ---------------------------------------------------------
 * somaProfundidades — soma a profundidade de cada no da
 * subarvore (raiz tem profundidade 0). "qtdNos" acumula
 * quantos nos foram visitados, para depois calcular a media.
 * Funcao auxiliar de profundidadeMedia().
 * --------------------------------------------------------- */
void somaProfundidades(No *no, int profundidadeAtual, long *somaAcumulada, long *qtdNos) {
    if (no == NULL) {
        return;
    }
    *somaAcumulada += profundidadeAtual;
    *qtdNos += 1;
    somaProfundidades(no->esq, profundidadeAtual + 1, somaAcumulada, qtdNos);
    somaProfundidades(no->dir, profundidadeAtual + 1, somaAcumulada, qtdNos);
}

/* ---------------------------------------------------------
 * profundidadeMedia — calcula a profundidade media dos nos
 * da arvore (raiz = profundidade 0). Retorna 0.0 para arvore
 * vazia.
 * --------------------------------------------------------- */
double profundidadeMedia(No *raiz) {
    if (raiz == NULL) {
        return 0.0;
    }
    long soma = 0;
    long qtd = 0;
    somaProfundidades(raiz, 0, &soma, &qtd);
    return (double) soma / (double) qtd;
}

/* ---------------------------------------------------------
 * imprimirEstatisticas — exibe um relatorio com as metricas
 * coletadas em "stats" e o tempo total/medio de acesso.
 * Recebe a raiz atual para tambem reportar a profundidade media.
 * --------------------------------------------------------- */
void imprimirEstatisticas(No *raiz) {
    printf("----- Estatisticas de desempenho -----\n");
    printf("  Buscas realizadas : %ld\n", stats.buscas);
    printf("  Hits              : %ld\n", stats.hits);
    printf("  Misses            : %ld\n", stats.misses);
    printf("  Rotacoes          : %ld\n", stats.rotacoes);
    printf("  Profundidade media: %.3f\n", profundidadeMedia(raiz));
    printf("  Tempo total acesso: %.6f s\n", tempoTotalAcesso);
    if (stats.buscas > 0) {
        printf("  Tempo medio/busca : %.9f s\n", tempoTotalAcesso / (double) stats.buscas);
    }
    printf("---------------------------------------\n");
}

/* ---------------------------------------------------------
 * Politicas de substituicao (eviction)
 *
 * LRU_PROFUNDIDADE (Least Recently Used, aproximado):
 *   Remove o no mais PROFUNDO da arvore. Cada acesso faz splay
 *   do no acessado para a raiz, entao nos raramente acessados
 *   tendem a ficar mais distantes da raiz com o tempo. Nao exige
 *   campo extra no no — aproveita a propriedade da Splay Tree.
 *
 * LFU (Least Frequently Used):
 *   Remove o no com o menor contador de acessos (campo "acessos"
 *   adicionado nesta etapa). Captura frequencia exata, mas ignora
 *   recencia — uma pagina muito usada no passado e raramente usada
 *   agora nunca sera removida.
 *
 * HIBRIDA:
 *   Pondera os dois criterios com um score calculado por no:
 *
 *     score = profundidade * PESO_PROF + (1.0 / (acessos + 1)) * PESO_LFU
 *
 *   O no com o MAIOR score e o despejado. Isso penaliza nos que
 *   sao ao mesmo tempo profundos (pouco recentes) E pouco acessados,
 *   sem sacrificar uma pagina que, apesar de profunda, foi muito
 *   acessada no passado.
 * --------------------------------------------------------- */
typedef enum {
    POLITICA_LRU_PROFUNDIDADE,
    POLITICA_LFU,
    POLITICA_HIBRIDA
} PoliticaEviction;

#define PESO_PROF  1.0
#define PESO_LFU   4.0

/* ---------------------------------------------------------
 * CacheSplay — encapsula a arvore Splay como um cache de
 * paginas com capacidade limitada e politica de despejo
 * configuravel.
 * --------------------------------------------------------- */
typedef struct {
    No             *raiz;
    int             capacidade;
    int             tamanho;
    PoliticaEviction politica;
} CacheSplay;

/* ---------------------------------------------------------
 * criarCache — inicializa um CacheSplay com a politica dada.
 * --------------------------------------------------------- */
CacheSplay *criarCache(int capacidade, PoliticaEviction politica) {
    if (capacidade < 1) {
        fprintf(stderr, "Erro: capacidade do cache deve ser >= 1.\n");
        exit(EXIT_FAILURE);
    }
    CacheSplay *cache = (CacheSplay *) malloc(sizeof(CacheSplay));
    if (cache == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para o cache.\n");
        exit(EXIT_FAILURE);
    }
    cache->raiz      = NULL;
    cache->capacidade = capacidade;
    cache->tamanho   = 0;
    cache->politica  = politica;
    return cache;
}

/* ---------------------------------------------------------
 * Auxiliares de despejo: cada funcao percorre a arvore e
 * retorna o NO VITIMA segundo seu criterio proprio.
 * --------------------------------------------------------- */

/* LRU: no mais profundo */
void buscarMaisProfundoRec(No *no, int profAtual,
                            No **melhor, int *melhorProf) {
    if (no == NULL) return;
    if (profAtual > *melhorProf) { *melhorProf = profAtual; *melhor = no; }
    buscarMaisProfundoRec(no->esq, profAtual + 1, melhor, melhorProf);
    buscarMaisProfundoRec(no->dir, profAtual + 1, melhor, melhorProf);
}
No *vitimaLRU(No *raiz) {
    if (raiz == NULL) return NULL;
    No *melhor = raiz; int melhorProf = 0;
    buscarMaisProfundoRec(raiz, 0, &melhor, &melhorProf);
    return melhor;
}

/* LFU: no com menor contagem de acessos (desempate: maior profundidade) */
void buscarMenorAcessosRec(No *no, int profAtual,
                            No **melhor, int *melhorAcessos, int *melhorProf) {
    if (no == NULL) return;
    if (no->acessos < *melhorAcessos ||
        (no->acessos == *melhorAcessos && profAtual > *melhorProf)) {
        *melhorAcessos = no->acessos;
        *melhorProf    = profAtual;
        *melhor        = no;
    }
    buscarMenorAcessosRec(no->esq, profAtual + 1, melhor, melhorAcessos, melhorProf);
    buscarMenorAcessosRec(no->dir, profAtual + 1, melhor, melhorAcessos, melhorProf);
}
No *vitimaLFU(No *raiz) {
    if (raiz == NULL) return NULL;
    No *melhor = raiz;
    int melhorAcessos = raiz->acessos;
    int melhorProf    = 0;
    buscarMenorAcessosRec(raiz, 0, &melhor, &melhorAcessos, &melhorProf);
    return melhor;
}

/* Hibrida: maior score = profundidade * PESO_PROF + (1/(acessos+1)) * PESO_LFU */
void buscarMaiorScoreRec(No *no, int profAtual,
                          No **melhor, double *melhorScore) {
    if (no == NULL) return;
    double score = profAtual * PESO_PROF + (1.0 / (no->acessos + 1)) * PESO_LFU;
    if (score > *melhorScore) { *melhorScore = score; *melhor = no; }
    buscarMaiorScoreRec(no->esq, profAtual + 1, melhor, melhorScore);
    buscarMaiorScoreRec(no->dir, profAtual + 1, melhor, melhorScore);
}
No *vitimaHibrida(No *raiz) {
    if (raiz == NULL) return NULL;
    No *melhor = raiz;
    double melhorScore = 0.0 * PESO_PROF + (1.0 / (raiz->acessos + 1)) * PESO_LFU;
    buscarMaiorScoreRec(raiz, 0, &melhor, &melhorScore);
    return melhor;
}

/* ---------------------------------------------------------
 * despejarPagina — seleciona a vitima segundo cache->politica
 * e a remove da arvore.
 * --------------------------------------------------------- */
void despejarPagina(CacheSplay *cache) {
    if (cache == NULL || cache->raiz == NULL) return;

    No *vitima = NULL;
    switch (cache->politica) {
        case POLITICA_LFU:
            vitima = vitimaLFU(cache->raiz);
            break;
        case POLITICA_HIBRIDA:
            vitima = vitimaHibrida(cache->raiz);
            break;
        case POLITICA_LRU_PROFUNDIDADE:
        default:
            vitima = vitimaLRU(cache->raiz);
            break;
    }

    if (vitima != NULL) {
        removerSplay(&cache->raiz, vitima->pagina.id);
        cache->tamanho--;
    }
}

/* ---------------------------------------------------------
 * buscarPagina — busca uma pagina no cache pelo id.
 * --------------------------------------------------------- */
No *buscarPagina(CacheSplay *cache, int id) {
    if (cache == NULL) return NULL;
    return buscarSplay(&cache->raiz, id);
}

/* ---------------------------------------------------------
 * inserirPagina — insere uma nova pagina no cache.
 *
 * Se a pagina (mesmo id) ja existir, apenas atualiza seus dados
 * e faz splay nela (equivale a um "acesso" aquela pagina).
 *
 * Se o cache estiver cheio (tamanho == capacidade) e o id for
 * novo, despeja a pagina menos recentemente usada antes de
 * inserir a nova, mantendo o tamanho dentro da capacidade.
 * --------------------------------------------------------- */
void inserirPagina(CacheSplay *cache, int id, const char *dados) {
    if (cache == NULL) {
        return;
    }

    No *existente = buscar(cache->raiz, id);
    if (existente != NULL) {
        /* pagina ja esta no cache: apenas atualiza os dados,
           incrementa o contador de acessos e faz splay */
        if (dados != NULL) {
            snprintf(existente->pagina.dados, sizeof(existente->pagina.dados), "%s", dados);
        }
        existente->acessos++;
        splay(&cache->raiz, existente);
        return;
    }

    if (cache->tamanho >= cache->capacidade) {
        despejarPagina(cache);
    }

    No *novo = inserirSplay(&cache->raiz, id, dados);
    if (novo != NULL) {
        novo->acessos = 1; /* primeiro acesso: a propria insercao */
    }
    cache->tamanho++;
}

/* ---------------------------------------------------------
 * removerPagina — remove uma pagina do cache pelo id, se ela
 * existir. Retorna 1 se removeu, 0 se a pagina nao estava no
 * cache.
 * --------------------------------------------------------- */
int removerPagina(CacheSplay *cache, int id) {
    if (cache == NULL) {
        return 0;
    }
    int removido = removerSplay(&cache->raiz, id);
    if (removido) {
        cache->tamanho--;
    }
    return removido;
}

/* ---------------------------------------------------------
 * liberarCache — libera toda a memoria do cache (arvore +
 * estrutura do cache em si).
 * --------------------------------------------------------- */
void liberarCache(CacheSplay *cache) {
    if (cache == NULL) {
        return;
    }
    liberarArvore(cache->raiz);
    free(cache);
}

/* ---------------------------------------------------------
 * printArvore — exibe a arvore rotacionada 90 graus:
 * a raiz fica na coluna da esquerda, filhos direitos em cima,
 * filhos esquerdos embaixo. Util para visualizar a forma da
 * arvore apos cada acesso.
 * --------------------------------------------------------- */
void printArvore(No *no, int profundidade) {
    if (no == NULL) {
        return;
    }
    printArvore(no->dir, profundidade + 1);
    for (int i = 0; i < profundidade; i++) {
        printf("    ");
    }
    printf("%d\n", no->pagina.id);
    printArvore(no->esq, profundidade + 1);
}

/* ---------------------------------------------------------
 * Percurso em ordem (in-order)
 * --------------------------------------------------------- */
void emOrdem(No *no) {
    if (no == NULL) {
        return;
    }
    emOrdem(no->esq);
    printf("%d ", no->pagina.id);
    emOrdem(no->dir);
}

/* ---------------------------------------------------------
 * Benchmark de politicas de substituicao
 * --------------------------------------------------------- */

/* Executa a sequencia de acessos num cache com a politica dada
 * e retorna o numero de hits (paginas que ja estavam em cache).
 * A sequencia simula um padrao de acesso com localidade temporal:
 * algumas paginas sao acessadas com muito mais frequencia. */
long benchmarkPolitica(const int *seq, int n, int capacidade,
                        PoliticaEviction politica, const char *nomeP) {
    resetEstatisticas();
    CacheSplay *cache = criarCache(capacidade, politica);
    long hits = 0;

    for (int i = 0; i < n; i++) {
        int id = seq[i];
        int era_hit = (buscar(cache->raiz, id) != NULL);
        if (era_hit) hits++;
        inserirPagina(cache, id, NULL);
    }

    printf("  %-22s | hits=%-3ld  misses=%-3ld  hit_rate=%.0f%%"
           "  rotacoes=%-4ld  prof_media=%.2f\n",
           nomeP,
           hits, (long)n - hits,
           (double)hits / n * 100.0,
           stats.rotacoes,
           profundidadeMedia(cache->raiz));

    liberarCache(cache);
    return hits;
}

int main(void) {
    /* ============================================================
     * Sequencia de acesso com FORTE localidade temporal:
     * paginas 1 e 2 sao acessadas com muita frequencia;
     * paginas 10..19 sao "frias" (acessadas so uma vez cada).
     * Com cache de capacidade 4, as politicas vao divergir em
     * quais paginas frias sao despejadas.
     * ============================================================ */
    int seq[] = {
        1, 2, 1, 2, 10,   /* 10 e um miss; entra no cache */
        1, 2, 1, 2, 11,   /* 11 entra; alguma pagina sai */
        1, 2, 1, 2, 12,
        1, 2, 1, 2, 13,
        1, 2, 1, 2, 14,
        1, 1, 1, 1,        /* reforco de localidade de 1 */
        2, 2, 2, 2,        /* reforco de localidade de 2 */
        10, 11, 12, 13, 14 /* re-acesso as paginas frias */
    };
    int n = (int)(sizeof(seq) / sizeof(seq[0]));
    int capacidade = 4;

    printf("=====================================================\n");
    printf("  Benchmark de politicas de substituicao\n");
    printf("  Sequencia: %d acessos  |  Capacidade do cache: %d\n", n, capacidade);
    printf("=====================================================\n\n");

    printf("Politica              | hits   misses   hit_rate"
           "  rotacoes   prof_media\n");
    printf("----------------------+------------------------------------------\n");

    long h_lru = benchmarkPolitica(seq, n, capacidade, POLITICA_LRU_PROFUNDIDADE,
                                    "LRU (profundidade)");
    long h_lfu = benchmarkPolitica(seq, n, capacidade, POLITICA_LFU,
                                    "LFU (freq. exata)");
    long h_hib = benchmarkPolitica(seq, n, capacidade, POLITICA_HIBRIDA,
                                    "Hibrida (prof+freq)");

    printf("\n");

    /* ============================================================
     * VERIFICACOES
     * ============================================================ */
    printf("--- Verificacoes e analise ---\n");
    printf("  Politica com maior hit rate:   %s\n",
           (h_lru >= h_lfu && h_lru >= h_hib) ? "LRU (profundidade)" :
           (h_lfu >= h_lru && h_lfu >= h_hib) ? "LFU (freq. exata)"  :
                                                  "Hibrida (prof+freq)");
    printf("  Nota: para este workload com localidade temporal forte,\n");
    printf("  LRU aproveita melhor a propriedade nativa da Splay Tree.\n");
    printf("  LFU tende a superar LRU em workloads com frequencias\n");
    printf("  muito distintas e sem padrão temporal predominante.\n");
    printf("  Hibrida hits >= min(LRU,LFU)?  %s  (Hib=%ld)\n",
           (h_hib >= (h_lru < h_lfu ? h_lru : h_lfu)) ? "OK" : "FALHOU",
           h_hib);

    /* ============================================================
     * TESTE UNITARIO: LFU despeja o menos frequente, nao o
     * mais profundo. Cache capacidade=2, inserimos A(3x), B(1x),
     * entao forcamos despejo inserindo C. LFU deve despejar B.
     * ============================================================ */
    printf("\n--- Teste unitario: LFU despeja o menos frequente ---\n");

    CacheSplay *cLFU = criarCache(2, POLITICA_LFU);
    inserirPagina(cLFU, 100, NULL);  /* acessos=1 */
    inserirPagina(cLFU, 100, NULL);  /* acessos=2 */
    inserirPagina(cLFU, 100, NULL);  /* acessos=3 */
    inserirPagina(cLFU, 200, NULL);  /* acessos=1 (mais recente, mas menos frequente que 100) */
    /* cache cheio (100 com 3 acessos, 200 com 1). Inserir 300 deve despejar 200 */
    inserirPagina(cLFU, 300, NULL);

    printf("  100 ainda no cache (3 acessos)? %s\n",
           (buscar(cLFU->raiz, 100) != NULL) ? "OK" : "FALHOU");
    printf("  200 foi despejado (1 acesso)?    %s\n",
           (buscar(cLFU->raiz, 200) == NULL) ? "OK" : "FALHOU");
    printf("  300 foi inserido?                %s\n",
           (buscar(cLFU->raiz, 300) != NULL) ? "OK" : "FALHOU");
    printf("  tamanho continua 2?              %s (valor=%d)\n",
           (cLFU->tamanho == 2) ? "OK" : "FALHOU", cLFU->tamanho);

    liberarCache(cLFU);

    /* ============================================================
     * TESTE UNITARIO: LRU despeja o mais profundo, independente
     * de frequencia. Cache capacidade=2, inserimos A e B, depois
     * acessamos B (sobe ao topo). Ao inserir C, A esta mais profundo
     * e deve ser despejado — mesmo que A tenha sido acessado antes.
     * ============================================================ */
    printf("\n--- Teste unitario: LRU despeja o mais profundo ---\n");

    CacheSplay *cLRU = criarCache(2, POLITICA_LRU_PROFUNDIDADE);
    inserirPagina(cLRU, 10, NULL);  /* 10 na raiz */
    inserirPagina(cLRU, 20, NULL);  /* 20 na raiz, 10 fica abaixo */
    /* acessa 20 de novo: 20 continua na raiz, 10 continua abaixo */
    inserirPagina(cLRU, 20, NULL);
    /* cache cheio. Inserir 30: despeja o mais profundo (10) */
    inserirPagina(cLRU, 30, NULL);

    printf("  20 ainda no cache (estava na raiz)?  %s\n",
           (buscar(cLRU->raiz, 20) != NULL) ? "OK" : "FALHOU");
    printf("  10 foi despejado (era o mais profundo)? %s\n",
           (buscar(cLRU->raiz, 10) == NULL) ? "OK" : "FALHOU");
    printf("  30 foi inserido?                     %s\n",
           (buscar(cLRU->raiz, 30) != NULL) ? "OK" : "FALHOU");

    liberarCache(cLRU);

    return 0;
}
