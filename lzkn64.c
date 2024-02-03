#include "lzkn64.h"

size_t lzkn64_compress_efficient(const u8 *input_buffer, u8 *output_buffer, size_t input_size) {
    size_t input_offset = 0;
    size_t output_offset = 4; // Skip the first 4 bytes since they are the compressed file size.

    size_t input_last_processed_data_offset = 0;

    while (input_offset < input_size) {
        size_t sliding_window_copy_maximum_length = 0;

        // Find the maximum length of the sliding window copy, e.g. how many bytes can be copied without going out of bounds.
        if ((input_size - input_offset) >= SLIDING_WINDOW_COPY_MAXIMUM_LENGTH) {
            sliding_window_copy_maximum_length = SLIDING_WINDOW_COPY_MAXIMUM_LENGTH;
        } else {
            sliding_window_copy_maximum_length = input_size - input_offset;
        }

        size_t sliding_window_maximum_offset = 0;

        // Find the maximum offset of the sliding window copy, e.g. how far back can we go to copy bytes.
        if (input_offset >= SLIDING_WINDOW_SIZE_EFFICIENT) {
            sliding_window_maximum_offset = SLIDING_WINDOW_SIZE_EFFICIENT;
        } else {
            sliding_window_maximum_offset = input_offset;
        }

        size_t rle_window_maximum_length = 0;

        // Find the maximum length of the RLE window, e.g. how many bytes can be matched without going out of bounds.
        if ((input_size - input_offset) >= RLE_LONG_MAXIMUM_LENGTH) {
            rle_window_maximum_length = RLE_LONG_MAXIMUM_LENGTH;
        } else {
            rle_window_maximum_length = input_size - input_offset;
        }

        size_t sliding_window_match_offset = 0;
        size_t sliding_window_match_length = 0;

        // Find the longest match in the sliding window.
        for (size_t i = 1; i <= sliding_window_maximum_offset; i++) {
            size_t match_length = 0;

            for (size_t j = 0; j < sliding_window_copy_maximum_length; j++) {
                if (input_buffer[input_offset - i + j] == input_buffer[input_offset + j]) {
                    match_length++;
                } else {
                    break;
                }
            }

            if (match_length > sliding_window_match_length) {
                sliding_window_match_offset = i;
                sliding_window_match_length = match_length;
            }
        }

        size_t rle_match_value = 0;
        size_t rle_match_length = 0;

        // Find the longest match in the RLE window.
        {
            size_t match_value = input_buffer[input_offset];
            size_t match_length = 0;

            if (match_value != 0x00 && rle_window_maximum_length > RLE_SHORT_MAXIMUM_LENGTH) {
                // We are matching a non-zero value, so the maximum length we are able to match is the maximum copy length.
                rle_window_maximum_length = RLE_SHORT_MAXIMUM_LENGTH;
            }

            for (size_t i = 0; i < rle_window_maximum_length; i++) {
                if (input_buffer[input_offset + i] == match_value) {
                    match_length++;
                } else {
                    break;
                }
            }

            if (match_length > rle_match_length) {
                rle_match_value = match_value;
                rle_match_length = match_length;
            }
        }

        u8 command = COMMAND_UNDEFINED;

        // Try to pick a command that works best with the values calculated above.
        if (sliding_window_match_length >= 3) {
            command = COMMAND_SLIDING_WINDOW_COPY; // Takes up 2 bytes.
        } else if (rle_match_length >= 3) {
            if (rle_match_value == 0x00) {
                if (rle_match_length <= RLE_SHORT_MAXIMUM_LENGTH) {
                    command = COMMAND_RLE_WRITE_SHORT_ZERO; // Takes up 1 byte.
                } else if (rle_match_length <= RLE_LONG_MAXIMUM_LENGTH) {
                    command = COMMAND_RLE_WRITE_LONG_ZERO; // Takes up 2 bytes.
                }
            } else {
                if (rle_match_length <= RLE_SHORT_MAXIMUM_LENGTH) {
                    command = COMMAND_RLE_WRITE_SHORT_ANY_VALUE; // Takes up 2 bytes.
                }
            }
        } else if (rle_match_length == 2) {
            if (rle_match_value == 0x00) {
                command = COMMAND_RLE_WRITE_SHORT_ZERO; // Takes up 1 byte.
            }
        }

        size_t raw_copy_length = input_offset - input_last_processed_data_offset;

        // Force a raw copy command under the following conditions:
        // 1. A command has been picked and there is raw data to copy. (This happens when a command couldn't be found for the data.)
        // 2. The raw data length is at the maximum length or greater.
        // 3. The input offset is at the end of the input buffer.
        if ((command != COMMAND_UNDEFINED && raw_copy_length > 0) || raw_copy_length >= RAW_COPY_MAXIMUM_LENGTH || (input_offset + 1) >= input_size) {
            if ((input_offset + 1) >= input_size) {
                raw_copy_length = input_size - input_last_processed_data_offset;
            }

            while (raw_copy_length > 0) {
                size_t length = 0;

                if (raw_copy_length > RAW_COPY_MAXIMUM_LENGTH) {
                    length = RAW_COPY_MAXIMUM_LENGTH;
                } else {
                    length = raw_copy_length;
                }

                output_buffer[output_offset++] = COMMAND_RAW_COPY | (length & COMMAND_RAW_COPY_LENGTH_MASK);

                for (size_t i = 0; i < length; i++) {
                    output_buffer[output_offset++] = input_buffer[input_last_processed_data_offset++];
                }

                raw_copy_length -= length;
            }
        }

        if (command == COMMAND_SLIDING_WINDOW_COPY) {
            output_buffer[output_offset++] = COMMAND_SLIDING_WINDOW_COPY | (((sliding_window_match_length - 2) << 2) & COMMAND_SLIDING_WINDOW_COPY_LENGTH_MASK) | ((sliding_window_match_offset >> 8) & COMMAND_SLIDING_WINDOW_COPY_OFFSET_FIRST_BYTE_MASK);
            output_buffer[output_offset++] = sliding_window_match_offset & COMMAND_SLIDING_WINDOW_COPY_OFFSET_SECOND_BYTE_MASK;

            input_offset += sliding_window_match_length;
            input_last_processed_data_offset = input_offset;
        } else if (command == COMMAND_RLE_WRITE_SHORT_ANY_VALUE) {
            while (rle_match_length > 0) {
                size_t length = 0;

                if (rle_match_length > RLE_SHORT_MAXIMUM_LENGTH) {
                    length = RLE_SHORT_MAXIMUM_LENGTH;
                } else {
                    length = rle_match_length;
                }

                output_buffer[output_offset++] = COMMAND_RLE_WRITE_SHORT_ANY_VALUE | ((length - 2) & COMMAND_RLE_WRITE_SHORT_ANY_VALUE_LENGTH_MASK);
                output_buffer[output_offset++] = rle_match_value;

                rle_match_length -= length;
                input_offset += length;
            }

            input_last_processed_data_offset = input_offset;
        } else if (command == COMMAND_RLE_WRITE_SHORT_ZERO) {
            while (rle_match_length > 0) {
                size_t length = 0;

                if (rle_match_length > RLE_SHORT_MAXIMUM_LENGTH) {
                    length = RLE_SHORT_MAXIMUM_LENGTH;
                } else {
                    length = rle_match_length;
                }

                output_buffer[output_offset++] = COMMAND_RLE_WRITE_SHORT_ZERO | ((length - 2) & COMMAND_RLE_WRITE_SHORT_ZERO_LENGTH_MASK);

                rle_match_length -= length;
                input_offset += length;
            }

            input_last_processed_data_offset = input_offset;
        } else if (command == COMMAND_RLE_WRITE_LONG_ZERO) {
            while (rle_match_length > 0) {
                size_t length = 0;

                if (rle_match_length > RLE_LONG_MAXIMUM_LENGTH) {
                    length = RLE_LONG_MAXIMUM_LENGTH;
                } else {
                    length = rle_match_length;
                }

                output_buffer[output_offset++] = COMMAND_RLE_WRITE_LONG_ZERO;
                output_buffer[output_offset++] = (length - 2) & COMMAND_RLE_WRITE_LONG_ZERO_LENGTH_MASK;

                rle_match_length -= length;
                input_offset += length;
            }

            input_last_processed_data_offset = input_offset;
        } else {
            input_offset++;
        }
    }

    // Write the compressed size into the first 4 bytes of the output buffer.
    output_buffer[0] = 0x00; // The first byte is always 0x00.
    output_buffer[1] = (output_offset >> 16) & 0xFF;
    output_buffer[2] = (output_offset >> 8) & 0xFF;
    output_buffer[3] = output_offset & 0xFF;
    
    // Return the output offset as the output size.
    return output_offset;
}

size_t lzkn64_compress_accurate(const u8 *input_buffer, u8 *output_buffer, size_t input_size) {
    size_t input_offset = 0;
    size_t output_offset = 4; // Skip the first 4 bytes since they are the compressed file size.

    size_t input_last_processed_data_offset = 0;

    while (input_offset < input_size) {
        size_t sliding_window_copy_maximum_length = 0;

        // Find the maximum length of the sliding window copy, e.g. how many bytes can be copied without going out of bounds.
        if ((input_size - input_offset) >= SLIDING_WINDOW_COPY_MAXIMUM_LENGTH) {
            sliding_window_copy_maximum_length = SLIDING_WINDOW_COPY_MAXIMUM_LENGTH;
        } else {
            sliding_window_copy_maximum_length = input_size - input_offset;
        }

        size_t sliding_window_maximum_offset = 0;

        // Find the maximum offset of the sliding window copy, e.g. how far back can we go to copy bytes.
        if (input_offset >= SLIDING_WINDOW_SIZE_ACCURATE) {
            sliding_window_maximum_offset = SLIDING_WINDOW_SIZE_ACCURATE;
        } else {
            sliding_window_maximum_offset = input_offset;
        }

        size_t rle_window_maximum_length = 0;

        // Find the maximum length of the RLE window, e.g. how many bytes can be matched without going out of bounds.
        if ((input_size - input_offset) >= RLE_LONG_MAXIMUM_LENGTH) {
            rle_window_maximum_length = RLE_LONG_MAXIMUM_LENGTH;
        } else {
            rle_window_maximum_length = input_size - input_offset;
        }
        
        //! First difference from the efficient algorithm.
        if (rle_window_maximum_length > RLE_SHORT_MAXIMUM_LENGTH) {
            for (size_t i = (RLE_SHORT_MAXIMUM_LENGTH + 1); i <= rle_window_maximum_length; i++) {
                size_t j = (input_offset + i) & 0xFFF;

                // Check every 0x400 to see if we are RLE_SHORT_MAXIMUM_LENGTH bytes away from the start of the 0x400 block.
                if (j % 0x400 == RLE_SHORT_MAXIMUM_LENGTH) {
                    rle_window_maximum_length = i;
                    break;
                }
            }
        }

        size_t sliding_window_match_offset = 0;
        size_t sliding_window_match_length = 0;

        // Find the longest match in the sliding window.
        for (size_t i = 1; i <= sliding_window_maximum_offset; i++) {
            size_t match_length = 0;

            for (size_t j = 0; j < sliding_window_copy_maximum_length; j++) {
                if (input_buffer[input_offset - i + j] == input_buffer[input_offset + j]) {
                    match_length++;
                } else {
                    break;
                }
            }

            if (match_length > sliding_window_match_length) {
                sliding_window_match_offset = i;
                sliding_window_match_length = match_length;
            }
        }

        size_t rle_match_value = 0;
        size_t rle_match_length = 0;

        // Find the longest match in the RLE window.
        {
            size_t match_value = input_buffer[input_offset];
            size_t match_length = 0;

            //! Second difference from the efficient algorithm. 
            // For some reason, when emitting a COMMAND_RLE_WRITE_SHORT_ANY_VALUE, the maximum length is RLE_SHORT_MAXIMUM_LENGTH - 1 instead of RLE_SHORT_MAXIMUM_LENGTH.
            if (match_value != 0x00 && rle_window_maximum_length > (RLE_SHORT_MAXIMUM_LENGTH - 1)) {
                // We are matching a non-zero value, so the maximum length we are able to match is the maximum copy length.
                rle_window_maximum_length = (RLE_SHORT_MAXIMUM_LENGTH - 1);
            }

            for (size_t i = 0; i < rle_window_maximum_length; i++) {
                if (input_buffer[input_offset + i] == match_value) {
                    match_length++;
                } else {
                    break;
                }
            }

            if (match_length > 0) {
                rle_match_value = match_value;
                rle_match_length = match_length;
            }
        }

        u8 command = COMMAND_UNDEFINED;

        //! Third difference from the efficient algorithm.
        // Try to pick a command that works best with the values calculated above.
        if (sliding_window_match_length >= 4 && sliding_window_match_length > rle_match_length) {
            command = COMMAND_SLIDING_WINDOW_COPY; // Takes up 2 bytes.
        } else if (rle_match_length >= 3) {
            if (rle_match_value == 0x00) {
                if (rle_match_length < RLE_SHORT_MAXIMUM_LENGTH) {
                    command = COMMAND_RLE_WRITE_SHORT_ZERO; // Takes up 1 byte.
                } else if (rle_match_length <= RLE_LONG_MAXIMUM_LENGTH) {
                    command = COMMAND_RLE_WRITE_LONG_ZERO; // Takes up 2 bytes.
                }
            } else {
                if (rle_match_length <= RLE_SHORT_MAXIMUM_LENGTH) {
                    command = COMMAND_RLE_WRITE_SHORT_ANY_VALUE; // Takes up 2 bytes.
                }
            }
        } else if (rle_match_length == 2) {
            if (rle_match_value == 0x00) {
                command = COMMAND_RLE_WRITE_SHORT_ZERO; // Takes up 1 byte.
            }
        }

        size_t raw_copy_length = input_offset - input_last_processed_data_offset;

        // Force a raw copy command under the following conditions:
        // 1. A command has been picked and there is raw data to copy. (This happens when a command couldn't be found for the data.)
        // 2. The raw data length is at the maximum length or greater.
        // 3. The input offset is at the end of the input buffer.
        if ((command != COMMAND_UNDEFINED && raw_copy_length > 0) || raw_copy_length >= RAW_COPY_MAXIMUM_LENGTH || (input_offset + 1) >= input_size) {
            if ((input_offset + 1) >= input_size) {
                raw_copy_length = input_size - input_last_processed_data_offset;
            }

            while (raw_copy_length > 0) {
                size_t length = 0;

                if (raw_copy_length > RAW_COPY_MAXIMUM_LENGTH) {
                    length = RAW_COPY_MAXIMUM_LENGTH;
                } else {
                    length = raw_copy_length;
                }

                output_buffer[output_offset++] = COMMAND_RAW_COPY | (length & COMMAND_RAW_COPY_LENGTH_MASK);

                for (size_t i = 0; i < length; i++) {
                    output_buffer[output_offset++] = input_buffer[input_last_processed_data_offset++];
                }

                raw_copy_length -= length;
            }
        }

        if (command == COMMAND_SLIDING_WINDOW_COPY) {
            output_buffer[output_offset++] = COMMAND_SLIDING_WINDOW_COPY | (((sliding_window_match_length - 2) << 2) & COMMAND_SLIDING_WINDOW_COPY_LENGTH_MASK) | ((sliding_window_match_offset >> 8) & COMMAND_SLIDING_WINDOW_COPY_OFFSET_FIRST_BYTE_MASK);
            output_buffer[output_offset++] = sliding_window_match_offset & COMMAND_SLIDING_WINDOW_COPY_OFFSET_SECOND_BYTE_MASK;

            input_offset += sliding_window_match_length;
            input_last_processed_data_offset = input_offset;
        } else if (command == COMMAND_RLE_WRITE_SHORT_ANY_VALUE) {
            while (rle_match_length > 0) {
                size_t length = 0;

                if (rle_match_length > RLE_SHORT_MAXIMUM_LENGTH) {
                    length = RLE_SHORT_MAXIMUM_LENGTH;
                } else {
                    length = rle_match_length;
                }

                output_buffer[output_offset++] = COMMAND_RLE_WRITE_SHORT_ANY_VALUE | ((length - 2) & COMMAND_RLE_WRITE_SHORT_ANY_VALUE_LENGTH_MASK);
                output_buffer[output_offset++] = rle_match_value;

                rle_match_length -= length;
                input_offset += length;
            }

            input_last_processed_data_offset = input_offset;
        } else if (command == COMMAND_RLE_WRITE_SHORT_ZERO) {
            while (rle_match_length > 0) {
                size_t length = 0;

                if (rle_match_length > RLE_SHORT_MAXIMUM_LENGTH) {
                    length = RLE_SHORT_MAXIMUM_LENGTH;
                } else {
                    length = rle_match_length;
                }

                output_buffer[output_offset++] = COMMAND_RLE_WRITE_SHORT_ZERO | ((length - 2) & COMMAND_RLE_WRITE_SHORT_ZERO_LENGTH_MASK);

                rle_match_length -= length;
                input_offset += length;
            }

            input_last_processed_data_offset = input_offset;
        } else if (command == COMMAND_RLE_WRITE_LONG_ZERO) {
            while (rle_match_length > 0) {
                size_t length = 0;

                if (rle_match_length > RLE_LONG_MAXIMUM_LENGTH) {
                    length = RLE_LONG_MAXIMUM_LENGTH;
                } else {
                    length = rle_match_length;
                }

                output_buffer[output_offset++] = COMMAND_RLE_WRITE_LONG_ZERO;
                output_buffer[output_offset++] = (length - 2) & COMMAND_RLE_WRITE_LONG_ZERO_LENGTH_MASK;

                rle_match_length -= length;
                input_offset += length;
            }

            input_last_processed_data_offset = input_offset;
        } else {
            input_offset++;
        }
    }

    // Write the compressed size into the first 4 bytes of the output buffer.
    output_buffer[0] = 0x00; // The first byte is always 0x00.
    output_buffer[1] = (output_offset >> 16) & 0xFF;
    output_buffer[2] = (output_offset >> 8) & 0xFF;
    output_buffer[3] = output_offset & 0xFF;

    // Return the output offset as the output size.
    return output_offset;
}

size_t lzkn64_decompress(const u8 *input_buffer, u8 *output_buffer, size_t input_size) {
    size_t input_offset = 4; // Skip the first 4 bytes since they are the compressed file size.
    size_t output_offset = 0;
    
    while (input_offset < input_size) {
        u8 command = input_buffer[input_offset++];

        if (command >= COMMAND_SLIDING_WINDOW_COPY_START && command <= COMMAND_SLIDING_WINDOW_COPY_END) {
            u8 length = (command & COMMAND_SLIDING_WINDOW_COPY_LENGTH_MASK) >> 2;
            u16 offset_first_byte = (command & COMMAND_SLIDING_WINDOW_COPY_OFFSET_FIRST_BYTE_MASK) << 8;
            u8 offset_second_byte = input_buffer[input_offset++];
            u16 offset = (offset_first_byte | offset_second_byte) & COMMAND_SLIDING_WINDOW_COPY_OFFSET_MAX_MASK;

            // Add 2 to get the actual length since 2 is the minimum length.
            length += 2;
            
            for (size_t i = 0; i < length; i++) {
                output_buffer[output_offset] = output_buffer[output_offset - offset];
                output_offset++;
            }
        } else if (command >= COMMAND_RAW_COPY_START && command <= COMMAND_RAW_COPY_END) {
            u8 length = command & COMMAND_RAW_COPY_LENGTH_MASK;
            
            for (size_t i = 0; i < length; i++) {
                output_buffer[output_offset++] = input_buffer[input_offset++];
            }
        } else if (command >= COMMAND_RLE_WRITE_SHORT_ANY_VALUE_START && command <= COMMAND_RLE_WRITE_SHORT_ANY_VALUE_END) {
            u8 length = command & COMMAND_RLE_WRITE_SHORT_ANY_VALUE_LENGTH_MASK;
            u8 value = input_buffer[input_offset++];
 
            // Add 2 to get the actual length since 2 is the minimum length.
            length += 2;

            for (size_t i = 0; i < length; i++) {
                output_buffer[output_offset++] = value;
            }
        } else if (command >= COMMAND_RLE_WRITE_SHORT_ZERO_START && command <= COMMAND_RLE_WRITE_SHORT_ZERO_END) {
            u8 length = command & COMMAND_RLE_WRITE_SHORT_ZERO_LENGTH_MASK;

            // Add 2 to get the actual length since 2 is the minimum length.
            length += 2;

            for (size_t i = 0; i < length; i++) {
                output_buffer[output_offset++] = 0;
            }
        } else if (command == COMMAND_RLE_WRITE_LONG_ZERO) {
            u16 length = input_buffer[input_offset++] & COMMAND_RLE_WRITE_LONG_ZERO_LENGTH_MASK;

            // Add 2 to get the actual length since 2 is the minimum length.
            length += 2;

            for (size_t i = 0; i < length; i++) {
                output_buffer[output_offset++] = 0;
            }
        } else {
            // Invalid command.
        }
    }

    // Return the output offset as the output size.
    return output_offset;
}
