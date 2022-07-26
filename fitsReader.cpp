// fitsReader
// Author mmdanggg2 (James Horsley)

// Reads fits files using cfitsio (https://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html)

#include <fitsio.h>
#include "DDImage/DDWindows.h"
#include "DDImage/Reader.h"
#include "DDImage/Row.h"
#include "DDImage/ARRAY.h"
#include "DDImage/Thread.h"
#include "DDImage/LUT.h"

using namespace DD::Image;

class fitsReader : public Reader {
	Lock lock;
	fitsfile* fptr = nullptr;
	int status = 0;
public:
	fitsReader(Read*, int fd, const unsigned char* b, int n);
	~fitsReader();
	void engine(int y, int x, int r, ChannelMask, Row &);
	void open();
	static const Description d;
};

/** Check for the correct magic numbers at the start of the file
 */
static bool test(int, const unsigned char* block, int length) {
	return memcmp(block, "SIMPLE", 6) == 0;
}

static Reader* build(Read* iop, int fd, const unsigned char* b, int n) {
	return new fitsReader(iop, fd, b, n);
}

const Reader::Description fitsReader::d("fits\0", "fits file reader", build, test);

fitsReader::fitsReader(Read* r, int fd, const unsigned char* b, int n) : Reader(r) {
	// Make it default to linear colorspace:
	lut_ = LUT::GetLut(LUT::FLOAT, this);
	
	std::cout << "Reading in FITS file: " << r->filename() << std::endl;
	if (fits_open_image(&fptr, r->filename(), READONLY, &status)) {
		std::cout << "Error reading file: ";
		fits_report_error(stdout, status);
		char errStr[31] = {};
		fits_get_errstatus(status, errStr);
		iop->error("Error getting image parameters: %s", errStr);
		return;
	}
    int bitpix, naxis;
	long naxes[2] = {1,1};
	if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
		std::cout << "Error getting image parameters: ";
		fits_report_error(stdout, status);
		char errStr[31] = {};
		fits_get_errstatus(status, errStr);
		iop->error("Error getting image parameters: %s", errStr);
		return;
	}
	std::cout << "bpp: " << bitpix << std::endl;
	std::cout << "naxis: " << naxis << std::endl;
	std::cout << "Width: " << naxes[0] << std::endl;
	std::cout << "Height: " << naxes[1] << std::endl;
	set_info(naxes[0], naxes[1], 1);
}

fitsReader::~fitsReader() {
	if (fptr != nullptr) {
		std::cout << "Closing FITS file: " << filename() << std::endl;
		fits_close_file(fptr, &status);
	}
}

void fitsReader::open() {
	Reader::open();
}

// The engine reads individual rows out of the input.
void fitsReader::engine(int y, int x, int xr, ChannelMask channels, Row& row) {
	long startLoc[2] = {x+1, y+1};
	float* writeable = row.writable(Chan_Red);
	
	lock.lock();
	float nan = NAN;
	if (fits_read_pix(fptr, TFLOAT, startLoc, xr-x, &nan, writeable, NULL, &status)) {
		std::cout << "Error reading image pixels: ";
		fits_report_error(stdout, status);
	}
	
	lock.unlock();
}
