# ============================================================
#  minicache — Makefile
#  Build do projeto + bateria de testes (estilo TDD).
#  Edital exige compilar SEM warnings: -Wall -Wextra -Werror.
# ============================================================

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Iinclude -g
LDFLAGS := -pthread

# Sanitizers. ASan e TSan NÃO convivem no mesmo binário -> alvos separados.
ASAN    := -fsanitize=address,undefined
TSAN    := -fsanitize=thread

SRC     := $(wildcard src/*.c)
BUILD   := build

# Componentes que já têm teste escrito.
# A cada ciclo de TDD novo, adicione o nome aqui (ex.: splay disk stats lru).
TESTS     := splay
TEST_BINS := $(addprefix $(BUILD)/test_,$(TESTS))

.PHONY: all test asan stress clean

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
stress: $(BUILD)/test_concurrency_tsan
	./$(BUILD)/test_concurrency_tsan

$(BUILD)/test_concurrency_tsan: tests/test_concurrency.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(TSAN) $^ -o $@ $(LDFLAGS)

# Regra genérica: build/test_X  <-  tests/test_X.c + todos os src/*.c
$(BUILD)/test_%: tests/test_%.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# '|' = pré-requisito de ordem: cria a pasta build/ sem forçar recompilação.
$(BUILD):
	mkdir -p $(BUILD)

# Remove binários e artefatos de build.
clean:
	rm -rf $(BUILD)
