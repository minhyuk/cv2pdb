// Convert DMD CodeView debug information to PDB files
// Copyright (c) 2009-2010 by Rainer Schuetze, All Rights Reserved
//
// License for redistribution is given by the Artistic License 2.0
// see file LICENSE for further details

#include "cvutil.h"

/**
 * @brief 주어진 코드 뷰 타입이 구조체인지 여부를 확인합니다.
 *
 * @param cvtype 검사할 코드 뷰 타입의 포인터입니다.
 * @return 구조체 또는 클래스일 경우 true를, 그렇지 않을 경우 false를 반환합니다.
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
 * @brief 주어진 코드 뷰 타입이 클래스인지 여부를 확인합니다.
 *
 * @param cvtype 검사할 코드 뷰 타입의 포인터입니다.
 * @return 클래스일 경우 true를, 그렇지 않을 경우 false를 반환합니다.
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
 * @brief 주어진 코드 뷰 타입의 구조체 혹은 클래스의 속성을 반환합니다.
 *
 * @param cvtype 검사할 코드 뷰 타입의 포인터입니다.
 * @return 구조체 혹은 클래스의 속성을 반환하며, 타입이 다를 경우 0을 반환합니다.
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
 * @brief 주어진 코드 뷰 타입의 구조체 또는 클래스의 필드 리스트를 반환합니다.
 *
 * @param cvtype 검사할 코드 뷰 타입의 포인터입니다.
 * @return 구조체 또는 클래스의 필드 리스트를 반환하며, 타입이 다를 경우 0을 반환합니다.
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
 * @brief 주어진 코드 뷰 타입의 구조체 또는 클래스의 이름을 반환합니다.
 *
 * @param cvtype 검사할 코드 뷰 타입의 포인터입니다.
 * @param cstr 리턴되는 이름이 C-Style 문자열인지 여부를 나타내는 참조 변수입니다. LF_STRUCTURE_V3 또는 LF_CLASS_V3의 경우 true로 설정됩니다.
 * @return 구조체 또는 클래스의 이름에 대한 포인터를 반환합니다. 타입이 다를 경우 nullptr를 반환합니다.
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
 * @brief 주어진 이름과 코드 뷰 타입의 구조체 또는 클래스의 이름을 비교합니다.
 *
 * @param cvtype 검사할 코드 뷰 타입의 포인터입니다.
 * @param name 비교할 이름에 대한 포인터입니다.
 * @param cstr 비교할 이름이 C-Style 문자열인지 여부를 나타내는 변수입니다.
 * @return 두 이름이 같으면 true, 다르면 false를 반환합니다.
 */
bool cmpStructName(const codeview_type* cvtype, const BYTE* name, bool cstr)
{
	bool cstr2;
	const BYTE* name2 = getStructName(cvtype, cstr2);
	if (!name || !name2)
		return name == name2;
	return dstrcmp(name, cstr, name2, cstr2);
}

/**
 * @brief 주어진 코드 뷰 타입이 완전한 구조체인지 여부를 확인합니다.
 *
 * @param type 검사할 코드 뷰 타입의 포인터입니다.
 * @param name 비교할 이름에 대한 포인터입니다.
 * @param cstr 비교할 이름이 C-Style 문자열인지 여부를 나타내는 변수입니다.
 * @return 타입이 구조체이며 완전하고, 이름이 일치하면 true를, 그렇지 않으면 false를 반환합니다.
 */
bool isCompleteStruct(const codeview_type* type, const BYTE* name, bool cstr)
{
	return isStruct(type) 
		&& !(getStructProperty(type) & kPropIncomplete) 
		&& cmpStructName(type, name, cstr);
}

/**
 * @brief 주어진 리프(leaf) 값을 읽어서 숫자 값으로 변환하고, 해당 리프의 길이를 반환합니다.
 *
 * @param value 변환된 숫자 값을 저장할 포인터입니다.
 * @param leaf 리프 데이터를 나타내는 포인터입니다.
 * @return 리프의 길이를 바이트 단위로 반환합니다. 에러가 발생한 경우 0을 반환합니다.
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
 * @brief 주어진 숫자 값을 리프(leaf) 형식으로 변환하여 저장합니다.
 *
 * @param value 변환할 숫자 값입니다.
 * @param leaf 리프 데이터를 저장할 포인터입니다.
 * @return 리프 데이터의 길이를 바이트 단위로 반환합니다.
 */
int write_numeric_leaf(int value, void* leaf)
{
	if (value >= 0 && value < LF_NUMERIC)
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

