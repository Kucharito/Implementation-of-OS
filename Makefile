all:
	gcc fat_core.c fat_linux_adapter.c -o fat

clean:
	rm -f fat
