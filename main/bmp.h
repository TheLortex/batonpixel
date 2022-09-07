typedef struct __attribute__((__packed__))
{
  unsigned char fileMarker1;
  unsigned char fileMarker2;
  unsigned int bfSize;
  uint16_t unused1;
  uint16_t unused2;
  unsigned int imageDataOffset;
} FILEHEADER;

typedef struct __attribute__((__packed__))
{
  unsigned int biSize;
  int width;
  int height;
  uint16_t planes;
  uint16_t bitPix;
  unsigned int biCompression;
  unsigned int biSizeImage;
  int biXPelsPerMeter;
  int biYPelsPerMeter;
  unsigned int biClrUsed;
  unsigned int biClrImportant;
} INFOHEADER;

typedef struct __attribute__((__packed__))
{
  unsigned char b;
  unsigned char g;
  unsigned char r;
} IMAGE;