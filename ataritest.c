// SPDX-License-Identifier: MIT

#include <assert.h>
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "emulator.h"
#include "gpio/ps_protocol.h"

#define SIZE_KILO 1024
#define SIZE_MEGA (1024 * 1024)
#define SIZE_GIGA (1024 * 1024 * 1024)

#define OFFSET          0x0600
#define MEM_READ        0
#define MEM_WRITE       1
#define TEST_8          8
#define TEST_8_ODD      81
#define TEST_8_RANDOM   82
#define TEST_16         16
#define TEST_16_ODD     161
#define TEST_16_RANDOM  162
#define TEST_32         32
#define TEST_32_ODD     321
#define TEST_32_RANDOM  322

#define REVERSE_VIDEO   "\033[7m"
#define NORMAL          "\033[0m"

void memTest ( int direction, int type, uint32_t startAdd, uint32_t length, uint8_t *garbagePtr );
void clearmem ( uint32_t length, uint32_t *duration );



uint8_t *garbege_datas;
extern volatile unsigned int *gpio;
extern uint8_t fc;

struct timespec f2;

uint32_t mem_fd;
uint32_t errors = 0;
uint8_t  loop_tests = 0;
uint32_t cur_loop;



void sigint_handler(int sig_num)
{
  printf ( "\nATARITEST aborted\n\n");

  if (mem_fd)
    close(mem_fd);

  exit(0);
}


void ps_reinit() 
{
    ps_reset_state_machine();
    ps_pulse_reset();

    usleep(1500);

    /* assuming this is for AMIGA ??? nothing in ATARI memory map */
    //write8(0xbfe201, 0x0101);       //CIA OVL
	//write8(0xbfe001, 0x0000);       //CIA OVL LOW
}
/*
unsigned int dump_read_8(unsigned int address) {
    uint32_t bwait = 10000;

    *(gpio + 0) = GPFSEL0_OUTPUT;
    *(gpio + 1) = GPFSEL1_OUTPUT;
    *(gpio + 2) = GPFSEL2_OUTPUT;

    *(gpio + 7) = ((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0);
    *(gpio + 7) = 1 << PIN_WR;
    *(gpio + 10) = 1 << PIN_WR;
    *(gpio + 10) = 0xffffec;

    *(gpio + 7) = ((0x0300 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0);
    *(gpio + 7) = 1 << PIN_WR;
    *(gpio + 10) = 1 << PIN_WR;
    *(gpio + 10) = 0xffffec;

    *(gpio + 0) = GPFSEL0_INPUT;
    *(gpio + 1) = GPFSEL1_INPUT;
    *(gpio + 2) = GPFSEL2_INPUT;

    *(gpio + 7) = (REG_DATA << PIN_A0);
    *(gpio + 7) = 1 << PIN_RD;


    //while (bwait && (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS))) {
    while ( (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS))) {
       // bwait--;
    }

    unsigned int value = *(gpio + 13);

    *(gpio + 10) = 0xffffec;

    value = (value >> 8) & 0xffff;

    //if ( !bwait ) {
    //    ps_reinit();
    //}

    if ((address & 1) == 0)
        return (value >> 8) & 0xff;  // EVEN, A0=0,UDS
    else
        return value & 0xff;  // ODD , A0=1,LDS
}
*/


int check_emulator() 
{

    DIR* dir;
    struct dirent* ent;
    char buf[512];

    long  pid;
    char pname[100] = {0,};
    char state;
    FILE *fp=NULL;
    const char *name = "emulator";

    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc, assuming emulator running");
        return 1;
    }

    while((ent = readdir(dir)) != NULL) {
        long lpid = atol(ent->d_name);
        if(lpid < 0)
            continue;
        snprintf(buf, sizeof(buf), "/proc/%ld/stat", lpid);
        fp = fopen(buf, "r");

        if (fp) {
            if ( (fscanf(fp, "%ld (%[^)]) %c", &pid, pname, &state)) != 3 ){
                printf("fscanf failed, assuming emulator running\n");
                fclose(fp);
                closedir(dir);
                return 1;
            }
            if (!strcmp(pname, name)) {
                fclose(fp);
                closedir(dir);
                return 1;
            }
            fclose(fp);
        }
    }

    closedir(dir);
    return 0;
}


int main(int argc, char *argv[]) 
{
    uint32_t test_size = 2 * SIZE_KILO;
    uint32_t duration;


    cur_loop = 1;

    if (check_emulator()) {
        printf("PiStorm emulator running, please stop this before running ataritest\n");
        return 1;
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &f2);
    srand((unsigned int)f2.tv_nsec);

    signal(SIGINT, sigint_handler);

    ps_setup_protocol();
    ps_reset_state_machine();
    ps_pulse_reset();

    usleep(1500);

	fc = 0b101;
    /* WHAT DOES THIS DO????? */
    /* okay - now I know - thanks to BW as usual :) - configure ATARI MMU for amount of system RAM */
	//write8( 0xff8001, 0b00000100 ); // memory config 512k bank 0
    /*
    #define ATARI_MMU_128K  0b00000000 // bank 0
    #define ATARI_MMU_512K  0b00000100 // bank 0
    #define ATARI_MMU_2M    0b00001000 // bank 0
    */

    write8 ( 0xff8001, 0b00001010 ); // configure MMU for 4MB - configure banks 0 AND 1 - *NOTE the address


    if (argc > 1) {

        if (strcmp(argv[1], "dumptos") == 0) {
            printf ("Dumping onboard STE ROM from $E00000 to file tos.rom.\n");
            FILE *out = fopen("tos.rom", "wb+");
            if (out == NULL) {
                printf ("Failed to open tos.rom for writing.\nTOS has not been dumped.\n");
                return 1;
            }

            for (int i = 0; i < 192 * SIZE_KILO; i++) {
                //unsigned char in = read8(0xFC0000 + i);
                unsigned char in = read8(0xE00000 + i);
                fputc(in, out);
            }

            fclose(out);
            printf ("STE TOS ROM dumped.\n");
            return 0;
        }

        if ( strcmp ( argv[1], "clearmem" ) == 0 ) 
        {
            int size = 512; // default to 512 KB

            if ( argc == 3 )
            {                
                sscanf ( argv[2], "%d", &size );
            }

            if ( size > 4096 || size < 512 )
                size = 512;

            printf ( "Clearing ATARI ST RAM - %d KB\n", size ) ;

            clearmem ( size * 1024, &duration );
            
            printf ( "\nATARI ST RAM cleared in %d ms @ %.2f KB/s\n", 
                duration, ( (float)((size * 1024) - OFFSET) / (float)duration * 1000.0) / 1024 );

            return 0;
        }

        if (strcmp(argv[1], "peek") == 0) {
            
            char ascii[17];
            size_t i, j;
            unsigned int address;
            unsigned char data [0x100]; /* 256 byte block */
            int size = 0x100;
            unsigned int start;

            if ( argc == 3 )
            {
                unsigned int var1;
                
                sscanf( argv[2], "%x", &var1 );
                
                start = (var1 / 16) * 16; /* we want a 16 byte boundary */
            }

            else
                start = 0;  
            

            ascii[16] = '\0';

            printf ( "\n Address    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n\n" );

            for (i = 0, address = start; i < size; ++i, address++) 
            {
                //for (int n=1000;n;n--) __asm__("nop");
                data [i] = read8 (address);

                if ( !(address % 16) )
                    printf( " $%.6X  | ", address );

                printf( "%02X ", ((unsigned char*)data)[i]);

                if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') 
                {
                    ascii[i % 16] = ((unsigned char*)data)[i];
                } 

                else 
                {
                    ascii[i % 16] = '.';
                }

                if ((i+1) % 8 == 0 || i+1 == size) 
                {
                    //printf(" ");

                    if ((i+1) % 16 == 0) 
                    {
                        printf("|  %s \n", ascii);
                    } 

                    else if (i+1 == size) 
                    {
                        ascii[(i+1) % 16] = '\0';

                        if ((i+1) % 16 <= 8) 
                        {
                         //   printf(" ");
                        }

                        for (j = (i+1) % 16; j < 16; ++j) 
                        {
                            printf("   ");
                        }

                        printf("|  %s \n", ascii);
                    }
                }
            }
        
            return 0;
        }

        test_size = atoi(argv[1]) * SIZE_KILO;

        if (test_size < 2 || test_size > 4 * SIZE_MEGA) {
            test_size = 2 * SIZE_KILO;
        }

        if (argc > 2) {
            if (strcmp(argv[2], "l") == 0) {
                //printf("Looping tests.\n");
                loop_tests = 1;
            }
        }
    }


    garbege_datas = malloc(test_size);
    if (!garbege_datas) {
        printf ("Failed to allocate memory for garbege datas.\n");
        return 1;
    }

    printf ( "\nATARITEST\n" );
	if( test_size <= OFFSET ) {
		printf("Test size is too small -- nothing to do!\n");
		return 0;
	}

    printf ( "Testing %d KB of memory - Starting address $%.6X\n", test_size / SIZE_KILO, OFFSET );

    if ( loop_tests )
        printf ( "Test looping enabled\n" );

    printf ( "Priming test data between %ld and %ld (%ld bytes test size).\n",
	OFFSET, test_size, test_size-OFFSET );
    
    for ( uint32_t i = 0, add = OFFSET ; add < test_size; i++, add++) 
    {
	garbege_datas[i] =  add % 2 ? add-1 & 0xff : (add >> 8);

        write8 ( add, garbege_datas [i] );
    }

test_loop:

    printf ( "\n%sTesting Reads...%s\n\n", REVERSE_VIDEO, NORMAL );

    memTest ( MEM_READ,  TEST_8,         OFFSET, test_size, garbege_datas );
    memTest ( MEM_READ,  TEST_16,        OFFSET, test_size, garbege_datas );
    memTest ( MEM_READ,  TEST_16_ODD,    OFFSET, test_size, garbege_datas );
    memTest ( MEM_READ,  TEST_32,        OFFSET, test_size, garbege_datas );
    memTest ( MEM_READ,  TEST_32_ODD,    OFFSET, test_size, garbege_datas );

    printf ( "\n%sTesting Writes...%s\n\n", REVERSE_VIDEO, NORMAL );

    memTest ( MEM_WRITE, TEST_8,         OFFSET, test_size, garbege_datas );
    memTest ( MEM_WRITE, TEST_16,        OFFSET, test_size, garbege_datas );
    memTest ( MEM_WRITE, TEST_16_ODD,    OFFSET, test_size, garbege_datas );
    memTest ( MEM_WRITE, TEST_32,        OFFSET, test_size, garbege_datas );
    memTest ( MEM_WRITE, TEST_32_ODD,    OFFSET, test_size, garbege_datas );

    printf ( "\n%sTesting Random Reads / Writes...%s\n\n", REVERSE_VIDEO, NORMAL );

    memTest ( MEM_READ,  TEST_8_RANDOM,  OFFSET, test_size, garbege_datas );
    memTest ( MEM_READ,  TEST_16_RANDOM, OFFSET, test_size, garbege_datas );
    memTest ( MEM_READ,  TEST_32_RANDOM, OFFSET, test_size, garbege_datas );

    if (loop_tests) 
    {
        printf ( "\nPass %d complete\nStarting pass %d\n", cur_loop, cur_loop + 1);
        //printf ( "Current total errors: %d.\n\n", total_errors);

        sleep(1);

	    printf ( "Priming test data\n" );

	    for ( uint32_t i = 0, add = OFFSET ; add < test_size; i++, add++ ) 
        {
	        garbege_datas [i] = (uint8_t)(rand() % 0xFF );
	        write8 ( add, (uint32_t) garbege_datas [i] );
	    }

        cur_loop++;

        goto test_loop;
    }

    printf ( "\nATARITEST completed\n\n");

    return 0;
}

void m68k_set_irq(unsigned int level) {
}



void memTest ( int direction, int type, uint32_t startAdd, uint32_t length, uint8_t *garbagePtr )
{
    uint8_t  d8, rd8;
    uint16_t d16, rd16;
    uint32_t d32, rd32;
    uint32_t radd;
    long int nanoStart;
    long int nanoEnd; 

    char dirStr  [6];
    char typeStr [20];
    char testStr [80];

    struct timespec tmsStart, tmsEnd;

    static uint32_t totalErrors = 0;
    static int testNumber;
    static int currentPass      = 0;
    int errors                  = 0;


    if ( currentPass != cur_loop )
    {
        currentPass = cur_loop;
        testNumber = 1;
    }

    switch (direction) 
    {
        case MEM_READ:

            sprintf ( dirStr, "READ" );

                switch (type)
                {
                    case TEST_8:

                        sprintf ( typeStr, "8:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[BYTE] Reading RAM...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 0, add = startAdd ; add < length; n++, add++ ) 
                        {
                            d8 = read8 (add);
                            
                            if ( d8 != garbagePtr [n] ) 
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.2X expected, found %.2X.\n", testStr, add, garbagePtr[n], d8 );
                                }

                                errors++;
                            }

                            /* sanity feedback - one dot per 64 KB */
                            if ( n % (64 * 1024)  == 0 )
                            {
                                printf ( "." );
                                fflush ( stdout );
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    case TEST_16:

                        sprintf ( typeStr, "16:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[WORD] Reading RAM aligned...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 0, add = startAdd ; add < length - 2; n += 2, add += 2 ) 
                        {
                            d16 = be16toh ( read16 (add) );
                            
                            if ( d16 != *( (uint16_t *) &garbagePtr [n] ) )
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, add, d16, (uint16_t)(garbagePtr [n] << 8 | garbagePtr [n] ) );
                                }

                                errors++;
                            }

                            if ( n % (64 * 1024)  == 0 )
                            {
                                printf ( "." );
                                fflush ( stdout );
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    case TEST_16_ODD:

                        sprintf ( typeStr, "16_ODD:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[WORD] Reading RAM unaligned...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 1, add = startAdd + 1; add < length - 2; n += 2, add += 2 ) 
                        {
                            d16 = be16toh ( (read8 (add) << 8) | read8 (add + 1) );
                            
                            if ( d16 != *( (uint16_t *) &garbagePtr [n] ) )
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, add, d16, *( (uint16_t *) &garbagePtr [n] ) );
                                }

                                errors++;
                            }

                            if ( !errors )
                            {
                                if ( (n - 1) % (64 * 1024)  == 0 )
                                {
                                    printf ( "." );
                                    fflush ( stdout );
                                }
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    case TEST_32:

                        sprintf ( typeStr, "32:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[LONG] Reading RAM aligned...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 0, add = startAdd; add < length - 4; n += 4, add += 4 ) 
                        {
                            d32 = be32toh ( read32 (add) );
                            
                            if ( d32 != *( (uint32_t *) &garbagePtr [n] ) )
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.8X expected, found %.8X.\n", testStr, add, d32, *( (uint32_t *) &garbagePtr [n] ) );
                                }

                                errors++;
                            }

                            if ( !errors )
                            {
                                if ( n % (64 * 1024)  == 0 )
                                {
                                    printf ( "." );
                                    fflush ( stdout );
                                }
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    case TEST_32_ODD:

                        sprintf ( typeStr, "32_ODD:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[LONG] Reading RAM unaligned...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 1, add = startAdd + 1; add < length - 4; n += 4, add += 4 ) 
                        {
                            d32 = read8 (add);
                            d32 |= (be16toh ( read16 (add + 1) ) << 8);
                            d32 |= (read8 (add + 3) << 24 );
                            
                            if ( d32 != *( (uint32_t *) &garbagePtr [n] ) )
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.8X expected, found %.8X.\n", testStr, add, d32, *( (uint32_t *) &garbagePtr [n] ) );
                                }

                                errors++;
                            }

                            if ( !errors )
                            {
                                if ( (n - 1) % (64 * 1024)  == 0 )
                                {
                                    printf ( "." );
                                    fflush ( stdout );
                                }
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    /* random data / random addresses */
                    case TEST_8_RANDOM:

                        srand (length);

                        sprintf ( typeStr, "8_RANDOM_RW:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[BYTE] Writing random data to random addresses...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 0, add = startAdd ; add < length; n++, add++ ) 
                        {
                            
                            rd8  = (uint8_t)  ( rand () % 0xFF );

                            for ( int z = 10; z; z-- ) /* ten retries expected, found enough especially for small mem size */
                            {
                                radd = (uint32_t) ( rand () % length );

                                if ( radd < startAdd )
                                    continue;

                                break;
                            }

                            write8 ( radd, rd8 );
                            d8 = read8 (radd);
                            
                            if ( d8 != rd8 ) 
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.2X expected, found %.2X.\n", testStr, radd, d8, rd8 );
                                }

                                errors++;
                            }

                            /* sanity feedback - one dot per 64 KB */
                            if ( n % (64 * 1024)  == 0 )
                            {
                                printf ( "." );
                                fflush ( stdout );
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    case TEST_16_RANDOM:

                        srand (length);

                        sprintf ( typeStr, "16_RANDOM_RW:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[WORD] Writing random data to random addresses aligned...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 0, add = startAdd ; add < length - 2; n += 2, add += 2 ) 
                        {
                            
                            rd16  = (uint16_t)  ( rand () % 0xffff );

                            for ( int z = 10; z; z-- ) /* ten retries expected, found enough especially for small mem size */
                            {
                                radd = (uint32_t) ( rand () % length );

                                if ( radd < startAdd )
                                    continue;

                                break;
                            }

                            write16 ( radd, rd16 );
                            d16 = read16 (radd);
                            //d16 = be16toh ( read16 (add) );
                            
                            if ( d16 != rd16 ) 
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, radd, d16, rd16 );
                                }

                                errors++;
                            }

                            /* sanity feedback - one dot per 64 KB */
                            if ( n % (64 * 1024)  == 0 )
                            {
                                printf ( "." );
                                fflush ( stdout );
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;

                    case TEST_32_RANDOM:

                        srand (length);

                        sprintf ( typeStr, "32_RANDOM_RW:" );
                        sprintf ( testStr, "%s%s ", dirStr, typeStr );

                        printf ( "Test %d\n", testNumber );
                        printf ( "%-20s[LONG] Writing random data to random addresses aligned...\n", testStr );

                        clock_gettime ( CLOCK_REALTIME, &tmsStart );

                        for ( uint32_t n = 0, add = startAdd ; add < length - 4; n += 4, add += 4 ) 
                        {
                            
                            rd32  = (uint32_t)  ( rand () % 0xffffffff );

                            for ( int z = 10; z; z-- ) /* ten retries expected, found enough especially for small mem size */
                            {
                                radd = (uint32_t) ( rand () % length );

                                if ( radd < startAdd )
                                    continue;

                                break;
                            }

                            write32 ( radd, rd32 );
                            d32 = read32  ( radd );
                            
                            if ( d32 != rd32 ) 
                            {
                                if ( errors < 10 )
                                {
                                    if (errors == 0)
                                        printf ( "\n" );

                                    printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, radd, d32, rd32 );
                                }

                                errors++;
                            }

                            /* sanity feedback - one dot per 64 KB */
                            if ( n % (64 * 1024)  == 0 )
                            {
                                printf ( "." );
                                fflush ( stdout );
                            }
                        }

                        clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                        printf ( "\n" );

                    break;
                }
            

        break;

        case MEM_WRITE:

            sprintf ( dirStr, "WRITE" );

            switch (type)
            {
                case TEST_8:

                    sprintf ( typeStr, "8:" );
                    sprintf ( testStr, "%s%s ", dirStr, typeStr );

                    printf ( "Test %d\n", testNumber );
                    printf ( "%-20s[BYTE] Writing to RAM... \n", testStr );

                    clock_gettime ( CLOCK_REALTIME, &tmsStart );

                    for ( uint32_t n = 0, add = startAdd; add < length; n++, add++ ) 
                    {
                        d8 = garbagePtr [n];

                        write8 ( add, d8 );
                        rd8 = read8  ( add );
                        
                        if ( d8 != rd8 ) 
                        {
                            if ( errors < 10 )
                            {
                                if (errors == 0)
                                    printf ( "\n" );

                                printf ( "%sData mismatch at $%.6X: %.2X expected, found %.2X.\n", testStr, add, d8, rd8 );
                            }

                            errors++;
                        }

                        if ( (n - 1) % (64 * 1024)  == 0 )
                        {
                            printf ( "." );
                            fflush ( stdout );
                        }
                    }

                    clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                    printf ( "\n" );

                break;

                case TEST_16:

                    sprintf ( typeStr, "16:" );
                    sprintf ( testStr, "%s%s ", dirStr, typeStr );

                    printf ( "Test %d\n", testNumber );
                    printf ( "%-20s[WORD] Writing to RAM aligned... \n", testStr );

                    clock_gettime ( CLOCK_REALTIME, &tmsStart );

                    for ( uint32_t n = 0, add = startAdd; add < length - 2; n += 2, add += 2) 
                    {
                        d16 = htobe16( *( (uint16_t *) &garbagePtr [n] ) );

                        write16 ( add, d16 );
                        rd16 = read16  ( add );
                        
                        if ( d16 != rd16 ) 
                        {
                            if ( errors < 10 )
                            {
                                if (errors == 0)
                                    printf ( "\n" );

                                printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, add, d16, rd16 );
                            }

                            errors++;
                        }

                        if ( (n) % (64 * 1024)  == 0 )
                        {
                            printf ( "." );
                            fflush ( stdout );
                        }
                    }

                    clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                    printf ( "\n" );

                break;

                case TEST_16_ODD:

                    sprintf ( typeStr, "16_ODD:" );
                    sprintf ( testStr, "%s%s ", dirStr, typeStr );

                    printf ( "Test %d\n", testNumber );
                    printf ( "%-20s[WORD] Writing to RAM unaligned... \n", testStr );

                    clock_gettime ( CLOCK_REALTIME, &tmsStart );

                    for ( uint32_t n = 1, add = startAdd + 1; add < length - 2; n += 2, add += 2) 
                    {
                        d16 = *( (uint16_t *) &garbagePtr [n] );

                        write8 ( add, (d16 & 0x00FF) );
                        write8 ( add + 1, (d16 >> 8) );                      
                        rd16 = be16toh ( (read8 (add) << 8) | read8 (add + 1) );

                        if ( d16 != rd16 ) 
                        {
                            if ( errors < 10 )
                            {
                                if (errors == 0)
                                    printf ( "\n" );

                                printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, add, d16, rd16 );
                            }

                            errors++;
                        }

                        if ( (n - 1) % (64 * 1024)  == 0 )
                        {
                            printf ( "." );
                            fflush ( stdout );
                        }
                    }

                    clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                    printf ( "\n" );

                break;

                case TEST_32:

                    sprintf ( typeStr, "32:" );
                    sprintf ( testStr, "%s%s ", dirStr, typeStr );

                    printf ( "Test %d\n", testNumber );
                    printf ( "%-20s[LONG] Writing to RAM aligned... \n", testStr );

                    clock_gettime ( CLOCK_REALTIME, &tmsStart );

                    for ( uint32_t n = 0, add = startAdd; add < length - 4; n += 4, add += 4) 
                    {
                        d32 = *( (uint32_t *) &garbagePtr [n] );

                        write32 ( add, d32 );
                        rd32 = read32 ( add );

                        if ( d32 != rd32 ) 
                        {
                            if ( errors < 10 )
                            {
                                if (errors == 0)
                                    printf ( "\n" );

                                printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, add, d32, rd32 );
                            }

                            errors++;
                        }

                        if ( (n) % (64 * 1024)  == 0 )
                        {
                            printf ( "." );
                            fflush ( stdout );
                        }
                    }

                    clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                    printf ( "\n" );

                break;

                case TEST_32_ODD:

                    sprintf ( typeStr, "32_ODD:" );
                    sprintf ( testStr, "%s%s ", dirStr, typeStr );

                    printf ( "Test %d\n", testNumber );
                    printf ( "%-20s[LONG] Writing to RAM unaligned... \n", testStr );

                    clock_gettime ( CLOCK_REALTIME, &tmsStart );

                    for ( uint32_t n = 1, add = startAdd + 1; add < length - 4; n += 4, add += 4) 
                    {
                        d32 = *( (uint32_t *) &garbagePtr [n] );

                        write8  ( add, (d32 & 0x0000FF) );
                        write16 ( add + 1, htobe16 ( ( (d32 & 0x00FFFF00) >> 8) ) );
                        write8  ( add + 3, (d32 & 0xFF000000) >> 24);

                        rd32  = read8 (add);
                        rd32 |= (be16toh ( read16 (add + 1) ) << 8);
                        rd32 |= (read8 (add + 3) << 24 );

                        if ( d32 != rd32 ) 
                        {
                            if ( errors < 10 )
                            {
                                if (errors == 0)
                                    printf ( "\n" );

                                printf ( "%sData mismatch at $%.6X: %.4X expected, found %.4X.\n", testStr, add, d32, rd32 );
                            }

                            errors++;
                        }

                        if ( (n - 1) % (64 * 1024)  == 0 )
                        {
                            printf ( "." );
                            fflush ( stdout );
                        }
                    }

                    clock_gettime ( CLOCK_REALTIME, &tmsEnd );

                    printf ( "\n" );

                break;
            }
                

        break;
    }

    nanoStart = (tmsStart.tv_sec * 1000) + (tmsStart.tv_nsec / 1000000);
    nanoEnd   = (tmsEnd.tv_sec * 1000) + (tmsEnd.tv_nsec / 1000000);

    totalErrors += errors;
    testNumber++;

    uint32_t calcLength = (length - startAdd);

    /* recalculate data transfer size as the MEM_WRITES have a read and write component */
    if ( direction == MEM_WRITE ) //&& (type == TEST_8 || type == TEST_8_ODD || type == TEST_8_RANDOM) )
    {
        calcLength *= 2;
    }

    else if ( direction == MEM_READ && (type == TEST_8_RANDOM || type == TEST_16_RANDOM || type == TEST_32_RANDOM) )
    {
        calcLength *= 2;
    }

    


    printf ( "%-20sCompleted %sin %d ms (%.2f KB/s)\nTotal errors = %d\n\n", 
        testStr, 
        errors ? "with errors " : "",
        (nanoEnd - nanoStart), 
        (( (float)calcLength / (float)(nanoEnd - nanoStart)) * 1000.0) / 1024,     /* KB/s */
        totalErrors );
	
	if( errors )
		exit(1);
}


void clearmem ( uint32_t length, uint32_t *duration )
{
    struct timespec tmsStart, tmsEnd;


    clock_gettime ( CLOCK_REALTIME, &tmsStart );
    
    for ( uint32_t n = 0; n < length; n += 2 ) {

        write16 (n, 0x0000);

        if ( n % (64 * 1024)  == 0 )
        {
            printf ( "." );
            fflush ( stdout );
        }
    }

    clock_gettime ( CLOCK_REALTIME, &tmsEnd );

    long int nanoStart = (tmsStart.tv_sec * 1000) + (tmsStart.tv_nsec / 1000000);
    long int nanoEnd = (tmsEnd.tv_sec * 1000) + (tmsEnd.tv_nsec / 1000000);

    *duration = (nanoEnd - nanoStart);
}
