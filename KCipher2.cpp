#include<string.h>
#include"KCipher2.h"

/* Variables globales */


// Estado S
uint32_t A[5]; // cinco unidades de 32 bits
uint32_t B[11]; // once unidades de 32 bits
uint32_t L1, R1, L2, R2; // una unidad de 32 bits para cada

// La clave interna (IK) y el vector de inicialización (IV)
uint32_t IK[12]; // (12 * 32) bits
uint32_t IV[4]; // (4 * 32) bits

/**
* Do multiplication in GF(2#8) of the irreducible polynomial,
* f(x) = x#8 + x#4 + x#3 + x + 1. The given parameter is multiplied
* by 2.
* @param    t : (INPUT). 8 bits. The number will be multiplied by 2
* @return     : (OUTPUT). 8 bits. The multiplication result
*/
uint8_t GF_mult_by_2(uint8_t t)
{
    uint8_t q;
    uint32_t lq;

    lq = t << 1;
    if ((lq & 0x100) != 0) lq ^= 0x011B;
    q = lq ^ 0xFF;

    return q;
}

/**
* Do multiplication in GF(2#8) of the irreducible polynomial,
* f(x) = x#8 + x#4 + x#3 + x + 1. The given parameter is multiplied
* by 3.
* @param    t   : (INPUT). 8 bits. The number will be multiplied by 3
* @return       : (OUTPUT). 8 bits. The multiplication result
*/
uint8_t GF_mult_by_3(uint8_t t)
{
    uint8_t q;
    uint32_t lq;

    lq = (t << 1) ^ t;
    if ((lq & 0x100) != 0) lq ^= 0x011B;
    q = lq ^ 0xFF;

    return q;
}

/**
* Do substitution on a given input. See Section 2.4.2.
* @param    t   : (INPUT), (1*32) bits
* @return       : (OUTPUT), (1*32) bits
*/
uint32_t sub_k2(uint32_t in)
{
    uint32_t out;

    uint8_t w0 = in & 0x000000ff;
    uint8_t w1 = (in >> 8) & 0x000000ff;
    uint8_t w2 = (in >> 16) & 0x000000ff;
    uint8_t w3 = (in >> 24) & 0x000000ff;

    uint8_t t3, t2, t1, t0;
    uint8_t q3, q2, q1, q0;

    t0 = S_box[w0];
    t1 = S_box[w1];
    t2 = S_box[w2];
    t3 = S_box[w3];

    q0 = GF_mult_by_2(t0) ^ GF_mult_by_3(t1) ^ t2 ^ t3;
    q1 = t0 ^ GF_mult_by_2(t1) ^ GF_mult_by_3(t2) ^ t3;
    q2 = t0 ^ t1 ^ GF_mult_by_2(t2) ^ GF_mult_by_3(t3);
    q3 = GF_mult_by_3(t0) ^ t1 ^ t2 ^ GF_mult_by_2(t3);

    out = (q3 << 24) | (q2 << 16) | (q1 << 8) | q0;

    return out;
}

/**
* Expand a given 128-bit key (K) to a 384-bit internal key
* information (IK).
* See Step 1 of init() in Section 2.3.2.
* @param    key[4]  : (INPUT), (4*32) bits
* @param    iv[4]   : (INPUT), (4*32) bits
* @modify   IK[12]  : (OUTPUT), (12*32) bits
* @modify   IV[12]  : (OUTPUT), (4*32) bits
*/
void key_expansion(uint32_t *key, uint32_t * iv)
{
    // copy iv to IV
    IV[0] = iv[0];
    IV[1] = iv[1];
    IV[2] = iv[2];
    IV[3] = iv[3];

    // m = 0 ... 3
    IK[0] = key[0];
    IK[1] = key[1];
    IK[2] = key[2];
    IK[3] = key[3];
    // m = 4
    IK[4] = IK[0] ^ sub_k2((IK[3] << 8) ^ (IK[3] >> 24)) ^
            0x01000000;

    // m = 4 ... 11, but not 4 nor 8
    IK[5] = IK[1] ^ IK[4];
    IK[6] = IK[2] ^ IK[5];
    IK[7] = IK[3] ^ IK[6];

    // m = 8
    IK[8] = IK[4] ^ sub_k2((IK[7] << 8) ^ (IK[7] >> 24)) ^
            0x02000000;

    // m = 4 ... 11, but not 4 nor 8
    IK[9] = IK[5] ^ IK[8];
    IK[10] = IK[6] ^ IK[9];
    IK[11] = IK[7] ^ IK[10];
}

/**
* Set up the initial state value using IK and IV. See Step 2 of
* init() in Section 2.3.2.
* @param    key[4]  : (INPUT), (4*32) bits
* @param    iv[4]   : (INPUT), (4*32) bits
* @modify   S       : (OUTPUT), (A, B, L1, R1, L2, R2)
*/
void setup_state_values(uint32_t *key, uint32_t * iv)
{
    // setting up IK and IV by calling key_expansion(key, iv)
    key_expansion(key, iv);

    // setting up the internal state values
    A[0] = IK[4];
    A[1] = IK[3];
    A[2] = IK[2];
    A[3] = IK[1];
    A[4] = IK[0];

    B[0] = IK[10];
    B[1] = IK[11];
    B[2] = IV[0];
    B[3] = IV[1];
    B[4] = IK[8];
    B[5] = IK[9];
    B[6] = IV[2];
    B[7] = IV[3];
    B[8] = IK[7];
    B[9] = IK[5];
    B[10] = IK[6];

    L1 = R1 = L2 = R2 = 0x00000000;
}

/**
* Initialize the system with a 128-bit key (K) and a 128-bit
* initialization vector (IV). It sets up the internal state value
* and invokes next(INIT) iteratively 24 times. After this,
* the system is ready to produce key streams. See Section 2.3.2.
* @param    key[12] : (INPUT), (4*32) bits
* @param    iv[4]   : (INPUT), (4*32) bits
* @modify   IK      : (12*32) bits, by calling setup_state_values()
* @modify   IV      : (4*32) bits,  by calling setup_state_values()
* @modify   S       : (OUTPUT), (A, B, L1, R1, L2, R2)
*/
void init(uint32_t *k, uint32_t * iv)
{
    int i;
    setup_state_values(k, iv);

    for (i = 0; i < 24; i++) {
        next(INIT);
    }
}

/**
* Non-linear function. See Section 2.4.1.
* @param    A   : (INPUT), 8 bits
* @param    B   : (INPUT), 8 bits
* @param    C   : (INPUT), 8 bits
* @param    D   : (INPUT), 8 bits
* @return       : (OUTPUT), 8 bits
*/
uint32_t NLF(uint32_t A, uint32_t B,
             uint32_t C, uint32_t D)
{
    uint32_t Q;

    Q = (A + B) ^ C ^ D;

    return Q;
}

/**
* Derive a new state from the current state values.
* See Section 2.3.1.
* @param    mode    : (INPUT) INIT (= 0) or NORMAL (= 1)
* @modify   S       : (OUTPUT)
*/
void next(int mode)
{
    uint32_t nA[5];
    uint32_t nB[11];
    uint32_t nL1, nR1, nL2, nR2;
    uint32_t temp1, temp2;

    nL1 = sub_k2(R2 + B[4]);
    nR1 = sub_k2(L2 + B[9]);
    nL2 = sub_k2(L1);
    nR2 = sub_k2(R1);

    // m = 0 ... 3
    nA[0] = A[1];
    nA[1] = A[2];
    nA[2] = A[3];
    nA[3] = A[4];

    // m = 0 ... 9
    nB[0] = B[1];
    nB[1] = B[2];
    nB[2] = B[3];
    nB[3] = B[4];
    nB[4] = B[5];
    nB[5] = B[6];
    nB[6] = B[7];
    nB[7] = B[8];
    nB[8] = B[9];
    nB[9] = B[10];

    // update nA[4]
    temp1 = (A[0] << 8) ^ amul0[(A[0] >> 24)];
    nA[4] = temp1 ^ A[3];
    if (mode == INIT)
        nA[4] ^= NLF(B[0], R2, R1, A[4]);

    // update nB[10]
    if (A[2] & 0x40000000) { /* if A[2][30] == 1 */
        temp1 = (B[0] << 8) ^ amul1[(B[0] >> 24)];
    } else { /*if A[2][30] == 0*/
        temp1 = (B[0] << 8) ^ amul2[(B[0] >> 24)];
    }

    if (A[2] & 0x80000000) { /* if A[2][31] == 1 */
        temp2 = (B[8] << 8) ^ amul3[(B[8] >> 24)];
    } else { /* if A[2][31] == 0 */
        temp2 = B[8];
    }

    nB[10] = temp1 ^ B[1] ^ B[6] ^ temp2;

    if (mode == INIT)
        nB[10] ^= NLF(B[10], L2, L1, A[0]);

    /* copy S' to S */
    A[0] = nA[0];
    A[1] = nA[1];
    A[2] = nA[2];
    A[3] = nA[3];
    A[4] = nA[4];

    B[0] = nB[0];
    B[1] = nB[1];
    B[2] = nB[2];
    B[3] = nB[3];
    B[4] = nB[4];
    B[5] = nB[5];
    B[6] = nB[6];
    B[7] = nB[7];
    B[8] = nB[8];
    B[9] = nB[9];
    B[10] = nB[10];

    L1 = nL1;
    R1 = nR1;
    L2 = nL2;
    R2 = nR2;
}

/**
* Obtain a key stream = (ZH, ZL) from the current state values.
* See Section 2.3.3.
* @param    ZH  : (OUTPUT) (1 * 32)-bit
* @modify   ZL  : (OUTPUT) (1 * 32)-bit
*/
void stream(uint32_t *ZH, uint32_t * ZL)
{
    *ZH = NLF(B[10], L2, L1, A[0]);
    *ZL = NLF(B[0], R2, R1, A[4]);
}

void operar(uint8_t* ptr, unsigned int size)
{
    uint32_t zh, zl;

    int offset = 0;
    while (offset < size) {
        stream(&zh, &zl);

        if (offset + 8 > size) {

            uint32_t buffer[2];
            memcpy(buffer, ptr + offset, size - offset);
            for (int i = size - offset; i < 8; i++) {
                ((uint8_t*)&buffer)[i] = 0;
            }

            buffer[0] ^= zh;
            buffer[1] ^= zl;
            memcpy(ptr + offset, buffer, size - offset);

        } else {

            ((uint32_t*)(ptr + offset))[0] ^= zh;
            ((uint32_t*)(ptr + offset))[1] ^= zh;

        }

        offset += 8;
        next(NORMAL);
    }
}