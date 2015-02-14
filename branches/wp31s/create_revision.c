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
#include <ctype.h>
#include "licence.h"

static char *Template =
	"/* Generated by create_revision.\n"
	" */\n"
	"\n"
	"#ifdef REALBUILD\n"
	"__attribute__((section(\".revision\"),externally_visible))\n"
	"#endif\n"
	"const char SvnRevision[ 4 ] = \"%-4d\";\n";

int get_revision_num(char *vcs_cmd, char *vcs);


int main( int argc, char **argv )
{
    int rev;

    // Get the revision number from subversion
    rev = get_revision_num("svnversion -n >%s", "subversion");
    if (rev < 0)
    {
	rev = get_revision_num("git svn find-rev `git rev-parse master` >%s", "git");
    }

    // If neither svn nor git can give us an answer, just assume 0
    if (rev < 0)
    {
	rev = 0;
    }

    // Increment
    if ( rev != 0 ) {
	// Assume the next higher revision number on next commit
	++rev;
	}
    fprintf( stderr, "    Revision number will be %d\n\n", rev );

    // Create output
    license(stdout, "/* ", " * ", " */");
    printf( Template, rev );

    return 0;
}




// Get VCS revision number 
// (If there is an error, return a negative number)

int get_revision_num(char *vcs_cmd, char *vcs)
{
    char buffer[ 100 ];
    char tmpname[ FILENAME_MAX ];
    char errmsg[256];
    FILE *f;
    char *p;
    int i;
    int result;
	
    if (tmpnam( tmpname ) == NULL)
    {
	perror("Unable to create tempory file name");
	return -1;
    }

    // Try to execute the VCS command
    sprintf( buffer, vcs_cmd, tmpname );
    fprintf( stderr, "    Executing '%s'\n", buffer );
    result = system(buffer);
    if (result == -1)
    {
	// The VCS call failed
	sprintf(errmsg, "Unable to run %s command", vcs);
	perror(errmsg);
	remove( tmpname );
	return -1;
    }

    // Read result
    f = fopen( tmpname, "r" );
    if ( f == NULL )
    {
	sprintf(errmsg, "Opening the %s output file failed!", vcs);
	perror(errmsg);
	return -1;
    }
    else
    {
	if (fgets( buffer, sizeof( buffer ) - 1, f ) == NULL) 
	{
	    sprintf(errmsg, "Unable to read %s revision from temporary output file", vcs);
	    perror(errmsg);
	    fclose(f);
	    remove(tmpname);
	    return -1;
	}
	fclose(f);
	remove(tmpname );

	// Make sure we received something!
	if ((strlen(buffer) == 0) ||
	    (strncmp(buffer, "exported", 100) == 0))
	{
	    fprintf(stderr, "    No revision number obtained from %s (empty)\n", vcs);
	    return -1;
	}

	// Convert any LFs to spaces
	for (i=0; i<(int)strlen(buffer); i++)
	{
	    if (buffer[i] == '\n')
		buffer[i] = ' ';
	    else if (strchr(":MSP", buffer[i]) != NULL) {
		// ignore extra characters from svnversion
		}
	    else if (!isdigit(buffer[i])) {
		fprintf(stderr, "    No revision number obtained from %s (invalid)\n", vcs);
		return -1;
		}
	}

	fprintf(stderr, "    %s revision number(s): %s\n", vcs, buffer);
    }

    // Determine the revision number
    p = strchr( buffer, ':' );
    if ( p == NULL ) 
    {
	p = buffer;
    }
    else 
    {
	++p;
    }

    // Parse the number
    return atoi( p );
}
