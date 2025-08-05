#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

char g_erronoMsg[256] = { 0 };
#define strerror_s_(buffer, sizeInBytes, errnum) \
    memset(buffer, 0, sizeInBytes);              \
    strerror_s(buffer, sizeInBytes, errnum)

unsigned char* read_file(const char* file_name, size_t* size)
{
    FILE* file = NULL;
    if (fopen_s(&file, file_name, "rb") != 0)
    {
        strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
        printf_s("Error opening read file: %s\n", g_erronoMsg);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* data = (unsigned char*)malloc(*size);
    if (!data)
    {
        strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
        printf_s("Error allocating memory: %s\n", g_erronoMsg);
        exit(EXIT_FAILURE);
    }

    if (fread(data, 1, *size, file) != *size)
    {
        strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
        printf_s("Error reading file: %s\n", g_erronoMsg);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    printf_s("read data from \"%s\" %zu bytes at %p\n", file_name, *size, (void*)data);
    return data;
}

void write_file(const char* file_name, const unsigned char* data, const size_t size)
{
    FILE* file = NULL;
    if (fopen_s(&file, file_name, "wb") != 0)
    {
        strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
        printf_s("Error opening write file: %s\n", g_erronoMsg);
        exit(EXIT_FAILURE);
    }

    if (fwrite(data, 1, size, file) != size)
    {
        strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
        printf_s("Error writing to file: %s\n", g_erronoMsg);
        exit(EXIT_FAILURE);
    }
    printf_s("write data from %p to \"%s\" %zu bytes\n", (void*)data, file_name, size);
    fclose(file);
}

#define SKIP_FILE "skip.txt"

static int check_skipfile_exist(void)
{
    FILE* file = NULL;
    if (fopen_s(&file, SKIP_FILE, "rb") != 0)
    {
        return 0;
    }
    fclose(file);
    printf_s("\nSkip file (" SKIP_FILE ") exist, closing instantly.");
    return 1;
}

#undef SKIP_FILE

// 0x14106A7D0 bytes 32 CA 0F B6 C1 0F B6 D1 C0 E8 04 80 E2 03 24 03 C0 E2 04 0A C2 80 E1 CC 0A C1 C3
unsigned char decrypt_byte(unsigned char data, unsigned char key)
{
    unsigned char bVar1 = data ^ key;
    return (bVar1 >> 4 & 3 | (bVar1 & 3) << 4 | bVar1 & 0xcc);
}

// 0x14106A7F0 bytes 0F B6 C1 44 0F B6 C1 C0 E8 04 41 80 E0 03 24 03 41 C0 E0 04 41 0A C0 80 E1 CC 0A C1 32 C2 C3
unsigned char encrypt_byte(unsigned char data, unsigned char key)
{
    return ((((data & 0xff) >> 4) & 3 | (data & 3) << 4 | data & 0xcc) ^ key);
}

// _WIN32 Only
void Sleep(int dwMilliseconds);

#define WAIT_TIME 10
#define ONE_SEC (1 * 1000) // 1ms * 1000

static void wait_program_quit(void)
{
#ifndef _DEBUG
    if (!check_skipfile_exist())
    {
        int s = 0;
        while (s < WAIT_TIME)
        {
            Sleep(ONE_SEC);
            s++;
            putchar('.');
        }
    }
#endif
}

static void show_invalid_arg(const char* program_name)
{
    printf_s("Invalid arguments\n"
             "Usage (auto-detect): %s <file>\n"
             "Usage (explicit):    %s [encrypt|decrypt] <file>\n"
             "Example: %s SaveData0001.sav\n"
             "Example: %s decrypt SaveData0001.sav\n\n"
             "Program will exit in %d seconds",
             program_name, program_name, program_name, program_name, WAIT_TIME);
    wait_program_quit();
    putchar('\n');
    exit(EXIT_FAILURE);
}

#undef ONE_SEC

#define SAVE_KEY "ae5zeitaix1joowooNgie3fahP5Ohph"
const char* g_OrSaveKey = SAVE_KEY;
const size_t g_keylen = (sizeof(SAVE_KEY) - 1);
#undef SAVE_KEY

enum
{
    ENCRYPT_GVAS_MAGIC = 0x0B650015,
    DECRYPT_GVAS_MAGIC = 0x53415647,
};

static void tell_save_magic(const uint32_t file_magic)
{
    switch (file_magic)
    {
        case ENCRYPT_GVAS_MAGIC:
        {
            printf_s("Detected encrypted save file. (0x%08x == 0x%08x)\n", file_magic, ENCRYPT_GVAS_MAGIC);
            break;
        }
        case DECRYPT_GVAS_MAGIC:
        {
            printf_s("Detected decrypted save file. (0x%08x == 0x%08x)\n", file_magic, DECRYPT_GVAS_MAGIC);
            break;
        }
        default:
        {
            printf_s("Unknown save file. (0x%08x)\n", file_magic);
            break;
        }
    }
}

static int check_magic(const unsigned char* data, const size_t data_size, const uint32_t magic, uint32_t* out_magic)
{
    if (data_size < sizeof(uint32_t))
    {
        return 0;
    }
    uint32_t file_magic = *((uint32_t*)data);
    if (out_magic)
    {
        *out_magic = file_magic;
    }
    return file_magic == magic;
}

int main(int argc, char** argv)
{
    printf_s("p3r-save: Built " __DATE__ " @ " __TIME__ "\n");
    if (argc < 2)
    {
        show_invalid_arg(argv[0]);
    }

    const char* save_path = NULL;
    int force_decrypt = 0;
    int force_encrypt = 0;
    
    // Handle backward compatibility with explicit encrypt/decrypt commands
    if (argc == 3 && 
        (strcmp(argv[1], "decrypt") == 0 || strcmp(argv[1], "-d") == 0))
    {
        force_decrypt = 1;
        save_path = argv[2];
    }
    else if (argc == 3 && 
             (strcmp(argv[1], "encrypt") == 0 || strcmp(argv[1], "-e") == 0))
    {
        force_encrypt = 1;
        save_path = argv[2];
    }
    else if (argc == 2)
    {
        // Check if this looks like a command rather than a filename
        if (strcmp(argv[1], "decrypt") == 0 || strcmp(argv[1], "-d") == 0 ||
            strcmp(argv[1], "encrypt") == 0 || strcmp(argv[1], "-e") == 0)
        {
            printf_s("Missing filename after %s command\n", argv[1]);
            show_invalid_arg(argv[0]);
        }
        // Auto-detect mode (new interface)
        save_path = argv[1];
    }
    else
    {
        show_invalid_arg(argv[0]);
    }

    if (save_path && *save_path != '\0')
    {
        size_t filesize = 0;
        unsigned char* save_data = read_file(save_path, &filesize);
        uint32_t file_magic = 0;
        
        // Check if we can read the magic number
        if (filesize < sizeof(uint32_t))
        {
            printf_s("File too small to be a valid save file (need at least %zu bytes, got %zu)\n", 
                     sizeof(uint32_t), filesize);
            if (save_data)
            {
                free(save_data);
            }
            exit(EXIT_FAILURE);
        }
        
        if (force_decrypt || 
            (!force_encrypt && check_magic(save_data, filesize, ENCRYPT_GVAS_MAGIC, &file_magic)))
        {
            if (!force_decrypt)
            {
                tell_save_magic(file_magic);
            }
            else
            {
                printf_s("Forced decryption mode\n");
            }
            
            unsigned char* decrypted_data = (unsigned char*)malloc(filesize);
            if (!decrypted_data)
            {
                strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
                printf_s("Error allocating memory: %s\n", g_erronoMsg);
                exit(EXIT_FAILURE);
            }

            size_t key_idx = 0;

            for (size_t i = 0; i < filesize; ++i)
            {
                if (key_idx >= g_keylen)
                {
                    // reset index
                    key_idx = 0;
                }
                decrypted_data[i] = decrypt_byte(save_data[i], g_OrSaveKey[key_idx]);
                key_idx++;
            }

            write_file("decrypt_out.sav", decrypted_data, filesize);
            if (decrypted_data)
            {
                free(decrypted_data);
            }
        }
        else if (force_encrypt || 
                 check_magic(save_data, filesize, DECRYPT_GVAS_MAGIC, &file_magic))
        {
            if (!force_encrypt)
            {
                tell_save_magic(file_magic);
            }
            else
            {
                printf_s("Forced encryption mode\n");
            }
            
            unsigned char* encrypted_data = (unsigned char*)malloc(filesize);
            if (!encrypted_data)
            {
                strerror_s_(g_erronoMsg, sizeof(g_erronoMsg), errno);
                printf_s("Error allocating memory: %s\n", g_erronoMsg);
                exit(EXIT_FAILURE);
            }

            size_t key_idx = 0;

            for (size_t i = 0; i < filesize; ++i)
            {
                if (key_idx >= g_keylen)
                {
                    // reset index
                    key_idx = 0;
                }
                encrypted_data[i] = encrypt_byte(save_data[i], g_OrSaveKey[key_idx]);
                key_idx++;
            }

            write_file("encrypt_out.sav", encrypted_data, filesize);
            if (encrypted_data)
            {
                free(encrypted_data);
            }
        }
        else
        {
            tell_save_magic(file_magic);
            printf_s("Invalid save file or unrecognized format.\n");
            printf_s("Expected encrypted magic: 0x%08x or decrypted magic: 0x%08x\n", 
                     ENCRYPT_GVAS_MAGIC, DECRYPT_GVAS_MAGIC);
            printf_s("If you believe this is a valid save file, try using explicit mode:\n");
            printf_s("  %s decrypt %s  (to decrypt)\n", argv[0], save_path);
            printf_s("  %s encrypt %s  (to encrypt)\n", argv[0], save_path);
            if (save_data)
            {
                free(save_data);
            }
            exit(EXIT_FAILURE);
        }

        if (save_data)
        {
            printf_s("free save_data: %p\n", (void*)save_data);
            free(save_data);
        }
        else
        {
            printf_s("failed to free save_data: %p\n", (void*)save_data);
        }
    }
    else
    {
        show_invalid_arg(argv[0]);
    }
    printf_s("Program will exit in %d seconds", WAIT_TIME);
    wait_program_quit();
    putchar('\n');
    return 0;
}
