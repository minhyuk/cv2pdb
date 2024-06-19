// Convert DMD CodeView debug information to PDB files
// Copyright (c) 2009-2010 by Rainer Schuetze, All Rights Reserved
//
// License for redistribution is given by the Artistic License 2.0
// see file LICENSE for further details

#include "cvutil.h"

# 여기에 코드를 입력하세요

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

name = 'Alice'
age = 30
city = 'New York'

variables = [name, age, city]
print(variables)

# 원본 코드
var1 = 10
var2 = "hello"
var3 = [1, 2, 3]

print(var1, var2, var3)

city = 'New York'

bool isCompleteStruct(const codeview_type* type, const BYTE* name, bool cstr)
{
	return isStruct(type) 
		&& !(getStructProperty(type) & kPropIncomplete)
		&& cmpStructName(type, name, cstr);
}

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

# 여기에 코드를 입력해주세요

