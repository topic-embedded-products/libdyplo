#include <unistd.h>
#include "yaffut.h"
#include "hardware.hpp"
#include "config.h"
#include <vector>

class TestContext
{
public:
	TestContext()
	{
		create("/tmp/dyploctl");
	}
	~TestContext()
	{
		::unlink("/tmp/bitstream");
		::unlink("/tmp/xdevcfg");
		::unlink("/tmp/dyploctl");
	}
	static void create(const char* filename, int mode = S_IRUSR|S_IWUSR)
	{
		dyplo::File ctl(::open(filename, O_CREAT, mode));
	}
};

static const unsigned char invalid_bitstream[64] = 
	{0}; /* Not valid for any type */
static const unsigned char valid_bin_bitstream[128] =
	{0xFF, 0xFF, 0xFF, 0xFF, 1, 2, 3, 4, 5, 6, 7, 8}; /* Valid binary bitstream */
static const unsigned char valid_bit_bitstream[128] = {
	0x00, 0x09, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x00, 0x00, 0x01, 0x61, 0x00, 0x20,
	0x64, 0x79, 0x70, 0x6c, 0x6f, 0x5f, 0x77, 0x72, 0x61, 0x70, 0x70, 0x65, 0x72, 0x3b, 0x55, 0x73,
	0x65, 0x72, 0x49, 0x44, 0x3d, 0x30, 0x58, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x00,
	0x62, 0x00, 0x0c, 0x37, 0x7a, 0x30, 0x32, 0x30, 0x63, 0x6c, 0x67, 0x34, 0x38, 0x34, 0x00, 0x63,
	0x00, 0x0b, 0x32, 0x30, 0x31, 0x33, 0x2f, 0x30, 0x38, 0x2f, 0x33, 0x30, 0x00, 0x64, 0x00, 0x09,
	0x31, 0x31, 0x3a, 0x35, 0x32, 0x3a, 0x31, 0x35, 0x00, 0x65, 0x00, 0x00, 0x00,   32,    4,    3, /* Size = 32 */
	   2,    1,    8,    7,    6,    5,   12,   11,   10,    9,   16,   15,   14,   13,   20,   19,
	  18,   17,   24,   23,   22,   21,   28,   27,   26,   25,   32,   31,   30,   29, 0xE0, 0x0E,
};
FUNC(program_bin_file)
{
	TestContext tc;
	dyplo::HardwareContext ctrl("/tmp/dyplo"); /* Create a "fake" device */
	dyplo::File xdevcfg(::open("/tmp/xdevcfg", O_CREAT | O_RDWR, S_IRUSR|S_IWUSR));
	dyplo::File bitstream(::open("/tmp/bitstream", O_CREAT | O_WRONLY, S_IRUSR|S_IWUSR));
	/* Invalid stream must throw exception */
	bitstream.write(invalid_bitstream, sizeof(invalid_bitstream));
	ASSERT_THROW(ctrl.program(xdevcfg, "/tmp/bitstream"), std::runtime_error);
	/* Valid binary "raw" stream, pass through unchanged */
	{
		bitstream.seek(0);
		bitstream.write(valid_bin_bitstream, sizeof(valid_bin_bitstream));
		EQUAL(sizeof(valid_bin_bitstream), ctrl.program(xdevcfg, "/tmp/bitstream"));
		std::vector<unsigned char> buffer(sizeof(valid_bin_bitstream));
		xdevcfg.seek(0);
		EQUAL(sizeof(valid_bin_bitstream), xdevcfg.read(&buffer[0], sizeof(valid_bin_bitstream)));
		CHECK(memcmp(valid_bin_bitstream, &buffer[0], sizeof(valid_bin_bitstream)) == 0);
	}
	/* Valid "bit" stream, must remove header and flip bytes */
	{
		xdevcfg.seek(0);
		::ftruncate(xdevcfg, 0);
		bitstream.seek(0);
		bitstream.write(valid_bit_bitstream, sizeof(valid_bit_bitstream));
		EQUAL(32, ctrl.program(xdevcfg, "/tmp/bitstream"));
		std::vector<unsigned char> buffer(sizeof(valid_bit_bitstream));
		xdevcfg.seek(0);
		EQUAL(32, xdevcfg.read(&buffer[0], sizeof(valid_bit_bitstream))); /* Properly truncated */
		for (int i=0; i<32; ++i)
			EQUAL(i+1, (int)buffer[i]); /* Byte reversal check */
	}
	/* Truncated "bit" stream */
	{
		xdevcfg.seek(0);
		::ftruncate(xdevcfg, 0);
		bitstream.seek(0);
		::ftruncate(bitstream, sizeof(valid_bit_bitstream) - 10);
		ASSERT_THROW(ctrl.program(xdevcfg, "/tmp/bitstream"), dyplo::TruncatedFileException);
	}
	::unlink("/tmp/xdevcfg");
}

FUNC(program_mode)
{
	try
	{
		dyplo::HardwareContext ctrl; /* Create a real device */
		try
		{
			ctrl.setProgramMode(true);
			CHECK(ctrl.getProgramMode());
			ctrl.setProgramMode(false);
			CHECK(!ctrl.getProgramMode());
		}
		catch (const dyplo::IOException& ex)
		{
			/* Above methods should NOT throw errors */
			FAIL(ex.what());
		}
	}
	catch (const dyplo::IOException&)
	{
		std::cerr << " (skip)";
		return;
	}
}
