#include "Pg.h"
#include "types.h"
#include <assert.h>

/*
 *	These are not being set automatically so I have set them
 *	assuming libpq has them.  bjm 2003-04-01
 */
#define HAVE_PQescapeString
#define HAVE_PQescapeBytea
#define HAVE_PQunescapeBytea

// #include"pg_functions.c"


/* This section was stolen from libpq */
#ifndef HAVE_PQescapeString
size_t
PQescapeString(char *to, const char *from, size_t length)
{
	const char *source = from;
	char	   *target = to;
	unsigned int remaining = length;

	while (remaining > 0)
	{
		switch (*source)
		{
			case '\\':
				*target = '\\';
				target++;
				*target = '\\';
				/* target and remaining are updated below. */
				break;

			case '\'':
				*target = '\'';
				target++;
				*target = '\'';
				/* target and remaining are updated below. */
				break;

			default:
				*target = *source;
				/* target and remaining are updated below. */
		}
		source++;
		target++;
		remaining--;
	}

	/* Write the terminating NUL character. */
	*target = '\0';

	return target - to;
}
#endif

/*
 *		PQescapeBytea	- converts from binary string to the
 *		minimal encoding necessary to include the string in an SQL
 *		INSERT statement with a bytea type column as the target.
 *
 *		The following transformations are applied
 *		'\0' == ASCII  0 == \\000
 *		'\'' == ASCII 39 == \'
 *		'\\' == ASCII 92 == \\\\
 *		anything >= 0x80 ---> \\ooo (where ooo is an octal expression)
 */
#ifndef HAVE_PQescapeBytea
unsigned char *
PQescapeBytea(unsigned char *bintext, size_t binlen, size_t *bytealen)
{
	unsigned char *vp;
	unsigned char *rp;
	unsigned char *result;
	size_t		i;
	size_t		len;

	/*
	 * empty string has 1 char ('\0')
	 */
	len = 1;

	vp = bintext;
	for (i = binlen; i > 0; i--, vp++)
	{
		if (*vp == 0 || *vp >= 0x80)
			len += 5;			/* '5' is for '\\ooo' */
		else if (*vp == '\'')
			len += 2;
		else if (*vp == '\\')
			len += 4;
		else
			len++;
	}

	rp = result = (unsigned char *) malloc(len);
	if (rp == NULL)
		return NULL;

	vp = bintext;
	*bytealen = len;

	for (i = binlen; i > 0; i--, vp++)
	{
		if (*vp == 0 || *vp >= 0x80)
		{
			(void) sprintf(rp, "\\\\%03o", *vp);
			rp += 5;
		}
		else if (*vp == '\'')
		{
			rp[0] = '\\';
			rp[1] = '\'';
			rp += 2;
		}
		else if (*vp == '\\')
		{
			rp[0] = '\\';
			rp[1] = '\\';
			rp[2] = '\\';
			rp[3] = '\\';
			rp += 4;
		}
		else
			*rp++ = *vp;
	}
	*rp = '\0';

	return result;
}
#endif

/*
 *		PQunescapeBytea - converts the null terminated string representation
 *		of a bytea, strtext, into binary, filling a buffer. It returns a
 *		pointer to the buffer which is NULL on error, and the size of the
 *		buffer in retbuflen. The pointer may subsequently be used as an
 *		argument to the function free(3). It is the reverse of PQescapeBytea.
 *
 *		The following transformations are reversed:
 *		'\0' == ASCII  0 == \000
 *		'\'' == ASCII 39 == \'
 *		'\\' == ASCII 92 == \\
 *
 *		States:
 *		0	normal		0->1->2->3->4
 *		1	\			   1->5
 *		2	\0			   1->6
 *		3	\00
 *		4	\000
 *		5	\'
 *		6	\\
 */
#ifndef HAVE_PQunescapeBytea
unsigned char *
PQunescapeBytea(unsigned char *strtext, size_t *retbuflen)
{
	size_t		buflen;
	unsigned char *buffer,
			   *sp,
			   *bp;
	unsigned int state = 0;

	if (strtext == NULL)
		return NULL;
	buflen = strlen(strtext);	/* will shrink, also we discover if
								 * strtext */
	buffer = (unsigned char *) malloc(buflen);	/* isn't NULL terminated */
	if (buffer == NULL)
		return NULL;
	for (bp = buffer, sp = strtext; *sp != '\0'; bp++, sp++)
	{
		switch (state)
		{
			case 0:
				if (*sp == '\\')
					state = 1;
				*bp = *sp;
				break;
			case 1:
				if (*sp == '\'')	/* state=5 */
				{				/* replace \' with 39 */
					bp--;
					*bp = '\'';
					buflen--;
					state = 0;
				}
				else if (*sp == '\\')	/* state=6 */
				{				/* replace \\ with 92 */
					bp--;
					*bp = '\\';
					buflen--;
					state = 0;
				}
				else
				{
					if (isdigit(*sp))
						state = 2;
					else
						state = 0;
					*bp = *sp;
				}
				break;
			case 2:
				if (isdigit(*sp))
					state = 3;
				else
					state = 0;
				*bp = *sp;
				break;
			case 3:
				if (isdigit(*sp))		/* state=4 */
				{
					int			v;

					bp -= 3;
					sscanf(sp - 2, "%03o", &v);
					*bp = v;
					buflen -= 3;
					state = 0;
				}
				else
				{
					*bp = *sp;
					state = 0;
				}
				break;
		}
	}
	buffer = realloc(buffer, buflen);
	if (buffer == NULL)
		return NULL;

	*retbuflen = buflen;
	return buffer;
}
#endif



char *
dbd_quote  (string, pg_type, length, retlen)
	char *string;
	int pg_type;
	size_t length;
	size_t *retlen;
{
	sql_type_info_t *type_info;
	char *retval;

    if(pg_type == 17)
	  croak("Use of SQL_BINARY invalid in quote()");

	type_info = pg_type_data(pg_type);
	if(!type_info)
		croak("No Type info");


	retval = type_info->quote(string, length, retlen);
 	return retval;
}


char *
null_quote(string, len, retlen)
	void *string;
	size_t len;
	size_t *retlen;
{
	char *result;
	Newc(0,result,len+1,char, char);
	strncpy(result,string, len);
	*retlen = len;
	return result;
}


char *
quote_varchar(string, len, retlen)
	char *string;
	size_t len;
	size_t *retlen;
{
	size_t	outlen;
	char *result;
	

	Newc(0,result,len*2+3,char, char);
	outlen = PQescapeString(result+1, string, len);

	// TODO: remalloc outlen
	*result = '\'';
	outlen++;
	*(result+outlen)='\'';
	outlen++;
	*(result+outlen)='\0';
	*retlen = outlen;
	return result;
}

char *
quote_char(string, len, retlen)
	void *string;
	size_t len;
	size_t *retlen;
{
	size_t	outlen;
	char *result;
	
	// TODO: ChopBlanks
	Newc(0,result,len*2+3,char, char);
	outlen = PQescapeString(result+1, string, len);

	// TODO: remalloc outlen
	*result = '\'';
	outlen++;
	*(result+outlen)='\'';
	outlen++;
	*(result+outlen)='\0';
	*retlen = outlen;
	return result;
}


char *
quote_sql_binary( string, len, retlen)
	void *string;
	size_t	len;
	size_t	*retlen;
{
	char *result;
	char *dest;
	int max_len = 0, i;

	/* +4 ==  3 for X'';  1 for \0 */
	max_len = len*2+4;
	Newc(0, result, max_len,char,char);


	dest = result;
	memcpy(dest++, "X'",1);

	for (i=0 ; i <= len ; ++i, dest+=2) 
		sprintf(dest, "%X", *((char*)string++));

	strcat(dest, "\'");
		
	*retlen = strlen(result);
	assert(*retlen+1 <= max_len);
	return result;
}




char *
quote_bytea(string, len, retlen)
	void* string;
	size_t len;
	size_t *retlen;
{
	char *result;
	size_t resultant_len = 0;
	unsigned char *intermead, *dest;

	intermead = PQescapeBytea(string, len, &resultant_len);
	Newc(0,result,resultant_len+2,char, char);


	dest = result;

	memcpy(dest++, "'",1);
	strcpy(dest,intermead);
	strcat(dest,"\'");

	free(intermead);
	*retlen=strlen(result);
	assert(*retlen+1 <=  resultant_len+2);
	

	return result;
}


char *
quote_bool(value, len, retlen) 
	void *value;
	size_t	len;
	size_t	*retlen;
{
	char *result;
	long int int_value;
	size_t	max_len=6;

	if (isDIGIT(*(char*)value)) {
 		/* For now -- will go away when quote* take SVs */
		int_value = atoi(value);
	} else {
		int_value = 42; /* Not true, not false. Just is */
	}
	Newc(0,result,max_len,char,char);

	if (0 == int_value)
		strcpy(result,"FALSE");
	else if (1 == int_value)
		strcpy(result,"TRUE");
	else
		croak("Error: Bool must be either 1 or 0");

	*retlen = strlen(result);
	assert(*retlen+1 <= max_len);

	return result;
}



char *
quote_integer(value, len, retlen) 
	void *value;
	size_t	len;
	size_t	*retlen;
{
	char *result;
	size_t	max_len=6;

	Newc(0,result,max_len,char,char);

	if (*((int*)value) == 0)
		strcpy(result,"FALSE");
	if (*((int*)value) == 1)
		strcpy(result,"TRUE");

	*retlen = strlen(result);
	assert(*retlen+1 <= max_len);

	return result;
}



void
dequote_char(string, retlen)
	char *string;
	int *retlen;
{
	//TODO: chop_blanks if requested
	*retlen = strlen(string);
}


void
dequote_varchar (string, retlen)
	char *string;
	int *retlen;
{
	*retlen = strlen(string);
}



void
dequote_bytea(string, retlen)
	char *string;
	int *retlen;
{
	char *s, *p;
	int c1,c2,c3;
	/* Stolen right from dbdquote. This probably should be cleaned up
	   & made more robust.  Maybe later...
	 */
	s = string;
	p = string;
	while (*s) {
		if (*s == '\\') {
			if (*(s+1) == '\\') { /* double backslash */
				*p++ = '\\';
				s += 2;
				continue;
			} else if ( isdigit(c1=(*(s+1))) &&
                                  isdigit(c2=(*(s+2))) &&
                                  isdigit(c3=(*(s+3))) ) 
			{
				*p++ = (c1-'0') * 64 + (c2-'0') * 8 + (c3-'0');
				s += 4;
				continue;
			}
		}
		*p++ = *s++;
	}
	*retlen = (p-string);
}



/*
   This one is not used in PG, but since we have a quote_sql_binary,
   it might be nice to let people go the other way too.  Say when talking
   to something that uses SQL_BINARY
 */
void
dequote_sql_binary (string, retlen)
	char *string;
	int *retlen;
{
	*retlen = strlen(string);
}



void
dequote_bool (string, retlen)
	char *string;
	int *retlen;
{
	switch(*string){
		case 'f': *string = '0'; break;
		case 't': *string = '1'; break;
		default:
			croak("I do not know how to deal with %c as a bool",
			    *string);
	}
	*retlen = 1;
}



void
null_dequote (string, retlen)
	void *string;
	size_t *retlen;
{
	*retlen = strlen(string);
}
