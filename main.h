#ifndef MAIN_H
#define MAIN_H

#include "types.h"

#define LZKN64_MAXIMUM_FILE_SIZE 0xFFFFFF // 16 MB

enum Mode {
    MODE_UNDEFINED,
    MODE_COMPRESS,
    MODE_DECOMPRESS
};

enum CompressionType {
    COMPRESSION_TYPE_ACCURATE,
    COMPRESSION_TYPE_EFFICIENT,
};

struct Arguments {
    enum Mode mode;
    const char *input_file;
    const char *output_file;
    enum CompressionType compression_type;
    bool pad_output;
};

bool parse_arguments(int argc, const char *argv[], struct Arguments *arguments);
void print_help(void);

#endif // MAIN_H
