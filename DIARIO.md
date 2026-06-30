# Diário de Bordo — minicache

Registro semanal de decisões, bugs e uso de IA, conforme exigido pelo edital
(Seção 11 do README). Para cada uso de IA, anotamos o prompt, o que a ferramenta
errou e o que a equipe corrigiu.

---

## Dia 1 — 28/06/2026

### Arquitetura principal definida

Estabelecemos a divisão em camadas desacopladas, cada uma com seu header:

- **`page_cache`** — fachada estilo syscall (`pc_open/close/read/write/flush`).
  É a porta de entrada; coordena política + pool de frames. Parametrizável por
  política (`PC_POLICY_SPLAY` / `PC_POLICY_LRU`) e por número de shards
  (`nshards`), já prevendo a concorrência fina da E3.
- **`splay`** — índice autoajustável `pageno → frame` (núcleo de ED). Expõe
  `out_depth` no lookup para medir a profundidade **antes** do splay (métrica
  central do relatório).
- **`lru`** — baseline clássico para o comparativo splay vs LRU, com despejo do
  menos recentemente usado (`lru_evict_victim`).
- **`disk`** — camada de blocos de tamanho fixo, **instrumentada** (contadores
  `disk_reads`/`disk_writes`).
- **`stats`** — agregador de métricas (hit ratio e profundidade média).

Princípio que guiou o desenho: só o pool de frames toca o disco; o índice
(splay ou lru) vive apenas em memória. Política trocável atrás de uma interface
comum, sem mexer no resto.

### Metodologia: TDD (test-first)

Decidimos conduzir o desenvolvimento por **TDD**. A ordem é sempre:

1. escrever o teste contra a interface desejada (vermelho);
2. implementar o `src/*.c` mínimo até passar (verde);
3. adicionar o próximo teste que exija mais comportamento.

Os testes usam **asserts puros** (sem framework de terceiros) — decisão tomada
para que a equipe consiga explicar cada linha na defesa oral e não dependa de
código externo.

### Infraestrutura de build

`Makefile` com `all`, `test`, `asan`, `stress`, `clean`. Flags do edital desde o
início: `-std=c11 -Wall -Wextra -Werror` (zero warnings). Alvos de sanitizer
separados (`asan` = ASan+UBSan; `stress` = TSan), porque ASan e TSan não convivem
no mesmo binário. A variável `TESTS` lista os componentes ativos na bateria e
cresce a cada ciclo de TDD.

### Implementação inicial: testes falhando (fase vermelha)

Escritos os 6 testes da fundação, todos compilando **sem warnings** e falhando
apenas no **link** (interfaces definidas, `src/*.c` ainda vazios):

| Teste | Verifica |
|---|---|
| `test_splay` | criar, buscar vazio (miss), inserir, achar, `out_depth` |
| `test_disk` | roundtrip write→read e contadores de I/O |
| `test_stats` | hit ratio (0.75) e profundidade média, sem divisão por zero |
| `test_lru` | recência governa o despejo (vítima = menos recente) |
| `test_cache` | roundtrip, hit ratio > 0 e **write-back após despejo** (reabre o arquivo e confere persistência) |
| `test_concurrency` | 8 threads em páginas disjuntas, working set > capacidade (alvo do TSan) |

Estado ao fim da sessão: **fase vermelha completa**, nenhum `src/*.c`
implementado. Próximo passo: fechar o primeiro verde.

### Uso de IA

- **Ferramenta:** Claude Code (assistente de IA).
- **Para que serviu:** desenhar os headers das camadas, escrever o `Makefile` e
  os testes da fundação; explicar o que é um Makefile e o papel de cada alvo.
- **O que precisou ser corrigido pela equipe:**
  - A IA tendeu a já **implementar a splay** (`src/splay.c`) junto com o teste;
    interrompemos para manter a disciplina **test-first** — só testes e
    interfaces nesta etapa, implementação depois.
  - Reforçamos o escopo: **nenhum `src/*.c`** deve ser escrito agora; os testes
    (que são `.c`) sim.
  - Faltou `#include <stddef.h>` em `lru.h` (uso de `size_t`) e `-lm` no link
    (uso de `<math.h>` em `test_stats.c`) — ambos ajustados.
- Prompts usados: 
   - Etapa 1 — Construir uma Árvore Binária de Busca (BST) básica

      Objetivo: criar a estrutura mínima funcional.

      Estruturas
      typedef struct No {
        int chave;
        struct No *esq;
        struct No *dir;
      } No;
      Implementar
       Criar nó
       Inserir
       Buscar
       Percurso em ordem
       Liberar memória
      Testes

      Inserir:

      50
      30
      70
      20
      40
      60
      80

      A saída em ordem deve ser:

      20 30 40 50 60 70 80
      
  - Etapa 2 — Adicionar ponteiro para o pai

      A árvore Splay depende fortemente do relacionamento pai-filho.

      Modificar estrutura
      typedef struct No {
        int chave;
        struct No *esq;
        struct No *dir;
        struct No *raiz;
      } No;
      Ajustar
       Inserção
       Busca
       Remoção (opcional por enquanto)
      Testes

      Verificar se:

      20->raiz == 30
      30->raiz == 50

  -  Etapa 3 — Implementar rotações

      Esta é a etapa mais importante.

      Rotação à direita

      Antes:

            x
           /
          y

      Depois:

          y
           \
            x

      Implementar:

      No* rotacaoDireita(No *x);
      Rotação à esquerda

      Antes:

          x
           \
            y

      Depois:

            y
           /
          x

      Implementar:

      No* rotacaoEsquerda(No *x);
      Testes

      Criar árvores pequenas e verificar:

      ponteiros pai
      ponteiros filho
      nova raiz

  -  Etapa 4 — Implementar o caso Zig

      Agora você implementa apenas o caso mais simples do Splay.

      Exemplo:

          30
         /
       10

      Após Splay(10):

         10
           \
            30
    
      Implementar:

      void splayZig(No **raiz, No *x);
      Testes
      filho esquerdo
      filho direito

  -  Etapa 5 — Implementar Zig-Zig

      Exemplo:
  
            50
           /
          30
         /
       20

      Resultado:

          20
            \
            30
              \
              50

      Implementar:

      esquerda-esquerda
      direita-direita

  -  Etapa 6 — Implementar Zig-Zag

      Exemplo:

            50
           /
          30
            \
            40

      Resultado:

            40
           /  \
          30  50

      Implementar:

      esquerda-direita
      direita-esquerda

  -  Etapa 7 — Implementar o algoritmo Splay completo

      Agora unifique tudo:

      void splay(No **raiz, No *x)
      {
          while(x->pai != NULL)
          {
              // Zig
              // Zig-Zig
              // Zig-Zag
          }
      }
      Testes

      Árvore:

              50
             /  \
           30    70
          / \
         20 40

      Buscar:

      20

      Resultado:

      20
       \
       30
         \
          50

  -  Etapa 8 — Transformar a busca em busca Splay

      Antes:

      buscar(40);

      Agora:

      buscarSplay(40);

      A busca deve:

      localizar o nó;
      executar splay().

  -  Etapa 9 — Transformar inserção em inserção Splay

      Ao inserir:

      insert(100);

      O novo elemento deve virar a raiz.

  -  Etapa 10 — Implementar remoção Splay

      Passos:

      Fazer Splay do elemento.
      Remover a raiz.
      Unir as subárvores esquerda e direita.

  -  Etapa 11 — Medir desempenho

      Criar estatísticas:

      typedef struct {
          long buscas;
          long rotacoes;
          long hits;
          long misses;
      } Estatisticas;

      Medir:

      quantidade de rotações;
      profundidade média;
      tempo de acesso.

  -  Etapa 12 — Criar a estrutura de Página

      Agora você deixa de armazenar apenas inteiros.

      typedef struct Pagina {
          int id;
          char dados[256];
      } Pagina;

      O nó vira:

      typedef struct No {
          Pagina pagina;
          struct No *esq;
          struct No *dir;
          struct No *pai;
      } No;

  -  Etapa 13 — Implementar Cache de Páginas

      Criar:

      typedef struct {
          No *raiz;
          int capacidade;
          int tamanho;
      } CacheSplay;

      Operações:

      buscarPagina();
      inserirPagina();
      removerPagina();

  -  Etapa 14 — Explorar localidade temporal

      Simular acessos:

      1
      5
      2
      5
      5
      3
      5
      8
      5

      Após vários acessos, a árvore naturalmente ficará parecida com:

              5
             / \
            2   8
           /
          1

      Isso demonstra a localidade temporal, pois a página mais acessada permanece próxima da raiz.

  -  Etapa 15 — Implementar política de substituição

      Quando o cache estiver cheio:

      remover a página menos utilizada;
      ou remover a página mais profunda;
      ou implementar uma estratégia híbrida.
