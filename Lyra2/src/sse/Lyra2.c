/**
* Implementation of the Lyra2 Password Hashing Scheme (PHS). SSE-oriented implementation.
*
* Author: The Lyra PHC team (http://www.lyra-kdf.net/) -- 2014.
*
* This software is hereby placed in the public domain.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "Lyra2.h"
#include "Sponge.h"

/**
* Executes Lyra2 based on the G function from Blake2b. The number of columns of the memory matrix is set to nCols = 64.
* This version supports salts and passwords whose combined length is smaller than the size of the memory matrix,
* (i.e., (nRows x nCols x b) bits, where "b" is the underlying sponge's bitrate). In this implementation, the "basil" 
 * is composed by all integer parameters in the order they are provided, plus the value of nCols, 
 * (i.e., basil = kLen || pwdlen || saltlen || timeCost || nRows || nCols).
*
* @param out The derived key to be output by the algorithm
* @param outlen Desired key length
* @param in User password
* @param inlen Password length
* @param salt Salt
* @param saltlen Salt length
* @param t_cost Parameter to determine the processing time (T)
* @param m_cost Memory cost parameter (defines the number of rows of the memory matrix, R)
*
* @return 0 if the key is generated correctly; -1 if there is an error (usually due to lack of memory for allocation)
*/
int PHS(void *out, size_t outlen, const void *in, size_t inlen, const void *salt, size_t saltlen, unsigned int t_cost, unsigned int m_cost){
    return LYRA2(out, outlen, in, inlen, salt, saltlen, t_cost, m_cost, N_COLS);
}

void print128(__m128i *v){
    uint64_t *v1 = malloc(16 * sizeof (uint64_t));
    int i;

    _mm_store_si128( (__m128i *)&v1[0], (__m128i)v[0]);
    _mm_store_si128( (__m128i *)&v1[2], (__m128i)v[1]);
    _mm_store_si128( (__m128i *)&v1[4], (__m128i)v[2]);
    _mm_store_si128( (__m128i *)&v1[6], (__m128i)v[3]);
    _mm_store_si128( (__m128i *)&v1[8], (__m128i)v[4]);
    _mm_store_si128( (__m128i *)&v1[10], (__m128i)v[5]);
    _mm_store_si128( (__m128i *)&v1[12], (__m128i)v[6]);
    _mm_store_si128( (__m128i *)&v1[14], (__m128i)v[7]);

    for (i = 0; i < 16; i++) {
        printf("%ld|",v1[i]);
    }
    printf("\n");
}
/**
* Executes Lyra2 based on the G function from Blake2b. This version supports salts and passwords
* whose combined length is smaller than the size of the memory matrix, (i.e., (nRows x nCols x b) bits,
* where "b" is the underlying sponge's bitrate). In this implementation, the "basil" is composed by all 
 * integer parameters, in the order they are provided (i.e., basil = kLen || pwdlen || saltlen || timeCost || nRows || nCols).
*
* @param K The derived key to be output by the algorithm
* @param kLen Desired key length
* @param pwd User password
* @param pwdlen Password length
* @param salt Salt
* @param saltlen Salt length
* @param timeCost Parameter to determine the processing time (T)
* @param nRows Number or rows of the memory matrix (R)
* @param nCols Number of columns of the memory matrix (C)
*
* @return 0 if the key is generated correctly; -1 if there is an error (usually due to lack of memory for allocation)
*/
int LYRA2(unsigned char *K, int kLen, const unsigned char *pwd, int pwdlen, const unsigned char *salt, int saltlen, int timeCost, int nRows, int nCols) {
    int rowLenBytes = BLOCK_LEN_INT64 * nCols * 8;

    //Checks whether or not the salt+password are within the accepted limits
    if (pwdlen + saltlen > rowLenBytes) {
        return -1;
    }

    //============================= Basic variables ============================//
    int row = 2; //index of row to be processed
    int prev = 1; //index of prev (last row ever computed/modified)
    int rowa = 0; //index of row* (a previous row, deterministically picked during Setup and randomly picked during Wandering)
    int tau; //Time Loop iterator
    int i; //auxiliary iteration counter
    //==========================================================================/

    //========== Initializing the Memory Matrix and pointers to it =============//
    //Allocates enough space for the whole memory matrix
    __m128i *wholeMatrix = malloc(nRows * rowLenBytes);
    if (wholeMatrix == NULL) {
        return -1;
    }
    //Allocates pointers to each row of the matrix
    __m128i **memMatrix = malloc(nRows * sizeof (uint64_t*));
    if (memMatrix == NULL) {
        return -1;
    }
    //Places the pointers in the correct positions
    __m128i *ptrWord = wholeMatrix;
    for (i = 0; i < nRows; i++) {
        memMatrix[i] = ptrWord;
        ptrWord += BLOCK_LEN_INT128 * nCols;
    }
    //==========================================================================/

    //============== Initializing the Sponge State =============/
    //Sponge state: 8 __m128i, BLOCK_LEN_INT128 words of them for the bitrate (b) and the remainder for the capacity (c)
    __m128i *state = malloc(8 * sizeof (__m128i));
    initStateSSE(state);
    //========================================================//

    //============= Getting the password + salt + basil padded with 10*1 ===============//

    //OBS.:The memory matrix will temporarily hold the password: not for saving memory,
    //but this ensures that the password copied locally will be overwritten as soon as possible

    //First, we clean enough blocks for the password, salt, basil and padding
    int nBlocksInput = ((saltlen + pwdlen + 6*sizeof(int)) / BLOCK_LEN_BYTES) + 1;
    byte *ptrByte = (byte*) wholeMatrix;
    memset(ptrByte, 0, nBlocksInput * BLOCK_LEN_BYTES);

    //Prepends the password
    memcpy(ptrByte, pwd, pwdlen);
    ptrByte += pwdlen;
    
    //Concatenates the salt
    memcpy(ptrByte, salt, saltlen);
    ptrByte += saltlen;
    
    //Concatenates the basil: every integer passed as parameter, in the order they are provided by the interface
    memcpy(ptrByte, &kLen, sizeof(int));
    ptrByte += sizeof(int);
    memcpy(ptrByte, &pwdlen, sizeof(int));
    ptrByte += sizeof(int);
    memcpy(ptrByte, &saltlen, sizeof(int));
    ptrByte += sizeof(int);
    memcpy(ptrByte, &timeCost, sizeof(int));
    ptrByte += sizeof(int);
    memcpy(ptrByte, &nRows, sizeof(int));
    ptrByte += sizeof(int);
    memcpy(ptrByte, &nCols, sizeof(int));
    ptrByte += sizeof(int);
    

    //Now comes the padding
    *ptrByte = 0x80; //first byte of padding: right after the password
    ptrByte = (byte*) wholeMatrix; //resets the pointer to the start of the memory matrix
    ptrByte += nBlocksInput * BLOCK_LEN_BYTES - 1; //sets the pointer to the correct position: end of incomplete block
    *ptrByte ^= 0x01; //last byte of padding: at the end of the last incomplete block

    //==========================================================================/

    //====================== Setup Phase =====================//

    //Absorbing salt and password
    ptrWord = memMatrix[0];
    for (i = 0; i < nBlocksInput; i++) {
        absorbBlockSSE(state, ptrWord); //absorbs each block of pad(pwd || salt || basil)
        ptrWord += BLOCK_LEN_INT128; //goes to next block of pad(pwd || salt || basil)
    }
    
    reducedSqueezeRowSSE(state, memMatrix[0], nCols); //The locally copied password is overwritten here
    reducedSqueezeRowSSE(state, memMatrix[1], nCols);

    do {
        //M[row] = rand; //M[row*] = M[row*] XOR rotW(rand)
        reducedDuplexRowSetupSSE(state, memMatrix[prev], memMatrix[rowa], memMatrix[row], nCols);

        //updates the value of row* (deterministically picked during Setup))
        rowa--;
        if (rowa < 0) {
            rowa = prev;
        }
        //update prev: it now points to the last row ever computed
        prev = row;
        //updates row: does to the next row to be computed
        row++;
    } while (row < nRows);
    //========================================================//

    //================== Wandering Phase =====================//
    int maxIndex = nRows - 1;
    for (tau = 1; tau <= timeCost; tau++) {
        //========= Iterations for an odd tau ==========
        row = maxIndex; //Odd iterations of the Wandering phase start with the last row ever computed
        prev = 0; //The companion "prev" is 0
        do {
            //Selects a pseudorandom index row*
            //rowa = ((unsigned int) (((int *) state)[0] ^ prev)) & maxIndex; //(USE THIS IF nRows IS A POWER OF 2)
            rowa = ((unsigned int) (((unsigned int *) state)[0] ^ prev)) % nRows; //(USE THIS FOR THE "GENERIC" CASE)

            //Performs a reduced-round duplexing operation over M[row*] XOR M[prev], updating both M[row*] and M[row]
            reducedDuplexRowSSE(state, memMatrix[prev], memMatrix[rowa], memMatrix[row], nCols);

            //Goes to the next row (inverse order)
            prev = row;
            row--;
        } while (row >= 0);

        if (++tau > timeCost) {
            break; //end of the Wandering phase
        }

        //========= Iterations for an even tau ==========
        row = 0; //Even iterations of the Wandering phase start with row = 0
        prev = maxIndex; //The companion "prev" is the last row in the memory matrix
        do {
            //rowa = ((unsigned int) (((int *) state)[0] ^ prev)) & maxIndex; //(USE THIS IF nRows IS A POWER OF 2)
            rowa = ((unsigned int) (((unsigned int *) state)[0] ^ prev)) % nRows; //(USE THIS FOR THE "GENERIC" CASE)

            //Performs a reduced-round duplexing operation over M[row*] XOR M[prev], updating both M[row*] and M[row]
            reducedDuplexRowSSE(state, memMatrix[prev], memMatrix[rowa], memMatrix[row], nCols);

            //Goes to the next row (direct order)
            prev = row;
            row++;
        } while (row <= maxIndex);
    }
    //========================================================//

    //==================== Wrap-up Phase =====================//
    //Absorbs the last block of the memory matrix
    absorbBlockSSE(state, memMatrix[rowa]);

    //Squeezes the key
    squeezeSSE(state, K, kLen);
    //========================================================//
    
    //========================= Freeing the memory =============================//
    free(memMatrix);
    free(wholeMatrix);
    
    //Wiping out the sponge's internal state before freeing it
    memset(state, 0, 16 * sizeof (uint64_t));
    free(state);
    //==========================================================================/


    return 0;
}