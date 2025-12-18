#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "errors.h"
#include "output.h"

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT    16

#define EI_MAG0      0     // File identification
#define EI_MAG1      1     // File identification
#define EI_MAG2      2     // File identification
#define EI_MAG3      3     // File identification
#define EI_CLASS     4     // File class
#define EI_DATA      5     // Data encoding
#define EI_VERSION   6     // File version
#define EI_PAD       7     // Start of padding bytes

#define ELFCLASSNONE 0     // Invalid class
#define ELFCLASS32   1     // 32-bit objects

#define ELFDATANONE  0     // Invalid data encoding
#define ELFDATA2LSB  1     // Little-endian

#define ET_NONE      0     // No file type
#define ET_EXEC      2     // Executable file

#define EM_NONE      0     // No machine
#define EM_RISCV     0xF3  // RISC-V

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

#define PT_NULL  0    // Ignored segment
#define PT_LOAD  1    // Loadable segment
#define PT_NOTE  4    // Target-dependent auxiliary information

#define PF_R     0x4  // Program segment Read flag
#define PF_W     0x2  // Program segment Write flag
#define PF_X     0x1  // Program segment eXecute flag

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

#define SHN_UNDEF     0         // undefined section ID

#define SHT_NULL      0         // null section
#define SHT_PROGBITS  1         // section loaded with the program
#define SHT_STRTAB    3         // string table

#define SHF_WRITE     (1 << 0)  // section writable flag
#define SHF_ALLOC     (1 << 1)  // section initialized flag
#define SHF_EXECINSTR (1 << 2)  // section executable flag
#define SHF_STRINGS   (1 << 5)  // section string table flag

typedef struct __attribute__((packed)) Elf32_Shdr {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
} Elf32_Shdr;


static uint32_t toLE32(uint32_t v)
{
  union {
    uint32_t word;
    uint8_t bytes[4];
  } res;
  res.bytes[0] = v & 0xFF;
  res.bytes[1] = (v >> 8) & 0xFF;
  res.bytes[2] = (v >> 16) & 0xFF;
  res.bytes[3] = (v >> 24) & 0xFF;
  return res.word;
}

static uint16_t toLE16(uint16_t v)
{
  union {
    uint16_t word;
    uint8_t bytes[2];
  } res;
  res.bytes[0] = v & 0xFF;
  res.bytes[1] = (v >> 8) & 0xFF;
  return res.word;
}


typedef struct t_outStrTbl {
  char *buf;
  size_t bufSz;
  size_t tail;
} t_outStrTbl;

void initOutStrTbl(t_outStrTbl *tbl)
{
  tbl->bufSz = 64;
  tbl->tail = 0;
  tbl->buf = calloc(tbl->bufSz, sizeof(char));
  if (!tbl->buf)
    fatalError("out of memory");
  tbl->buf[tbl->tail++] = '\0';
}

void deinitOutStrTbl(t_outStrTbl *tbl)
{
  free(tbl->buf);
}

void outStrTblAddString(t_outStrTbl *tbl, char *str, Elf32_Word *outIdx)
{
  size_t strSz = strlen(str) + 1;
  if (tbl->bufSz - tbl->tail < strSz) {
    size_t newBufSz = tbl->bufSz * 2 + strSz;
    char *newBuf = realloc(tbl->buf, tbl->bufSz);
    if (!newBuf)
      fatalError("out of memory");
    tbl->buf = newBuf;
    tbl->bufSz = newBufSz;
  }
  strcpy(tbl->buf + tbl->tail, str);
  if (outIdx)
    *outIdx = (Elf32_Word)tbl->tail;
  tbl->tail += strSz;
}

Elf32_Shdr outputStrTabToELFSHdr(
    t_outStrTbl *tbl, Elf32_Addr fileOffset, Elf32_Word name)
{
  Elf32_Shdr shdr = {0};

  shdr.sh_name = toLE32(name);
  shdr.sh_type = toLE32(SHT_STRTAB);
  shdr.sh_flags = toLE32(SHF_STRINGS);
  shdr.sh_addr = toLE32(0);
  shdr.sh_offset = toLE32(fileOffset);
  shdr.sh_size = toLE32((Elf32_Word)tbl->tail);
  shdr.sh_link = toLE32(SHN_UNDEF);
  shdr.sh_info = toLE32(0);
  shdr.sh_addralign = toLE32(0);
  shdr.sh_entsize = toLE32(0);

  return shdr;
}

t_outError outputStrTabContentToFile(FILE *fp, long whence, t_outStrTbl *tbl)
{
  if (fseek(fp, whence, SEEK_SET) < 0)
    return OUT_FILE_ERROR;
  if (fwrite(tbl->buf, tbl->tail, 1, fp) < 1)
    return OUT_FILE_ERROR;
  return OUT_NO_ERROR;
}


Elf32_Phdr outputSecToELFPHdr(
    t_objSection *sec, Elf32_Addr fileOffset, Elf32_Word flags)
{
  Elf32_Phdr phdr = {0};
  uint32_t secSize = objSecGetSize(sec);

  phdr.p_type = toLE32(PT_LOAD);
  phdr.p_vaddr = toLE32(objSecGetStart(sec));
  phdr.p_offset = toLE32(fileOffset);
  phdr.p_filesz = toLE32(secSize);
  phdr.p_memsz = toLE32(secSize);
  phdr.p_flags = toLE32(flags);
  phdr.p_align = toLE32(0);

  return phdr;
}

Elf32_Shdr outputSecToELFSHdr(
    t_objSection *sec, Elf32_Addr fileOffset, Elf32_Word name, Elf32_Word flags)
{
  Elf32_Shdr shdr = {0};

  shdr.sh_name = toLE32(name);
  shdr.sh_type = toLE32(SHT_PROGBITS);
  shdr.sh_flags = toLE32(flags);
  shdr.sh_addr = toLE32(objSecGetStart(sec));
  shdr.sh_offset = toLE32(fileOffset);
  shdr.sh_size = toLE32(objSecGetSize(sec));
  shdr.sh_link = toLE32(SHN_UNDEF);
  shdr.sh_info = toLE32(0);
  shdr.sh_addralign = toLE32(0);
  shdr.sh_entsize = toLE32(0);

  return shdr;
}

t_outError outputSecContentToFile(FILE *fp, long whence, t_objSection *sec)
{
  if (fseek(fp, whence, SEEK_SET) < 0)
    return OUT_FILE_ERROR;

  t_objSecItem *itm = objSecGetItemList(sec);
  for (; itm != NULL; itm = itm->next) {
    if (itm->class == OBJ_SEC_ITM_CLASS_VOID) {
      continue;
    } else if (itm->class == OBJ_SEC_ITM_CLASS_DATA) {
      if (itm->body.data.initialized) {
        if (fwrite(itm->body.data.data, itm->body.data.dataSize, 1, fp) < 1)
          return OUT_FILE_ERROR;
      } else {
        uint8_t tmp = 0;
        for (int i = 0; i < itm->body.data.dataSize; i++)
          if (fwrite(&tmp, sizeof(uint8_t), 1, fp) < 1)
            return OUT_FILE_ERROR;
      }
    } else if (itm->class == OBJ_SEC_ITM_CLASS_ALIGN_DATA) {
      if (itm->body.alignData.nopFill) {
        uint32_t tmp = toLE32(0x00000013); // nop = addi x0, x0, 0
        assert((itm->body.alignData.effectiveSize % 4) == 0);
        for (int i = 0; i < itm->body.alignData.effectiveSize; i += 4)
          if (fwrite(&tmp, sizeof(uint32_t), 1, fp) < 1)
            return OUT_FILE_ERROR;
      } else {
        uint8_t tmp = itm->body.alignData.fillByte;
        for (int i = 0; i < itm->body.alignData.effectiveSize; i++)
          if (fwrite(&tmp, sizeof(uint8_t), 1, fp) < 1)
            return OUT_FILE_ERROR;
      }
    } else {
      assert(0 && "bug, unexpected item type in section");
    }
  }
  return OUT_NO_ERROR;
}


enum {
  PRG_ID_TEXT = 0,
  PRG_ID_DATA,
  PRG_NUM
};

enum {
  SEC_ID_NULL = SHN_UNDEF,
  SEC_ID_TEXT,
  SEC_ID_DATA,
  SEC_ID_SYMTAB,
  SEC_NUM
};

typedef struct __attribute__((packed)) t_outputELFHead {
  Elf32_Ehdr e;
  Elf32_Phdr p[PRG_NUM];
  Elf32_Shdr s[SEC_NUM];
} t_outputELFHead;

t_outError outputToELF(t_object *obj, const char *fname)
{
  t_outError res = OUT_NO_ERROR;

  t_objSection *text = objGetSection(obj, OBJ_SECTION_TEXT);
  t_objSection *data = objGetSection(obj, OBJ_SECTION_DATA);

  t_outputELFHead head = {0};
  head.e.e_ident[EI_MAG0] = 0x7F;
  head.e.e_ident[EI_MAG1] = 'E';
  head.e.e_ident[EI_MAG2] = 'L';
  head.e.e_ident[EI_MAG3] = 'F';
  head.e.e_ident[EI_CLASS] = ELFCLASS32;
  head.e.e_ident[EI_DATA] = ELFDATA2LSB;
  head.e.e_ident[EI_VERSION] = 1;
  head.e.e_type = toLE16(ET_EXEC);
  head.e.e_machine = toLE16(EM_RISCV);
  head.e.e_version = toLE32(1);
  head.e.e_phoff = toLE32((Elf32_Off)((intptr_t)head.p - (intptr_t)&head.e));
  head.e.e_shoff = toLE32((Elf32_Off)((intptr_t)head.s - (intptr_t)&head.e));
  head.e.e_flags = toLE32(0);
  head.e.e_ehsize = toLE16(sizeof(Elf32_Ehdr));
  head.e.e_phentsize = toLE16(sizeof(Elf32_Phdr));
  head.e.e_phnum = toLE16(PRG_NUM);
  head.e.e_shentsize = toLE16(sizeof(Elf32_Shdr));
  head.e.e_shnum = toLE16(SEC_NUM);
  head.e.e_shstrndx = toLE16(SEC_ID_SYMTAB);

  t_objLabel *l_entry = objFindLabel(obj, "_start");
  if (!l_entry) {
    emitWarning(nullFileLocation,
        "_start symbol not found, entry will be start of .text section");
    head.e.e_entry = toLE32(objSecGetStart(text));
  } else {
    head.e.e_entry = toLE32(objLabelGetPointer(l_entry));
  }

  Elf32_Addr textAddr = sizeof(t_outputELFHead);
  Elf32_Addr dataAddr = textAddr + objSecGetSize(text);
  Elf32_Addr strtabAddr = dataAddr + objSecGetSize(data);

  t_outStrTbl strTbl;
  initOutStrTbl(&strTbl);
  Elf32_Word textSecName, dataSecName, strtabSecName;
  outStrTblAddString(&strTbl, ".text", &textSecName);
  outStrTblAddString(&strTbl, ".data", &dataSecName);
  outStrTblAddString(&strTbl, ".strtab", &strtabSecName);

  head.p[PRG_ID_TEXT] = outputSecToELFPHdr(text, textAddr, PF_R + PF_X);
  head.p[PRG_ID_DATA] = outputSecToELFPHdr(data, dataAddr, PF_R + PF_W);

  head.s[SEC_ID_TEXT] = outputSecToELFSHdr(
      text, textAddr, textSecName, SHF_ALLOC + SHF_EXECINSTR);
  head.s[SEC_ID_DATA] =
      outputSecToELFSHdr(data, dataAddr, dataSecName, SHF_ALLOC + SHF_WRITE);
  head.s[SEC_ID_SYMTAB] =
      outputStrTabToELFSHdr(&strTbl, strtabAddr, strtabSecName);

  FILE *fp = fopen(fname, "wb");
  if (fp == NULL) {
    res = OUT_FILE_ERROR;
    goto exit;
  }
  if (fwrite(&head, sizeof(t_outputELFHead), 1, fp) < 1) {
    res = OUT_FILE_ERROR;
    goto exit;
  }
  res = outputSecContentToFile(fp, textAddr, text);
  if (res != OUT_NO_ERROR)
    goto exit;
  res = outputSecContentToFile(fp, dataAddr, data);
  if (res != OUT_NO_ERROR)
    goto exit;
  res = outputStrTabContentToFile(fp, strtabAddr, &strTbl);
  if (res != OUT_NO_ERROR)
    goto exit;

exit:
  deinitOutStrTbl(&strTbl);
  if (fp)
    fclose(fp);
  return res;
}
