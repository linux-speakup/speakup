#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <linux/version.h>
#include <ctype.h>

int get_define(void);

#define MAXKEYS 512
#define MAXKEYVAL 160
#define HASHSIZE 101
#define is_shift -3
#define is_spk -2
#define is_input -1
typedef struct st_key t_key;
struct st_key {
	char *name;
	t_key *next;
	int value, shift;
};

t_key key_table[MAXKEYS];
t_key *extra_keys = key_table+HASHSIZE;
char buffer[256], filename[256];
FILE *infile;
char delims[] = "\t\n ";
char *dir_name, *def_name, *def_val, *cp;
int lc;

void open_input( char *name )
{
	sprintf( filename, "%s/%s", dir_name, name );
	if ( ( infile = fopen( filename, "r" ) ) == 0 ) {
		fprintf( stderr, "can't open %s\n", filename );
		exit( 1 );
	}
	lc = 0;
}

int
oops( char *msg, char *info )
{
	if ( info == NULL ) info = " ";
	fprintf( stderr, "error: file %s line %d\n", filename, lc );
	fprintf( stderr, "%s %s\n", msg, info );
	exit( 1 );
}

int get_define( )
{
	char *c;
	while ( fgets( buffer, 250, infile ) ) {
		lc++;
		if ( strncmp( buffer, "#define", 7 ) ) continue;
		c = buffer + 7;
		while (*c == ' ' || *c == '\t')
			c++;
		def_name = c;
		while (*c && *c != ' ' && *c != '\t' && *c != '\n')
			c++;
		if (!*c || *c == '\n')
			continue;
		*c++ = '\0';
		while (*c == ' ' || *c == '\t' || *c == '(')
			c++;
		def_val = c;
		while (*c && *c != '\n' && *c != ')')
			c++;
		*c++ = '\0';
		return 1;
	}
	fclose( infile );
	infile = 0;
	return 0;
}

t_key *hash_name( char *name )
{
	u_char *pn = (u_char *)name;
	int hash = 0;
	while ( *pn ) {
		hash = ( hash * 17 ) & 0xfffffff;
	if ( isupper( *pn ) ) *pn = tolower( *pn );
		hash += ( int )*pn;
		pn++;
	}
	hash %= HASHSIZE;
	return &key_table[hash];
}

t_key *find_key( char *name )
{
	t_key *this = hash_name( name );
	while ( this ) {
		if ( this->name && !strcmp( name, this->name ) ) return this;
		this = this->next;
	}
	return this;
}

t_key *add_key( char *name, int value, int shift )
{
	t_key *this = hash_name( name );
	if ( extra_keys-key_table >= MAXKEYS )
		oops( "out of key table space, enlarge MAXKEYS", NULL );
	if ( this->name != NULL ) {
		while ( this->next ) {
			if ( !strcmp( name, this->name ) )
				oops( "attempt to add duplicate key", name );
			this = this->next;
		}
		this->next = extra_keys++;
		this = this->next;
	}
	this->name = strdup( name );
	this->value = value;
	this->shift = shift;
	return this;
}

int
main( int argc, char *argv[] )
{
	int value, i;
	t_key *this;
	dir_name = getenv( "TOPDIR" );
	if ( !dir_name ) dir_name = "/usr/src/linux";
	bzero( key_table, sizeof( key_table ) );
	add_key( "shift",	1, is_shift );
	add_key( "altgr",	2, is_shift );
	add_key( "ctrl",	4, is_shift );
	add_key( "alt",	8, is_shift );
	add_key( "spk", 16, is_shift );
	add_key( "double", 32, is_shift );

	open_input( "include/linux/input.h" );
	while ( get_define( ) ) {
		if ( strncmp( def_name, "KEY_", 4 ) ) continue;
		value = atoi( def_val );
		if ( value > 0 && value < MAXKEYVAL )
			add_key(  def_name, value, is_input );
	}

	open_input( "include/uapi/linux/input-event-codes.h" );
	while ( get_define( ) ) {
		if ( strncmp( def_name, "KEY_", 4 ) ) continue;
		value = atoi( def_val );
		if ( value > 0 && value < MAXKEYVAL )
			add_key(  def_name, value, is_input );
	}

	open_input( "drivers/accessibility/speakup/spk_priv_keyinfo.h" );
	while ( get_define( ) ) {
		if ( strlen( def_val ) > 5 ) {
			//if (def_val[0] == '(')
			//	def_val++;
			if ( !( cp = strchr( def_val, '+' ) ) ) continue;
			if (cp[-1] == ' ')
				cp[-1] = '\0';
			*cp++ = '\0';
			this = find_key( def_val );
			while (*cp == ' ')
				cp++;
			if ( !this || *cp < '0' || *cp > '9' ) continue;
			value = this->value+atoi( cp );
		} else if ( !strncmp( def_val, "0x", 2 ) )
			sscanf( def_val+2, "%x", &value );
		else if ( *def_val >= '0' && *def_val <= '9' )
			value = atoi( def_val );
		else continue;
		add_key( def_name, value, is_spk );
	}

	printf( "t_key_init init_key_data[] = {\n" );
	for ( i = 0; i < HASHSIZE; i++ ) {
		this = &key_table[i];
		if ( !this->name ) continue;
		do {
			printf( "\t\"%s\", %d, %d,\n", this->name, this->value, this->shift );
			this = this->next;
		} while ( this );
	}
	printf( "\t\".\", 0, 0\n};\n" );
	exit( 0 );
}
