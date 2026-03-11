SRCS_COMMON = \
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
	fat_linux_adapter_io.c

SRCS_CLI = \
	$(SRCS_COMMON) \
	fat_linux_main.c

SRCS_FUSE = \
	$(SRCS_COMMON) \
	fat_fuse.c

all:
	gcc $(SRCS_CLI) -o fat

fat_fuse:
	gcc $(SRCS_FUSE) -o fat_fuse $$(pkg-config --cflags --libs fuse3)

clean:
	rm -f fat fat_fuse
