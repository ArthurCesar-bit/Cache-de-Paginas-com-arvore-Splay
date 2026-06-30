# ============================================================
#  minicache — Makefile
#  Build do projeto + bateria de testes (estilo TDD).
#  Edital exige compilar SEM warnings: -Wall -Wextra -Werror.
# ============================================================

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Iinclude -g
LDFLAGS := -pthread -lm

# Sanitizers. ASan e TSan NÃO convivem no mesmo binário -> alvos separados.
ASAN    := -fsanitize=address,undefined
TSAN    := -fsanitize=thread

SRC     := $(wildcard src/*.c)
BUILD   := build

# Componentes que já têm teste escrito.
# A cada ciclo de TDD novo, adicione o nome aqui (ex.: splay disk stats lru).
TESTS     := splay disk stats lru cache concurrency
TEST_BINS := $(addprefix $(BUILD)/test_,$(TESTS))

# Camada de benchmark (E4): driver + gerador de carga.
BENCH_SRC := bench/bench_main.c bench/workload.c

.PHONY: all test asan stress bench plots clean

# Compila os testes ativos (sem rodar).
all: $(TEST_BINS)

# Compila e RODA a bateria de testes unitários (o comando do dia a dia em TDD).
test: $(TEST_BINS)
	@echo "== rodando testes =="
	@for t in $(TEST_BINS); do \
		echo ">> $$t"; \
		./$$t || exit 1; \
	done
	@echo "== todos passaram =="

# Mesma bateria, porém sob AddressSanitizer + UBSan (pega vazamento e UB).
asan: CFLAGS += $(ASAN)
asan: clean test

# Teste de concorrência sob ThreadSanitizer (pega data race). Para a E3.
# Em alguns kernels o runtime do TSan falha com "unexpected memory mapping"
# por causa do ASLR; 'setarch -R' desliga a aleatorização e contorna isso
# (não altera a semântica do teste). Cai no modo direto se não houver setarch.
stress: $(BUILD)/test_concurrency_tsan
	@if command -v setarch >/dev/null 2>&1; then \
		setarch -R ./$(BUILD)/test_concurrency_tsan; \
	else \
		./$(BUILD)/test_concurrency_tsan; \
	fi

$(BUILD)/test_concurrency_tsan: tests/test_concurrency.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(TSAN) $^ -o $@ $(LDFLAGS)

# Compila E RODA o benchmark (gera os CSVs em build/ para o relatório).
bench: $(BUILD)/bench
	./$(BUILD)/bench

# Roda o benchmark e gera os gráficos PNG (precisa de python3 + matplotlib).
plots: $(BUILD)/bench
	./$(BUILD)/bench
	python3 bench/gen_plots.py

# bench/ tem header próprio (workload.h) -> -Ibench além de -Iinclude.
$(BUILD)/bench: $(BENCH_SRC) $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) -Ibench $^ -o $@ $(LDFLAGS)

# Regra genérica: build/test_X  <-  tests/test_X.c + todos os src/*.c
$(BUILD)/test_%: tests/test_%.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# '|' = pré-requisito de ordem: cria a pasta build/ sem forçar recompilação.
$(BUILD):
	mkdir -p $(BUILD)

# Remove binários e artefatos de build.
clean:
	rm -rf $(BUILD)
