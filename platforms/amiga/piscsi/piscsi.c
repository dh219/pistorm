#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include "piscsi.h"
#include "piscsi-enums.h"
#include "../hunk-reloc.h"
#include "../../../config_file/config_file.h"
#include "../../../gpio/ps_protocol.h"

// Comment this line to restore debug output:
//#define printf(...)

#ifdef FAKESTORM
#define lseek64 lseek
#endif

extern struct emulator_config *cfg;
extern void stop_cpu_emulation(uint8_t disasm_cur);

struct piscsi_dev devs[8];
uint8_t piscsi_cur_drive = 0;
uint32_t piscsi_u32[4];
uint32_t piscsi_dbg[8];
uint32_t piscsi_rom_size = 0;
uint8_t *piscsi_rom_ptr;

extern unsigned char ac_piscsi_rom[];

static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};

//static const char *partition_marker = "PART";

struct hunk_info piscsi_hinfo;
struct hunk_reloc piscsi_hreloc[256];

void piscsi_init() {
    for (int i = 0; i < 8; i++) {
        devs[i].fd = -1;
        devs[i].lba = 0;
        devs[i].c = devs[i].h = devs[i].s = 0;
    }

    FILE *in = fopen("./platforms/amiga/piscsi/piscsi.rom", "rb");
    if (in == NULL) {
        printf("[PISCSI] Could not open PISCSI Boot ROM file for reading.\n");
        // Zero out the boot ROM offset from the autoconfig ROM.
        ac_piscsi_rom[20] = 0;
        ac_piscsi_rom[21] = 0;
        ac_piscsi_rom[22] = 0;
        ac_piscsi_rom[23] = 0;
        return;
    }
    fseek(in, 0, SEEK_END);
    piscsi_rom_size = ftell(in);
    fseek(in, 0, SEEK_SET);
    piscsi_rom_ptr = malloc(piscsi_rom_size);
    fread(piscsi_rom_ptr, piscsi_rom_size, 1, in);
    fclose(in);

    // Parse the hunks in the device driver to find relocation offsets
    in = fopen("./platforms/amiga/piscsi/device_driver_amiga/pi-scsi.device", "rb");
    fseek(in, 0x0, SEEK_SET);
    process_hunks(in, &piscsi_hinfo, piscsi_hreloc);

    fclose(in);
    printf("[PISCSI] Loaded Boot ROM.\n");
}

void piscsi_find_partitions(struct piscsi_dev *d) {
    int fd = d->fd;
    char *block = malloc(512);
    int cur_partition = 0;
    uint8_t tmp;

    if (!d->rdb || d->rdb->rdb_PartitionList == 0) {
        printf("[PISCSI] No partitions on disk.\n");
        return;
    }

    lseek(fd, be32toh(d->rdb->rdb_PartitionList) * 512, SEEK_SET);
next_partition:;
    read(fd, block, 512);

    struct PartitionBlock *pb = (struct PartitionBlock *)block;
    tmp = pb->pb_DriveName[0];
    pb->pb_DriveName[tmp + 1] = 0x00;
    printf("Partition %d: %s\n", cur_partition, pb->pb_DriveName + 1);
    d->pb[cur_partition] = pb;

    if (d->pb[cur_partition]->pb_Next != 0xFFFFFFFF) {
        uint64_t next = be32toh(pb->pb_Next);
        block = malloc(512);
        lseek64(fd, next * 512, SEEK_SET);
        cur_partition++;
        printf("Next partition at block %d.\n", be32toh(pb->pb_Next));
        goto next_partition;
    }
    printf("No more partitions on disk.\n");

    return;
}

int piscsi_parse_rdb(struct piscsi_dev *d) {
    int fd = d->fd;
    int i = 0;
    uint8_t *block = malloc(512);

    lseek(fd, 0, SEEK_SET);
    for (i = 0; i < RDB_BLOCK_LIMIT; i++) {
        read(fd, block, 512);
        uint32_t first = be32toh(*((uint32_t *)&block[0]));
        if (first == RDB_IDENTIFIER)
            goto rdb_found;
        else {
            printf("Block %d is not RDB, looking for %.8X, found %.8X\n", i, RDB_IDENTIFIER, first);
        }
    }
    goto no_rdb_found;
rdb_found:;
    struct RigidDiskBlock *rdb = (struct RigidDiskBlock *)block;
    printf("[PISCSI] RDB found at block %d.\n", i);
    d->c = be32toh(rdb->rdb_Cylinders);
    d->h = be32toh(rdb->rdb_Heads);
    d->s = be32toh(rdb->rdb_Sectors);
    printf("[PISCSI] RDB - first partition at block %d.\n", be32toh(rdb->rdb_PartitionList));
    d->rdb = rdb;
    sprintf(d->rdb->rdb_DriveInitName, "pi-scsi.device");
    return 0;

no_rdb_found:;
    if (block)
        free(block);

    return -1;
}

void piscsi_map_drive(char *filename, uint8_t index) {
    if (index > 7) {
        printf("[PISCSI] Drive index %d out of range.\nUnable to map file %s to drive.\n", index, filename);
        return;
    }

    int32_t tmp_fd = open(filename, O_RDWR);
    if (tmp_fd == -1) {
        printf("[PISCSI] Failed to open file %s, could not map drive %d.\n", filename, index);
        return;
    }

    struct piscsi_dev *d = &devs[index];

    uint64_t file_size = lseek(tmp_fd, 0, SEEK_END);
    d->fs = file_size;
    d->fd = tmp_fd;
    lseek(tmp_fd, 0, SEEK_SET);
    printf("[PISCSI] Map %d: [%s] - %llu bytes.\n", index, filename, file_size);

    if (piscsi_parse_rdb(d) == -1) {
        printf("[PISCSI] No RDB found on disk, making up some CHS values.\n");
        d->h = 64;
        d->s = 63;
        d->c = (file_size / 512) / (d->s * d->h);
    }
    printf("[PISCSI] CHS: %d %d %d\n", d->c, d->h, d->s);

    piscsi_find_partitions(d);
    //stop_cpu_emulation(1);
}

void piscsi_unmap_drive(uint8_t index) {
    if (devs[index].fd != -1) {
        printf("[PISCSI] Unmapped drive %d.\n", index);
        close (devs[index].fd);
        devs[index].fd = -1;
    }
}

char *io_cmd_name(int index) {
    switch (index) {
        case CMD_INVALID: return "INVALID";
        case CMD_RESET: return "RESET";
        case CMD_READ: return "READ";
        case CMD_WRITE: return "WRITE";
        case CMD_UPDATE: return "UPDATE";
        case CMD_CLEAR: return "CLEAR";
        case CMD_STOP: return "STOP";
        case CMD_START: return "START";
        case CMD_FLUSH: return "FLUSH";
        case TD_MOTOR: return "TD_MOTOR";
        case TD_SEEK: return "SEEK";
        case TD_FORMAT: return "FORMAT";
        case TD_REMOVE: return "REMOVE";
        case TD_CHANGENUM: return "CHANGENUM";
        case TD_CHANGESTATE: return "CHANGESTATE";
        case TD_PROTSTATUS: return "PROTSTATUS";
        case TD_RAWREAD: return "RAWREAD";
        case TD_RAWWRITE: return "RAWWRITE";
        case TD_GETDRIVETYPE: return "GETDRIVETYPE";
        case TD_GETNUMTRACKS: return "GETNUMTRACKS";
        case TD_ADDCHANGEINT: return "ADDCHANGEINT";
        case TD_REMCHANGEINT: return "REMCHANGEINT";
        case TD_GETGEOMETRY: return "GETGEOMETRY";
        case TD_EJECT: return "EJECT";
        case TD_LASTCOMM: return "LASTCOMM/READ64";
        case TD_WRITE64: return "WRITE64";
        case HD_SCSICMD: return "HD_SCSICMD";
        case NSCMD_DEVICEQUERY: return "NSCMD_DEVICEQUERY";
        case NSCMD_TD_READ64: return "NSCMD_TD_READ64";
        case NSCMD_TD_WRITE64: return "NSCMD_TD_WRITE64";
        case NSCMD_TD_FORMAT64: return "NSCMD_TD_FORMAT64";

        default:
            return "!!!Unhandled IO command";
    }
}

char *scsi_cmd_name(int index) {
    switch(index) {
        case 0x00: return "TEST UNIT READY";
        case 0x12: return "INQUIRY";
        case 0x08: return "READ (6)";
        case 0x0A: return "WRITE (6)";
        case 0x28: return "READ (10)";
        case 0x2A: return "WRITE (10)";
        case 0x25: return "READ CAPACITY";
        case 0x1A: return "MODE SENSE";
        case 0x37: return "READ DEFECT DATA";
        default:
            return "!!!Unhandled SCSI command";
    }
}

void print_piscsi_debug_message(int index) {
    switch (index) {
        case DBG_INIT:
            printf("[PISCSI] Initializing devices.\n");
            break;
        case DBG_OPENDEV:
            printf("[PISCSI] Opening device %d (%d). Flags: %d (%.2X)\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2], piscsi_dbg[2]);
            break;
        case DBG_CLEANUP:
            printf("[PISCSI] Cleaning up.\n");
            break;
        case DBG_CHS:
            printf("[PISCSI] C/H/S: %d / %d / %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2]);
            break;
        case DBG_BEGINIO:
            printf("[PISCSI] BeginIO: io_Command: %d - io_Flags = %d - quick: %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2]);
            break;
        case DBG_ABORTIO:
            printf("[PISCSI] AbortIO!\n");
            break;
        case DBG_SCSICMD:
            printf("[PISCSI] SCSI Command %d (%s)\n", piscsi_dbg[1], scsi_cmd_name(piscsi_dbg[1]));
            printf("Len: %d - %.2X %.2X %.2X - Command Length: %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2], piscsi_dbg[3], piscsi_dbg[4]);
            break;
        case DBG_SCSI_UNKNOWN_MODESENSE:
            printf("SCSI: Unknown modesense %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_SCSI_UNKNOWN_COMMAND:
            printf("SCSI: Unknown command %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_SCSIERR:
            printf("SCSI: An error occured: %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_IOCMD:
            printf("[PISCSI] IO Command %d (%s)\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
            break;
        case DBG_IOCMD_UNHANDLED:
            printf("[PISCSI] WARN: IO command %.4X (%s) is unhandled by driver.\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
    }
}

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type) {
    int32_t r;

    struct piscsi_dev *d = &devs[piscsi_cur_drive];

    uint16_t cmd = (addr & 0xFFFF);

    switch (cmd) {
        case PISCSI_CMD_READ64:
        case PISCSI_CMD_READ:
            d = &devs[val];
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted read from unmapped drive %d.\n", piscsi_cur_drive);
                break;
            }

            if (cmd == PISCSI_CMD_READ) {
                printf("[PISCSI] %d byte READ from block %d to address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
                d->lba = piscsi_u32[0];
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
            }
            else {
                uint64_t src = piscsi_u32[3];
                src = (src << 32) | piscsi_u32[0];
                printf("[PISCSI] %d byte READ64 from block %lld to address %.8X\n", piscsi_u32[1], (src / 512), piscsi_u32[2]);
                d->lba = (src / 512);
                lseek64(d->fd, src, SEEK_SET);
            }

            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1 && cfg->map_type[r] == MAPTYPE_RAM) {
                printf("[PISCSI] \"DMA\" Read goes to mapped range %d.\n", r);
                read(d->fd, cfg->map_data[r] + piscsi_u32[2] - cfg->map_offset[r], piscsi_u32[1]);
            }
            else {
                printf("[PISCSI] No mapped range found for read.\n");
                uint8_t c = 0;
                for (uint32_t i = 0; i < piscsi_u32[1]; i++) {
                    read(d->fd, &c, 1);
                    write8(piscsi_u32[2] + i, (uint32_t)c);
                }
            }
            break;
        case PISCSI_CMD_WRITE64:
        case PISCSI_CMD_WRITE:
            d = &devs[val];
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted write to unmapped drive %d.\n", piscsi_cur_drive);
                break;
            }

            if (cmd == PISCSI_CMD_WRITE) {
                printf("[PISCSI] %d byte WRITE to block %d from address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
                d->lba = piscsi_u32[0];
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
            }
            else {
                uint64_t src = piscsi_u32[3];
                src = (src << 32) | piscsi_u32[0];
                printf("[PISCSI] %d byte WRITE64 to block %lld from address %.8X\n", piscsi_u32[1], (src / 512), piscsi_u32[2]);
                d->lba = (src / 512);
                lseek64(d->fd, src, SEEK_SET);
            }

            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1) {
                printf("[PISCSI] \"DMA\" Write comes from mapped range %d.\n", r);
                write(d->fd, cfg->map_data[r] + piscsi_u32[2] - cfg->map_offset[r], piscsi_u32[1]);
            }
            else {
                printf("[PISCSI] No mapped range found for write.\n");
                uint8_t c = 0;
                for (uint32_t i = 0; i < piscsi_u32[1]; i++) {
                    c = read8(piscsi_u32[2] + i);
                    write(d->fd, &c, 1);
                }
            }
            break;
        case PISCSI_CMD_ADDR1: case PISCSI_CMD_ADDR2: case PISCSI_CMD_ADDR3: case PISCSI_CMD_ADDR4: {
            int i = ((addr & 0xFFFF) - PISCSI_CMD_ADDR1) / 4;
            piscsi_u32[i] = val;
            break;
        }
        case PISCSI_CMD_DRVNUM:
            if (val != 0) {
                if (val < 10) // Kludge for GiggleDisk
                    piscsi_cur_drive = val;
                else if (val >= 10 && val % 10 != 0)
                    piscsi_cur_drive = 255;
                else
                    piscsi_cur_drive = val / 10;
            }
            else
                piscsi_cur_drive = val;
            printf("[PISCSI] (%s) Drive number set to %d (%d)\n", op_type_names[type], piscsi_cur_drive, val);
            break;
        case PISCSI_CMD_DEBUGME:
            printf("[PISCSI] DebugMe triggered (%d).\n", val);
            stop_cpu_emulation(1);
            break;
        case PISCSI_CMD_DRIVER: {
            printf("[PISCSI] Driver copy/patch called, destination address %.8X.\n", val);
            int r = get_mapped_item_by_address(cfg, val);
            if (r != -1) {
                uint32_t addr = val - cfg->map_offset[r];
                uint8_t *dst_data = cfg->map_data[r];
                memcpy(dst_data + addr, piscsi_rom_ptr + 0x400, 0x3C00);

                piscsi_hinfo.base_offset = val;
                
                reloc_hunks(piscsi_hreloc, dst_data + addr, &piscsi_hinfo);
                stop_cpu_emulation(1);
            }
            else {
                for (int i = 0; i < 0x3C00; i++) {
                    uint8_t src = piscsi_rom_ptr[0x400 + i];
                    write8(addr + i, src);
                }
            }
            break;
        }
        case PISCSI_DBG_VAL1: case PISCSI_DBG_VAL2: case PISCSI_DBG_VAL3: case PISCSI_DBG_VAL4:
        case PISCSI_DBG_VAL5: case PISCSI_DBG_VAL6: case PISCSI_DBG_VAL7: case PISCSI_DBG_VAL8: {
            int i = ((addr & 0xFFFF) - PISCSI_DBG_VAL1) / 4;
            piscsi_dbg[i] = val;
            break;
        }
        case PISCSI_DBG_MSG:
            print_piscsi_debug_message(val);
            break;
        default:
            printf("[PISCSI] WARN: Unhandled %s register write to %.8X: %d\n", op_type_names[type], addr, val);
            break;
    }
}

#define PIB 0x00

uint32_t handle_piscsi_read(uint32_t addr, uint8_t type) {
    if (type) {}

    if ((addr & 0xFFFF) >= PISCSI_CMD_ROM) {
        uint32_t romoffs = (addr & 0xFFFF) - PISCSI_CMD_ROM;
        if (romoffs < (piscsi_rom_size + PIB)) {
            //printf("[PISCSI] %s read from Boot ROM @$%.4X (%.8X): ", op_type_names[type], romoffs, addr);
            uint32_t v = 0;
            switch (type) {
                case OP_TYPE_BYTE:
                    v = piscsi_rom_ptr[romoffs - PIB];
                    //printf("%.2X\n", v);
                    break;
                case OP_TYPE_WORD:
                    v = be16toh(*((uint16_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    //printf("%.4X\n", v);
                    break;
                case OP_TYPE_LONGWORD:
                    v = be32toh(*((uint32_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    //printf("%.8X\n", v);
                    break;
            }
            return v;
        }
        return 0;
    }
    
    switch (addr & 0xFFFF) {
        case PISCSI_CMD_DRVTYPE:
            if (devs[piscsi_cur_drive].fd == -1) {
                printf("[PISCSI] %s Read from DRVTYPE %d, drive not attached.\n", op_type_names[type], piscsi_cur_drive);
                return 0;
            }
            printf("[PISCSI] %s Read from DRVTYPE %d, drive attached.\n", op_type_names[type], piscsi_cur_drive);
            return 1;
            break;
        case PISCSI_CMD_DRVNUM:
            return piscsi_cur_drive;
            break;
        case PISCSI_CMD_CYLS:
            printf("[PISCSI] %s Read from CYLS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].c);
            return devs[piscsi_cur_drive].c;
            break;
        case PISCSI_CMD_HEADS:
            printf("[PISCSI] %s Read from HEADS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].h);
            return devs[piscsi_cur_drive].h;
            break;
        case PISCSI_CMD_SECS:
            printf("[PISCSI] %s Read from SECS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].s);
            return devs[piscsi_cur_drive].s;
            break;
        case PISCSI_CMD_BLOCKS: {
            uint32_t blox = devs[piscsi_cur_drive].fs / 512;
            printf("[PISCSI] %s Read from BLOCKS %d: %d\n", op_type_names[type], piscsi_cur_drive, (uint32_t)(devs[piscsi_cur_drive].fs / 512));
            printf("fs: %lld (%d)\n", devs[piscsi_cur_drive].fs, blox);
            return blox;
            break;
        }
        default:
            printf("[PISCSI] Unhandled %s register read from %.8X\n", op_type_names[type], addr);
            break;
    }

    return 0;
}
