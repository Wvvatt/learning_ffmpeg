#include <stdint.h>
#include <stdio.h>

static int parse_pcr(int64_t *ppcr_high, int *ppcr_low, const uint8_t *packet)
{
    int afc, len, flags;
    const uint8_t *p;
    uint32_t v;

    afc = (packet[3] >> 4) & 3;
    if (afc <= 1)
        return -1;
    p   = packet + 4;
    len = p[0];
    p++;
    if (len == 0)
        return -1;
    flags = *p++;
    len--;
    if (!(flags & 0x10))
        return -1;
    if (len < 6)
        return -1;
    v          = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
    *ppcr_high = ((int64_t) v << 1) | (p[4] >> 7);
    *ppcr_low  = ((p[4] & 1) << 8) | p[5];
    return 0;
}

int main()
{
    uint8_t packet[12] = {0x47, 0x00, 0x91, 0x20, 
                          0xb7, 0x10,
                          0x04, 0x4e, 0x65, 0x6f, 0xfe, 0x2e};
    int64_t pcr_h = 0;
    int pcr_l = 0;
    parse_pcr(&pcr_h, &pcr_l, packet);
    int64_t pcr = pcr_h * 300 + pcr_l;
    printf("pcr = %ld, high = %ld, low = %d\n", pcr, pcr_h, pcr_l);

    return 0;
}