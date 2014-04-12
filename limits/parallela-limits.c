#include <stdint.h>
#include <stdbool.h>

uint32_t MIN_TCOST = 1;
uint32_t MAX_TCOST = 126 << 16;
uint32_t TCOST_LOGARITHMIC = false;
uint32_t MIN_MCOST = 0;
uint32_t MAX_MCOST = 0;
uint32_t MCOST_LOGARITHMIC = false;
uint32_t MIN_INLEN = 0;
uint32_t MAX_INLEN = 2048;
uint32_t MIN_SALTLEN = 0;
uint32_t MAX_SALTLEN = 2048;
uint32_t MIN_OUTLEN = 1;
uint32_t MAX_OUTLEN = 64;

// Only needed if there are invalid t_cost/m_cost combinations.  Increase one or the other
// to make them valid.
void validateCosts(uint32_t *t_cost, uint32_t *m_cost) {
    uint8_t lower = *t_cost;
    uint8_t upper = *t_cost >> 16;
    if(lower > 106) {
        lower = 106;
    }
    if(upper > 126) {
        upper = 126;
    }
    *t_cost = ((uint32_t)upper << 16) | lower;
}
