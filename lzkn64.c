/*************************************************************
* LZKN64 Compression and Decompression Utility               *
* Written by Fluvian, 2021                                   *
*************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define VERSION "Version 1.0\n"
#define AUTHOR "Written by Fluvian, 2021\n"

#define MODE_COMPRESS 1
#define MODE_DECOMPRESS 2

const char* usageText = "LZKN64 Compression and Decompression Utility\n"
                        VERSION
                        AUTHOR
                        "\n"
                        "lzkn64 [-c|-d] input_file output_file\n"
                        "   -c: Compress the input file.\n"
                        "   -d: Decompress the input file.\n";

uint32_t currentMode = 0;

/*
 * Prints the usage help of this program.
 */
void printUsage() {
    puts(usageText);
}

/*
 * Parses arguments from the input.
 */
int parseArguments(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        fprintf(stderr, "Error: You have not specified enough arguments.\n");
        return EXIT_FAILURE;
    } else if (argc > 4) {
        printUsage();
        fprintf(stderr, "Error: You have specified too many arguments or an unexpected argument was found.\n");
        return EXIT_FAILURE;
    }

    const char* modeOption = argv[1];

    // Does the mode option actually have the "-" sign?
    if (modeOption[0] == '-') {
        if(modeOption[1] == 'c') {
            currentMode = MODE_COMPRESS;
        } else if (modeOption[1] == 'd') {
            currentMode = MODE_DECOMPRESS;
        } else {
            printUsage();
            fprintf(stderr, "Error: The option parameter you specified is not correct.\n");
            return EXIT_FAILURE;
        }
    } else {
        printUsage();
        fprintf(stderr, "Error: You have not specified an option parameter.\n");
        return EXIT_FAILURE;
    }

    return 0;
}

/*
 * Decompresses the data in the buffer specified in the arguments.
 */
int decompressBuffer(uint8_t* fileBuffer, uint8_t** writeBufferPtr) {
    #define MODE_WINDOW_COPY 0x00
    #define MODE_RAW_COPY    0x80
    #define MODE_RLE_WRITE_A 0xA0
    #define MODE_RLE_WRITE_B 0xE0
    #define MODE_RLE_WRITE_C 0xFF

    // Position of the current read location in the buffer.
    uint32_t bufferPosition = 0;

    // Position of the current write location in the written buffer.
    uint32_t writePosition = 0;

    // Get compressed size.
    uint32_t compressedSize = (fileBuffer[1] << 16) + (fileBuffer[2] << 8) + fileBuffer[3];

    // Allocate writeBuffer with size of 0xFFFFFF (24-bit).
    uint8_t *writeBuffer = malloc(0xFFFFFF);

    bufferPosition += 4;

    while (bufferPosition < compressedSize) {
        uint8_t modeCommand = fileBuffer[bufferPosition++];

        if (modeCommand >= MODE_WINDOW_COPY && modeCommand < MODE_RAW_COPY) {
            uint8_t copyLength = (modeCommand >> 2) + 2;
            uint16_t copyDisplacement = fileBuffer[bufferPosition++] + (modeCommand << 8) & 0x3FF;

            for (uint8_t currentLength = copyLength; currentLength > 0; currentLength--) {
                writeBuffer[writePosition++] = writeBuffer[writePosition - copyDisplacement];
            }
        } else if (modeCommand >= MODE_RAW_COPY && modeCommand < MODE_RLE_WRITE_A) {
            uint8_t copyLength = modeCommand & 0x1F;

            for (uint8_t currentLength = copyLength; currentLength > 0; currentLength--) {
                writeBuffer[writePosition++] = fileBuffer[bufferPosition++];
            }
        } else if (modeCommand >= MODE_RLE_WRITE_A && modeCommand <= MODE_RLE_WRITE_C) {
            uint16_t writeLength = 0;
            uint8_t writeValue = 0x00;

            if (modeCommand >= MODE_RLE_WRITE_A && modeCommand < MODE_RLE_WRITE_B) {
                writeLength = (modeCommand & 0x1F) + 2;
                writeValue = fileBuffer[bufferPosition++];
            } else if (modeCommand >= MODE_RLE_WRITE_B && modeCommand < MODE_RLE_WRITE_C) {
                writeLength = (modeCommand & 0x1F) + 2;
            } else if (modeCommand == MODE_RLE_WRITE_C) {
                writeLength = fileBuffer[bufferPosition++] + 2;
            }

            for (uint16_t currentLength = writeLength; currentLength > 0; currentLength--) {
                writeBuffer[writePosition++] = writeValue;
            }
        }
    }

    *writeBufferPtr = writeBuffer;

    // Return the current position of the write buffer, essentially giving us the size of the write buffer.
    return writePosition;
}

/*
 * The main function. argv should contain three arguments, 
 * the current mode, the input file and the output file. 
 */
int main(int argc, char** argv) {
    int32_t argumentReturn = parseArguments(argc, argv);

    if (argumentReturn != 0) {
        return argumentReturn;
    }
    
    // Load the input file into a buffer.
    FILE* inputFile = fopen(argv[2], "rb");
    size_t inputFileSize;

    fseek(inputFile, 0, SEEK_END);
    inputFileSize = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    uint8_t* inputBuffer = malloc(inputFileSize);
    fread(inputBuffer, 1, inputFileSize, inputFile);

    fclose(inputFile);

    // Initialize the output buffer.
    FILE* outputFile = fopen(argv[3], "wb");
    size_t outputFileSize;
    uint8_t* outputBuffer = NULL;

    switch (currentMode) {
        case MODE_COMPRESS:
            // Stub for now.
            break;

        case MODE_DECOMPRESS:
            outputFileSize = decompressBuffer(inputBuffer, &outputBuffer);
            break;
    }

    // Write the output buffer to a file.
    fwrite(outputBuffer, 1, outputFileSize, outputFile);
    fclose(outputFile);

    return EXIT_SUCCESS;
}
