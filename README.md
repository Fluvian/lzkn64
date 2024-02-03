# LZKN64

LZKN64 (also known as Nagano) is a compression format commonly used in Konami's Nintendo 64 titles. This application allows you to compress uncompressed files to LZKN64 files, or decompress LZKN64 files.

*As of 2023-12-11, the compression algorithm fully matches the one Konami used in their tools, meaning files which have been decompressed and recompressed from the games match the original in the ROM file. Huge thanks to **[LiquidCat64](https://github.com/LiquidCat64)** for this!*

## Format Documentation

This format combines a sliding window algorithm with an RLE algorithm. There are 3 modes available.

All symbols below are explained here:
* **Size**: Size of the command and all values in bits
* **Range**: Range of byte values that actually determine the command
* **c**: Command
* **n**: Length to copy
* **o**: Displacement in buffer
* **v**: Value to write
* **x**: Any possible binary digit

### Sliding Window Copy

This is the sliding window copy mode, it copies already decompressed bytes from the decompressed buffer by using a displacement value specified by the command and using that as an offset to go backwards into the buffer. Afterwards it goes forwards and starts to append bytes for the specified length in the command to the front of the buffer.

```
Size: 16 bits
Range: 0x00xx <---> 0x7Fxx

0xxx xxxx xxxx xxxx
cnnn nnoo oooo oooo
```

### Raw Data Copy

This is the raw data copy mode, it copies the bytes following the command in the compressed buffer for a specified length.

```
Size: 8 bits
Range: 0x80 <---> 0x9F  

100x xxxx
cccn nnnn
```

### RLE Write

This is the RLE write mode, it has 3 different submodes, it writes either a zero value or a specified value for a specified length.

**RLE Submode 1**: 
Write a value which is encoded in the next byte for a length encoded in the command.

```
Size: 16 bits
Range: 0xC0xx <---> 0xDFxx

101x xxxx xxxx xxxx
cccn nnnn vvvv vvvv
```

**RLE Submode 2**: 
Write a zero value for a length encoded in the command.

```
Size: 8 bits
Range: 0xE0 <---> 0xFE

111x xxxx
cccn nnnn
```

**RLE Submode 3**: 
Write a zero value for a length encoded in the next byte.

```
Size: 16 bits
Range: 0xFFxx

1111 1111 xxxx xxxx
cccc cccc nnnn nnnn
```
