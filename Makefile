BUILD ?= build
SRC = src
HEADERS = headers

CC ?= gcc
CFLAGS = -Wall -pthread -Wextra -Wpedantic -I$(HEADERS)

ALL = client serveur

.PHONY: all
all: $(ALL)

client: $(BUILD)/users.o $(BUILD)/message.o $(BUILD)/func/func_client.o $(SRC)/client.c
	$(CC) $(CFLAGS) $^ -o $@

serveur: $(BUILD)/users.o $(BUILD)/message.o $(BUILD)/billet.o $(BUILD)/func/func_serveur.o $(SRC)/serveur.c
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: $(SRC)/%.c $(HEADERS)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(ALL) $(BUILD)
