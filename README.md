# P3R Save EnDecryptor - Technical Analysis

## Overview

This tool converts Persona 3 Reload save files between Xbox Game Pass (encrypted) and Steam (decrypted) formats. The conversion process uses a custom encryption algorithm with a hardcoded key.

## Low-Level Conversion Process

### Save File Format Detection

The tool identifies save file types using magic numbers in the first 4 bytes:

- **Xbox Game Pass (Encrypted)**: `0x0B650015` 
- **Steam (Decrypted/GVAS)**: `0x53415647` ("GVAS" in ASCII)

### Encryption Key

```c
#define SAVE_KEY "ae5zeitaix1joowooNgie3fahP5Ohph"
```

The encryption uses a 32-character hardcoded key that cycles through each byte of the save file.

### Encryption Algorithm Details

#### Decrypt Operation (Xbox Game Pass → Steam)
```c
unsigned char decrypt_byte(unsigned char data, unsigned char key)
{
    unsigned char bVar1 = data ^ key;  // XOR with key
    return (bVar1 >> 4 & 3 | (bVar1 & 3) << 4 | bVar1 & 0xcc);
}
```

**Step-by-step breakdown:**
1. XOR the encrypted byte with the key character
2. Extract bits 4-5: `(bVar1 >> 4) & 3`
3. Extract bits 0-1 and shift left 4 positions: `(bVar1 & 3) << 4`
4. Preserve bits 2-3 and 6-7: `bVar1 & 0xcc`
5. Combine all parts with bitwise OR

#### Encrypt Operation (Steam → Xbox Game Pass)
```c
unsigned char encrypt_byte(unsigned char data, unsigned char key)
{
    return ((((data & 0xff) >> 4) & 3 | (data & 3) << 4 | data & 0xcc) ^ key);
}
```

**Step-by-step breakdown:**
1. Extract bits 4-5: `((data >> 4) & 3)`
2. Extract bits 0-1 and shift left 4 positions: `(data & 3) << 4`
3. Preserve bits 2-3 and 6-7: `data & 0xcc`
4. Combine all parts with bitwise OR
5. XOR the result with the key character

### Conversion Process Flow

#### Xbox Game Pass → Steam (Decryption)
```
1. Read encrypted save file
2. Verify magic number (0x0B650015)
3. For each byte in file:
   a. Get current key character (cycling through 32-char key)
   b. Apply decrypt_byte(encrypted_byte, key_char)
   c. Store decrypted byte
4. Output decrypted save with GVAS magic (0x53415647)
```

#### Steam → Xbox Game Pass (Encryption)
```
1. Read decrypted GVAS save file  
2. Verify magic number (0x53415647)
3. For each byte in file:
   a. Get current key character (cycling through 32-char key)
   b. Apply encrypt_byte(decrypted_byte, key_char)
   c. Store encrypted byte
4. Output encrypted save with Xbox magic (0x0B650015)
```

### Key Implementation Details

#### Key Cycling Logic
```c
size_t key_idx = 0;
for (size_t i = 0; i < filesize; ++i)
{
    if (key_idx >= g_keylen)  // g_keylen = 32
    {
        key_idx = 0;  // Reset to start of key
    }
    // Process byte with g_OrSaveKey[key_idx]
    key_idx++;
}
```

The key cycles every 32 bytes throughout the entire save file.

#### Bit Manipulation Visualization

For decrypt operation, if we have a byte `10110011`:
```
Original:     1 0 1 1 0 0 1 1
After XOR:    ? ? ? ? ? ? ? ?  (depends on key)
Bit reorder:  
- Bits 4-5 → Bits 0-1
- Bits 0-1 → Bits 4-5  
- Bits 2-3,6-7 stay in place
```

### Platform Differences

| Platform | Format | Magic Number | File State |
|----------|--------|--------------|------------|
| **Xbox Game Pass** | Encrypted | `0x0B650015` | Custom encrypted format |
| **Steam** | Decrypted | `0x53415647` | Standard GVAS format |

### Tool Usage Modes

#### Auto-Detection Mode (Build 15+)
```bash
p3r-save SaveData0001.sav
```
- Reads magic number to determine conversion direction
- Xbox → Steam: Decrypts and outputs `decrypt_out.sav`
- Steam → Xbox: Encrypts and outputs `encrypt_out.sav`

#### Explicit Mode (Build 11 Compatible)
```bash
p3r-save decrypt SaveData0001.sav  # Force Xbox → Steam
p3r-save encrypt SaveData0001.sav  # Force Steam → Xbox
p3r-save -d SaveData0001.sav       # Short flag for decrypt
p3r-save -e SaveData0001.sav       # Short flag for encrypt
```

### Security Considerations

- The encryption key is hardcoded and publicly visible
- This is **not** cryptographically secure encryption
- Purpose is platform compatibility, not data protection
- The bit manipulation adds obfuscation but minimal security

### File Integrity

The conversion process is **lossless** and **reversible**:
- Xbox → Steam → Xbox produces identical files
- Steam → Xbox → Steam produces identical files
- No data corruption occurs during conversion

## Implementation Notes

- Uses secure C functions (`fopen_s`, `strerror_s`) for better error handling
- Proper memory management with cleanup in all code paths
- File size validation (minimum 4 bytes for magic number detection)
- Comprehensive error messages for debugging

This technical analysis reveals that the "conversion" between Xbox Game Pass and Steam saves is essentially a format transformation using a custom encryption scheme, allowing save files to be used across both platforms.