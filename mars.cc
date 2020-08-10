/**
    MARS: Multiple circular sequence Alignment using Refined Sequences
    Copyright (C) 2016 Lorraine A.K. Ayad, Solon P. Pissis

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include <iostream>
#include <cstdlib>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <fstream>
#include <float.h>
#include <limits.h>
#include <omp.h>
#include "mars.h"
#include "sacsc.h"
#include "ced.h"
#include "nj.h"

int main(int argc, char **argv)
{
	struct TSwitch  sw;

	FILE *          in_fd;                  // the input file descriptor
	FILE *          out_fd;                 // the output file descriptor
        char *          input_filename;         // the input file name
        char *          output_filename;        // the output file name

        unsigned char ** seq    = NULL;         // the sequence(s) in memory
        unsigned char ** seq_id = NULL;         // the sequence(s) id in memory

	char *          alphabet;               // the alphabet
	unsigned int    i, j;
	unsigned int    q, l;
	unsigned int    total_length = 0;

	/* Decodes the arguments */
        i = decode_switches ( argc, argv, &sw );

	omp_set_num_threads( sw.T );

	/* Check the arguments */
        if ( i < 3 )
        {
                usage ();
                return ( 1 );
        }
        else
        {

                if      ( ! strcmp ( "DNA", sw . alphabet ) )   { alphabet = ( char * ) DNA;  sw . matrix = 0; }
                else if ( ! strcmp ( "PROT", sw . alphabet ) )  { alphabet = ( char * ) PROT; sw . matrix = 1; }
                else
                {
                        fprintf ( stderr, " Error: alphabet argument a should be `DNA' for nucleotide sequences or `PROT' for protein sequences!\n" );
                        return ( 1 );
                }

		if( sw . m != 0 && sw . m != 1 )
		{
			fprintf( stderr, " Error: method m must be 0 for hCED or 1 for branch and bound. \n" );
			return ( 1 );
		}

		if ( sw . m == 0 && sw . P < 0 )
		{
			fprintf ( stderr, " Error: The number of refinement blocks cannot be smaller than 0.\n" );
			return ( 1 );
		}

		if ( sw . m  == 0 && sw . q < 2 )
		{
			fprintf ( stderr, " Error: The q-gram length is too small.\n" );
			return ( 1 );	
		}

                input_filename       = sw . input_filename;
		if ( input_filename == NULL )
		{
			fprintf ( stderr, " Error: Cannot open file for input!\n" );
			return ( 1 );
		}
		output_filename         = sw . output_filename;

        }

	double start = gettime();

	/* Read the (Multi)FASTA file in memory */
	fprintf ( stderr, " Reading the (Multi)FASTA input file: %s\n", input_filename );
	if ( ! ( in_fd = fopen ( input_filename, "r") ) )
	{
		fprintf ( stderr, " Error: Cannot open file %s!\n", input_filename );
		return ( 1 );
	}

	char c;
        unsigned int num_seqs = 0;           // the total number of sequences considered
	unsigned int max_alloc_seq_id = 0;
	unsigned int max_alloc_seq = 0;
	c = fgetc( in_fd );

	do
	{
		if ( c != '>' )
		{
			fprintf ( stderr, " Error: input file %s is not in FASTA format!\n", input_filename );
			return ( 1 );
		}
		else
		{
			if ( num_seqs >= max_alloc_seq_id )
			{
				seq_id = ( unsigned char ** ) realloc ( seq_id,   ( max_alloc_seq_id + ALLOC_SIZE ) * sizeof ( unsigned char * ) );
				max_alloc_seq_id += ALLOC_SIZE;
			}

			unsigned int max_alloc_seq_id_len = 0;
			unsigned int seq_id_len = 0;

			seq_id[ num_seqs ] = NULL;

			while ( ( c = fgetc( in_fd ) ) != EOF && c != '\n' )
			{
				if ( seq_id_len >= max_alloc_seq_id_len )
				{
					seq_id[ num_seqs ] = ( unsigned char * ) realloc ( seq_id[ num_seqs ],   ( max_alloc_seq_id_len + ALLOC_SIZE ) * sizeof ( unsigned char ) );
					max_alloc_seq_id_len += ALLOC_SIZE;
				}
				seq_id[ num_seqs ][ seq_id_len++ ] = c;
			}
			seq_id[ num_seqs ][ seq_id_len ] = '\0';
			
		}

		if ( num_seqs >= max_alloc_seq )
		{
			seq = ( unsigned char ** ) realloc ( seq,   ( max_alloc_seq + ALLOC_SIZE ) * sizeof ( unsigned char * ) );
			max_alloc_seq += ALLOC_SIZE;
		}

		unsigned int seq_len = 0;
		unsigned int max_alloc_seq_len = 0;

		seq[ num_seqs ] = NULL;

		while ( ( c = fgetc( in_fd ) ) != EOF && c != '>' )
		{
			if( seq_len == 0 && c == '\n' )
			{
				fprintf ( stderr, " Omitting empty sequence in file %s!\n", input_filename );
				c = fgetc( in_fd );
				break;
			}
			if( c == '\n' || c == ' ' ) continue;

			c = toupper( c );

			if ( seq_len >= max_alloc_seq_len )
			{
				seq[ num_seqs ] = ( unsigned char * ) realloc ( seq[ num_seqs ],   ( max_alloc_seq_len + ALLOC_SIZE ) * sizeof ( unsigned char ) );
				max_alloc_seq_len += ALLOC_SIZE;
			}

			if( strchr ( alphabet, c ) )
			{
				seq[ num_seqs ][ seq_len++ ] = c;
			}
			else
			{
				fprintf ( stderr, " Error: input file %s contains an unexpected character %c!\n", input_filename, c );
				return ( 1 );
			}

		}

		if( seq_len != 0 )
		{
			if ( seq_len >= max_alloc_seq_len )
			{
				seq[ num_seqs ] = ( unsigned char * ) realloc ( seq[ num_seqs ],   ( max_alloc_seq_len + ALLOC_SIZE ) * sizeof ( unsigned char ) ); 
				max_alloc_seq_len += ALLOC_SIZE;
			}
			seq[ num_seqs ][ seq_len ] = '\0';

			total_length += seq_len;
			num_seqs++;
		}
		
	} while( c != EOF );

	seq[ num_seqs ] = NULL;

	if ( fclose ( in_fd ) )
	{
		fprintf( stderr, " Error: file close error!\n");
		return ( 1 );
	}

	fprintf ( stderr, " Computing cyclic edit distance for all sequence pairs\n" );
		

	TPOcc ** D;

	int * Rot = ( int * ) calloc ( num_seqs , sizeof ( int ) );

	if ( ( D = ( TPOcc ** ) calloc ( ( num_seqs ) , sizeof( TPOcc * ) ) ) == NULL )
	{
		fprintf( stderr, " Error: Cannot allocate memory!\n" );
		exit( EXIT_FAILURE );
	}

	for(int i=0; i<num_seqs; i++)
	{
		if ( ( D[i] = ( TPOcc * ) calloc ( ( num_seqs + 1 ) , sizeof( TPOcc ) ) ) == NULL )
		{
			fprintf( stderr, " Error: Cannot allocate memory!\n" );
			exit( EXIT_FAILURE );
		}

	}
		

	int prevL = sw . l;
	
	/*Finds an approximate rotation for every pair of sequences in the data sets for method hCED*/ 
	if ( sw . m == 0 )
		circular_sequence_comparison ( seq, sw, D, num_seqs );

	init_substitution_score_tables ();

	#pragma omp parallel for
	for ( int i = 0; i < num_seqs; i++ )
	{	
		unsigned int m = strlen ( ( char * ) seq[i] );
		
		if( sw . l == 0 )
			sw . l = sqrt(m);
	
		if( sw . P * sw . l > m/3 )
		{
			fprintf( stderr, " Error: P is too large!\n" );
			exit( EXIT_FAILURE );
		}

		if( sw . m == 0 && sw . q >= sw . l )
		{
			fprintf ( stderr, " Error: The length of the q-gram must be smaller than the block length.\n" );
			exit( 1 );
		}

		for ( int j = 0; j < num_seqs; j ++ )
		{
			if ( i == j ) 
				continue;
	
			unsigned char * xr = ( unsigned char * ) calloc( ( m + 1 ) , sizeof( unsigned char ) );
			unsigned int n = strlen ( ( char * ) seq[j] );	

			if ( sw . l > m - sw . q + 1  || sw . l > n - sw . q + 1 )
			{
				fprintf( stderr, " Error: Illegal block length.\n" );
				exit ( 1 );
			}

			unsigned int distance = D[i][j] . err;
			unsigned int rotation = D[i][j] . rot;

			if( sw . m == 0 )
			{
				create_rotation ( seq[i], rotation, xr );

				/*Produces more accurate rotations using refined sequences for method hCED*/
				sacsc_refinement(seq[i], xr, seq[j], sw, &rotation, &distance);
			}
			else 
			{

				if( m > 20000 || n > 20000 )
				{
					fprintf( stderr, " Method -m 1 is only suitable for short sequences. Please use method -m 0.\n" );
					exit ( 1 );
				}

				/*Find rotation and distance using branch and bound method*/
				cyclic( seq[i], seq[j], m, n, 1, 0, &rotation, &distance ) ;
				create_rotation ( seq[i], rotation, xr );

				/*Produces more accurate rotations using refined sequences for branch and bound method*/
				sacsc_refinement( seq[i], xr, seq[j], sw, &rotation, &distance);
			}

			D[i][j] . err = distance;
			D[i][j] . rot = rotation;

			free( xr );
		}
		
	}


	if ( sw . m == 0 )
	{
		for(int i=0; i<num_seqs; i++)
		{
		

			for(int j=0; j<num_seqs; j++)
			{
				unsigned int distance = 0;
				unsigned int rotation = 0;

				if( D[i][j] . err - D[j][i] . err > ( total_length/num_seqs )*0.05 )
				{
					unsigned int m = strlen ( ( char * ) seq[i] );
					unsigned int n = strlen ( ( char * ) seq[j] );
					unsigned char * xr = ( unsigned char * ) calloc( ( m + 1 ) , sizeof( unsigned char ) );

					cyclic( seq[i], seq[j], m, n, 1, 0, &rotation, &distance ) ;
					create_rotation ( seq[i], rotation, xr );
					sacsc_refinement( seq[i], xr, seq[j], sw, &rotation, &distance);

					D[i][j] . err = distance;
					D[i][j] . rot = rotation;

					free( xr );
				}

			
			}
		}
	}

	fprintf ( stderr, " Creating the guide tree\n" );

	sw . l = prevL;

	/*Creates the guide tree*/
	nj ( D, num_seqs, seq, sw, Rot );

	fprintf ( stderr, " Preparing the output\n" );

	if ( ! ( out_fd = fopen ( output_filename, "w") ) )
	{
		fprintf ( stderr, " Error: Cannot open file %s!\n", output_filename );
		return ( 1 );
	}

	for ( int i = 0; i < num_seqs; i ++ )
	{
		if ( Rot[i] >= 0 )
		{
			Rot[i] = Rot[i] % strlen ( ( char * ) seq[i] );
			unsigned char * rotation;
			unsigned int m = strlen ( ( char * ) seq[i] );
			rotation = ( unsigned char * ) calloc ( m + 1, sizeof ( unsigned char ) );
			create_rotation ( seq[i], Rot[i], rotation );
			fprintf( out_fd, ">%s (rotated %d bases)\n", seq_id[i], Rot[i] );
			fprintf( out_fd, "%s\n", rotation );
			free ( rotation );
		}		
	}

		
	if ( fclose ( out_fd ) )
	{
		fprintf( stderr, " Error: file close error!\n");
		return ( 1 );
	}

	free ( Rot );
	for ( i = 0; i < num_seqs; i ++ )
	{
		free ( D[i] );
	}
	free ( D );

	double end = gettime();

        fprintf( stderr, "Elapsed time for processing %d sequence(s): %lf secs.\n", num_seqs, ( end - start ) );
	
	/* Deallocate */
	
	for ( i = 0; i < num_seqs; i ++ )
	{
		free ( seq[i] );
		free( seq_id[i] );
	}	
	free ( seq );
	free ( seq_id );
        free ( sw . input_filename );
        free ( sw . output_filename );
        free ( sw . alphabet );

	return ( 0 );
}
