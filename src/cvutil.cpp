// Convert DMD CodeView debug information to PDB files
// Copyright (c) 2009-2010 by Rainer Schuetze, All Rights Reserved
//
// License for redistribution is given by the Artistic License 2.0
// see file LICENSE for further details

#include "cvutil.h"

bool isStruct(const codeview_type* typeInfo)
{
	switch (typeInfo->generic.id)
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

bool isClass(const codeview_type* typeInfo)
{
	switch (typeInfo->generic.id)
	{
	case LF_CLASS_V1:
	case LF_CLASS_V2:
	case LF_CLASS_V3:
		return true;
	}
	return false;
}

int getStructProperty(const codeview_type* typeInfo)
{
    switch (typeInfo->generic.id)
    {
        case LF_STRUCTURE_V1:
        case LF_CLASS_V1:
            return typeInfo->struct_v1.property;
        case LF_STRUCTURE_V2:
        case LF_CLASS_V2:
            return typeInfo->struct_v2.property;
        case LF_STRUCTURE_V3:
        case LF_CLASS_V3:
            return typeInfo->struct_v3.property;
    }
    return 0;
}

int getStructFieldlist(const codeview_type* typeInfo)
{
	switch (typeInfo->generic.id)
	{
	case LF_STRUCTURE_V1:
	case LF_CLASS_V1:
		return typeInfo->struct_v1.fieldlist;
	case LF_STRUCTURE_V2:
	case LF_CLASS_V2:
		return typeInfo->struct_v2.fieldlist;
	case LF_STRUCTURE_V3:
	case LF_CLASS_V3:
		return typeInfo->struct_v3.fieldlist;
	}
	return 0;
}

const BYTE* getTypeInfo(const codeview_type* typeInfo, bool &isCString)
{
	int leafValue, leafLength;
	switch (typeInfo->generic.id)
	{
	case LF_STRUCTURE_V1:
	case LF_CLASS_V1:
		isCString = false;
		leafLength = numeric_leaf(&leafValue, &typeInfo->struct_v1.structlen);
		return (const BYTE*) &typeInfo->struct_v1.structlen + leafLength;
	case LF_STRUCTURE_V2:
	case LF_CLASS_V2:
		isCString = false;
		leafLength = numeric_leaf(&leafValue, &typeInfo->struct_v2.structlen);
		return (const BYTE*) &typeInfo->struct_v2.structlen + leafLength;
	case LF_STRUCTURE_V3:
	case LF_CLASS_V3:
		isCString = true;
		leafLength = numeric_leaf(&leafValue, &typeInfo->struct_v3.structlen);
		return (const BYTE*) &typeInfo->struct_v3.structlen + leafLength;
	}
	return 0;
}

bool cmpStructName(const codeview_type* cvtype, const BYTE* name, bool isCString)
{
    const BYTE* name2 = getStructName(cvtype, isCString);
    if(!name || !name2)
        return name == name2;
    return dstrcmp(name, isCString, name2, isCString);
}

bool isCompleteStruct(const codeview_type* type, const BYTE* name, bool isCStringType)
{
	return isStruct(type) 
		&& !(getStructProperty(type) & kPropIncomplete)
		&& cmpStructName(type, name, isCStringType);
}

int numeric_leaf(int* dataValue, const void* leaf)
{
	unsigned short int dataType = *(const unsigned short int*) leaf;
	leaf = (const unsigned short int*) leaf + 1;
	int length = 2;

	*dataValue = 0;
	switch (dataType)
	{
	case LF_CHAR:
		length += 1;
		*dataValue = *(const char*)leaf;
		break;

	case LF_SHORT:
		length += 2;
		*dataValue = *(const short*)leaf;
		break;

	case LF_USHORT:
		length += 2;
		*dataValue = *(const unsigned short*)leaf;
		break;

	case LF_LONG:
	case LF_ULONG:
		length += 4;
		*dataValue = *(const int*)leaf;
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
		if (dataType < LF_NUMERIC)
			*dataValue = dataType;
		else
		{
			length = 0; // error!
		}
		break;
	}
	return length;
}

int write_numeric_leaf(int inputNumericValue, void* dataLeafPointer)
{
	if(inputNumericValue >= 0 && inputNumericValue < LF_NUMERIC)
	{
		*(unsigned short int*) dataLeafPointer = (unsigned short) inputNumericValue;
		return 2;
	}
	unsigned short int* dataType = (unsigned short int*) dataLeafPointer;
	dataLeafPointer = dataType + 1;
	if (inputNumericValue >= -128 && inputNumericValue <= 127)
	{
		*dataType = LF_CHAR;
		*(char*) dataLeafPointer = (char)inputNumericValue;
		return 3;
	}
	if (inputNumericValue >= -32768 && inputNumericValue <= 32767)
	{
		*dataType = LF_SHORT;
		*(short*) dataLeafPointer = (short)inputNumericValue;
		return 4;
	}
	if (inputNumericValue >= 0 && inputNumericValue <= 65535)
	{
		*dataType = LF_USHORT;
		*(unsigned short*) dataLeafPointer = (unsigned short)inputNumericValue;
		return 4;
	}
	*dataType = LF_LONG;
	*(long*) dataLeafPointer = (long)inputNumericValue;
	return 6;
}

