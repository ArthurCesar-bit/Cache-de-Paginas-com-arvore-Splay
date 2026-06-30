# RUNS — como rodar o minicache

Guia de execução do projeto de ponta a ponta: build, testes, sanitizers,
benchmark e gráficos. Todos os comandos rodam a partir da **raiz do projeto**.

> Os artefatos vão para `build/` (binários, `*.dat`, CSVs e PNGs) e são ignorados
> pelo Git. `make clean` apaga tudo isso.

---

## 0. Pré-requisitos

| Ferramenta | Para quê | Como obter (Debian/Ubuntu) |
|---|---|---|
| `gcc` + `make` | compilar e orquestrar o build | `sudo apt install build-essential` |
| `libpthread` | threads (vem com a glibc) | já incluso |
| `setarch` (opcional) | contornar o ASLR no TSan | `sudo apt install util-linux` (já costuma ter) |
| `python3` + `matplotlib` | gerar os gráficos (`make plots`) | `pip install --user matplotlib` |

Flags de build (já no `Makefile`, exigência do edital): `-std=c11 -Wall -Wextra
-Werror` → **compila sem warnings**.

---

## 1. Caminho rápido (tudo de uma vez)

```sh
make clean            # parte de um estado limpo
make test             # compila e roda os 6 testes unitários
make asan             # mesma bateria sob AddressSanitizer + UBSan
make stress           # teste de concorrência sob ThreadSanitizer
make plots            # roda o benchmark e gera os gráficos PNG
```

Se os quatro passarem, o projeto está validado (correção + sem vazamento + sem
data race) e os dados/gráficos do relatório estão em `build/`.

---

## 2. Build

```sh
make            # (= make all) compila a bateria de testes, sem rodar
make clean      # remove build/ (binários, *.dat, CSVs, PNGs)
```

---

## 3. Testes unitários

```sh
make test
```

Compila e roda os 6 testes (asserts puros, cada um com seu `main()`):

| Teste | Verifica |
|---|---|
| `test_splay` | criar, buscar vazio (miss), inserir, achar, `out_depth` |
| `test_disk` | roundtrip write→read e contadores de I/O |
| `test_stats` | hit ratio e profundidade média, sem divisão por zero |
| `test_lru` | recência governa o despejo (vítima = menos recente) |
| `test_cache` | roundtrip, hit ratio > 0 e write-back após despejo |
| `test_concurrency` | 8 threads em páginas disjuntas (working set > capacidade) |

Rodar **um** teste isolado (depois de `make` ou `make all`):

```sh
./build/test_splay
./build/test_cache
```

---

## 4. Sanitizers (correção sob estresse)

```sh
make asan     # AddressSanitizer + UBSan: acessos inválidos, vazamentos, UB
make stress   # ThreadSanitizer: data races no teste de concorrência
```

- `asan` e `stress` usam **binários separados** (ASan e TSan não convivem no
  mesmo binário).
- `make stress` roda sob `setarch -R` quando disponível, para desligar o **ASLR**
  e evitar o falso `unexpected memory mapping` do runtime do TSan em alguns
  kernels (não altera a semântica do teste).

---

## 5. Benchmark (E4) — splay × LRU

```sh
make bench
```

Compila `build/bench` e roda **dois experimentos** sobre a fachada pública `pc_*`:

1. **Comparativo (1 thread):** `{splay, lru} × {uniform, zipfian}` → hit ratio,
   profundidade média e vazão.
2. **Escalabilidade (zipfian):** 1/2/4/8/16 threads (`nshards = nthreads`) → vazão.

Imprime uma tabela no terminal e escreve os CSVs:

- `build/bench_compare.csv`
- `build/bench_threads.csv`

### 5.1. Rodar com parâmetros próprios

Compile o binário e chame-o direto (passar flags exige rodar o binário, não o
alvo `make bench`, que usa os defaults):

```sh
make build/bench      # compila SÓ o bench, sem rodar (make all não inclui o bench)
./build/bench --help
```

| Flag | Significado | Default |
|---|---|---|
| `--npages N` | tamanho do espaço de páginas | 20000 |
| `--capacity N` | frames em RAM (páginas que cabem no cache) | 2000 |
| `--nops N` | número de acessos do experimento | 500000 |
| `--write-pct N` | % de escritas \[0..100] | 20 |
| `--theta F` | expoente da zipf (maior = mais localidade) | 0.99 |
| `--block N` | bytes por bloco/página | 4096 |
| `--seed N` | semente do RNG (reprodutibilidade) | 1 |

Exemplos:

```sh
# localidade mais forte e cache menor (deve subir o hit ratio da zipfiana)
./build/bench --theta 1.2 --capacity 1000

# carga maior e mais estável (mais lenta), só leitura
./build/bench --nops 2000000 --write-pct 0

# varredura de theta (compara o efeito da localidade)
for t in 0.6 0.8 0.99 1.2; do echo "theta=$t"; ./build/bench --theta "$t"; done
```

---

## 6. Gráficos do relatório

```sh
make plots
```

Roda o benchmark e converte os CSVs em PNG via `bench/gen_plots.py`:

- `build/plot_hit_ratio.png` — hit ratio splay × LRU (uniform vs zipfian)
- `build/plot_avg_depth.png` — profundidade média de acesso
- `build/plot_throughput.png` — vazão × nº de threads

Se já existem CSVs e você só quer regerar os PNGs:

```sh
python3 bench/gen_plots.py
```

> Se aparecer `matplotlib nao instalado`: `pip install --user matplotlib`
> (ou `pip install --user --break-system-packages matplotlib` em distros com
> ambiente Python "externally managed" / PEP 668).

---

## 7. Resumo dos alvos do Makefile

| Alvo | O que faz |
|---|---|
| `make` / `make all` | compila a bateria de testes (sem rodar) |
| `make test` | compila e roda os 6 testes unitários |
| `make asan` | testes sob AddressSanitizer + UBSan |
| `make stress` | concorrência sob ThreadSanitizer |
| `make bench` | compila e roda o benchmark (gera os CSVs) |
| `make plots` | roda o benchmark e gera os gráficos PNG |
| `make clean` | remove `build/` |
