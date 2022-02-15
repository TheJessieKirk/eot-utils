/* eotinfo -- print some info from an EOT file
 *
 * Displays some of the information in an EOT file in a human-readable way.
 *
 * TODO: add the missing code page names.
 * TODO: properly convert UTF-16LE to current locale instead of to UTF-8.
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

#define TTEMBED_SUBSET				0x00000001
#define TTEMBED_TTCOMPRESSED			0x00000004
#define TTEMBED_FAILIFVARIATIONSIMULATED	0x00000010
#define TTEMBED_EMBEDEUDC			0x00000020
#define TTEMBED_VALIDATIONTESTS			0x00000040
#define TTEMBED_WEBOBJECT			0x00000080
#define TTEMBED_XORENCRYPTDATA			0x10000000

#define FSTYPE_RESTRICTED	0x0002
#define FSTYPE_PREVIEW		0x0004
#define FSTYPE_EDITABLE		0x0008
#define FSTYPE_NOSUBSETTING	0x0100
#define FSTYPE_BITMAP		0x0200

typedef struct _EOT_Header {
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
  unsigned char *FontData;
} EOT_Header;


/* read_ulong -- read a little endian unsigned long */
static bool read_ulong(FILE *f, unsigned long *x)
{
  unsigned char s[4];

  if (fread(s, 1, 4, f) != 4) return false;
  *x = (s[3] << 24) + (s[2] << 16) + (s[1] << 8) + s[0];
  return true;
}


/* read_ushort -- read a little endian unsigned short */
static bool read_ushort(FILE *f, unsigned short *x)
{
  unsigned char s[2];

  if (fread(s, 1, 2, f) != 2) return false;
  *x = (s[1] << 8) + s[0];
  return true;
}


/* read_EOT_header -- read the header part of an EOT file */
static bool read_EOT_header(FILE *f, EOT_Header *h)
{
  if (!read_ulong(f, &(h->EOTSize))) return false;
  if (!read_ulong(f, &(h->FontDataSize))) return false;
  if (!read_ulong(f, &(h->Version))) return false;
  if (!read_ulong(f, &(h->Flags))) return false;
  if (fread(h->FontPANOSE, 1, 10, f) != 10) return false;
  if (fread(&(h->Charset), 1, 1, f) != 1) return false;
  if (fread(&(h->Italic), 1, 1, f) != 1) return false;
  if (!read_ulong(f, &(h->Weight))) return false;
  if (!read_ushort(f, &(h->fsType))) return false;
  if (!read_ushort(f, &(h->MagicNumber))) return false;
  if (h->MagicNumber != 0x504C) return false;
  if (!read_ulong(f, &(h->UnicodeRange1))) return false;
  if (!read_ulong(f, &(h->UnicodeRange2))) return false;
  if (!read_ulong(f, &(h->UnicodeRange3))) return false;
  if (!read_ulong(f, &(h->UnicodeRange4))) return false;
  if (!read_ulong(f, &(h->CodePageRange1))) return false;
  if (!read_ulong(f, &(h->CodePageRange2))) return false;
  if (!read_ulong(f, &(h->CheckSumAdjustment))) return false;
  if (!read_ulong(f, &(h->Reserved1))) return false;
  if (h->Reserved1 != 0) return false;
  if (!read_ulong(f, &(h->Reserved2))) return false;
  if (h->Reserved2 != 0) return false;
  if (!read_ulong(f, &(h->Reserved3))) return false;
  if (h->Reserved3 != 0) return false;
  if (!read_ulong(f, &(h->Reserved4))) return false;
  if (h->Reserved4 != 0) return false;
  if (!read_ushort(f, &(h->Padding1))) return false;
  if (h->Padding1 != 0) return false;
  if (!read_ushort(f, &(h->FamilyNameSize))) return false;
  if (!(h->FamilyName = malloc(h->FamilyNameSize))) err(EX_OSERR, NULL);
  if (fread(h->FamilyName, 1, h->FamilyNameSize, f) != h->FamilyNameSize) return false;
  if (!read_ushort(f, &(h->Padding2))) return false;
  if (h->Padding2 != 0) return false;
  if (!read_ushort(f, &(h->StyleNameSize))) return false;
  if (!(h->StyleName = malloc(h->StyleNameSize))) err(EX_OSERR, NULL);
  if (fread(h->StyleName, 1, h->StyleNameSize, f) != h->StyleNameSize) return false;
  if (!read_ushort(f, &(h->Padding3))) return false;
  if (h->Padding3 != 0) return false;
  if (!read_ushort(f, &(h->VersionNameSize))) return false;
  if (!(h->VersionName = malloc(h->VersionNameSize))) err(EX_OSERR, NULL);
  if (fread(h->VersionName, 1, h->VersionNameSize, f) != h->VersionNameSize) return false;
  if (!read_ushort(f, &(h->Padding4))) return false;
  if (h->Padding4 != 0) return false;
  if (!read_ushort(f, &(h->FullNameSize))) return false;
  if (!(h->FullName = malloc(h->FullNameSize))) err(EX_OSERR, NULL);
  if (fread(h->FullName, 1, h->FullNameSize, f) != h->FullNameSize) return false;

  switch (h->Version) {
  case 0x0001000:
    break;
  case 0x00020001:
    if (!read_ushort(f, &(h->Padding5))) return false;
    if (h->Padding5 != 0) return false;
    if (!read_ushort(f, &(h->RootStringSize))) return false;
    if (!(h->RootString = malloc(h->RootStringSize))) err(EX_OSERR, NULL);
    if (fread(h->RootString, 1, h->RootStringSize, f) != h->RootStringSize) return false;
    break;
  case 0x00020002:
    if (!read_ushort(f, &(h->Padding5))) return false;
    if (h->Padding5 != 0) return false;
    if (!read_ushort(f, &(h->RootStringSize))) return false;
    if (!(h->RootString = malloc(h->RootStringSize))) err(EX_OSERR, NULL);
    if (fread(h->RootString, 1, h->RootStringSize, f) != h->RootStringSize) return false;
    break;
    if (!read_ulong(f, &(h->RootStringCheckSum))) return false;
    if (!read_ulong(f, &(h->EUDCCodePage))) return false;
    if (!read_ushort(f, &(h->Padding6))) return false;
    if (h->Padding6 != 0) return false;
    if (!read_ushort(f, &(h->SignatureSize))) return false;
    if (!(h->Signature = malloc(h->SignatureSize))) err(EX_OSERR, NULL);
    if (fread(h->Signature, 1, h->SignatureSize, f) != h->SignatureSize) return false;
    if (!read_ulong(f, &(h->EUDCFlags))) return false;
    if (!read_ulong(f, &(h->EUDCFontSize))) return false;
    if (!(h->EUDCFontData = malloc(h->EUDCFontSize))) err(EX_OSERR, NULL);
    if (fread(h->EUDCFontData, 1, h->EUDCFontSize, f) != h->EUDCFontSize) return false;
    break;
  default:
    return false;
  }
  return true;
}


/* print_unicode_range -- print keywords for all bits in the Unicode range */
static void print_unicode_range(EOT_Header h)
{
  if (h.UnicodeRange1 & 0x00000001) printf(" basic-latin");
  if (h.UnicodeRange1 & 0x00000002) printf(" latin-1-supplement");
  if (h.UnicodeRange1 & 0x00000004) printf(" latin-extended-a");
  if (h.UnicodeRange1 & 0x00000008) printf(" latin-extended-b");
  if (h.UnicodeRange1 & 0x00000010) printf(" ipa-extensions");
  if (h.UnicodeRange1 & 0x00000020) printf(" spacing-modifiers");
  if (h.UnicodeRange1 & 0x00000040) printf(" combining-diacritical");
  if (h.UnicodeRange1 & 0x00000080) printf(" greek-and-coptic");
  if (h.UnicodeRange1 & 0x00000100) printf(" coptic");
  if (h.UnicodeRange1 & 0x00000200) printf(" cyrillic");
  if (h.UnicodeRange1 & 0x00000400) printf(" armenian");
  if (h.UnicodeRange1 & 0x00000800) printf(" hebrew");
  if (h.UnicodeRange1 & 0x00001000) printf(" vai");
  if (h.UnicodeRange1 & 0x00002000) printf(" arabic");
  if (h.UnicodeRange1 & 0x00004000) printf(" nko");
  if (h.UnicodeRange1 & 0x00008000) printf(" devanagari");
  if (h.UnicodeRange1 & 0x00010000) printf(" bengali");
  if (h.UnicodeRange1 & 0x00020000) printf(" gurmukhi");
  if (h.UnicodeRange1 & 0x00040000) printf(" gujarati");
  if (h.UnicodeRange1 & 0x00080000) printf(" oriya");
  if (h.UnicodeRange1 & 0x00100000) printf(" tamil");
  if (h.UnicodeRange1 & 0x00200000) printf(" teluga");
  if (h.UnicodeRange1 & 0x00400000) printf(" kannada");
  if (h.UnicodeRange1 & 0x00800000) printf(" malayalam");
  if (h.UnicodeRange1 & 0x01000000) printf(" thai");
  if (h.UnicodeRange1 & 0x02000000) printf(" lao");
  if (h.UnicodeRange1 & 0x04000000) printf(" georgian");
  if (h.UnicodeRange1 & 0x08000000) printf(" balinese");
  if (h.UnicodeRange1 & 0x10000000) printf(" hangul-jamo");
  if (h.UnicodeRange1 & 0x20000000) printf(" latin-extended-additional");
  if (h.UnicodeRange1 & 0x40000000) printf(" greek-extended");
  if (h.UnicodeRange1 & 0x80000000) printf(" general-punctuation");
  if (h.UnicodeRange2 & 0x00000001) printf(" super-and-subscripts");
  if (h.UnicodeRange2 & 0x00000002) printf(" currency");
  if (h.UnicodeRange2 & 0x00000004) printf(" combining-diacriticals-for-symbols");
  if (h.UnicodeRange2 & 0x00000008) printf(" letterlike");
  if (h.UnicodeRange2 & 0x00000010) printf(" number-forms");
  if (h.UnicodeRange2 & 0x00000020) printf(" arrows");
  if (h.UnicodeRange2 & 0x00000040) printf(" mathematical");
  if (h.UnicodeRange2 & 0x00000080) printf(" technical");
  if (h.UnicodeRange2 & 0x00000100) printf(" control-pictures");
  if (h.UnicodeRange2 & 0x00000200) printf(" ocr");
  if (h.UnicodeRange2 & 0x00000400) printf(" enclosed-alphanumerics");
  if (h.UnicodeRange2 & 0x00000800) printf(" box-drawing");
  if (h.UnicodeRange2 & 0x00001000) printf(" block-elements");
  if (h.UnicodeRange2 & 0x00002000) printf(" geometric-shapes");
  if (h.UnicodeRange2 & 0x00004000) printf(" misc-symbols");
  if (h.UnicodeRange2 & 0x00008000) printf(" dingbats");
  if (h.UnicodeRange2 & 0x00010000) printf(" cjk-symbols-and-punctuation");
  if (h.UnicodeRange2 & 0x00020000) printf(" hiragana");
  if (h.UnicodeRange2 & 0x00040000) printf(" katakana");
  if (h.UnicodeRange2 & 0x00080000) printf(" bopomofo");
  if (h.UnicodeRange2 & 0x00100000) printf(" hangul-compatibility-jamo");
  if (h.UnicodeRange2 & 0x00200000) printf(" phags-pa");
  if (h.UnicodeRange2 & 0x00400000) printf(" enclosed-cjk");
  if (h.UnicodeRange2 & 0x00800000) printf(" cjk-compatibility");
  if (h.UnicodeRange2 & 0x01000000) printf(" hangul-syllables");
  if (h.UnicodeRange2 & 0x02000000) printf(" non-plane-0");
  if (h.UnicodeRange2 & 0x04000000) printf(" phoenician");
  if (h.UnicodeRange2 & 0x08000000) printf(" cjk");
  if (h.UnicodeRange2 & 0x10000000) printf(" private-use-0");
  if (h.UnicodeRange2 & 0x20000000) printf(" cjk-strokes");
  if (h.UnicodeRange2 & 0x40000000) printf(" alphabetic-presentation-forms");
  if (h.UnicodeRange2 & 0x80000000) printf(" arabic-presentation-a");
  if (h.UnicodeRange3 & 0x00000001) printf(" combining-half-marks");
  if (h.UnicodeRange3 & 0x00000002) printf(" vertical-forms");
  if (h.UnicodeRange3 & 0x00000004) printf(" small-form-variants");
  if (h.UnicodeRange3 & 0x00000008) printf(" arabic-presentation-b");
  if (h.UnicodeRange3 & 0x00000010) printf(" halfwidth-andfullwidth-forms");
  if (h.UnicodeRange3 & 0x00000020) printf(" specials");
  if (h.UnicodeRange3 & 0x00000040) printf(" tibetan");
  if (h.UnicodeRange3 & 0x00000080) printf(" syriac");
  if (h.UnicodeRange3 & 0x00000100) printf(" thaana");
  if (h.UnicodeRange3 & 0x00000200) printf(" sinhala");
  if (h.UnicodeRange3 & 0x00000400) printf(" myanmar");
  if (h.UnicodeRange3 & 0x00000800) printf(" ethiopic");
  if (h.UnicodeRange3 & 0x00001000) printf(" cherokee");
  if (h.UnicodeRange3 & 0x00002000) printf(" unified-canadian-aboriginal");
  if (h.UnicodeRange3 & 0x00004000) printf(" ogham");
  if (h.UnicodeRange3 & 0x00008000) printf(" runic");
  if (h.UnicodeRange3 & 0x00010000) printf(" khmer");
  if (h.UnicodeRange3 & 0x00020000) printf(" mongolian");
  if (h.UnicodeRange3 & 0x00040000) printf(" braille");
  if (h.UnicodeRange3 & 0x00080000) printf(" yi");
  if (h.UnicodeRange3 & 0x00100000) printf(" tagalog-hanunoo-buhid-tagbanwa");
  if (h.UnicodeRange3 & 0x00200000) printf(" old-italic");
  if (h.UnicodeRange3 & 0x00400000) printf(" gothic");
  if (h.UnicodeRange3 & 0x00800000) printf(" deseret");
  if (h.UnicodeRange3 & 0x01000000) printf(" musical-symbols");
  if (h.UnicodeRange3 & 0x02000000) printf(" mathematical alphanumeric symbols");
  if (h.UnicodeRange3 & 0x04000000) printf(" private-use-15-16");
  if (h.UnicodeRange3 & 0x08000000) printf(" variation-selectors");
  if (h.UnicodeRange3 & 0x10000000) printf(" tags");
  if (h.UnicodeRange3 & 0x20000000) printf(" limbu");
  if (h.UnicodeRange3 & 0x40000000) printf(" taile");
  if (h.UnicodeRange3 & 0x80000000) printf(" new-tai-lue");
  if (h.UnicodeRange4 & 0x00000001) printf(" buginese");
  if (h.UnicodeRange4 & 0x00000002) printf(" glagolitic");
  if (h.UnicodeRange4 & 0x00000004) printf(" tifinagh");
  if (h.UnicodeRange4 & 0x00000008) printf(" yijing-hexagram");
  if (h.UnicodeRange4 & 0x00000010) printf(" syloti-nagri");
  if (h.UnicodeRange4 & 0x00000020) printf(" linear-b");
  if (h.UnicodeRange4 & 0x00000040) printf(" ancient-greek-numbers");
  if (h.UnicodeRange4 & 0x00000080) printf(" ugaritic");
  if (h.UnicodeRange4 & 0x00000100) printf(" old-persian");
  if (h.UnicodeRange4 & 0x00000200) printf(" shavian");
  if (h.UnicodeRange4 & 0x00000400) printf(" osmanya");
  if (h.UnicodeRange4 & 0x00000800) printf(" cypriot");
  if (h.UnicodeRange4 & 0x00001000) printf(" kharoshthi");
  if (h.UnicodeRange4 & 0x00002000) printf(" tai-xuan-jing");
  if (h.UnicodeRange4 & 0x00004000) printf(" cuneiform");
  if (h.UnicodeRange4 & 0x00008000) printf(" counting-rod-numerals");
  if (h.UnicodeRange4 & 0x00010000) printf(" sundanese");
  if (h.UnicodeRange4 & 0x00020000) printf(" lepcha");
  if (h.UnicodeRange4 & 0x00040000) printf(" ol-chiki");
  if (h.UnicodeRange4 & 0x00080000) printf(" saurashtra");
  if (h.UnicodeRange4 & 0x00100000) printf(" kayah-li");
  if (h.UnicodeRange4 & 0x00200000) printf(" rejang");
  if (h.UnicodeRange4 & 0x00400000) printf(" cham");
  if (h.UnicodeRange4 & 0x00800000) printf(" ancient-symbols");
  if (h.UnicodeRange4 & 0x01000000) printf(" phaistos-disc");
  if (h.UnicodeRange4 & 0x02000000) printf(" carian-lycian-lydian");
  if (h.UnicodeRange4 & 0x04000000) printf(" domino-mahjong");
}


/* print_code_page_range -- print codepages supported by the font */
static void print_code_page_range(EOT_Header h)
{
  printf(" to do...");
}


/* putUTF8 -- write a character to stdout in UTF8 encoding */
static void putUTF8(long c)
{
  if (c <= 0x7F) {				/* Leave ASCII encoded */
    putchar(c);
  } else if (c <= 0x07FF) {			/* 110xxxxx 10xxxxxx */
    putchar(0xC0 | (c >> 6));
    putchar(0x80 | (c & 0x3F));
  } else if (c <= 0xFFFF) {			/* 1110xxxx + 2 */
    putchar(0xE0 | (c >> 12));
    putchar(0x80 | ((c >> 6) & 0x3F));
    putchar(0x80 | (c & 0x3F));
  } else if (c <= 0x1FFFFF) {			/* 11110xxx + 3 */
    putchar(0xF0 | (c >> 18));
    putchar(0x80 | ((c >> 12) & 0x3F));
    putchar(0x80 | ((c >> 6) & 0x3F));
    putchar(0x80 | (c & 0x3F));
  } else if (c <= 0x3FFFFFF) {			/* 111110xx + 4 */
    putchar(0xF8 | (c >> 24));
    putchar(0x80 | ((c >> 18) & 0x3F));
    putchar(0x80 | ((c >> 12) & 0x3F));
    putchar(0x80 | ((c >> 6) & 0x3F));
    putchar(0x80 | (c & 0x3F));
  } else if (c <= 0x7FFFFFFF) {			/* 1111110x + 5 */
    putchar(0xFC | (c >> 30));
    putchar(0x80 | ((c >> 24) & 0x3F));
    putchar(0x80 | ((c >> 18) & 0x3F));
    putchar(0x80 | ((c >> 12) & 0x3F));
    putchar(0x80 | ((c >> 6) & 0x3F));
    putchar(0x80 | (c & 0x3F));
  } else {					/* Not a valid character... */
    printf("<%ld>", c);
  } 
}


/* dump_header -- print out the header in a readable way */
static void dump_header(EOT_Header h)
{
  unsigned short i;
  long c;

  printf("EOTSize:            %ld\n", h.EOTSize);
  printf("FontDataSize:       %ld\n", h.FontDataSize);
  printf("Version:            0x%08lX\n", h.Version);
  printf("Flags:              %s %s %s %s\n",
	 (h.Version & TTEMBED_SUBSET) ? "subsetted" : "not-subsetted",
	 (h.Version & TTEMBED_TTCOMPRESSED) ? "compressed" : "not-compressed",
	 (h.Version & TTEMBED_EMBEDEUDC) ? "EUDC": "no-EUDC",
	 (h.Version & TTEMBED_XORENCRYPTDATA) ? "xor" : "no-xor");
  printf("PANOSE:             %u %u %u %u %u %u %u %u %u %u\n", h.FontPANOSE[0],
	 h.FontPANOSE[1], h.FontPANOSE[2], h.FontPANOSE[3], h.FontPANOSE[4],
	 h.FontPANOSE[5], h.FontPANOSE[6], h.FontPANOSE[7], h.FontPANOSE[8],
	 h.FontPANOSE[9]);
  printf("Charset:            %u\n", h.Charset);
  printf("Italic:             %s\n", h.Italic ? "yes" : "no");
  printf("Weight:             %lu\n", h.Weight);
  printf("fsType:            ");
  if (h.fsType == 0) printf(" installable");
  else if (h.fsType & FSTYPE_EDITABLE) printf(" editable");
  else if (h.fsType & FSTYPE_PREVIEW) printf(" preview-and-print");
  else if (h.fsType & FSTYPE_RESTRICTED) printf(" restricted");
  if (h.fsType & FSTYPE_NOSUBSETTING) printf(" no-subsetting");
  if (h.fsType & FSTYPE_BITMAP) printf(" bitmap-only");
  printf("\n");
  printf("UnicodeRange:      "); print_unicode_range(h); printf("\n");
  printf("CodePageRange:     "); print_code_page_range(h); printf("\n");
  printf("CheckSumAdjustment: %lu\n", h.CheckSumAdjustment);
  printf("FamilyName:         ");
  for (i = 0; i < h.FamilyNameSize; i += 2) /* UTF-16LE -> UTF-8 */
    if ((c = h.FamilyName[i] + 256 * h.FamilyName[i+1])) putUTF8(c);
  printf("\n");
  printf("StyleName:          ");
  for (i = 0; i < h.StyleNameSize; i += 2) /* UTF-16LE -> UTF-8 */
    if ((c = h.StyleName[i] + 256 * h.StyleName[i+1])) putUTF8(c);
  printf("\n");
  printf("VersionName:        ");
  for (i = 0; i < h.VersionNameSize; i += 2) /* UTF-16LE -> UTF-8 */
    if ((c = h.VersionName[i] + 256 * h.VersionName[i+1])) putUTF8(c);
  printf("\n");
  printf("FullName:           ");
  for (i = 0; i < h.FullNameSize; i += 2) /* UTF-16LE -> UTF-8 */
    if ((c = h.FullName[i] + 256 * h.FullName[i+1])) putUTF8(c);
  printf("\n");
  printf("RootString:         ");
  for (i = 0; i < h.RootStringSize; i += 2) /* UTF-16LE -> UTF-8 */
    if ((c = h.RootString[i] + 256 * h.RootString[i+1])) putUTF8(c);
    else putUTF8(' ');
  printf("\n");
}


/* usage -- print usage message and exit */
static void usage(char *progname)
{
  fprintf(stderr, "%s EOT-file\n", progname);
  exit(1);
}


int main(int argc, char *argv[])
{
  FILE *f;
  EOT_Header header;

  switch (argc) {
  case 1: f = stdin; break;
  case 2: if (! (f = fopen(argv[1], "r"))) err(EX_NOINPUT,"%s",argv[1]); break;
  default: usage(argv[0]);
  }
  if (! read_EOT_header(f, &header)) errx(1, "Unrecognized EOT header");
  dump_header(header);
  return 0;
}
