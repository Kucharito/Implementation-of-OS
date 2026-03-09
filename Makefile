SRCS = \
	fat_core.c \
	fat_state.c \
	fat_common.c \
	fat_disk.c \
	fat_init.c \
	fat_search.c \
	fat_tree.c \
	fat_dir.c \
	fat_file.c \
	fat_rw.c \
	fat_linux_adapter.c \
	fat_linux_adapter_io.c \
	fat_linux_main.c

all:
	gcc $(SRCS) -o fat

clean:
	rm -f fat
