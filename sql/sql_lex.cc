/* Copyright (C) 2000-2006 MySQL AB

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


/* A lexical scanner on a temporary buffer with a yacc interface */

#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "item_create.h"
#include <m_ctype.h>
#include <hash.h>
#include "sp.h"
#include "sp_head.h"

/*
  We are using pointer to this variable for distinguishing between assignment
  to NEW row field (when parsing trigger definition) and structured variable.
*/
sys_var_long_ptr trg_new_row_fake_var(0, 0);

/* Macros to look like lex */

#define yyGet()		*(lip->ptr++)
#define yyGetLast()	lip->ptr[-1]
#define yyPeek()	lip->ptr[0]
#define yyPeek2()	lip->ptr[1]
#define yyUnget()	lip->ptr--
#define yySkip()	lip->ptr++
#define yyLength()	((uint) (lip->ptr - lip->tok_start)-1)

/* Longest standard keyword name */
#define TOCK_NAME_LENGTH 24

/*
  The following data is based on the latin1 character set, and is only
  used when comparing keywords
*/

static uchar to_upper_lex[]=
{
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

  DBUG_VOID_RETURN;
}


void lex_free(void)
{					// Call this when daemon ends
  DBUG_ENTER("lex_free");
  DBUG_VOID_RETURN;
}


void
st_parsing_options::reset()
{
  allows_variable= TRUE;
  allows_select_into= TRUE;
  allows_select_procedure= TRUE;
  allows_derived= TRUE;
}

Lex_input_stream::Lex_input_stream(THD *thd,
                                   const char* buffer,
                                   unsigned int length)
: m_thd(thd),
  yylineno(1),
  yytoklen(0),
  yylval(NULL),
  ptr(buffer),
  tok_start(NULL),
  tok_end(NULL),
  end_of_query(buffer + length),
  tok_start_prev(NULL),
  buf(buffer),
  next_state(MY_LEX_START),
  found_semicolon(NULL),
  ignore_space(test(thd->variables.sql_mode & MODE_IGNORE_SPACE)),
  stmt_prepare_mode(FALSE)
{
}

Lex_input_stream::~Lex_input_stream()
{}


/*
  This is called before every query that is to be parsed.
  Because of this, it's critical to not do too much things here.
  (We already do too much here)
*/

void lex_start(THD *thd)
{
  LEX *lex= thd->lex;
  DBUG_ENTER("lex_start");

  lex->thd= lex->unit.thd= thd;

  lex->context_stack.empty();
  lex->unit.init_query();
  lex->unit.init_select();
  /* 'parent_lex' is used in init_query() so it must be before it. */
  lex->select_lex.parent_lex= lex;
  lex->select_lex.init_query();
  lex->value_list.empty();
  lex->update_list.empty();
  lex->param_list.empty();
  lex->view_list.empty();
  lex->prepared_stmt_params.empty();
  lex->auxiliary_table_list.empty();
  lex->unit.next= lex->unit.master=
    lex->unit.link_next= lex->unit.return_to= 0;
  lex->unit.prev= lex->unit.link_prev= 0;
  lex->unit.slave= lex->unit.global_parameters= lex->current_select=
    lex->all_selects_list= &lex->select_lex;
  lex->select_lex.master= &lex->unit;
  lex->select_lex.prev= &lex->unit.slave;
  lex->select_lex.link_next= lex->select_lex.slave= lex->select_lex.next= 0;
  lex->select_lex.link_prev= (st_select_lex_node**)&(lex->all_selects_list);
  lex->select_lex.options= 0;
  lex->select_lex.sql_cache= SELECT_LEX::SQL_CACHE_UNSPECIFIED;
  lex->select_lex.init_order();
  lex->select_lex.group_list.empty();
  lex->describe= 0;
  lex->subqueries= FALSE;
  lex->view_prepare_mode= FALSE;
  lex->derived_tables= 0;
  lex->lock_option= TL_READ;
  lex->safe_to_cache_query= 1;
  lex->time_zone_tables_used= 0;
  lex->leaf_tables_insert= 0;
  lex->parsing_options.reset();
  lex->empty_field_list_on_rset= 0;
  lex->select_lex.select_number= 1;

  lex->in_comment=0;
  lex->length=0;
  lex->select_lex.in_sum_expr=0;
  lex->select_lex.expr_list.empty();
  lex->select_lex.ftfunc_list_alloc.empty();
  lex->select_lex.ftfunc_list= &lex->select_lex.ftfunc_list_alloc;
  lex->select_lex.group_list.empty();
  lex->select_lex.order_list.empty();
  lex->select_lex.udf_list.empty();
  lex->current_select= &lex->select_lex;
  lex->yacc_yyss=lex->yacc_yyvs=0;
  lex->sql_command= lex->orig_sql_command= SQLCOM_END;
  lex->duplicates= DUP_ERROR;
  lex->ignore= 0;
  lex->sphead= NULL;
  lex->spcont= NULL;
  lex->proc_list.first= 0;
  lex->escape_used= FALSE;
  lex->reset_query_tables_list(FALSE);

  lex->nest_level=0 ;
  lex->allow_sum_func= 0;
  lex->in_sum_func= NULL;
  DBUG_VOID_RETURN;
}

void lex_end(LEX *lex)
{
  DBUG_ENTER("lex_end");
  DBUG_PRINT("enter", ("lex: 0x%lx", (long) lex));
  x_free(lex->yacc_yyss);
  x_free(lex->yacc_yyvs);
  DBUG_VOID_RETURN;
}


static int find_keyword(Lex_input_stream *lip, uint len, bool function)
{
  const char *tok= lip->tok_start;

  SYMBOL *symbol= get_hash_symbol(tok, len, function);
  if (symbol)
  {
    lip->yylval->symbol.symbol=symbol;
    lip->yylval->symbol.str= (char*) tok;
    lip->yylval->symbol.length=len;
    
    if ((symbol->tok == NOT_SYM) &&
        (lip->m_thd->variables.sql_mode & MODE_HIGH_NOT_PRECEDENCE))
      return NOT2_SYM;
    if ((symbol->tok == OR_OR_SYM) &&
	!(lip->m_thd->variables.sql_mode & MODE_PIPES_AS_CONCAT))
      return OR2_SYM;
    
    return symbol->tok;
  }
  return 0;
}

/*
  Check if name is a keyword

  SYNOPSIS
    is_keyword()
    name      checked name (must not be empty)
    len       length of checked name

  RETURN VALUES
    0         name is a keyword
    1         name isn't a keyword
*/

bool is_keyword(const char *name, uint len)
{
  DBUG_ASSERT(len != 0);
  return get_hash_symbol(name,len,0)!=0;
}

/* make a copy of token before ptr and set yytoklen */

static LEX_STRING get_token(Lex_input_stream *lip, uint skip, uint length)
{
  LEX_STRING tmp;
  yyUnget();			// ptr points now after last token char
  tmp.length=lip->yytoklen=length;
  tmp.str= lip->m_thd->strmake(lip->tok_start + skip, tmp.length);
  return tmp;
}

/* 
 todo: 
   There are no dangerous charsets in mysql for function 
   get_quoted_token yet. But it should be fixed in the 
   future to operate multichar strings (like ucs2)
*/

static LEX_STRING get_quoted_token(Lex_input_stream *lip,
                                   uint skip,
                                   uint length, char quote)
{
  LEX_STRING tmp;
  byte *from, *to, *end;
  yyUnget();			// ptr points now after last token char
  tmp.length=lip->yytoklen=length;
  tmp.str=(char*) lip->m_thd->alloc(tmp.length+1);
  from= (byte*) lip->tok_start + skip;
  to= (byte*) tmp.str;
  end= to+length;
  for ( ; to != end; )
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

static char *get_text(Lex_input_stream *lip)
{
  reg1 uchar c,sep;
  uint found_escape=0;
  CHARSET_INFO *cs= lip->m_thd->charset();

  lip->tok_bitmap= 0;
  sep= yyGetLast();			// String should end with this
  while (lip->ptr != lip->end_of_query)
  {
    c= yyGet();
    lip->tok_bitmap|= c;
#ifdef USE_MB
    {
      int l;
      if (use_mb(cs) &&
          (l = my_ismbchar(cs,
                           lip->ptr-1,
                           lip->end_of_query))) {
	lip->ptr += l-1;
	continue;
      }
    }
#endif
    if (c == '\\' &&
	!(lip->m_thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES))
    {					// Escaped character
      found_escape=1;
      if (lip->ptr == lip->end_of_query)
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
      const char *str;
      const char *end;
      char *start;

      str=lip->tok_start+1;
      end=lip->ptr-1;
      if (!(start=(char*) lip->m_thd->alloc((uint) (end-str)+1)))
	return (char*) "";		// Sql_alloc has set error flag
      if (!found_escape)
      {
	lip->yytoklen=(uint) (end-str);
	memcpy(start,str,lip->yytoklen);
	start[lip->yytoklen]=0;
      }
      else
      {
	char *to;

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
	  if (!(lip->m_thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES) &&
              *str == '\\' && str+1 != end)
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
              *to++= *str;
	      break;
	    }
	  }
	  else if (*str == sep)
	    *to++= *str++;		// Two ' or "
	  else
	    *to++ = *str;
	}
	*to=0;
	lip->yytoklen=(uint) (to-start);
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

static inline uint int_token(const char *str,uint length)
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
      return DECIMAL_NUM;
    else
    {
      cmp=signed_longlong_str+1;
      smaller=LONG_NUM;				// If <= signed_longlong_str
      bigger=DECIMAL_NUM;
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
        return DECIMAL_NUM;
      cmp=unsigned_longlong_str;
      smaller=ULONGLONG_NUM;
      bigger=DECIMAL_NUM;
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
  MYSQLlex remember the following states from the following MYSQLlex()

  - MY_LEX_EOQ			Found end of query
  - MY_LEX_OPERATOR_OR_IDENT	Last state was an ident, text or number
				(which can't be followed by a signed number)
*/

int MYSQLlex(void *arg, void *yythd)
{
  reg1	uchar c;
  bool comment_closed;
  int	tokval, result_state;
  uint length;
  enum my_lex_states state;
  THD *thd= (THD *)yythd;
  Lex_input_stream *lip= thd->m_lip;
  LEX *lex= thd->lex;
  YYSTYPE *yylval=(YYSTYPE*) arg;
  CHARSET_INFO *cs= thd->charset();
  uchar *state_map= cs->state_map;
  uchar *ident_map= cs->ident_map;

  lip->yylval=yylval;			// The global state

  lip->tok_start_prev= lip->tok_start;

  lip->tok_start=lip->tok_end=lip->ptr;
  state=lip->next_state;
  lip->next_state=MY_LEX_OPERATOR_OR_IDENT;
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
	  lip->yylineno++;
      }
      lip->tok_start=lip->ptr-1;	// Start of real token
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
      yylval->lex_str.str=(char*) (lip->ptr=lip->tok_start);// Set to first chr
      yylval->lex_str.length=1;
      c=yyGet();
      if (c != ')')
	lip->next_state= MY_LEX_START;	// Allow signed numbers
      if (c == ',')
	lip->tok_start=lip->ptr;	// Let tok_start point at next item
      /*
        Check for a placeholder: it should not precede a possible identifier
        because of binlogging: when a placeholder is replaced with
        its value in a query for the binlog, the query must stay
        grammatically correct.
      */
      else if (c == '?' && lip->stmt_prepare_mode && !ident_map[yyPeek()])
        return(PARAM_MARKER);
      return((int) c);

    case MY_LEX_IDENT_OR_NCHAR:
      if (yyPeek() != '\'')
      {					
	state= MY_LEX_IDENT;
	break;
      }
      /* Found N'string' */
      lip->tok_start++;                 // Skip N
      yySkip();                         // Skip '
      if (!(yylval->lex_str.str = get_text(lip)))
      {
	state= MY_LEX_CHAR;             // Read char by char
	break;
      }
      yylval->lex_str.length= lip->yytoklen;
      lex->text_string_is_7bit= (lip->tok_bitmap & 0x80) ? 0 : 1;
      return(NCHAR_STRING);

    case MY_LEX_IDENT_OR_HEX:
      if (yyPeek() == '\'')
      {					// Found x'hex-number'
	state= MY_LEX_HEX_NUMBER;
	break;
      }
    case MY_LEX_IDENT_OR_BIN:
      if (yyPeek() == '\'')
      {                                 // Found b'bin-number'
        state= MY_LEX_BIN_NUMBER;
        break;
      }
    case MY_LEX_IDENT:
      const char *start;
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(cs))
      {
	result_state= IDENT_QUOTED;
        if (my_mbcharlen(cs, yyGetLast()) > 1)
        {
          int l = my_ismbchar(cs,
                              lip->ptr-1,
                              lip->end_of_query);
          if (l == 0) {
            state = MY_LEX_CHAR;
            continue;
          }
          lip->ptr += l - 1;
        }
        while (ident_map[c=yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l;
            if ((l = my_ismbchar(cs,
                                 lip->ptr-1,
                                 lip->end_of_query)) == 0)
              break;
            lip->ptr += l-1;
          }
        }
      }
      else
#endif
      {
        for (result_state= c; ident_map[c= yyGet()]; result_state|= c);
        /* If there were non-ASCII characters, mark that we must convert */
        result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      }
      length= (uint) (lip->ptr - lip->tok_start)-1;
      start= lip->ptr;
      if (lip->ignore_space)
      {
        /*
          If we find a space then this can't be an identifier. We notice this
          below by checking start != lex->ptr.
        */
        for (; state_map[c] == MY_LEX_SKIP ; c= yyGet());
      }
      if (start == lip->ptr && c == '.' && ident_map[yyPeek()])
	lip->next_state=MY_LEX_IDENT_SEP;
      else
      {					// '(' must follow directly if function
	yyUnget();
	if ((tokval = find_keyword(lip, length, c == '(')))
	{
	  lip->next_state= MY_LEX_START;	// Allow signed numbers
	  return(tokval);		// Was keyword
	}
	yySkip();			// next state does a unget
      }
      yylval->lex_str=get_token(lip, 0, length);

      /* 
         Note: "SELECT _bla AS 'alias'"
         _bla should be considered as a IDENT if charset haven't been found.
         So we don't use MYF(MY_WME) with get_charset_by_csname to avoid 
         producing an error.
      */

      if ((yylval->lex_str.str[0]=='_') && 
          (lex->underscore_charset=
             get_charset_by_csname(yylval->lex_str.str + 1,
                                   MY_CS_PRIMARY,MYF(0))))
        return(UNDERSCORE_CHARSET);
      return(result_state);			// IDENT or IDENT_QUOTED

    case MY_LEX_IDENT_SEP:		// Found ident and now '.'
      yylval->lex_str.str=(char*) lip->ptr;
      yylval->lex_str.length=1;
      c=yyGet();			// should be '.'
      lip->next_state= MY_LEX_IDENT_START;// Next is an ident (not a keyword)
      if (!ident_map[yyPeek()])		// Probably ` or "
	lip->next_state= MY_LEX_START;
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
	    yylval->lex_str=get_token(lip, 0, yyLength());
	    return(FLOAT_NUM);
	  }
	}
	yyUnget(); /* purecov: inspected */
      }
      else if (c == 'x' && (lip->ptr - lip->tok_start) == 2 &&
	  lip->tok_start[0] == '0' )
      {						// Varbinary
	while (my_isxdigit(cs,(c = yyGet()))) ;
	if ((lip->ptr - lip->tok_start) >= 4 && !ident_map[c])
	{
          /* skip '0x' */
	  yylval->lex_str=get_token(lip, 2, yyLength()-2);
	  return (HEX_NUM);
	}
	yyUnget();
      }
      else if (c == 'b' && (lip->ptr - lip->tok_start) == 2 &&
               lip->tok_start[0] == '0' )
      {						// b'bin-number'
	while (my_isxdigit(cs,(c = yyGet()))) ;
	if ((lip->ptr - lip->tok_start) >= 4 && !ident_map[c])
	{
          /* Skip '0b' */
	  yylval->lex_str= get_token(lip, 2, yyLength()-2);
	  return (BIN_NUM);
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
                                 lip->ptr-1,
                                 lip->end_of_query)) == 0)
              break;
            lip->ptr += l-1;
          }
        }
      }
      else
#endif
      {
        for (result_state=0; ident_map[c= yyGet()]; result_state|= c);
        /* If there were non-ASCII characters, mark that we must convert */
        result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      }
      if (c == '.' && ident_map[yyPeek()])
	lip->next_state=MY_LEX_IDENT_SEP;// Next is '.'

      yylval->lex_str= get_token(lip, 0, yyLength());
      return(result_state);

    case MY_LEX_USER_VARIABLE_DELIMITER:	// Found quote char
    {
      uint double_quotes= 0;
      char quote_char= c;                       // Used char
      while ((c=yyGet()))
      {
	int var_length;
	if ((var_length= my_mbcharlen(cs, c)) == 1)
	{
	  if (c == (uchar) NAMES_SEP_CHAR)
	    break; /* Old .frm format can't handle this char */
	  if (c == quote_char)
	  {
	    if (yyPeek() != quote_char)
	      break;
	    c=yyGet();
	    double_quotes++;
	    continue;
	  }
	}
#ifdef USE_MB
	else if (var_length < 1)
	  break;				// Error
	lip->ptr+= var_length-1;
#endif
      }
      if (double_quotes)
	yylval->lex_str=get_quoted_token(lip, 1,
                                         yyLength() - double_quotes -1,
					 quote_char);
      else
	yylval->lex_str=get_token(lip, 1, yyLength() -1);
      if (c == quote_char)
	yySkip();			// Skip end `
      lip->next_state= MY_LEX_START;
      return(IDENT_QUOTED);
    }
    case MY_LEX_INT_OR_REAL:		// Complete int or incomplete real
      if (c != '.')
      {					// Found complete integer number.
	yylval->lex_str=get_token(lip, 0, yyLength());
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
	yylval->lex_str=get_token(lip, 0, yyLength());
	return(FLOAT_NUM);
      }
      yylval->lex_str=get_token(lip, 0, yyLength());
      return(DECIMAL_NUM);

    case MY_LEX_HEX_NUMBER:		// Found x'hexstring'
      yyGet();				// Skip '
      while (my_isxdigit(cs,(c = yyGet()))) ;
      length=(lip->ptr - lip->tok_start);	// Length of hexnum+3
      if (!(length & 1) || c != '\'')
      {
	return(ABORT_SYM);		// Illegal hex constant
      }
      yyGet();				// get_token makes an unget
      yylval->lex_str=get_token(lip,
                                2,          // skip x'
                                length-3);  // don't count x' and last '
      return (HEX_NUM);

    case MY_LEX_BIN_NUMBER:           // Found b'bin-string'
      yyGet();                                // Skip '
      while ((c= yyGet()) == '0' || c == '1');
      length= (lip->ptr - lip->tok_start);    // Length of bin-num + 3
      if (c != '\'')
      return(ABORT_SYM);              // Illegal hex constant
      yyGet();                        // get_token makes an unget
      yylval->lex_str= get_token(lip,
                                 2,         // skip b'
                                 length-3); // don't count b' and last '
      return (BIN_NUM);

    case MY_LEX_CMP_OP:			// Incomplete comparison operator
      if (state_map[yyPeek()] == MY_LEX_CMP_OP ||
	  state_map[yyPeek()] == MY_LEX_LONG_CMP_OP)
	yySkip();
      if ((tokval = find_keyword(lip,(uint) (lip->ptr - lip->tok_start),0)))
      {
	lip->next_state= MY_LEX_START;	// Allow signed numbers
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
      if ((tokval = find_keyword(lip,(uint) (lip->ptr - lip->tok_start),0)))
      {
	lip->next_state= MY_LEX_START;	// Found long op
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
      tokval = find_keyword(lip,2,0);	// Is a bool operator
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      return(tokval);

    case MY_LEX_STRING_OR_DELIMITER:
      if (thd->variables.sql_mode & MODE_ANSI_QUOTES)
      {
	state= MY_LEX_USER_VARIABLE_DELIMITER;
	break;
      }
      /* " used for strings */
    case MY_LEX_STRING:			// Incomplete text string
      if (!(yylval->lex_str.str = get_text(lip)))
      {
	state= MY_LEX_CHAR;		// Read char by char
	break;
      }
      yylval->lex_str.length=lip->yytoklen;
      lex->text_string_is_7bit= (lip->tok_bitmap & 0x80) ? 0 : 1;
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
	  version=strtol((char*) lip->ptr,(char**) &lip->ptr,10);
	}
	if (version <= MYSQL_VERSION_ID)
	{
	  lex->in_comment=1;
	  break;
	}
      }
      /*
        Discard:
        - regular '/' '*' comments,
        - special comments '/' '*' '!' for a future version,
        by scanning until we find a closing '*' '/' marker.
        Note: There is no such thing as nesting comments,
        the first '*' '/' sequence seen will mark the end.
      */
      comment_closed= FALSE;
      while (lip->ptr != lip->end_of_query)
      {
        c= yyGet();
        if (c == '*')
        {
          if (yyPeek() == '/')
          {
            yySkip();
            comment_closed= TRUE;
            state = MY_LEX_START;
            break;
          }
        }
        else if (c == '\n')
          lip->yylineno++;
      }
      /* Unbalanced comments with a missing '*' '/' are a syntax error */
      if (! comment_closed)
        return (ABORT_SYM);
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
    case MY_LEX_SEMICOLON:			// optional line terminator
      if (yyPeek())
      {
        if ((thd->client_capabilities & CLIENT_MULTI_STATEMENTS) && 
            !lip->stmt_prepare_mode)
        {
	  lex->safe_to_cache_query= 0;
          lip->found_semicolon= lip->ptr;
          thd->server_status|= SERVER_MORE_RESULTS_EXISTS;
          lip->next_state=     MY_LEX_END;
          return (END_OF_INPUT);
        }
        state= MY_LEX_CHAR;		// Return ';'
	break;
      }
      /* fall true */
    case MY_LEX_EOL:
      if (lip->ptr >= lip->end_of_query)
      {
	lip->next_state=MY_LEX_END;	// Mark for next loop
        /* Unbalanced comments with a missing '*' '/' are a syntax error */
        if (lex->in_comment)
          return (ABORT_SYM);
	return(END_OF_INPUT);
      }
      state=MY_LEX_CHAR;
      break;
    case MY_LEX_END:
      lip->next_state=MY_LEX_END;
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
	lip->next_state=MY_LEX_SYSTEM_VAR;
	break;
      default:
	lip->next_state=MY_LEX_HOSTNAME;
	break;
      }
      yylval->lex_str.str=(char*) lip->ptr;
      yylval->lex_str.length=1;
      return((int) '@');
    case MY_LEX_HOSTNAME:		// end '@' of user@hostname
      for (c=yyGet() ; 
	   my_isalnum(cs,c) || c == '.' || c == '_' ||  c == '$';
	   c= yyGet()) ;
      yylval->lex_str=get_token(lip, 0, yyLength());
      return(LEX_HOSTNAME);
    case MY_LEX_SYSTEM_VAR:
      yylval->lex_str.str=(char*) lip->ptr;
      yylval->lex_str.length=1;
      yySkip();					// Skip '@'
      lip->next_state= (state_map[yyPeek()] ==
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
      
      for (result_state= 0; ident_map[c= yyGet()]; result_state|= c);
      /* If there were non-ASCII characters, mark that we must convert */
      result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      
      if (c == '.')
	lip->next_state=MY_LEX_IDENT_SEP;
      length= (uint) (lip->ptr - lip->tok_start)-1;
      if (length == 0) 
        return(ABORT_SYM);              // Names must be nonempty.
      if ((tokval= find_keyword(lip, length,0)))
      {
	yyUnget();				// Put back 'c'
	return(tokval);				// Was keyword
      }
      yylval->lex_str=get_token(lip, 0, length);
      return(result_state);
    }
  }
}


Alter_info::Alter_info(const Alter_info &rhs, MEM_ROOT *mem_root)
  :drop_list(rhs.drop_list, mem_root),
  alter_list(rhs.alter_list, mem_root),
  key_list(rhs.key_list, mem_root),
  create_list(rhs.create_list, mem_root),
  flags(rhs.flags),
  keys_onoff(rhs.keys_onoff),
  tablespace_op(rhs.tablespace_op)
{}


/*
  Skip comment in the end of statement.

  SYNOPSIS
    skip_rear_comments()
      cs      character set
      begin   pointer to the beginning of statement
      end     pointer to the end of statement

  DESCRIPTION
    The function is intended to trim comments at the end of the statement.

  RETURN
    Pointer to the last non-comment symbol of the statement.
*/

char *skip_rear_comments(CHARSET_INFO *cs, char *begin, char *end)
{
  while (begin < end && (end[-1] == '*' ||
                         end[-1] == '/' || end[-1] == ';' ||
                         my_isspace(cs, end[-1])))
    end-= 1;
  return end;
}

/*
  st_select_lex structures initialisations
*/

void st_select_lex_node::init_query()
{
  options= 0;
  sql_cache= SQL_CACHE_UNSPECIFIED;
  linkage= UNSPECIFIED_TYPE;
  no_error= no_table_names_allowed= 0;
  uncacheable= 0;
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
  union_distinct= 0;
  prepared= optimized= executed= 0;
  item= 0;
  union_result= 0;
  table= 0;
  fake_select_lex= 0;
  cleaned= 0;
  item_list.empty();
  describe= 0;
  found_rows_for_union= 0;
}

void st_select_lex::init_query()
{
  st_select_lex_node::init_query();
  table_list.empty();
  top_join_list.empty();
  join_list= &top_join_list;
  embedding= leaf_tables= 0;
  item_list.empty();
  join= 0;
  having= prep_having= where= prep_where= 0;
  olap= UNSPECIFIED_OLAP_TYPE;
  having_fix_field= 0;
  context.select_lex= this;
  context.init();
  /*
    Add the name resolution context of the current (sub)query to the
    stack of contexts for the whole query.
    TODO:
    push_context may return an error if there is no memory for a new
    element in the stack, however this method has no return value,
    thus push_context should be moved to a place where query
    initialization is checked for failure.
  */
  parent_lex->push_context(&context);
  cond_count= between_count= with_wild= 0;
  max_equal_elems= 0;
  conds_processed_with_permanent_arena= 0;
  ref_pointer_array= 0;
  select_n_where_fields= 0;
  select_n_having_items= 0;
  subquery_in_having= explicit_limit= 0;
  is_item_list_lookup= 0;
  first_execution= 1;
  first_cond_optimization= 1;
  parsing_place= NO_MATTER;
  exclude_from_table_unique_test= no_wrap_view_item= FALSE;
  nest_level= 0;
  link_next= 0;
}

void st_select_lex::init_select()
{
  st_select_lex_node::init_select();
  group_list.empty();
  type= db= 0;
  having= 0;
  use_index_ptr= ignore_index_ptr= 0;
  table_join_options= 0;
  in_sum_expr= with_wild= 0;
  options= 0;
  sql_cache= SQL_CACHE_UNSPECIFIED;
  braces= 0;
  expr_list.empty();
  udf_list.empty();
  interval_list.empty();
  use_index.empty();
  ftfunc_list_alloc.empty();
  inner_sum_func_list= 0;
  ftfunc_list= &ftfunc_list_alloc;
  linkage= UNSPECIFIED_TYPE;
  order_list.elements= 0;
  order_list.first= 0;
  order_list.next= (byte**) &order_list.first;
  /* Set limit and offset to default values */
  select_limit= 0;      /* denotes the default limit = HA_POS_ERROR */
  offset_limit= 0;      /* denotes the default offset = 0 */
  with_sum_func= 0;
  is_correlated= 0;
  cur_pos_in_select_list= UNDEF_POS;
  non_agg_fields.empty();
  cond_value= having_value= Item::COND_UNDEF;
  inner_refs_list.empty();
  full_group_by_flag= 0;
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
    if (!(s->uncacheable & UNCACHEABLE_DEPENDENT))
    {
      // Select is dependent of outer select
      s->uncacheable= (s->uncacheable & ~UNCACHEABLE_UNITED) |
                       UNCACHEABLE_DEPENDENT;
      SELECT_LEX_UNIT *munit= s->master_unit();
      munit->uncacheable= (munit->uncacheable & ~UNCACHEABLE_UNITED) |
                       UNCACHEABLE_DEPENDENT;
      for (SELECT_LEX *sl= munit->first_select(); sl ; sl= sl->next_select())
      {
        if (sl != s &&
            !(sl->uncacheable & (UNCACHEABLE_DEPENDENT | UNCACHEABLE_UNITED)))
          sl->uncacheable|= UNCACHEABLE_UNITED;
      }
    }
  is_correlated= TRUE;
  this->master_unit()->item->is_correlated= TRUE;
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
  if (select_limit != 0)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "LIMIT & IN/ALL/ANY/SOME subquery");
    return(1);
  }
  return(0);
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
  DBUG_ENTER("st_select_lex::add_item_to_list");
  DBUG_PRINT("info", ("Item: %p", item));
  DBUG_RETURN(item_list.push_back(item));
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

  /*
    We have to create array in prepared statement memory if it is
    prepared statement
  */
  Query_arena *arena= thd->stmt_arena;
  return (ref_pointer_array=
          (Item **)arena->alloc(sizeof(Item*) * (n_child_sum_items +
                                                 item_list.elements +
                                                 select_n_having_items +
                                                 select_n_where_fields +
                                                 order_group_num)*5)) == 0;
}


void st_select_lex_unit::print(String *str)
{
  bool union_all= !union_distinct;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl != first_select())
    {
      str->append(STRING_WITH_LEN(" union "));
      if (union_all)
	str->append(STRING_WITH_LEN("all "));
      else if (union_distinct == sl)
        union_all= TRUE;
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
      str->append(STRING_WITH_LEN(" order by "));
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
    if (order->counter_used)
    {
      char buffer[20];
      uint length= my_snprintf(buffer, 20, "%d", order->counter);
      str->append(buffer, length);
    }
    else
      (*order->item)->print(str);
    if (!order->asc)
      str->append(STRING_WITH_LEN(" desc"));
    if (order->next)
      str->append(',');
  }
}
 

void st_select_lex::print_limit(THD *thd, String *str)
{
  SELECT_LEX_UNIT *unit= master_unit();
  Item_subselect *item= unit->item;
  if (item && unit->global_parameters == this &&
      (item->substype() == Item_subselect::EXISTS_SUBS ||
       item->substype() == Item_subselect::IN_SUBS ||
       item->substype() == Item_subselect::ALL_SUBS))
  {
    DBUG_ASSERT(!item->fixed ||
                select_limit->val_int() == LL(1) && offset_limit == 0);
    return;
  }

  if (explicit_limit)
  {
    str->append(STRING_WITH_LEN(" limit "));
    if (offset_limit)
    {
      offset_limit->print(str);
      str->append(',');
    }
    select_limit->print(str);
  }
}

/**
  @brief Restore the LEX and THD in case of a parse error.

  This is a clean up call that is invoked by the Bison generated
  parser before returning an error from MYSQLparse. If your
  semantic actions manipulate with the global thread state (which
  is a very bad practice and should not normally be employed) and
  need a clean-up in case of error, and you can not use %destructor
  rule in the grammar file itself, this function should be used
  to implement the clean up.
*/

void st_lex::cleanup_lex_after_parse_error(THD *thd)
{
  /*
    Delete sphead for the side effect of restoring of the original
    LEX state, thd->lex, thd->mem_root and thd->free_list if they
    were replaced when parsing stored procedure statements.  We
    will never use sphead object after a parse error, so it's okay
    to delete it only for the sake of the side effect.
    TODO: make this functionality explicit in sp_head class.
    Sic: we must nullify the member of the main lex, not the
    current one that will be thrown away
  */
  if (thd->lex->sphead)
  {
    delete thd->lex->sphead;
    thd->lex->sphead= NULL;
  }
}

/*
  Initialize (or reset) Query_tables_list object.

  SYNOPSIS
    reset_query_tables_list()
      init  TRUE  - we should perform full initialization of object with
                    allocating needed memory
            FALSE - object is already initialized so we should only reset
                    its state so it can be used for parsing/processing
                    of new statement

  DESCRIPTION
    This method initializes Query_tables_list so it can be used as part
    of LEX object for parsing/processing of statement. One can also use
    this method to reset state of already initialized Query_tables_list
    so it can be used for processing of new statement.
*/

void Query_tables_list::reset_query_tables_list(bool init)
{
  query_tables= 0;
  query_tables_last= &query_tables;
  query_tables_own_last= 0;
  if (init)
  {
    /*
      We delay real initialization of hash (and therefore related
      memory allocation) until first insertion into this hash.
    */
    hash_clear(&sroutines);
  }
  else if (sroutines.records)
  {
    /* Non-zero sroutines.records means that hash was initialized. */
    my_hash_reset(&sroutines);
  }
  sroutines_list.empty();
  sroutines_list_own_last= sroutines_list.next;
  sroutines_list_own_elements= 0;
}


/*
  Destroy Query_tables_list object with freeing all resources used by it.

  SYNOPSIS
    destroy_query_tables_list()
*/

void Query_tables_list::destroy_query_tables_list()
{
  hash_free(&sroutines);
}


/*
  Initialize LEX object.

  SYNOPSIS
    st_lex::st_lex()

  NOTE
    LEX object initialized with this constructor can be used as part of
    THD object for which one can safely call open_tables(), lock_tables()
    and close_thread_tables() functions. But it is not yet ready for
    statement parsing. On should use lex_start() function to prepare LEX
    for this.
*/

st_lex::st_lex()
  :result(0), yacc_yyss(0), yacc_yyvs(0),
   sql_command(SQLCOM_END)
{
  reset_query_tables_list(TRUE);
}


/*
  Check whether the merging algorithm can be used on this VIEW

  SYNOPSIS
    st_lex::can_be_merged()

  DESCRIPTION
    We can apply merge algorithm if it is single SELECT view  with
    subqueries only in WHERE clause (we do not count SELECTs of underlying
    views, and second level subqueries) and we have not grpouping, ordering,
    HAVING clause, aggregate functions, DISTINCT clause, LIMIT clause and
    several underlying tables.

  RETURN
    FALSE - only temporary table algorithm can be used
    TRUE  - merge algorithm can be used
*/

bool st_lex::can_be_merged()
{
  // TODO: do not forget implement case when select_lex.table_list.elements==0

  /* find non VIEW subqueries/unions */
  bool selects_allow_merge= select_lex.next_select() == 0;
  if (selects_allow_merge)
  {
    for (SELECT_LEX_UNIT *tmp_unit= select_lex.first_inner_unit();
         tmp_unit;
         tmp_unit= tmp_unit->next_unit())
    {
      if (tmp_unit->first_select()->parent_lex == this &&
          (tmp_unit->item == 0 ||
           (tmp_unit->item->place() != IN_WHERE &&
            tmp_unit->item->place() != IN_ON)))
      {
        selects_allow_merge= 0;
        break;
      }
    }
  }

  return (selects_allow_merge &&
	  select_lex.group_list.elements == 0 &&
	  select_lex.having == 0 &&
          select_lex.with_sum_func == 0 &&
	  select_lex.table_list.elements >= 1 &&
	  !(select_lex.options & SELECT_DISTINCT) &&
          select_lex.select_limit == 0);
}


/*
  check if command can use VIEW with MERGE algorithm (for top VIEWs)

  SYNOPSIS
    st_lex::can_use_merged()

  DESCRIPTION
    Only listed here commands can use merge algorithm in top level
    SELECT_LEX (for subqueries will be used merge algorithm if
    st_lex::can_not_use_merged() is not TRUE).

  RETURN
    FALSE - command can't use merged VIEWs
    TRUE  - VIEWs with MERGE algorithms can be used
*/

bool st_lex::can_use_merged()
{
  switch (sql_command)
  {
  case SQLCOM_SELECT:
  case SQLCOM_CREATE_TABLE:
  case SQLCOM_UPDATE:
  case SQLCOM_UPDATE_MULTI:
  case SQLCOM_DELETE:
  case SQLCOM_DELETE_MULTI:
  case SQLCOM_INSERT:
  case SQLCOM_INSERT_SELECT:
  case SQLCOM_REPLACE:
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_LOAD:
    return TRUE;
  default:
    return FALSE;
  }
}

/*
  Check if command can't use merged views in any part of command

  SYNOPSIS
    st_lex::can_not_use_merged()

  DESCRIPTION
    Temporary table algorithm will be used on all SELECT levels for queries
    listed here (see also st_lex::can_use_merged()).

  RETURN
    FALSE - command can't use merged VIEWs
    TRUE  - VIEWs with MERGE algorithms can be used
*/

bool st_lex::can_not_use_merged()
{
  switch (sql_command)
  {
  case SQLCOM_CREATE_VIEW:
  case SQLCOM_SHOW_CREATE:
  /*
    SQLCOM_SHOW_FIELDS is necessary to make 
    information schema tables working correctly with views.
    see get_schema_tables_result function
  */
  case SQLCOM_SHOW_FIELDS:
    return TRUE;
  default:
    return FALSE;
  }
}

/*
  Detect that we need only table structure of derived table/view

  SYNOPSIS
    only_view_structure()

  RETURN
    TRUE yes, we need only structure
    FALSE no, we need data
*/

bool st_lex::only_view_structure()
{
  switch (sql_command) {
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_REVOKE_ALL:
  case SQLCOM_REVOKE:
  case SQLCOM_GRANT:
  case SQLCOM_CREATE_VIEW:
    return TRUE;
  default:
    return FALSE;
  }
}


/*
  Should Items_ident be printed correctly

  SYNOPSIS
    need_correct_ident()

  RETURN
    TRUE yes, we need only structure
    FALSE no, we need data
*/


bool st_lex::need_correct_ident()
{
  switch(sql_command)
  {
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_CREATE_VIEW:
    return TRUE;
  default:
    return FALSE;
  }
}

/*
  Get effective type of CHECK OPTION for given view

  SYNOPSIS
    get_effective_with_check()
    view    given view

  NOTE
    It have not sense to set CHECK OPTION for SELECT satement or subqueries,
    so we do not.

  RETURN
    VIEW_CHECK_NONE      no need CHECK OPTION
    VIEW_CHECK_LOCAL     CHECK OPTION LOCAL
    VIEW_CHECK_CASCADED  CHECK OPTION CASCADED
*/

uint8 st_lex::get_effective_with_check(TABLE_LIST *view)
{
  if (view->select_lex->master_unit() == &unit &&
      which_check_option_applicable())
    return (uint8)view->with_check;
  return VIEW_CHECK_NONE;
}


/**
  This method should be called only during parsing.
  It is aware of compound statements (stored routine bodies)
  and will initialize the destination with the default
  database of the stored routine, rather than the default
  database of the connection it is parsed in.
  E.g. if one has no current database selected, or current database 
  set to 'bar' and then issues:

  CREATE PROCEDURE foo.p1() BEGIN SELECT * FROM t1 END//

  t1 is meant to refer to foo.t1, not to bar.t1.

  This method is needed to support this rule.

  @return TRUE in case of error (parsing should be aborted, FALSE in
  case of success
*/

bool
st_lex::copy_db_to(char **p_db, uint *p_db_length) const
{
  if (sphead)
  {
    DBUG_ASSERT(sphead->m_db.str && sphead->m_db.length);
    /*
      It is safe to assign the string by-pointer, both sphead and
      its statements reside in the same memory root.
    */
    *p_db= sphead->m_db.str;
    if (p_db_length)
      *p_db_length= sphead->m_db.length;
    return FALSE;
  }
  return thd->copy_db_to(p_db, p_db_length);
}

/*
  initialize limit counters

  SYNOPSIS
    st_select_lex_unit::set_limit()
    values	- SELECT_LEX with initial values for counters
*/

void st_select_lex_unit::set_limit(SELECT_LEX *sl)
{
  ha_rows select_limit_val;

  DBUG_ASSERT(! thd->stmt_arena->is_stmt_prepare());
  select_limit_val= (ha_rows)(sl->select_limit ? sl->select_limit->val_uint() :
                                                 HA_POS_ERROR);
  offset_limit_cnt= (ha_rows)(sl->offset_limit ? sl->offset_limit->val_uint() :
                                                 ULL(0));
  select_limit_cnt= select_limit_val + offset_limit_cnt;
  if (select_limit_cnt < select_limit_val)
    select_limit_cnt= HA_POS_ERROR;		// no limit
}


/**
  @brief Set the initial purpose of this TABLE_LIST object in the list of used
    tables.

  We need to track this information on table-by-table basis, since when this
  table becomes an element of the pre-locked list, it's impossible to identify
  which SQL sub-statement it has been originally used in.

  E.g.:

  User request:                 SELECT * FROM t1 WHERE f1();
  FUNCTION f1():                DELETE FROM t2; RETURN 1;
  BEFORE DELETE trigger on t2:  INSERT INTO t3 VALUES (old.a);

  For this user request, the pre-locked list will contain t1, t2, t3
  table elements, each needed for different DML.

  The trigger event map is updated to reflect INSERT, UPDATE, DELETE,
  REPLACE, LOAD DATA, CREATE TABLE .. SELECT, CREATE TABLE ..
  REPLACE SELECT statements, and additionally ON DUPLICATE KEY UPDATE
  clause.
*/

void st_lex::set_trg_event_type_for_tables()
{
  uint8 new_trg_event_map= 0;

  /*
    Some auxiliary operations
    (e.g. GRANT processing) create TABLE_LIST instances outside
    the parser. Additionally, some commands (e.g. OPTIMIZE) change
    the lock type for a table only after parsing is done. Luckily,
    these do not fire triggers and do not need to pre-load them.
    For these TABLE_LISTs set_trg_event_type is never called, and
    trg_event_map is always empty. That means that the pre-locking
    algorithm will ignore triggers defined on these tables, if
    any, and the execution will either fail with an assert in
    sql_trigger.cc or with an error that a used table was not
    pre-locked, in case of a production build.

    TODO: this usage pattern creates unnecessary module dependencies
    and should be rewritten to go through the parser.
    Table list instances created outside the parser in most cases
    refer to mysql.* system tables. It is not allowed to have
    a trigger on a system table, but keeping track of
    initialization provides extra safety in case this limitation
    is circumvented.
  */

  switch (sql_command) {
  case SQLCOM_LOCK_TABLES:
  /*
    On a LOCK TABLE, all triggers must be pre-loaded for this TABLE_LIST
    when opening an associated TABLE.
  */
    new_trg_event_map= static_cast<uint8>
                        (1 << static_cast<int>(TRG_EVENT_INSERT)) |
                      static_cast<uint8>
                        (1 << static_cast<int>(TRG_EVENT_UPDATE)) |
                      static_cast<uint8>
                        (1 << static_cast<int>(TRG_EVENT_DELETE));
    break;
  /*
    Basic INSERT. If there is an additional ON DUPLIATE KEY UPDATE
    clause, it will be handled later in this method.
  */
  case SQLCOM_INSERT:                           /* fall through */
  case SQLCOM_INSERT_SELECT:
  /*
    LOAD DATA ... INFILE is expected to fire BEFORE/AFTER INSERT
    triggers.
    If the statement also has REPLACE clause, it will be
    handled later in this method.
  */
  case SQLCOM_LOAD:                             /* fall through */
  /*
    REPLACE is semantically equivalent to INSERT. In case
    of a primary or unique key conflict, it deletes the old
    record and inserts a new one. So we also may need to
    fire ON DELETE triggers. This functionality is handled
    later in this method.
  */
  case SQLCOM_REPLACE:                          /* fall through */
  case SQLCOM_REPLACE_SELECT:
  /*
    CREATE TABLE ... SELECT defaults to INSERT if the table or
    view already exists. REPLACE option of CREATE TABLE ...
    REPLACE SELECT is handled later in this method.
  */
  case SQLCOM_CREATE_TABLE:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_INSERT));
    break;
  /* Basic update and multi-update */
  case SQLCOM_UPDATE:                           /* fall through */
  case SQLCOM_UPDATE_MULTI:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_UPDATE));
    break;
  /* Basic delete and multi-delete */
  case SQLCOM_DELETE:                           /* fall through */
  case SQLCOM_DELETE_MULTI:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_DELETE));
    break;
  default:
    break;
  }

  switch (duplicates) {
  case DUP_UPDATE:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_UPDATE));
    break;
  case DUP_REPLACE:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_DELETE));
    break;
  case DUP_ERROR:
  default:
    break;
  }


  /*
    Do not iterate over sub-selects, only the tables in the outermost
    SELECT_LEX can be modified, if any.
  */
  TABLE_LIST *tables= select_lex.get_table_list();

  while (tables)
  {
    /*
      This is a fast check to filter out statements that do
      not change data, or tables  on the right side, in case of
      INSERT .. SELECT, CREATE TABLE .. SELECT and so on.
      Here we also filter out OPTIMIZE statement and non-updateable
      views, for which lock_type is TL_UNLOCK or TL_READ after
      parsing.
    */
    if (static_cast<int>(tables->lock_type) >=
        static_cast<int>(TL_WRITE_ALLOW_WRITE))
      tables->trg_event_map= new_trg_event_map;
    tables= tables->next_local;
  }
}


/*
  Unlink the first table from the global table list and the first table from
  outer select (lex->select_lex) local list

  SYNOPSIS
    unlink_first_table()
    link_to_local	Set to 1 if caller should link this table to local list

  NOTES
    We assume that first tables in both lists is the same table or the local
    list is empty.

  RETURN
    0	If 'query_tables' == 0
    unlinked table
      In this case link_to_local is set.

*/
TABLE_LIST *st_lex::unlink_first_table(bool *link_to_local)
{
  TABLE_LIST *first;
  if ((first= query_tables))
  {
    /*
      Exclude from global table list
    */
    if ((query_tables= query_tables->next_global))
      query_tables->prev_global= &query_tables;
    else
      query_tables_last= &query_tables;
    first->next_global= 0;

    /*
      and from local list if it is not empty
    */
    if ((*link_to_local= test(select_lex.table_list.first)))
    {
      select_lex.context.table_list= 
        select_lex.context.first_name_resolution_table= first->next_local;
      select_lex.table_list.first= (byte*) (first->next_local);
      select_lex.table_list.elements--;	//safety
      first->next_local= 0;
      /*
        Ensure that the global list has the same first table as the local
        list.
      */
      first_lists_tables_same();
    }
  }
  return first;
}


/*
  Bring first local table of first most outer select to first place in global
  table list

  SYNOPSYS
     st_lex::first_lists_tables_same()

  NOTES
    In many cases (for example, usual INSERT/DELETE/...) the first table of
    main SELECT_LEX have special meaning => check that it is the first table
    in global list and re-link to be first in the global list if it is
    necessary.  We need such re-linking only for queries with sub-queries in
    the select list, as only in this case tables of sub-queries will go to
    the global list first.
*/

void st_lex::first_lists_tables_same()
{
  TABLE_LIST *first_table= (TABLE_LIST*) select_lex.table_list.first;
  if (query_tables != first_table && first_table != 0)
  {
    TABLE_LIST *next;
    if (query_tables_last == &first_table->next_global)
      query_tables_last= first_table->prev_global;

    if ((next= *first_table->prev_global= first_table->next_global))
      next->prev_global= first_table->prev_global;
    /* include in new place */
    first_table->next_global= query_tables;
    /*
       We are sure that query_tables is not 0, because first_table was not
       first table in the global list => we can use
       query_tables->prev_global without check of query_tables
    */
    query_tables->prev_global= &first_table->next_global;
    first_table->prev_global= &query_tables;
    query_tables= first_table;
  }
}


/*
  Add implicitly used time zone description tables to global table list
  (if needed).

  SYNOPSYS
    st_lex::add_time_zone_tables_to_query_tables()
      thd - pointer to current thread context

  RETURN VALUE
   TRUE  - error
   FALSE - success
*/

bool st_lex::add_time_zone_tables_to_query_tables(THD *thd_arg)
{
  /* We should not add these tables twice */
  if (!time_zone_tables_used)
  {
    time_zone_tables_used= my_tz_get_table_list(thd_arg, &query_tables_last);
    if (time_zone_tables_used == &fake_time_zone_tables_list)
      return TRUE;
  }
  return FALSE;
}

/*
  Link table back that was unlinked with unlink_first_table()

  SYNOPSIS
    link_first_table_back()
    link_to_local	do we need link this table to local

  RETURN
    global list
*/

void st_lex::link_first_table_back(TABLE_LIST *first,
				   bool link_to_local)
{
  if (first)
  {
    if ((first->next_global= query_tables))
      query_tables->prev_global= &first->next_global;
    else
      query_tables_last= &first->next_global;
    query_tables= first;

    if (link_to_local)
    {
      first->next_local= (TABLE_LIST*) select_lex.table_list.first;
      select_lex.context.table_list= first;
      select_lex.table_list.first= (byte*) first;
      select_lex.table_list.elements++;	//safety
    }
  }
}



/*
  cleanup lex for case when we open table by table for processing

  SYNOPSIS
    st_lex::cleanup_after_one_table_open()

  NOTE
    This method is mostly responsible for cleaning up of selects lists and
    derived tables state. To rollback changes in Query_tables_list one has
    to call Query_tables_list::reset_query_tables_list(FALSE).
*/

void st_lex::cleanup_after_one_table_open()
{
  /*
    thd->lex->derived_tables & additional units may be set if we open
    a view. It is necessary to clear thd->lex->derived_tables flag
    to prevent processing of derived tables during next open_and_lock_tables
    if next table is a real table and cleanup & remove underlying units
    NOTE: all units will be connected to thd->lex->select_lex, because we
    have not UNION on most upper level.
    */
  if (all_selects_list != &select_lex)
  {
    derived_tables= 0;
    /* cleunup underlying units (units of VIEW) */
    for (SELECT_LEX_UNIT *un= select_lex.first_inner_unit();
         un;
         un= un->next_unit())
      un->cleanup();
    /* reduce all selects list to default state */
    all_selects_list= &select_lex;
    /* remove underlying units (units of VIEW) subtree */
    select_lex.cut_subtree();
  }
  time_zone_tables_used= 0;
}


/*
  Save current state of Query_tables_list for this LEX, and prepare it
  for processing of new statemnt.

  SYNOPSIS
    reset_n_backup_query_tables_list()
      backup  Pointer to Query_tables_list instance to be used for backup
*/

void st_lex::reset_n_backup_query_tables_list(Query_tables_list *backup)
{
  backup->set_query_tables_list(this);
  /*
    We have to perform full initialization here since otherwise we
    will damage backed up state.
  */
  this->reset_query_tables_list(TRUE);
}


/*
  Restore state of Query_tables_list for this LEX from backup.

  SYNOPSIS
    restore_backup_query_tables_list()
      backup  Pointer to Query_tables_list instance used for backup
*/

void st_lex::restore_backup_query_tables_list(Query_tables_list *backup)
{
  this->destroy_query_tables_list();
  this->set_query_tables_list(backup);
}


/*
  Do end-of-prepare fixup for list of tables and their merge-VIEWed tables

  SYNOPSIS
    fix_prepare_info_in_table_list()
      thd  Thread handle
      tbl  List of tables to process

  DESCRIPTION
    Perform end-end-of prepare fixup for list of tables, if any of the tables
    is a merge-algorithm VIEW, recursively fix up its underlying tables as
    well.

*/

static void fix_prepare_info_in_table_list(THD *thd, TABLE_LIST *tbl)
{
  for (; tbl; tbl= tbl->next_local)
  {
    if (tbl->on_expr)
    {
      tbl->prep_on_expr= tbl->on_expr;
      tbl->on_expr= tbl->on_expr->copy_andor_structure(thd);
    }
    fix_prepare_info_in_table_list(thd, tbl->merge_underlying_list);
  }
}


/*
  Save WHERE/HAVING/ON clauses and replace them with disposable copies

  SYNOPSIS
    st_select_lex::fix_prepare_information
      thd          thread handler
      conds        in/out pointer to WHERE condition to be met at execution
      having_conds in/out pointer to HAVING condition to be met at execution
  
  DESCRIPTION
    The passed WHERE and HAVING are to be saved for the future executions.
    This function saves it, and returns a copy which can be thrashed during
    this execution of the statement. By saving/thrashing here we mean only
    AND/OR trees.
    The function also calls fix_prepare_info_in_table_list that saves all
    ON expressions.    
*/

void st_select_lex::fix_prepare_information(THD *thd, Item **conds, 
                                            Item **having_conds)
{
  if (!thd->stmt_arena->is_conventional() && first_execution)
  {
    first_execution= 0;
    if (*conds)
    {
      prep_where= *conds;
      *conds= where= prep_where->copy_andor_structure(thd);
    }
    if (*having_conds)
    {
      prep_having= *having_conds;
      *having_conds= having= prep_having->copy_andor_structure(thd);
    }
    fix_prepare_info_in_table_list(thd, (TABLE_LIST *)table_list.first);
  }
}

/*
  There are st_select_lex::add_table_to_list &
  st_select_lex::set_lock_for_tables are in sql_parse.cc

  st_select_lex::print is in sql_select.cc

  st_select_lex_unit::prepare, st_select_lex_unit::exec,
  st_select_lex_unit::cleanup, st_select_lex_unit::reinit_exec_mechanism,
  st_select_lex_unit::change_result
  are in sql_union.cc
*/

