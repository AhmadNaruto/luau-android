#include "crypto_module.h"
#include "lualib.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

// ─── Hex helper ───────────────────────────────────────────────────────────────

static void bin_to_hex(const uint8_t *src, size_t len, char *out) {
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = HEX[src[i] >> 4];
        out[i * 2 + 1] = HEX[src[i] & 0xF];
    }
    out[len * 2] = '\0';
}

// ─── MD5 (RFC 1321) ───────────────────────────────────────────────────────────

#define MD5_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static const uint32_t MD5_T[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static const uint8_t MD5_S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5, 9,14,20,5, 9,14,20,5, 9,14,20,5, 9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t M[16];
    for (int i = 0; i < 16; i++) {
        M[i] = ((uint32_t)block[i*4])     | ((uint32_t)block[i*4+1]<<8) |
               ((uint32_t)block[i*4+2]<<16) | ((uint32_t)block[i*4+3]<<24);
    }
    for (int i = 0; i < 64; i++) {
        uint32_t F, g;
        if (i < 16)      { F = (b & c) | (~b & d); g = (uint32_t)i; }
        else if (i < 32) { F = (d & b) | (~d & c); g = (5*(uint32_t)i + 1) % 16; }
        else if (i < 48) { F = b ^ c ^ d;           g = (3*(uint32_t)i + 5) % 16; }
        else             { F = c ^ (b | ~d);         g = (7*(uint32_t)i) % 16; }
        F += a + MD5_T[i] + M[g];
        a = d; d = c; c = b; b = b + MD5_ROTL(F, MD5_S[i]);
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5(const uint8_t *data, size_t len, uint8_t out[16]) {
    uint32_t state[4] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    uint8_t block[64];
    size_t i;
    for (i = 0; i + 64 <= len; i += 64) md5_transform(state, data + i);
    size_t rem = len - i;
    memcpy(block, data + i, rem);
    block[rem++] = 0x80;
    if (rem > 56) { memset(block + rem, 0, 64 - rem); md5_transform(state, block); rem = 0; }
    memset(block + rem, 0, 56 - rem);
    uint64_t bits = (uint64_t)len * 8;
    for (int j = 0; j < 8; j++) block[56 + j] = (uint8_t)(bits >> (j * 8));
    md5_transform(state, block);
    for (int j = 0; j < 4; j++) {
        out[j*4]   = state[j] & 0xFF;
        out[j*4+1] = (state[j] >> 8)  & 0xFF;
        out[j*4+2] = (state[j] >> 16) & 0xFF;
        out[j*4+3] = (state[j] >> 24) & 0xFF;
    }
}

// ─── SHA-1 (FIPS 180-4) ──────────────────────────────────────────────────────

#define SHA1_ROTL(x,n) (((x)<<(n))|((x)>>(32-(n))))

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t W[80], a,b,c,d,e,f,k,temp;
    for (int i=0;i<16;i++) W[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for (int i=16;i<80;i++) W[i]=SHA1_ROTL(W[i-3]^W[i-8]^W[i-14]^W[i-16],1);
    a=state[0];b=state[1];c=state[2];d=state[3];e=state[4];
    for (int i=0;i<80;i++){
        if(i<20){f=(b&c)|(~b&d);k=0x5a827999;}
        else if(i<40){f=b^c^d;k=0x6ed9eba1;}
        else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}
        else{f=b^c^d;k=0xca62c1d6;}
        temp=SHA1_ROTL(a,5)+f+e+k+W[i];
        e=d;d=c;c=SHA1_ROTL(b,30);b=a;a=temp;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
    uint32_t state[5]={0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    uint8_t block[64];
    size_t i;
    for(i=0;i+64<=len;i+=64) sha1_transform(state,data+i);
    size_t rem=len-i; memcpy(block,data+i,rem); block[rem++]=0x80;
    if(rem>56){memset(block+rem,0,64-rem);sha1_transform(state,block);rem=0;}
    memset(block+rem,0,56-rem);
    uint64_t bits=(uint64_t)len*8;
    for(int j=7;j>=0;j--) block[56+7-j]=(uint8_t)(bits>>(j*8));
    sha1_transform(state,block);
    for(int j=0;j<5;j++){out[j*4]=(uint8_t)(state[j]>>24);out[j*4+1]=(uint8_t)(state[j]>>16);out[j*4+2]=(uint8_t)(state[j]>>8);out[j*4+3]=(uint8_t)state[j];}
}

// ─── SHA-256 (FIPS 180-4) ────────────────────────────────────────────────────

static const uint32_t SHA256_K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)  (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (ROTR32(x,2)^ROTR32(x,13)^ROTR32(x,22))
#define EP1(x) (ROTR32(x,6)^ROTR32(x,11)^ROTR32(x,25))
#define SIG0(x)(ROTR32(x,7)^ROTR32(x,18)^((x)>>3))
#define SIG1(x)(ROTR32(x,17)^ROTR32(x,19)^((x)>>10))

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t W[64],a,b,c,d,e,f,g,h,t1,t2;
    for(int i=0;i<16;i++) W[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for(int i=16;i<64;i++) W[i]=SIG1(W[i-2])+W[i-7]+SIG0(W[i-15])+W[i-16];
    a=state[0];b=state[1];c=state[2];d=state[3];e=state[4];f=state[5];g=state[6];h=state[7];
    for(int i=0;i<64;i++){t1=h+EP1(e)+CH(e,f,g)+SHA256_K[i]+W[i];t2=EP0(a)+MAJ(a,b,c);h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
}

static void sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint32_t state[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t block[64];
    size_t i;
    for(i=0;i+64<=len;i+=64) sha256_transform(state,data+i);
    size_t rem=len-i; memcpy(block,data+i,rem); block[rem++]=0x80;
    if(rem>56){memset(block+rem,0,64-rem);sha256_transform(state,block);rem=0;}
    memset(block+rem,0,56-rem);
    uint64_t bits=(uint64_t)len*8;
    for(int j=7;j>=0;j--) block[56+7-j]=(uint8_t)(bits>>(j*8));
    sha256_transform(state,block);
    for(int j=0;j<8;j++){out[j*4]=(uint8_t)(state[j]>>24);out[j*4+1]=(uint8_t)(state[j]>>16);out[j*4+2]=(uint8_t)(state[j]>>8);out[j*4+3]=(uint8_t)state[j];}
}

// ─── SHA-512 (FIPS 180-4) ────────────────────────────────────────────────────

static const uint64_t SHA512_K[80]={
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};

#define ROTR64(x,n) (((x)>>(n))|((x)<<(64-(n))))
#define CH64(x,y,z)  (((x)&(y))^(~(x)&(z)))
#define MAJ64(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP064(x) (ROTR64(x,28)^ROTR64(x,34)^ROTR64(x,39))
#define EP164(x) (ROTR64(x,14)^ROTR64(x,18)^ROTR64(x,41))
#define SIG064(x)(ROTR64(x,1)^ROTR64(x,8)^((x)>>7))
#define SIG164(x)(ROTR64(x,19)^ROTR64(x,61)^((x)>>6))

static void sha512_transform(uint64_t state[8], const uint8_t block[128]) {
    uint64_t W[80],a,b,c,d,e,f,g,h,t1,t2;
    for(int i=0;i<16;i++){
        W[i]=0;
        for(int j=0;j<8;j++) W[i]=(W[i]<<8)|block[i*8+j];
    }
    for(int i=16;i<80;i++) W[i]=SIG164(W[i-2])+W[i-7]+SIG064(W[i-15])+W[i-16];
    a=state[0];b=state[1];c=state[2];d=state[3];e=state[4];f=state[5];g=state[6];h=state[7];
    for(int i=0;i<80;i++){
        t1=h+EP164(e)+CH64(e,f,g)+SHA512_K[i]+W[i];
        t2=EP064(a)+MAJ64(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
}

static void sha512(const uint8_t *data, size_t len, uint8_t out[64]) {
    uint64_t state[8]={
        0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL
    };
    uint8_t block[128];
    size_t i;
    for(i=0;i+128<=len;i+=128) sha512_transform(state,data+i);
    size_t rem=len-i; memcpy(block,data+i,rem); block[rem++]=0x80;
    if(rem>112){memset(block+rem,0,128-rem);sha512_transform(state,block);rem=0;}
    memset(block+rem,0,112-rem);
    // Length in bits as 128-bit big-endian (we only use low 64 bits)
    memset(block+112,0,8);
    uint64_t bits=(uint64_t)len*8;
    for(int j=7;j>=0;j--) block[120+7-j]=(uint8_t)(bits>>(j*8));
    sha512_transform(state,block);
    for(int j=0;j<8;j++){
        for(int k=7;k>=0;k--) out[j*8+7-k]=(uint8_t)(state[j]>>(k*8));
    }
}

// ─── HMAC (RFC 2104) ─────────────────────────────────────────────────────────

typedef void (*hash_fn_t)(const uint8_t*, size_t, uint8_t*);

static void hmac_generic(const uint8_t *key, size_t key_len,
                         const uint8_t *data, size_t data_len,
                         hash_fn_t hash_fn, size_t block_size, size_t digest_size,
                         uint8_t *out) {
    uint8_t k_pad[128] = {0};
    uint8_t tk[64];

    if (key_len > block_size) {
        hash_fn(key, key_len, tk);
        key = tk; key_len = digest_size;
    }
    memcpy(k_pad, key, key_len);

    // ipad
    uint8_t i_key_pad[128];
    for (size_t i = 0; i < block_size; i++) i_key_pad[i] = k_pad[i] ^ 0x36;

    // o_pad
    uint8_t o_key_pad[128];
    for (size_t i = 0; i < block_size; i++) o_key_pad[i] = k_pad[i] ^ 0x5c;

    // inner = hash(ipad || data)
    uint8_t *inner_input = (uint8_t*)malloc(block_size + data_len);
    memcpy(inner_input, i_key_pad, block_size);
    memcpy(inner_input + block_size, data, data_len);
    uint8_t inner_hash[64];
    hash_fn(inner_input, block_size + data_len, inner_hash);
    free(inner_input);

    // outer = hash(opad || inner)
    uint8_t *outer_input = (uint8_t*)malloc(block_size + digest_size);
    memcpy(outer_input, o_key_pad, block_size);
    memcpy(outer_input + block_size, inner_hash, digest_size);
    hash_fn(outer_input, block_size + digest_size, out);
    free(outer_input);
}

// ─── Random via /dev/urandom ─────────────────────────────────────────────────

static bool read_urandom(uint8_t *buf, size_t n) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, buf + done, n - done);
        if (r <= 0) { close(fd); return false; }
        done += (size_t)r;
    }
    close(fd);
    return true;
}

// ─── Lua bindings ────────────────────────────────────────────────────────────

static int push_digest(lua_State *L, const uint8_t *digest, size_t dlen, bool as_hex) {
    if (as_hex) {
        char hex[129];
        bin_to_hex(digest, dlen, hex);
        lua_pushstring(L, hex);
    } else {
        lua_pushlstring(L, (const char*)digest, dlen);
    }
    return 1;
}

static int crypto_md5(lua_State *L) {
    size_t len; const uint8_t *d = (const uint8_t*)luaL_checklstring(L, 1, &len);
    bool hex = lua_isnoneornil(L, 2) ? true : (bool)lua_toboolean(L, 2);
    uint8_t out[16]; md5(d, len, out);
    return push_digest(L, out, 16, hex);
}

static int crypto_sha1(lua_State *L) {
    size_t len; const uint8_t *d = (const uint8_t*)luaL_checklstring(L, 1, &len);
    bool hex = lua_isnoneornil(L, 2) ? true : (bool)lua_toboolean(L, 2);
    uint8_t out[20]; sha1(d, len, out);
    return push_digest(L, out, 20, hex);
}

static int crypto_sha256(lua_State *L) {
    size_t len; const uint8_t *d = (const uint8_t*)luaL_checklstring(L, 1, &len);
    bool hex = lua_isnoneornil(L, 2) ? true : (bool)lua_toboolean(L, 2);
    uint8_t out[32]; sha256(d, len, out);
    return push_digest(L, out, 32, hex);
}

static int crypto_sha512(lua_State *L) {
    size_t len; const uint8_t *d = (const uint8_t*)luaL_checklstring(L, 1, &len);
    bool hex = lua_isnoneornil(L, 2) ? true : (bool)lua_toboolean(L, 2);
    uint8_t out[64]; sha512(d, len, out);
    return push_digest(L, out, 64, hex);
}

static int crypto_hmac(lua_State *L) {
    size_t key_len, data_len;
    const uint8_t *key  = (const uint8_t*)luaL_checklstring(L, 1, &key_len);
    const uint8_t *data = (const uint8_t*)luaL_checklstring(L, 2, &data_len);
    const char *alg = luaL_optstring(L, 3, "sha256");
    bool as_hex = lua_isnoneornil(L, 4) ? true : (bool)lua_toboolean(L, 4);

    uint8_t out[64];
    size_t dlen;
    if (strcmp(alg, "md5") == 0) {
        hmac_generic(key, key_len, data, data_len, md5, 64, 16, out); dlen = 16;
    } else if (strcmp(alg, "sha1") == 0) {
        hmac_generic(key, key_len, data, data_len, sha1, 64, 20, out); dlen = 20;
    } else if (strcmp(alg, "sha256") == 0) {
        hmac_generic(key, key_len, data, data_len, sha256, 64, 32, out); dlen = 32;
    } else if (strcmp(alg, "sha512") == 0) {
        hmac_generic(key, key_len, data, data_len, sha512, 128, 64, out); dlen = 64;
    } else {
        luaL_error(L, "crypto.hmac: unknown algorithm '%s'", alg);
        return 0;
    }
    return push_digest(L, out, dlen, as_hex);
}

static int crypto_random(lua_State *L) {
    lua_Integer n = luaL_checkinteger(L, 1);
    if (n <= 0 || n > 65536) { luaL_error(L, "crypto.random: n must be 1..65536"); return 0; }
    uint8_t *buf = (uint8_t*)malloc((size_t)n);
    if (!buf) { luaL_error(L, "crypto.random: allocation failure"); return 0; }
    if (!read_urandom(buf, (size_t)n)) { free(buf); luaL_error(L, "crypto.random: /dev/urandom failed"); return 0; }
    lua_pushlstring(L, (const char*)buf, (size_t)n);
    free(buf);
    return 1;
}

static int crypto_uuid(lua_State *L) {
    uint8_t b[16];
    if (!read_urandom(b, 16)) { luaL_error(L, "crypto.uuid: /dev/urandom failed"); return 0; }
    b[6] = (b[6] & 0x0F) | 0x40; // version 4
    b[8] = (b[8] & 0x3F) | 0x80; // variant
    char uuid[37];
    snprintf(uuid, sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
        b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
    lua_pushstring(L, uuid);
    return 1;
}

// ─── Module open ─────────────────────────────────────────────────────────────

int luaopen_crypto(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, crypto_md5,    "md5");    lua_setfield(L, -2, "md5");
    lua_pushcfunction(L, crypto_sha1,   "sha1");   lua_setfield(L, -2, "sha1");
    lua_pushcfunction(L, crypto_sha256, "sha256"); lua_setfield(L, -2, "sha256");
    lua_pushcfunction(L, crypto_sha512, "sha512"); lua_setfield(L, -2, "sha512");
    lua_pushcfunction(L, crypto_hmac,   "hmac");   lua_setfield(L, -2, "hmac");
    lua_pushcfunction(L, crypto_random, "random"); lua_setfield(L, -2, "random");
    lua_pushcfunction(L, crypto_uuid,   "uuid");   lua_setfield(L, -2, "uuid");
    return 1;
}
