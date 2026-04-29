CC      = gcc
CFLAGS  = -I/ucrt64/include/SDL2
LDFLAGS = -L/ucrt64/lib -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lcomdlg32

text_editor.exe: text_editor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f text_editor.exe