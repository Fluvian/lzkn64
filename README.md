# LZKN64

LZKN64 is a compression format commonly used in Konami's Nintendo 64 titles. This application allows you to compress uncompressed files to LZKN64 files, or decompress LZKN64 files.

## Format Documentation

This format combines a sliding window algorithm with an RLE algorithm. There are 3 "modes" available.

### Sliding Window Copy

This is the sliding window copy mode, it copies already decompressed bytes from the decompressed buffer for a length (**length + 2**) and displacement encoded in the command, 
the bit structure is as follows:

```
Command size in bytes: 2.
c: Command identifier.
o: Offset of the start of the sliding window.
n: Length to copy.

#0x00
#%0000 0000 0000 0000
 %cnnn nnoo oooo oooo
```

### Raw Data Copy

This is the raw data copy mode, it copies the following bytes in the compressed buffer for a length encoded in the command, the bit structure is as follows:

```
Command size in bytes: 1.
c: Command identifier.
n: Length to copy.

#0x80
#%1000 0000
 %cccn nnnn
```

### RLE Write

This is the RLE write mode, it has 3 different submodes, it writes either a zero or a specified value for a specific length.

**RLE Submode 1**: Write a value contained in the byte following the command for a length (**length + 2**) encoded in the command.

**RLE Submode 2**: Write zero for a length (**length + 2**) encoded in the command.

**RLE Submode 3**: Write zero for a length (**length + 2**) encoded in the following command.

```
Command size in bytes: 1 or 2.
c: Command identifier.
v: Value to write.
n: Length to copy.

<--- RLE Submode 1 --->

#0xA0
#%1010 0000 0000 0000
 %cccn nnnn vvvv vvvv

<--- RLE Submode 2 --->

#0xE0
#%1110 0000
 %cccn nnnn

<--- RLE Submode 3 --->

#0xFF
#%1111 1111 0000 0000
 %cccc cccc nnnn nnnn
```
