/* This file is part of 34S.
 * 
 * 34S is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * 34S is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with 34S.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *Template =
	"/* This file is part of 34S.\n"
	" *\n"
	" * 34S is free software: you can redistribute it and/or modify\n"
	" * it under the terms of the GNU General Public License as published by\n"
	" * the Free Software Foundation, either version 3 of the License, or\n"
	" * (at your option) any later version.\n"
	" *\n"
	" * 34S is distributed in the hope that it will be useful,\n"
	" * but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	" * GNU General Public License for more details.\n"
	" *\n"
	" * You should have received a copy of the GNU General Public License\n"
	" * along with 34S.  If not, see <http://www.gnu.org/licenses/>.\n"
	" *\n"
	" * Generated by create_revision.\n"
	" */\n"
	"\n"
	"#ifdef REALBUILD\n"
	"__attribute__((section(\".revision\"),externally_visible))\n"
	"#endif\n"
	"const char SvnRevision[ 4 ] = \"%-4d\";\n";

int main( int argc, char **argv )
{
	char buffer[ 1000 ]; // ND change to prevent compiler warning
//	char tmpname[ FILENAME_MAX ]; // tmpnam returns an inaccessible filename in Windows 10
	char tmpname[] = "wptmpxxx.tmp";
	FILE *f;
	int rev;
	char *p;
	
//	if (tmpnam( tmpname ) == NULL) {
//		perror("Unable to create temporary file name");
//		return 1;
//	}

	// Try to execute svnversion
	sprintf( buffer, "svnversion -n >%s", tmpname );
	fprintf( stderr, "Executing %s\n", buffer );
	if (system( buffer ) == -1) {
		perror("unable to run subversion command");
		remove( tmpname );
		return 1;
	}

	// Read result
	f = fopen( tmpname, "r" );
	if ( f == NULL ) {
		fprintf( stderr, "Revision number is unknown\n" );
		strcpy( buffer, "0000" );
	}
	else {
		if (fgets( buffer, sizeof( buffer ) - 1, f ) == NULL) {
			perror("unable to read revision");
			fclose( f );
			remove( tmpname );
			return 1;
		}
		fprintf( stderr, "Revision number(s): %s\n", buffer );
		fclose( f );
		remove( tmpname );
	}

	// Determine the revision number
	p = strchr( buffer, ':' );
	if ( p == NULL ) {
		p = buffer;
	}
	else {
		++p;
	}
	rev = atoi( p );
	if ( rev != 0 ) {
		// Assume the next higher revision number on next commit
		++rev;
	}
	fprintf( stderr, "Revision number will be %d\n", rev );

	// Create output
	printf( Template, rev );

	return 0;
}
