/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* A lexical scanner on a temporary buffer with a yacc interface */

#include "mysql_priv.h"
#include "item_create.h"
#include <m_ctype.h>
#include <hash.h>

LEX_STRING tmp_table_alias= {(char*) "tmp-table",8};

/* Macros to look like lex */

#define yyGet()		*(lex->ptr++)
#define yyGetLast()	lex->ptr[-1]
#define yyPeek()	lex->ptr[0]
#define yyPeek2()	lex->ptr[1]
#define yyUnget()	lex->ptr--
#define yySkip()	lex->ptr++
#define yyLength()	((uint) (lex->ptr - lex->tok_start)-1)

#if MYSQL_VERSION_ID < 32300
#define FLOAT_NUM	REAL_NUM
#endif

pthread_key(LEX*,THR_LEX);

/* Longest standard keyword name */
#define TOCK_NAME_LENGTH 24

/*
  Map to default keyword characters.  This is used to test if an identifer
  is 'simple', in which case we don't have to do any character set conversions
  on it
*/
uchar *bin_ident_map= my_charset_bin.ident_map;

/*
  The following data is based on the latin1 character set, and is only
  used when comparing keywords
*/

uchar to_upper_lex[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,247,216,217,218,219,220,221,222,255
};


inline int lex_casecmp(const char *s, const char *t, uint len)
{
  while (len-- != 0 &&
	 to_upper_lex[(uchar) *s++] == to_upper_lex[(uchar) *t++]) ;
  return (int) len+1;
}

#include "lex_hash.h"


void lex_init(void)
{
  uint i;
  DBUG_ENTER("lex_init");
  for (i=0 ; i < array_elements(symbols) ; i++)
    symbols[i].length=(uchar) strlen(symbols[i].name);
  for (i=0 ; i < array_elements(sql_functions) ; i++)
    sql_functions[i].length=(uchar) strlen(sql_functions[i].name);

  VOID(pthread_key_create(&THR_LEX,NULL));

  DBUG_VOID_RETURN;
}


void lex_free(void)
{					// Call this when daemon ends
  DBUG_ENTER("lex_free");
  DBUG_VOID_RETURN;
}


/*
  This is called before every query that is to be parsed.
  Because of this, it's critical to not do too much things here.
  (We already do too much here)
*/

LEX *lex_start(THD *thd, uchar *buf,uint length)
{
  LEX *lex= &thd->lex;
  lex->thd= thd;
  lex->next_state=MY_LEX_START;
  lex->end_of_query=(lex->ptr=buf)+length;
  lex->yylineno = 1;
  lex->select_lex.parsing_place= SELECT_LEX_NODE::NO_MATTER;
  lex->in_comment=0;
  lex->length=0;
  lex->select_lex.in_sum_expr=0;
  lex->select_lex.expr_list.empty();
  lex->select_lex.ftfunc_list_alloc.empty();
  lex->select_lex.ftfunc_list= &lex->select_lex.ftfunc_list_alloc;
  lex->current_select= &lex->select_lex;
  lex->yacc_yyss=lex->yacc_yyvs=0;
  lex->ignore_space=test(thd->variables.sql_mode & MODE_IGNORE_SPACE);
  lex->sql_command=SQLCOM_END;
  lex->duplicates= DUP_ERROR;
  return lex;
}

void lex_end(LEX *lex)
{
  lex->select_lex.expr_list.delete_elements();	// If error when parsing sql-varargs
  x_free(lex->yacc_yyss);
  x_free(lex->yacc_yyvs);
}


static int find_keyword(LEX *lex, uint len, bool function)
{
  uchar *tok=lex->tok_start;

  SYMBOL *symbol = get_hash_symbol((const char *)tok,len,function);
  if (symbol)
  {
    lex->yylval->symbol.symbol=symbol;
    lex->yylval->symbol.str= (char*) tok;
    lex->yylval->symbol.length=len;
    return symbol->tok;
  }
#ifdef HAVE_DLOPEN
  udf_func *udf;
  if (function && using_udf_functions && (udf=find_udf((char*) tok, len)))
  {
    lex->safe_to_cache_query=0;
    lex->yylval->udf=udf;
    switch (udf->returns) {
    case STRING_RESULT:
      return (udf->type == UDFTYPE_FUNCTION) ? UDF_CHAR_FUNC : UDA_CHAR_SUM;
    case REAL_RESULT:
      return (udf->type == UDFTYPE_FUNCTION) ? UDF_FLOAT_FUNC : UDA_FLOAT_SUM;
    case INT_RESULT:
      return (udf->type == UDFTYPE_FUNCTION) ? UDF_INT_FUNC : UDA_INT_SUM;
    case ROW_RESULT:
    default:
      // This case should never be choosen
      DBUG_ASSERT(0);
      return 0;
    }
  }
#endif
  return 0;
}


/* make a copy of token before ptr and set yytoklen */

static LEX_STRING get_token(LEX *lex,uint length)
{
  LEX_STRING tmp;
  yyUnget();			// ptr points now after last token char
  tmp.length=lex->yytoklen=length;
  tmp.str=(char*) lex->thd->strmake((char*) lex->tok_start,tmp.length);
  return tmp;
}

static LEX_STRING get_quoted_token(LEX *lex,uint length, char quote)
{
  LEX_STRING tmp;
  byte *from, *to, *end;
  yyUnget();			// ptr points now after last token char
  tmp.length=lex->yytoklen=length;
  tmp.str=(char*) lex->thd->alloc(tmp.length+1);
  for (from= (byte*) lex->tok_start, to= (byte*) tmp.str, end= to+length ;
       to != end ;
       )
  {
    if ((*to++= *from++) == quote)
      from++;					// Skip double quotes
  }
  *to= 0;					// End null for safety
  return tmp;
}


/*
  Return an unescaped text literal without quotes
  Fix sometimes to do only one scan of the string
*/

static char *get_text(LEX *lex)
{
  reg1 uchar c,sep;
  uint found_escape=0;
  CHARSET_INFO *cs= lex->thd->charset();

  sep= yyGetLast();			// String should end with this
  //lex->tok_start=lex->ptr-1;		// Remember '
  while (lex->ptr != lex->end_of_query)
  {
    c = yyGet();
#ifdef USE_MB
    int l;
    if (use_mb(cs) &&
        (l = my_ismbchar(cs,
                         (const char *)lex->ptr-1,
                         (const char *)lex->end_of_query))) {
	lex->ptr += l-1;
	continue;
    }
#endif
    if (c == '\\')
    {					// Escaped character
      found_escape=1;
      if (lex->ptr == lex->end_of_query)
	return 0;
      yySkip();
    }
    else if (c == sep)
    {
      if (c == yyGet())			// Check if two separators in a row
      {
	found_escape=1;			// dupplicate. Remember for delete
	continue;
      }
      else
	yyUnget();

      /* Found end. Unescape and return string */
      uchar *str,*end,*start;

      str=lex->tok_start+1;
      end=lex->ptr-1;
      if (!(start=(uchar*) lex->thd->alloc((uint) (end-str)+1)))
	return (char*) "";		// Sql_alloc has set error flag
      if (!found_escape)
      {
	lex->yytoklen=(uint) (end-str);
	memcpy(start,str,lex->yytoklen);
	start[lex->yytoklen]=0;
      }
      else
      {
	uchar *to;
	for (to=start ; str != end ; str++)
	{
#ifdef USE_MB
	  int l;
	  if (use_mb(cs) &&
              (l = my_ismbchar(cs,
                               (const char *)str, (const char *)end))) {
	      while (l--)
		  *to++ = *str++;
	      str--;
	      continue;
	  }
#endif
	  if (*str == '\\' && str+1 != end)
	  {
	    switch(*++str) {
	    case 'n':
	      *to++='\n';
	      break;
	    case 't':
	      *to++= '\t';
	      break;
	    case 'r':
	      *to++ = '\r';
	      break;
	    case 'b':
	      *to++ = '\b';
	      break;
	    case '0':
	      *to++= 0;			// Ascii null
	      break;
	    case 'Z':			// ^Z must be escaped on Win32
	      *to++='\032';
	      break;
	    case '_':
	    case '%':
	      *to++= '\\';		// remember prefix for wildcard
	      /* Fall through */
	    default:
	      *to++ = *str;
	      break;
	    }
	  }
	  else if (*str == sep)
	    *to++= *str++;		// Two ' or "
	  else
	    *to++ = *str;

	}
	*to=0;
	lex->yytoklen=(uint) (to-start);
      }
      return (char*) start;
    }
  }
  return 0;					// unexpected end of query
}


/*
** Calc type of integer; long integer, longlong integer or real.
** Returns smallest type that match the string.
** When using unsigned long long values the result is converted to a real
** because else they will be unexpected sign changes because all calculation
** is done with longlong or double.
*/

static const char *long_str="2147483647";
static const uint long_len=10;
static const char *signed_long_str="-2147483648";
static const char *longlong_str="9223372036854775807";
static const uint longlong_len=19;
static const char *signed_longlong_str="-9223372036854775808";
static const uint signed_longlong_len=19;
static const char *unsigned_longlong_str="18446744073709551615";
static const uint unsigned_longlong_len=20;

inline static uint int_token(const char *str,uint length)
{
  if (length < long_len)			// quick normal case
    return NUM;
  bool neg=0;

  if (*str == '+')				// Remove sign and pre-zeros
  {
    str++; length--;
  }
  else if (*str == '-')
  {
    str++; length--;
    neg=1;
  }
  while (*str == '0' && length)
  {
    str++; length --;
  }
  if (length < long_len)
    return NUM;

  uint smaller,bigger;
  const char *cmp;
  if (neg)
  {
    if (length == long_len)
    {
      cmp= signed_long_str+1;
      smaller=NUM;				// If <= signed_long_str
      bigger=LONG_NUM;				// If >= signed_long_str
    }
    else if (length < signed_longlong_len)
      return LONG_NUM;
    else if (length > signed_longlong_len)
      return REAL_NUM;
    else
    {
      cmp=signed_longlong_str+1;
      smaller=LONG_NUM;				// If <= signed_longlong_str
      bigger=REAL_NUM;
    }
  }
  else
  {
    if (length == long_len)
    {
      cmp= long_str;
      smaller=NUM;
      bigger=LONG_NUM;
    }
    else if (length < longlong_len)
      return LONG_NUM;
    else if (length > longlong_len)
    {
      if (length > unsigned_longlong_len)
	return REAL_NUM;
      cmp=unsigned_longlong_str;
      smaller=ULONGLONG_NUM;
      bigger=REAL_NUM;
    }
    else
    {
      cmp=longlong_str;
      smaller=LONG_NUM;
      bigger= ULONGLONG_NUM;
    }
  }
  while (*cmp && *cmp++ == *str++) ;
  return ((uchar) str[-1] <= (uchar) cmp[-1]) ? smaller : bigger;
}


/*
  yylex remember the following states from the following yylex()

  - MY_LEX_EOQ			Found end of query
  - MY_LEX_OPERATOR_OR_IDENT	Last state was an ident, text or number
				(which can't be followed by a signed number)
*/

int yylex(void *arg, void *yythd)
{
  reg1	uchar c;
  int	tokval, result_state;
  uint length;
  enum my_lex_states state,prev_state;
  LEX	*lex= &(((THD *)yythd)->lex);
  YYSTYPE *yylval=(YYSTYPE*) arg;
  CHARSET_INFO *cs= ((THD *) yythd)->charset();
  uchar *state_map= cs->state_map;
  uchar *ident_map= cs->ident_map;

  lex->yylval=yylval;			// The global state
  lex->tok_start=lex->tok_end=lex->ptr;
  prev_state=state=lex->next_state;
  lex->next_state=MY_LEX_OPERATOR_OR_IDENT;
  LINT_INIT(c);
  for (;;)
  {
    switch (state) {
    case MY_LEX_OPERATOR_OR_IDENT:	// Next is operator or keyword
    case MY_LEX_START:			// Start of token
      // Skip startspace
      for (c=yyGet() ; (state_map[c] == MY_LEX_SKIP) ; c= yyGet())
      {
	if (c == '\n')
	  lex->yylineno++;
      }
      lex->tok_start=lex->ptr-1;	// Start of real token
      state= (enum my_lex_states) state_map[c];
      break;
    case MY_LEX_ESCAPE:
      if (yyGet() == 'N')
      {					// Allow \N as shortcut for NULL
	yylval->lex_str.str=(char*) "\\N";
	yylval->lex_str.length=2;
	return NULL_SYM;
      }
    case MY_LEX_CHAR:			// Unknown or single char token
    case MY_LEX_SKIP:			// This should not happen
      if (c == '-' && yyPeek() == '-' &&
          (my_isspace(cs,yyPeek2()) || 
           my_iscntrl(cs,yyPeek2())))
      {
        state=MY_LEX_COMMENT;
        break;
      }
      yylval->lex_str.str=(char*) (lex->ptr=lex->tok_start);// Set to first chr
      yylval->lex_str.length=1;
      c=yyGet();
      if (c != ')')
	lex->next_state= MY_LEX_START;	// Allow signed numbers
      if (c == ',')
	lex->tok_start=lex->ptr;	// Let tok_start point at next item
      return((int) c);

    case MY_LEX_IDENT_OR_NCHAR:
      if (yyPeek() != '\'')
      {					// Found x'hex-number'
	state= MY_LEX_IDENT;
	break;
      }
      yyGet();				// Skip '
      while ((c = yyGet()) && (c !='\'')) ;
      length=(lex->ptr - lex->tok_start);	// Length of hexnum+3
      if (c != '\'')
      {
	return(ABORT_SYM);		// Illegal hex constant
      }
      yyGet();				// get_token makes an unget
      yylval->lex_str=get_token(lex,length);
      yylval->lex_str.str+=2;		// Skip x'
      yylval->lex_str.length-=3;	// Don't count x' and last '
      lex->yytoklen-=3;
      return (NCHAR_STRING);

    case MY_LEX_IDENT_OR_HEX:
      if (yyPeek() == '\'')
      {					// Found x'hex-number'
	state= MY_LEX_HEX_NUMBER;
	break;
      }
      /* Fall through */
    case MY_LEX_IDENT_OR_BIN:		// TODO: Add binary string handling
    case MY_LEX_IDENT:
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(cs))
      {
	result_state= IDENT_QUOTED;
        if (my_mbcharlen(cs, yyGetLast()) > 1)
        {
          int l = my_ismbchar(cs,
                              (const char *)lex->ptr-1,
                              (const char *)lex->end_of_query);
          if (l == 0) {
            state = MY_LEX_CHAR;
            continue;
          }
          lex->ptr += l - 1;
        }
        while (ident_map[c=yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l;
            if ((l = my_ismbchar(cs,
                              (const char *)lex->ptr-1,
                              (const char *)lex->end_of_query)) == 0)
              break;
            lex->ptr += l-1;
          }
        }
      }
      else
#endif
      {
	result_state= bin_ident_map[c] ? IDENT : IDENT_QUOTED;
        while (ident_map[c=yyGet()])
	{
	  /* If not simple character, mark that we must convert it */
	  if (!bin_ident_map[c])
	    result_state= IDENT_QUOTED;
	}
      }
      length= (uint) (lex->ptr - lex->tok_start)-1;
      if (lex->ignore_space)
      {
	for (; state_map[c] == MY_LEX_SKIP ; c= yyGet());
      }
      if (c == '.' && ident_map[yyPeek()])
	lex->next_state=MY_LEX_IDENT_SEP;
      else
      {					// '(' must follow directly if function
	yyUnget();
	if ((tokval = find_keyword(lex,length,c == '(')))
	{
	  lex->next_state= MY_LEX_START;	// Allow signed numbers
	  return(tokval);		// Was keyword
	}
	yySkip();			// next state does a unget
      }
      yylval->lex_str=get_token(lex,length);

      /* 
         Note: "SELECT _bla AS 'alias'"
         _bla should be considered as a IDENT if charset haven't been found.
         So we don't use MYF(MY_WME) with get_charset_by_csname to avoid 
         producing an error.
      */

      if ((yylval->lex_str.str[0]=='_') && 
          (lex->charset=get_charset_by_csname(yylval->lex_str.str+1,
					      MY_CS_PRIMARY,MYF(0))))
        return(UNDERSCORE_CHARSET);
      return(result_state);			// IDENT or IDENT_QUOTED

    case MY_LEX_IDENT_SEP:		// Found ident and now '.'
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      c=yyGet();			// should be '.'
      lex->next_state= MY_LEX_IDENT_START;// Next is an ident (not a keyword)
      if (!ident_map[yyPeek()])		// Probably ` or "
	lex->next_state= MY_LEX_START;
      return((int) c);

    case MY_LEX_NUMBER_IDENT:		// number or ident which num-start
      while (my_isdigit(cs,(c = yyGet()))) ;
      if (!ident_map[c])
      {					// Can't be identifier
	state=MY_LEX_INT_OR_REAL;
	break;
      }
      if (c == 'e' || c == 'E')
      {
	// The following test is written this way to allow numbers of type 1e1
	if (my_isdigit(cs,yyPeek()) || 
            (c=(yyGet())) == '+' || c == '-')
	{				// Allow 1E+10
	  if (my_isdigit(cs,yyPeek()))	// Number must have digit after sign
	  {
	    yySkip();
	    while (my_isdigit(cs,yyGet())) ;
	    yylval->lex_str=get_token(lex,yyLength());
	    return(FLOAT_NUM);
	  }
	}
	yyUnget(); /* purecov: inspected */
      }
      else if (c == 'x' && (lex->ptr - lex->tok_start) == 2 &&
	  lex->tok_start[0] == '0' )
      {						// Varbinary
	while (my_isxdigit(cs,(c = yyGet()))) ;
	if ((lex->ptr - lex->tok_start) >= 4 && !ident_map[c])
	{
	  yylval->lex_str=get_token(lex,yyLength());
	  yylval->lex_str.str+=2;		// Skip 0x
	  yylval->lex_str.length-=2;
	  lex->yytoklen-=2;
	  return (HEX_NUM);
	}
	yyUnget();
      }
      // fall through
    case MY_LEX_IDENT_START:			// We come here after '.'
      result_state= IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(cs))
      {
	result_state= IDENT_QUOTED;
        while (ident_map[c=yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l;
            if ((l = my_ismbchar(cs,
                                 (const char *)lex->ptr-1,
                                 (const char *)lex->end_of_query)) == 0)
              break;
            lex->ptr += l-1;
          }
        }
      }
      else
#endif
        while (ident_map[c = yyGet()])
	{
	  /* If not simple character, mark that we must convert it */
	  if (!bin_ident_map[c])
	    result_state= IDENT_QUOTED;
	}
      if (c == '.' && ident_map[yyPeek()])
	lex->next_state=MY_LEX_IDENT_SEP;// Next is '.'

      yylval->lex_str= get_token(lex,yyLength());
      return(result_state);

    case MY_LEX_USER_VARIABLE_DELIMITER:
    {
      char delim= c;				// Used char
      lex->tok_start=lex->ptr;			// Skip first `
#ifdef USE_MB
      if (use_mb(cs))
      {
	while ((c=yyGet()) && c != delim && c != (uchar) NAMES_SEP_CHAR)
	{
          if (my_mbcharlen(cs, c) > 1)
          {
            int l;
            if ((l = my_ismbchar(cs,
                                 (const char *)lex->ptr-1,
                                 (const char *)lex->end_of_query)) == 0)
              break;
            lex->ptr += l-1;
          }
        }
	yylval->lex_str=get_token(lex,yyLength());
      }
      else
#endif
      {
	uint double_quotes= 0;
	char quote_char= c;
	while ((c=yyGet()))
	{
	  if (c == quote_char)
	  {
	    if (yyPeek() != quote_char)
	      break;
	    c=yyGet();
	    double_quotes++;
	    continue;
	  }
	  if (c == (uchar) NAMES_SEP_CHAR)
	    break;
	}
	if (double_quotes)
	  yylval->lex_str=get_quoted_token(lex,yyLength() - double_quotes,
					   quote_char);
	else
	  yylval->lex_str=get_token(lex,yyLength());
      }
      if (c == delim)
	yySkip();			// Skip end `
      lex->next_state= MY_LEX_START;
      return(IDENT_QUOTED);
    }
    case MY_LEX_INT_OR_REAL:		// Compleat int or incompleat real
      if (c != '.')
      {					// Found complete integer number.
	yylval->lex_str=get_token(lex,yyLength());
	return int_token(yylval->lex_str.str,yylval->lex_str.length);
      }
      // fall through
    case MY_LEX_REAL:			// Incomplete real number
      while (my_isdigit(cs,c = yyGet())) ;

      if (c == 'e' || c == 'E')
      {
	c = yyGet();
	if (c == '-' || c == '+')
	  c = yyGet();			// Skip sign
	if (!my_isdigit(cs,c))
	{				// No digit after sign
	  state= MY_LEX_CHAR;
	  break;
	}
	while (my_isdigit(cs,yyGet())) ;
	yylval->lex_str=get_token(lex,yyLength());
	return(FLOAT_NUM);
      }
      yylval->lex_str=get_token(lex,yyLength());
      return(REAL_NUM);

    case MY_LEX_HEX_NUMBER:		// Found x'hexstring'
      yyGet();				// Skip '
      while (my_isxdigit(cs,(c = yyGet()))) ;
      length=(lex->ptr - lex->tok_start);	// Length of hexnum+3
      if (!(length & 1) || c != '\'')
      {
	return(ABORT_SYM);		// Illegal hex constant
      }
      yyGet();				// get_token makes an unget
      yylval->lex_str=get_token(lex,length);
      yylval->lex_str.str+=2;		// Skip x'
      yylval->lex_str.length-=3;	// Don't count x' and last '
      lex->yytoklen-=3;
      return (HEX_NUM);

    case MY_LEX_CMP_OP:			// Incomplete comparison operator
      if (state_map[yyPeek()] == MY_LEX_CMP_OP ||
	  state_map[yyPeek()] == MY_LEX_LONG_CMP_OP)
	yySkip();
      if ((tokval = find_keyword(lex,(uint) (lex->ptr - lex->tok_start),0)))
      {
	lex->next_state= MY_LEX_START;	// Allow signed numbers
	return(tokval);
      }
      state = MY_LEX_CHAR;		// Something fishy found
      break;

    case MY_LEX_LONG_CMP_OP:		// Incomplete comparison operator
      if (state_map[yyPeek()] == MY_LEX_CMP_OP ||
	  state_map[yyPeek()] == MY_LEX_LONG_CMP_OP)
      {
	yySkip();
	if (state_map[yyPeek()] == MY_LEX_CMP_OP)
	  yySkip();
      }
      if ((tokval = find_keyword(lex,(uint) (lex->ptr - lex->tok_start),0)))
      {
	lex->next_state= MY_LEX_START;	// Found long op
	return(tokval);
      }
      state = MY_LEX_CHAR;		// Something fishy found
      break;

    case MY_LEX_BOOL:
      if (c != yyPeek())
      {
	state=MY_LEX_CHAR;
	break;
      }
      yySkip();
      tokval = find_keyword(lex,2,0);	// Is a bool operator
      lex->next_state= MY_LEX_START;	// Allow signed numbers
      return(tokval);

    case MY_LEX_STRING_OR_DELIMITER:
      if (((THD *) yythd)->variables.sql_mode & MODE_ANSI_QUOTES)
      {
	state= MY_LEX_USER_VARIABLE_DELIMITER;
	break;
      }
      /* " used for strings */
    case MY_LEX_STRING:			// Incomplete text string
      if (!(yylval->lex_str.str = get_text(lex)))
      {
	state= MY_LEX_CHAR;		// Read char by char
	break;
      }
      yylval->lex_str.length=lex->yytoklen;
      return(TEXT_STRING);

    case MY_LEX_COMMENT:			//  Comment
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      while ((c = yyGet()) != '\n' && c) ;
      yyUnget();			// Safety against eof
      state = MY_LEX_START;		// Try again
      break;
    case MY_LEX_LONG_COMMENT:		/* Long C comment? */
      if (yyPeek() != '*')
      {
	state=MY_LEX_CHAR;		// Probable division
	break;
      }
      yySkip();				// Skip '*'
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      if (yyPeek() == '!')		// MySQL command in comment
      {
	ulong version=MYSQL_VERSION_ID;
	yySkip();
	state=MY_LEX_START;
	if (my_isdigit(cs,yyPeek()))
	{				// Version number
	  version=strtol((char*) lex->ptr,(char**) &lex->ptr,10);
	}
	if (version <= MYSQL_VERSION_ID)
	{
	  lex->in_comment=1;
	  break;
	}
      }
      while (lex->ptr != lex->end_of_query &&
	     ((c=yyGet()) != '*' || yyPeek() != '/'))
      {
	if (c == '\n')
	  lex->yylineno++;
      }
      if (lex->ptr != lex->end_of_query)
	yySkip();			// remove last '/'
      state = MY_LEX_START;		// Try again
      break;
    case MY_LEX_END_LONG_COMMENT:
      if (lex->in_comment && yyPeek() == '/')
      {
	yySkip();
	lex->in_comment=0;
	state=MY_LEX_START;
      }
      else
	state=MY_LEX_CHAR;		// Return '*'
      break;
    case MY_LEX_SET_VAR:		// Check if ':='
      if (yyPeek() != '=')
      {
	state=MY_LEX_CHAR;		// Return ':'
	break;
      }
      yySkip();
      return (SET_VAR);
    case MY_LEX_COLON:			// optional line terminator
      if (yyPeek())
      {
        if (((THD *)yythd)->client_capabilities & CLIENT_MULTI_QUERIES)
        {
          lex->found_colon=(char*)lex->ptr;
          ((THD *)yythd)->server_status |= SERVER_MORE_RESULTS_EXISTS;
          lex->next_state=MY_LEX_END;
          return(END_OF_INPUT);
        }
        else
 	  state=MY_LEX_CHAR;		// Return ';'
	break;
      }
      /* fall true */
    case MY_LEX_EOL:
      lex->next_state=MY_LEX_END;	// Mark for next loop
      return(END_OF_INPUT);
    case MY_LEX_END:
      lex->next_state=MY_LEX_END;
      return(0);			// We found end of input last time
      
      /* Actually real shouldn't start with . but allow them anyhow */
    case MY_LEX_REAL_OR_POINT:
      if (my_isdigit(cs,yyPeek()))
	state = MY_LEX_REAL;		// Real
      else
      {
	state= MY_LEX_IDENT_SEP;	// return '.'
	yyUnget();			// Put back '.'
      }
      break;
    case MY_LEX_USER_END:		// end '@' of user@hostname
      switch (state_map[yyPeek()]) {
      case MY_LEX_STRING:
      case MY_LEX_USER_VARIABLE_DELIMITER:
      case MY_LEX_STRING_OR_DELIMITER:
	break;
      case MY_LEX_USER_END:
	lex->next_state=MY_LEX_SYSTEM_VAR;
	break;
      default:
	lex->next_state=MY_LEX_HOSTNAME;
	break;
      }
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      return((int) '@');
    case MY_LEX_HOSTNAME:		// end '@' of user@hostname
      for (c=yyGet() ; 
	   my_isalnum(cs,c) || c == '.' || c == '_' ||  c == '$';
	   c= yyGet()) ;
      yylval->lex_str=get_token(lex,yyLength());
      return(LEX_HOSTNAME);
    case MY_LEX_SYSTEM_VAR:
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      yySkip();					// Skip '@'
      lex->next_state= (state_map[yyPeek()] ==
			MY_LEX_USER_VARIABLE_DELIMITER ?
			MY_LEX_OPERATOR_OR_IDENT :
			MY_LEX_IDENT_OR_KEYWORD);
      return((int) '@');
    case MY_LEX_IDENT_OR_KEYWORD:
      /*
	We come here when we have found two '@' in a row.
	We should now be able to handle:
	[(global | local | session) .]variable_name
      */
      result_state= IDENT;
      while (ident_map[c=yyGet()])
      {
	/* If not simple character, mark that we must convert it */
	if (!bin_ident_map[c])
	  result_state= IDENT_QUOTED;
      }
      if (c == '.')
	lex->next_state=MY_LEX_IDENT_SEP;
      length= (uint) (lex->ptr - lex->tok_start)-1;
      if ((tokval= find_keyword(lex,length,0)))
      {
	yyUnget();				// Put back 'c'
	return(tokval);				// Was keyword
      }
      yylval->lex_str=get_token(lex,length);
      return(result_state);
    }
  }
}

/*
  st_select_lex structures initialisations
*/

void st_select_lex_node::init_query()
{
  options= 0;
  linkage= UNSPECIFIED_TYPE;
  no_error= no_table_names_allowed= uncacheable= dependent= 0;
}

void st_select_lex_node::init_select()
{
}

void st_select_lex_unit::init_query()
{
  st_select_lex_node::init_query();
  linkage= GLOBAL_OPTIONS_TYPE;
  global_parameters= first_select();
  select_limit_cnt= HA_POS_ERROR;
  offset_limit_cnt= 0;
  union_option= 0;
  prepared= optimized= executed= 0;
  item= 0;
  union_result= 0;
  table= 0;
  fake_select_lex= 0;
}

void st_select_lex::init_query()
{
  st_select_lex_node::init_query();
  table_list.empty();
  item_list.empty();
  join= 0;
  where= 0;
  olap= UNSPECIFIED_OLAP_TYPE;
  having_fix_field= 0;
  resolve_mode= NOMATTER_MODE;
  cond_count= with_wild= 0;
  ref_pointer_array= 0;
  select_n_having_items= 0;
  prep_where= 0;
}

void st_select_lex::init_select()
{
  st_select_lex_node::init_select();
  group_list.empty();
  type= db= db1= table1= db2= table2= 0;
  having= 0;
  use_index_ptr= ignore_index_ptr= 0;
  table_join_options= 0;
  in_sum_expr= with_wild= 0;
  options= 0;
  braces= 0;
  when_list.empty(); 
  expr_list.empty();
  interval_list.empty(); 
  use_index.empty();
  ftfunc_list_alloc.empty();
  ftfunc_list= &ftfunc_list_alloc;
  linkage= UNSPECIFIED_TYPE;
  order_list.elements= 0;
  order_list.first= 0;
  order_list.next= (byte**) &order_list.first;
  select_limit= HA_POS_ERROR;
  offset_limit= 0;
  with_sum_func= 0;
  parsing_place= SELECT_LEX_NODE::NO_MATTER;
}

/*
  st_select_lex structures linking
*/

/* include on level down */
void st_select_lex_node::include_down(st_select_lex_node *upper)
{
  if ((next= upper->slave))
    next->prev= &next;
  prev= &upper->slave;
  upper->slave= this;
  master= upper;
  slave= 0;
}

/*
  include on level down (but do not link)
  
  SYNOPSYS
    st_select_lex_node::include_standalone()
    upper - reference on node underr which this node should be included
    ref - references on reference on this node
*/
void st_select_lex_node::include_standalone(st_select_lex_node *upper,
					    st_select_lex_node **ref)
{
  next= 0;
  prev= ref;
  master= upper;
  slave= 0;
}

/* include neighbour (on same level) */
void st_select_lex_node::include_neighbour(st_select_lex_node *before)
{
  if ((next= before->next))
    next->prev= &next;
  prev= &before->next;
  before->next= this;
  master= before->master;
  slave= 0;
}

/* including in global SELECT_LEX list */
void st_select_lex_node::include_global(st_select_lex_node **plink)
{
  if ((link_next= *plink))
    link_next->link_prev= &link_next;
  link_prev= plink;
  *plink= this;
}

//excluding from global list (internal function)
void st_select_lex_node::fast_exclude()
{
  if (link_prev)
  {
    if ((*link_prev= link_next))
      link_next->link_prev= link_prev;
  }
  // Remove slave structure
  for (; slave; slave= slave->next)
    slave->fast_exclude();
  
}

/*
  excluding select_lex structure (except first (first select can't be
  deleted, because it is most upper select))
*/
void st_select_lex_node::exclude()
{
  //exclude from global list
  fast_exclude();
  //exclude from other structures
  if ((*prev= next))
    next->prev= prev;
  /* 
     We do not need following statements, because prev pointer of first 
     list element point to master->slave
     if (master->slave == this)
       master->slave= next;
  */
}


/*
  Exclude level of current unit from tree of SELECTs

  SYNOPSYS
    st_select_lex_unit::exclude_level()

  NOTE: units which belong to current will be brought up on level of
  currernt unit 
*/
void st_select_lex_unit::exclude_level()
{
  SELECT_LEX_UNIT *units= 0, **units_last= &units;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    // unlink current level from global SELECTs list
    if (sl->link_prev && (*sl->link_prev= sl->link_next))
      sl->link_next->link_prev= sl->link_prev;

    // bring up underlay levels
    SELECT_LEX_UNIT **last= 0;
    for (SELECT_LEX_UNIT *u= sl->first_inner_unit(); u; u= u->next_unit())
    {
      u->master= master;
      last= (SELECT_LEX_UNIT**)&(u->next);
    }
    if (last)
    {
      (*units_last)= sl->first_inner_unit();
      units_last= last;
    }
  }
  if (units)
  {
    // include brought up levels in place of current
    (*prev)= units;
    (*units_last)= (SELECT_LEX_UNIT*)next;
    if (next)
      next->prev= (SELECT_LEX_NODE**)units_last;
    units->prev= prev;
  }
  else
  {
    // exclude currect unit from list of nodes
    (*prev)= next;
    if (next)
      next->prev= prev;
  }
}


/*
  Exclude subtree of current unit from tree of SELECTs

  SYNOPSYS
    st_select_lex_unit::exclude_tree()
*/
void st_select_lex_unit::exclude_tree()
{
  SELECT_LEX_UNIT *units= 0, **units_last= &units;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    // unlink current level from global SELECTs list
    if (sl->link_prev && (*sl->link_prev= sl->link_next))
      sl->link_next->link_prev= sl->link_prev;

    // unlink underlay levels
    for (SELECT_LEX_UNIT *u= sl->first_inner_unit(); u; u= u->next_unit())
    {
      u->exclude_level();
    }
  }
  // exclude currect unit from list of nodes
  (*prev)= next;
  if (next)
    next->prev= prev;
}


/*
  st_select_lex_node::mark_as_dependent mark all st_select_lex struct from 
  this to 'last' as dependent

  SYNOPSIS
    last - pointer to last st_select_lex struct, before wich all 
           st_select_lex have to be marked as dependent

  NOTE
    'last' should be reachable from this st_select_lex_node
*/

void st_select_lex::mark_as_dependent(SELECT_LEX *last)
{
  /*
    Mark all selects from resolved to 1 before select where was
    found table as depended (of select where was found table)
  */
  for (SELECT_LEX *s= this;
       s && s != last;
       s= s->outer_select())
    if ( !s->dependent )
    {
      // Select is dependent of outer select
      s->dependent= s->uncacheable= 1;
      SELECT_LEX_UNIT *munit= s->master_unit();
      munit->dependent= munit->uncacheable= 1;
      //Tables will be reopened many times
      for (TABLE_LIST *tbl= s->get_table_list();
	   tbl;
	   tbl= tbl->next)
	tbl->shared= 1;
    }
}

bool st_select_lex_node::set_braces(bool value)      { return 1; }
bool st_select_lex_node::inc_in_sum_expr()           { return 1; }
uint st_select_lex_node::get_in_sum_expr()           { return 0; }
TABLE_LIST* st_select_lex_node::get_table_list()     { return 0; }
List<Item>* st_select_lex_node::get_item_list()      { return 0; }
List<String>* st_select_lex_node::get_use_index()    { return 0; }
List<String>* st_select_lex_node::get_ignore_index() { return 0; }
TABLE_LIST *st_select_lex_node::add_table_to_list(THD *thd, Table_ident *table,
						  LEX_STRING *alias,
						  ulong table_join_options,
						  thr_lock_type flags,
						  List<String> *use_index,
						  List<String> *ignore_index,
                                                  LEX_STRING *option)
{
  return 0;
}
ulong st_select_lex_node::get_table_join_options()
{
  return 0;
}

/*
  prohibit using LIMIT clause
*/
bool st_select_lex::test_limit()
{
  if (select_limit != HA_POS_ERROR)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
         "LIMIT & IN/ALL/ANY/SOME subquery");
    return(1);
  }
  // We need only 1 row to determinate existence
  select_limit= 1;
  // no sense in ORDER BY without LIMIT
  order_list.empty();
  return(0);
}























/*  
  Interface method of table list creation for query
  
  SYNOPSIS
    st_select_lex_unit::create_total_list()
    thd            THD pointer
    result         pointer on result list of tables pointer
    check_derived  force derived table chacking (used for creating 
                   table list for derived query)
  DESCRIPTION
    This is used for UNION & subselect to create a new table list of all used 
    tables.
    The table_list->table entry in all used tables are set to point
    to the entries in this list.

  RETURN
    0 - OK
    !0 - error
*/
bool st_select_lex_unit::create_total_list(THD *thd, st_lex *lex,
					   TABLE_LIST **result,
					   bool check_derived)
{
  *result= 0;
  for (SELECT_LEX_UNIT *unit= this; unit; unit= unit->next_unit())
  {
    if ((res= unit->create_total_list_n_last_return(thd, lex, &result,
						    check_derived)))
      return res;
  }
  return 0;
}

/*  
  Table list creation for query
  
  SYNOPSIS
    st_select_lex_unit::create_total_list()
    thd            THD pointer
    lex            pointer on LEX stricture
    result         pointer on pointer on result list of tables pointer
    check_derived  force derived table chacking (used for creating 
                   table list for derived query)
  DESCRIPTION
    This is used for UNION & subselect to create a new table list of all used 
    tables.
    The table_list->table entry in all used tables are set to point
    to the entries in this list.

  RETURN
    0 - OK
    !0 - error
*/
bool st_select_lex_unit::create_total_list_n_last_return(THD *thd, st_lex *lex,
							 TABLE_LIST ***result,
							 bool check_derived)
{
  TABLE_LIST *slave_list_first=0, **slave_list_last= &slave_list_first;
  TABLE_LIST **new_table_list= *result, *aux;
  SELECT_LEX *sl= (SELECT_LEX*)slave;
  
  /*
    iterate all inner selects + fake_select (if exists),
    fake_select->next_select() always is 0
  */
  for (;
       sl;
       sl= (sl->next_select() ?
	    sl->next_select() :
	    (sl == fake_select_lex ?
	     0 :
	     fake_select_lex)))
  {
    // check usage of ORDER BY in union
    if (sl->order_list.first && sl->next_select() && !sl->braces &&
	sl->linkage != GLOBAL_OPTIONS_TYPE)
    {
      net_printf(thd,ER_WRONG_USAGE,"UNION","ORDER BY");
      return 1;
    }

    if (sl->linkage == DERIVED_TABLE_TYPE && !check_derived)
      goto end;

    for (SELECT_LEX_UNIT *inner=  sl->first_inner_unit();
	 inner;
	 inner= inner->next_unit())
    {
      if (inner->create_total_list_n_last_return(thd, lex,
						 &slave_list_last, 0))
	return 1;
    }

    if ((aux= (TABLE_LIST*) sl->table_list.first))
    {
      TABLE_LIST *next;
      for (; aux; aux= next)
      {
	TABLE_LIST *cursor;
	next= aux->next;
	for (cursor= **result; cursor; cursor= cursor->next)
	  if (!strcmp(cursor->db, aux->db) &&
	      !strcmp(cursor->real_name, aux->real_name) &&
	      !strcmp(cursor->alias, aux->alias))
	    break;
	if (!cursor)
	{
	  /* Add not used table to the total table list */
	  if (!(cursor= (TABLE_LIST *) thd->memdup((char*) aux,
						   sizeof(*aux))))
	  {
	    send_error(thd,0);
	    return 1;
	  }
	  *new_table_list= cursor;
	  cursor->table_list= aux; //to be able mark this table as shared
	  new_table_list= &cursor->next;
	  *new_table_list= 0;			// end result list
	}
	else
	  // Mark that it's used twice
	  cursor->table_list->shared= aux->shared= 1;
	aux->table_list= cursor;
      }
    }
  }
end:
  if (slave_list_first)
  {
    *new_table_list= slave_list_first;
    new_table_list= slave_list_last;
  }
  *result= new_table_list;
  return 0;
}

st_select_lex_unit* st_select_lex_unit::master_unit()
{
    return this;
}

st_select_lex* st_select_lex_unit::outer_select()
{
  return (st_select_lex*) master;
}

bool st_select_lex::add_order_to_list(THD *thd, Item *item, bool asc)
{
  return add_to_list(thd, order_list, item, asc);
}

bool st_select_lex::add_item_to_list(THD *thd, Item *item)
{
  return item_list.push_back(item);
}

bool st_select_lex::add_group_to_list(THD *thd, Item *item, bool asc)
{
  return add_to_list(thd, group_list, item, asc);
}

bool st_select_lex::add_ftfunc_to_list(Item_func_match *func)
{
  return !func || ftfunc_list->push_back(func); // end of memory?
}

st_select_lex_unit* st_select_lex::master_unit()
{
  return (st_select_lex_unit*) master;
}

st_select_lex* st_select_lex::outer_select()
{
  return (st_select_lex*) master->get_master();
}

bool st_select_lex::set_braces(bool value)
{
  braces= value;
  return 0; 
}

bool st_select_lex::inc_in_sum_expr()
{
  in_sum_expr++;
  return 0;
}

uint st_select_lex::get_in_sum_expr()
{
  return in_sum_expr;
}

TABLE_LIST* st_select_lex::get_table_list()
{
  return (TABLE_LIST*) table_list.first;
}

List<Item>* st_select_lex::get_item_list()
{
  return &item_list;
}

List<String>* st_select_lex::get_use_index()
{
  return use_index_ptr;
}

List<String>* st_select_lex::get_ignore_index()
{
  return ignore_index_ptr;
}

ulong st_select_lex::get_table_join_options()
{
  return table_join_options;
}

bool st_select_lex::setup_ref_array(THD *thd, uint order_group_num)
{
  if (ref_pointer_array)
    return 0;
  return (ref_pointer_array= 
	  (Item **)thd->alloc(sizeof(Item*) *
			      (item_list.elements +
			       select_n_having_items +
			       order_group_num)* 5)) == 0;
}

void st_select_lex_unit::print(String *str)
{
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl != first_select())
    {
      str->append(" union ", 7);
      if (union_option & UNION_ALL)
	str->append("all ", 4);
    }
    if (sl->braces)
      str->append('(');
    sl->print(thd, str);
    if (sl->braces)
      str->append(')');
  }
  if (fake_select_lex == global_parameters)
  {
    if (fake_select_lex->order_list.elements)
    {
      str->append(" order by ", 10);
      fake_select_lex->print_order(str,
				   (ORDER *) fake_select_lex->
				   order_list.first);
    }
    fake_select_lex->print_limit(thd, str);
  }
}


void st_select_lex::print_order(String *str, ORDER *order)
{
  for (; order; order= order->next)
  {
    (*order->item)->print(str);
    if (!order->asc)
      str->append(" desc", 5);
    if (order->next)
      str->append(',');
  }
}
 
void st_select_lex::print_limit(THD *thd, String *str)
{
  if (!thd)
    thd= current_thd;

  if (select_limit != thd->variables.select_limit ||
      select_limit != HA_POS_ERROR ||
      offset_limit != 0L)
  {
    str->append(" limit ", 7);
    char buff[20];
    // latin1 is good enough for numbers
    String st(buff, sizeof(buff),  &my_charset_latin1);
    st.set((ulonglong)select_limit, &my_charset_latin1);
    str->append(st);
    if (offset_limit)
    {
      str->append(',');
      st.set((ulonglong)select_limit, &my_charset_latin1);
      str->append(st);
    }
  }
}

/*
  There are st_select_lex::add_table_to_list & 
  st_select_lex::set_lock_for_tables in sql_parse.cc

  st_select_lex::print is in sql_select.h
*/
