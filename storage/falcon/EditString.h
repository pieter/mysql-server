/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// EditString.h: interface for the EditString class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_EDITSTRING_H__6A819565_75F7_11D3_AB7C_0000C01D2301__INCLUDED_)
#define AFX_EDITSTRING_H__6A819565_75F7_11D3_AB7C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

enum FormatType {
	fmtString,
	fmtNumber,
	fmtDate,
	fmtTime,
	fmtBlob,
	fmtWrapped,
	fmtImage,
	};

class Value;
class DateTime;

class EditString  
{
public:
	char* formatDate (int32 date, char *output);
	char* format (const char *str, char *output);
	char* format (Value *value, char *string);
	char digit (int number, int pos, int length, bool blank);
	char* formatDate (DateTime date, char *string);
	char* formatString (Value *value, char *string);
	char* formatNumber (Value *value, char *expansion);
	char next();
	void reset();
	void parse();
	EditString(const char *string);
	virtual ~EditString();

	JString		editString;
	const char	*chars;
	int			stringLength;
	FormatType	type;
	QUAD		number;		
	int			width, height;
	char		last, quote;
	int			digits, months, years, days, fractions,
				length, weekdays, hours, minutes,
				seconds, julians, meridians, numericMonths,
				currencySymbols, wrapped, zones;

private:
	int			pos, repeat;
	char		*expansion;
protected:
	char nextDigit();
};

#endif // !defined(AFX_EDITSTRING_H__6A819565_75F7_11D3_AB7C_0000C01D2301__INCLUDED_)
