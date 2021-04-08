/*************************************************************
* LZKN64 Compression and Decompression Utility               *
*************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define TYPE_COMPRESS 1
#define TYPE_DECOMPRESS 2

#define MODE_NONE        0x7F
#define MODE_WINDOW_COPY 0x00
#define MODE_RAW_COPY    0x80
#define MODE_RLE_WRITE_A 0xA0
#define MODE_RLE_WRITE_B 0xE0
#define MODE_RLE_WRITE_C 0xFF

#define WINDOW_SIZE 0x3FF
#define COPY_SIZE   0x21
#define RLE_SIZE    0x101

const char* usageText = "LZKN64 Compression and Decompression Utility\n"
                        "\n"
                        "lzkn64 [-c|-d] input_file output_file\n"
                        "   -c: Compress the input file.\n"
                        "   -d: Decompress the input file.\n";

uint32_t currentType = 0;

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
            currentType = TYPE_COMPRESS;
        } else if (modeOption[1] == 'd') {
            currentType = TYPE_DECOMPRESS;
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
 * Compresses the data in the buffer specified in the arguments.
 */
int compressBuffer(uint8_t* fileBuffer, size_t bufferSize, uint8_t** writeBufferPtr) {
    // Position of the current read location in the buffer.
    int32_t bufferPosition = 0;

    // Position of the current write location in the written buffer.
    int32_t writePosition = 4;

    // Allocate writeBuffer with size of 0xFFFFFF (24-bit).
    uint8_t *writeBuffer = malloc(0xFFFFFF);

    // Position in the input buffer of the last time one of the copy modes was used.
    int32_t bufferLastCopyPosition = 0;

    while (bufferPosition < bufferSize) {
        // Calculate maximum length we are able to copy without going out of bounds.
        const int32_t slidingWindowMaximumLength = COPY_SIZE < (bufferSize - bufferPosition) ? COPY_SIZE : (bufferSize - bufferPosition);

        // Calculate how far we are able to look back without going behind the start of the uncompressed buffer.
        const int32_t slidingWindowMaximumOffset = (bufferPosition - WINDOW_SIZE) > 0 ? (bufferPosition - WINDOW_SIZE) : 0;

        // Calculate maximum length the forwarding looking window is able to search.
        const int32_t forwardWindowMaximumLength = RLE_SIZE < (bufferSize - bufferPosition) ? RLE_SIZE : (bufferSize - bufferPosition);

        int32_t slidingWindowMatchPosition = -1;
        int32_t slidingWindowMatchSize = 0;

        int32_t forwardWindowMatchValue = 0;
        int32_t forwardWindowMatchSize = 0;

        // The current mode the compression algorithm prefers. (0x7F == None)
        uint8_t currentMode = MODE_NONE;

        // The current submode the compression algorithm prefers.
        uint8_t currentSubmode = MODE_NONE;

        // How many bytes will have to be copied in the raw copy command.
        int32_t rawCopySize = bufferPosition - bufferLastCopyPosition;

        /*
         * Go backwards in the buffer, is there a matching value? 
         * If yes, search forward and check for more matching values in a loop. 
         * If no, go further back and repeat.
         */
        for (int32_t searchPosition = bufferPosition - 1; searchPosition >= slidingWindowMaximumOffset; searchPosition--) {
            int32_t matchingSequenceSize = 0;

            while (fileBuffer[searchPosition + matchingSequenceSize] == fileBuffer[bufferPosition + matchingSequenceSize]) {
                matchingSequenceSize++;

                if (matchingSequenceSize >= slidingWindowMaximumLength) {
                    break;
                }
            }

            // Once we find a match or a match that is bigger than the match before it, we save it's position and length.
            if (matchingSequenceSize > slidingWindowMatchSize) {
                slidingWindowMatchPosition = searchPosition;
                slidingWindowMatchSize = matchingSequenceSize;
            }
        }

        /*
         * Look one step forward in the buffer, is there a matching value? 
         * If yes, search further and check for a repeating value in a loop. 
         * If no, continue to the rest of the function.
         */
        {
            const int32_t matchingSequenceValue = fileBuffer[bufferPosition];
            int32_t matchingSequenceSize = 1;

            while (fileBuffer[bufferPosition + matchingSequenceSize] == matchingSequenceValue) {
                matchingSequenceSize++;

                if (matchingSequenceSize >= forwardWindowMaximumLength) {
                    break;
                }
            }

            // If we find a sequence of matching values, save them.
            if (matchingSequenceSize > 1) {
                forwardWindowMatchValue = matchingSequenceValue;
                forwardWindowMatchSize = matchingSequenceSize;
            }
        }

        /* Try to pick which mode works best with the current values.
         * Make sure the matched bytes have a length of at least 3 bytes.
         * Exception for RLE Mode B, which just uses a single byte.
         */
        if (forwardWindowMatchSize >= 3) {
            currentMode = MODE_RLE_WRITE_A;

            if (forwardWindowMatchValue != 0x00 && forwardWindowMatchSize <= COPY_SIZE) {
                currentSubmode = MODE_RLE_WRITE_A;
            } else if (forwardWindowMatchValue != 0x00 && forwardWindowMatchSize > COPY_SIZE) {
                currentSubmode = MODE_RLE_WRITE_A;
            } else if (forwardWindowMatchValue == 0x00 && forwardWindowMatchSize <= COPY_SIZE) {
                currentSubmode = MODE_RLE_WRITE_B;
            } else if (forwardWindowMatchValue == 0x00 && forwardWindowMatchSize > COPY_SIZE) {
                currentSubmode = MODE_RLE_WRITE_C;
            }
        } else if (slidingWindowMatchSize >= 3) {
            currentMode = MODE_WINDOW_COPY;
        }

        /*
         * Write a raw copy command when these following conditions are met:
         * The current mode is set and there are raw bytes available to be copied.
         * The raw byte length exceeds the maximum length that can be stored.
         * Raw bytes need to be written due to the proximity to the end of the buffer.
         */
        if ((currentMode != MODE_NONE && rawCopySize >= 1) || rawCopySize >= 0x1F || (bufferPosition + 1) == bufferSize) {
            if ((bufferPosition + 1) == bufferSize) {
                rawCopySize = bufferSize - bufferLastCopyPosition;
            }

            writeBuffer[writePosition++] = MODE_RAW_COPY | rawCopySize & 0x1F;

            for (int32_t writtenBytes = 0; writtenBytes < rawCopySize; writtenBytes++) {
                writeBuffer[writePosition++] = fileBuffer[bufferLastCopyPosition++]; 
            }
        }

        if (currentMode == MODE_WINDOW_COPY) {
            writeBuffer[writePosition++] = MODE_WINDOW_COPY | ((slidingWindowMatchSize - 2) & 0x1F) << 2 | (((bufferPosition - slidingWindowMatchPosition) & 0x300) >> 8);
            writeBuffer[writePosition++] = (bufferPosition - slidingWindowMatchPosition) & 0xFF;

            bufferPosition += slidingWindowMatchSize;
            bufferLastCopyPosition = bufferPosition;
        } else if (currentMode == MODE_RLE_WRITE_A) {
            if (currentSubmode == MODE_RLE_WRITE_A) {
                writeBuffer[writePosition++] = MODE_RLE_WRITE_A | (forwardWindowMatchSize - 2) & 0x1F;
                writeBuffer[writePosition++] = forwardWindowMatchValue & 0xFF;
            } else if (currentSubmode == MODE_RLE_WRITE_B) {
                writeBuffer[writePosition++] = MODE_RLE_WRITE_B | (forwardWindowMatchSize - 2) & 0x1F;
            } else if (currentSubmode == MODE_RLE_WRITE_C) {
                writeBuffer[writePosition++] = MODE_RLE_WRITE_C;
                writeBuffer[writePosition++] = (forwardWindowMatchSize - 2) & 0xFF;
            }

            bufferPosition += forwardWindowMatchSize;
            bufferLastCopyPosition = bufferPosition;
        } else {
            bufferPosition += 1;
        }
    }

    // Write the compressed size.
    writeBuffer[1] = 0x00;
    writeBuffer[1] = writePosition >> 16 & 0xFF;
    writeBuffer[2] = writePosition >>  8 & 0xFF;
    writeBuffer[3] = writePosition       & 0xFF;

    *writeBufferPtr = writeBuffer;

    // Return the current position of the write buffer, essentially giving us the size of the write buffer.
    return writePosition;
}

/*
 * Decompresses the data in the buffer specified in the arguments.
 */
int decompressBuffer(uint8_t* fileBuffer, uint8_t** writeBufferPtr) {
    // Position of the current read location in the buffer.
    uint32_t bufferPosition = 4;

    // Position of the current write location in the written buffer.
    uint32_t writePosition = 0;

    // Get compressed size.
    uint32_t compressedSize = (fileBuffer[1] << 16) + (fileBuffer[2] << 8) + fileBuffer[3];

    // Allocate writeBuffer with size of 0xFFFFFF (24-bit).
    uint8_t *writeBuffer = malloc(0xFFFFFF);

    while (bufferPosition < compressedSize) {
        uint8_t modeCommand = fileBuffer[bufferPosition++];

        if (modeCommand >= MODE_WINDOW_COPY && modeCommand < MODE_RAW_COPY) {
            uint8_t copyLength = (modeCommand >> 2) + 2;
            uint16_t copyOffset = fileBuffer[bufferPosition++] + (modeCommand << 8) & 0x3FF;

            for (uint8_t currentLength = copyLength; currentLength > 0; currentLength--) {
                writeBuffer[writePosition++] = writeBuffer[writePosition - copyOffset];
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

    switch (currentType) {
        case TYPE_COMPRESS:
            outputFileSize = compressBuffer(inputBuffer, inputFileSize, &outputBuffer);
            break;

        case TYPE_DECOMPRESS:
            outputFileSize = decompressBuffer(inputBuffer, &outputBuffer);
            break;
    }

    // Write the output buffer to a file.
    fwrite(outputBuffer, 1, outputFileSize, outputFile);
    fclose(outputFile);

    return EXIT_SUCCESS;
}
