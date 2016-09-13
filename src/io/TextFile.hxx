/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef TEXT_FILE_HXX
#define TEXT_FILE_HXX

#include <stdio.h>

/**
 * Read a file line-by-line.
 */
class TextFile {
    const char *const path;
    FILE *const file;

    unsigned no = 0;

    char buffer[4096];

public:
    TextFile(const char *_path);

    ~TextFile() {
        fclose(file);
    }

    TextFile(const TextFile &) = delete;
    TextFile &operator=(const TextFile &) = delete;

    const char *GetPath() const {
        return path;
    }

    unsigned GetLineNumber() {
        return no;
    }

    char *ReadLine();
};

#endif
