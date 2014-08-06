/*
*   Library for shrinking images as much as possible to make them fit in a small flash mm in a uC
*   Arduino and the likes
*   author fredrik johansson mumme74 at github.com
*/
#ifndef EIMG_H
#define EIMG_H

#include <limits.h>
#include <stdint.h>


typedef struct {uint16_t width, height, colorCount;} EImgData_t ;

#include <Adafruit_ST7735.h>


template<uint8_t DISPBITS, typename COLOR_TYPE, typename COLOR_LOOKUP>
class _EImgT {
	typedef void (Adafruit_ST7735::*DRAW_PIXEL_METHOD)(int16_t, int16_t, uint16_t);
	typedef Adafruit_ST7735* DRAW_PIXEL_OBJECT;
	
	const uint8_t DISPLAY_COLOR_DEPTH = DISPBITS;
  const uint8_t *image;
	COLOR_LOOKUP getColor; //(const uint8_t byteStream[], unsigned int colorMapStarts, unsigned int colorMapIdx);
	
	void parseMeta();
	struct { uint32_t cTableStart, pixelsStart; uint8_t colorDepth; bool hasTransparency; uint16_t width, height;} metaInfo;
	DRAW_PIXEL_METHOD pDrawPixel;
	DRAW_PIXEL_OBJECT pDrawObject;
	
	uint32_t pow(uint16_t base, uint16_t exp) const;
	
public:
	_EImgT(){}
	_EImgT(const uint8_t *imgData, DRAW_PIXEL_OBJECT object, DRAW_PIXEL_METHOD method){
		pDrawPixel = method;
		pDrawObject = object;
		image = imgData;
		parseMeta();
	}
	_EImgT &operator= (const _EImgT &other){
		pDrawPixel = other.pDrawPixel;
		pDrawObject = other.pDrawObject;
		image = other.image;
		metaInfo = other.metaInfo;
		return *this;
	}
	
	EImgData_t  &getImageData() const;
	uint16_t width() const { return metaInfo.width; }
	uint16_t height() const { return metaInfo.height; }
	
	void draw(const int16_t startX, const int16_t startY);
};



struct _EIMG_GetColor1{	// DISPLAY_COLOR_DEPTH == 1
    inline
    uint8_t operator ()(const uint8_t byteStream[], unsigned int colorMapStarts, unsigned int colorMapIdx){
        if (byteStream[colorMapStarts] & (1 << (7 - colorMapIdx)))
            return 0; // pixel on = black color
        return 1; // white color
    }
};
struct _EIMG_GetColor8{
		// DISPLAY_COLOR_DEPTH == 8
    inline
    uint8_t operator()(const uint8_t byteStream[], unsigned int colorMapStarts, unsigned int colorMapIdx){
        return byteStream[colorMapStarts + colorMapIdx];
    }
};
struct _EIMG_GetColor16{
    // DISPLAY_COLOR_DEPTH == 16
    inline
    uint16_t operator()(const uint8_t byteStream[], unsigned int colorMapStarts, unsigned int colorMapIdx){
        unsigned int pos = colorMapStarts + (colorMapIdx * 2);
        return (byteStream[pos] << 8) | byteStream[pos + 1];
    }
};
struct _EIMG_GetColor24 {
		// DISPLAY_COLOR_DEPTH == 24
    inline
    uint32_t operator()(const uint8_t byteStream[], unsigned int colorMapStarts, unsigned int colorMapIdx){
        unsigned int pos = colorMapStarts + (colorMapIdx * 3);
        return (byteStream[pos] << 16) | (byteStream[pos+1] << 8) | byteStream[pos+2];
    }
};


// dont rely on math.h pow function, it makes build huge
template<uint8_t DISPBITS, typename COLOR_TYPE, typename COLOR_LOOKUP>
uint32_t _EImgT<DISPBITS, COLOR_TYPE, COLOR_LOOKUP>::pow(uint16_t base, uint16_t exp) const
{
    uint32_t result = 1;
    while (exp)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        base *= base;
    }
    return result;
}

/*
template<uint8_t DISPBITS, typename COLOR_TYPE, typename COLOR_LOOKUP>
_EImgT<DISPBITS, COLOR_TYPE, COLOR_LOOKUP>::_EImgT(const uint8_t *imgData){
	image = imgData;
	parseMeta();
}
*/

template<uint8_t DISPBITS, typename COLOR_TYPE, typename COLOR_LOOKUP>
void _EImgT<DISPBITS, COLOR_TYPE, COLOR_LOOKUP>::parseMeta(){
    static const uint8_t IS_EXTENDED_META       = 0x80; // 0b1000 0000
    static const uint8_t IS_TRANSPARENT         = 0x40; // 0b0100 0000
    static const uint8_t EXTENDED_COLOR_DEPTH   = 0x3C; // 0b0011 1100
    static const uint8_t SHORT_COLOR_DEPTH      = 0x38; // 0b0011 1000

    uint16_t colorTableSize, pos, nextPos;
    uint32_t bytesPos = 0;
    uint8_t meta_b1, byte;
    bytesPos = 0;
    meta_b1 = image[bytesPos++];

    if (meta_b1 & IS_EXTENDED_META){
        metaInfo.colorDepth = (meta_b1 & EXTENDED_COLOR_DEPTH) >> 2; // 2 reserved bits
        metaInfo.width = (image[bytesPos] << 4) | (image[bytesPos + 1] & 0xF0) >> 4;// b2->0b1111 1111 + b3->0b1111 0000
        metaInfo.height = ((image[bytesPos + 1] & 0x0F) << 8) | image[bytesPos + 2];  // b3->0b0000 1111 + b4->0b1111 1111
        bytesPos = 4;
    } else {
        metaInfo.colorDepth = (meta_b1 & SHORT_COLOR_DEPTH) >> 3; // 3 bits shifted
        metaInfo.width = ((meta_b1 & 0x03) << 3) | ((image[bytesPos] & 0xE0) >> 5); // b1->0b0000 0011 + b2->0b1110 0000
        metaInfo.height = image[bytesPos++] & 0x1F; 						 // b2->0b0001 1111
        metaInfo.width += 1; // 0 = 1 pixel saves some space assuming nobody has a picture of 0 pixels :)
        metaInfo.height += 1;
    }
    
    metaInfo.hasTransparency = meta_b1 & IS_TRANSPARENT; 
    // done with meta header

    // ------------ start read color table size ------------
    // start reading in number of entries in the colorTable (might be less than what is set as max in meta)
    colorTableSize = 0;

    // read the stream length, it should be here just before the colors
    // first find the stop byte for this segment (bit[0] = 0)

    pos = bytesPos;
    do {
        byte = image[pos++];
    }while(byte & 1);
    nextPos = pos;

    // read it into variable, little end last
    uint8_t bitPos = 0;
    do { // continue as long as last bit is 1, size ends with 0 on last byte
        // NOTE in C we might use union here with uint32 and uchar[4] to speed up?
        byte = image[--pos];
        colorTableSize |= ((byte & 0xFE) >> 1) << bitPos;
        bitPos += 7;
    } while(pos > bytesPos);

    bytesPos = nextPos;
    // ------------ end color table size --------------


    // ---------- color table start and finish -----------
    metaInfo.cTableStart = bytesPos;
    //var pixelStart = bytesPos + colorTableSize;


    // ---------- start render pixels ----------------
    metaInfo.pixelsStart = bytesPos + colorTableSize; //pixelStart;

    if (!metaInfo.colorDepth) metaInfo.colorDepth = 1; // monocrome?
}

template<uint8_t DISPBITS, typename COLOR_TYPE, typename COLOR_LOOKUP>
EImgData_t  &_EImgT<DISPBITS, COLOR_TYPE, COLOR_LOOKUP>::getImageData() const {
	EImgData_t  data;
	data.width = metaInfo.width;
	data.height = metaInfo.height;
	data.colorCount = pow(2, metaInfo.colorDepth);
	return data;
}

template<uint8_t DISPBITS, typename COLOR_TYPE, typename COLOR_LOOKUP>
void _EImgT<DISPBITS, COLOR_TYPE, COLOR_LOOKUP>::draw(const int16_t startX, const int16_t startY){
    uint32_t bytesPos = 0;
    unsigned int test, colorIdx, bitPos, colorMask; // use most efficient type of int

		
		bytesPos = metaInfo.pixelsStart;

    // find out how many bits there is in colorDepth
    test = pow(2, metaInfo.colorDepth) -1;

    colorMask = 0;
    while(test > 0){
        colorMask = (colorMask << 1) | 1;
        test >>= 1;
    }
    colorMask <<= 23 - metaInfo.colorDepth;

    // render pixels
   // maxColors = pow(2, metaInfo.colorDepth);
    colorIdx = 0;

    if (metaInfo.hasTransparency){
        uint32_t ui24, pMask, cMask, p, end;
        uint16_t x, y;

        ui24 = 0; pMask = 1 << 15; bitPos = 15; //cMask; // = colorMask << (15 - colorDepth);
        for(p = 0, end = metaInfo.width * metaInfo.height; p < end; /*change in code*/){
            ui24 = (image[bytesPos] << 16);
            ui24 |= (image[bytesPos +1] << 8);
            ui24 |= image[bytesPos +2]; // we load one byte at a time swaping low for higher byte
            ++bytesPos;
            pMask <<= 8;
            bitPos += 8;
            //cMask <<= 8;
            while(bitPos > 15 && p < end){
                if(pMask & ui24){
                    // a visible pixel lookup color
                    cMask = colorMask >> (23 - bitPos);
                    bitPos -= metaInfo.colorDepth;
                    colorIdx = (ui24 & cMask) >> bitPos;
                    pMask >>= metaInfo.colorDepth;
                    //cMask >>= colorDepth;

                   // assert(colorIdx <= maxColors);

                    // render it
                    y = p / metaInfo.width;
                    x = p - (y * metaInfo.width);
                    ((*pDrawObject).*pDrawPixel)(x + startX, y + startY,  getColor(image, metaInfo.cTableStart, colorIdx));
                }
                pMask >>= 1;
                //cMask >>= 1;
                --bitPos;
                ++p; // increment pixels
            }
        }

    } else {
        uint32_t ui24, cMask, p, end;
        uint16_t x, y;
        ui24 = 0; bitPos = 15; //cMask;// = (colorMask <<  (16 - colorDepth));
        for(p = 0, end = metaInfo.width * metaInfo.height; p < end; /*in code*/){
            ui24  = image[bytesPos] << 16;
            ui24 |= image[bytesPos +1] << 8;
            ui24 |= image[bytesPos +2];
            ++bytesPos;
            bitPos += 8;
            cMask <<= 8;
            while(bitPos > 15 && p < end){
                cMask = colorMask >> (23 - bitPos);
                bitPos -= metaInfo.colorDepth;
                colorIdx = (ui24 & cMask) >> (bitPos +1);
               // cMask >>= colorDepth;

                //assert(colorIdx <= maxColors);

                // render it
                y = p / metaInfo.width;
                x = p - (y * metaInfo.width);
                ((*pDrawObject).*pDrawPixel)(x + startX, y + startY,  getColor(image, metaInfo.cTableStart, colorIdx));
                ++p;
            }
        }
    }
}





/*
template<typename COLOR_INT_TYPE, void (F (int16_t,int16_t,uint16_t)>
struct _EImgDPF{
	inline
	void operator()(int16_t x, int16_t y, COLOR_INT_TYPE color){
		F(x, y, color);
	}
};
template <typename F>
struct EImg16Bits {
	typedef _EImgT<16,uint16_t, _EIMG_GetColor16, EImgDrawPixelFunction<uint16_t, F> >  type;
}*/
typedef _EImgT<16, uint16_t, _EIMG_GetColor16 >  EImg16;

//using EImg16 = _EImgT<16, uint16_t, _EIMG_GetColor16 > ;





#endif // EIMG_H
