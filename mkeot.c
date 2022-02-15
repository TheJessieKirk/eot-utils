/* mkeot -- create an EOT file from an OTF or TTF file
 *
 * Creates an EOT file given one or more URL prefixes and an OTF/TTF
 * font file. For the moment, MicroType Express compression is not
 * performed. Also, no obfuscation is done, and no subsetting. The
 * URLs can currently only be in ASCII.
 *
 * Author: Bert Bos <bert@w3.org>
 * Created: 24 January 2010
 *
 * Copyright 2010 W3C (MIT, ERCIM, Keio), see
 * http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
 */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdbool.h>
#include <sysexits.h>
#include <assert.h>
#include <string.h>

#define FSTYPE_RESTRICTED			0x0002
#define FSTYPE_PREVIEW				0x0004
#define FSTYPE_EDITABLE				0x0008
#define FSTYPE_NOSUBSETTING			0x0100
#define FSTYPE_BITMAP				0x0200

/* sfnt version numbers for OpenType/TrueType */
#define SFNT_OTTO (('O' << 24) | ('T' << 16) | ('T' << 8) | 'O')
#define SFNT_TRUE (('t' << 24) | ('r' << 16) | ('u' << 8) | 'e')
#define SFNT_TYP1 (('t' << 24) | ('y' << 16) | ('p' << 8) | '1')
#define SFNT_VERSION_1_0 0x00010000

typedef struct {
  unsigned long EOTSize;
  unsigned long FontDataSize;
  unsigned long Version;
  unsigned long Flags;
  unsigned char FontPANOSE[10];
  unsigned char Charset;
  unsigned char Italic;
  unsigned long Weight;
  unsigned short fsType;
  unsigned short MagicNumber;	/* = 0x504C */
  unsigned long UnicodeRange1;
  unsigned long UnicodeRange2;
  unsigned long UnicodeRange3;
  unsigned long UnicodeRange4;
  unsigned long CodePageRange1;
  unsigned long CodePageRange2;
  unsigned long CheckSumAdjustment;
  unsigned long Reserved1;	/* = 0 */
  unsigned long Reserved2;	/* = 0 */
  unsigned long Reserved3;	/* = 0 */
  unsigned long Reserved4;	/* = 0 */
  unsigned short Padding1;	/* = 0 */
  unsigned short FamilyNameSize;
  unsigned char *FamilyName;	/* in UTF-16! */
  unsigned short Padding2;	/* = 0 */
  unsigned short StyleNameSize;
  unsigned char *StyleName;	/* in UTF-16! */
  unsigned short Padding3;	/* = 0 */
  unsigned short VersionNameSize;
  unsigned char *VersionName;	/* in UTF-16! */
  unsigned short Padding4;	/* = 0 */
  unsigned short FullNameSize;
  unsigned char *FullName;	/* in UTF-16! */
  unsigned short Padding5;	/* = 0 */
  unsigned short RootStringSize;
  unsigned char *RootString;	/* in UTF-16! */
  unsigned long RootStringCheckSum;
  unsigned long EUDCCodePage;
  unsigned short Padding6;	/* = 0 */
  unsigned short SignatureSize;
  unsigned char *Signature;
  unsigned long EUDCFlags;
  unsigned long EUDCFontSize;
  unsigned char *EUDCFontData;
} EOT_header;

typedef struct {
  char tag[4];
  unsigned long checkSum;
  unsigned long offset;
  unsigned long length;
} sfnt_table_record;

typedef struct {
  unsigned long sfnt_version;
  unsigned short numTables;
  unsigned short searchRange;
  unsigned short entrySelector;
  unsigned short rangeShift;
  sfnt_table_record *tables;
} sfnt_offset_table;

typedef struct {
  unsigned short version; /* = 0x0004 */
  short xAvgCharWidth;
  unsigned short usWeightClass;
  unsigned short usWidthClass;
  unsigned short fsType;
  short ySubscriptXSize;
  short ySubscriptYSize;
  short ySubscriptXOffset;
  short ySubscriptYOffset;
  short ySuperscriptXSize;
  short ySuperscriptYSize;
  short ySuperscriptXOffset;
  short ySuperscriptYOffset;
  short yStrikeoutSize;
  short yStrikeoutPosition;
  short sFamilyClass;
  unsigned char panose[10];
  unsigned long ulUnicodeRange1;
  unsigned long ulUnicodeRange2;
  unsigned long ulUnicodeRange3;
  unsigned long ulUnicodeRange4;
  char achVendID[4];
  unsigned short fsSelection;
  unsigned short usFirstCharIndex;
  unsigned short usLastCharIndex;
  short sTypoAscender;
  short sTypoDescender;
  short sTypoLineGap;
  unsigned short usWinAscent;
  unsigned short usWinDescent;
  unsigned long ulCodePageRange1;
  unsigned long ulCodePageRange2;
  short sxHeight;
  short sCapHeight;
  unsigned short usDefaultChar;
  unsigned short usBreakChar;
  unsigned short usMaxContext;
} OS2_table;

typedef struct {
  unsigned short length;
  unsigned short offset;
} LangTagRecord;

typedef struct {
  unsigned short platformID;
  unsigned short encodingID;
  unsigned short languageID;
  unsigned short nameID;
  unsigned short length;
  unsigned short offset;
} NameRecord;

typedef struct {
  unsigned short format;	/* = 0 or 1 */
  unsigned short count;
  unsigned short stringOffset;
  NameRecord *nameRecord;
  unsigned short langTagCount;
  LangTagRecord *langTagRecord;
  unsigned char **names;	/* UTF-16BE */
  unsigned char **langtags;	/* UTF-16BE */
} Name_table;

typedef struct {
  unsigned long Table_version_number; /* = 0x00010000 */
  unsigned long fontRevision;
  unsigned long checkSumAdjustment;
  unsigned long magicNumber;	/* = 0x5F0F3CF5 */
  unsigned short flags;
  unsigned short unitsPerEm;
  unsigned long long created;
  unsigned long long modified;
  short xMin;
  short yMin;
  short xMax;
  short yMax;
  unsigned short macStyle;
  unsigned short lowestRecPPEM;
  short fontDirectionHint;
  short indexToLocFormat;
  short glyphDataFormat;
} Head_table;


/* read_8be -- read a big endian unsigned 64-bit number */
static bool read_8be(FILE *f, unsigned long long *x)
{
  unsigned char s[8];

  if (fread(s, 1, 8, f) != 8) return false;
  *x = ((unsigned long long)s[0] << 56) |
    ((unsigned long long)s[1] << 48) |
    ((unsigned long long)s[2] << 40) |
    ((unsigned long long)s[3] << 32) |
    ((unsigned long long)s[4] << 24) |
    ((unsigned long long)s[5] << 16) |
    ((unsigned long long)s[6] << 8) |
    (unsigned long long)s[7];
  return true;
}


/* read_4be -- read a big endian unsigned long */
static bool read_4be(FILE *f, unsigned long *x)
{
  unsigned char s[4];

  if (fread(s, 1, 4, f) != 4) return false;
  *x = (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
  return true;
}


/* write_4le -- write a little endian unsigned long */
static bool write_4le(FILE *f, unsigned long x)
{
  unsigned char s[4];

  s[0] = x & 0x000000ff;
  s[1] = (x & 0x0000ff00) >> 8;
  s[2] = (x & 0x00ff0000) >> 16;
  s[3] = x >> 24;
  return fwrite(s, 1, 4, f) == 4;
}


/* read_2be -- read a big endian unsigned short */
static bool read_2be(FILE *f, unsigned short *x)
{
  unsigned char s[2];

  if (fread(s, 1, 2, f) != 2) return false;
  *x = (s[0] << 8) + s[1];
  return true;
}


/* read_2be_signed -- read a big endian short */
static bool read_2be_signed(FILE *f, short *x)
{
  unsigned char s[2];
  union {short s; unsigned short u;} h;

  if (fread(s, 1, 2, f) != 2) return false;
  h.u = (s[0] << 8) + s[1];
  *x = h.s;
  return true;
}


/* write_2le -- write a little endian unsigned short */
static bool write_2le(FILE *f, unsigned short x)
{
  unsigned char s[2];

  s[0] = x & 0x00ff;
  s[1] = x >> 8;
  return fwrite(s, 1, 2, f) == 2;
}


/* write_1 -- write one byte to file f */
static bool write_1(FILE *f, unsigned char x)
{
  return fwrite(&x, 1, 1, f) == 1;
}


/* read_1 -- read one byte from file f */
static bool read_1(FILE *f, unsigned char *x)
{
  return fread(x, 1, 1, f) == 1;
}


/* initialize_EOT_header -- fill an EOT struct with consistent values */
static void initialize_EOT_header(EOT_header *h)
{
  h->EOTSize = 120;
  h->FontDataSize = 0;
  h->Version = 0x00020002;
  h->Flags = 0;
  memset(h->FontPANOSE, 0, 10);
  h->Charset = 0;
  h->Italic = 0;
  h->Weight = 400;		/* "normal" */
  h->fsType = 0x0;		/* "installable" */
  h->MagicNumber = 0x504C;
  h->UnicodeRange1 = 0;
  h->UnicodeRange2 = 0;
  h->UnicodeRange3 = 0;
  h->UnicodeRange4 = 0;
  h->CodePageRange1 = 0;
  h->CodePageRange2 = 0;
  h->CheckSumAdjustment = 0;
  h->Reserved1 = 0;
  h->Reserved2 = 0;
  h->Reserved3 = 0;
  h->Reserved4 = 0;
  h->Padding1 = 0;
  h->FamilyNameSize = 0;
  h->FamilyName = NULL;
  h->Padding2 = 0;
  h->StyleNameSize = 0;
  h->StyleName = NULL;
  h->Padding3 = 0;
  h->VersionNameSize = 0;
  h->VersionName = NULL;
  h->Padding4 = 0;
  h->FullNameSize = 0;
  h->FullName = NULL;
  h->Padding5 = 0;
  h->RootStringSize = 0;
  h->RootString = NULL;
  h->RootStringCheckSum = 0;
  h->EUDCCodePage = 0;
  h->Padding6 = 0;
  h->SignatureSize = 0;
  h->Signature = NULL;
  h->EUDCFlags = 0;
  h->EUDCFontSize = 0;
  h->EUDCFontData = NULL;
}


/* write_EOT_header -- write an EOT header to file f */
static bool write_EOT_header(FILE *f, EOT_header h)
{
  if (!write_4le(f, h.EOTSize) ||
      !write_4le(f, h.FontDataSize) ||
      !write_4le(f, h.Version) ||
      !write_4le(f, h.Flags) ||
      fwrite(h.FontPANOSE, 1, 10, f) != 10 ||
      !write_1(f, h.Charset) ||
      !write_1(f, h.Italic) ||
      !write_4le(f, h.Weight) ||
      !write_2le(f, h.fsType) ||
      !write_2le(f, h.MagicNumber) ||
      !write_4le(f, h.UnicodeRange1) ||
      !write_4le(f, h.UnicodeRange2) ||
      !write_4le(f, h.UnicodeRange3) ||
      !write_4le(f, h.UnicodeRange4) ||
      !write_4le(f, h.CodePageRange1) ||
      !write_4le(f, h.CodePageRange2) ||
      !write_4le(f, h.CheckSumAdjustment) ||
      !write_4le(f, h.Reserved1) ||
      !write_4le(f, h.Reserved2) ||
      !write_4le(f, h.Reserved3) ||
      !write_4le(f, h.Reserved4) ||
      !write_2le(f, h.Padding1) ||
      !write_2le(f, h.FamilyNameSize) ||
      fwrite(h.FamilyName, 1, h.FamilyNameSize, f) != h.FamilyNameSize ||
      !write_2le(f, h.Padding2) ||
      !write_2le(f, h.StyleNameSize) ||
      fwrite(h.StyleName, 1, h.StyleNameSize, f) != h.StyleNameSize ||
      !write_2le(f, h.Padding3) ||
      !write_2le(f, h.VersionNameSize) ||
      fwrite(h.VersionName, 1, h.VersionNameSize, f) != h.VersionNameSize ||
      !write_2le(f, h.Padding4) ||
      !write_2le(f, h.FullNameSize) ||
      fwrite(h.FullName, 1, h.FullNameSize, f) != h.FullNameSize)
    return false;

  switch (h.Version) {
  case 0x0001000:
    break;
  case 0x00020001:
    if (!write_2le(f, h.Padding5) ||
	!write_2le(f, h.RootStringSize) ||
	fwrite(h.RootString, 1, h.RootStringSize, f) != h.RootStringSize)
      return false;
    break;
  case 0x00020002:
    if (!write_2le(f, h.Padding5) ||
	!write_2le(f, h.RootStringSize) ||
	fwrite(h.RootString, 1, h.RootStringSize, f) != h.RootStringSize ||
	!write_4le(f, h.RootStringCheckSum) ||
	!write_4le(f, h.EUDCCodePage) ||
	!write_2le(f, h.Padding6) ||
	!write_2le(f, h.SignatureSize) ||
	fwrite(h.Signature, 1, h.SignatureSize, f) != h.SignatureSize ||
	!write_4le(f, h.EUDCFlags) ||
	!write_4le(f, h.EUDCFontSize) ||
	fwrite(h.EUDCFontData, 1, h.EUDCFontSize, f) != h.EUDCFontSize)
      return false;
    break;
  default:
    assert(!"Cannot happen!");
  }
  return true;
}


/* read_sfnt_header -- get the directory of tables of an OpenType font */
static bool read_sfnt_header(FILE *f, sfnt_offset_table *h)
{
  unsigned short i;

  rewind(f);
  if (!read_4be(f, &h->sfnt_version) ||
      !read_2be(f, &h->numTables) ||
      !read_2be(f, &h->searchRange) ||
      !read_2be(f, &h->entrySelector) ||
      !read_2be(f, &h->rangeShift)) return false;
  if (h->sfnt_version != SFNT_OTTO &&
      h->sfnt_version != SFNT_TRUE &&
      h->sfnt_version != SFNT_TYP1 &&
      h->sfnt_version != SFNT_VERSION_1_0) return false;
  if (!(h->tables = malloc(h->numTables * sizeof(h->tables[0]))))
    err(EX_OSERR, NULL);
  for (i = 0; i < h->numTables; i++)
    if (fread(h->tables[i].tag, 1, 4, f) != 4 ||
	!read_4be(f, &h->tables[i].checkSum) ||
	!read_4be(f, &h->tables[i].offset) ||
	!read_4be(f, &h->tables[i].length)) return false;
  return true;
}


/* read_name_table -- find and read the Name table in an OpenType file */
static bool read_name_table(FILE *f, sfnt_offset_table sfnt, Name_table *t)
{
  unsigned short i = 0, j, k;
  long offset;

  /* Find the name table in the sfnt table directory and seek to it */
  while (i < sfnt.numTables && memcmp(sfnt.tables[i].tag, "name", 4)) i++;
  if (i >= sfnt.numTables) return false;
  if (fseek(f, sfnt.tables[i].offset, SEEK_SET) != 0) return false;

  /* Read the table */
  if (!read_2be(f, &t->format) ||
      !read_2be(f, &t->count) ||
      !read_2be(f, &t->stringOffset)) return false;
  t->nameRecord = malloc(t->count * sizeof(t->nameRecord[0]));
  if (!t->nameRecord) err(EX_OSERR, NULL);
  for (j = 0; j < t->count; j++) {
    if (!read_2be(f, &t->nameRecord[j].platformID) ||
	!read_2be(f, &t->nameRecord[j].encodingID) ||
	!read_2be(f, &t->nameRecord[j].languageID) ||
	!read_2be(f, &t->nameRecord[j].nameID) ||
	!read_2be(f, &t->nameRecord[j].length) ||
	!read_2be(f, &t->nameRecord[j].offset)) return false;
  }
  switch (t->format) {
  case 0:
    t->langTagCount = 0;
    break;
  case 1:
    if (!read_2be(f, &t->langTagCount)) return false;
    t->langTagRecord = malloc(t->langTagCount * sizeof(t->langTagRecord[0]));
    if (!t->langTagRecord) err(EX_OSERR, NULL);
    for (j = 0; j < t->langTagCount; j++) {
      if (!read_2be(f, &t->langTagRecord[j].length) ||
	  !read_2be(f, &t->langTagRecord[j].offset)) return false;
    }
    break;
  default:
    return false;
  }
  /* TODO: read only names we actually need? */
  offset = ftell(f);
  t->names = malloc(t->count * sizeof(t->names[0]));
  if (!t->names) err(EX_OSERR, NULL);
  for (j = 0; j < t->count; j++) {
    if (fseek(f, offset + t->nameRecord[j].offset, SEEK_SET) != 0) return false;
    t->names[j] = malloc(t->nameRecord[j].length * sizeof(t->names[j][0]));
    if (!t->names[j]) err(EX_OSERR, NULL);
    for (k = 0; k < t->nameRecord[j].length; k++)
      if (!read_1(f, &t->names[j][k])) return false;
  }
  /* TODO: read lang tags */
  return true;
}


/* is_english -- check if NameRecord n holds an English name */
static bool is_english(Name_table table, unsigned short n)
{
  switch (table.nameRecord[n].platformID) {
  case 0:			/* Unicode */
    return false;
  case 1:			/* Macintosh */
    if (table.nameRecord[n].languageID >= 0x8000)
      errx(EX_DATAERR, "Unsupported Name Table format.");
    if (table.nameRecord[n].encodingID != 0)
      errx(EX_DATAERR, "Unsupported encoding in Name Table.");
    return table.nameRecord[n].languageID == 0;
  case 2:			/* ISO (deprecated) */
    return false;
  case 3:			/* Windows */
    if (table.nameRecord[n].languageID >= 0x8000)
      errx(EX_DATAERR, "Unsupported Name Table format.");
    if (table.nameRecord[n].encodingID != 1)
      errx(EX_DATAERR, "Unsupported encoding in Name Table.");
    return table.nameRecord[n].languageID == 0x0409;
  case 4:			/* Custom */
    return false;
  default:
    errx(EX_DATAERR, "Invalid PlatformID in Name Table.");
  }
}


/* find_name -- find the English name with given ID in the Name table */
static void find_name(Name_table table, unsigned short id,
		      unsigned short *size, unsigned char **name)
{
  unsigned short i = 0, j, k;

  while (i < table.count &&
	 (table.nameRecord[i].nameID != id || !is_english(table, i))) i++;
  if (i >= table.count) {
    *size = 0;			/* Not found */
  } else {
    switch (table.nameRecord[i].platformID) {
    case 1:			/* Macintosh */
      *size = 2 * table.nameRecord[i].length;
      *name = malloc(*size);
      if (!*name) err(EX_OSERR, NULL);
      for (j = 0, k = 0; j < table.nameRecord[i].length; j++) {
	(*name)[k++] = table.names[i][j];
	(*name)[k++] = '\0'; /* Simplistic Roman -> UTF-16LE */
      }
      break;
    case 3:			/* Windows */
      *size = table.nameRecord[i].length;
      *name = malloc(*size);
      for (j = 0; j < table.nameRecord[i].length; j++)
	(*name)[j] = table.names[i][j];
      break;
    default:
      assert(!"Cannot happen!");
    }
  }
}


/* read_OS2_table -- find and read the OS/2 table in an OpenType file */
static bool read_OS2_table(FILE *f, sfnt_offset_table sfnt, OS2_table *t)
{
  unsigned short i = 0;

  /* Find the OS/2 table in the sfnt table directory and seek to it */
  while (i < sfnt.numTables && memcmp(sfnt.tables[i].tag, "OS/2", 4)) i++;
  if (i >= sfnt.numTables) return false;
  if (fseek(f, sfnt.tables[i].offset, SEEK_SET) != 0) return false;

  /* Read the table */
  return read_2be(f, &t->version) &&
    t->version <= 0x0004 &&
    read_2be_signed(f, &t->xAvgCharWidth) &&
    read_2be(f, &t->usWeightClass) &&
    read_2be(f, &t->usWidthClass) &&
    read_2be(f, &t->fsType) &&
    read_2be_signed(f, &t->ySubscriptXSize) &&
    read_2be_signed(f, &t->ySubscriptYSize) &&
    read_2be_signed(f, &t->ySubscriptXOffset) &&
    read_2be_signed(f, &t->ySubscriptYOffset) &&
    read_2be_signed(f, &t->ySuperscriptXSize) &&
    read_2be_signed(f, &t->ySuperscriptYSize) &&
    read_2be_signed(f, &t->ySuperscriptXOffset) &&
    read_2be_signed(f, &t->ySuperscriptYOffset) &&
    read_2be_signed(f, &t->yStrikeoutSize) &&
    read_2be_signed(f, &t->yStrikeoutPosition) &&
    read_2be_signed(f, &t->sFamilyClass) &&
    fread(t->panose, 1, 10, f) == 10 &&
    read_4be(f, &t->ulUnicodeRange1) &&
    read_4be(f, &t->ulUnicodeRange2) &&
    read_4be(f, &t->ulUnicodeRange3) &&
    read_4be(f, &t->ulUnicodeRange4) &&
    fread(t->achVendID, 1, 4, f) == 4 &&
    read_2be(f, &t->fsSelection) &&
    read_2be(f, &t->usFirstCharIndex) &&
    read_2be(f, &t->usLastCharIndex) &&
    read_2be_signed(f, &t->sTypoAscender) &&
    read_2be_signed(f, &t->sTypoDescender) &&
    read_2be_signed(f, &t->sTypoLineGap) &&
    read_2be(f, &t->usWinAscent) &&
    read_2be(f, &t->usWinDescent) &&
    read_4be(f, &t->ulCodePageRange1) &&
    read_4be(f, &t->ulCodePageRange2) &&
    read_2be_signed(f, &t->sxHeight) &&
    read_2be_signed(f, &t->sCapHeight) &&
    read_2be(f, &t->usDefaultChar) &&
    read_2be(f, &t->usBreakChar) &&
    read_2be(f, &t->usMaxContext);
}


/* read_head_table -- find and read the head table in an OpenType file */
static bool read_head_table(FILE *f, sfnt_offset_table sfnt, Head_table *t)
{
  unsigned short i = 0;

  /* Find the OS/2 table in the sfnt table directory and seek to it */
  while (i < sfnt.numTables && memcmp(sfnt.tables[i].tag, "head", 4)) i++;
  if (i >= sfnt.numTables) return false;
  if (fseek(f, sfnt.tables[i].offset, SEEK_SET) != 0) return false;

  /* Read the table */
  return read_4be(f, &t->Table_version_number) &&
    read_4be(f, &t->fontRevision) &&
    read_4be(f, &t->checkSumAdjustment) &&
    read_4be(f, &t->magicNumber) &&
    read_2be(f, &t->flags) &&
    read_2be(f, &t->unitsPerEm) &&
    read_8be(f, &t->created) &&
    read_8be(f, &t->modified) &&
    read_2be_signed(f, &t->xMin) &&
    read_2be_signed(f, &t->yMin) &&
    read_2be_signed(f, &t->xMax) &&
    read_2be_signed(f, &t->yMax) &&
    read_2be(f, &t->macStyle) &&
    read_2be(f, &t->lowestRecPPEM) &&
    read_2be_signed(f, &t->fontDirectionHint) &&
    read_2be_signed(f, &t->indexToLocFormat) &&
    read_2be_signed(f, &t->glyphDataFormat);
}


/* read_some_opentype_data -- get data from a font that is needed for EOT */
static bool read_some_opentype_data(FILE *f, EOT_header *header)
{
  long size;
  sfnt_offset_table sfnt;
  OS2_table os2;
  Name_table name;
  Head_table head;

  if (!read_sfnt_header(f, &sfnt)) return false;
  if (!read_OS2_table(f, sfnt, &os2)) return false;
  if (!read_name_table(f, sfnt, &name)) return false;
  if (!read_head_table(f, sfnt, &head)) return false;

  memcpy(header->FontPANOSE, os2.panose, 10);
  header->Italic = os2.fsSelection & 0x01;
  header->Weight = os2.usWeightClass;
  header->fsType = os2.fsType;
  header->UnicodeRange1 = os2.ulUnicodeRange1;
  header->UnicodeRange2 = os2.ulUnicodeRange2;
  header->UnicodeRange3 = os2.ulUnicodeRange3;
  header->UnicodeRange4 = os2.ulUnicodeRange4;
  header->CodePageRange1 = os2.ulCodePageRange1;
  header->CodePageRange2 = os2.ulCodePageRange2;
  header->CheckSumAdjustment = head.checkSumAdjustment;
  find_name(name, 1, &header->FamilyNameSize, &header->FamilyName);
  header->EOTSize += header->FamilyNameSize;
  find_name(name, 2, &header->StyleNameSize, &header->StyleName);
  header->EOTSize += header->StyleNameSize;
  find_name(name, 5, &header->VersionNameSize, &header->VersionName);
  header->EOTSize += header->VersionNameSize;
  find_name(name, 4, &header->FullNameSize, &header->FullName);
  header->EOTSize += header->FullNameSize;

  if (fseek(f, 0, SEEK_END) != 0) return false;
  if ((size = ftell(f)) == -1) return false;
  header->FontDataSize = size;
  header->EOTSize += header->FontDataSize;
  return true;
}


/* get_byte_checksum -- calculate checksum over rootstrings */
static unsigned long get_byte_checksum(unsigned char* s, unsigned short len)
{
  int i;
  unsigned long sum;

  for (i = 0, sum = 0; i < len; i++) sum += s[i];
  return sum ^ 0x50475342;
}


/* add_rootstring -- add a URL to the EOT header */
static void add_rootstring(char *url, EOT_header *h)
{
  unsigned long j, len;

  assert(url);
  len = 2 * strlen(url) + 2;
  if (!(h->RootString = realloc(h->RootString, h->RootStringSize + len)))
    err(EX_OSERR, NULL);

  j = h->RootStringSize;
  while (*url) {		/* Simplistic conversion to UTF-16LE */
    h->RootString[j++] = *url;
    h->RootString[j++] = '\0';
    url++;
  }
  h->RootString[j++] = '\0';
  h->RootString[j++] = '\0';
  h->RootStringSize += len;
  h->RootStringCheckSum = get_byte_checksum(h->RootString, h->RootStringSize);
  h->EOTSize += len;
}


/* usage -- print usage message and exit */
static void usage(char *progname)
{
  fprintf(stderr, "%s OTF-file [URL [URL...]]\n", progname);
  exit(1);
}


int main(int argc, char *argv[])
{
  FILE *f;
  EOT_header header;
  int i, c;

  if (argc < 2 || argv[1][0] == '-') usage(argv[0]);
  if (!(f = fopen(argv[1], "r"))) err(EX_DATAERR, "%s", argv[1]);

  /* Fill the EOT header with data from the font and with URLs */
  initialize_EOT_header(&header);
  if (!read_some_opentype_data(f, &header))
    errx(EX_DATAERR, "Could not read font file %s.", argv[1]);
  for (i = 2; i < argc; i++) add_rootstring(argv[i], &header);

  /* Write the EOT file, first write the header, then copy the font file */
  if (!(header.fsType & FSTYPE_EDITABLE) &&
      !(header.fsType & FSTYPE_PREVIEW) &&
      header.fsType & FSTYPE_RESTRICTED)
    errx(EX_DATAERR, "%s does not allow embedding.", argv[1]);
  if (header.fsType & FSTYPE_BITMAP)
    errx(EX_DATAERR, "Unsupported (%s requires bitmap embedding).", argv[1]);
  if (!write_EOT_header(stdout, header))
    err(EX_IOERR, "Could not write EOT file");

  rewind(f);
  while ((c = getc(f)) != EOF)
    if (putchar(c) == EOF) err(EX_IOERR, NULL);
  if (ferror(f)) err(EX_IOERR, "%s", argv[1]);
  if (fclose(f) != 0) err(EX_IOERR, "%s", argv[1]);

  return 0;
}
