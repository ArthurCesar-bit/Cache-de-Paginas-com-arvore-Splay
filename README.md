# minicache — Page Cache com Árvore Splay

> Trabalho Interdisciplinar **Estrutura de Dados (C) × Sistemas Operacionais**
> Ifes — Campus Cachoeiro de Itapemirim · **Tema 12**
> Apresentação final: **02/07/2026**

Camada de *page cache* para um arquivo de dados grande, lido por múltiplas
threads, em que o índice das páginas em cache é uma **árvore splay
(autoajustável)**. O objetivo central é investigar, com dados reais, se o
autoajuste da splay aproveita **localidade temporal** melhor que um LRU clássico
em determinadas cargas de acesso.

---

## 1. O que o sistema faz

Um arquivo grande é visto como uma sequência de blocos (páginas) de tamanho
fixo. Entre a aplicação e esse arquivo existe um cache de páginas em memória com
capacidade limitada. Quando uma página é acessada:

- se já está no cache → **hit**: os bytes são servidos da RAM;
- se não está → **miss**: a página é lida do disco; se o cache estiver cheio,
  uma página **fria** é despejada antes (com *write-back* se estiver suja).

A diferença deste tema para um cache comum está em **como o cache decide o que é
quente e o que é frio**: pela posição na árvore splay.

## 2. A árvore central (núcleo de ED): Splay

A árvore splay indexa as páginas presentes no cache (`pageno → frame`).

- **Splay no acesso:** todo acesso a uma página a rotaciona até a **raiz**.
  Assim, páginas quentes migram naturalmente para perto do topo e ficam baratas
  de reencontrar.
- **Despejo:** quando o cache enche, a vítima é uma **página fria** — uma folha
  profunda, que por construção é a menos acessada recentemente.
- **Baseline obrigatório:** uma implementação **LRU clássica** (tabela hash +
  lista duplamente encadeada) roda atrás da **mesma interface**, para o
  comparativo direto exigido pelo enunciado.

A árvore splay nunca toca o disco: ela vive só em memória e mapeia páginas para
*frames*. Quem lê e escreve blocos é a camada de disco.

## 3. O mecanismo de SO (núcleo de SO)

- **Cache de blocos multithread:** várias threads acessam o arquivo
  concorrentemente.
- **Write-back:** páginas sujas (escritas) são gravadas no disco no despejo e no
  fechamento; *flush* explícito disponível.
- **Instrumentação:** *hit ratio* e **profundidade média de acesso** (a
  profundidade do nó **antes** do splay) são medidos por execução — é o número
  que evidencia se a localidade está, de fato, aproximando as páginas quentes da
  raiz.

> **Por que a concorrência é o ponto difícil deste tema:** a *leitura* da splay
> **muta** a árvore (o splay rotaciona nós). Portanto **não** se pode usar um
> *read-write lock* simples (todo "leitor" é, na prática, escritor). A solução
> de concorrência fina adotada é **sharding**: o espaço de páginas é
> particionado em N shards independentes, cada um com sua própria árvore + pool
> de frames + mutex (`shard = hash(pageno) % N`). Threads em páginas de shards
> distintos não contendem, e não há estado mutável compartilhado entre shards.

## 4. Teste de fogo (anti-atalho)

O que impede uma solução trivial e orienta a avaliação experimental:

1. Sob carga com **localidade forte (zipfiana)** e **sem localidade
   (uniforme)**, comparar **hit ratio** e **custo amortizado por acesso** entre
   **splay** e **LRU**.
2. **Explicar teoricamente** por que a splay vence ou perde em cada caso
   (apoiada no teorema do acesso amortizado `O(log n)` e nas propriedades de
   *static optimality* / *working set* da splay).
3. Garantir **thread-safety sem data race** comprovado pelo **ThreadSanitizer**.

## 5. Requisitos de engenharia (do edital geral)

| Requisito | Detalhe |
|---|---|
| Linguagem | **C (C11)**, `gcc -Wall -Wextra -Werror`, **sem warnings** |
| Concorrência | **sem data races** (TSan) · **sem vazamentos** (Valgrind/ASan) |
| Locking | **mutex global** só é aceito **até a Entrega 2** |
| Build | `Makefile` com alvos `all`, `test`, `stress`, `clean` |
| Testes | bateria própria + scripts de geração dos gráficos |
| Rastreabilidade | `DIARIO.md` semanal + commits **incrementais** (nada de "initial commit" gigante) |
| Relatório | PDF de **8–15 páginas** com gráficos de dados reais |
| Defesa | **oral individual — vale 50%**: explicar e modificar o código ao vivo |

**Penalidades automáticas:** warning de compilação `−5%` · vazamento no caminho
feliz `−10%` · race no TSan `−15%`.

## 6. Arquitetura

A aplicação fala com uma fachada estilo *syscall* (`pc_read`, `pc_write`,
`pc_flush`). A fachada coordena duas peças desacopladas: a **política/índice**
(splay **ou** LRU, atrás de uma mesma vtable) e o **pool de frames** (bytes +
*dirty bits*). Só o pool de frames conversa com a **camada de disco**
(arquivo de blocos + `fsync`).

A interface de política é o que permite trocar splay por LRU sem mexer no resto:

```c
typedef struct cache_policy {
    void    *self;
    int      (*lookup)(void *self, uint64_t pageno, int *out_depth);
    void     (*insert)(void *self, uint64_t pageno, int frame_id);
    uint64_t (*evict_victim)(void *self, int *out_frame_id);
    void     (*remove)(void *self, uint64_t pageno);
    void     (*destroy)(void *self);
} cache_policy_t;
```

## 7. Estrutura de diretórios

```
minicache/
├── Makefile
├── DIARIO.md
├── include/   # page_cache, cache_policy, splay, lru, disk, stats
├── src/       # implementações
├── bench/      # workload (zipfiano/uniforme), bench_main, gen_plots
└── tests/     # test_splay, test_lru, test_cache, test_concurrency
```

## 8. Build e uso

```sh
make            # compila tudo (sem warnings)
make test       # roda a bateria de testes unitários
make stress     # teste de estresse multithread (alvo do TSan)
make clean      # limpa artefatos de build
```

## 9. Entregas

| Entrega | Escopo | Peso |
|---|---|---|
| **E1 — Fundação** | camada de disco instrumentada, pool de frames, stats, testes da fundação | 15% |
| **E2 — Núcleo de ED** | splay e LRU completas + cache sob mutex global | 40% |
| **E3 — Núcleo de SO** | sharding (concorrência fina), multithread, write-back, TSan limpo | 10% |
| **E4 — Robustez e relatório** | flush/close, teste de fogo, relatório experimental, defesa | 35% |

## 10. Métricas coletadas (para o relatório)

- *Hit ratio* (splay vs LRU) sob carga zipfiana e uniforme;
- profundidade média de acesso na splay;
- custo amortizado por acesso (toques em nós: comparações + rotações),
  confrontado com a previsão `O(log n)`;
- leituras/escritas em disco por operação;
- vazão com 1, 2, 4, 8 e 16 threads (versão com sharding) e o gargalo.

## 11. Política de IA e autoria

O uso de assistentes de IA é permitido e esperado, **com** histórico de commits
incrementais e `DIARIO.md` registrando decisões, bugs e — quando a IA for usada
— o prompt, o que ela errou e o que a equipe corrigiu. A nota individual é
condicionada à **defesa oral**.

## 12. Referências

- Tanenbaum & Bos — *Modern Operating Systems* (cache, escalonamento, memória)
- Arpaci-Dusseau — *Operating Systems: Three Easy Pieces* (locking, paginação)
- Cormen et al. — *Introduction to Algorithms* (árvores balanceadas, splay, hashing)
- Sleator & Tarjan (1985) — *Self-Adjusting Binary Search Trees* (a splay original)