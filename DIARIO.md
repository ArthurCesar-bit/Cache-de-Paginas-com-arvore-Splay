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

---

## Dia 2 — 30/06/2026

### Consolidação do protótipo e decisão de arquitetura

Os 15 snapshots incrementais do protótipo (`bench/bst.c`, `bst.c (1)` …
`bst.c (14)`) eram estágios do **mesmo** programa, baixados etapa a etapa. A
última versão (Etapa 15) já era o acumulado de tudo (BST → rotações → splay →
cache de páginas → políticas LRU/LFU/híbrida). Consolidamos num único arquivo e
o **movemos para `docs/prototipo-splay.c`** (com `git mv`), como material de
referência/defesa — ele **não** entra no build da biblioteca.

Diante de duas arquiteturas incompatíveis no repositório, decidimos o rumo:

- **Design A — interfaces pré-definidas (`include/`)**: splay como índice
  `pageno → frame`, camada de disco, pool de frames, LRU clássico como baseline,
  fachada `page_cache` com sharding. É o projeto do edital.
- **Design B — só reorganizar o monólito**: o nó *é* a página, sem disco, sem
  threads, "LRU" = despejar o nó mais profundo.

Optamos pelo **Design A**. Motivo registrado: o Design B feriria exigências do
enunciado — o **baseline LRU clássico** (hash + lista), **todo o núcleo de SO**
(cache de blocos multithread, write-back, sharding, TSan) e a separação
**índice ≠ bytes**. O monólito fica preservado em `docs/` como referência de ED.

### Saída da fase vermelha: `src/` implementado (TDD → verde)

Implementamos os 5 módulos atrás das interfaces de `include/`, fechando o verde
da bateria escrita no Dia 1:

| Módulo | O que faz |
|---|---|
| `src/disk.c` | blocos de tamanho fixo via `pread`/`pwrite`; contadores de I/O **atômicos** (`stdatomic`) |
| `src/splay.c` | índice `pageno→frame` autoajustável (zig/zig-zig/zig-zag); `out_depth` antes do splay; despejo da folha mais profunda |
| `src/lru.c` | baseline LRU clássico (hash + lista duplamente encadeada) |
| `src/stats.c` | hit ratio + profundidade média, sem divisão por zero |
| `src/page_cache.c` | fachada `pc_*`; pool de frames + dirty bits; **write-back**; **sharding** (mutex/árvore/pool por shard) |

Decisões de implementação:

- **Despacho de política por `enum`, não por vtable.** A troca splay↔LRU é uma
  camada fina (`pol_lookup`/`pol_insert`/`pol_evict`) que despacha por
  `pc_policy_t`. Por isso `include/cache_policy.h` ficou **vazio** (a vtable do
  README é refinamento opcional, ainda não usado).
- **Extensão de `include/splay.h`** com `splay_evict_victim`, simétrica a
  `lru_evict_victim` — necessária para o `page_cache` despejar a página fria.

### Bugs e percalços

- **Link falhando (esperado):** com `src/*.c` vazios, `make test` compilava sem
  warning mas quebrava no link (`undefined reference to 'splay_create'…`). Era a
  fase vermelha; resolvido ao implementar os módulos.
- **TSan abortando com `unexpected memory mapping`:** **não era data race** — é
  incompatibilidade do runtime do ThreadSanitizer com o ASLR deste kernel.
  Contornado rodando o binário sob `setarch -R` (desliga o ASLR); ajustamos o
  alvo `make stress` para usar `setarch -R` quando disponível.

### Infra: `.gitignore`

Criamos `.gitignore` (ignora `build/`, `*.o`, `*.dat`, `*.png`) — antes a pasta
`build/` (binários + arquivos `.dat` dos testes) aparecia como não rastreada.

### Estado ao fim da sessão

`make test`, `make asan` e `make stress` **passando** — 6/6 testes, zero
warnings, sem vazamento (ASan/UBSan) e sem data race (TSan). **Falta** a camada
de benchmark (`bench/bench_main.c` e `bench/workload.c` ainda são stubs) e o
relatório experimental (E4).

### Uso de IA

- **Ferramenta:** Claude Code (assistente de IA).
- **Para que serviu:** consolidar os snapshots do protótipo; comparar as duas
  arquiteturas; implementar os `src/*.c` atrás das interfaces; criar
  `.gitignore`; ajustar o `Makefile`; atualizar `README.md` e este diário.
  Serviu também para explicar conceitos (árvore splay, sharding, mutex, header
  × implementação) com vistas à defesa oral.
- **O que precisou de decisão/correção da equipe:**
  - A IA chegou a **oferecer o Design B** (só reorganizar o monólito) como
    opção. Pedimos a análise contra o edital; ela mesma apontou que B feria o
    baseline LRU clássico e o núcleo de SO. A equipe então **fixou o Design A**.
  - A IA tendeu a acoplar `stats` ao `page_cache`; mantivemos `stats` como
    módulo isolado (testado por `test_stats`) e o `page_cache` com contadores
    próprios sob o mutex do shard, para não reintroduzir estado compartilhado.
  - Diante do erro do TSan, a IA primeiro tratou como possível race; a verificação
    mostrou ser ambiente (ASLR), e a equipe validou o contorno com `setarch -R`.
- **Prompts usados (resumo):**
  - "junte todos os bst.c em um único arquivo com essas funcionalidades";
  - "existe alguma forma de alterar minha arquitetura para satisfazer a
    arquitetura pré-definida (bench/tests/src separados)? pode apagar os testes
    se não forem necessários" → e, em seguida, "esse design fere algum princípio
    do comando da atividade?";
  - "siga com o caminho A e mova o bst.c para um arquivo de docs";
  - "crie o gitignore"; "atualize o README.md / DIARIO.md com as decisões".

---

## Dia 2 (continuação) — 30/06/2026 · tarde — Camada de benchmark (E4)

### Implementação da camada de benchmark

Os três arquivos de `bench/` ainda eram **stubs vazios** (0 byte). Implementamos
a camada de medição que faltava para a E4:

| Arquivo | O que faz |
|---|---|
| `bench/workload.h` | contrato do gerador de carga: enum `WL_UNIFORM`/`WL_ZIPFIAN` e a struct `workload_t` (vetores `pages` + `is_write`) |
| `bench/workload.c` | gera a sequência de acessos: RNG splitmix64 (reprodutível), zipf clássico de Gray/YCSB e embaralhamento FNV-1a das páginas quentes |
| `bench/bench_main.c` | driver: replaya a carga só pela fachada `pc_*`, mede hit ratio / profundidade / vazão, escreve os CSVs |
| `bench/gen_plots.py` | lê os CSVs e gera os PNGs do relatório (matplotlib, backend `Agg`) |

`Makefile`: novos alvos `bench` (compila + roda, gera os CSVs) e `plots` (CSVs →
PNG). A regra usa `-Ibench` além de `-Iinclude` porque `bench/` tem header
próprio (`workload.h`).

### Decisões de projeto do benchmark

- **Carga pré-computada.** Geramos todo o vetor de acessos **antes** de medir,
  para manter o custo do RNG/zipf **fora** do cronômetro — o laço cronometrado
  só lê o vetor e chama `pc_read`/`pc_write`.
- **Embaralhar as páginas quentes (FNV-1a).** O gerador zipf produz "ranks de
  popularidade" (0 = mais quente). Sem tratar, as quentes seriam sempre os
  `pageno` baixos e **cairiam todas no mesmo shard**, falseando a escalabilidade.
  Mapear `rank → fnv1a(rank) % npages` espalha as quentes por todos os shards.
- **Medir só pela fachada pública.** O driver usa apenas `pc_*`; não espia o
  interior do cache. Métricas: `pc_hit_ratio`, `pc_avg_depth` e tempo de parede
  (`clock_gettime(CLOCK_MONOTONIC)`).
- **Dois experimentos:** (1) comparativo de 1 thread `{splay,lru} × {uniform,
  zipfian}`; (2) escalabilidade da carga zipfiana com 1/2/4/8/16 threads
  (`nshards = nthreads`). Parâmetros configuráveis por flags (`--theta`,
  `--capacity`, `--nops`, …) com default modesto (roda em segundos).

### Resultado observado (a discutir no relatório)

Com o default (`npages=20000`, `capacity=2000`, `nops=500000`, `theta=0.99`):

- **Uniforme:** hit ratio ~10% nas duas políticas (= `capacidade/npages`), como
  esperado sem localidade.
- **Zipfiana:** hit ratio sobe para ~70% — a localidade aparece.
- **Surpresa honesta:** nesta configuração o **LRU saiu mais rápido e com hit
  ratio ligeiramente maior** que a splay. Não é bug — é justamente a pergunta que
  o tema manda investigar. A splay paga rotações (muta a árvore a cada acesso) e,
  sob zipf "puro", o LRU já captura bem a localidade. Fica como ponto a explorar
  no relatório: variar `theta`/`capacity` e testar um *working set* que muda em
  fases, onde a propriedade de working set da splay tende a ajudar.

### Bugs e percalços

- **`matplotlib` ausente:** o `gen_plots.py` falhou de forma limpa avisando a
  falta. A primeira tentativa de instalar foi bloqueada pelo PEP 668
  (ambiente "externally managed"); resolvido com
  `pip install --user --break-system-packages matplotlib`. O script já usa o
  backend `Agg` para rodar sem display.
- Fora isso, compilou de primeira **sem warnings** (`-Wall -Wextra -Werror`) e os
  6 testes seguem passando após `make clean && make test`.

### Pendência de infra: `.vscode/`

O editor (extensão **C/C++ Runner**) gerou uma pasta `.vscode/` com `launch.json`
contendo **caminhos absolutos** da máquina (`/home/arthur/...`) e apontando para
um build genérico que **não** é o do nosso `Makefile`. Decisão tomada: tratá-la
como config local (candidata a entrar no `.gitignore`), **não** versioná-la como
está. Ainda não aplicado ao `.gitignore`.

### Uso de IA

- **Ferramenta:** Claude Code (assistente de IA).
- **Para que serviu:** implementar `bench/` (gerador de carga + driver + script
  de gráficos), adicionar os alvos `make bench`/`make plots`, atualizar
  `README.md` e `DICIONARIO.md`, e explicar conceitos de C/SO sob demanda
  (`uint64_t`/`size_t`, sufixo `_t`, `_POSIX_C_SOURCE`, o que é POSIX) com vistas
  à defesa oral.
- **O que precisou de decisão/correção da equipe:**
  - Validamos que o benchmark usa **só a fachada `pc_*`** (sem espiar o interior),
    para o número ser honesto.
  - Conferimos que o embaralhamento FNV das páginas quentes era necessário para a
    escalabilidade não ficar enviesada por shard.
  - A equipe decidiu **não** versionar `.vscode/` (caminhos absolutos da máquina).
- **Prompts usados (resumo):**
  - "aplique os códigos da pasta bench, para o sistema rodar";
  - "explique o que cada código de bench/ faz";
  - "o que é uint64_t / size_t / o sufixo _t / _POSIX_C_SOURCE / POSIX?";
  - "atualize o README.md, o DICIONARIO.md e o DIARIO.md".
