// QQ Music QRC DES transform, adapted from github.com/jixunmoe-go/qrc (MIT License).
// The QRC format uses DES with little-endian block/key mapping; Windows BCrypt DES is not byte-compatible here.

#include "qrc_des.h"

#include <string.h>

static const uint8_t key_rnd_shifts[] = {
0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01,
};

static const uint8_t large_state_shifts[] = {
0x1a, 0x14, 0x0e, 0x08, 0x3a, 0x34, 0x2e, 0x28,
};

static const uint8_t sboxes[8][64] = {
{ 14, 0, 4, 15, 13, 7, 1, 4, 2, 14, 15, 2, 11, 13, 8, 1, 3, 10, 10, 6, 6, 12, 12, 11, 5, 9, 9, 5, 0, 3, 7, 8, 4, 15, 1, 12, 14, 8, 8, 2, 13, 4, 6, 9, 2, 1, 11, 7, 15, 5, 12, 11, 9, 3, 7, 14, 3, 10, 10, 0, 5, 6, 0, 13 },
	{ 15, 3, 1, 13, 8, 4, 14, 7, 6, 15, 11, 2, 3, 8, 4, 15, 9, 12, 7, 0, 2, 1, 13, 10, 12, 6, 0, 9, 5, 11, 10, 5, 0, 13, 14, 8, 7, 10, 11, 1, 10, 3, 4, 15, 13, 4, 1, 2, 5, 11, 8, 6, 12, 7, 6, 12, 9, 0, 3, 5, 2, 14, 15, 9 },
	{ 10, 13, 0, 7, 9, 0, 14, 9, 6, 3, 3, 4, 15, 6, 5, 10, 1, 2, 13, 8, 12, 5, 7, 14, 11, 12, 4, 11, 2, 15, 8, 1, 13, 1, 6, 10, 4, 13, 9, 0, 8, 6, 15, 9, 3, 8, 0, 7, 11, 4, 1, 15, 2, 14, 12, 3, 5, 11, 10, 5, 14, 2, 7, 12 },
	{ 7, 13, 13, 8, 14, 11, 3, 5, 0, 6, 6, 15, 9, 0, 10, 3, 1, 4, 2, 7, 8, 2, 5, 12, 11, 1, 12, 10, 4, 14, 15, 9, 10, 3, 6, 15, 9, 0, 0, 6, 12, 10, 11, 10, 7, 13, 13, 8, 15, 9, 1, 4, 3, 5, 14, 11, 5, 12, 2, 7, 8, 2, 4, 14 },
	{ 2, 14, 12, 11, 4, 2, 1, 12, 7, 4, 10, 7, 11, 13, 6, 1, 8, 5, 5, 0, 3, 15, 15, 10, 13, 3, 0, 9, 14, 8, 9, 6, 4, 11, 2, 8, 1, 12, 11, 7, 10, 1, 13, 14, 7, 2, 8, 13, 15, 6, 9, 15, 12, 0, 5, 9, 6, 10, 3, 4, 0, 5, 14, 3 },
	{ 12, 10, 1, 15, 10, 4, 15, 2, 9, 7, 2, 12, 6, 9, 8, 5, 0, 6, 13, 1, 3, 13, 4, 14, 14, 0, 7, 11, 5, 3, 11, 8, 9, 4, 14, 3, 15, 2, 5, 12, 2, 9, 8, 5, 12, 15, 3, 10, 7, 11, 0, 14, 4, 1, 10, 7, 1, 6, 13, 0, 11, 8, 6, 13 },
	{ 4, 13, 11, 0, 2, 11, 14, 7, 15, 4, 0, 9, 8, 1, 13, 10, 3, 14, 12, 3, 9, 5, 7, 12, 5, 2, 10, 15, 6, 8, 1, 6, 1, 6, 4, 11, 11, 13, 13, 8, 12, 1, 3, 4, 7, 10, 14, 7, 10, 9, 15, 5, 6, 0, 8, 15, 0, 14, 5, 2, 9, 3, 2, 12 },
	{ 13, 1, 2, 15, 8, 13, 4, 8, 6, 10, 15, 3, 11, 7, 1, 4, 10, 12, 9, 5, 3, 6, 14, 11, 5, 0, 0, 14, 12, 9, 7, 2, 7, 2, 11, 1, 4, 14, 1, 7, 9, 4, 12, 10, 14, 8, 2, 13, 0, 15, 6, 12, 10, 9, 13, 0, 15, 3, 3, 5, 5, 6, 8, 11 },
};

static const uint8_t p_box[] = {
0x0f, 0x06, 0x13, 0x14, 0x1c, 0x0b, 0x1b, 0x10, 0x00, 0x0e, 0x16, 0x19, 0x04, 0x11, 0x1e, 0x09, 
	0x01, 0x07, 0x17, 0x0d, 0x1f, 0x1a, 0x02, 0x08, 0x12, 0x0c, 0x1d, 0x05, 0x15, 0x0a, 0x03, 0x18,
};

static const uint8_t ip[] = {
0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01, 0x3b, 0x33, 0x2b, 0x23, 0x1b, 0x13, 0x0b, 0x03, 
	0x3d, 0x35, 0x2d, 0x25, 0x1d, 0x15, 0x0d, 0x05, 0x3f, 0x37, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07, 
	0x38, 0x30, 0x28, 0x20, 0x18, 0x10, 0x08, 0x00, 0x3a, 0x32, 0x2a, 0x22, 0x1a, 0x12, 0x0a, 0x02, 
	0x3c, 0x34, 0x2c, 0x24, 0x1c, 0x14, 0x0c, 0x04, 0x3e, 0x36, 0x2e, 0x26, 0x1e, 0x16, 0x0e, 0x06,
};

static const uint8_t ip_inv[] = {
0x27, 0x07, 0x2f, 0x0f, 0x37, 0x17, 0x3f, 0x1f, 0x26, 0x06, 0x2e, 0x0e, 0x36, 0x16, 0x3e, 0x1e, 
	0x25, 0x05, 0x2d, 0x0d, 0x35, 0x15, 0x3d, 0x1d, 0x24, 0x04, 0x2c, 0x0c, 0x34, 0x14, 0x3c, 0x1c, 
	0x23, 0x03, 0x2b, 0x0b, 0x33, 0x13, 0x3b, 0x1b, 0x22, 0x02, 0x2a, 0x0a, 0x32, 0x12, 0x3a, 0x1a, 
	0x21, 0x01, 0x29, 0x09, 0x31, 0x11, 0x39, 0x19, 0x20, 0x00, 0x28, 0x08, 0x30, 0x10, 0x38, 0x18,
};

static const uint8_t key_permutation_table[] = {
0x38, 0x30, 0x28, 0x20, 0x18, 0x10, 0x08, 0x00, 0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01, 
	0x3a, 0x32, 0x2a, 0x22, 0x1a, 0x12, 0x0a, 0x02, 0x3b, 0x33, 0x2b, 0x23, 0x3e, 0x36, 0x2e, 0x26, 
	0x1e, 0x16, 0x0e, 0x06, 0x3d, 0x35, 0x2d, 0x25, 0x1d, 0x15, 0x0d, 0x05, 0x3c, 0x34, 0x2c, 0x24, 
	0x1c, 0x14, 0x0c, 0x04, 0x1b, 0x13, 0x0b, 0x03,
};

static const uint8_t key_compression[] = {
0x0d, 0x10, 0x0a, 0x17, 0x00, 0x04, 0x02, 0x1b, 0x0e, 0x05, 0x14, 0x09, 0x16, 0x12, 0x0b, 0x03, 
	0x19, 0x07, 0x0f, 0x06, 0x1a, 0x13, 0x0c, 0x01, 0x2d, 0x38, 0x23, 0x29, 0x33, 0x3b, 0x22, 0x2c, 
	0x37, 0x31, 0x25, 0x34, 0x30, 0x35, 0x2b, 0x3c, 0x26, 0x39, 0x32, 0x2e, 0x36, 0x28, 0x21, 0x24,
};

static const uint8_t key_expansion[] = {
0x1f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x07, 0x08, 0x09, 0x0a, 
	0x0b, 0x0c, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x13, 0x14, 
	0x15, 0x16, 0x17, 0x18, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x00,
};

static const uint64_t des_shift_table_cache[] = {
0x0000000080000000, 0x0000000040000000, 0x0000000020000000, 0x0000000010000000, 
	0x0000000008000000, 0x0000000004000000, 0x0000000002000000, 0x0000000001000000, 
	0x0000000000800000, 0x0000000000400000, 0x0000000000200000, 0x0000000000100000, 
	0x0000000000080000, 0x0000000000040000, 0x0000000000020000, 0x0000000000010000, 
	0x0000000000008000, 0x0000000000004000, 0x0000000000002000, 0x0000000000001000, 
	0x0000000000000800, 0x0000000000000400, 0x0000000000000200, 0x0000000000000100, 
	0x0000000000000080, 0x0000000000000040, 0x0000000000000020, 0x0000000000000010, 
	0x0000000000000008, 0x0000000000000004, 0x0000000000000002, 0x0000000000000001, 
	0x8000000000000000, 0x4000000000000000, 0x2000000000000000, 0x1000000000000000, 
	0x0800000000000000, 0x0400000000000000, 0x0200000000000000, 0x0100000000000000, 
	0x0080000000000000, 0x0040000000000000, 0x0020000000000000, 0x0010000000000000, 
	0x0008000000000000, 0x0004000000000000, 0x0002000000000000, 0x0001000000000000, 
	0x0000800000000000, 0x0000400000000000, 0x0000200000000000, 0x0000100000000000, 
	0x0000080000000000, 0x0000040000000000, 0x0000020000000000, 0x0000010000000000, 
	0x0000008000000000, 0x0000004000000000, 0x0000002000000000, 0x0000001000000000, 
	0x0000000800000000, 0x0000000400000000, 0x0000000200000000, 0x0000000100000000,
};


static uint64_t make_u64(uint32_t hi32, uint32_t lo32)
{
    return ((uint64_t)hi32 << 32) | (uint64_t)lo32;
}

static uint64_t swap_u64_side(uint64_t value)
{
    return (value >> 32) | (value << 32);
}

static uint32_t u64_get_lo32(uint64_t value)
{
    return (uint32_t)value;
}

static uint32_t u64_get_hi32(uint64_t value)
{
    return (uint32_t)(value >> 32);
}

static uint64_t get_u64_by_shift_idx(uint8_t value)
{
    return des_shift_table_cache[value & 0x3f];
}

static void map_bit(uint64_t* result, uint64_t src, uint8_t check, uint8_t set)
{
    if ((get_u64_by_shift_idx(check) & src) != 0)
        *result |= get_u64_by_shift_idx(set);
}

static uint32_t map_u32_bits(uint32_t src_value, const uint8_t* table, size_t table_size)
{
    uint64_t result = 0;
    for (size_t i = 0; i < table_size; ++i)
        map_bit(&result, (uint64_t)src_value, table[i], (uint8_t)i);
    return (uint32_t)result;
}

static uint64_t map_u64(uint64_t src_value, const uint8_t* table, size_t table_size)
{
    size_t mid_idx = table_size / 2;
    uint64_t lo32 = 0;
    uint64_t hi32 = 0;
    for (size_t i = 0; i < mid_idx; ++i)
        map_bit(&lo32, src_value, table[i], (uint8_t)i);
    for (size_t i = 0; i < table_size - mid_idx; ++i)
        map_bit(&hi32, src_value, table[mid_idx + i], (uint8_t)i);
    return make_u64((uint32_t)hi32, (uint32_t)lo32);
}

static uint64_t read_le64(const uint8_t* data)
{
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i)
        value = (value << 8) | data[i];
    return value;
}

static void write_le64(uint8_t* data, uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        data[i] = (uint8_t)(value & 0xff);
        value >>= 8;
    }
}

static void update_param(uint32_t* param, uint8_t shift_left)
{
    uint8_t shift_right = (uint8_t)(28 - shift_left);
    *param = (*param << shift_left) | ((*param >> shift_right) & 0xFFFFFFF0u);
}

static uint64_t des_ip(uint64_t data)
{
    return map_u64(data, ip, sizeof(ip));
}

static uint64_t des_ip_inv(uint64_t data)
{
    return map_u64(data, ip_inv, sizeof(ip_inv));
}

static uint32_t sbox_transform(uint64_t state)
{
    uint32_t result = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        uint64_t sbox_idx = (state >> large_state_shifts[i]) & 0x3f;
        result = (result << 4) | sboxes[i][sbox_idx];
    }
    return result;
}

static uint64_t des_crypt_proc(uint64_t state, uint64_t key)
{
    uint32_t state_hi32 = u64_get_hi32(state);
    uint32_t state_lo32 = u64_get_lo32(state);

    state = map_u64(make_u64(state_hi32, state_hi32), key_expansion, sizeof(key_expansion));
    state ^= key;

    uint32_t next_lo32 = sbox_transform(state);
    next_lo32 = map_u32_bits(next_lo32, p_box, sizeof(p_box));
    next_lo32 ^= state_lo32;

    return make_u64(next_lo32, state_hi32);
}

static void set_key(uint64_t subkeys[16], const uint8_t key_bytes[8], bool encrypt)
{
    uint64_t key = read_le64(key_bytes);
    uint64_t param = map_u64(key, key_permutation_table, sizeof(key_permutation_table));
    uint32_t param_c = u64_get_lo32(param);
    uint32_t param_d = u64_get_hi32(param);

    for (size_t i = 0; i < 16; ++i)
    {
        size_t subkey_idx = encrypt ? i : (15 - i);
        update_param(&param_c, key_rnd_shifts[i]);
        update_param(&param_d, key_rnd_shifts[i]);
        subkeys[subkey_idx] = map_u64(make_u64(param_d, param_c), key_compression, sizeof(key_compression));
    }
}

static uint64_t transform_block(uint64_t data, const uint64_t subkeys[16])
{
    uint64_t state = des_ip(data);
    for (size_t i = 0; i < 16; ++i)
        state = des_crypt_proc(state, subkeys[i]);
    state = swap_u64_side(state);
    return des_ip_inv(state);
}

bool qrc_des_transform(uint8_t* data, size_t size, const uint8_t key[8], bool encrypt)
{
    if (!data || !key || size % 8 != 0)
        return false;

    uint64_t subkeys[16] = {};
    set_key(subkeys, key, encrypt);
    for (size_t i = 0; i < size; i += 8)
    {
        uint64_t value = read_le64(data + i);
        value = transform_block(value, subkeys);
        write_le64(data + i, value);
    }
    return true;
}
