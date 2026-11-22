#include <net/NetBuffer.h>
#include <gtest/gtest.h>
#include <cmath>

TEST(NetBuffer, RoundTrip)
{
	Net::NetWriter w;
	w.U8(0xab);
	w.U16(0xbeef);
	w.U32(0xdeadbeef);
	w.I32(-123456);
	w.F32(3.25f);
	w.F32(-0.0f);
	w.Bool(true);
	w.Str("hello");
	w.Str("");
	const uint8_t raw[3] = { 1, 2, 3 };
	w.Bytes(raw, sizeof(raw));

	auto data = w.Take();
	EXPECT_EQ(1 + 2 + 4 + 4 + 4 + 4 + 1 + (2 + 5) + 2 + 3, data.size());

	Net::NetReader r(data);
	EXPECT_EQ(0xab, r.U8());
	EXPECT_EQ(0xbeef, r.U16());
	EXPECT_EQ(0xdeadbeefu, r.U32());
	EXPECT_EQ(-123456, r.I32());
	EXPECT_EQ(3.25f, r.F32());
	EXPECT_TRUE(std::signbit(r.F32()));
	EXPECT_TRUE(r.Bool());
	EXPECT_EQ("hello", r.Str());
	EXPECT_EQ("", r.Str());
	uint8_t rawOut[3] = {};
	EXPECT_TRUE(r.Bytes(rawOut, sizeof(rawOut)));
	EXPECT_EQ(1, rawOut[0]);
	EXPECT_EQ(3, rawOut[2]);
	EXPECT_TRUE(r.Ok());
	EXPECT_TRUE(r.AtEnd());
}

TEST(NetBuffer, LittleEndian)
{
	Net::NetWriter w;
	w.U32(0x04030201);
	auto data = w.Take();
	ASSERT_EQ(4u, data.size());
	EXPECT_EQ(1, data[0]);
	EXPECT_EQ(2, data[1]);
	EXPECT_EQ(3, data[2]);
	EXPECT_EQ(4, data[3]);
}

TEST(NetBuffer, OverrunIsSticky)
{
	Net::NetWriter w;
	w.U16(7);
	auto data = w.Take();

	Net::NetReader r(data);
	EXPECT_EQ(0u, r.U32()); // not enough data
	EXPECT_FALSE(r.Ok());
	EXPECT_EQ(0, r.U8());   // still fails even though one byte would fit
	EXPECT_EQ("", r.Str());
	EXPECT_FALSE(r.Ok());
}

TEST(NetBuffer, TruncatedString)
{
	Net::NetWriter w;
	w.U16(100); // claims 100 bytes
	w.U8('x');
	auto data = w.Take();

	Net::NetReader r(data);
	EXPECT_EQ("", r.Str());
	EXPECT_FALSE(r.Ok());
}
