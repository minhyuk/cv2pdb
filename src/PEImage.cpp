// Convert DMD CodeView debug information to PDB files
// Copyright (c) 2009-2010 by Rainer Schuetze, All Rights Reserved
//
// License for redistribution is given by the Artistic License 2.0
// see file LICENSE for further details

#include "PEImage.h"

extern "C" {
#include "mscvpdb.h"
}

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <ctype.h>
#include <direct.h>
#include <share.h>
#include <sys/stat.h>
#include <vector>

#ifdef UNICODE
#define T_sopen	_wsopen
#define T_open	_wopen
#else
#define T_sopen	sopen
#define T_open	open
#endif

///////////////////////////////////////////////////////////////////////
/**
* Constructor for PEImage class. Loads the specified executable file.
*
* @param iname The path to the executable file to load.
*/
PEImage::PEImage(const TCHAR* iname)
: dump_base(0)
, dump_total_len(0)
, dirHeader(0)
, hdr32(0)
, hdr64(0)
, fd(-1)
, debug_aranges(0)
, debug_pubnames(0)
, debug_pubtypes(0)
, debug_info(0), debug_info_length(0)
, debug_abbrev(0), debug_abbrev_length(0)
, debug_line(0), debug_line_length(0)
, debug_frame(0), debug_frame_length(0)
, debug_str(0)
, debug_loc(0), debug_loc_length(0)
, debug_ranges(0), debug_ranges_length(0)
, codeSegment(0)
, linesSegment(-1)
, reloc(0), reloc_length(0)
, nsec(0)
, nsym(0)
, symtable(0)
, strtable(0)
, bigobj(false)
{
	if(iname)
		loadExe(iname);
}

///Destructor for PEImage class
///Closes the file descriptor and frees the memory allocated for the dump base if they were allocated during object creation.
PEImage::~PEImage()
{
    ///Close the file descriptor if it was not -1
    if(fd != -1)
        close(fd);
    ///Free the memory allocated for the dump base if it was allocated
    if(dump_base)
        free_aligned(dump_base);
}

///////////////////////////////////////////////////////////////////////
bool PEImage::readAll(const TCHAR* iname)
//------------------------- Read all data from a file and store it in memory
{
    if (fd != -1)
        // Check if the file is already open
        return setError("file already open");

    fd = T_sopen(iname, O_RDONLY | O_BINARY, SH_DENYWR);
    // Open the file in binary read mode
    if (fd == -1)
        // Error opening the file
        return setError("Can't open fileirut");

    struct stat s;
    if (fstat(fd, &s) < 0)
        // Get the file size
        return setError("Can't get size");
    dump_total_len = s.st_size;

    dump_base = alloc_aligned(dump_total_len, 0x1000);
    // Allocate memory for the file data
    if (!dump_base)
        // Error allocating memory
        return setError("Out of memory");
    if (read(fd, dump_base, dump_total_len) != dump_total_len)
        // Read the file data
        return setError("Cannot read file");

    close(fd);
    fd = -1;
    return true;
}

///////////////////////////////////////////////////////////////////////
bool PEImage::loadExe(const TCHAR* iname)
{
    /// Loads an executable file and initializes CV and DWARF pointers.
    if (!readAll(iname))
        return false;

    return initCVPtr(true) || initDWARFPtr(true);

///////////////////////////////////////////////////////////////////////
/**
 * Loads the PE image from the specified file.
 *
 * @param iname the name of the file to load
 * @return true if the loading was successful, false otherwise
 */
bool PEImage::loadObj(const TCHAR* iname)
{
    if (!readAll(iname))
        return false;

    return initDWARFObject();
}

///////////////////////////////////////////////////////////////////////
bool PEImage::save(const TCHAR* oname)
{
	if (fd != -1)
		return setError("file already open");

	if (!dump_base)
		return setError("no data to dump");

	fd = T_open(oname, O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, S_IREAD | S_IWRITE | S_IEXEC);
	if (fd == -1)
		return setError("Can't create file");

	if (write(fd, dump_base, dump_total_len) != dump_total_len)
		return setError("Cannot write file");

	close(fd);
	fd = -1;
	return true;
}

///////////////////////////////////////////////////////////////////////
/**
 * Replace the debug section in the PE image with new debug data.
 * @param data    The new debug data
 * @param datalen The length of the new debug data
 * @param initCV  Whether to initialize the debug directory
 * @return        Whether the operation is successful
 */
bool PEImage::replaceDebugSection (const void* data, int datalen, bool initCV)
{
    // ...

    // append new debug directory to data
    IMAGE_DEBUG_DIRECTORY debugdir;
    if(dbgDir)
        debugdir = *dbgDir;
    else
        memset(&debugdir, 0, sizeof(debugdir));
    int xdatalen = datalen + sizeof(debugdir);

    // ...

    // assume there is place for another section because of section alignment
    int s;
    DWORD lastVirtualAddress = 0;
    int firstDWARFsection = -1;
    int cntSections = countSections();
    for(s = 0; s < cntSections; s++)
    {
        // ...

        if (strcmp (name, ".debug") == 0)
        {
            // ...

        }
        lastVirtualAddress = sec[s].VirtualAddress + sec[s].Misc.VirtualSize;
    }

    // ...

    // append debug data chunk to existing file image
    memcpy(newdata, dump_base, dump_total_len);
    memset(newdata + dump_total_len, 0, fill);
    memcpy(newdata + dump_total_len + fill, data, datalen);

    // ...

    // update debug directory information
    if(!dbgDir)
    {
        debugdir.Type = 2;
    }
    dbgDir = (IMAGE_DEBUG_DIRECTORY*) (newdata + dump_total_len + fill + datalen);
    memcpy(dbgDir, &debugdir, sizeof(debugdir));

    // ...

    free_aligned(dump_base);
    dump_base = newdata;
    dump_total_len += fill + xdatalen;

    return !initCV || initCVPtr(false);
}

///////////////////////////////////////////////////////////////////////
bool PEImage::initCVPtr(bool initDbgDir)
{
	dos = DPV<IMAGE_DOS_HEADER> (0);
	if(!dos)
		return setError("file too small for DOS header");
	if(dos->e_magic != IMAGE_DOS_SIGNATURE)
		return setError("this is not a DOS executable");

	hdr32 = DPV<IMAGE_NT_HEADERS32> (dos->e_lfanew);
	hdr64 = DPV<IMAGE_NT_HEADERS64> (dos->e_lfanew);
	if(!hdr32)
		return setError("no optional header found");
	if(hdr32->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ||
	   hdr32->FileHeader.Machine == IMAGE_FILE_MACHINE_IA64)
		hdr32 = 0;
	else
		hdr64 = 0;

	if(IMGHDR(Signature) != IMAGE_NT_SIGNATURE)
		return setError("optional header does not have PE signature");
	if(IMGHDR(FileHeader.SizeOfOptionalHeader) < sizeof(IMAGE_OPTIONAL_HEADER32))
		return setError("optional header too small");

	sec = hdr32 ? IMAGE_FIRST_SECTION(hdr32) : IMAGE_FIRST_SECTION(hdr64);
    nsec = IMGHDR(FileHeader.NumberOfSections);

    symtable = DPV<char>(IMGHDR(FileHeader.PointerToSymbolTable));
    nsym = IMGHDR(FileHeader.NumberOfSymbols);
	strtable = symtable + nsym * IMAGE_SIZEOF_SYMBOL;

	if(IMGHDR(OptionalHeader.NumberOfRvaAndSizes) <= IMAGE_DIRECTORY_ENTRY_DEBUG)
		return setError("too few entries in data directory");

	unsigned int i;
	int found = false;
	for(i = 0; i < IMGHDR(OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size)/sizeof(IMAGE_DEBUG_DIRECTORY); i++)
	{
		int off = IMGHDR(OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress) + i*sizeof(IMAGE_DEBUG_DIRECTORY);
		dbgDir = RVA<IMAGE_DEBUG_DIRECTORY>(off, sizeof(IMAGE_DEBUG_DIRECTORY));
		if (!dbgDir)
			continue; //return setError("debug directory not placed in image");
		if (dbgDir->Type != IMAGE_DEBUG_TYPE_CODEVIEW)
			continue; //return setError("debug directory not of type CodeView");

		cv_base = dbgDir->PointerToRawData;
		OMFSignature* sig = DPV<OMFSignature>(cv_base, dbgDir->SizeOfData);
		if (!sig)
			return setError("invalid debug data base address and size");
		if (memcmp(sig->Signature, "NB09", 4) != 0 && memcmp(sig->Signature, "NB11", 4) != 0)
		{
			// return setError("can only handle debug info of type NB09 and NB11");
			dirHeader = 0;
			dirEntry = 0;
			return true;
		}
		dirHeader = CVP<OMFDirHeader>(sig->filepos);
		if (!dirHeader)
			return setError("invalid CodeView dir header data base address");
		dirEntry = CVP<OMFDirEntry>(sig->filepos + dirHeader->cbDirHeader);
		if (!dirEntry)
			return setError("CodeView debug dir entries invalid");
		return true;
	}
	return setError("no CodeView debug info data found");
}

///////////////////////////////////////////////////////////////////////
/**
 * Initializes the PE image and sets up pointers to the DOS header, NT headers, 
 * and other important structures.
 *
 * @param initDbgDir ignored, this function always initializes the debug directory
 * @return true if initialization is successful, false otherwise
 */
bool PEImage::initDWARFPtr(bool initDbgDir)
{
	dos = DPV<IMAGE_DOS_HEADER> (0);
	if(!dos)
		return setError("file too small for DOS header");
	if(dos->e_magic != IMAGE_DOS_SIGNATURE)
		return setError("this is not a DOS executable");

	hdr32 = DPV<IMAGE_NT_HEADERS32> (dos->e_lfanew);
	hdr64 = DPV<IMAGE_NT_HEADERS64> (dos->e_lfanew);
	if(!hdr32)
		return setError("no optional header found");
	if(hdr32->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ||
	   hdr32->FileHeader.Machine == IMAGE_FILE_MACHINE_IA64)
		hdr32 = 0;
	else
		hdr64 = 0;

	if(IMGHDR(Signature) != IMAGE_NT_SIGNATURE)
		return setError("optional header does not have PE signature");
	if(IMGHDR(FileHeader.SizeOfOptionalHeader) < sizeof(IMAGE_OPTIONAL_HEADER32))
		return setError("optional header too small");

	dbgDir = 0;
	sec = hdr32 ? IMAGE_FIRST_SECTION(hdr32) : IMAGE_FIRST_SECTION(hdr64);
	symtable = DPV<char>(IMGHDR(FileHeader.PointerToSymbolTable));
    nsym = IMGHDR(FileHeader.NumberOfSymbols);
	strtable = symtable + nsym * IMAGE_SIZEOF_SYMBOL;
	initDWARFSegments();

	setError(0);
	return true;
}

`/**
 * Initializes the PEImage object, loading the DWARF object.
 *
 * @return true if initialization was successful, false otherwise
 */`
bool PEImage::initDWARFObject()
{
    IMAGE_FILE_HEADER* hdr = DPV<IMAGE_FILE_HEADER> (0);
    if(!dos)
        return setError("file too small for COFF header");

    if (hdr->Machine == IMAGE_FILE_MACHINE_UNKNOWN && hdr->NumberOfSections == 0xFFFF)
    {
        // Special handling for big object files
        static CLSID bigObjClSID = { 0xD1BAA1C7, 0xBAEE, 0x4ba9, { 0xAF, 0x20, 0xFA, 0xF6, 0x6A, 0xA4, 0xDC, 0xB8 } };
        ANON_OBJECT_HEADER_BIGOBJ* bighdr = DPV<ANON_OBJECT_HEADER_BIGOBJ> (0);
        if (!bighdr || bighdr->Version < 2 || bighdr->ClassID != bigObjClSID)
            return setError("invalid big object file COFF header");
        sec = (IMAGE_SECTION_HEADER*)((char*)(bighdr + 1) + bighdr->SizeOfData);
        nsec = bighdr->NumberOfSections;
        bigobj = true;
        symtable = DPV<char>(bighdr->PointerToSymbolTable);
        nsym = bighdr->NumberOfSymbols;
        strtable = symtable + nsym * sizeof(IMAGE_SYMBOL_EX);
    }
    else if (hdr->Machine != IMAGE_FILE_MACHINE_UNKNOWN)
    {
        // Regular PE file handling
        sec = (IMAGE_SECTION_HEADER*)(hdr + 1);
        nsec = hdr->NumberOfSections;
        bigobj = false;
        hdr32 = (IMAGE_NT_HEADERS32*)((char*)hdr - 4); // skip back over signature
        symtable = DPV<char>(IMGHDR(FileHeader.PointerToSymbolTable));
        nsym = IMGHDR(FileHeader.NumberOfSymbols);
        strtable = symtable + nsym * IMAGE_SIZEOF_SYMBOL;
    }
    else
        return setError("Unknown object file format");

    if (!symtable || !strtable)
        return setError("Unknown object file format");

    initDWARFSegments();
    setError(0);
    return true;
}

/// Returns the size of a section in an image
static DWORD sizeInImage(const IMAGE_SECTION_HEADER& sec)
{
    if (sec.Misc.VirtualSize == 0)
        return sec.SizeOfRawData; // for object files
    return sec.SizeOfRawData < sec.Misc.VirtualSize ? sec.SizeOfRawData : sec.Misc.VirtualSize;
}

"""
Initialize DWARF segments.
"""
void PEImage::initDWARFSegments()
{
 /**
  * Iterate through each section in the PE file.
  */
	for(int s = 0; s < nsec; s++)
	{
 /**
  * Get the name of the current section.
  */
		const char* name = (const char*) sec[s].Name;
 /**
  * If the name starts with '/', assume it's an offset to a string table.
  */
		if(name[0] == '/')
		{
			int off = strtol(name + 1, 0, 10);
			name = strtable + off;
		}
 /**
  * Check if the section is one of the standard DWARF segments.
  */
		if(strcmp(name, ".debug_aranges") == 0)
			debug_aranges = DPV<char>(sec[s].PointerToRawData, sizeInImage(sec[s]));
		if(strcmp(name, ".debug_pubnames") == 0)
			debug_pubnames = DPV<char>(sec[s].PointerToRawData, sizeInImage(sec[s]));
		if(strcmp(name, ".debug_pubtypes") == 0)
			debug_pubtypes = DPV<char>(sec[s].PointerToRawData, sizeInImage(sec[s]));
		if(strcmp(name, ".debug_info") == 0)
			debug_info = DPV<char>(sec[s].PointerToRawData, debug_info_length = sizeInImage(sec[s]));
		if(strcmp(name, ".debug_abbrev") == 0)
			debug_abbrev = DPV<char>(sec[s].PointerToRawData, debug_abbrev_length = sizeInImage(sec[s]));
		if(strcmp(name, ".debug_line") == 0)
			debug_line = DPV<char>(sec[linesSegment = s].PointerToRawData, debug_line_length = sizeInImage(sec[s]));
		if(strcmp(name, ".debug_frame") == 0)
			debug_frame = DPV<char>(sec[s].PointerToRawData, debug_frame_length = sizeInImage(sec[s]));
		if(strcmp(name, ".debug_str") == 0)
			debug_str = DPV<char>(sec[s].PointerToRawData, sizeInImage(sec[s]));
		if(strcmp(name, ".debug_loc") == 0)
			debug_loc = DPV<char>(sec[s].PointerToRawData, debug_loc_length = sizeInImage(sec[s]));
		if(strcmp(name, ".debug_ranges") == 0)
			debug_ranges = DPV<char>(sec[s].PointerToRawData, debug_ranges_length = sizeInImage(sec[s]));
		if(strcmp(name, ".reloc") == 0)
			reloc = DPV<char>(sec[s].PointerToRawData, reloc_length = sizeInImage(sec[s]));
	/**
  * If the section is the "text" segment, set the code segment.
  */
		if(strcmp(name, ".text") == 0)
			codeSegment = s;
	}
}
"""

/**
 * Relocate Debug Line Information
 * 
 * This function relocates the debug line information in the PE image based on the given relocation base.
 * 
 * @param img_base The base address of the relocation image.
 * 
 * @returns true if the relocation was successful, false otherwise.
 */
bool PEImage::relocateDebugLineInfo(unsigned int img_base)
{
    if(!reloc || !reloc_length)
        return true;

    char* relocbase = reloc;
    char* relocend = reloc + reloc_length;
    while(relocbase < relocend)
    {
        unsigned int virtadr = *(unsigned int *) relocbase;
        unsigned int chksize = *(unsigned int *) (relocbase + 4);

        char* p = RVA<char> (virtadr, 1);
        if(p >= debug_line && p < debug_line + debug_line_length)
        {
            for (unsigned int w = 8; w < chksize; w += 2)
            {
                unsigned short entry = *(unsigned short*)(relocbase + w);
                unsigned short type = (entry >> 12) & 0xf;
                unsigned short off = entry & 0xfff;

                if(type == 3) // HIGHLOW
                {
                    *(long*) (p + off) += img_base;
                }
            }
        }
        if(chksize == 0 || chksize >= reloc_length)
            break;
        relocbase += chksize;
    }
    return true;
}

int PEImage::getRelocationInLineSegment(unsigned int offset) const
///
/// Returns the relocation information for the given offset within a line segment.
///
/// @param offset The offset within a line segment for which to retrieve the relocation information
/// @return The relocation information for the given offset
///
return getRelocationInSegment(linesSegment, offset);

int PEImage::getRelocationInSegment(int segment, unsigned int offset) const
{
    if (segment < 0)
        return -1;

    int cnt = sec[segment].NumberOfRelocations;
    IMAGE_RELOCATION* rel = DPV<IMAGE_RELOCATION>(sec[segment].PointerToRelocations, cnt * sizeof(IMAGE_RELOCATION));
    if (!rel)
        return -1;

    for (int i = 0; i < cnt; i++)
        if (rel[i].VirtualAddress == offset)
        {
            if (bigobj)
            {
                IMAGE_SYMBOL_EX* sym = (IMAGE_SYMBOL_EX*)(symtable + rel[i].SymbolTableIndex * sizeof(IMAGE_SYMBOL_EX));
                if (!sym)
                    return -1;
                return sym->SectionNumber;
            }
            else
            {
                IMAGE_SYMBOL* sym = (IMAGE_SYMBOL*)(symtable + rel[i].SymbolTableIndex * IMAGE_SIZEOF_SYMBOL);
                if (!sym)
                    return -1;
                return sym->SectionNumber;
            }
        }

    return -1;
}

///////////////////////////////////////////////////////////////////////
struct LineInfoData
{
    int funcoff;
    int funcidx;
    int funcsiz;
    int srcfileoff;
    int npairs;
    int size;
};

struct LineInfoPair
{
    int offset;
    int line;
};

/**
 * Dumps COFF-style line info from a PE image.
 * 
 * This function traverses the sections of the PE image, looking for 
 * .debug$S sections. It then decodes the contents of these sections 
 * to extract line information, such as symbol names, file names, 
 * and line numbers. The information is printed to the console.
 */
{
    char* f3section = 0;
    char* f4section = 0;
    for(int s = 0; s < nsec; s++)
    {
        if (strncmp((char*)sec[s].Name, ".debug$S", 8) == 0)
        {
            DWORD* base = DPV<DWORD>(sec[s].PointerToRawData, sec[s].SizeOfRawData);
            if (!base || *base != 4)
                continue;
            DWORD* end = base + sec[s].SizeOfRawData / 4;
            for (DWORD* p = base + 1; p < end; p += (p[1] + 3) / 4 + 2)
            {
                if (!f4section && p[0] == 0xf4)
                    f4section = (char*)p + 8;
                if (!f3section && p[0] == 0xf3)
                    f3section = (char*)p + 8;
                if (p[0] != 0xf2)
                    continue;

                LineInfoData* info = (LineInfoData*) (p + 2);
                if (p[1] != info->size + 12)
                    continue;

                int* f3off = f4section ? (int*)(f4section + info->srcfileoff) : 0;
                const char* fname = f3off ? f3section + *f3off : "unknown";
                int section = getRelocationInSegment(s, (char*)info - (char*)base);
                const char* secname = findSectionSymbolName(section);
                printf("Sym: %s\n", secname ? secname : "<none>");
                printf("File: %s\n", fname);
                LineInfoPair* pairs = (LineInfoPair*)(info + 1);
                for (int i = 0; i < info->npairs; i++)
                    printf("\tOff 0x%x: Line %d\n", pairs[i].offset, pairs[i].line & 0x7fffffff);
            }
        }
    }
    return 0;
}

/*
** Returns the length of a null-terminated string stored in bytes
**
** @param p: pointer to a byte array
** @return: the length of the string
*/
int _pstrlen(const BYTE* &p)
{
    int len = *p++;
    // ...
    return len;
}

unsigned _getIndex(const BYTE* &p)
{
    if (*p & 0x80)
    {
        p += 2;
        return ((p[-2] << 8) | p[-1]) & 0x7fff;
    }
    return *p++;
}

/**************************************************************************
 * dumpDebugLineInfoOMF: Reads and prints debugging line information 
 * from the OMF (Object Module Format) PEImage.
 **************************************************************************/
int PEImage::dumpDebugLineInfoOMF()
{
    // Initialize vectors to hold file names and line names.
    std::vector<const unsigned char*> lnames;
    std::vector<const unsigned char*> llnames;
    // Get the file name from the initial THEADR record.
    const unsigned char* fname = 0;
    unsigned char* base = (unsigned char*) dump_base;
    if (*base != 0x80) // assume THEADR record
        return -1;
    unsigned char* end = base + dump_total_len;
    for(unsigned char* p = base; p < end; p += *(unsigned short*)(p + 1) + 3)
    {
        // Process each record in the file.
        switch(*p)
        {
        case 0x80: // THEADR
            fname = p + 3; // pascal string
            break;
        case 0x96: // LNAMES
        {
            // Read the length of the name and store it in the lnames vector.
            int len = *(unsigned short*)(p + 1);
            for(const unsigned char* q = p + 3; q < p + len + 2; q += _pstrlen (q)) 
                lnames.push_back(q);
            break;
        }
        case 0xCA: // LLNAMES
        {
            // Read the length of the name and store it in the llnames vector.
            int len = *(unsigned short*)(p + 1);
            for(const unsigned char* q = p + 3; q < p + len + 2; q += _pstrlen (q)) 
                llnames.push_back(q);
            break;
        }
        case 0x95: // LINNUM
        {
            // Read the number of line numbers and print the debugging information.
            const unsigned char* q = p + 3;
            int basegrp = _getIndex(q);
            int baseseg = _getIndex(q);
            unsigned num = (p + *(unsigned short*)(p + 1) + 2 - q) / 6;
            const unsigned char* fn = fname;
            int flen = fn ? _pstrlen(fn) : 0;
            printf("File: %.*s, BaseSegment %d\n", flen, fn, baseseg);
            for (int i = 0; i < num; i++)
                printf("\tOff 0x%x: Line %d\n", *(int*)(q + 2 + 6 * i), *(unsigned short*)(p + 6 * i));
            break;
        }
        case 0xc5: // LINSYM
        {
            // Read the line symbol information and print it.
            const unsigned char* q = p + 3;
            unsigned flags = *q++;
            unsigned pubname = _getIndex(q);
            unsigned num = (p + *(unsigned short*)(p + 1) + 2 - q) / 6;
            if (num == 0)
                break;
            const unsigned char* fn = fname;
            int flen = fn ? _pstrlen(fn) : 0;
            const unsigned char* sn = (pubname == 0 || pubname > lnames.size() ? 0 : lnames[pubname-1]);
            int slen = sn ? _pstrlen(sn) : 0;
            printf("Sym: %.*s\n", slen, sn);
            printf("File: %.*s\n", flen, fn);
            for (unsigned i = 0; i < num; i++)
                printf("\tOff 0x%x: Line %d\n", *(int*)(q + 2 + 6 * i), *(unsigned short*)(q + 6 * i));
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////
/// <summary>
/// Find the section that contains the given offset.
/// </summary>
/// <param name="off">The offset to find.</param>
/// <returns>The index of the section that contains the offset, or -1 if not found.</returns>
int PEImage::findSection(unsigned int off) const
{
    off -= IMGHDR(OptionalHeader.ImageBase);
    for(int s = 0; s < nsec; s++)
        if(sec[s].VirtualAddress <= off && off < sec[s].VirtualAddress + sec[s].Misc.VirtualSize)
            return s;
    return -1;
}

template<typename SYM>
const char* PEImage::t_findSectionSymbolName(int s) const
{
	SYM* sym = 0;
	for(int i = 0; i < nsym; i += 1 + sym->NumberOfAuxSymbols)
	{
		sym = (SYM*) symtable + i;
        if (sym->SectionNumber == s && sym->StorageClass == IMAGE_SYM_CLASS_EXTERNAL)
        {
            static char sname[10] = { 0 };

		    if (sym->N.Name.Short == 0)
                return strtable + sym->N.Name.Long;
            return strncpy (sname, (char*)sym->N.ShortName, 8);
        }
	}
    return 0;
}

const char* PEImage::findSectionSymbolName(int s) const
{
    if (s < 0 || s >= nsec)
        return 0;
    if (!(sec[s].Characteristics & IMAGE_SCN_LNK_COMDAT))
        return 0;

    if (bigobj)
        return t_findSectionSymbolName<IMAGE_SYMBOL_EX> (s);
    else
        return t_findSectionSymbolName<IMAGE_SYMBOL> (s);
}

/**
* Finds a symbol in the PE image by its name and returns its offset.
*
* @param name the name of the symbol to find
* @param off an output parameter that will hold the offset of the symbol
*
* @return the section number of the symbol if found, -1 otherwise
*/
int PEImage::findSymbol(const char* name, unsigned long& off) const
{
    int sizeof_sym = bigobj ? sizeof(IMAGE_SYMBOL_EX) : IMAGE_SIZEOF_SYMBOL;
	for(int i = 0; i < nsym; i++)
	{
		IMAGE_SYMBOL* sym = (IMAGE_SYMBOL*) (symtable + i * sizeof_sym);
		const char* symname = sym->N.Name.Short == 0 ? strtable + sym->N.Name.Long : (char*)sym->N.ShortName;
		if(strcmp(symname, name) == 0 || (symname[0] == '_' && strcmp(symname + 1, name) == 0))
		{
			off = sym->Value;
			return bigobj ? ((IMAGE_SYMBOL_EX*)sym)->SectionNumber : sym->SectionNumber;
		}
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////
/**
* Returns the count of CodeView entries.
*
* @return The number of CodeView entries in the PE image, or 0 if dirHeader is null.
*/
int PEImage::countCVEntries() const
{
	return dirHeader ? dirHeader->cDir : 0;
}

​
/// Returns the OMFDirEntry at the specified index (i) from the PEImage's directory
OMFDirEntry* PEImage::getCVEntry(int i) const
{
    return dirEntry + i;
}


///////////////////////////////////////////////////////////////////////
// utilities
/// Allocates a memory block with the specified alignment and alignment offset.
///
/// @param size The size of the memory block to be allocated.
/// @param align The alignment requirement for the memory block.
/// @param alignoff The alignment offset for the memory block.
///
void* PEImage::alloc_aligned(unsigned int size, unsigned int align, unsigned int alignoff)
{
    if (align & (align - 1))
        return 0;

    unsigned int pad = align + sizeof(void*);
    char* p = (char*) malloc(size + pad);
    unsigned int off = (align + alignoff - sizeof(void*) - (p - (char*) 0)) & (align - 1);
    char* q = p + sizeof(void*) + off;
    ((void**) q)[-1] = p;
    return q;
}

///////////////////////////////////////////////////////////////////////
/*
 * Frees the memory allocated for the PE image.
 */
void PEImage::free_aligned(void* p)
{
    /*
     * Gets the previous pointer from the aligned pointer and frees the memory.
     */
    void* q = ((void**) p)[-1];
    free(q);
}
