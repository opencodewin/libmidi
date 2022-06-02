#include <stdio.h>
#include <stdlib.h>

FILE* openOutfileC( char* _baseOutFileName )
{
	char outname_c[ 2048 ];
	sprintf( outname_c, "%s.c", _baseOutFileName );
	FILE* f = fopen( outname_c, "w" );
	
	return f;
}

void writeBin( FILE* out_f, FILE* in_f ,char* _symbolNameBase, int* _pTotalOutputSize)
{
    fseek(in_f, 0, SEEK_END);
    size_t len = ftell(in_f);
    fseek(in_f, 0, SEEK_SET);
    *_pTotalOutputSize = 0;
    fprintf(out_f, "extern const unsigned char %s_bin[];\n\n", _symbolNameBase );
	fprintf(out_f, "const unsigned char %s_bin[] =\n{\n", _symbolNameBase );
    fprintf(out_f, "\t" );
    int line_size = 0;
    unsigned char c = 0;
    while (!feof(in_f))
    {
        size_t l = fread(&c, 1, 1, in_f);
        if (!l) 
        {
            fprintf(out_f, "\n" );
            break;
        }
        fprintf(out_f, "0x%02x", c);
        if (*_pTotalOutputSize < len - 1)
            fprintf(out_f, ", " );
        line_size++;
        if (line_size == 16)
        {
            fprintf(out_f, "\n" );
            if (*_pTotalOutputSize < len - 1)
                fprintf(out_f, "\t" );
            line_size = 0;
        }
        (*_pTotalOutputSize)++;
    }
    fprintf(out_f, "};\n\n" );
}

int main(int argc, char ** argv)
{
    if( argc != 4 )
	{
		printf("Usage error: Program need 3 arguments:\n");
		printf("  bin2c <in_file> <out_file_base> <symbol_name>\n");
		return -1;
	}

    char* pszInFileName = argv[ 1 ];
	char* pszOutFilenameBase = argv[ 2 ];
	char* pszSymbolNameBase = argv[ 3 ];

    // Open Bin file
    FILE* in_f = fopen(pszInFileName, "rb");
    if (!in_f)
    {
        printf("Open input file error:%s\n", pszInFileName);
        return -1;
    }

    //
	// Write cpp file
	//
	FILE* out_f = openOutfileC( pszOutFilenameBase );
    if (!out_f)
    {
        printf("Open output file error:%s\n", pszOutFilenameBase);
        fclose(in_f);
        return -1;
    }

    int totalOutputSize = 0;
	writeBin( out_f, in_f, pszSymbolNameBase, &totalOutputSize);
    fclose(out_f);
	fclose(in_f);

    printf("Total output size: %i\n", totalOutputSize );
	return 0;

    return 0;
}