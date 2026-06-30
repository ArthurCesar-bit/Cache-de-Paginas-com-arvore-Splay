# Dicionário — minicache

Glossário de termos para a **defesa oral**, mais um mapa de o que cada pasta e
arquivo faz. A ideia é que você consiga, ao apontar para qualquer parte do
código, explicar *o que é* e *por que está ali*.

> Convenção: onde ajuda, há um **➜ no projeto:** ligando o conceito ao arquivo /
> função real onde ele aparece.

---

## 1. Estrutura de Dados (o núcleo de ED)

**BST (Árvore Binária de Busca)**
Árvore em que, para cada nó, tudo à esquerda é menor e tudo à direita é maior.
Busca/inserção custam a *altura* da árvore. Pode degenerar (virar quase uma
lista) e ficar lenta.

**Árvore Splay**
BST **autoajustável**: toda vez que um nó é acessado, ele é levado até a **raiz**
por rotações (a operação *splay*). Páginas quentes sobem; frias afundam. Não
guarda contadores nem timestamps — a *posição* na árvore já codifica "quão
recente/quente" é o nó.
➜ no projeto: `src/splay.c`.

**Rotação (esquerda / direita)**
Operação local que troca um nó de lugar com o pai mantendo a ordem da BST. É o
"tijolo" com que o splay reorganiza a árvore.
➜ no projeto: função `rotate()` em `src/splay.c`.

**Zig / Zig-Zig / Zig-Zag**
Os três casos do splay ao subir um nó `x`:
- **Zig**: `x` é filho direto da raiz → 1 rotação.
- **Zig-Zig**: `x` e o pai na **mesma** direção → rotaciona o **avô**, depois o pai.
- **Zig-Zag**: `x` e o pai em direções **opostas** → rotaciona `x` duas vezes.
A diferença entre zig-zig e zig-zag é o que garante o desempenho amortizado.
➜ no projeto: `splay_node()` em `src/splay.c`.

**O(log n) amortizado**
Uma operação isolada pode custar caro (O(n)), mas o próprio splay rebalanceia o
caminho, então a **média sobre uma sequência** de operações é O(log n). "Amortizado"
= custo médio garantido ao longo do tempo, não pior caso de uma operação só.

**Static optimality**
Propriedade da splay: numa sequência de acessos, ela chega perto do desempenho
da *melhor BST estática possível* para aquela sequência — sem conhecer a
sequência de antemão.

**Working set**
Conjunto de itens acessados recentemente. A splay tem a propriedade do working
set: reacessar algo recente é barato — exatamente o que define um bom cache.

**Profundidade (de um nó)**
Distância da raiz até o nó (raiz = 0). No projeto, medimos a profundidade
**antes** do splay: ela indica quão "longe" a página estava — a métrica que
mostra se a localidade está aproximando as páginas quentes da raiz.
➜ no projeto: parâmetro `out_depth` em `splay_lookup()`.

**Folha**
Nó sem filhos. Na splay, a **folha mais profunda** é, por construção, a página
mais fria → vítima natural do despejo.
➜ no projeto: `splay_evict_victim()` busca a folha mais profunda.

---

## 2. Sistemas Operacionais (o núcleo de SO)

**Page cache (cache de páginas)**
Camada em RAM que guarda páginas (blocos) de um arquivo grande, para evitar ir
ao disco a cada acesso. O disco é lento; a RAM é rápida.
➜ no projeto: `src/page_cache.c`.

**Página / Bloco**
Pedaço de tamanho fixo do arquivo (ex.: 64 bytes nos testes). Endereçado por
`pageno`; no disco fica no offset `pageno × block_size`.

**Hit / Miss**
- **Hit**: a página pedida já está no cache → servida da RAM (rápido).
- **Miss**: não está → é lida do disco (lento), possivelmente despejando outra.

**Hit ratio**
`hits / (hits + misses)`. Quanto maior, melhor o cache aproveita os acessos. É a
métrica central do comparativo splay × LRU.
➜ no projeto: `stats_hit_ratio()` / `pc_hit_ratio()`.

**Frame**
Um "slot" de RAM que guarda os bytes de uma página. O **pool de frames** é o
armazenamento real; o índice (splay/lru) só diz *qual frame* tem *qual página*.
➜ no projeto: `frame_t` em `src/page_cache.c`.

**Índice ≠ bytes (princípio de design)**
A árvore splay guarda só o mapeamento `pageno → frame_id` (metadado). Os bytes
ficam no pool de frames. Isso desacopla *política* (quem é quente) de
*armazenamento* (os dados) e permite trocar splay por LRU sem mexer no resto.

**Despejo (eviction)**
Quando o cache enche, escolher uma página para sair e liberar o frame. Na splay,
a vítima é a folha mais profunda; no LRU, a menos recentemente usada.
➜ no projeto: `splay_evict_victim()` / `lru_evict_victim()`.

**Dirty bit (página suja)**
Marca de que a página foi *escrita* desde que entrou no cache, logo difere do
disco e precisa ser gravada antes de ser despejada.
➜ no projeto: campo `dirty` em `frame_t`.

**Write-back**
Estratégia em que a escrita fica só na RAM (marca suja) e vai ao disco **depois**
— no despejo, no `flush` ou no `close`. Oposto de **write-through** (grava no
disco a cada escrita, mais lento e seguro).
➜ no projeto: `pc_flush()`, e a gravação da vítima suja em `access_page()`.

**Localidade temporal**
Tendência de reacessar em breve o que foi acessado agora. É a aposta que faz a
splay (e o LRU) funcionarem. Cargas **zipfianas** têm localidade forte;
**uniformes** não têm.

**Distribuição zipfiana × uniforme (cargas do benchmark)**
- **Zipfiana**: poucas páginas "quentes" concentram a maioria dos acessos (lei de
  potência, controlada pelo expoente `theta` ≈ 0.99) → localidade forte.
- **Uniforme**: toda página é igualmente provável → sem localidade. O hit ratio
  fica perto de `capacidade / nº de páginas`.
➜ no projeto: `WL_ZIPFIAN` / `WL_UNIFORM` em `bench/workload.c` (gerador clássico
de Gray/YCSB; as páginas quentes são **embaralhadas por um hash FNV-1a** para se
espalharem pelos shards e não ficarem grudadas nos `pageno` baixos).

**Vazão (throughput / ops por segundo)**
Quantos acessos por segundo o cache atende (`nops / tempo`). É a métrica do
experimento de escalabilidade: cresce ao aumentar threads/shards.
➜ no projeto: `ops_per_sec` em `bench/bench_main.c`.

**Carga pré-computada**
Gerar toda a sequência de acessos **antes** de medir, para que o custo do RNG/zipf
não entre no cronômetro — o laço de medição só lê o vetor e chama `pc_read`/`pc_write`.
➜ no projeto: `workload_t` (vetores `pages`/`is_write`) em `bench/workload.h`.

**LRU (Least Recently Used)**
Política clássica de despejo: sai quem ficou mais tempo sem ser usado.
Implementada com **tabela hash** (achar em O(1)) + **lista duplamente encadeada**
(ordenar por recência; a cauda é a vítima). É o *baseline* obrigatório do
comparativo.
➜ no projeto: `src/lru.c`.

**Thread**
Linha de execução. Várias threads rodam "ao mesmo tempo" sobre a mesma memória —
daí a necessidade de sincronização.
➜ no projeto: `tests/test_concurrency.c` cria 8 threads.

**Race condition (condição de corrida)**
Bug em que o resultado depende da ordem imprevisível em que threads se intercalam
ao mexer no mesmo dado (ex.: dois `hits++` simultâneos perdendo uma contagem).

**Mutex (exclusão mútua)**
"Cadeado" que garante que **só uma thread por vez** entre numa seção crítica.
Protege a árvore/frames/contadores de cada shard.
➜ no projeto: `pthread_mutex_t mu` em cada `shard_t`; `lock`/`unlock` em
`access_page()`.

**Seção crítica**
Trecho de código entre `lock` e `unlock` onde só passa uma thread — onde os
dados compartilhados são tocados.

**Sharding**
Particionar o cache em N fatias independentes (`shard = pageno % nshards`), cada
uma com **sua própria** árvore + pool + mutex. Threads em shards diferentes não
se esbarram → paralelismo real, sem um gargalo único. Não há estado mutável
compartilhado entre shards.
➜ no projeto: `shard_t` e `pc->shards[...]` em `src/page_cache.c`.

**Deadlock**
Travamento mútuo: A segura o cadeado 1 e espera o 2, enquanto B segura o 2 e
espera o 1. O projeto evita porque cada operação tranca **um único** mutex.

**Atomic (operação atômica)**
Operação indivisível (sem meio-termo visível), feita pelo hardware — não precisa
de mutex. Usada nos contadores de I/O do disco, que são tocados por várias
threads.
➜ no projeto: `atomic_uint_fast64_t reads/writes` em `src/disk.c`.

**Mutex × Atomic (quando usar cada um)**
- **Atomic**: para um contador simples (um `++`). Barato, sem fila.
- **Mutex**: para uma *sequência* de passos que precisa ser indivisível como um
  todo (mexer na árvore + nos frames + nos contadores juntos).

---

## 3. Engenharia, C e Build

**Header (`.h`) — interface/contrato**
Declara **o que existe** (assinaturas das funções), sem o código. Quem usa dá
`#include` no header.
➜ no projeto: `include/*.h`.

**Implementação (`.c`) — definição**
O **corpo** das funções — o *como*. Vive em `src/`.
Analogia: header = cardápio; `.c` = cozinha.

**Linker (ligação)**
Etapa final do build que conecta cada chamada de função ao código que a
implementa. Se o `.c` não existe, dá `undefined reference` (foi o que acontecia
na "fase vermelha", com os `src/` vazios).

**`int main()`**
Ponto de entrada de um programa: onde a execução começa. Cada **executável** tem
exatamente um. A biblioteca (`src/`) **não tem** `main()`; cada teste e o futuro
`bench` têm o seu.

**Biblioteca × Aplicação**
- **Biblioteca** (`src/`): conjunto de funções para serem chamadas; não roda sozinha.
- **Aplicação**: um `main()` que liga a biblioteca e faz algo (testar, medir).

**TDD (Test-Driven Development)**
Escrever o **teste antes** da implementação: teste falha (vermelho) → implementa
o mínimo até passar (verde) → repete. Foi a metodologia adotada (ver `DIARIO.md`).

**vtable (tabela de funções virtuais)**
Struct com **ponteiros de função** que permite chamar "a política" sem saber se é
splay ou LRU. É o desenho-alvo do README, mas **não usado** hoje: a troca é feita
por um `enum` + `if` (mais simples).
➜ no projeto: `cache_policy_t` (em `include/cache_policy.h`, **vazio**); o
despacho real está em `pol_lookup/insert/evict` de `src/page_cache.c`.

**`enum` (dispatch por política)**
Tipo enumerado (`PC_POLICY_SPLAY` / `PC_POLICY_LRU`) que diz qual política usar.
A camada fina `pol_*` despacha conforme esse valor.

**pread / pwrite**
Leitura/escrita em disco numa posição (offset) explícita, **atômicas** e seguras
entre threads — ao contrário de `fseek`+`fread`, que dependem de uma "posição
atual" compartilhada (insegura sob concorrência).
➜ no projeto: `disk_read_block` / `disk_write_block` em `src/disk.c`.

**Sanitizer**
Instrumentação do compilador que pega bugs em tempo de execução:
- **ASan** (AddressSanitizer): acessos inválidos e **vazamentos de memória**.
- **UBSan**: comportamento indefinido (overflow, etc.).
- **TSan** (ThreadSanitizer): **data races**.
ASan e TSan não convivem no mesmo binário → alvos separados (`asan`, `stress`).
➜ no projeto: alvos `make asan` e `make stress`.

**ASLR (Address Space Layout Randomization)**
Defesa do SO que aleatoriza endereços de memória a cada execução. Em alguns
kernels ela conflita com o runtime do TSan (`unexpected memory mapping`). O
projeto contorna rodando o teste sob `setarch -R` (desliga o ASLR) — não é bug do
código.
➜ no projeto: alvo `stress` no `Makefile`.

**Warning como erro (`-Werror`)**
Flag que transforma todo aviso do compilador em erro, forçando código limpo. O
edital exige compilar sem warnings (`-Wall -Wextra -Werror`).

**`.gitignore`**
Lista de arquivos que o Git deve **ignorar** — aqui, artefatos regeneráveis
(`build/`, `*.o`, `*.dat`, `*.png`) que não devem ir para o repositório.

---

## 4. Mapa do projeto (o que cada pasta/arquivo faz)

```
minicache/
├── include/   contratos (.h): o QUE cada módulo oferece
├── src/       implementações (.c): o COMO — é o coração do projeto
├── tests/     bateria de testes (cada um com seu main())
├── bench/     programa-cliente que mede splay×LRU + gerador de carga
├── docs/      protótipo de referência (não entra no build)
├── build/     artefatos gerados pelo make (fora do Git)
├── Makefile   regras de compilação (all/test/asan/stress/bench/plots/clean)
├── README.md  visão geral, arquitetura e entregas
├── DIARIO.md  registro semanal (decisões, bugs, uso de IA)
└── DICIONARIO.md  este arquivo
```

### `src/` — a biblioteca (o projeto de verdade)

| Arquivo | Em uma frase |
|---|---|
| `disk.c` | trata o arquivo como blocos de tamanho fixo; lê/grava via `pread`/`pwrite`; conta I/O com atômicos |
| `splay.c` | a árvore splay: índice `pageno→frame` que se autoajusta no acesso e despeja a folha mais profunda |
| `lru.c` | o baseline LRU clássico (hash + lista duplamente encadeada) para o comparativo |
| `stats.c` | agrega métricas: hit ratio e profundidade média, sem divisão por zero |
| `page_cache.c` | a fachada `pc_*`: pool de frames, write-back e **sharding** (concorrência) — cola tudo |

### `include/` — os contratos

Um `.h` por módulo de `src/`. **Exceção:** `cache_policy.h` está **vazio** — era a
vtable opcional do README, hoje substituída pelo despacho via `enum`. Os demais
(`disk.h`, `splay.h`, `lru.h`, `stats.h`, `page_cache.h`) estão implementados e
em uso.

### `tests/` — a prova de correção

Seis programas independentes, asserts puros (sem framework), cada um com seu
`main()`. Rodados por `make test`. Cobrem: splay, disco, stats, lru, cache
(roundtrip + write-back) e concorrência (8 threads, alvo do TSan).

### `bench/` — o programa-cliente (implementado)

Contém o **`int main()` da aplicação** e o gerador de carga:
- `workload.h` / `workload.c` — geram as cargas **pré-computadas** (zipfiana =
  com localidade; uniforme = sem). Pré-computar mantém o custo do RNG **fora** do
  cronômetro: o benchmark só replaya o vetor de acessos.
- `bench_main.c` — roda a carga em splay e LRU usando só a fachada `pc_*` e mede
  hit ratio / profundidade / vazão; faz dois experimentos (comparativo de 1
  thread e escalabilidade com 1/2/4/8/16 threads) e escreve os CSVs em `build/`.
- `gen_plots.py` — transforma os CSVs em gráficos PNG do relatório (matplotlib).
Alvos: `make bench` (roda e gera os CSVs) e `make plots` (CSVs → PNG).

### `docs/` — referência

`prototipo-splay.c` consolida o protótipo passo-a-passo (etapas 1–15) que guiou o
estudo da splay. É material de apoio/defesa; **não** entra no build da
biblioteca.

---

## 5. Frases-resumo (para a defesa)

- **Splay:** "Toda página acessada vira a raiz; as frias afundam e viram a
  vítima do despejo — o autoajuste captura localidade temporal sem contadores."
- **Índice × bytes:** "A árvore só mapeia página→frame; os bytes vivem no pool de
  frames; só a camada de disco faz I/O."
- **Write-back:** "Escrita fica na RAM marcada como suja e vai ao disco depois —
  no despejo, no flush ou no close."
- **Concorrência:** "Ler a splay a modifica, então não dá read-write lock; a
  solução é sharding — N fatias independentes, um mutex cada, sem estado
  compartilhado, TSan limpo."
- **Comparativo:** "O LRU clássico é o baseline; medimos splay×LRU sob carga
  zipfiana e uniforme para ver quem aproveita melhor a localidade."
