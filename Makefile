# Makefile — BB84 QKD Simulation
# ================================
# Usage:
#   make                         — build
#   make run                     — 64 photons, both scenarios
#   make run ARGS="-n 32 -b -c -p -v"  — Cascade + PA, verbose
#   make stats                   — 1000-trial QBER report, no Eve
#   make stats-eve               — 1000-trial QBER report, Eve active
#   make cascade                 — single run with Cascade + PA, both scenarios
#   make sweep                   — photon-count sweep → sweep.csv
#   make help                    — print flag reference
#   make clean                   — remove build artefacts

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS = -lm
TARGET  = bb84_sim
SRCS    = main.c bb84.c cli.c privacy_amp.c stats.c cascade.c export.c
HDRS    = bb84.h cli.h privacy_amp.h stats.h cascade.h export.h
N      ?= 64
ARGS   ?= -n $(N) -b

.PHONY: all run stats stats-eve cascade sweep help clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) $(ARGS)

stats: $(TARGET)
	./$(TARGET) -n $(N) --stats 1000

stats-eve: $(TARGET)
	./$(TARGET) -n $(N) --stats 1000 -e

cascade: $(TARGET)
	./$(TARGET) -n $(N) -b -c -p

sweep: $(TARGET)
	./$(TARGET) --sweep sweep.csv

help: $(TARGET)
	./$(TARGET) -h

clean:
	rm -f $(TARGET) *.o sweep.csv bb84_trials.csv
