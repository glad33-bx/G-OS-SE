CC = gcc
CFLAGS = -m32 -fno-stack-protector -fno-builtin -fno-pic -fno-pie -ffreestanding
LD = ld
LDFLAGS = -m elf_i386 -T app.ld --oformat binary

all: bin/TEST.BIN

bin/TEST.BIN: o/utils/test.o o/lib/ulib.o o/lib/crt0.o
	$(LD) $(LDFLAGS) $^ -o $@

o/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@