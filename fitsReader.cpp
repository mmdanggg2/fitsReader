// fitsReader
// Author mmdanggg2 (James Horsley)

// Reads fits files using cfitsio (https://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html)

#include <fitsio.h>
#undef TBYTE // Fix overlap with windows type name
#include "DDImage/DDWindows.h"
#include "DDImage/Reader.h"
#include "DDImage/Row.h"
#include "DDImage/ARRAY.h"
#include "DDImage/Thread.h"
#include "DDImage/LUT.h"

using namespace DD::Image;
using std::vector;
using std::map;
using std::string;

struct hduImageInfo {
	int hduIndex;
	char name[70] = {};
	string chName;
	
	int bpp;
	int nDims;
	long size[3] = {1,1,1};
};


class fitsReader : public Reader {
	Lock lock;
	fitsfile* fptr = nullptr;
	int status = 0;
	map<Channel, hduImageInfo> hduImageList;
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

// Prints the current fits error message to cout
// if iop is not null, it will be used to display the error message in nuke
// clears the error status unless clearStatus is false
void printFitsError(const char* msg, int* status, Iop* iop, bool clearStatus = true) {
	std::cout << msg << ": ";
	fits_report_error(stdout, *status);

	if (iop) {
		char errStr[31] = {};
		fits_get_errstatus(*status, errStr);
		iop->error("%s: %s", msg, errStr);
	}
	if (clearStatus) {
		*status = 0;
	}
}

fitsReader::fitsReader(Read* r, int fd, const unsigned char* b, int n) : Reader(r) {
	// Make it default to linear colorspace:
	lut_ = LUT::GetLut(LUT::FLOAT, this);
	
	std::cout << "Reading in FITS file: " << r->filename() << std::endl;
	if (fits_open_file(&fptr, r->filename(), READONLY, &status)) {
		printFitsError("Error reading file", &status, iop);
		return;
	}
	
	int hduCount;
	if (fits_get_num_hdus(fptr, &hduCount, &status)) {
		printFitsError("Error getting number of hdu's in file", &status, iop);
		return;
	}
	std::cout << "Number of HDUs: " << hduCount << std::endl;
	
	ChannelSet channels = ChannelSet();
	
	for (int i = 1; i <= hduCount; i++) {
		int hduType;
		if (fits_movabs_hdu(fptr, i, &hduType, &status)) {
			printFitsError("Error moving to hdu", &status, iop);
			continue;
		}
			
		/*/char* hdrText;
		int nKeys;
		if (fits_hdr2str(fptr, 0, nullptr, 0, &hdrText, &nKeys, &status)) {
			printFitsError("Error reading header", &status, nullptr);
			continue;
		}
		std::cout << "Header: " << std::endl << hdrText << std::endl;//*/

		if (hduType == IMAGE_HDU) {
			std::cout << "Found image hdu at " << i << std::endl;
			hduImageInfo info = {};
			info.hduIndex = i;
			if (fits_read_key(fptr, TSTRING, "EXTNAME", info.name, nullptr, &status)) {
				printFitsError("Error reading EXTNAME", &status, nullptr);
				continue;
			}
			std::cout << "name: " << info.name << std::endl;
			info.chName = info.name;
			if (fits_get_img_param(fptr, 3, &info.bpp, &info.nDims, info.size, &status)) {
				printFitsError("Error getting image parameters", &status, iop);
				continue;
			}
			std::cout << "bpp: " << info.bpp << std::endl;
			std::cout << "naxis: " << info.nDims << std::endl;
			std::cout << "Width: " << info.size[0] << std::endl;
			std::cout << "Height: " << info.size[1] << std::endl;
			if (info.nDims != 2) {
				std::cout << "Image not 2D, ignoring..." << std::endl;
				continue;
			}
			Channel ch = getChannel(info.chName.append(".r").c_str());
			channels += ch;
			hduImageList[ch] = info;
		}
	}
	auto infoPair = hduImageList.begin();
	hduImageInfo info = infoPair->second;
	if (fits_movabs_hdu(fptr, info.hduIndex, nullptr, &status)) {
		printFitsError("Error moving to hdu", &status, iop);
	}
	set_info(info.size[0], info.size[1], 1);
	info_.channels(channels);
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
void fitsReader::engine(int y, int x, int xr, ChannelMask chans, Row& row) {
	long startLoc[2] = {x+1, y+1};
	float nan = NAN;
	
	foreach(ch, chans) {
		int wasNulls = 0;
		hduImageInfo info = hduImageList[ch];
		lock.lock();
		if (fits_movabs_hdu(fptr, info.hduIndex, nullptr, &status)) {
			printFitsError("Error moving to hdu", &status, iop);
			continue;
		}
		float* writeable = row.writable(ch);
		
		if (fits_read_pix(fptr, TFLOAT, startLoc, xr-x, &nan, writeable, &wasNulls, &status)) {
			printFitsError("Error reading image pixels", &status, iop);
		}
		lock.unlock();
	}
	
}
