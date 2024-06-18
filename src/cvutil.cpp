// Convert DMD CodeView debug information to PDB files
// Copyright (c) 2009-2010 by Rainer Schuetze, All Rights Reserved
//
// License for redistribution is given by the Artistic License 2.0
// see file LICENSE for further details

#include "cvutil.h"

/**
 * Checks if the given codeview_type is a struct or class.
 *
 * @param cvtype Pointer to the codeview_type.
 * @return True if the codeview_type is a struct or class, otherwise false.
 */
bool isStruct(const codeview_type* cvtype)
{
	switch (cvtype->generic.id)
	{
	case LF_STRUCTURE_V1:
	case LF_CLASS_V1:
	case LF_STRUCTURE_V2:
	case LF_CLASS_V2:
	case LF_STRUCTURE_V3:
	case LF_CLASS_V3:
		return true;
	}
	return false;
}

/**
 * Checks if the given codeview_type is a class.
 *
 * @param cvtype Pointer to the codeview_type.
 * @return True if the codeview_type is a class, otherwise false.
 */
bool isClass(const codeview_type* cvtype)
{
	switch (cvtype->generic.id)
	{
	case LF_CLASS_V1:
	case LF_CLASS_V2:
	case LF_CLASS_V3:
		return true;
	}
	return false;
}

/**
 * Retrieves the property of a struct or class from a given codeview_type.
 *
 * @param cvtype Pointer to the codeview_type.
 * @return The property value of the struct or class, or 0 if the type is not recognized.
 */
int getStructProperty(const codeview_type* cvtype)
{
	switch (cvtype->generic.id)
	{
	case LF_STRUCTURE_V1:
	case LF_CLASS_V1:
		return cvtype->struct_v1.property;
	case LF_STRUCTURE_V2:
	case LF_CLASS_V2:
		return cvtype->struct_v2.property;
	case LF_STRUCTURE_V3:
	case LF_CLASS_V3:
		return cvtype->struct_v3.property;
	}
	return 0;
}

/**
 * Retrieves the field list of a struct or class from a given codeview_type.
 *
 * @param cvtype Pointer to the codeview_type.
 * @return The field list value of the struct or class, or 0 if the type is not recognized.
 */
int getStructFieldlist(const codeview_type* cvtype)
{
	switch (cvtype->generic.id)
	{
	case LF_STRUCTURE_V1:
	case LF_CLASS_V1:
		return cvtype->struct_v1.fieldlist;
	case LF_STRUCTURE_V2:
	case LF_CLASS_V2:
		return cvtype->struct_v2.fieldlist;
	case LF_STRUCTURE_V3:
	case LF_CLASS_V3:
		return cvtype->struct_v3.fieldlist;
	}
	return 0;
}

/**
 * Retrieves the name of a struct or class from a given codeview_type.
 *
 * @param cvtype Pointer to the codeview_type.
 * @param cstr Reference to a boolean that will be set to true if the name is a C-style string.
 * @return Pointer to the name of the struct or class, or 0 if the type is not recognized.
 */
const BYTE* getStructName(const codeview_type* cvtype, bool &cstr)
{
	int value, leaf_len;
	switch (cvtype->generic.id)
	{
	case LF_STRUCTURE_V1:
	case LF_CLASS_V1:
		cstr = false;
		leaf_len = numeric_leaf(&value, &cvtype->struct_v1.structlen);
		return (const BYTE*) &cvtype->struct_v1.structlen + leaf_len;
	case LF_STRUCTURE_V2:
	case LF_CLASS_V2:
		cstr = false;
		leaf_len = numeric_leaf(&value, &cvtype->struct_v2.structlen);
		return (const BYTE*) &cvtype->struct_v2.structlen + leaf_len;
	case LF_STRUCTURE_V3:
	case LF_CLASS_V3:
		cstr = true;
		leaf_len = numeric_leaf(&value, &cvtype->struct_v3.structlen);
		return (const BYTE*) &cvtype->struct_v3.structlen + leaf_len;
	}
	return 0;
}

/**
 * Compares the name of a struct or class with a given name.
 *
 * @param cvtype Pointer to the codeview_type.
 * @param name Pointer to the name to compare.
 * @param cstr Boolean indicating if the provided name is a C-style string.
 * @return True if the names are equal, false otherwise.
 */
bool cmpStructName(const codeview_type* cvtype, const BYTE* name, bool cstr)
{
	bool cstr2;
	const BYTE* name2 = getStructName(cvtype, cstr2);
	if(!name || !name2)
		return name == name2;
	return dstrcmp(name, cstr, name2, cstr2);
}

/**
 * Checks if the given codeview_type is a complete struct with the specified name.
 *
 * @param type Pointer to the codeview_type.
 * @param name Pointer to the name to compare.
 * @param cstr Boolean indicating if the provided name is a C-style string.
 * @return True if the type is a complete struct with the specified name, otherwise false.
 */
bool isCompleteStruct(const codeview_type* type, const BYTE* name, bool cstr)
{
	return isStruct(type) 
		&& !(getStructProperty(type) & kPropIncomplete)
		&& cmpStructName(type, name, cstr);
}

/**
 * Processes a numeric leaf from the given leaf data pointer and extracts the value.
 *
 * @param value Pointer to an integer where the extracted value will be stored.
 * @param leaf Pointer to the leaf data to be processed.
 * @return The length of the processed leaf in bytes, or 0 if there is an error.
 */
int numeric_leaf(int* value, const void* leaf)
{
	unsigned short int type = *(const unsigned short int*) leaf;
	leaf = (const unsigned short int*) leaf + 1;
	int length = 2;

	*value = 0;
	switch (type)
	{
	case LF_CHAR:
		length += 1;
		*value = *(const char*)leaf;
		break;

	case LF_SHORT:
		length += 2;
		*value = *(const short*)leaf;
		break;

	case LF_USHORT:
		length += 2;
		*value = *(const unsigned short*)leaf;
		break;

	case LF_LONG:
	case LF_ULONG:
		length += 4;
		*value = *(const int*)leaf;
		break;

	case LF_COMPLEX64:
	case LF_QUADWORD:
	case LF_UQUADWORD:
	case LF_REAL64:
		length += 8;
		break;

	case LF_COMPLEX32:
	case LF_REAL32:
		length += 4;
		break;

	case LF_REAL48:
		length += 6;
		break;

	case LF_COMPLEX80:
	case LF_REAL80:
		length += 10;
		break;

	case LF_COMPLEX128:
	case LF_REAL128:
		length += 16;
		break;

	case LF_VARSTRING:
		length += 2 + *(const unsigned short*)leaf;
		break;

	default:
		if (type < LF_NUMERIC)
			*value = type;
		else
		{
			length = 0; // error!
		}
		break;
	}
	return length;
}

/**
 * Writes a numeric value into the specified leaf buffer, considering the value's range.
 *
 * @param value The integer value to be written.
 * @param leaf Pointer to the leaf buffer where the value will be written.
 * @return The length of the written leaf in bytes.
 */
int write_numeric_leaf(int value, void* leaf)
{
	if(value >= 0 && value < LF_NUMERIC)
	{
		*(unsigned short int*) leaf = (unsigned short) value;
		return 2;
	}
	unsigned short int* type = (unsigned short int*) leaf;
	leaf = type + 1;
	if (value >= -128 && value <= 127)
	{
		*type = LF_CHAR;
		*(char*) leaf = (char)value;
		return 3;
	}
	if (value >= -32768 && value <= 32767)
	{
		*type = LF_SHORT;
		*(short*) leaf = (short)value;
		return 4;
	}
	if (value >= 0 && value <= 65535)
	{
		*type = LF_USHORT;
		*(unsigned short*) leaf = (unsigned short)value;
		return 4;
	}
	*type = LF_LONG;
	*(long*) leaf = (long)value;
	return 6;
}

