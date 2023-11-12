/*************************************************************
* LZKN64 Compression and Decompression Utility               *
*************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define TYPE_COMPRESS 1
#define TYPE_DECOMPRESS 2

#define MODE_NONE        0x7F
#define MODE_WINDOW_COPY 0x00
#define MODE_RAW_COPY    0x80
#define MODE_RLE_WRITE_A 0xC0
#define MODE_RLE_WRITE_B 0xE0
#define MODE_RLE_WRITE_C 0xFF

#define WINDOW_SIZE 0x3DF
#define COPY_SIZE   0x21
#define RLE_SIZE    0x101

const char* usageText = "LZKN64 Compression and Decompression Utility\n"
                        "\n"
                        "lzkn64 [-c|-d] input output\n"
                        "   -c: Compress the input file.\n"
                        "   -d: Decompress the input file.\n";

uint32_t currentType = 0;

/*
 * Checks if a specified signed integer value is in a specified signed array.
 */
bool isValueInArray(int32_t value, const int32_t* array, const int32_t arraySize) {
    for (int32_t i = 0; i < arraySize; i++) {
        if (value == array[i]) {
            return true;
        }
    }

    return false;
}

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
            fprintf(stderr, "Error: The mode option you specified is not correct.\n");
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
        int32_t slidingWindowMaximumLength = 0;

        // Calculate maximum length we are able to copy without going out of bounds.
        if (COPY_SIZE <= (bufferSize - bufferPosition)) {
            slidingWindowMaximumLength = COPY_SIZE;
        } else {
            slidingWindowMaximumLength = bufferSize - bufferPosition;
        }

        int32_t slidingWindowMaximumOffset = 0;

        // Calculate how far we are able to look back without going behind the start of the uncompressed buffer.
        if ((bufferPosition - WINDOW_SIZE) > 0) {
            slidingWindowMaximumOffset = bufferPosition - WINDOW_SIZE;
        } else {
            slidingWindowMaximumOffset = 0;
        }

        int32_t forwardWindowMaximumLength = 0;

        // Calculate maximum length the forwarding looking window is able to search.
        if (RLE_SIZE <= (bufferSize - bufferPosition - 1)) {
            forwardWindowMaximumLength = RLE_SIZE;
        } else {
            forwardWindowMaximumLength = bufferSize - bufferPosition;
        }

        if (forwardWindowMaximumLength > COPY_SIZE) {
            for (int32_t i = (COPY_SIZE + 1); i < (forwardWindowMaximumLength + 1); i++) {
                const int32_t positionArray[] = { 0x021, 0x421, 0x821, 0xC21 };
                const int32_t position = (bufferPosition + i) & 0xFFF;

                if (isValueInArray(position, positionArray, sizeof(positionArray) / sizeof(positionArray[0]))) {
                    forwardWindowMaximumLength = i;
                    break;
                }
            }
        }

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

        // How many bytes we still have to copy in RLE matches with more than 0x21 bytes.
        int32_t rleBytesLeft = 0;

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

            // Once we find a match or a match that is bigger than the match before it, we save its position and length.
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
            int32_t matchingSequenceSize = 0;

        	// If our matching sequence number is not 0x00, set the forward window maximum length to the copy size minus 1.
            // This is the highest it can really be in that case.
            if (matchingSequenceValue != 0x00 && forwardWindowMaximumLength > COPY_SIZE - 1) {
                forwardWindowMaximumLength = COPY_SIZE - 1;
            }

            while (fileBuffer[bufferPosition + matchingSequenceSize] == matchingSequenceValue) {
                matchingSequenceSize++;

                if (matchingSequenceSize >= forwardWindowMaximumLength) {
                    break;
                }
            }

            // If we find a sequence of matching values, save them.
            if (matchingSequenceSize >= 1) {
                forwardWindowMatchValue = matchingSequenceValue;
                forwardWindowMatchSize = matchingSequenceSize;
            }
        }

        // Try to pick which mode works best with the current values.
        if (slidingWindowMatchSize >= 4 && slidingWindowMatchSize > forwardWindowMatchSize) {
            currentMode = MODE_WINDOW_COPY;
        } else if (forwardWindowMatchSize >= 3) {
            currentMode = MODE_RLE_WRITE_A;

            if (forwardWindowMatchValue != 0x00) {
                currentSubmode = MODE_RLE_WRITE_A;
            } else if (forwardWindowMatchValue == 0x00 && forwardWindowMatchSize < COPY_SIZE) {
                currentSubmode = MODE_RLE_WRITE_B;
            } else if (forwardWindowMatchValue == 0x00 && forwardWindowMatchSize >= COPY_SIZE) {
                currentSubmode = MODE_RLE_WRITE_C;
            }
        } else if (forwardWindowMatchSize >= 2 && forwardWindowMatchValue == 0x00) {
            currentMode = MODE_RLE_WRITE_A;
            currentSubmode = MODE_RLE_WRITE_B;
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

            while (rawCopySize > 0) {
                if (rawCopySize > 0x1F) {
                    writeBuffer[writePosition++] = MODE_RAW_COPY | 0x1F;

                    for (int32_t writtenBytes = 0; writtenBytes < 0x1F; writtenBytes++) {
                        writeBuffer[writePosition++] = fileBuffer[bufferLastCopyPosition++]; 
                    }

                    rawCopySize -= 0x1F;
                } else {
                    writeBuffer[writePosition++] = MODE_RAW_COPY | rawCopySize & 0x1F;

                    for (int32_t writtenBytes = 0; writtenBytes < rawCopySize; writtenBytes++) {
                        writeBuffer[writePosition++] = fileBuffer[bufferLastCopyPosition++]; 
                    }

                    rawCopySize = 0;
                }
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

    // If the write buffer is not aligned to 16-bit, add a 0x00 byte to the end.
    if ((writePosition % 2) != 0) {
        writeBuffer[writePosition++] = 0x00;
    }

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

void compressFile(const char* inputFilePath, uint8_t** outputBuffer, size_t* outputSize) {
    // Open the input file
    FILE* inputFile = fopen(inputFilePath, "rb");
    if (inputFile == NULL) {
        perror("Failed to open input file");
        return;
    }

    // Get the size of the input file
    fseek(inputFile, 0, SEEK_END);
    size_t inputFileSize = ftell(inputFile);
    rewind(inputFile);

    // Read the input file into a buffer
    uint8_t* inputBuffer = (uint8_t*)malloc(inputFileSize);
    fread(inputBuffer, 1, inputFileSize, inputFile);
    fclose(inputFile);

    // Compress the input buffer
    *outputSize = compressBuffer(inputBuffer, inputFileSize, outputBuffer);

    // Clean up
    free(inputBuffer);
}

void decompressFile(const char* inputFilePath, uint8_t** outputBuffer, size_t* outputSize) {
    // Open the input file
    FILE* inputFile = fopen(inputFilePath, "rb");
    if (inputFile == NULL) {
        perror("Failed to open input file");
        return;
    }

    // Get the size of the input file
    fseek(inputFile, 0, SEEK_END);
    size_t inputFileSize = ftell(inputFile);
    rewind(inputFile);

    // Read the input file into a buffer
    uint8_t* inputBuffer = (uint8_t*)malloc(inputFileSize);
    fread(inputBuffer, 1, inputFileSize, inputFile);
    fclose(inputFile);

    // Decompress the input buffer
    *outputSize = decompressBuffer(inputBuffer, outputBuffer);

    // Clean up
    free(inputBuffer);
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

    uint8_t* outputBuffer = NULL;
    size_t outputSize = 0;
    
    if (currentType == TYPE_COMPRESS) {
        compressFile(argv[2], &outputBuffer, &outputSize);
    } else if (currentType == TYPE_DECOMPRESS) {
        decompressFile(argv[2], &outputBuffer, &outputSize);
    }

    FILE* outputFile = fopen(argv[3], "wb");
    fwrite(outputBuffer, 1, outputSize, outputFile);
    fclose(outputFile);

    free(outputBuffer);

    return EXIT_SUCCESS;
}
