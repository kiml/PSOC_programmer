#include <stdint.h>
#include <string.h>

#include "config_data.h"


#ifdef SRC_TEST_MODE
#include <stdio.h>
#endif

// Config Table Types
#define CFG_TT_UNDEF 0
#define CFG_TT_CF 1
#define CFG_TT_OV 2
#define CFG_TT_DA 3
#define CFG_TT_COPY 4


void cfg_set_regions(const uint8_t *cfg_base_addr)
{
    // Refer: docs/romdata.txt

    // table: [ ttcc aaaa aaaa <element> ]* 0000
    //          tt = type, cc = value
    // element: tt == 1:  constant fill
    //           ccvv   # cccc
    // element: tt == 2:  offset value
    //           [ oovv ]+(cc)
    // element: tt == 3:  data array
    //           [ vv ]+(cc)
    // element: tt == 4:  mem to mem copy
    //           ssss ssss

    // WARNING for compactness 32 bit addresses are NOT aligned on relevant memory boundaries.
    // easiest way to manage data is 8 bit chunks. (unless I pad case 3 for cc == 1)

    const uint8_t *tbl = cfg_base_addr;
    while(1)
    {
        uint8_t table_type = tbl[0];
        uint8_t count_low = tbl[1];

        if (table_type == CFG_TT_UNDEF) break;

        uint32_t addr = (tbl[5] << 24) | (tbl[4] << 16) | (tbl[3] << 8) | tbl[2];
        uint8_t *ap = (uint8_t *)((size_t)addr);
       
#ifdef SRC_TEST_MODE
        //printf("T: %d, A:%x ", table_type, addr);
        printf("T: %d ", table_type);
#endif
        tbl += 6;

        if (table_type == CFG_TT_CF)
        {
            // constant fill: ccvv
            uint8_t count_hi = tbl[0];
            uint8_t value = tbl[1];
            uint16_t count = (count_hi << 8) | count_low;
#ifdef SRC_TEST_MODE
            printf("CF: %lx = %d * %d\n", (size_t)ap, value, count);
#else
            memset(ap, value, count);
#endif
            tbl += 2;
        }
        else if (table_type == CFG_TT_OV)
        {
            // offset value: [ oovv ]+(count)
            uint8_t i;
            for(i=0; i<count_low; i++)
            {
                uint8_t offset = tbl[0];
                uint8_t value = tbl[1];
#ifdef SRC_TEST_MODE
                printf("OV %d/%d: %lx[%d] = %d\n", i, count_low, (size_t)ap, offset, value);
#else
                ap[offset] = value;
#endif
                tbl += 2;
            }
        }
        else if (table_type == CFG_TT_DA)
        {
            // data array: [ vv ]+(cc)
#ifdef SRC_TEST_MODE
            printf("DA: %lx (%d) = ", (size_t)ap, count_low);
            int i; for(i=0;i<count_low;i++) printf("%02x ", tbl[i]);
            printf("\n");
#else
            memcpy(ap, tbl, count_low);
#endif
            tbl += count_low;
        }
        else if (table_type == CFG_TT_COPY)
        {
            // copy mem-mem: ssss ssss
            uint32_t saddr = (tbl[3] << 24) | (tbl[2] << 16) | (tbl[1] << 8) | tbl[0];
            uint8_t *sap = (uint8_t *)((size_t)saddr);
#ifdef SRC_TEST_MODE
            printf("DA: %lx = %lx * %d\n", (size_t)ap, (size_t)sap, count_low);
#else
            memcpy(ap, sap, count_low);
#endif
            tbl += 4;
        }
        else
        {
#ifdef SRC_TEST_MODE
            printf("UNKNOWN CONFIG TYPE\n");
#else
            // anything we can do in practice !?
#endif
        }
    }
}
