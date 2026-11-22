#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Net
{
	// Little-endian binary writer. All multi-byte values are written with an explicit
	// byte order so that the wire format does not depend on the host.
	class NetWriter final
	{
	public:
		void U8(uint8_t value);
		void U16(uint16_t value);
		void U32(uint32_t value);
		void I32(int32_t value);
		void F32(float value);
		void Bool(bool value) { U8(value ? 1 : 0); }
		void Str(std::string_view value); // U16 length + bytes, max 65535 bytes
		void Bytes(const void *data, size_t size);

		size_t GetSize() const { return _data.size(); }
		const uint8_t* GetData() const { return _data.data(); }
		std::vector<uint8_t> Take() { return std::move(_data); }

	private:
		std::vector<uint8_t> _data;
	};

	// Little-endian binary reader. Never throws: after the first overrun Ok() becomes false
	// and all subsequent reads return zero / empty values.
	class NetReader final
	{
	public:
		NetReader(const uint8_t *data, size_t size);
		explicit NetReader(const std::vector<uint8_t> &data) : NetReader(data.data(), data.size()) {}

		uint8_t U8();
		uint16_t U16();
		uint32_t U32();
		int32_t I32();
		float F32();
		bool Bool() { return U8() != 0; }
		std::string Str();
		bool Bytes(void *dst, size_t size);

		bool Ok() const { return _ok; }
		bool AtEnd() const { return _pos == _size; }
		size_t GetRemaining() const { return _size - _pos; }

	private:
		bool Ensure(size_t size);

		const uint8_t *_data;
		size_t _size;
		size_t _pos = 0;
		bool _ok = true;
	};
}
