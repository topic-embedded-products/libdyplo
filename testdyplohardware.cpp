/*
 * testdyplohardware.cpp
 *
 * Dyplo library for Kahn processing networks.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. (http://www.topic.nl).
 * All rights reserved.
 *
 * This file is part of libdyplo.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA or see <http://www.gnu.org/licenses/>.
 *
 * You can contact Topic by electronic mail via info@topic.nl or via
 * paper mail at the following address: Postbus 440, 5680 AK Best, The Netherlands.
 */
#include <unistd.h>
#include "yaffut.h"
#include "hardware.hpp"
#ifdef __linux__
#include "config.h"
#endif
#include <vector>
#include <list>
#include <string>
#include <fstream>

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

static const unsigned char valid_partial_bitstream[128] =
	{0x00, 1, 2, 3, 0xFF, 0xFF, 0xFF, 0xFF, 1, 2, 3, 4, 5, 6, 7, 8};

static const unsigned char invalid_partial_bitstream[128] =
	{0x00, 0xAA, 0xAA, 0xAA, 0xFF, 0xFF, 0xFF, 0xFE, 1, 2, 3, 4, 5, 6, 7, 8};

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

struct hardware_programmer {};

class FpgaImageReaderCallbackMock : public dyplo::FpgaImageReaderCallback
{
public:
	virtual ssize_t processData(const void* data, const size_t length_bytes)
	{
		// do nothing
		return 0;
	}

	virtual void verifyStaticID(const unsigned int user_id)
	{
		// do nothing
	}
};


class FpgaImageReaderWithTagValidation : public dyplo::FpgaImageReader
{
public:
	FpgaImageReaderWithTagValidation(dyplo::FpgaImageReaderCallback& callback) :
		FpgaImageReader(callback)
	{
	}

	bool parseDescriptionTag(const char* data, unsigned short size, bool *is_partial, unsigned int *user_id)
	{
		return dyplo::FpgaImageReader::parseDescriptionTag(data, size, is_partial, user_id);
	}

	void processTag(char tag, unsigned short size, const void *data)
	{
		tags[tag] = std::string((const char*)data, size);
		dyplo::FpgaImageReader::processTag(tag, size, data);
	}

	void verify()
	{
		CHECK(!tags.empty());
		std::string v = tags['a'];
		std::cout << v.size() << ':';

		EQUAL(std::string("dyplo_wrapper;UserID=0XFFFFFFFF\0", 32), tags['a']);
		EQUAL(std::string("7z020clg484\0", 12), tags['b']);
		EQUAL(std::string("2013/08/30\0", 11), tags['c']);
		EQUAL(std::string("11:52:15\0", 9), tags['d']);
	}

private:
	std::map<char, std::string> tags;
};

// testing bin / bit support by FpgaImageReader
#ifndef __rtems__

static unsigned int bswap32(unsigned int x)
{
	return (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | ((x & 0xFF000000) >> 24);
}

TEST(hardware_programmer, bin_file)
{
	TestContext tc;

	const char* bitstreamPath = "/tmp/bitstream";

	dyplo::File xdevcfg(::open("/tmp/xdevcfg", O_CREAT | O_RDWR, S_IRUSR|S_IWUSR));
	dyplo::File bitstream(::open(bitstreamPath, O_CREAT | O_WRONLY, S_IRUSR|S_IWUSR));
	dyplo::File bitstreamToRead = dyplo::File(::open(bitstreamPath, O_RDONLY));
	dyplo::FpgaImageFileWriter writer(xdevcfg);
	dyplo::FpgaImageReader reader(writer);

	/* Invalid stream must throw exception */
	bitstream.write(invalid_bitstream, sizeof(invalid_bitstream));

	ASSERT_THROW(reader.processFile(bitstreamToRead), std::runtime_error);

	/* Valid binary "raw" stream, pass through unchanged */
	{
		bitstream.seek(0);
		bitstream.write(valid_bin_bitstream, sizeof(valid_bin_bitstream));
		bitstreamToRead.seek(0);
		EQUAL(sizeof(valid_bin_bitstream), reader.processFile(bitstreamToRead));
		std::vector<unsigned char> buffer(sizeof(valid_bin_bitstream));
		xdevcfg.seek(0);
		EQUAL((ssize_t)sizeof(valid_bin_bitstream), xdevcfg.read(&buffer[0], sizeof(valid_bin_bitstream)));
		CHECK(memcmp(valid_bin_bitstream, &buffer[0], sizeof(valid_bin_bitstream)) == 0);
		EQUAL((ssize_t)sizeof(valid_bin_bitstream), dyplo::File::get_size("/tmp/bitstream"));
	}

	/* Valid "bit" stream, must remove header and flip bytes */
	{
		FpgaImageReaderWithTagValidation readerWithTagger(writer);
		xdevcfg.seek(0);
		EQUAL(0, ::ftruncate(xdevcfg, 0));
		bitstream.seek(0);
		bitstream.write(valid_bit_bitstream, sizeof(valid_bit_bitstream));
		bitstreamToRead.seek(0);
		EQUAL(32u, readerWithTagger.processFile(bitstreamToRead));
		std::vector<unsigned char> buffer(sizeof(valid_bit_bitstream));
		xdevcfg.seek(0);
		EQUAL(32, xdevcfg.read(&buffer[0], sizeof(valid_bit_bitstream))); // Properly truncated
		for (int i=0; i<32; ++i)
			EQUAL(i+1, (int)buffer[i]); // Byte reversal check
		readerWithTagger.verify();
	}

	/* Big bit stream (multiple read/write cycles of 16k */
	{
		xdevcfg.seek(0);
		EQUAL(0, ::ftruncate(xdevcfg, 0));
		bitstream.seek(0);
		unsigned char size[4] = {0, 1, 0, 0}; /* 64k */
		bitstream.write(valid_bit_bitstream, 0x5A); /* up until "size" */
		bitstream.write(&size, 4);
		std::vector<unsigned int> buffer(0x4000); /* 16k words */
		for (unsigned int i = 0; i < buffer.size(); ++i)
			buffer[i] = bswap32(i);
		bitstream.write(&buffer[0], 0x10000);
		bitstreamToRead.seek(0);
		EQUAL(0x10000u, reader.processFile(bitstreamToRead));
		xdevcfg.seek(0);
		EQUAL(0x10000, xdevcfg.read(&buffer[0], 0x10000u)); /* Properly truncated */
		for (unsigned int i = 0; i < buffer.size(); ++i)
			EQUAL(i, buffer[i]);
	}
	/* Truncated "bit" stream */
	{
		FpgaImageReaderWithTagValidation readerWithTagger(writer);
		xdevcfg.seek(0);
		EQUAL(0, ::ftruncate(xdevcfg, 0));
		bitstream.seek(0);
		bitstreamToRead.seek(0);
		EQUAL(0, ::ftruncate(bitstream, sizeof(valid_bit_bitstream) - 10));
		ASSERT_THROW(readerWithTagger.processFile(bitstreamToRead), dyplo::TruncatedFileException);
		// Tags must still have been processed
		readerWithTagger.verify();
	}
	::unlink("/tmp/xdevcfg");
}

// testing .partial support by FpgaImageReader
TEST(hardware_programmer, partial_file)
{
	TestContext tc;

	const char* bitstreamPath = "/tmp/bitstream";

	dyplo::File xdevcfg(::open("/tmp/xdevcfg", O_CREAT | O_RDWR, S_IRUSR|S_IWUSR));
	dyplo::File bitstream(::open(bitstreamPath, O_CREAT | O_WRONLY, S_IRUSR|S_IWUSR));
	dyplo::File bitstreamToRead = dyplo::File(::open(bitstreamPath, O_RDONLY));
	dyplo::FpgaImageFileWriter writer(xdevcfg);
	dyplo::FpgaImageReader reader(writer);

	// Valid "partial" stream
	{
		dyplo::FpgaImageReader readerWithTagger(writer);
		xdevcfg.seek(0);
		EQUAL(0, ::ftruncate(xdevcfg, 0));
		bitstream.seek(0);
		bitstream.write(valid_partial_bitstream, sizeof(valid_partial_bitstream));
		bitstreamToRead.seek(0);
		unsigned int expected_size = sizeof(valid_partial_bitstream); // the first 32-bits are the header of the .partial file
		EQUAL(expected_size, readerWithTagger.processFile(bitstreamToRead));
		std::vector<unsigned char> buffer(sizeof(valid_partial_bitstream));
		xdevcfg.seek(0);
		EQUAL((int)expected_size, xdevcfg.read(&buffer[0], expected_size)); // Properly truncated
		CHECK(memcmp(valid_partial_bitstream, &buffer[0], expected_size) == 0);
	}

	// Invalid "partial" stream
	{
		dyplo::FpgaImageReader readerWithTagger(writer);
		xdevcfg.seek(0);
		EQUAL(0, ::ftruncate(xdevcfg, 0));
		bitstream.seek(0);
		bitstream.write(invalid_partial_bitstream, sizeof(invalid_partial_bitstream));
		bitstreamToRead.seek(0);
		ASSERT_THROW(readerWithTagger.processFile(bitstreamToRead), std::runtime_error);
	}

	::unlink("/tmp/xdevcfg");
}
#endif

TEST(hardware_programmer, parse_description_tag)
{
	unsigned int user_id = 0;
	bool partial = false;
	bool result;
	static const char d1[] = "dyplo_wrapper;UserID=0X1234abcd\0";

	FpgaImageReaderCallbackMock mock;
	FpgaImageReaderWithTagValidation reader(mock);

	result = reader.parseDescriptionTag(d1, sizeof(d1), &partial, &user_id);
	EQUAL(0x1234abcdu, user_id);
	EQUAL(false, partial);
	EQUAL(true, result);
	static const char d2[] = "dyplo_wrapper;UserID=0xAB12;PARTIAL=TRUE\0";
	result = reader.parseDescriptionTag(d2, sizeof(d2), &partial, &user_id);
	EQUAL(0xAB12u, user_id);
	EQUAL(true, partial);
	EQUAL(true, result);
	static const char d3[] = "dyplo_wrapper;PARTIAL=FALSE;UserID=0x98765432";
	result = reader.parseDescriptionTag(d3, sizeof(d3), &partial, &user_id);
	EQUAL(0x98765432, user_id);
	EQUAL(false, partial);
	EQUAL(true, result);
	static const char d4[] = "dyplo_wrapper;PARTIAL=TRUE\0;NoUserID=42";
	result = reader.parseDescriptionTag(d4, sizeof(d4), &partial, &user_id);
	EQUAL(true, partial);
	EQUAL(false, result);
	EQUAL(0x98765432, user_id);
	// user_id may be null, don't crash
	result = reader.parseDescriptionTag(d3, sizeof(d3), &partial, NULL);
	EQUAL(false, partial);
	EQUAL(true, result);
}

class LotsOfFiles
{
	std::list<std::string> files;
	std::list<std::string> dirs;
public:
	void file(const char* filename, int mode = S_IRUSR|S_IWUSR)
	{
		dyplo::File ctl(::open(filename, O_CREAT, mode));
		files.push_back(filename);
	}
	void dir(const char* filename, int mode = S_IRWXU)
	{
		if (::mkdir(filename, mode) != 0)
			throw dyplo::IOException(filename);
		dirs.push_back(filename);
	}
	void cleanup()
	{
		while (!files.empty()) {
			::unlink(files.front().c_str());
			files.pop_front();
		}
		while (!dirs.empty()) {
			::rmdir(dirs.front().c_str());
			dirs.pop_front();
		}
	}
	~LotsOfFiles()
	{
		cleanup();
	}
};

TEST(hardware_programmer, find_bitstreams)
{
	mkdir("tmp", 0755);
	dyplo::HardwareContext context("/tmp/dyplo"); /* Fake device */
	LotsOfFiles f;
	f.dir("/tmp/dyplo_func_1");
	f.file("/tmp/dyplo_func_1/1.bit");
	f.file("/tmp/dyplo_func_1/2.bit");
	f.file("/tmp/dyplo_func_1/13.bit");
	f.file("/tmp/dyplo_func_1/31.bit");
	f.dir("/tmp/dyplo_func_2");
	f.file("/tmp/dyplo_func_2/only1last2digits3matter1.bit");
	f.file("/tmp/dyplo_func_2/2.bit");
	f.file("/tmp/dyplo_func_2/function2partition12.bit");
	f.file("/tmp/dyplo_func_2/2-21-and-more.bit");
	f.dir("/tmp/dyplo_func_3");
	f.file("/tmp/dyplo_func_3/only1last2digits3matter1.partial");
	f.file("/tmp/dyplo_func_3/2.bit");
	f.file("/tmp/dyplo_func_3/2.partial");
	f.file("/tmp/dyplo_func_3/2.bit.bin");
	f.file("/tmp/dyplo_func_3/function2partition12.partial");
	f.file("/tmp/dyplo_func_3/21-and-more.bot");
	unsigned int parts;
	context.setBitstreamBasepath("/tmp");
	parts = context.getAvailablePartitions("dyplo_func_1");
	EQUAL((unsigned)(1<<31)|(1<<13)|(1<<2)|(1<<1), parts);
	parts = context.getAvailablePartitions("dyplo_func_2");
	EQUAL((unsigned)(1<<21)|(1<<12)|(1<<2)|(1<<1), parts);
	parts = context.getAvailablePartitions("dyplo_func_3");
	EQUAL((unsigned)(1<<21)|(1<<12)|(1<<2)|(1<<1), parts);
	std::string filename;
	filename = context.findPartition("dyplo_func_2", 12);
	EQUAL("/tmp/dyplo_func_2/function2partition12.bit", filename);
	filename = context.findPartition("dyplo_func_2", 11);
	EQUAL("", filename); /* not found */
	filename = context.findPartition("dyplo_func_3", 2);
	EQUAL("/tmp/dyplo_func_3/2.partial", filename);
	filename = context.findPartition("dyplo_func_3", 12);
	EQUAL("/tmp/dyplo_func_3/function2partition12.partial", filename);
	filename = context.findPartition("dyplo_func_3", 21);
	EQUAL("/tmp/dyplo_func_3/21-and-more.bot", filename);
	filename = context.findPartition("dyplo_func_3", 22);
	EQUAL("", filename); // not found
}
