#include "main.h"
#include "lzkn64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool parse_arguments(int argc, const char *argv[], struct Arguments *arguments) {
    if (argc < 4) {
        return false;
    }

    if (strcmp(argv[1], "-c") == 0) {
        arguments->mode = MODE_COMPRESS;
    } else if (strcmp(argv[1], "-d") == 0) {
        arguments->mode = MODE_DECOMPRESS;
    } else {
        return false;
    }

    arguments->input_file = argv[2];
    arguments->output_file = argv[3];

    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            arguments->compression_type = COMPRESSION_TYPE_ACCURATE;
        } else if (strcmp(argv[i], "-e") == 0) {
            arguments->compression_type = COMPRESSION_TYPE_EFFICIENT;
        } else if (strcmp(argv[i], "-p") == 0) {
            arguments->pad_output = true;
        } else {
            return false;
        }
    }

    return true;
}

void print_help(void) {
    printf("Usage: lzkn64 [-c|-d] <input_file> <output_file> [-a|-e] [-p]\n");
    printf("Compress or decompress a file using lzkn64.\n");
    printf("\n");
    printf("  -c  Compress the input file.\n");
    printf("  -d  Decompress the input file.\n");
    printf("  -a  Use accurate compression (default).\n");
    printf("  -e  Use efficient compression.\n");
    printf("  -p  Pad the output file to the nearest 2-byte boundary.\n");
}

int main(int argc, const char *argv[]) {
    struct Arguments arguments;
    arguments.mode = MODE_UNDEFINED;
    arguments.input_file = NULL;
    arguments.output_file = NULL;
    arguments.compression_type = COMPRESSION_TYPE_ACCURATE;
    arguments.pad_output = false;

    if (!parse_arguments(argc, argv, &arguments)) {
        print_help();
        return EXIT_FAILURE;
    }

    FILE *input_file = fopen(arguments.input_file, "rb");
    if (input_file == NULL) {
        printf("Error: Could not open input file.\n");
        return EXIT_FAILURE;
    }

    fseek(input_file, 0, SEEK_END);
    size_t input_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    u8 *input_buffer = malloc(input_size);
    if (input_buffer == NULL) {
        printf("Error: Could not allocate memory for input buffer.\n");
        return EXIT_FAILURE;
    }

    if (fread(input_buffer, 1, input_size, input_file) != input_size) {
        printf("Error: Could not read input file.\n");
        return EXIT_FAILURE;
    }

    fclose(input_file);

    size_t output_size;
    u8 *output_buffer;

    if (arguments.mode == MODE_COMPRESS) {
        // Allocate maximum possible size for compressed file since we don't know the exact size yet.
        output_buffer = malloc(LZKN64_MAXIMUM_FILE_SIZE);
        if (output_buffer == NULL) {
            printf("Error: Could not allocate memory for output buffer.\n");
            return EXIT_FAILURE;
        }

        if (arguments.compression_type == COMPRESSION_TYPE_ACCURATE) {
            output_size = lzkn64_compress_accurate(input_buffer, output_buffer, input_size);
        } else if (arguments.compression_type == COMPRESSION_TYPE_EFFICIENT) {
            output_size = lzkn64_compress_efficient(input_buffer, output_buffer, input_size);
        }

        if (arguments.pad_output) {
            output_size = (output_size + 1) & ~1;
        }
    } else if (arguments.mode == MODE_DECOMPRESS) {
        // Allocate maximum possible size for decompressed file since we don't know the exact size yet.
        output_buffer = malloc(LZKN64_MAXIMUM_FILE_SIZE);
        if (output_buffer == NULL) {
            printf("Error: Could not allocate memory for output buffer.\n");
            return EXIT_FAILURE;
        }
        output_size = lzkn64_decompress(input_buffer, output_buffer, input_size);
    } else {
        printf("Error: Invalid mode.\n");
        return EXIT_FAILURE;
    }

    FILE *output_file = fopen(arguments.output_file, "wb");
    if (output_file == NULL) {
        printf("Error: Could not open output file.\n");
        return EXIT_FAILURE;
    }

    if (fwrite(output_buffer, 1, output_size, output_file) != output_size) {
        printf("Error: Could not write output file.\n");
        return EXIT_FAILURE;
    }

    fclose(output_file);

    free(input_buffer);
    free(output_buffer);

    return EXIT_SUCCESS;
}
