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


/* This file defines all string functions
** Warning: Some string functions doesn't always put and end-null on a String
** (This shouldn't be needed)
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#endif /* HAVE_OPENSSL */
#include "md5.h"
#include "sha1.h"
#include "my_aes.h"

String empty_string("",default_charset_info);

uint nr_of_decimals(const char *str)
{
  if ((str=strchr(str,'.')))
  {
    const char *start= ++str;
    for (; my_isdigit(system_charset_info,*str) ; str++) ;
    return (uint) (str-start);
  }
  return 0;
}

double Item_str_func::val()
{
  int err;
  String *res;
  res=val_str(&str_value);
  return res ? my_strntod(res->charset(), (char*) res->ptr(),res->length(),
			  NULL, &err) : 0.0;
}

longlong Item_str_func::val_int()
{
  int err;
  String *res;
  res=val_str(&str_value);
  return res ? my_strntoll(res->charset(),res->ptr(),res->length(),10,NULL,&err) : (longlong) 0;
}


String *Item_func_md5::val_str(String *str)
{
  String * sptr= args[0]->val_str(str);
  if (sptr)
  {
    my_MD5_CTX context;
    unsigned char digest[16];

    null_value=0;
    my_MD5Init (&context);
    my_MD5Update (&context,(unsigned char *) sptr->ptr(), sptr->length());
    my_MD5Final (digest, &context);
    if (str->alloc(32))				// Ensure that memory is free
    {
      null_value=1;
      return 0;
    }
    sprintf((char *) str->ptr(),
	    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    digest[0], digest[1], digest[2], digest[3],
	    digest[4], digest[5], digest[6], digest[7],
	    digest[8], digest[9], digest[10], digest[11],
	    digest[12], digest[13], digest[14], digest[15]);
    str->length((uint) 32);
    return str;
  }
  null_value=1;
  return 0;
}


void Item_func_md5::fix_length_and_dec()
{
   max_length=32;
}


String *Item_func_sha::val_str(String *str)
{
  String * sptr= args[0]->val_str(str);
  if (sptr)  /* If we got value different from NULL */
  {
    SHA1_CONTEXT context;  /* Context used to generate SHA1 hash */
    /* Temporary buffer to store 160bit digest */
    uint8 digest[SHA1_HASH_SIZE];
    sha1_reset(&context);  /* We do not have to check for error here */
    /* No need to check error as the only case would be too long message */
    sha1_input(&context,(const unsigned char *) sptr->ptr(), sptr->length());
    /* Ensure that memory is free and we got result */
    if (!( str->alloc(SHA1_HASH_SIZE*2) || (sha1_result(&context,digest))))
    {
      sprintf((char *) str->ptr(),
      "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\
%02x%02x%02x%02x%02x%02x%02x%02x",
           digest[0], digest[1], digest[2], digest[3],
           digest[4], digest[5], digest[6], digest[7],
           digest[8], digest[9], digest[10], digest[11],
           digest[12], digest[13], digest[14], digest[15],
           digest[16], digest[17], digest[18], digest[19]);

      str->length((uint)  SHA1_HASH_SIZE*2);
      null_value=0;
      return str;
    }
  }
  null_value=1;
  return 0;
}

void Item_func_sha::fix_length_and_dec()
{
   max_length=SHA1_HASH_SIZE*2; // size of hex representation of hash
}


/* Implementation of AES encryption routines */

String *Item_func_aes_encrypt::val_str(String *str)
{
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
  String *sptr= args[0]->val_str(str);			// String to encrypt
  String *key=  args[1]->val_str(&tmp_key_value);	// key
  int aes_length;
  if (sptr && key) // we need both arguments to be not NULL
  {
    null_value=0;
    aes_length=my_aes_get_size(sptr->length()); // Calculate result length

    if (!str_value.alloc(aes_length))		// Ensure that memory is free
    {
      // finally encrypt directly to allocated buffer.
      if (my_aes_encrypt(sptr->ptr(),sptr->length(), (char*) str_value.ptr(),
			 key->ptr(), key->length()) == aes_length)
      {
	// We got the expected result length
	str_value.length((uint) aes_length);
	return &str_value;
      }
    }
  }
  null_value=1;
  return 0;
}


void Item_func_aes_encrypt::fix_length_and_dec()
{
  max_length=my_aes_get_size(args[0]->max_length);
}


String *Item_func_aes_decrypt::val_str(String *str)
{
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
  String *sptr, *key;
  DBUG_ENTER("Item_func_aes_decrypt::val_str");

  sptr= args[0]->val_str(str);			// String to decrypt
  key=  args[1]->val_str(&tmp_key_value);	// Key
  if (sptr && key)  			// Need to have both arguments not NULL
  {
    null_value=0;
    if (!str_value.alloc(sptr->length()))  // Ensure that memory is free
    {
      // finally decrypt directly to allocated buffer.
      int length;
      length=my_aes_decrypt(sptr->ptr(), sptr->length(),
			    (char*) str_value.ptr(),
                            key->ptr(), key->length());
      if (length >= 0)  // if we got correct data data
      {
        str_value.length((uint) length);
        DBUG_RETURN(&str_value);
      }
    }
  }
  // Bad parameters. No memory or bad data will all go here
  null_value=1;
  DBUG_RETURN(0);
}


void Item_func_aes_decrypt::fix_length_and_dec()
{
   max_length=args[0]->max_length;
}


/*
  Concatenate args with the following premises:
  If only one arg (which is ok), return value of arg
  Don't reallocate val_str() if not absolute necessary.
*/

String *Item_func_concat::val_str(String *str)
{
  String *res,*res2,*use_as_buff;
  uint i;

  null_value=0;
  if (!(res=args[0]->val_str(str)))
    goto null;
  use_as_buff= &tmp_value;
  for (i=1 ; i < arg_count ; i++)
  {
    if (args[i]->binary())
      set_charset(&my_charset_bin);
    if (res->length() == 0)
    {
      if (!(res=args[i]->val_str(str)))
	goto null;
    }
    else
    {
      if (!(res2=args[i]->val_str(use_as_buff)))
	goto null;
      if (res2->length() == 0)
	continue;
      if (res->length()+res2->length() >
	  current_thd->variables.max_allowed_packet)
	goto null;				// Error check
      if (res->alloced_length() >= res->length()+res2->length())
      {						// Use old buffer
	res->append(*res2);
      }
      else if (str->alloced_length() >= res->length()+res2->length())
      {
	if (str == res2)
	  str->replace(0,0,*res);
	else
	{
	  str->copy(*res);
	  str->append(*res2);
	}
	res=str;
	res->set_charset(charset());
      }
      else if (res == &tmp_value)
      {
	if (res->append(*res2))			// Must be a blob
	  goto null;
      }
      else if (res2 == &tmp_value)
      {						// This can happend only 1 time
	if (tmp_value.replace(0,0,*res))
	  goto null;
	res= &tmp_value;
	res->set_charset(charset());
	use_as_buff=str;			// Put next arg here
      }
      else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	       res2->ptr() <= tmp_value.ptr() + tmp_value.alloced_length())
      {
	/*
	  This happens really seldom:
	  In this case res2 is sub string of tmp_value.  We will
	  now work in place in tmp_value to set it to res | res2
	*/
	/* Chop the last characters in tmp_value that isn't in res2 */
	tmp_value.length((uint32) (res2->ptr() - tmp_value.ptr()) +
			 res2->length());
	/* Place res2 at start of tmp_value, remove chars before res2 */
	if (tmp_value.replace(0,(uint32) (res2->ptr() - tmp_value.ptr()),
			      *res))
	  goto null;
	res= &tmp_value;
	res->set_charset(charset());
	use_as_buff=str;			// Put next arg here
      }
      else
      {						// Two big const strings
	if (tmp_value.alloc(max_length) ||
	    tmp_value.copy(*res) ||
	    tmp_value.append(*res2))
	  goto null;
	res= &tmp_value;
	res->set_charset(charset());
	use_as_buff=str;
      }
    }
  }
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat::fix_length_and_dec()
{
  max_length=0;
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}

/*
  Function des_encrypt() by tonu@spam.ee & monty
  Works only if compiled with OpenSSL library support.
  This returns a binary string where first character is CHAR(128 | key-number).
  If one uses a string key key_number is 127.
  Encryption result is longer than original by formula:
  new_length= org_length + (8-(org_length % 8))+1
*/

String *Item_func_des_encrypt::val_str(String *str)
{
#ifdef HAVE_OPENSSL
  des_cblock ivec;
  struct st_des_keyblock keyblock;
  struct st_des_keyschedule keyschedule;
  const char *append_str="********";
  uint key_number, res_length, tail;
  String *res= args[0]->val_str(str);

  if ((null_value=args[0]->null_value))
    return 0;
  if ((res_length=res->length()) == 0)
    return &empty_string;

  if (arg_count == 1)
  {
    /* Protect against someone doing FLUSH DES_KEY_FILE */
    VOID(pthread_mutex_lock(&LOCK_des_key_file));
    keyschedule= des_keyschedule[key_number=des_default_key];
    VOID(pthread_mutex_unlock(&LOCK_des_key_file));
  }
  else if (args[1]->result_type() == INT_RESULT)
  {
    key_number= (uint) args[1]->val_int();
    if (key_number > 9)
      goto error;
    VOID(pthread_mutex_lock(&LOCK_des_key_file));
    keyschedule= des_keyschedule[key_number];
    VOID(pthread_mutex_unlock(&LOCK_des_key_file));
  }
  else
  {
    String *keystr=args[1]->val_str(&tmp_value);
    if (!keystr)
      goto error;
    key_number=127;				// User key string

    /* We make good 24-byte (168 bit) key from given plaintext key with MD5 */
    bzero((char*) &ivec,sizeof(ivec));
    EVP_BytesToKey(EVP_des_ede3_cbc(),EVP_md5(),NULL,
		   (uchar*) keystr->ptr(), (int) keystr->length(),
		   1, (uchar*) &keyblock,ivec);
    des_set_key_unchecked(&keyblock.key1,keyschedule.ks1);
    des_set_key_unchecked(&keyblock.key2,keyschedule.ks2);
    des_set_key_unchecked(&keyblock.key3,keyschedule.ks3);
  }

  /*
     The problem: DES algorithm requires original data to be in 8-bytes
     chunks. Missing bytes get filled with '*'s and result of encryption
     can be up to 8 bytes longer than original string. When decrypted,
     we do not know the size of original string :(
     We add one byte with value 0x1..0x8 as the last byte of the padded
     string marking change of string length.
  */

  tail=  (8-(res_length) % 8);			// 1..8 marking extra length
  res_length+=tail;
  if (tail && res->append(append_str, tail) || tmp_value.alloc(res_length+1))
    goto error;
  (*res)[res_length-1]=tail;			// save extra length
  tmp_value.length(res_length+1);
  tmp_value[0]=(char) (128 | key_number);
  // Real encryption
  bzero((char*) &ivec,sizeof(ivec));
  des_ede3_cbc_encrypt((const uchar*) (res->ptr()),
		       (uchar*) (tmp_value.ptr()+1),
		       res_length,
		       keyschedule.ks1,
		       keyschedule.ks2,
		       keyschedule.ks3,
		       &ivec, TRUE);
  return &tmp_value;

error:
#endif	/* HAVE_OPENSSL */
  null_value=1;
  return 0;
}


String *Item_func_des_decrypt::val_str(String *str)
{
#ifdef HAVE_OPENSSL
  des_key_schedule ks1, ks2, ks3;
  des_cblock ivec;
  struct st_des_keyblock keyblock;
  struct st_des_keyschedule keyschedule;
  String *res= args[0]->val_str(str);
  uint length=res->length(),tail;

  if ((null_value=args[0]->null_value))
    return 0;
  length=res->length();
  if (length < 9 || (length % 8) != 1 || !((*res)[0] & 128))
    return res;				// Skip decryption if not encrypted

  if (arg_count == 1)			// If automatic uncompression
  {
    uint key_number=(uint) (*res)[0] & 127;
    // Check if automatic key and that we have privilege to uncompress using it
    if (!(current_thd->master_access & SUPER_ACL) || key_number > 9)
      goto error;
    VOID(pthread_mutex_lock(&LOCK_des_key_file));
    keyschedule= des_keyschedule[key_number];
    VOID(pthread_mutex_unlock(&LOCK_des_key_file));
  }
  else
  {
    // We make good 24-byte (168 bit) key from given plaintext key with MD5
    String *keystr=args[1]->val_str(&tmp_value);
    if (!keystr)
      goto error;

    bzero((char*) &ivec,sizeof(ivec));
    EVP_BytesToKey(EVP_des_ede3_cbc(),EVP_md5(),NULL,
		   (uchar*) keystr->ptr(),(int) keystr->length(),
		   1,(uchar*) &keyblock,ivec);
    // Here we set all 64-bit keys (56 effective) one by one
    des_set_key_unchecked(&keyblock.key1,keyschedule.ks1);
    des_set_key_unchecked(&keyblock.key2,keyschedule.ks2);
    des_set_key_unchecked(&keyblock.key3,keyschedule.ks3);
  }
  if (tmp_value.alloc(length-1))
    goto error;

  bzero((char*) &ivec,sizeof(ivec));
  des_ede3_cbc_encrypt((const uchar*) res->ptr()+1,
		       (uchar*) (tmp_value.ptr()),
		       length-1,
		       keyschedule.ks1,
		       keyschedule.ks2,
		       keyschedule.ks3,
		       &ivec, FALSE);
  /* Restore old length of key */
  if ((tail=(uint) (uchar) tmp_value[length-2]) > 8)
    goto error;					// Wrong key
  tmp_value.length(length-1-tail);
  return &tmp_value;

error:
#endif	/* HAVE_OPENSSL */
  null_value=1;
  return 0;
}


/*
  concat with separator. First arg is the separator
  concat_ws takes at least two arguments.
*/

String *Item_func_concat_ws::val_str(String *str)
{
  char tmp_str_buff[10];
  String tmp_sep_str(tmp_str_buff, sizeof(tmp_str_buff),default_charset_info),
         *sep_str, *res, *res2,*use_as_buff;
  uint i;

  null_value=0;
  if (!(sep_str= separator->val_str(&tmp_sep_str)))
    goto null;

  use_as_buff= &tmp_value;
  str->length(0);				// QQ; Should be removed
  res=str;

  // Skip until non-null and non-empty argument is found.
  // If not, return the empty string
  for (i=0; i < arg_count; i++)
    if ((res= args[i]->val_str(str)) && res->length())
      break;
  if (i ==  arg_count)
    return &empty_string;

  for (i++; i < arg_count ; i++)
  {
    if (!(res2= args[i]->val_str(use_as_buff)) || !res2->length())
      continue;					// Skip NULL and empty string

    if (res->length() + sep_str->length() + res2->length() >
	current_thd->variables.max_allowed_packet)
      goto null;				// Error check
    if (res->alloced_length() >=
	res->length() + sep_str->length() + res2->length())
    {						// Use old buffer
      res->append(*sep_str);			// res->length() > 0 always
      res->append(*res2);
    }
    else if (str->alloced_length() >=
	     res->length() + sep_str->length() + res2->length())
    {
      /* We have room in str;  We can't get any errors here */
      if (str == res2)
      {						// This is quote uncommon!
	str->replace(0,0,*sep_str);
	str->replace(0,0,*res);
      }
      else
      {
	str->copy(*res);
	str->append(*sep_str);
	str->append(*res2);
      }
      res=str;
      use_as_buff= &tmp_value;
    }
    else if (res == &tmp_value)
    {
      if (res->append(*sep_str) || res->append(*res2))
	goto null; // Must be a blob
    }
    else if (res2 == &tmp_value)
    {						// This can happend only 1 time
      if (tmp_value.replace(0,0,*sep_str) || tmp_value.replace(0,0,*res))
	goto null;
      res= &tmp_value;
      use_as_buff=str;				// Put next arg here
    }
    else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	     res2->ptr() < tmp_value.ptr() + tmp_value.alloced_length())
    {
      /*
	This happens really seldom:
	In this case res2 is sub string of tmp_value.  We will
	now work in place in tmp_value to set it to res | sep_str | res2
      */
      /* Chop the last characters in tmp_value that isn't in res2 */
      tmp_value.length((uint32) (res2->ptr() - tmp_value.ptr()) +
		       res2->length());
      /* Place res2 at start of tmp_value, remove chars before res2 */
      if (tmp_value.replace(0,(uint32) (res2->ptr() - tmp_value.ptr()),
			    *res) ||
	  tmp_value.replace(res->length(),0, *sep_str))
	goto null;
      res= &tmp_value;
      use_as_buff=str;			// Put next arg here
    }
    else
    {						// Two big const strings
      if (tmp_value.alloc(max_length) ||
	  tmp_value.copy(*res) ||
	  tmp_value.append(*sep_str) ||
	  tmp_value.append(*res2))
	goto null;
      res= &tmp_value;
      use_as_buff=str;
    }
  }
  return res;

null:
  null_value=1;
  return 0;
}

void Item_func_concat_ws::split_sum_func(Item **ref_pointer_array,
					 List<Item> &fields)
{
  if (separator->with_sum_func && separator->type() != SUM_FUNC_ITEM)
    separator->split_sum_func(ref_pointer_array, fields);
  else if (separator->used_tables() || separator->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    fields.push_front(separator);
    ref_pointer_array[el]= separator;
    separator= new Item_ref(ref_pointer_array + el, 0, separator->name);
  }
  Item_str_func::split_sum_func(ref_pointer_array, fields);
}

void Item_func_concat_ws::fix_length_and_dec()
{
  max_length=separator->max_length*(arg_count-1);
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
  used_tables_cache|=separator->used_tables();
  const_item_cache&=separator->const_item();
  with_sum_func= with_sum_func || separator->with_sum_func;
}

void Item_func_concat_ws::update_used_tables()
{
  Item_func::update_used_tables();
  separator->update_used_tables();
  used_tables_cache|=separator->used_tables();
  const_item_cache&=separator->const_item();
}


String *Item_func_reverse::val_str(String *str)
{
  String *res = args[0]->val_str(str);
  char *ptr,*end;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &empty_string;
  res=copy_if_not_alloced(str,res,res->length());
  ptr = (char *) res->ptr();
  end=ptr+res->length();
#ifdef USE_MB
  if (use_mb(res->charset()) && !binary())
  {
    String tmpstr;
    tmpstr.copy(*res);
    char *tmp = (char *) tmpstr.ptr() + tmpstr.length();
    register uint32 l;
    while (ptr < end)
    {
      if ((l=my_ismbchar(res->charset(), ptr,end)))
        tmp-=l, memcpy(tmp,ptr,l), ptr+=l;
      else
        *--tmp=*ptr++;
    }
    memcpy((char *) res->ptr(),(char *) tmpstr.ptr(), res->length());
  }
  else
#endif /* USE_MB */
  {
    char tmp;
    while (ptr < end)
    {
      tmp=*ptr;
      *ptr++=*--end;
      *end=tmp;
    }
  }
  return res;
}


void Item_func_reverse::fix_length_and_dec()
{
  max_length = args[0]->max_length;
}

/*
** Replace all occurences of string2 in string1 with string3.
** Don't reallocate val_str() if not needed
*/

/* TODO: Fix that this works with binary strings when using USE_MB */

String *Item_func_replace::val_str(String *str)
{
  String *res,*res2,*res3;
  int offset;
  uint from_length,to_length;
  bool alloced=0;
#ifdef USE_MB
  const char *ptr,*end,*strend,*search,*search_end;
  register uint32 l;
  bool binary_cmp;
#endif

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

#ifdef USE_MB
  binary_cmp = (args[0]->binary() || args[1]->binary() || !use_mb(res->charset()));
#endif

  if (res2->length() == 0)
    return res;
#ifndef USE_MB
  if ((offset=res->strstr(*res2)) < 0)
    return res;
#else
  offset=0;
  if (binary_cmp && (offset=res->strstr(*res2)) < 0)
    return res;
#endif
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

#ifdef USE_MB
  if (!binary_cmp)
  {
    search=res2->ptr();
    search_end=search+from_length;
redo:
    ptr=res->ptr()+offset;
    strend=res->ptr()+res->length();
    end=strend-from_length+1;
    while (ptr < end)
    {
        if (*ptr == *search)
        {
          register char *i,*j;
          i=(char*) ptr+1; j=(char*) search+1;
          while (j != search_end)
            if (*i++ != *j++) goto skipp;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length >
	      current_thd->variables.max_allowed_packet)
            goto null;
          if (!alloced)
          {
            alloced=1;
            res=copy_if_not_alloced(str,res,res->length()+to_length);
          }
          res->replace((uint) offset,from_length,*res3);
	  offset+=(int) to_length;
          goto redo;
        }
skipp:
        if ((l=my_ismbchar(res->charset(), ptr,strend))) ptr+=l;
        else ++ptr;
    }
  }
  else
#endif /* USE_MB */
    do
    {
      if (res->length()-from_length + to_length >
	  current_thd->variables.max_allowed_packet)
        goto null;
      if (!alloced)
      {
        alloced=1;
        res=copy_if_not_alloced(str,res,res->length()+to_length);
      }
      res->replace((uint) offset,from_length,*res3);
      offset+=(int) to_length;
    }
    while ((offset=res->strstr(*res2,(uint) offset)) >= 0);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_replace::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  int diff=(int) (args[2]->max_length - args[1]->max_length);
  if (diff > 0 && args[1]->max_length)
  {						// Calculate of maxreplaces
    max_length= max_length/args[1]->max_length;
    max_length= (max_length+1)*(uint) diff;
  }
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_insert::val_str(String *str)
{
  String *res,*res2;
  uint start,length;

  null_value=0;
  res=args[0]->val_str(str);
  res2=args[3]->val_str(&tmp_value);
  start=(uint) args[1]->val_int()-1;
  length=(uint) args[2]->val_int();
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null; /* purecov: inspected */
  start=res->charpos(start);
  length=res->charpos(length,start);
  if (start > res->length()+1)
    return res;					// Wrong param; skip insert
  if (length > res->length()-start)
    length=res->length()-start;
  if (res->length() - length + res2->length() >
      current_thd->variables.max_allowed_packet)
    goto null;					// OOM check
  res=copy_if_not_alloced(str,res,res->length());
  res->replace(start,length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}


void Item_func_insert::fix_length_and_dec()
{
  max_length=args[0]->max_length+args[3]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lcase::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->casedn();
  return res;
}


String *Item_func_ucase::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->caseup();
  return res;
}


String *Item_func_left::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0;
  if (length <= 0)
    return &empty_string;
  length= res->charpos(length);
  if (res->length() > (ulong) length)
  {						// Safe even if const arg
    if (!res->alloced_length())
    {						// Don't change const str
      str_value= *res;				// Not malloced string
      set_charset(res->charset());
      res= &str_value;
    }
    res->length((uint) length);
  }
  return res;
}


void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int();
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_left::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_right::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  if (length <= 0)
    return &empty_string; /* purecov: inspected */
  if (res->length() <= (uint) length)
    return res; /* purecov: inspected */

  uint start=res->numchars()-(uint) length;
  if (start<=0) return res;
  start=res->charpos(start);
  tmp_value.set(*res,start,res->length()-start);
  return &tmp_value;
}


void Item_func_right::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_substr::val_str(String *str)
{
  String *res  = args[0]->val_str(str);
  int32 start	= (int32) args[1]->val_int()-1;
  int32 length	= arg_count == 3 ? (int32) args[2]->val_int() : INT_MAX32;
  int32 tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */
  start=res->charpos(start);
  length=res->charpos(length,start);
  if (start < 0 || (uint) start+1 > res->length() || length <= 0)
    return &empty_string;

  tmp_length=(int32) res->length()-start;
  length=min(length,tmp_length);

  if (!start && res->length() == (uint) length)
    return res;
  tmp_value.set(*res,(uint) start,(uint) length);
  return &tmp_value;
}


void Item_func_substr::fix_length_and_dec()
{
  max_length=args[0]->max_length;

  if (args[1]->const_item())
  {
    int32 start=(int32) args[1]->val_int()-1;
    if (start < 0 || start >= (int32) max_length)
      max_length=0; /* purecov: inspected */
    else
      max_length-= (uint) start;
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32 length= (int32) args[2]->val_int();
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
}


String *Item_func_substr_index::val_str(String *str)
{
  String *res =args[0]->val_str(str);
  String *delimeter =args[1]->val_str(&tmp_value);
  int32 count = (int32) args[2]->val_int();
  uint offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint delimeter_length=delimeter->length();
  if (!res->length() || !delimeter_length || !count)
    return &empty_string;		// Wrong parameters

#ifdef USE_MB
  if (use_mb(res->charset()) && !binary())
  {
    const char *ptr=res->ptr();
    const char *strend = ptr+res->length();
    const char *end=strend-delimeter_length+1;
    const char *search=delimeter->ptr();
    const char *search_end=search+delimeter_length;
    int32 n=0,c=count,pass;
    register uint32 l;
    for (pass=(count>0);pass<2;++pass)
    {
      while (ptr < end)
      {
        if (*ptr == *search)
        {
	  register char *i,*j;
	  i=(char*) ptr+1; j=(char*) search+1;
	  while (j != search_end)
	    if (*i++ != *j++) goto skipp;
	  if (pass==0) ++n;
	  else if (!--c) break;
	  ptr+=delimeter_length;
	  continue;
	}
    skipp:
        if ((l=my_ismbchar(res->charset(), ptr,strend))) ptr+=l;
        else ++ptr;
      } /* either not found or got total number when count<0 */
      if (pass == 0) /* count<0 */
      {
        c+=n+1;
        if (c<=0) return res; /* not found, return original string */
        ptr=res->ptr();
      }
      else
      {
        if (c) return res; /* Not found, return original string */
        if (count>0) /* return left part */
        {
	  tmp_value.set(*res,0,(ulong) (ptr-res->ptr()));
        }
        else /* return right part */
        {
	  ptr+=delimeter_length;
	  tmp_value.set(*res,(ulong) (ptr-res->ptr()), (ulong) (strend-ptr));
        }
      }
    }
  }
  else
#endif /* USE_MB */
  {
    if (count > 0)
    {					// start counting from the beginning
      for (offset=0 ;; offset+=delimeter_length)
      {
	if ((int) (offset=res->strstr(*delimeter,offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!--count)
	{
	  tmp_value.set(*res,0,offset);
	  break;
	}
      }
    }
    else
    {					// Start counting at end
      for (offset=res->length() ; ; offset-=delimeter_length-1)
      {
	if ((int) (offset=res->strrstr(*delimeter,offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!++count)
	{
	  offset+=delimeter_length;
	  tmp_value.set(*res,offset,res->length()- offset);
	  break;
	}
      }
    }
  }
  return (&tmp_value);
}

/*
** The trim functions are extension to ANSI SQL because they trim substrings
** They ltrim() and rtrim() functions are optimized for 1 byte strings
** They also return the original string if possible, else they return
** a substring that points at the original string.
*/


String *Item_func_ltrim::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),res->charset());
  String *remove_str=args[1]->val_str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
    while (ptr != end && *ptr == chr)
      ptr++;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
    end-=remove_length;
    while (ptr < end && !memcmp(ptr,r_ptr,remove_length))
      ptr+=remove_length;
    end+=remove_length;
  }
  if (ptr == res->ptr())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_rtrim::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),res->charset());
  String *remove_str=args[1]->val_str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
#ifdef USE_MB
  char *p=ptr;
  register uint32 l;
#endif
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
#ifdef USE_MB
    if (use_mb(res->charset()) && !binary())
    {
      while (ptr < end)
      {
	if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l,p=ptr;
	else ++ptr;
      }
      ptr=p;
    }
#endif
    while (ptr != end  && end[-1] == chr)
      end--;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
#ifdef USE_MB
    if (use_mb(res->charset()) && !binary())
    {
  loop:
      while (ptr + remove_length < end)
      {
	if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l;
	else ++ptr;
      }
      if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
      {
	end-=remove_length;
	ptr=p;
	goto loop;
      }
    }
    else
#endif /* USE_MB */
    {
      while (ptr + remove_length < end &&
	     !memcmp(end-remove_length,r_ptr,remove_length))
	end-=remove_length;
    }
  }
  if (end == res->ptr()+res->length())
    return res;
  tmp_value.set(*res,0,(uint) (end-res->ptr()));
  return &tmp_value;
}


String *Item_func_trim::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),res->charset());
  String *remove_str=args[1]->val_str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  const char *r_ptr=remove_str->ptr();
  while (ptr+remove_length <= end && !memcmp(ptr,r_ptr,remove_length))
    ptr+=remove_length;
#ifdef USE_MB
  if (use_mb(res->charset()) && !binary())
  {
    char *p=ptr;
    register uint32 l;
 loop:
    while (ptr + remove_length < end)
    {
      if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l;
      else ++ptr;
    }
    if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
    {
      end-=remove_length;
      ptr=p;
      goto loop;
    }
    ptr=p;
  }
  else
#endif /* USE_MB */
  {
    while (ptr + remove_length <= end &&
	   !memcmp(end-remove_length,r_ptr,remove_length))
      end-=remove_length;
  }
  if (ptr == res->ptr() && end == ptr+res->length())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}

void Item_func_password::fix_length_and_dec()
{
  max_length= get_password_length(use_old_passwords);
}

/*
 Password() function has 2 arguments. Second argument can be used
 to make results repeatable
*/ 

String *Item_func_password::val_str(String *str)
{
  struct rand_struct rand_st; // local structure for 2 param version
  ulong  seed=0;              // seed to initialise random generator to
  
  String *res  =args[0]->val_str(str);          
  if ((null_value=args[0]->null_value))
    return 0;
  
  if (arg_count == 1)
  {    
    if (res->length() == 0)
      return &empty_string;
    make_scrambled_password(tmp_value,res->c_ptr(),use_old_passwords,
                            &current_thd->rand);
    str->set(tmp_value,get_password_length(use_old_passwords),res->charset());
    return str;
  }
  else
  {
   /* We'll need the buffer to get second parameter */
    char key_buff[80];
    String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
    String *key  =args[1]->val_str(&tmp_key_value);          
    
    /* Check second argument for NULL value. First one is already checked */
    if ((null_value=args[1]->null_value))
      return 0;
      
    /* This shall be done after checking for null for proper results */       
    if (res->length() == 0)
      return &empty_string;  
      
    /* Generate the seed first this allows to avoid double allocation */  
    char* seed_ptr=key->c_ptr();
    while (*seed_ptr)
    {
      seed=(seed*211+*seed_ptr) & 0xffffffffL; /* Use simple hashing */
      seed_ptr++;
    }
    
    /* Use constants which allow nice random values even with small seed */
    randominit(&rand_st,
	       (ulong) ((ulonglong) seed*111111+33333333L) & (ulong) 0xffffffff,
	       (ulong) ((ulonglong) seed*1111+55555555L) & (ulong) 0xffffffff);
    
    make_scrambled_password(tmp_value,res->c_ptr(),use_old_passwords,
                            &rand_st);
    str->set(tmp_value,get_password_length(use_old_passwords),res->charset());
    return str;
  }       
}

String *Item_func_old_password::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;
  make_scrambled_password(tmp_value,res->c_ptr(),1,&current_thd->rand);
  str->set(tmp_value,16,res->charset());
  return str;
}



#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::val_str(String *str)
{
  String *res  =args[0]->val_str(str);

#ifdef HAVE_CRYPT
  char salt[3],*salt_ptr;
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;

  if (arg_count == 1)
  {					// generate random salt
    time_t timestamp=current_thd->query_start();
    salt[0] = bin_to_ascii( (ulong) timestamp & 0x3f);
    salt[1] = bin_to_ascii(( (ulong) timestamp >> 5) & 0x3f);
    salt[2] = 0;
    salt_ptr=salt;
  }
  else
  {					// obtain salt from the first two bytes
    String *salt_str=args[1]->val_str(&tmp_value);
    if ((null_value= (args[1]->null_value || salt_str->length() < 2)))
      return 0;
    salt_ptr= salt_str->c_ptr();
  }
  pthread_mutex_lock(&LOCK_crypt);
  char *tmp=crypt(res->c_ptr(),salt_ptr);
  str->set(tmp,(uint) strlen(tmp),res->charset());
  str->copy();
  pthread_mutex_unlock(&LOCK_crypt);
  return str;
#else
  null_value=1;
  return 0;
#endif	/* HAVE_CRYPT */
}

void Item_func_encode::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  maybe_null=args[0]->maybe_null;
}

String *Item_func_encode::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.encode((char*) res->ptr(),res->length());
  return res;
}

String *Item_func_decode::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.decode((char*) res->ptr(),res->length());
  return res;
}


String *Item_func_database::val_str(String *str)
{
  THD *thd= current_thd;
  if (!thd->db)
    str->length(0);
  else
    str->copy((const char*) thd->db,(uint) strlen(thd->db),
	      system_charset_info, thd->variables.thd_charset);
  return str;
}

String *Item_func_user::val_str(String *str)
{
  THD          *thd=current_thd;
  CHARSET_INFO *cs=thd->variables.thd_charset;
  const char   *host=thd->host ? thd->host : thd->ip ? thd->ip : "";
  uint32       res_length=(strlen(thd->user)+strlen(host)+10) * cs->mbmaxlen;

  if (str->alloc(res_length))
  {
      null_value=1;
      return 0;
  }
  res_length=cs->snprintf(cs, (char*)str->ptr(), res_length, "%s@%s",thd->user,host);
  str->length(res_length);
  str->set_charset(cs);
  return str;
}

void Item_func_soundex::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  set_if_bigger(max_length,4);
}


  /*
    If alpha, map input letter to soundex code.
    If not alpha and remove_garbage is set then skip to next char
    else return 0
    */

extern "C" {
extern const char *soundex_map;		// In mysys/static.c
}

static char get_scode(CHARSET_INFO *cs,char *ptr)
{
  uchar ch=my_toupper(cs,*ptr);
  if (ch < 'A' || ch > 'Z')
  {
					// Thread extended alfa (country spec)
    return '0';				// as vokal
  }
  return(soundex_map[ch-'A']);
}


String *Item_func_soundex::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  char last_ch,ch;
  CHARSET_INFO *cs= &my_charset_latin1;

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  if (tmp_value.alloc(max(res->length(),4)))
    return str; /* purecov: inspected */
  char *to= (char *) tmp_value.ptr();
  char *from= (char *) res->ptr(), *end=from+res->length();
  tmp_value.set_charset(cs);
  
  while (from != end && my_isspace(cs,*from)) // Skip pre-space
    from++; /* purecov: inspected */
  if (from == end)
    return &empty_string;		// No alpha characters.
  *to++ = my_toupper(cs,*from);		// Copy first letter
  last_ch = get_scode(cs,from);		// code of the first letter
					// for the first 'double-letter check.
					// Loop on input letters until
					// end of input (null) or output
					// letter code count = 3
  for (from++ ; from < end ; from++)
  {
    if (!my_isalpha(cs,*from))
      continue;
    ch=get_scode(cs,from);
    if ((ch != '0') && (ch != last_ch)) // if not skipped or double
    {
       *to++ = ch;			// letter, copy to output
       last_ch = ch;			// save code of last input letter
    }					// for next double-letter check
  }
  for (end=(char*) tmp_value.ptr()+4 ; to < end ; to++)
    *to = '0';
  *to=0;				// end string
  tmp_value.length((uint) (to-tmp_value.ptr()));
  return &tmp_value;
}


/*
** Change a number to format '3,333,333,333.000'
** This should be 'internationalized' sometimes.
*/

Item_func_format::Item_func_format(Item *org,int dec) :Item_str_func(org)
{
  decimals=(uint) set_zone(dec,0,30);
}


String *Item_func_format::val_str(String *str)
{
  double nr	=args[0]->val();
  uint32 diff,length,str_length;
  uint dec;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  dec= decimals ? decimals+1 : 0;
  str->set(nr,decimals,thd_charset());
  str_length=str->length();
  if (nr < 0)
    str_length--;				// Don't count sign
  length=str->length()+(diff=(str_length- dec-1)/3);
  if (diff)
  {
    char *tmp,*pos;
    str=copy_if_not_alloced(&tmp_str,str,length);
    str->length(length);
    tmp=(char*) str->ptr()+length - dec-1;
    for (pos=(char*) str->ptr()+length ; pos != tmp; pos--)
      pos[0]=pos[- (int) diff];
    while (diff)
    {
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=',';
      pos--;
      diff--;
    }
  }
  return str;
}


void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
  for (uint i=1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
  with_sum_func= with_sum_func || item->with_sum_func;
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


void Item_func_elt::split_sum_func(Item **ref_pointer_array,
				   List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(ref_pointer_array, fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    fields.push_front(item);
    ref_pointer_array[el]= item;
    item= new Item_ref(ref_pointer_array + el, 0, item->name);
  }
  Item_str_func::split_sum_func(ref_pointer_array, fields);
}


void Item_func_elt::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


double Item_func_elt::val()
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  return args[tmp-1]->val();
}

longlong Item_func_elt::val_int()
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return args[tmp-1]->val_int();
}

String *Item_func_elt::val_str(String *str)
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return NULL;
  }
  null_value=0;
  return args[tmp-1]->val_str(str);
}


void Item_func_make_set::split_sum_func(Item **ref_pointer_array,
					List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(ref_pointer_array, fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    fields.push_front(item);
    ref_pointer_array[el]= item;
    item= new Item_ref(ref_pointer_array + el, 0, item->name);
  }
  Item_str_func::split_sum_func(ref_pointer_array, fields);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;
  for (uint i=1 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
  with_sum_func= with_sum_func || item->with_sum_func;
}


void Item_func_make_set::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


String *Item_func_make_set::val_str(String *str)
{
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args;
  String *result=&empty_string;

  bits=item->val_int();
  if ((null_value=item->null_value))
    return NULL;

  if (arg_count < 64)
    bits &= ((ulonglong) 1 << arg_count)-1;

  for (; bits; bits >>= 1, ptr++)
  {
    if (bits & 1)
    {
      String *res= (*ptr)->val_str(str);
      if (res)					// Skip nulls
      {
	if (!first_found)
	{					// First argument
	  first_found=1;
	  if (res != str)
	    result=res;				// Use original string
	  else
	  {
	    if (tmp_str.copy(*res))		// Don't use 'str'
	      return &empty_string;
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    if (tmp_str.alloc(result->length()+res->length()+1) ||
		tmp_str.copy(*result))
	      return &empty_string;
	    result= &tmp_str;
	  }
	  if (tmp_str.append(',') || tmp_str.append(*res))
	    return &empty_string;
	}
      }
    }
  }
  return result;
}


String *Item_func_char::val_str(String *str)
{
  str->length(0);
  for (uint i=0 ; i < arg_count ; i++)
  {
    int32 num=(int32) args[i]->val_int();
    if (!args[i]->null_value)
#ifdef USE_MB
      if (use_mb(charset()))
      {
        if (num&0xFF000000L) {
           str->append((char)(num>>24));
           goto b2;
        } else if (num&0xFF0000L) {
b2:        str->append((char)(num>>16));
           goto b1;
        } else if (num&0xFF00L) {
b1:        str->append((char)(num>>8));
        }
      }
#endif
      str->append((char)num);
  }
  str->realloc(str->length());			// Add end 0 (for Purify)
  return str;
}


inline String* alloc_buffer(String *res,String *str,String *tmp_value,
			    ulong length)
{
  if (res->alloced_length() < length)
  {
    if (str->alloced_length() >= length)
    {
      (void) str->copy(*res);
      str->length(length);
      return str;
    }
    else
    {
      if (tmp_value->alloc(length))
	return 0;
      (void) tmp_value->copy(*res);
      tmp_value->length(length);
      return tmp_value;
    }
  }
  res->length(length);
  return res;
}


void Item_func_repeat::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    max_length=(long) (args[0]->max_length * args[1]->val_int());
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}

/*
** Item_func_repeat::str is carefully written to avoid reallocs
** as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::val_str(String *str)
{
  uint length,tot_length;
  char *to;
  long count= (long) args[1]->val_int();
  String *res =args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value=0;
  if (count <= 0)			// For nicer SQL code
    return &empty_string;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  // Safe length check
  if (length > current_thd->variables.max_allowed_packet/count)
    goto err;				// Probably an error
  tot_length= length*(uint) count;
  if (!(res= alloc_buffer(res,str,&tmp_value,tot_length)))
    goto err;

  to=(char*) res->ptr()+length;
  while (--count)
  {
    memcpy(to,res->ptr(),length);
    to+=length;
  }
  return (res);

err:
  null_value=1;
  return 0;
}


void Item_func_rpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_rpad::val_str(String *str)
{
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  int32 count= (int32) args[1]->val_int();
  String *res =args[0]->val_str(str);
  String *rpad = args[2]->val_str(str);

  if (!res || args[1]->null_value || !rpad)
    goto err;
  null_value=0;
  if (count <= (int32) (res_length=res->length()))
  {						// String to pad is big enough
    res->length(count);				// Shorten result if longer
    return (res);
  }
  length_pad= rpad->length();
  if ((ulong) count > current_thd->variables.max_allowed_packet ||
      args[2]->null_value || !length_pad)
    goto err;
  if (!(res= alloc_buffer(res,str,&tmp_value,count)))
    goto err;

  to= (char*) res->ptr()+res_length;
  ptr_pad=rpad->ptr();
  for (count-= res_length; (uint32) count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


void Item_func_lpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lpad::val_str(String *str)
{
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  ulong count= (long) args[1]->val_int();
  String *res= args[0]->val_str(str);
  String *lpad= args[2]->val_str(str);

  if (!res || args[1]->null_value || !lpad)
    goto err;
  null_value=0;
  if (count <= (res_length=res->length()))
  {						// String to pad is big enough
    res->length(count);				// Shorten result if longer
    return (res);
  }
  length_pad= lpad->length();
  if (count > current_thd->variables.max_allowed_packet ||
      args[2]->null_value || !length_pad)
    goto err;

  if (res->alloced_length() < count)
  {
    if (str->alloced_length() >= count)
    {
      memcpy((char*) str->ptr()+(count-res_length),res->ptr(),res_length);
      res=str;
    }
    else
    {
      if (tmp_value.alloc(count))
	goto err;
      memcpy((char*) tmp_value.ptr()+(count-res_length),res->ptr(),res_length);
      res=&tmp_value;
    }
  }
  else
    bmove_upp((char*) res->ptr()+count,res->ptr()+res_length,res_length);
  res->length(count);

  to= (char*) res->ptr();
  ptr_pad= lpad->ptr();
  for (count-= res_length; count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


String *Item_func_conv::val_str(String *str)
{
  String *res= args[0]->val_str(str);
  char *endptr,ans[65],*ptr;
  longlong dec;
  int from_base= (int) args[1]->val_int();
  int to_base= (int) args[2]->val_int();
  int err;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (from_base < 0)
    dec= my_strntoll(res->charset(),res->ptr(),res->length(),-from_base,&endptr,&err);
  else
    dec= (longlong) my_strntoull(res->charset(),res->ptr(),res->length(),from_base,&endptr,&err);
  ptr= longlong2str(dec,ans,to_base);
  if (str->copy(ans,(uint32) (ptr-ans), thd_charset()))
    return &empty_string;
  return str;
}


String *Item_func_conv_charset::val_str(String *str)
{
  my_wc_t wc;
  int cnvres;
  const uchar *s, *se;
  uchar *d, *d0, *de;
  uint32 dmaxlen;
  String *arg= args[0]->val_str(str);
  CHARSET_INFO *from,*to;

  if (!arg)
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  from=arg->charset();
  to=conv_charset;

  s=(const uchar*)arg->ptr();
  se=s+arg->length();

  dmaxlen=arg->length()*to->mbmaxlen+1;
  str->alloc(dmaxlen);
  d0=d=(unsigned char*)str->ptr();
  de=d+dmaxlen;

  while (1)
  {
    cnvres=from->mb_wc(from,&wc,s,se);
    if (cnvres>0)
    {
      s+=cnvres;
    }
    else if (cnvres==MY_CS_ILSEQ)
    {
      s++;
      wc='?';
    }
    else
      break;

outp:
    cnvres=to->wc_mb(to,wc,d,de);
    if (cnvres>0)
    {
      d+=cnvres;
    }
    else if (cnvres==MY_CS_ILUNI && wc!='?')
    {
        wc='?';
        goto outp;
    }
    else
      break;
  };

  str->length((uint32) (d-d0));
  str->set_charset(to);
  return str;
}

void Item_func_conv_charset::fix_length_and_dec()
{
  max_length = args[0]->max_length*conv_charset->mbmaxlen;
  set_charset(conv_charset);
}



String *Item_func_conv_charset3::val_str(String *str)
{
  my_wc_t wc;
  int cnvres;
  const uchar *s, *se;
  uchar *d, *d0, *de;
  uint32 dmaxlen;
  String *arg= args[0]->val_str(str);
  String *to_cs= args[1]->val_str(str);
  String *from_cs= args[2]->val_str(str);
  CHARSET_INFO *from_charset;
  CHARSET_INFO *to_charset;

  if (!arg     || args[0]->null_value ||
      !to_cs   || args[1]->null_value ||
      !from_cs || args[2]->null_value ||
      !(from_charset=get_charset_by_name(from_cs->ptr(), MYF(MY_WME))) ||
      !(to_charset=get_charset_by_name(to_cs->ptr(), MYF(MY_WME))))
  {
    null_value=1;
    return 0;
  }

  s=(const uchar*)arg->ptr();
  se=s+arg->length();

  dmaxlen=arg->length()*to_charset->mbmaxlen+1;
  str->alloc(dmaxlen);
  d0=d=(unsigned char*)str->ptr();
  de=d+dmaxlen;

  while (1)
  {
    cnvres=from_charset->mb_wc(from_charset,&wc,s,se);
    if (cnvres>0)
    {
      s+=cnvres;
    }
    else if (cnvres==MY_CS_ILSEQ)
    {
      s++;
      wc='?';
    }
    else
      break;

outp:
    cnvres=to_charset->wc_mb(to_charset,wc,d,de);
    if (cnvres>0)
    {
      d+=cnvres;
    }
    else if (cnvres==MY_CS_ILUNI && wc!='?')
    {
        wc='?';
        goto outp;
    }
    else
      break;
  };

  str->length((uint32) (d-d0));
  str->set_charset(to_charset);
  return str;
}


bool Item_func_conv_charset::fix_fields(THD *thd,struct st_table_list *tables, Item **ref)
{
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
  used_tables_cache=0;
  const_item_cache=1;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error if flag is set!
  if (args[0]->fix_fields(thd, tables, args) || args[0]->check_cols(1))
    return 1;
  maybe_null=args[0]->maybe_null;
  const_item_cache=args[0]->const_item();
  set_charset(conv_charset);
  fix_length_and_dec();
  fixed= 1;
  return 0;
}


void Item_func_conv_charset3::fix_length_and_dec()
{
  max_length = args[0]->max_length;
}

String *Item_func_set_collation::val_str(String *str)
{
  str=args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  str->set_charset(set_collation);
  return str;
}

bool Item_func_set_collation::fix_fields(THD *thd,struct st_table_list *tables, Item **ref)
{
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
  used_tables_cache=0;
  const_item_cache=1;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error if flag is set!
  if (args[0]->fix_fields(thd, tables, args) || args[0]->check_cols(1))
    return 1;
  maybe_null=args[0]->maybe_null;
  set_charset(set_collation);
  with_sum_func= with_sum_func || args[0]->with_sum_func;
  used_tables_cache=args[0]->used_tables();
  const_item_cache=args[0]->const_item();
  fix_length_and_dec();
  fixed= 1;
  return 0;
}

bool Item_func_set_collation::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      func_name() != item_func->func_name())
    return 0;
  Item_func_set_collation *item_func_sc=(Item_func_set_collation*) item;
  if (set_collation != item_func_sc->set_collation)
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func_sc->args[i], binary_cmp))
      return 0;
  return 1;
}

String *Item_func_charset::val_str(String *str)
{
  String *res = args[0]->val_str(str);

  if ((null_value=(args[0]->null_value || !res->charset())))
    return 0;
  str->copy(res->charset()->csname,strlen(res->charset()->csname),
	    &my_charset_latin1, thd_charset());
  return str;
}

String *Item_func_collation::val_str(String *str)
{
  String *res = args[0]->val_str(str);

  if ((null_value=(args[0]->null_value || !res->charset())))
    return 0;
  str->copy(res->charset()->name,strlen(res->charset()->name),
	    &my_charset_latin1, thd_charset());
  return str;
}


String *Item_func_hex::val_str(String *str)
{
  if (args[0]->result_type() != STRING_RESULT)
  {
    /* Return hex of unsigned longlong value */
    longlong dec= args[0]->val_int();
    char ans[65],*ptr;
    if ((null_value= args[0]->null_value))
      return 0;
    ptr= longlong2str(dec,ans,16);
    if (str->copy(ans,(uint32) (ptr-ans),default_charset_info))
      return &empty_string;			// End of memory
    return str;
  }

  /* Convert given string to a hex string, character by character */
  String *res= args[0]->val_str(str);
  const char *from, *end;
  char *to;
  if (!res || tmp_value.alloc(res->length()*2))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  tmp_value.length(res->length()*2);
  for (from=res->ptr(), end=from+res->length(), to= (char*) tmp_value.ptr();
       from != end ;
       from++, to+=2)
  {
    uint tmp=(uint) (uchar) *from;
    to[0]=_dig_vec[tmp >> 4];
    to[1]=_dig_vec[tmp & 15];
  }
  return &tmp_value;
}


#include <my_dir.h>				// For my_stat

String *Item_load_file::val_str(String *str)
{
  String *file_name;
  File file;
  MY_STAT stat_info;
  DBUG_ENTER("load_file");

  if (!(file_name= args[0]->val_str(str)) ||
      !(current_thd->master_access & FILE_ACL) ||
      !my_stat(file_name->c_ptr(), &stat_info, MYF(MY_WME)))
    goto err;
  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) current_thd->variables.max_allowed_packet)
  {
    /* my_error(ER_TOO_LONG_STRING, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (tmp_value.alloc(stat_info.st_size))
    goto err;
  if ((file = my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (my_read(file, (byte*) tmp_value.ptr(), stat_info.st_size, MYF(MY_NABP)))
  {
    my_close(file, MYF(0));
    goto err;
  }
  tmp_value.length(stat_info.st_size);
  my_close(file, MYF(0));
  null_value = 0;
  return &tmp_value;

err:
  null_value = 1;
  DBUG_RETURN(0);
}


String* Item_func_export_set::val_str(String* str)
{
  ulonglong the_set = (ulonglong) args[0]->val_int();
  String yes_buf, *yes;
  yes = args[1]->val_str(&yes_buf);
  String no_buf, *no;
  no = args[2]->val_str(&no_buf);
  String *sep = NULL, sep_buf ;

  uint num_set_values = 64;
  ulonglong mask = 0x1;
  str->length(0);

  /* Check if some argument is a NULL value */
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {
    null_value=1;
    return 0;
  }
  switch(arg_count) {
  case 5:
    num_set_values = (uint) args[4]->val_int();
    if (num_set_values > 64)
      num_set_values=64;
    if (args[4]->null_value)
    {
      null_value=1;
      return 0;
    }
    /* Fall through */
  case 4:
    if (!(sep = args[3]->val_str(&sep_buf)))	// Only true if NULL
    {
      null_value=1;
      return 0;
    }
    break;
  case 3:
    sep_buf.set(",", 1, default_charset_info);
    sep = &sep_buf;
  }
  null_value=0;

  for (uint i = 0; i < num_set_values; i++, mask = (mask << 1))
  {
    if (the_set & mask)
      str->append(*yes);
    else
      str->append(*no);
    if (i != num_set_values - 1)
      str->append(*sep);
  }
  return str;
}

void Item_func_export_set::fix_length_and_dec()
{
  uint length=max(args[1]->max_length,args[2]->max_length);
  uint sep_length=(arg_count > 3 ? args[3]->max_length : 1);
  max_length=length*64+sep_length*63;
}

String* Item_func_inet_ntoa::val_str(String* str)
{
  uchar buf[8], *p;
  ulonglong n = (ulonglong) args[0]->val_int();
  char num[4];

  /*
    We do not know if args[0] is NULL until we have called
    some val function on it if args[0] is not a constant!

    Also return null if n > 255.255.255.255
  */
  if ((null_value= (args[0]->null_value || n > (ulonglong) LL(4294967295))))
    return 0;					// Null value

  str->length(0);
  int4store(buf,n);

  /* Now we can assume little endian. */

  num[3]='.';
  for (p=buf+4 ; p-- > buf ; )
  {
    uint c = *p;
    uint n1,n2;					// Try to avoid divisions
    n1= c / 100;				// 100 digits
    c-= n1*100;
    n2= c / 10;					// 10 digits
    c-=n2*10;					// last digit
    num[0]=(char) n1+'0';
    num[1]=(char) n2+'0';
    num[2]=(char) c+'0';
    uint length=(n1 ? 4 : n2 ? 3 : 2);		// Remove pre-zero

    (void) str->append(num+4-length,length);
  }
  str->length(str->length()-1);			// Remove last '.';
  return str;
}


/*
  QUOTE() function returns argument string in single quotes suitable for
  using in a SQL statement.

  DESCRIPTION
    Adds a \ before all characters that needs to be escaped in a SQL string.
    We also escape '^Z' (END-OF-FILE in windows) to avoid probelms when
    running commands from a file in windows.

    This function is very useful when you want to generate SQL statements

    RETURN VALUES
    str		Quoted string
    NULL	Argument to QUOTE() was NULL or out of memory.
*/

#define get_esc_bit(mask, num) (1 & (*((mask) + ((num) >> 3))) >> ((num) & 7))

String *Item_func_quote::val_str(String *str)
{
  /*
    Bit mask that has 1 for set for the position of the following characters:
    0, \, ' and ^Z
  */

  static uchar escmask[32]=
  {
    0x01, 0x00, 0x00, 0x04, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  char *from, *to, *end, *start;
  String *arg= args[0]->val_str(str);
  uint arg_length, new_length;
  if (!arg)					// Null argument
    goto null;
  arg_length= arg->length();
  new_length= arg_length+2; /* for beginning and ending ' signs */

  for (from= (char*) arg->ptr(), end= from + arg_length; from < end; from++)
    new_length+= get_esc_bit(escmask, *from);

  /*
    We have to use realloc() instead of alloc() as we want to keep the
    old result in str
  */
  if (str->realloc(new_length))
    goto null;

  /*
    As 'arg' and 'str' may be the same string, we must replace characters
    from the end to the beginning
  */
  to= (char*) str->ptr() + new_length - 1;
  *to--= '\'';
  for (start= (char*) arg->ptr(),end= start + arg_length; end-- != start; to--)
  {
    /*
      We can't use the bitmask here as we want to replace \O and ^Z with 0
      and Z
    */
    switch (*end)  {
    case 0:
      *to--= '0';
      *to=   '\\';
      break;
    case '\032':
      *to--= 'Z';
      *to=   '\\';
      break;
    case '\'':
    case '\\':
      *to--= *end;
      *to=   '\\';
      break;
    default:
      *to= *end;
      break;
    }
  }
  *to= '\'';
  str->length(new_length);
  return str;

null:
  null_value= 1;
  return 0;
}


/*******************************************************
General functions for spatial objects
********************************************************/

String *Item_func_geometry_from_text::val_str(String *str)
{
  Geometry geom;
  String arg_val;
  String *wkt = args[0]->val_str(&arg_val);
  GTextReadStream trs(wkt->ptr(), wkt->length());

  str->length(0);
  if ((null_value=(args[0]->null_value || geom.create_from_wkt(&trs, str, 0))))
    return 0;
  return str;
}


void Item_func_geometry_from_text::fix_length_and_dec()
{
  max_length=MAX_BLOB_WIDTH;
}


String *Item_func_as_text::val_str(String *str)
{
  String arg_val;
  String *wkt = args[0]->val_str(&arg_val);
  Geometry geom;

  if ((null_value=(args[0]->null_value ||
                   geom.create_from_wkb(wkt->ptr(),wkt->length()))))
    return 0;

  str->length(0);

  if ((null_value=geom.as_wkt(str)))
    return 0;

  return str;
}

void Item_func_as_text::fix_length_and_dec()
{
  max_length=MAX_BLOB_WIDTH;
}

String *Item_func_geometry_type::val_str(String *str)
{
  String *wkt = args[0]->val_str(str);
  Geometry geom;

  if ((null_value=(args[0]->null_value ||
                   geom.create_from_wkb(wkt->ptr(),wkt->length()))))
    return 0;
  str->copy(geom.get_class_info()->m_name,
	    strlen(geom.get_class_info()->m_name),
	    default_charset_info);
  return str;
}


String *Item_func_envelope::val_str(String *str)
{
  String *res = args[0]->val_str(str);
  Geometry geom;
  
  if ((null_value = args[0]->null_value ||
               geom.create_from_wkb(res->ptr(),res->length())))
    return 0;
  
  res->length(0);
  return (null_value= geom.envelope(res)) ? 0 : res;
}


String *Item_func_centroid::val_str(String *str)
{
  String arg_val;
  String *wkb = args[0]->val_str(&arg_val);
  Geometry geom;

  null_value = args[0]->null_value ||
               geom.create_from_wkb(wkb->ptr(),wkb->length()) ||
               !GEOM_METHOD_PRESENT(geom,centroid) ||
               geom.centroid(str);

  return null_value ? 0: str;
}


/***********************************************
  Spatial decomposition functions
***********************************************/

String *Item_func_spatial_decomp::val_str(String *str)
{
  String arg_val;
  String *wkb = args[0]->val_str(&arg_val);
  Geometry geom;

  if ((null_value = (args[0]->null_value ||
                     geom.create_from_wkb(wkb->ptr(),wkb->length()))))
    return 0;

  null_value=1;
  str->length(0);
  switch(decomp_func)
  {
    case SP_STARTPOINT:
      if (!GEOM_METHOD_PRESENT(geom,start_point) || geom.start_point(str))
        goto ret;
      break;

    case SP_ENDPOINT:
      if (!GEOM_METHOD_PRESENT(geom,end_point) || geom.end_point(str))
        goto ret;
      break;

    case SP_EXTERIORRING:
      if (!GEOM_METHOD_PRESENT(geom,exterior_ring) || geom.exterior_ring(str))
        goto ret;
      break;

    default:
      goto ret;
  }
  null_value=0;

ret:
  return null_value ? 0 : str;
}


String *Item_func_spatial_decomp_n::val_str(String *str)
{
  String arg_val;
  String *wkb  =        args[0]->val_str(&arg_val);
  long n       = (long) args[1]->val_int();
  Geometry geom;

  if ((null_value = (args[0]->null_value ||
                     args[1]->null_value ||
                     geom.create_from_wkb(wkb->ptr(),wkb->length()) )))
    return 0;

  null_value=1;

  switch(decomp_func_n)
  {
    case SP_POINTN:
      if (!GEOM_METHOD_PRESENT(geom,point_n) ||
          geom.point_n(n,str))
        goto ret;
      break;

    case SP_GEOMETRYN:
      if (!GEOM_METHOD_PRESENT(geom,geometry_n) ||
          geom.geometry_n(n,str))
        goto ret;
      break;

    case SP_INTERIORRINGN:
      if (!GEOM_METHOD_PRESENT(geom,interior_ring_n) ||
          geom.interior_ring_n(n,str))
        goto ret;
      break;

    default:
      goto ret;
  }
  null_value=0;

ret:
  return null_value ? 0 : str;
}



/***********************************************
Functions to concatinate various spatial objects
************************************************/


/*
*  Concatinate doubles into Point
*/


String *Item_func_point::val_str(String *str)
{
  double x= args[0]->val();
  double y= args[1]->val();

  if ( (null_value = (args[0]->null_value ||
                     args[1]->null_value ||
                     str->realloc(1+4+8+8))))
    return 0;

  str->length(0);
  str->q_append((char)Geometry::wkbNDR);
  str->q_append((uint32)Geometry::wkbPoint);
  str->q_append(x);
  str->q_append(y);
  return str;
}


/*
  Concatinates various items into various collections
  with checkings for valid wkb type of items.
  For example, MultiPoint can be a collection of Points only.
  coll_type contains wkb type of target collection.
  item_type contains a valid wkb type of items.
  In the case when coll_type is wkbGeometryCollection,
  we do not check wkb type of items, any is valid.
*/

String *Item_func_spatial_collection::val_str(String *str)
{
  String arg_value;
  uint i;

  null_value=1;

  str->length(0);
  if (str->reserve(9,512))
    return 0;

  str->q_append((char)Geometry::wkbNDR);
  str->q_append((uint32)coll_type);
  str->q_append((uint32)arg_count);

  for (i = 0; i < arg_count; ++i)
  {
    String *res = args[i]->val_str(&arg_value);
    if (args[i]->null_value)
      goto ret;

    if ( coll_type == Geometry::wkbGeometryCollection )
    {
      /*
         In the case of GeometryCollection we don't need
         any checkings for item types, so just copy them
         into target collection
      */
      if ((null_value=(str->reserve(res->length(),512))))
        goto ret;

      str->q_append(res->ptr(),res->length());
    }
    else
    {
      enum Geometry::wkbType wkb_type;
      uint32 len=res->length();
      const char *data=res->ptr()+1;

      /*
         In the case of named collection we must to
         check that items are of specific type, let's
         do this checking now
      */

      if (len < 5)
        goto ret;
      wkb_type= (Geometry::wkbType) uint4korr(data);
      data+=4;
      len-=5;
      if (wkb_type != item_type)
        goto ret;

      switch (coll_type) {
      case Geometry::wkbMultiPoint:
      case Geometry::wkbMultiLineString:
      case Geometry::wkbMultiPolygon:
	if (len < WKB_HEADER_SIZE)
	  goto ret;

	data-=WKB_HEADER_SIZE;
	len+=WKB_HEADER_SIZE;
	if (str->reserve(len,512))
	  goto ret;
	str->q_append(data,len);
	break;

      case Geometry::wkbLineString:
	if (str->reserve(POINT_DATA_SIZE,512))
	  goto ret;
	str->q_append(data,POINT_DATA_SIZE);
	break;

      case Geometry::wkbPolygon:
      {
	uint32 n_points;
	double x1, y1, x2, y2;

	if (len < 4 + 2 * POINT_DATA_SIZE)
	  goto ret;

	uint32 llen=len;
	const char *ldata=data;

	n_points=uint4korr(data);
	data+=4;
	float8get(x1,data);
	data+=8;
	float8get(y1,data);
	data+=8;

	data+=(n_points-2) * POINT_DATA_SIZE;

	float8get(x2,data);
	float8get(y2,data+8);

	if ((x1 != x2) || (y1 != y2))
	  goto ret;

	if (str->reserve(llen,512))
	  goto ret;
	str->q_append(ldata, llen);
      }
      break;

      default:
	goto ret;
      }
    }
  }

  if (str->length() > current_thd->variables.max_allowed_packet)
    goto ret;

  null_value = 0;

ret:
  return null_value ? 0 : str;
}
