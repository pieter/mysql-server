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

// EditString.cpp: implementation of the EditString class.
//
//////////////////////////////////////////////////////////////////////

#include <time.h>
#include <string.h>
#include "Engine.h"
#include "EditString.h"
#include "Value.h"

#define CURRENCY_SYMBOL		'$'
#define DECIMAL_POINT		'.'
#define NUMERIC_SEPARATOR	','

extern const char *months [];
extern const char *weekDays [];

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

EditString::EditString(const char *string)
{
	editString = string;
	chars = editString;
	stringLength = editString.length();
	expansion = NULL;
	parse();
}

EditString::~EditString()
{
	if (expansion)
		delete [] expansion;
}

void EditString::parse()
{
	char	c;
	bool	fraction = false, hour = false, sign = false;
	reset();
	length = years = months = days = weekdays = 0;
	julians = numericMonths = 0;
	hours = minutes = seconds = meridians = 0;
	currencySymbols = 0;
	wrapped = 0;
	digits = fractions = 0;
	width = height = 0;
	zones = 0;
	type = fmtString;

	while ((c = next()) != 0)
		{
		++length;
		if (quote != 0)
			continue;
		switch (c)
			{
			case 'X':
			case 'A':
				type = fmtString;
				break;

			// Signs may be fixed or floating, depending on number

			case '-':
			case '+':
				if (!sign)
					{
					sign = true;
					break;
					}

			// Floating symbols

			case '$':
				++currencySymbols;
			case '9':
			case '*':
				type = fmtNumber;
				++digits;
				if (fraction)
					++fractions;
				break;

			case 'Z':
				if (type != fmtDate)
					type = fmtNumber;
				++digits;
				++zones;
				if (fraction)
					++fractions;
				break;

			case 'T':
				type = fmtWrapped;
				++wrapped;
				break;

			case '.':
				fraction = true;
				break;

			case 'N':
				++numericMonths;
				break;

			case 'M':
				if (hour && minutes < 2)
					++minutes;
				else
					{
					++months;
					type = fmtDate;
					}
				break;

			case 'H':
				++hours;
				hour = true;
				if (type != fmtDate)
					type = fmtTime;
				break;

			case 'S':
				++seconds;
				break;

			case 'D':
				++days;
				//type = fmtDate;		// conflicts w/ DB
				break;

			case 'Y':
				++years;
				type = fmtDate;
				break;

			case 'J':
				++julians;
				type = fmtDate;
				break;

			case 'W':
				++weekdays;
				type = fmtDate;
				break;

			case 'P':
				++meridians;
				//type = fmtDate;
				break;
			}
		}
}

void EditString::reset()
{
	repeat = pos = quote = 0;
}

char EditString::next()
{
	for (;;)
		{
		char	c;
		if (repeat > 0)
			{
			--repeat;
			return last;
			}
		if (pos >= stringLength)
			return 0;
		c = chars [pos++];
		if (quote != 0)
			{
			if (quote == '\\')
				quote = 0;
			if (c == quote)
				{
				quote = 0;
				continue;
				}
			last = c;
			return c;
			}
		if (c == '\\' || c == '"' || c == '\'')
			{
			quote = c;
			continue;
			}
		if (c == '(')
			{
			repeat = 0;
			while (pos < stringLength && 
				   (c = chars [pos++]) != 0 && c != ')')
				if (c >= '0' && c <= '9')
					repeat = repeat * 10 + c - '0';
			--repeat;
			}
		else
			{
			if (c >= 'a' && c <= 'z')
				c = (char) (c - 'a' + 'A');
			last = c;
			return c;
			}
		}
}

char* EditString::formatNumber(Value * value, char *string)
{
	int64 org = value->getQuad (fractions);
	number = (org >= 0) ? org : -org;
	char c;

	if (!expansion)
		{
		expansion = new char [length];
		reset();
		for (int n = 0; n < length; ++n)
			expansion [n] = next();
		}

	int s = length, lastComma = -1;
	bool floater = false, currency = false;

	for (pos = length - 1; pos >= 0; --pos)
		{
		c = expansion [pos];
		switch (c)
			{
			// B may be blank or part of DB

			case 'B':
				if (pos == 0 || expansion [pos - 1] != 'D')
					{
					c = ' ';
					break;
					}

			case 'C':
			case 'R':
			case 'D':
				if (org >= 0)
					c = ' ';
				break;

			case '0':
				break;
									
			case '9':
				c = nextDigit();
				break;
			
			case 'Z':
				c = (number == 0) ? ' ' : nextDigit();
				break;

			case '*':
				c = (number == 0) ? '*' : nextDigit();
				break;
										
			case '$':
				if (number != 0)
					c = nextDigit();
				else if (currency)
					c = ' ';
				else
					{
					currency = true;
					if (org == 0 && currencySymbols > 1)
						c = ' ';
					else if (lastComma > 0 && currencySymbols > 1)
						{
						string [lastComma] = CURRENCY_SYMBOL;	
						c = ' ';
						}
					else
						c = CURRENCY_SYMBOL;
					}
				break;
										
			case '.':
				c = DECIMAL_POINT;
				break;

			case ',':
				if (number == 0)
					{
					c = ' ';
					lastComma = s - 1;
					}
				else
					c = NUMERIC_SEPARATOR;
				break;

			case '-':
				if (number == 0)
					{
					if (floater)
						c = ' ';
					else
						{
						c = (org >= 0) ? ' ' : '-';
						floater = true;
						}
					}
				else
					c = nextDigit();
				break;
				
			case '+':
				if (number == 0)
					{
					if (floater)
						c = ' ';
					else
						{
						c = (org >= 0) ? '+' : '-';
						floater = true;
						}
					}
				else
					c = nextDigit();
				break;
			}
		string [--s] = c;
		}

	if (number != 0)
		for (int n = 0; n < length; ++n)
			string [n] = '#';

	return string;
}

char EditString::nextDigit()
{
	char c = (char) ('0' + (number % 10));
	number /= 10;

	return c;
}

char* EditString::formatString(Value * value, char * to)
{
	char		*temp;
	const char	*from = value->getString (&temp);
	int			fromLength = strlen (from);
	int			fpos = 0, tpos = 0;
	char		c;
	reset();

	while ((c = next()) != 0)
		{
		if (quote == 0)
			switch (c)
				{
				case 'X':
					c = (fpos < fromLength) ? from [fpos++] : ' ';
					break;
				
				case 'A':
					c = (fpos < fromLength) ? from [fpos++] : ' ';
					if (!(c == ' ' ||
						 (c >= 'a' && c <= 'z') ||
						 (c >= 'A' && c <= 'Z')))
						c = '*';
					break;

				case 'B':
					c = ' ';
					break;
				}
		to [tpos++] = c;
		}

	if (temp)
		delete [] temp;

	return to;
}

char* EditString::formatDate(int32 date, char * output)
{
	DateTime dt;
	dt.setSeconds (date);
	//dt = date;

	return formatDate (dt, output);
}

char* EditString::formatDate(DateTime date, char *to)
{
	struct tm	stuff, *time = &stuff;
	date.getLocalTime (&stuff);
	int 		month = time->tm_mon + 1;
	const char	*monthString = ::months [month - 1];
	const char	*zoneString = DateTime::getTimeZone();
	int 		year = time->tm_year + 1900;
	int 		day = time->tm_mday;
	int			hour = time->tm_hour;
	int			minute = time->tm_min;
	int			second = time->tm_sec;
	int			julian = time->tm_yday + 1;
	const char	*dayString = ::weekDays [time->tm_wday];
	int 		dayPos = 0, monthPos = 0, yearPos = 0, weekPos = 0, nPos = 0;
	int			hourPos = 0, minutePos = 0, secondPos = 0, julianPos = 0;
	int			tpos = 0, zonePos = 0;
	char		c;
	const char	*meridian = (hour >= 12) ? "PM" : "AM";
	bool		blank = false;

	reset();

	while ((c = next()) != 0)
		{
		if (quote == 0)
			switch (c)
				{
				case 'D':
					c = digit (day, dayPos++, days, blank);
					if (blank && dayPos == 1 && c == '0')
						c = ' ';
					break;

				case 'J':
					c = digit (julian, julianPos++, julians, true);
					if (julianPos == 1 && c == '0')
						c = ' ';
					break;

				case 'M':
					if (hourPos && minutePos < 2)
						{
						c = digit (minute, minutePos++, hours, false);
						break;
						}
					if ((c = monthString [monthPos]))
						++monthPos;
					else
						continue;
					break;

				case 'W':
					if ((c = dayString [weekPos]))
						++weekPos;
					else
						continue;
					break;

				case 'Z':
					if ((c = zoneString [zonePos]))
						++zonePos;
					else
						continue;
					break;

				case 'N':
					c = digit (month, nPos++, numericMonths, false);
					break;

				case 'Y':
					c = digit (year, yearPos++, years, false);
					break;

				case 'H':
					if (meridians)
						{
						c = digit ((hour - 1) % 12 + 1, hourPos++, hours, false);
						if (hourPos == 1 && c == '0')
							c = ' ';
						}
					else
						c = digit (hour, hourPos++, hours, false);
					break;

				case 'S':
					c = digit (second, secondPos++, seconds, false);
					break;

				case 'B':
					c = ' ';
					break;

				case 'P':
					if (*meridian)
						c = *meridian++;
					break;
				}
		to [tpos++] = c;
		if (c == ' ')
			blank = true;
		}

	while (tpos < length)
		to [tpos++] = ' ';

	return to;
}

char EditString::digit(int number, int pos, int length, bool blank)
{
	for (int n = length - pos - 1; n > 0; --n)
		number /= 10;

	if (blank && (number == 0))
		return ' ';

	return (char) ('0' + number % 10);
}

char* EditString::format(Value * value, char * string)
{
	switch (type)
		{
		case fmtNumber:
			return formatNumber (value, string);

		case fmtTime:
			if (value->isNull (Date))
				{
				*string = 0;
				return string;
				}
			return formatDate (value->getTime(), string);

		case fmtDate:
			if (value->isNull (Date))
				{
				*string = 0;
				return string;
				}
			return formatDate (value->getDate(), string);

		case fmtString:
			return formatString (value, string);

		// NOT_YET_IMPLEMENTED
		case  fmtBlob:
		case  fmtWrapped:
		case  fmtImage:
			return NULL;
		}
	return NULL;
}

char* EditString::format(const char * str, char * output)
{
	Value value;
	value.setString (str, false);

	return format (&value, output);
}
