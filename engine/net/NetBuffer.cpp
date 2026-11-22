#include "inc/net/NetBuffer.h"
#include <cstring>

using namespace Net;

void NetWriter::U8(uint8_t value)
{
	_data.push_back(value);
}

void NetWriter::U16(uint16_t value)
{
	_data.push_back(static_cast<uint8_t>(value & 0xff));
	_data.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void NetWriter::U32(uint32_t value)
{
	_data.push_back(static_cast<uint8_t>(value & 0xff));
	_data.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
	_data.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
	_data.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void NetWriter::I32(int32_t value)
{
	U32(static_cast<uint32_t>(value));
}

void NetWriter::F32(float value)
{
	static_assert(sizeof(float) == sizeof(uint32_t), "unexpected float size");
	uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	U32(bits);
}

void NetWriter::Str(std::string_view value)
{
	size_t size = value.size();
	if (size > 0xffff)
		size = 0xffff;
	U16(static_cast<uint16_t>(size));
	Bytes(value.data(), size);
}

void NetWriter::Bytes(const void *data, size_t size)
{
	auto bytes = static_cast<const uint8_t*>(data);
	_data.insert(_data.end(), bytes, bytes + size);
}

///////////////////////////////////////////////////////////////////////////////

NetReader::NetReader(const uint8_t *data, size_t size)
	: _data(data)
	, _size(size)
{
}

bool NetReader::Ensure(size_t size)
{
	if (!_ok || _size - _pos < size)
	{
		_ok = false;
		return false;
	}
	return true;
}

uint8_t NetReader::U8()
{
	if (!Ensure(1))
		return 0;
	return _data[_pos++];
}

uint16_t NetReader::U16()
{
	if (!Ensure(2))
		return 0;
	uint16_t value = static_cast<uint16_t>(_data[_pos]) | (static_cast<uint16_t>(_data[_pos + 1]) << 8);
	_pos += 2;
	return value;
}

uint32_t NetReader::U32()
{
	if (!Ensure(4))
		return 0;
	uint32_t value = static_cast<uint32_t>(_data[_pos])
		| (static_cast<uint32_t>(_data[_pos + 1]) << 8)
		| (static_cast<uint32_t>(_data[_pos + 2]) << 16)
		| (static_cast<uint32_t>(_data[_pos + 3]) << 24);
	_pos += 4;
	return value;
}

int32_t NetReader::I32()
{
	return static_cast<int32_t>(U32());
}

float NetReader::F32()
{
	uint32_t bits = U32();
	float value;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

std::string NetReader::Str()
{
	size_t size = U16();
	if (!Ensure(size))
		return std::string();
	std::string result(reinterpret_cast<const char*>(_data + _pos), size);
	_pos += size;
	return result;
}

bool NetReader::Bytes(void *dst, size_t size)
{
	if (!Ensure(size))
	{
		std::memset(dst, 0, size);
		return false;
	}
	std::memcpy(dst, _data + _pos, size);
	_pos += size;
	return true;
}
