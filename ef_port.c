/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2015-2019, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-01-16
 */

#include <easyflash.h>
#include <stdarg.h>
#include <stdio.h>
#include "system/spi.h"

/* default environment variables set for user */
static const ef_env default_env_set[] = {
	 {"version", "V1.0", 0},
};

/**
 * Flash port for hardware initialize.
 *
 * @param default_env default ENV set for user
 * @param default_env_size default ENV size
 *
 * @return result
 */
EfErrCode ef_port_init(ef_env const **default_env, size_t *default_env_size) {
    EfErrCode result = EF_NO_ERR;

    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set) / sizeof(default_env_set[0]);

    return result;
}

/**
 * Read data from flash.
 * @note This operation's units is word.
 *
 * @param addr flash address
 * @param buf buffer to store read data
 * @param size read bytes size
 *
 * @return result
 */
EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size) {
    EfErrCode result = EF_NO_ERR;

	FlashRead(addr, (uint8_t *)buf, size);
	
    return result;
}

#define EF_ERASE_MIN_SIZE   0x1000
#define EF_ERASE_32K_SIZE   0x8000
#define EF_ERASE_64K_SIZE   0x10000

#define __64k_start_addr(addr)      \
    addr%EF_ERASE_64K_SIZE ? addr + EF_ERASE_64K_SIZE - addr%EF_ERASE_64K_SIZE : addr
#define __32k_start_addr(addr)      \
    addr%EF_ERASE_32K_SIZE ? addr + EF_ERASE_32K_SIZE - addr%EF_ERASE_32K_SIZE : addr 
/**
 * Erase data on flash.
 * @note This operation is irreversible.
 * @note This operation's units is different which on many chips.
 *
 * @param addr flash address
 * @param size erase bytes size
 *
 * @return result
 */
EfErrCode ef_port_erase(uint32_t addr, size_t size) {
    EfErrCode result = EF_NO_ERR;
    uint32_t addr_64k_s = __64k_start_addr(addr);
    uint32_t erase_addr = 0, erase_len = 0;
    uint8_t size_64k = 0, size_32k = 0, size_4k = 0;
    
    /*
     * Initialize erase address and length, make sure they're
     * all multiples of EF_ERASE_MIN_SIZE, easy to calculate.
     */
    addr -= addr%EF_ERASE_MIN_SIZE;
    erase_addr = addr;

    if(size%EF_ERASE_MIN_SIZE)
    {
        size = size - size%EF_ERASE_MIN_SIZE + EF_ERASE_MIN_SIZE;
    }
    
    if(size == EF_ERASE_MIN_SIZE)
    {
        FlashSectorErase(erase_addr);
    }
    /*
     * Ready to erase flash
     */
    EF_INFO("Start erase flash.\n\r");
    EF_DEBUG("Erase sector addr 0x%x size:0x%x\n\r", addr, size);
    if(size < EF_ERASE_64K_SIZE)
    {
        size_4k  = size/EF_ERASE_MIN_SIZE;
        while(size_4k--)
        {
            FlashSectorErase(erase_addr);
			erase_addr += EF_ERASE_MIN_SIZE;
            erase_len += EF_ERASE_MIN_SIZE;
            EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
        }
        EF_INFO("\n\r                          \n\r");
        return result;
    }
    else
    {
        if(addr == addr_64k_s)
        {
            goto __fin_erase;         
        }
        else
        {
            if((addr_64k_s - addr) >= EF_ERASE_32K_SIZE)
            {
                size_4k  = (addr_64k_s - EF_ERASE_32K_SIZE -addr)/EF_ERASE_MIN_SIZE;
                size_32k = 1;

                while(size_4k--)
                {
                    FlashSectorErase(erase_addr);
                    erase_addr += EF_ERASE_MIN_SIZE;
                    erase_len += EF_ERASE_MIN_SIZE;
                    EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
                }
                while(size_32k--)
                {
                    FlashBlockErase32(erase_addr);
                    erase_addr += EF_ERASE_32K_SIZE;
                    erase_len += EF_ERASE_32K_SIZE;
                    EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
                }
            }
            else
            {
                size_4k  = (addr_64k_s - addr)/EF_ERASE_MIN_SIZE;

                while(size_4k--)
                {
                    FlashSectorErase(erase_addr);
                    erase_addr += EF_ERASE_MIN_SIZE;
                    erase_len += EF_ERASE_MIN_SIZE;
                    EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
                }
            }
        }
    }
__fin_erase:
    size_64k = (size - erase_len) / EF_ERASE_64K_SIZE;
    size_32k = ((size - erase_len) % EF_ERASE_64K_SIZE) / EF_ERASE_32K_SIZE;
    size_4k  = ((size - erase_len) % EF_ERASE_32K_SIZE) / EF_ERASE_MIN_SIZE;

    while(size_64k--)
    {
        FlashBlockErase64(erase_addr);
        erase_addr += EF_ERASE_64K_SIZE;
        erase_len += EF_ERASE_64K_SIZE;
        EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
    }

    while(size_32k--)
    {
        FlashBlockErase32(erase_addr);
        erase_addr += EF_ERASE_32K_SIZE;
        erase_len += EF_ERASE_32K_SIZE;
        EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
    }

    while(size_4k--)
    {
        FlashSectorErase(erase_addr);
        erase_addr += EF_ERASE_MIN_SIZE;
        erase_len += EF_ERASE_MIN_SIZE;
        EF_INFO("Erase %0.2f%%       \r", erase_len/(float)size * 100);
    }

    EF_INFO("\n\r                          \n\r");
    return result;
}
/**
 * Write data to flash.
 * @note This operation's units is word.
 * @note This operation must after erase. @see flash_erase.
 *
 * @param addr flash address
 * @param buf the write data buffer
 * @param size write bytes size
 *
 * @return result
 */
EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size) {
    EfErrCode result = EF_NO_ERR;

    FlashWrite(addr, (uint8_t *)buf, size);

    return result;
}

/**
 * lock the ENV ram cache
 */
void ef_port_env_lock(void) {
    
    /* You can add your code under here. */
    
}

/**
 * unlock the ENV ram cache
 */
void ef_port_env_unlock(void) {
    
    /* You can add your code under here. */
    
}

#define BUF_LEN		128

/**
 * This function is print flash debug info.
 *
 * @param file the file which has call this function
 * @param line the line number which has call this function
 * @param format output format
 * @param ... args
 *
 */
void ef_log_debug(const char *file, const long line, const char *format, ...) {

#ifdef EF_DBG

	char buf[BUF_LEN] = {0};
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

	printf("<%s> %u: %s\r", file, line, buf);

#endif

}

/**
 * This function is print flash routine info.
 *
 * @param format output format
 * @param ... args
 */
void ef_log_info(const char *format, ...) {
	char buf[BUF_LEN] = {0};
	va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
	
    va_end(args);
	printf("%s\r", buf);
}
/**
 * This function is print flash non-package info.
 *
 * @param format output format
 * @param ... args
 */
void ef_print(const char *format, ...) {
    va_list args;
	char buf[BUF_LEN] = {0};
	
    /* args point to the first variable parameter */
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
	
    va_end(args);
	printf("%s\r", buf);
}

