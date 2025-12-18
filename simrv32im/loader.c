#include <stdio.h>
#include <inttypes.h>
#include "cpu.h"
#include "loader.h"
#include "debugger.h"


t_ldrError ldrLoadBinary(
    const char *path, t_memAddress baseAddr, t_memAddress entry)
{
  dbgPrintf("Loading raw binary file \"%s\" at address %" PRIu32 "\n", path);

  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return LDR_FILE_ERROR;

  if (fseek(fp, 0, SEEK_END) < 0)
    return LDR_FILE_ERROR;
  long fpos = ftell(fp);
  if (fpos < 0 || fpos > 0x8000000L) {
    fclose(fp);
    return LDR_FILE_ERROR;
  }
  t_memSize size = (t_memSize)fpos;
  if (fseek(fp, 0, SEEK_SET) < 0) {
    fclose(fp);
    return LDR_FILE_ERROR;
  }

  uint8_t *buf;
  if (memMapArea(baseAddr, size, &buf) != MEM_NO_ERROR) {
    fclose(fp);
    return LDR_MEMORY_ERROR;
  }
  if (fread(buf, size, 1, fp) < 1) {
    fclose(fp);
    return LDR_FILE_ERROR;
  }

  cpuReset(entry);

  fclose(fp);
  return LDR_NO_ERROR;
}


#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16

#define EI_MAG0 0    /* File identification */
#define EI_MAG1 1    /* File identification */
#define EI_MAG2 2    /* File identification */
#define EI_MAG3 3    /* File identification */
#define EI_CLASS 4   /* File class */
#define EI_DATA 5    /* Data encoding */
#define EI_VERSION 6 /* File version */
#define EI_PAD 7     /* Start of padding bytes */

#define ELFCLASSNONE 0 /* Invalid class  */
#define ELFCLASS32 1   /* 32-bit objects */

#define ELFDATANONE 0 /* Invalid data encoding */
#define ELFDATA2LSB 1 /* Little-endian */

#define ET_NONE 0 /* No file type */
#define ET_EXEC 2 /* Executable file */

#define EM_NONE 0     /* No machine */
#define EM_RISCV 0xF3 /* RISC-V */

typedef struct __attribute__((packed)) Elf32_Ehdr {
  unsigned char e_ident[EI_NIDENT];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

#define PT_NULL 0 /* Ignored segment */
#define PT_LOAD 1 /* Loadable segment */
#define PT_NOTE 4 /* Target-dependent auxiliary information */

typedef struct __attribute__((packed)) Elf32_Phdr {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

static uint32_t fromLE32(uint32_t v)
{
  uint32_t res;
  union {
    uint32_t v;
    uint8_t bytes[4];
  } in;
  in.v = v;
  res = (uint32_t)in.bytes[0];
  res |= (uint32_t)in.bytes[1] << 8;
  res |= (uint32_t)in.bytes[2] << 16;
  res |= (uint32_t)in.bytes[3] << 24;
  return res;
}

static uint16_t fromLE16(uint16_t v)
{
  uint16_t res;
  union {
    uint16_t v;
    uint8_t bytes[2];
  } in;
  in.v = v;
  res = (uint16_t)in.bytes[0];
  res |= (uint16_t)in.bytes[1] << 8;
  return res;
}

t_ldrError ldrLoadELF(const char *path)
{
  t_ldrError res = LDR_NO_ERROR;

  dbgPrintf("Loading ELF file \"%s\"\n", path);

  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return LDR_FILE_ERROR;

  Elf32_Ehdr header;
  if (fread(&header, sizeof(Elf32_Ehdr), 1, fp) < 1)
    goto read_error;
  if (header.e_ident[EI_MAG0] != 0x7f || header.e_ident[EI_MAG1] != 'E' ||
      header.e_ident[EI_MAG2] != 'L' || header.e_ident[EI_MAG3] != 'F' ||
      header.e_ident[EI_CLASS] != ELFCLASS32 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_ident[EI_VERSION] != 1)
    goto invalid_file;
  if (fromLE16(header.e_type) != ET_EXEC || fromLE32(header.e_version) != 1)
    goto invalid_file;
  if (fromLE16(header.e_machine) != EM_RISCV)
    goto invalid_arch;

  long phnum = fromLE16(header.e_phnum);
  long phoff = fromLE32(header.e_phoff);
  long phentsize = fromLE16(header.e_phentsize);
  for (long phi = 0; phi < phnum; phi++) {
    Elf32_Phdr segment;
    fseek(fp, (long)(phoff + phi * phentsize), SEEK_SET);
    if (fread(&segment, sizeof(Elf32_Phdr), 1, fp) < 1)
      goto read_error;

    Elf32_Word ptype = fromLE32(segment.p_type);
    if (ptype == PT_NULL || ptype == PT_NOTE)
      continue;
    if (ptype != PT_LOAD)
      goto invalid_file;

    Elf32_Off poffset = fromLE32(segment.p_offset);
    Elf32_Word pfilesz = fromLE32(segment.p_filesz);
    Elf32_Word pvaddr = fromLE32(segment.p_vaddr);
    Elf32_Word pmemsz = fromLE32(segment.p_memsz);
    dbgPrintf("Loaded section at 0x%08" PRIx32 " (size=0x%08" PRIx32
              ") to 0x%08" PRIx32 " (size=0x%08" PRIx32 ")\n",
        poffset, pfilesz, pvaddr, pmemsz);
    if (pmemsz > 0) {
      uint8_t *buf;
      if (memMapArea(pvaddr, pmemsz, &buf) != MEM_NO_ERROR)
        goto mem_error;
      if (pfilesz > 0) {
        fseek(fp, (long)poffset, SEEK_SET);
        size_t readsz = MIN(pmemsz, pfilesz);
        if (fread(buf, readsz, 1, fp) < 1)
          goto read_error;
      }
    }
  }

  Elf32_Addr entry = fromLE32(header.e_entry);
  dbgPrintf("Setting the entry point to 0x%" PRIx32 "\n", entry);
  cpuReset(entry);

  goto cleanup;
mem_error:
  res = LDR_MEMORY_ERROR;
  goto cleanup;
read_error:
  res = LDR_FILE_ERROR;
  goto cleanup;
invalid_file:
  res = LDR_INVALID_FORMAT;
  goto cleanup;
invalid_arch:
  res = LDR_INVALID_ARCH;
cleanup:
  fclose(fp);
  return res;
}


t_ldrFileType ldrDetectExecType(const char *path)
{
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return LDR_FORMAT_DETECT_ERROR;

  char buf[4];
  if (fread(buf, 4, 1, fp) < 1)
    goto file_error;

  t_ldrFileType res;
  if (buf[EI_MAG0] == 0x7f && buf[EI_MAG1] == 'E' && buf[EI_MAG2] == 'L' &&
      buf[EI_MAG3] == 'F')
    res = LDR_FORMAT_ELF;
  else
    res = LDR_FORMAT_BINARY;

  goto cleanup;
file_error:
  res = LDR_FORMAT_DETECT_ERROR;
cleanup:
  fclose(fp);
  return res;
}
