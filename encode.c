#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "iconv.h"
 
#define LOGGING 0

typedef struct {
    int  width;
    int  height;
    int  numValues;
    int  numSuits;

    int  colorizerCode;
    char colorizerBlacklist[512];
    int  colorizerBlacklistSize;

    char cardData[8192];
    int  cardDataSize;
    
} CardConfig;

typedef struct {
    uint32_t codepoint;
    char     token;
} TokenEntry;

static const TokenEntry kTokenMap[] = {
    {0x250C, 'r'}, 
    {0x2510, 'l'},
    {0x2514, 's'}, 
    {0x2518, 'd'},
    {0x2500, '-'}, 
    {0x2502, '|'},
    {0x2660, 'S'}, 
    {0x2663, 'C'},
    {0x2665, 'H'}, 
    {0x2666, 'D'},
    {0xFD3E, '{'}, 
    {0xFD3F, '}'},
    {0x203F, '_'}, 
    {0x2040, '='},
    {0x02C4, '^'}, 
    {0x02C5, '$'},
    {0x2039, 'p'}, 
    {0x203A, 'q'},
    {0x22CF, ','}, 
    {0x02AC, 'w'},
    {0x04DC, '&'}, 
    {0x2021, 'T'},
    {0x2092, 'o'}, 
    {0x2534, 't'},
    {0x2572, 'x'}, 
    {0x2571, 'f'},
    {0x03DB, 'u'}, 
    {0x0550, 'y'},
    {0x053E, 'O'}, 
    {0x0196, 'I'},
    {0xA7B0, 'k'}, 
    {0x218A, 'b'},
    {0x218B, 'E'}, 
    {0x15C4, 'V'},
    {0x15C5, 'A'},
};

#define TOKEN_MAP_SIZE (sizeof(kTokenMap) / sizeof(kTokenMap[0]))

static int StartsWith(const char* line, const char* prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

static int ParseInt(const char* line)
{
    const char* eq = strchr(line, '=');
    
    if (!eq) {
        return 0;
    }
    
    return atoi(eq + 1);
}

static void ParseString(const char* line, char* out, size_t outSize)
{
    const char* eq = strchr(line, '=');

    if (!eq) { 
        out[0] = '\0'; 
        return; 
    }

    strncpy(out, eq + 1, outSize - 1);
    out[outSize - 1] = '\0';
}

static char LookupToken(uint32_t cp) {
    if (cp == ' ')   return '.';
    if (cp == '\\')  return '\\';
    if (cp == '/')   return '/';
    if (cp < 0x80)   return (char)cp;

    for (size_t i = 0; i < TOKEN_MAP_SIZE; i++) {
        if (kTokenMap[i].codepoint == cp) {
            return kTokenMap[i].token;
        }
    }
        
    return '\x7f';
}

#if defined(LOGGING) && LOGGING == 2
static int gLineCount = 0;
static int gTokenCount = 0;
#endif

static int TokenizeLine(iconv_t cd, const char* line, size_t lineLen,
                        char* out, int* outPos, int outMax) 
{
    char*  inBuf  = (char*)line;
    size_t inLeft = lineLen;

#if defined(LOGGING) && LOGGING == 2
    int before = *outPos;
#endif

    while (inLeft > 0) {
        uint32_t cp      = 0;
        char*    outBuf  = (char*)&cp;
        size_t   outLeft = sizeof(cp);
        size_t   inPrev  = inLeft;

        size_t ret = iconv(cd, &inBuf, &inLeft, &outBuf, &outLeft);

        if (inLeft == inPrev) {
            inBuf++;
            inLeft--;
            cp = 0xFFFD;
        }

        if (*outPos >= outMax - 1) {
                fprintf(stderr, "Card data buffer overflow\n");
                return 0;
        }

        out[(*outPos)++] = LookupToken(cp);
    }

    iconv(cd, NULL, NULL, NULL, NULL);

#if defined(LOGGING) && LOGGING == 2
    gLineCount++;
    gTokenCount += (*outPos - before);
    printf("Line %d: %d tokens this line, %d total\n", gLineCount, *outPos - before, gTokenCount);
#endif

    return 1;
}

typedef enum { SECTION_NONE, SECTION_SETTINGS, SECTION_DATA } Section;

int LoadConf(const char* path, CardConfig* cfg) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 0;
    }

    memset(cfg, 0, sizeof(CardConfig));

    char line[4096];
    Section section = SECTION_NONE;

    iconv_t cd = iconv_open("UTF-32LE", "UTF-8");
    if (cd == (iconv_t)-1) {
        fprintf(stderr, "iconv_open failed\n");
        fclose(f);
        return 0;
    }

    while (fgets(line, sizeof(line), f))
    {
        // Strip \r\n
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
        {
            line[--len] = '\0';
        }

        // Ignore comments
        if (len == 0 || line[0] == '#') {
            continue;
        }

        // Entered Settings section
        if (strcmp(line, "[Settings]") == 0) {
            section = SECTION_SETTINGS;
            continue;
        }

        // Entered Data section
        if (strcmp(line, "[Data]")     == 0) { 
            section = SECTION_DATA;
            continue;
        }

        // Handling settings or data
        if (section == SECTION_SETTINGS) {
            if (StartsWith(line, "WIDTH=")) {
                cfg->width = ParseInt(line);
            }
            else if (StartsWith(line, "HEIGHT=")) {
                cfg->height = ParseInt(line);
            }
            else if (StartsWith(line, "NUM_VALUES=")) {
                cfg->numValues = ParseInt(line);
            }
            else if (StartsWith(line, "NUM_SUITS=")) {
                cfg->numSuits = ParseInt(line);
            }
            else if (StartsWith(line, "COLORIZER_CODE=")) {
                cfg->colorizerCode = ParseInt(line);
            }
            else if (StartsWith(line, "COLORIZER_BLACKLIST=")) {
                char raw[512];
                ParseString(line, raw, sizeof(raw));

                int pos = 0;
                TokenizeLine(cd, raw, strlen(raw), cfg->colorizerBlacklist, &pos, sizeof(cfg->colorizerBlacklist));

                cfg->colorizerBlacklistSize = pos;
            }
        }
        else if (section == SECTION_DATA) {
            if (!TokenizeLine(cd, line, len, cfg->cardData, &cfg->cardDataSize, sizeof(cfg->cardData)))
            {
                iconv_close(cd);
                fclose(f);
                return 0;
            }
        }
    }

    iconv_close(cd);
    fclose(f);

    if (cfg->width == 0 || cfg->height == 0) {
        fprintf(stderr, "Missing WIDTH or HEIGHT in [Settings]\n");
        return 0;
    }

    int cardSize = cfg->width * cfg->height;

#if defined(LOGGING) && LOGGING
    int numCards = cfg->cardDataSize / cardSize;
    int remainder = cfg->cardDataSize % cardSize;

    printf("Complete cards parsed: %d\n", numCards);
    printf("Leftover tokens: %d\n", remainder);
    printf("Expected cards: %d\n\n", cfg->numValues * cfg->numSuits);
#endif

    int expected = cfg->numValues * cfg->numSuits * cfg->width * cfg->height;

#if defined(LOGGING) && LOGGING == 2
    printf("Expected: %d values x %d suits x %d width x %d height = %d\n",
       cfg->numValues, cfg->numSuits, cfg->width, cfg->height,
       cfg->numValues * cfg->numSuits * cfg->width * cfg->height);
#endif

    if (expected > 0 && cfg->cardDataSize != expected) {
        fprintf(stderr, "Data size mismatch: expected %d tokens, got %d\n", expected, cfg->cardDataSize);
        
        return 0;
    }

    return 1;
}

int main(int argc, char* argv[]) 
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cards.conf>\n", argv[0]);
        return 1;
    }

    CardConfig cfg;

    if (!LoadConf(argv[1], &cfg)) {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }

#if defined(LOGGING) && LOGGING == 2
    printf("Blacklist: %s\n", cfg.colorizerBlacklist);
#endif

#if defined(LOGGING) && LOGGING
    printf("Loaded %d cards (%d values x %d suits)\n\n", cfg.numValues * cfg.numSuits, cfg.numValues, cfg.numSuits);
#elif defined(LOGGING) && LOGGING == 2
    printf("Card size: %dx%d\n", cfg.width, cfg.height);
    printf("Token data size: %d bytes\n", cfg.cardDataSize);
#endif

#if defined(LOGGING) && LOGGING
    int cardIndex = 0;
    if (argc >= 3) {
        cardIndex = atoi(argv[2]);
    }

    int cardSize = cfg.width * cfg.height;
    int totalCards = cfg.numValues * cfg.numSuits;

    if (cardIndex < 0 || cardIndex >= totalCards) {
        fprintf(stderr, "Card index %d out of range (0..%d)\n", cardIndex, totalCards - 1);
        return 1;
    }

    printf("\nCard %d:\n", cardIndex);
    
    const char* card = cfg.cardData + cardIndex * cardSize;
    
    for (int row = 0; row < cfg.height; row++) {
        for (int col = 0; col < cfg.width; col++) {
            putchar(card[row * cfg.width + col]);
        }
            
        putchar('\n');
    }

    printf("\n");
#endif

    FILE* out = fopen("cards.dat", "wb");
    if (out) {
        fwrite(cfg.cardData, 1, cfg.cardDataSize, out);
        fclose(out);
    }

    printf("Outputted to cards.dat");

    return 0;
}